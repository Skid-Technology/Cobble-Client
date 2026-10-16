#pragma once






#include <Windows.h>
#include <wininet.h>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include "../test6/test30.hpp"
#include "generated/version.h"
#include "generated/server_config.h"
#include "test11.h"

#pragma comment(lib, "wininet.lib")

namespace Telemetry {




constexpr int THROTTLE_SECONDS = 300;
constexpr int HTTP_TIMEOUT_MS = 2000;


enum class Severity { INFO, WARN, HIGH, CRITICAL };

inline const char* SeverityToString(Severity s) {
    switch (s) {
        case Severity::INFO:
            return "info";
        case Severity::WARN:
            return "warn";
        case Severity::HIGH:
            return "high";
        case Severity::CRITICAL:
            return "critical";
        default:
            return "info";
    }
}




namespace Internal {
inline std::mutex g_Mutex;
inline std::unordered_map<std::string, int64_t> g_LastSent;
inline std::string g_SessionId;
inline std::string g_ServerHost;
inline bool g_Initialized = false;


inline void Initialize() {
    if (g_Initialized)
        return;
    std::string hwid = HWID::Generate();
    g_SessionId = hwid.empty() ? "unknown" : hwid.substr(0, 16);
    g_Initialized = true;
}

inline std::string NormalizeHost(const std::string& hostOrUrl) {
    std::string host = hostOrUrl;
    const std::string https = "https://";
    const std::string http = "http://";
    if (host.rfind(https, 0) == 0) {
        host = host.substr(https.size());
    } else if (host.rfind(http, 0) == 0) {
        host = host.substr(http.size());
    }
    size_t slash = host.find('/');
    if (slash != std::string::npos) {
        host = host.substr(0, slash);
    }
    return host;
}


inline std::string UrlEncode(const std::string& value) {
    std::string result;
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", c);
            result += hex;
        }
    }
    return result;
}


inline std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            continue;
        else
            out += c;
    }
    return out;
}


struct TelemetryPayload {
    std::string hwid;
    std::string event_code;
    std::string severity;
    int score;
    std::string details;
    std::string version;
};

inline bool SendPayloadToHost(const TelemetryPayload& payload, const std::string& serverHost) {
    HINTERNET hInternet =
        InternetOpenA(OBFUSCATE_STRING("Mozilla/5.0").c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet)
        return false;

    DWORD timeout = HTTP_TIMEOUT_MS;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = InternetConnectA(hInternet, serverHost.c_str(), 443, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }

    HINTERNET hRequest =
        HttpOpenRequestA(hConnect, "POST", OBFUSCATE_STRING("/api/telemetry").c_str(), NULL, NULL, NULL,
                         INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string json = "{";
    json += "\"hwid\":\"" + EscapeJson(payload.hwid) + "\",";
    json += "\"event_code\":\"" + EscapeJson(payload.event_code) + "\",";
    json += "\"severity\":\"" + EscapeJson(payload.severity) + "\",";
    json += "\"score\":" + std::to_string(payload.score) + ",";
    json += "\"details\":\"" + EscapeJson(payload.details) + "\",";
    json += "\"version\":\"" + EscapeJson(payload.version) + "\",";
    json += "\"source\":\"loader\"";
    json += "}";

    std::string headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)json.c_str(),
                                 (DWORD)json.length());

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return sent == TRUE;
}

inline bool SendPayload(const TelemetryPayload& payload) {

    if (!g_ServerHost.empty()) {
        return SendPayloadToHost(payload, g_ServerHost);
    }
    for (const auto& host : cobble_config::GetServerHosts()) {
        if (SendPayloadToHost(payload, host)) {
            cobble_config::PreferServerHost(host);
            return true;
        }
    }
    return false;
}


inline DWORD WINAPI SendThreadProc(LPVOID lpParam) {
    TelemetryPayload* pPayload = static_cast<TelemetryPayload*>(lpParam);
    TelemetryPayload payload = *pPayload;
    delete pPayload;


    Sleep(300 + (rand() % 500));

    SendPayload(payload);
    return 0;
}


inline bool IsThrottled(const std::string& code) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_LastSent.find(code);
    if (it == g_LastSent.end())
        return false;

    int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    return (now - it->second) < THROTTLE_SECONDS;
}


inline void MarkSent(const std::string& code) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_LastSent[code] =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}






inline void Init() {
    Internal::Initialize();
}

inline void Init(const std::string& serverHost) {
    {
        std::lock_guard<std::mutex> lock(Internal::g_Mutex);
        if (!serverHost.empty()) {
            Internal::g_ServerHost = Internal::NormalizeHost(serverHost);
        }
    }
    Internal::Initialize();
}


inline void ReportImmediate(const std::string& event_code, Severity severity, int score,
                            const std::string& details = "") {
    Internal::Initialize();

    Internal::TelemetryPayload payload;
    payload.hwid = Internal::g_SessionId;
    payload.event_code = event_code;
    payload.severity = SeverityToString(severity);
    payload.score = score;
    payload.details = details;
    payload.version = COBBLE_VERSION;


    for (int i = 0; i < 2; i++) {
        if (Internal::SendPayload(payload)) {
            return;
        }
        Sleep(150);
    }
}


inline void Report(const std::string& event_code, Severity severity, int score, const std::string& details = "") {

    Internal::Initialize();

    if (severity == Severity::CRITICAL) {
        ReportImmediate(event_code, severity, score, details);
        return;
    }


    if (Internal::IsThrottled(event_code)) {
        return;
    }


    Internal::MarkSent(event_code);


    Internal::TelemetryPayload* pPayload = new Internal::TelemetryPayload();
    pPayload->hwid = Internal::g_SessionId;
    pPayload->event_code = event_code;
    pPayload->severity = SeverityToString(severity);
    pPayload->score = score;
    pPayload->details = details;
    pPayload->version = COBBLE_VERSION;


    HANDLE hThread = CreateThread(NULL, 0, Internal::SendThreadProc, pPayload, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete pPayload;
    }
}


inline void ReportDebugger(const std::string& code, int score, const std::string& details = "") {
    Report(code, Severity::CRITICAL, score, details);
}

inline void ReportTool(const std::string& toolName, int score) {
    Report("TOOL_" + toolName, score >= 40 ? Severity::WARN : Severity::INFO, score, toolName + " detected");
}

inline void ReportVM(const std::string& method) {
    Report("VM_" + method, Severity::WARN, 30, method + " detected");
}






namespace Honeypot {

inline const char* FAKE_WEBHOOK_DO_NOT_USE =
    "https://discord.com/api/webhooks/1461846578431131870/"
    "dpl1XTvq-6fAkkDfyLn-yOxW-py5AORzJnInt4bOIimp_uF7H0RH1XavAqINOh7lkGqF";


inline void TriggerHoneypot() {
    Report("HONEYPOT_TRIGGERED", Severity::CRITICAL, 100, "Cracker attempted to use fake webhook");
}
}

}
