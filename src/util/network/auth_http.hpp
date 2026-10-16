#pragma once

#include <Windows.h>
#include <winhttp.h>

#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")










namespace authhttp {

inline std::wstring widen(const std::string& s) {
    if (s.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0)
        return std::wstring();
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

inline std::string narrow(const std::wstring& s) {
    if (s.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return std::string();
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n, nullptr, nullptr);
    return out;
}

inline std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline std::string urlencode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

inline std::string urldecode(const std::string& s) {
    auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size()) {
            const int hi = hexval(s[i + 1]);
            const int lo = hexval(s[i + 2]);
            if (hi < 0 || lo < 0) {
                out += s[i];
            } else {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

struct Response {
    int status = 0;         
    std::string body;
    std::string error;
    std::vector<std::pair<std::string, std::string>> headers;  

    bool ok() const { return status >= 200 && status < 300; }
    bool isRedirect() const { return status >= 300 && status < 400; }

    std::string header(const char* lowerName) const {
        for (const auto& kv : headers) {
            if (kv.first == lowerName)
                return kv.second;
        }
        return std::string();
    }

    std::vector<std::string> headerAll(const char* lowerName) const {
        std::vector<std::string> out;
        for (const auto& kv : headers) {
            if (kv.first == lowerName)
                out.push_back(kv.second);
        }
        return out;
    }

    std::string location() const { return header("location"); }
    std::vector<std::string> setCookies() const { return headerAll("set-cookie"); }
};

struct Request {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool followRedirects = false;
    
    
    
    DWORD timeoutMs = 15000;
    std::string userAgent = "Mozilla/5.0";
};

namespace _detail {

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h)
            WinHttpCloseHandle(h);
    }
};


inline std::wstring buildHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::string flat;
    for (const auto& kv : headers) {
        if (kv.first.empty())
            continue;
        flat += kv.first;
        flat += ": ";
        flat += kv.second;
        flat += "\r\n";
    }
    return widen(flat);
}

inline void parseRawHeaders(const std::wstring& raw, Response& res) {
    
    size_t pos = 0;
    bool first = true;
    while (pos < raw.size()) {
        size_t end = raw.find(L"\r\n", pos);
        if (end == std::wstring::npos)
            end = raw.size();
        const std::wstring line = raw.substr(pos, end - pos);
        pos = end + 2;
        if (line.empty()) {
            if (first)
                continue;
            break;
        }
        if (first) {  
            first = false;
            continue;
        }
        const size_t colon = line.find(L':');
        if (colon == std::wstring::npos)
            continue;
        std::string name = lower(narrow(line.substr(0, colon)));
        size_t vs = colon + 1;
        while (vs < line.size() && (line[vs] == L' ' || line[vs] == L'\t'))
            ++vs;
        res.headers.emplace_back(std::move(name), narrow(line.substr(vs)));
    }
}

}  

inline Response send(const Request& req) {
    Response res;

    const std::wstring wurl = widen(req.url);
    if (wurl.empty()) {
        res.error = "empty url";
        return res;
    }

    std::vector<wchar_t> hostBuf(wurl.size() + 2, 0);
    std::vector<wchar_t> pathBuf(wurl.size() + 2, 0);
    std::vector<wchar_t> extraBuf(wurl.size() + 2, 0);

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostBuf.data();
    uc.dwHostNameLength = static_cast<DWORD>(hostBuf.size() - 1);
    uc.lpszUrlPath = pathBuf.data();
    uc.dwUrlPathLength = static_cast<DWORD>(pathBuf.size() - 1);
    uc.lpszExtraInfo = extraBuf.data();
    uc.dwExtraInfoLength = static_cast<DWORD>(extraBuf.size() - 1);

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc)) {
        res.error = "malformed url";
        return res;
    }

    const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::wstring object(pathBuf.data());
    object += extraBuf.data();
    if (object.empty())
        object = L"/";

    _detail::Handle session;
    session.h = WinHttpOpen(widen(req.userAgent).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) {
        res.error = "WinHttpOpen failed";
        return res;
    }
    WinHttpSetTimeouts(session.h, static_cast<int>(req.timeoutMs), static_cast<int>(req.timeoutMs),
                       static_cast<int>(req.timeoutMs), static_cast<int>(req.timeoutMs));

    _detail::Handle connect;
    connect.h = WinHttpConnect(session.h, hostBuf.data(), uc.nPort, 0);
    if (!connect.h) {
        res.error = "WinHttpConnect failed";
        return res;
    }

    _detail::Handle request;
    request.h = WinHttpOpenRequest(connect.h, widen(req.method).c_str(), object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request.h) {
        res.error = "WinHttpOpenRequest failed";
        return res;
    }

    
    
    
    
    DWORD disable = WINHTTP_DISABLE_COOKIES;
    if (!req.followRedirects)
        disable |= WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(request.h, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));

    const std::wstring headers = _detail::buildHeaders(req.headers);

    if (!WinHttpSendRequest(request.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            headers.empty() ? 0 : static_cast<DWORD>(-1L),
                            req.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(req.body.c_str()),
                            static_cast<DWORD>(req.body.size()), static_cast<DWORD>(req.body.size()), 0)) {
        res.error = "send failed (" + std::to_string(GetLastError()) + ")";
        return res;
    }

    if (!WinHttpReceiveResponse(request.h, nullptr)) {
        res.error = "no response (" + std::to_string(GetLastError()) + ")";
        return res;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    res.status = static_cast<int>(statusCode);

    DWORD rawSize = 0;
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &rawSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (rawSize > 0) {
        std::vector<wchar_t> raw(rawSize / sizeof(wchar_t) + 1, 0);
        if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(),
                                &rawSize, WINHTTP_NO_HEADER_INDEX)) {
            _detail::parseRawHeaders(std::wstring(raw.data()), res);
        }
    }

    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.h, &available) && available > 0) {
        std::vector<char> buffer(available + 1, 0);
        DWORD read = 0;
        if (!WinHttpReadData(request.h, buffer.data(), available, &read) || read == 0)
            break;
        res.body.append(buffer.data(), read);
        if (res.body.size() > 8u * 1024u * 1024u)  
            break;
    }

    return res;
}

}  
