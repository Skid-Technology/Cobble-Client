#pragma once




#include <Windows.h>
#include <chrono>
#include <ctime>
#include <string>
#include <thread>

#include "../util/network/http.hpp"
#include "../util/aes_string.hpp"
#include "telemetry_host.hpp"
#include "telemetry_sender.hpp"


extern std::string g_Honeypot_HWID;
extern std::string g_Honeypot_Username;

namespace Honeypot {

inline std::wstring TelemetryPath()   { return OBFUSCATE_WSTRING(L"/api/telemetry"); }
inline std::wstring FakeWebhookHost() { return OBFUSCATE_WSTRING(L"discordapp.com"); }
inline std::wstring FakeWebhookPath() { return OBFUSCATE_WSTRING(L"/api/webhooks/1461847086676050022/VoosC-k5C0kbXtj41qYlMlVfCs00_NssoFQh89wj4Wm29ThH-LLJns_oMT4Rl86Ui_M6"); }


inline std::atomic<uint32_t> g_reportedFlags{0};

inline std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else
            out += c;
    }
    return out;
}

inline void reportToServer(const std::string& eventType, const std::string& details,
                           const std::string& severity = "critical") {
    std::string hwid = g_Honeypot_HWID;
    std::string user = g_Honeypot_Username;

    std::string json = "{";
    json += "\"hwid\":\"" + escapeJson(hwid) + "\",";
    json += "\"username\":\"" + escapeJson(user) + "\",";
    json += "\"event_code\":\"HONEYPOT_" + escapeJson(eventType) + "\",";
    json += "\"severity\":\"" + escapeJson(severity) + "\",";
    json += "\"score\":100,";
    json += "\"details\":\"" + escapeJson(details) + "\",";
    json += "\"source\":\"dll\"";
    json += "}";

    telemetry::send_async(json, TelemetryPath());
}

inline void Report(const std::string& eventType, const std::string& details = "", uint32_t flag = 0) {
    if (flag != 0) {
        uint32_t current = g_reportedFlags.load();
        if (current & flag)
            return;
        g_reportedFlags.fetch_or(flag);
    }

    reportToServer(eventType, details);
}

constexpr uint32_t FLAG_LICENSE_KEY = 1 << 0;
constexpr uint32_t FLAG_DECRYPT = 1 << 1;
constexpr uint32_t FLAG_DEV_MODE = 1 << 2;
constexpr uint32_t FLAG_INTERNAL_KEYS = 1 << 3;
constexpr uint32_t FLAG_FAKE_WEBHOOK = 1 << 4;

}  

inline std::string GetLicenseKey() {
    Honeypot::Report("license_key_accessed", "GetLicenseKey() called", Honeypot::FLAG_LICENSE_KEY);
    return "COBBLE-XXXX-PROD-2026";
}

inline std::string DecryptPayloadKey() {
    Honeypot::Report("decrypt_attempt", "DecryptPayloadKey() called", Honeypot::FLAG_DECRYPT);
    return "aes256_key_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
}

inline std::string GetRawSessionData() {
    Honeypot::Report("session_data_accessed", "GetRawSessionData() called", Honeypot::FLAG_LICENSE_KEY);
    return R"({"session":"FAKE","token":"XXXX"})";
}

inline void SendToDiscordWebhook(const std::string& message) {
    Honeypot::Report("fake_webhook_called", "Cracker tried to use extracted webhook: " + message.substr(0, 100),
                     Honeypot::FLAG_FAKE_WEBHOOK);
}

namespace internal {

inline bool g_DevOverride = false;

inline void checkDevOverride() {
    if (g_DevOverride) {
        Honeypot::Report("dev_override_enabled", "g_DevOverride set to true", Honeypot::FLAG_DEV_MODE);
        g_DevOverride = false;
    }
}

}  
