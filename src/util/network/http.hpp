#pragma once

#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

#include "../aes_string.hpp"
#include "generated/server_config.h"




namespace http {

struct Response {
    bool success = false;
    int statusCode = 0;
    std::string body;
    std::string error;
};

namespace _detail {

inline HINTERNET openSession() {
    HINTERNET h = WinHttpOpen(OBFUSCATE_WSTRING(L"Mozilla/5.0").c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h) return nullptr;
    DWORD timeout = 5000;
    WinHttpSetOption(h, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(h, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(h, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    return h;
}

inline void readBody(HINTERNET hRequest, Response& res) {
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode,
                        &statusCodeSize, nullptr);
    res.statusCode = static_cast<int>(statusCode);

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            res.body.append(buffer.data(), bytesRead);
        }
    }

    res.success = (statusCode >= 200 && statusCode < 300);
}

}  

inline Response get(const std::wstring& host, int port, const std::wstring& path, bool https = false) {
    Response res;
    HINTERNET session = _detail::openSession();
    if (!session) { res.error = "WinHttpOpen failed"; return res; }

    HINTERNET connect = WinHttpConnect(session, host.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!connect) { res.error = "WinHttpConnect failed"; WinHttpCloseHandle(session); return res; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { res.error = "WinHttpOpenRequest failed"; WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return res; }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        res.error = "request failed";
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return res;
    }

    _detail::readBody(request, res);

    WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
    return res;
}



inline Response post(const std::wstring& host, int port, const std::wstring& path, const std::string& jsonBody,
                     bool https = false) {
    Response res;
    HINTERNET session = _detail::openSession();
    if (!session) { res.error = "WinHttpOpen failed"; return res; }

    HINTERNET connect = WinHttpConnect(session, host.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!connect) { res.error = "WinHttpConnect failed"; WinHttpCloseHandle(session); return res; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { res.error = "WinHttpOpenRequest failed"; WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return res; }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), (LPVOID)jsonBody.c_str(),
                            static_cast<DWORD>(jsonBody.length()), static_cast<DWORD>(jsonBody.length()), 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        res.error = "request failed";
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return res;
    }

    _detail::readBody(request, res);

    WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
    return res;
}





inline Response post_cobble(const std::wstring& path, const std::string& jsonBody, int port = 443) {
    Response last;
    last.error = "no hosts";
    for (const auto& host : cobble_config::GetServerHostsW()) {
        last = post(host, port, path, jsonBody, true);
        if (last.statusCode > 0 && last.statusCode < 500) {
            cobble_config::PreferServerHost(std::string(host.begin(), host.end()));
            return last;
        }
        
    }
    return last;
}


inline std::string urlencode(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            result += buf;
        }
    }
    return result;
}


inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}





struct MojangProfile {
    std::string uuid;  
    std::string name;  
    bool found = false;
};

inline std::string formatUUID(const std::string& raw) {
    if (raw.length() != 32) return raw;
    return raw.substr(0, 8) + "-" + raw.substr(8, 4) + "-" + raw.substr(12, 4) + "-" + raw.substr(16, 4) + "-" + raw.substr(20);
}

inline MojangProfile lookupMojangUUID(const std::string& username) {
    MojangProfile profile;
    std::wstring path = OBFUSCATE_WSTRING(L"/users/profiles/minecraft/");
    for (char c : username) path += static_cast<wchar_t>(c);

    auto response = get(OBFUSCATE_WSTRING(L"api.mojang.com").c_str(), 443, path, true);
    if (!response.success) return profile;

    const std::string& body = response.body;
    size_t idPos = body.find("\"id\":");
    if (idPos != std::string::npos) {
        size_t start = body.find('\"', idPos + 5);
        if (start != std::string::npos) {
            size_t end = body.find('\"', start + 1);
            if (end != std::string::npos) {
                profile.uuid = formatUUID(body.substr(start + 1, end - start - 1));
            }
        }
    }
    size_t namePos = body.find("\"name\":");
    if (namePos != std::string::npos) {
        size_t start = body.find('\"', namePos + 7);
        if (start != std::string::npos) {
            size_t end = body.find('\"', start + 1);
            if (end != std::string::npos) {
                profile.name = body.substr(start + 1, end - start - 1);
            }
        }
    }
    profile.found = !profile.uuid.empty();
    return profile;
}

inline std::string lookupMojangName(const std::string& uuid) {
    std::string clean;
    for (char c : uuid) if (c != '-') clean += c;
    if (clean.length() != 32) return "";

    std::wstring path = OBFUSCATE_WSTRING(L"/session/minecraft/profile/");
    for (char c : clean) path += static_cast<wchar_t>(c);

    auto response = get(OBFUSCATE_WSTRING(L"sessionserver.mojang.com").c_str(), 443, path, true);
    if (!response.success) return "";

    const std::string& body = response.body;
    size_t namePos = body.find("\"name\"");
    if (namePos != std::string::npos) {
        size_t colonPos = body.find(':', namePos + 6);
        if (colonPos != std::string::npos) {
            size_t start = body.find('\"', colonPos + 1);
            if (start != std::string::npos) {
                size_t end = body.find('\"', start + 1);
                if (end != std::string::npos) {
                    return body.substr(start + 1, end - start - 1);
                }
            }
        }
    }
    return "";
}

}  
