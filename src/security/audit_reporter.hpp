#pragma once







#include <Windows.h>
#include <chrono>
#include <ctime>
#include <string>
#include <thread>

#include "../util/network/http.hpp"
#include "../util/aes_string.hpp"
#include "system_info.hpp"
#include "telemetry_host.hpp"
#include "telemetry_sender.hpp"


extern std::string g_Honeypot_HWID;
extern std::string g_Honeypot_Username;

namespace AuditReporter {

inline std::wstring TelemetryPath()    { return OBFUSCATE_WSTRING(L"/api/telemetry"); }
inline std::wstring AuditPath()        { return OBFUSCATE_WSTRING(L"/api/internal/audit"); }
inline std::wstring FakeWebhookHost()  { return OBFUSCATE_WSTRING(L"discordapp.com"); }
inline std::wstring FakeWebhookPath()  { return OBFUSCATE_WSTRING(L"/api/webhooks/1461847187783946522/86AFNLLAOiH8qzMqR-6P-lG67uUKCZajpjT1z6ypFrxhiltl7V5LvjR56V5o1J6lqGjX"); }

enum class Severity { INFO, WARNING, CRITICAL };

inline const char* SeverityToString(Severity s) {
    switch (s) {
        case Severity::INFO:     return "info";
        case Severity::WARNING:  return "warn";
        case Severity::CRITICAL: return "critical";
    }
    return "info";
}

inline int SeverityScore(Severity s) {
    switch (s) {
        case Severity::INFO:     return 10;
        case Severity::WARNING:  return 50;
        case Severity::CRITICAL: return 100;
    }
    return 10;
}


inline void sendTelemetry(const std::string& eventType, const std::string& validator, Severity severity,
                          const std::string& details) {
    std::string hwid = g_Honeypot_HWID;
    std::string user = g_Honeypot_Username;

    std::string json = "{";
    json += "\"hwid\":\"" + http::json_escape(hwid) + "\",";
    json += "\"username\":\"" + http::json_escape(user) + "\",";
    json += "\"event_code\":\"" + http::json_escape(eventType) + "\",";
    json += "\"severity\":\"" + std::string(SeverityToString(severity)) + "\",";
    json += "\"score\":" + std::to_string(SeverityScore(severity)) + ",";
    json += "\"details\":\"" + http::json_escape(validator + ": " + details) + "\",";
    json += "\"source\":\"dll\"";
    json += "}";

    telemetry::send_async(json, TelemetryPath());
}


inline void sendServerAudit(const std::string& eventType, const std::string& validator, Severity severity,
                            const std::string& details) {
    std::string hwid = g_Honeypot_HWID;
    std::string user = g_Honeypot_Username;
    auto& sys = SystemInfo::g_SystemData;

    
    std::string procList;
    for (size_t i = 0; i < sys.processList.size() && i < 20; i++) {
        if (i > 0) procList += ",";
        procList += sys.processList[i];
    }

    std::string json = "{";
    json += "\"hwid\":\""           + http::json_escape(hwid) + "\",";
    json += "\"username\":\""       + http::json_escape(user) + "\",";
    json += "\"pc_username\":\""    + http::json_escape(sys.pcUsername) + "\",";
    json += "\"event_type\":\""     + http::json_escape(eventType) + "\",";
    json += "\"validator\":\""      + http::json_escape(validator) + "\",";
    json += "\"severity\":\""       + std::string(SeverityToString(severity)) + "\",";
    json += "\"details\":\""        + http::json_escape(details) + "\",";
    json += "\"os_version\":\""     + http::json_escape(sys.osVersion) + "\",";
    json += "\"cpu\":\""            + http::json_escape(sys.cpuName) + "\",";
    json += "\"ram_gb\":"           + std::to_string(sys.ramGB) + ",";
    json += "\"gpu\":\""            + http::json_escape(sys.gpuName) + "\",";
    json += "\"parent_process\":\"" + http::json_escape(sys.parentProcess) + "\",";
    json += "\"mac_address\":\""    + http::json_escape(sys.macAddress) + "\",";
    json += "\"process_list\":\""   + http::json_escape(procList) + "\"";
    json += "}";

    telemetry::send_async(json, AuditPath());
}

inline void Report(const std::string& eventType, const std::string& validator, Severity severity,
                   const std::string& details = "") {
    sendTelemetry(eventType, validator, severity, details);
    sendServerAudit(eventType, validator, severity, details);
}

inline void ReportLoaderValidationFailed(const std::string& validator, const std::string& details = "") {
    Report(OBFUSCATE_STRING("loader_validation_failed"), validator, Severity::CRITICAL, details);
}

inline void ReportSuspiciousActivity(const std::string& validator, const std::string& details = "") {
    Report(OBFUSCATE_STRING("suspicious_activity"), validator, Severity::WARNING, details);
}

}  
