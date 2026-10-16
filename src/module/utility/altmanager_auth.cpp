#define _CRT_SECURE_NO_WARNINGS


#include <WinSock2.h>
#include <WS2tcpip.h>

#include "altmanager_auth.h"

#include "../../hooks/winsock_hooks.h"
#include "../../util/logger.hpp"
#include "../../util/network/auth_http.hpp"
#include "../../util/network/json.hpp"

#include <shellapi.h>
#include <commdlg.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace AltAuth {
namespace {





constexpr const char* kMsAuthorizeUrl = "https://login.live.com/oauth20_authorize.srf";
constexpr const char* kMsTokenUrl = "https://login.live.com/oauth20_token.srf";
constexpr const char* kXblAuthUrl = "https://user.auth.xboxlive.com/user/authenticate";
constexpr const char* kXstsAuthUrl = "https://xsts.auth.xboxlive.com/xsts/authorize";
constexpr const char* kMcLoginUrl = "https://api.minecraftservices.com/authentication/login_with_xbox";
constexpr const char* kMcProfileUrl = "https://api.minecraftservices.com/minecraft/profile";


constexpr const char* kOauthClientId = "42a60a84-599d-44b2-a7c6-b00cdef1d6a2";
constexpr const char* kOauthRedirect = "http://localhost:25575/callback";
constexpr int kCallbackPort = 25575;


constexpr const char* kDesktopClientId = "00000000402b5328";
constexpr const char* kDesktopRedirect = "https://login.live.com/oauth20_desktop.srf";
constexpr const char* kDesktopScope = "service::user.auth.xboxlive.com::MBI_SSL";


constexpr const char* kCookieOauthSisu =
    "https://login.live.com/oauth20_authorize.srf?redirect_uri=https://sisu.xboxlive.com/connect/oauth/XboxLive"
    "&response_type=token&client_id=000000004420578E&scope=XboxLive.Signin%20XboxLive.offline_access&prompt=none";
constexpr const char* kCookieOauthDesktop =
    "https://login.live.com/oauth20_authorize.srf?client_id=00000000402b5328"
    "&redirect_uri=https%3A%2F%2Flogin.live.com%2Foauth20_desktop.srf&response_type=token"
    "&scope=service%3A%3Auser.auth.xboxlive.com%3A%3AMBI_SSL&prompt=none";

constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36";



const char* const kCookieOrderJshp[] = {
    "__Host-MSAAUTH", "__Host-MSAAUTHP", "JSHP",  "JSH",       "MSPAuth",  "MSPBack", "MSPProf", "MSPRequ",
    "MSPSoftVis",     "MSPOK",           "MSPShared", "MSPPre", "MSPCID",  "MSPOAuthVis", "AMCSecAuth",
    "NAP",            "ANON",            "OParams", "PPLState", "WLSSC",   "uaid",    "pres",    "LOpt"};
const char* const kCookieOrderJsh[] = {
    "__Host-MSAAUTH", "__Host-MSAAUTHP", "JSH",   "JSHP",      "MSPAuth",  "MSPBack", "MSPProf", "MSPRequ",
    "MSPSoftVis",     "MSPOK",           "MSPShared", "MSPPre", "MSPCID",  "MSPOAuthVis", "AMCSecAuth",
    "NAP",            "ANON",            "OParams", "PPLState", "WLSSC",   "uaid",    "pres",    "LOpt"};





std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
        ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
        --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool startsWith(const std::string& s, const char* prefix) {
    const size_t n = strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}





std::string formatUuid(const std::string& raw) {
    std::string clean;
    for (char c : raw) {
        if (c != '-')
            clean += c;
    }
    return clean.size() == 32 ? clean : raw;
}

std::string randomHex(int chars) {
    static const char* hex = "0123456789abcdef";
    LARGE_INTEGER li{};
    QueryPerformanceCounter(&li);
    unsigned long long v = static_cast<unsigned long long>(li.QuadPart) ^ (GetTickCount64() * 2654435761ULL) ^
                           (static_cast<unsigned long long>(GetCurrentThreadId()) << 32);
    std::string out;
    for (int i = 0; i < chars; ++i) {
        v = v * 6364136223846793005ULL + 1442695040888963407ULL;
        out += hex[(v >> 33) & 0xF];
    }
    return out;
}


std::string paramFromQuery(const std::string& query, const std::string& key) {
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        if (amp == std::string::npos)
            amp = query.size();
        const std::string pair = query.substr(pos, amp - pos);
        const size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key)
            return authhttp::urldecode(pair.substr(eq + 1));
        pos = amp + 1;
    }
    return std::string();
}


std::string resolveUrl(const std::string& base, const std::string& location) {
    if (location.empty())
        return std::string();
    if (startsWith(lower(location), "http://") || startsWith(lower(location), "https://"))
        return location;

    const size_t schemeEnd = base.find("://");
    if (schemeEnd == std::string::npos)
        return location;
    const size_t hostEnd = base.find('/', schemeEnd + 3);
    const std::string origin = hostEnd == std::string::npos ? base : base.substr(0, hostEnd);

    if (location[0] == '/')
        return origin + location;

    const size_t lastSlash = base.rfind('/');
    if (lastSlash == std::string::npos || lastSlash < schemeEnd + 3)
        return origin + "/" + location;
    return base.substr(0, lastSlash + 1) + location;
}

std::string hostOf(const std::string& url) {
    const size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        return std::string();
    const size_t start = schemeEnd + 3;
    size_t end = url.find_first_of("/?#", start);
    if (end == std::string::npos)
        end = url.size();
    std::string host = url.substr(start, end - start);
    const size_t colon = host.find(':');
    if (colon != std::string::npos)
        host = host.substr(0, colon);
    return lower(host);
}

std::string pathOf(const std::string& url) {
    const size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        return "/";
    const size_t start = url.find('/', schemeEnd + 3);
    if (start == std::string::npos)
        return "/";
    size_t end = url.find_first_of("?#", start);
    if (end == std::string::npos)
        end = url.size();
    const std::string p = url.substr(start, end - start);
    return p.empty() ? "/" : p;
}

bool isHttps(const std::string& url) { return startsWith(lower(url), "https://"); }





bool parseJson(const std::string& body, json::Value& out) { return json::Parser::parse(body, out); }





std::mutex g_mutex;
std::thread g_worker;
std::atomic<bool> g_busy{false};
std::atomic<bool> g_cancel{false};
std::string g_status;
std::string g_error;
std::string g_authUrl;
std::vector<Account> g_results;






std::thread g_browseThread;
std::atomic<bool> g_browsing{false};
std::atomic<bool> g_browseDone{false};
std::atomic<HWND> g_browseWnd{nullptr};
std::string g_browsePath;

void setStatus(const std::string& s) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status = s;
}

void setError(const std::string& s) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_error = s;
    if (!s.empty())
        log_warn("[AltAuth] %s", s.c_str());
}

void pushResult(const Account& acc) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_results.push_back(acc);
}




void pushResults(const std::vector<Account>& accounts) {
    if (accounts.empty())
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_results.insert(g_results.end(), accounts.begin(), accounts.end());
}

bool cancelled() { return g_cancel.load(std::memory_order_relaxed); }



UINT_PTR CALLBACK browseHookProc(HWND child, UINT msg, WPARAM, LPARAM) {
    if (msg == WM_INITDIALOG)
        g_browseWnd.store(GetParent(child), std::memory_order_release);
    else if (msg == WM_DESTROY)
        g_browseWnd.store(nullptr, std::memory_order_release);
    return 0;  
}





struct MsTokens {
    std::string accessToken;
    std::string refreshToken;
};

bool msTokenRequest(const std::string& form, MsTokens& out, std::string& err) {
    authhttp::Request req;
    req.method = "POST";
    req.url = kMsTokenUrl;
    req.headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
    req.headers.emplace_back("Accept", "application/json");
    req.body = form;

    const authhttp::Response res = authhttp::send(req);
    if (res.status == 0) {
        err = "Microsoft: " + (res.error.empty() ? std::string("no connection") : res.error);
        return false;
    }

    json::Value parsed;
    if (!parseJson(res.body, parsed)) {
        err = "Microsoft: unreadable response (HTTP " + std::to_string(res.status) + ")";
        return false;
    }
    const json::Value& v = parsed;
    if (!res.ok()) {
        std::string desc = v["error_description"].as_string("");
        if (desc.empty())
            desc = v["error"].as_string("HTTP " + std::to_string(res.status));
        err = "Microsoft: " + desc;
        return false;
    }

    out.accessToken = v["access_token"].as_string("");
    out.refreshToken = v["refresh_token"].as_string("");
    if (out.accessToken.empty()) {
        err = "Microsoft: no access token in response";
        return false;
    }
    return true;
}

bool msExchangeCode(const std::string& code, MsTokens& out, std::string& err) {
    std::string form;
    form += "client_id=" + authhttp::urlencode(kOauthClientId);
    form += "&grant_type=authorization_code";
    form += "&code=" + authhttp::urlencode(code);
    form += "&redirect_uri=" + authhttp::urlencode(kOauthRedirect);
    return msTokenRequest(form, out, err);
}

bool msRefreshDesktop(const std::string& refreshToken, MsTokens& out, std::string& err) {
    std::string form;
    form += "client_id=" + authhttp::urlencode(kDesktopClientId);
    form += "&grant_type=refresh_token";
    form += "&redirect_uri=" + authhttp::urlencode(kDesktopRedirect);
    form += "&refresh_token=" + authhttp::urlencode(refreshToken);
    form += "&scope=" + authhttp::urlencode(kDesktopScope);
    return msTokenRequest(form, out, err);
}

bool msRefreshOauth(const std::string& refreshToken, MsTokens& out, std::string& err) {
    std::string form;
    form += "client_id=" + authhttp::urlencode(kOauthClientId);
    form += "&grant_type=refresh_token";
    form += "&refresh_token=" + authhttp::urlencode(refreshToken);
    form += "&redirect_uri=" + authhttp::urlencode(kOauthRedirect);
    return msTokenRequest(form, out, err);
}



bool xblAuthenticate(const std::string& msAccessToken, const char* prefix, std::string& xblToken, std::string& err) {
    std::string body = "{\"Properties\":{\"AuthMethod\":\"RPS\",\"SiteName\":\"user.auth.xboxlive.com\","
                       "\"RpsTicket\":\"";
    body += prefix;
    body += jsonEscape(msAccessToken);
    body += "\"},\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\"}";

    authhttp::Request req;
    req.method = "POST";
    req.url = kXblAuthUrl;
    req.headers.emplace_back("Content-Type", "application/json");
    req.headers.emplace_back("Accept", "application/json");
    req.headers.emplace_back("User-Agent", "Go-http-client/1.1");
    req.headers.emplace_back("X-Xbl-Contract-Version", "0");
    req.body = body;

    const authhttp::Response res = authhttp::send(req);
    if (res.status == 0) {
        err = "Xbox Live: " + (res.error.empty() ? std::string("no connection") : res.error);
        return false;
    }

    json::Value parsed;
    parseJson(res.body, parsed);
    const json::Value& v = parsed;
    if (!res.ok()) {
        std::string msg = v["Message"].as_string("");
        if (msg.empty())
            msg = "HTTP " + std::to_string(res.status);
        err = "Xbox Live: " + msg;
        return false;
    }

    xblToken = v["Token"].as_string("");
    if (xblToken.empty()) {
        err = "Xbox Live: no token in response";
        return false;
    }
    return true;
}

const char* xstsErrorFor(double xerr) {
    if (xerr == 2148916227.0)
        return "This account is banned from Xbox";
    if (xerr == 2148916233.0)
        return "This account has no Xbox profile - create one at xbox.com first";
    if (xerr == 2148916235.0)
        return "Xbox Live is not available in this account's country";
    if (xerr == 2148916236.0 || xerr == 2148916237.0)
        return "This account needs adult verification (South Korea)";
    if (xerr == 2148916238.0)
        return "Child account - it must be added to a Family to use Xbox Live";
    if (xerr == 2148916262.0)
        return "Xbox Live rejected the account (unknown reason)";
    return nullptr;
}

bool xstsAuthorize(const std::string& xblToken, std::string& xstsToken, std::string& uhs, std::string& err) {
    std::string body = "{\"Properties\":{\"SandboxId\":\"RETAIL\",\"UserTokens\":[\"";
    body += jsonEscape(xblToken);
    body += "\"]},\"RelyingParty\":\"rp://api.minecraftservices.com/\",\"TokenType\":\"JWT\"}";

    authhttp::Request req;
    req.method = "POST";
    req.url = kXstsAuthUrl;
    req.headers.emplace_back("Content-Type", "application/json");
    req.headers.emplace_back("Accept", "application/json");
    req.body = body;

    const authhttp::Response res = authhttp::send(req);
    if (res.status == 0) {
        err = "XSTS: " + (res.error.empty() ? std::string("no connection") : res.error);
        return false;
    }

    json::Value parsed;
    parseJson(res.body, parsed);
    const json::Value& v = parsed;
    if (!res.ok()) {
        const double xerr = v["XErr"].as_double(0.0);
        const char* mapped = xstsErrorFor(xerr);
        if (mapped) {
            err = mapped;
        } else {
            std::string msg = v["Message"].as_string("");
            err = "XSTS: " + (msg.empty() ? "HTTP " + std::to_string(res.status) : msg);
        }
        return false;
    }

    xstsToken = v["Token"].as_string("");
    const json::Value& claims = v["DisplayClaims"];
    if (claims["xui"].is_array() && claims["xui"].size() > 0)
        uhs = claims["xui"].array()[0]["uhs"].as_string("");

    if (xstsToken.empty() || uhs.empty()) {
        err = "XSTS: incomplete response";
        return false;
    }
    return true;
}

bool mcLoginWithXbox(const std::string& uhs, const std::string& xstsToken, bool ensureLegacy, std::string& mcToken,
                     std::string& err) {
    std::string identity = "XBL3.0 x=" + uhs + ";" + xstsToken;
    std::string body = "{\"identityToken\":\"" + jsonEscape(identity) + "\"";
    if (ensureLegacy)
        body += ",\"ensureLegacyEnabled\":true";
    body += "}";

    authhttp::Request req;
    req.method = "POST";
    req.url = kMcLoginUrl;
    req.headers.emplace_back("Content-Type", "application/json");
    req.headers.emplace_back("Accept", "application/json");
    req.body = body;

    const authhttp::Response res = authhttp::send(req);
    if (res.status == 0) {
        err = "Minecraft: " + (res.error.empty() ? std::string("no connection") : res.error);
        return false;
    }

    json::Value parsed;
    parseJson(res.body, parsed);
    const json::Value& v = parsed;
    if (!res.ok()) {
        std::string msg = v["errorMessage"].as_string("");
        if (msg.empty())
            msg = v["error"].as_string("HTTP " + std::to_string(res.status));
        err = "Minecraft: " + msg;
        return false;
    }

    mcToken = v["access_token"].as_string("");
    if (mcToken.empty()) {
        err = "Minecraft: no access token in response";
        return false;
    }
    return true;
}

bool mcProfile(const std::string& mcToken, std::string& name, std::string& uuid, std::string& err) {
    authhttp::Request req;
    req.method = "GET";
    req.url = kMcProfileUrl;
    req.headers.emplace_back("Authorization", "Bearer " + mcToken);
    req.headers.emplace_back("Accept", "application/json");

    const authhttp::Response res = authhttp::send(req);
    if (res.status == 0) {
        err = "Profile: " + (res.error.empty() ? std::string("no connection") : res.error);
        return false;
    }
    if (res.status == 404) {
        err = "This account does not own Minecraft: Java Edition";
        return false;
    }
    if (res.status == 401) {
        err = "The access token was rejected (expired or invalid)";
        return false;
    }

    json::Value parsed;
    if (!res.ok() || !parseJson(res.body, parsed)) {
        err = "Profile: HTTP " + std::to_string(res.status);
        return false;
    }
    const json::Value& v = parsed;

    name = v["name"].as_string("");
    uuid = formatUuid(v["id"].as_string(""));
    if (name.empty()) {
        err = "Profile: no username in response";
        return false;
    }
    return true;
}



bool msTokenToAccount(const std::string& msAccessToken, Account& out, std::string& err) {
    setStatus("Xbox Live...");
    std::string xbl;
    std::string firstErr;
    const char* prefixes[] = {"d=", "t="};
    bool gotXbl = false;
    for (const char* prefix : prefixes) {
        if (cancelled())
            return false;
        std::string localErr;
        if (xblAuthenticate(msAccessToken, prefix, xbl, localErr)) {
            gotXbl = true;
            break;
        }
        if (firstErr.empty())
            firstErr = localErr;
    }
    if (!gotXbl) {
        err = firstErr;
        return false;
    }

    if (cancelled())
        return false;
    setStatus("XSTS...");
    std::string xsts, uhs;
    if (!xstsAuthorize(xbl, xsts, uhs, err))
        return false;

    if (cancelled())
        return false;
    setStatus("Minecraft services...");
    std::string mcToken;
    if (!mcLoginWithXbox(uhs, xsts, false, mcToken, err)) {
        std::string legacyErr;
        if (!mcLoginWithXbox(uhs, xsts, true, mcToken, legacyErr))
            return false;
    }

    if (cancelled())
        return false;
    setStatus("Fetching profile...");
    if (!mcProfile(mcToken, out.username, out.uuid, err))
        return false;

    out.accessToken = mcToken;
    return true;
}





const char* kCallbackHtml =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>Cobble</title></head>"
    "<body style=\"background:#111;color:#ff942a;font-family:Segoe UI,sans-serif;text-align:center;padding-top:18vh\">"
    "<h2>Signed in</h2><p style=\"color:#888\">You can close this tab and go back to Minecraft.</p></body></html>";

void sendHttp(SOCKET client, const char* status, const char* contentType, const std::string& payload) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << "\r\n"
       << "Content-Type: " << contentType << "\r\n"
       << "Content-Length: " << payload.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << payload;
    const std::string out = ss.str();
    send(client, out.c_str(), static_cast<int>(out.size()), 0);
}



bool waitForCallback(const std::string& expectedState, std::string& code, std::string& err, int timeoutSeconds) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        err = "Winsock init failed";
        return false;
    }
    struct WsaGuard {
        ~WsaGuard() { WSACleanup(); }
    } wsaGuard;

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        err = "Could not create the callback socket";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(kCallbackPort));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server);
        err = "Port 25575 is already in use - close the other launcher and retry";
        return false;
    }
    if (listen(server, 4) == SOCKET_ERROR) {
        closesocket(server);
        err = "Could not listen on port 25575";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    bool done = false;
    bool ok = false;

    while (!done) {
        if (cancelled()) {
            err = "Cancelled";
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            err = "Timed out waiting for the browser sign-in";
            break;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(server, &readSet);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 250000;

        const int ready = select(0, &readSet, nullptr, nullptr, &tv);
        if (ready <= 0)
            continue;

        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            continue;

        DWORD recvTimeout = 3000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeout), sizeof(recvTimeout));

        std::string request;
        char buf[2048];
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
            const int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0)
                break;
            request.append(buf, n);
        }

        
        std::string target;
        const size_t sp1 = request.find(' ');
        if (sp1 != std::string::npos) {
            const size_t sp2 = request.find(' ', sp1 + 1);
            if (sp2 != std::string::npos)
                target = request.substr(sp1 + 1, sp2 - sp1 - 1);
        }

        if (!startsWith(target, "/callback")) {
            sendHttp(client, "404 Not Found", "text/plain", "not found");
            closesocket(client);
            continue;
        }

        std::string query;
        const size_t q = target.find('?');
        if (q != std::string::npos)
            query = target.substr(q + 1);

        const std::string state = paramFromQuery(query, "state");
        const std::string gotCode = paramFromQuery(query, "code");
        const std::string oauthErr = paramFromQuery(query, "error");

        if (!expectedState.empty() && state != expectedState) {
            sendHttp(client, "400 Bad Request", "text/plain", "state mismatch");
            closesocket(client);
            err = "OAuth state mismatch - sign-in aborted";
            done = true;
            continue;
        }

        if (!oauthErr.empty()) {
            std::string desc = paramFromQuery(query, "error_description");
            sendHttp(client, "200 OK", "text/html; charset=utf-8", kCallbackHtml);
            closesocket(client);
            err = "Microsoft: " + (desc.empty() ? oauthErr : desc);
            done = true;
            continue;
        }

        if (gotCode.empty()) {
            sendHttp(client, "400 Bad Request", "text/plain", "no code");
            closesocket(client);
            err = "Microsoft returned no authorization code";
            done = true;
            continue;
        }

        sendHttp(client, "200 OK", "text/html; charset=utf-8", kCallbackHtml);
        closesocket(client);
        code = gotCode;
        ok = true;
        done = true;
    }

    closesocket(server);
    return ok;
}





struct StoredCookie {
    std::string domain;  
    std::string path = "/";
    std::string name;
    std::string value;
    bool secure = true;
};

bool isRelevantDomain(const std::string& domain) {
    const std::string d = lower(domain);
    static const char* kInteresting[] = {"live.com", "microsoftonline.com", "microsoft.com",
                                         "xboxlive.com", "minecraft.net", "mojang.com"};
    for (const char* needle : kInteresting) {
        if (d.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

std::string normalizeDomain(std::string domain) {
    domain = lower(trim(domain));
    if (domain.empty())
        return domain;
    if (domain[0] != '.' && domain.find('.') != std::string::npos)
        domain = "." + domain;
    return domain;
}

class CookieStore {
public:
    void add(const StoredCookie& c) {
        if (c.name.empty() || c.value.empty())
            return;
        if (c.value == "Disabled")  
            return;
        for (auto& existing : cookies_) {
            if (existing.name == c.name && existing.domain == c.domain && existing.path == c.path) {
                existing.value = c.value;
                existing.secure = c.secure;
                return;
            }
        }
        cookies_.push_back(c);
    }

    bool empty() const { return cookies_.empty(); }
    size_t size() const { return cookies_.size(); }

    bool hasAnyAuthCookie() const {
        for (const auto& c : cookies_) {
            if (c.name == "__Host-MSAAUTH" || c.name == "__Host-MSAAUTHP" || c.name == "JSH" || c.name == "JSHP")
                return true;
        }
        return false;
    }

    
    void mergeSetCookie(const std::string& header, const std::string& requestHost) {
        const size_t semi = header.find(';');
        const std::string pair = trim(header.substr(0, semi == std::string::npos ? header.size() : semi));
        const size_t eq = pair.find('=');
        if (eq == std::string::npos)
            return;

        StoredCookie c;
        c.name = trim(pair.substr(0, eq));
        c.value = trim(pair.substr(eq + 1));
        c.domain = normalizeDomain(requestHost);
        c.path = "/";
        c.secure = true;

        if (semi != std::string::npos) {
            for (const std::string& rawAttr : splitOn(header.substr(semi + 1), ';')) {
                const std::string attr = trim(rawAttr);
                const std::string key = lower(attr.substr(0, attr.find('=')));
                std::string val;
                const size_t aeq = attr.find('=');
                if (aeq != std::string::npos)
                    val = trim(attr.substr(aeq + 1));
                if (key == "domain" && !val.empty())
                    c.domain = normalizeDomain(val);
                else if (key == "path" && !val.empty())
                    c.path = val;
                else if (key == "secure")
                    c.secure = true;
            }
        }

        if (c.value.empty() || c.value == "\"\"")
            return;
        if (!isRelevantDomain(c.domain))
            return;
        add(c);
    }

    
    std::string headerFor(const std::string& host, const std::string& path, bool https, const char* const* order,
                          size_t orderCount) const {
        std::vector<const StoredCookie*> matching;
        for (const auto& c : cookies_) {
            if (c.secure && !https)
                continue;
            if (!domainMatches(host, c.domain))
                continue;
            if (!pathMatches(path, c.path))
                continue;
            matching.push_back(&c);
        }
        if (matching.empty())
            return std::string();

        std::string out;
        std::vector<bool> used(matching.size(), false);

        for (size_t i = 0; i < orderCount; ++i) {
            for (size_t j = 0; j < matching.size(); ++j) {
                if (used[j] || matching[j]->name != order[i])
                    continue;
                used[j] = true;
                if (!out.empty())
                    out += "; ";
                out += matching[j]->name + "=" + matching[j]->value;
            }
        }
        for (size_t j = 0; j < matching.size(); ++j) {
            if (used[j])
                continue;
            if (!out.empty())
                out += "; ";
            out += matching[j]->name + "=" + matching[j]->value;
        }
        return out;
    }

    std::string serialize() const {
        json::Value root;
        root["v"] = 2;
        json::Value arr;
        arr.array();
        for (const auto& c : cookies_) {
            json::Value e;
            e["domain"] = c.domain;
            e["path"] = c.path;
            e["name"] = c.name;
            e["value"] = c.value;
            e["secure"] = c.secure;
            arr.push_back(e);
        }
        root["cookies"] = arr;
        return root.serialize();
    }

    static CookieStore deserialize(const std::string& blob) {
        CookieStore store;
        json::Value parsed;
        if (!parseJson(blob, parsed))
            return store;
        const json::Value& root = parsed;  
        if (!root["cookies"].is_array())
            return store;
        for (const auto& e : root["cookies"].array()) {
            StoredCookie c;
            c.domain = e["domain"].as_string("");
            c.path = e["path"].as_string("/");
            c.name = e["name"].as_string("");
            c.value = e["value"].as_string("");
            c.secure = e["secure"].as_bool(true);
            store.add(c);
        }
        return store;
    }

    static std::vector<std::string> splitOn(const std::string& s, char sep) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (c == sep) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        out.push_back(cur);
        return out;
    }

private:
    static bool domainMatches(const std::string& host, const std::string& domain) {
        if (domain.empty())
            return false;
        const std::string d = domain[0] == '.' ? domain.substr(1) : domain;
        if (host == d)
            return true;
        return host.size() > d.size() && host.compare(host.size() - d.size() - 1, d.size() + 1, "." + d) == 0;
    }

    static bool pathMatches(const std::string& reqPath, const std::string& cookiePath) {
        if (cookiePath.empty() || cookiePath == "/")
            return true;
        if (reqPath.compare(0, cookiePath.size(), cookiePath) != 0)
            return false;
        return reqPath.size() == cookiePath.size() || reqPath[cookiePath.size()] == '/' || cookiePath.back() == '/';
    }

    std::vector<StoredCookie> cookies_;
};


bool parseNetscapeCookies(const std::string& text, CookieStore& out) {
    bool any = false;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#')
            continue;

        const std::vector<std::string> parts = CookieStore::splitOn(line, '\t');
        if (parts.size() < 7)
            continue;

        StoredCookie c;
        c.domain = normalizeDomain(parts[0]);
        c.path = parts[2].empty() ? "/" : parts[2];
        c.secure = lower(trim(parts[3])) == "true";
        c.name = trim(parts[5]);
        c.value = trim(parts[6]);
        if (!isRelevantDomain(c.domain))
            continue;
        out.add(c);
        any = true;
    }
    return any;
}


bool parseJsonCookies(const std::string& text, CookieStore& out) {
    json::Value parsed;
    if (!parseJson(text, parsed))
        return false;
    const json::Value& root = parsed;

    const json::Value* arr = nullptr;
    if (root.is_array())
        arr = &root;
    else if (root["cookies"].is_array())
        arr = &root["cookies"];
    if (!arr)
        return false;

    
    const double nowSeconds =
        static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count());

    bool any = false;
    for (const auto& e : arr->array()) {
        if (!e.is_object())
            continue;
        StoredCookie c;
        c.name = e["name"].as_string("");
        c.value = e["value"].as_string("");
        if (c.name.empty() || c.value.empty())
            continue;

        const double expires = e["expirationDate"].as_double(0.0);
        if (expires > 0.0 && expires < nowSeconds)
            continue;

        std::string domain = e["domain"].as_string("");
        if (domain.empty())
            domain = e["host"].as_string("");
        c.domain = normalizeDomain(domain);
        c.path = e["path"].as_string("/");
        if (c.path.empty())
            c.path = "/";
        c.secure = e["secure"].as_bool(true);

        if (!isRelevantDomain(c.domain))
            continue;
        out.add(c);
        any = true;
    }
    return any;
}



bool parseLooseCookies(const std::string& text, CookieStore& out) {
    bool any = false;
    std::vector<std::string> chunks;
    for (const std::string& line : splitLines(text)) {
        for (const std::string& piece : CookieStore::splitOn(line, ';'))
            chunks.push_back(piece);
    }
    for (const std::string& rawChunk : chunks) {
        const std::string chunk = trim(rawChunk);
        const size_t eq = chunk.find('=');
        if (eq == std::string::npos || eq == 0)
            continue;
        StoredCookie c;
        c.name = trim(chunk.substr(0, eq));
        c.value = trim(chunk.substr(eq + 1));
        c.domain = ".live.com";
        c.path = "/";
        c.secure = true;
        if (c.name.empty() || c.value.empty() || c.name.find(' ') != std::string::npos)
            continue;
        out.add(c);
        any = true;
    }
    return any;
}

CookieStore parseCookieBlob(const std::string& text) {
    CookieStore store;
    const std::string t = trim(text);
    if (t.empty())
        return store;

    if (t[0] == '{' || t[0] == '[') {
        if (parseJsonCookies(t, store) && !store.empty())
            return store;
    }
    if (parseNetscapeCookies(t, store) && !store.empty())
        return store;
    if (store.empty())
        parseLooseCookies(t, store);
    return store;
}




std::string followOAuthRedirects(const std::string& startUrl, int maxHops, CookieStore& store, const char* const* order,
                                 size_t orderCount, std::string& err) {
    std::string url = startUrl;

    for (int hop = 0; hop < maxHops; ++hop) {
        if (cancelled()) {
            err = "Cancelled";
            return std::string();
        }

        const std::string host = hostOf(url);
        const std::string path = pathOf(url);

        authhttp::Request req;
        req.method = "GET";
        req.url = url;
        req.userAgent = kBrowserUserAgent;
        req.followRedirects = false;
        req.headers.emplace_back("User-Agent", kBrowserUserAgent);
        req.headers.emplace_back("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        req.headers.emplace_back("Accept-Language", "en-US,en;q=0.9");
        req.headers.emplace_back("Connection", "keep-alive");

        const std::string cookieHeader = store.headerFor(host, path, isHttps(url), order, orderCount);
        if (!cookieHeader.empty())
            req.headers.emplace_back("Cookie", cookieHeader);

        const authhttp::Response res = authhttp::send(req);
        if (res.status == 0) {
            err = res.error.empty() ? "no connection" : res.error;
            return std::string();
        }

        for (const std::string& sc : res.setCookies())
            store.mergeSetCookie(sc, host);

        const std::string location = res.location();
        if (!location.empty()) {
            const std::string absolute = resolveUrl(url, location);

            
            
            const size_t hash = absolute.find('#');
            if (hash != std::string::npos) {
                const std::string token = paramFromQuery(absolute.substr(hash + 1), "access_token");
                if (!token.empty())
                    return token;
            }
            const size_t q = absolute.find('?');
            if (q != std::string::npos) {
                std::string query = absolute.substr(q + 1);
                const size_t cut = query.find('#');
                if (cut != std::string::npos)
                    query = query.substr(0, cut);
                const std::string token = paramFromQuery(query, "access_token");
                if (!token.empty())
                    return token;
                const std::string oauthErr = paramFromQuery(query, "error");
                if (!oauthErr.empty()) {
                    const std::string desc = paramFromQuery(query, "error_description");
                    err = desc.empty() ? oauthErr : desc;
                    return std::string();
                }
            }

            if (res.isRedirect()) {
                url = absolute;
                continue;
            }
        }

        if (res.isRedirect()) {
            err = "redirect without a Location header";
            return std::string();
        }

        
        
        err = "the cookies are no longer valid for a silent sign-in";
        return std::string();
    }

    err = "too many redirects";
    return std::string();
}





bool isTokenChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.';
}



std::string findJwt(const std::string& s) {
    size_t pos = 0;
    while ((pos = s.find("eyJ", pos)) != std::string::npos) {
        size_t end = pos;
        int dots = 0;
        while (end < s.size() && isTokenChar(s[end])) {
            if (s[end] == '.')
                ++dots;
            ++end;
        }
        const size_t len = end - pos;
        if (dots >= 2 && len >= 20)
            return s.substr(pos, len);
        pos = end > pos ? end : pos + 1;
    }
    return std::string();
}

struct TokenEntry {
    std::string token;
    std::string hintName;
    std::string hintUuid;
};


bool looksLikeUuid(const std::string& s) {
    if (s.size() == 32) {
        for (char c : s) {
            if (!isxdigit(static_cast<unsigned char>(c)))
                return false;
        }
        return true;
    }
    if (s.size() != 36)
        return false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return false;
        } else if (!isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

std::vector<TokenEntry> parseTokenBlob(const std::string& raw) {
    std::vector<TokenEntry> out;
    for (const std::string& rawLine : splitLines(raw)) {
        std::string line = trim(rawLine);
        if (line.empty() || line.size() > 50000)
            continue;

        
        const size_t colon = line.find(':');
        if (colon != std::string::npos && colon <= 16) {
            const std::string prefix = lower(trim(line.substr(0, colon)));
            if (prefix == "accesstoken" || prefix == "mctoken" || prefix == "token")
                line = trim(line.substr(colon + 1));
        }

        
        TokenEntry entry;
        std::vector<std::string> parts = CookieStore::splitOn(line, '|');
        for (size_t i = 1; i < parts.size(); ++i) {
            std::string field = trim(parts[i]);
            if (field.empty())
                continue;
            const size_t fc = field.find(':');
            if (fc != std::string::npos) {
                const std::string key = lower(trim(field.substr(0, fc)));
                if (key == "mcname" || key == "name" || key == "username")
                    field = trim(field.substr(fc + 1));
            }
            if (looksLikeUuid(field))
                entry.hintUuid = formatUuid(field);
            else if (entry.hintName.empty())
                entry.hintName = field;
        }

        entry.token = findJwt(parts.empty() ? line : trim(parts[0]));
        if (entry.token.empty())
            continue;

        bool duplicate = false;
        for (const auto& existing : out) {
            if (existing.token == entry.token) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            out.push_back(entry);
    }
    return out;
}







bool startWorker(std::function<void()> job) {
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true))
        return false;

    if (g_worker.joinable())
        g_worker.join();

    g_cancel.store(false, std::memory_order_relaxed);
    setError("");
    setStatus("Starting...");
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_authUrl.clear();
    }

    g_worker = std::thread([job]() {
        
        
        
        WinsockHooks::ExemptCurrentThread();
        job();
        WinsockHooks::UnexemptCurrentThread();
        
        
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_authUrl.clear();
        }
        g_busy.store(false, std::memory_order_release);
    });
    return true;
}

void runMicrosoft() {
    const std::string state = randomHex(24);

    std::string url = kMsAuthorizeUrl;
    url += "?client_id=" + authhttp::urlencode(kOauthClientId);
    url += "&response_type=code";
    url += "&redirect_uri=" + authhttp::urlencode(kOauthRedirect);
    url += "&scope=" + authhttp::urlencode("XboxLive.signin XboxLive.offline_access");
    url += "&state=" + authhttp::urlencode(state);
    url += "&prompt=select_account";

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_authUrl = url;
    }

    setStatus("Waiting for the browser sign-in...");
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::string code, err;
    if (!waitForCallback(state, code, err, 180)) {
        setStatus("");
        setError(err.empty() ? "Sign-in cancelled" : err);
        return;
    }

    setStatus("Exchanging the authorization code...");
    MsTokens tokens;
    if (!msExchangeCode(code, tokens, err)) {
        setStatus("");
        setError(err);
        return;
    }

    Account acc;
    acc.kind = Kind::MICROSOFT;
    acc.secret = tokens.refreshToken;
    if (!msTokenToAccount(tokens.accessToken, acc, err)) {
        setStatus("");
        setError(err.empty() ? "Microsoft sign-in failed" : err);
        return;
    }

    pushResult(acc);
    setStatus("");
    log_ok("[AltAuth] Microsoft login OK: %s", acc.username.c_str());
}

void runRefresh(std::string refreshToken) {
    setStatus("Refreshing the Microsoft token...");

    MsTokens tokens;
    std::string err;
    
    
    if (!msRefreshDesktop(refreshToken, tokens, err)) {
        std::string secondErr;
        if (!msRefreshOauth(refreshToken, tokens, secondErr)) {
            setStatus("");
            setError(err);
            return;
        }
    }

    Account acc;
    acc.kind = Kind::REFRESH;
    acc.secret = tokens.refreshToken.empty() ? refreshToken : tokens.refreshToken;
    if (!msTokenToAccount(tokens.accessToken, acc, err)) {
        setStatus("");
        setError(err.empty() ? "Refresh token login failed" : err);
        return;
    }

    pushResult(acc);
    setStatus("");
    log_ok("[AltAuth] Refresh token login OK: %s", acc.username.c_str());
}

void runTokens(std::string raw) {
    const std::vector<TokenEntry> entries = parseTokenBlob(raw);
    if (entries.empty()) {
        setStatus("");
        setError("No access token found in the pasted text");
        return;
    }

    int okCount = 0;
    std::string lastErr;
    std::vector<Account> batch;

    for (size_t i = 0; i < entries.size(); ++i) {
        if (cancelled())
            break;

        char status[64];
        snprintf(status, sizeof(status), "Checking token %d/%d...", static_cast<int>(i + 1),
                 static_cast<int>(entries.size()));
        setStatus(status);

        Account acc;
        acc.kind = Kind::TOKEN;
        acc.accessToken = entries[i].token;

        std::string err;
        if (mcProfile(entries[i].token, acc.username, acc.uuid, err)) {
            batch.push_back(acc);
            ++okCount;
            continue;
        }

        
        
        if (!entries[i].hintName.empty()) {
            acc.username = entries[i].hintName;
            acc.uuid = entries[i].hintUuid;
            batch.push_back(acc);
            ++okCount;
            continue;
        }
        lastErr = err;
    }

    
    pushResults(batch);
    setStatus("");
    if (okCount == 0) {
        setError(lastErr.empty() ? "None of the tokens were accepted" : lastErr);
        return;
    }
    log_ok("[AltAuth] Imported %d/%d access tokens", okCount, static_cast<int>(entries.size()));
}

void runCookies(std::string raw) {
    setStatus("Reading cookies...");

    CookieStore base = parseCookieBlob(raw);
    if (base.empty()) {
        setStatus("");
        setError("No usable cookies found (expected Netscape .txt or a JSON export)");
        return;
    }
    if (!base.hasAnyAuthCookie()) {
        setStatus("");
        setError("The export has no MSA session cookie (__Host-MSAAUTH / JSH / JSHP)");
        return;
    }

    struct Attempt {
        const char* url;
        const char* const* order;
        size_t count;
    };
    const Attempt attempts[] = {
        {kCookieOauthSisu, kCookieOrderJshp, sizeof(kCookieOrderJshp) / sizeof(kCookieOrderJshp[0])},
        {kCookieOauthSisu, kCookieOrderJsh, sizeof(kCookieOrderJsh) / sizeof(kCookieOrderJsh[0])},
        {kCookieOauthDesktop, kCookieOrderJshp, sizeof(kCookieOrderJshp) / sizeof(kCookieOrderJshp[0])},
        {kCookieOauthDesktop, kCookieOrderJsh, sizeof(kCookieOrderJsh) / sizeof(kCookieOrderJsh[0])},
    };

    std::string msAccessToken;
    std::string lastErr;
    CookieStore working;

    for (size_t i = 0; i < sizeof(attempts) / sizeof(attempts[0]) && msAccessToken.empty(); ++i) {
        if (cancelled())
            break;
        char status[64];
        snprintf(status, sizeof(status), "Silent sign-in (%d/4)...", static_cast<int>(i + 1));
        setStatus(status);

        working = base;  
        std::string err;
        msAccessToken = followOAuthRedirects(attempts[i].url, 12, working, attempts[i].order, attempts[i].count, err);
        if (msAccessToken.empty() && lastErr.empty())
            lastErr = err;
    }

    if (msAccessToken.empty()) {
        setStatus("");
        setError(lastErr.empty() ? "Cookie sign-in failed" : "Cookies: " + lastErr);
        return;
    }

    Account acc;
    acc.kind = Kind::COOKIE;
    acc.secret = working.serialize();

    std::string err;
    if (!msTokenToAccount(msAccessToken, acc, err)) {
        setStatus("");
        setError(err.empty() ? "Cookie sign-in failed" : err);
        return;
    }

    pushResult(acc);
    setStatus("");
    log_ok("[AltAuth] Cookie login OK: %s", acc.username.c_str());
}

}  





bool IsBusy() { return g_busy.load(std::memory_order_acquire); }

std::string Status() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_status;
}

std::string LastError() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_error;
}

void ClearError() { setError(""); }

void Cancel() { g_cancel.store(true, std::memory_order_relaxed); }

std::vector<Account> TakeResults() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<Account> out;
    out.swap(g_results);
    return out;
}

std::string AuthUrl() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_authUrl;
}

bool BeginMicrosoft() { return startWorker([]() { runMicrosoft(); }); }

bool BeginRefresh(const std::string& refreshToken) {
    const std::string token = trim(refreshToken);
    if (token.size() < 10) {
        setError("That does not look like a refresh token");
        return false;
    }
    return startWorker([token]() { runRefresh(token); });
}

bool BeginTokens(const std::string& rawTokenBlob) {
    const std::string raw = rawTokenBlob;
    if (parseTokenBlob(raw).empty()) {
        setError("No access token found in the pasted text");
        return false;
    }
    return startWorker([raw]() { runTokens(raw); });
}

bool BeginCookies(const std::string& rawCookieData) {
    const std::string raw = rawCookieData;
    if (trim(raw).empty()) {
        setError("Paste a cookie export or load a file first");
        return false;
    }
    return startWorker([raw]() { runCookies(raw); });
}

int CountTokens(const std::string& raw) { return static_cast<int>(parseTokenBlob(raw).size()); }

std::string ReadFile(const std::string& path, std::string& errorOut) {
    errorOut.clear();
    std::string clean = trim(path);
    
    
    if (clean.size() >= 2 && clean.front() == '"' && clean.back() == '"')
        clean = trim(clean.substr(1, clean.size() - 2));
    if (clean.empty()) {
        errorOut = "No file path given";
        return std::string();
    }

    std::ifstream file(clean.c_str(), std::ios::binary);
    if (!file) {
        errorOut = "Could not open " + clean;
        return std::string();
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string data = ss.str();
    if (data.empty())
        errorOut = "The file is empty";
    return data;
}

bool BrowseForFile() {
    bool expected = false;
    if (!g_browsing.compare_exchange_strong(expected, true))
        return false;

    
    if (g_browseThread.joinable())
        g_browseThread.join();

    g_browseDone.store(false, std::memory_order_relaxed);
    g_browseThread = std::thread([]() {
        char path[2048] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        
        
        
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = "Token / cookie exports (*.txt;*.json)\0*.txt;*.json\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = sizeof(path);
        ofn.lpstrTitle = "Cobble - pick a token or cookie file";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_ENABLEHOOK |
                    OFN_ENABLESIZING;
        ofn.lpfnHook = &browseHookProc;

        const bool picked = GetOpenFileNameA(&ofn) != FALSE;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_browsePath = picked ? std::string(path) : std::string();
        }
        g_browseWnd.store(nullptr, std::memory_order_release);
        g_browseDone.store(true, std::memory_order_release);
        g_browsing.store(false, std::memory_order_release);
    });
    return true;
}

bool IsBrowsing() { return g_browsing.load(std::memory_order_acquire); }

bool TakeBrowsedFile(std::string& pathOut) {
    if (!g_browseDone.load(std::memory_order_acquire))
        return false;
    g_browseDone.store(false, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_browsePath.empty())
        return false;  
    pathOut.swap(g_browsePath);
    g_browsePath.clear();
    return true;
}

void Shutdown() {
    Cancel();
    if (g_worker.joinable())
        g_worker.join();
    g_busy.store(false, std::memory_order_release);

    
    
    
    
    
    if (g_browseThread.joinable()) {
        for (int i = 0; i < 1000 && g_browsing.load(std::memory_order_acquire); ++i) {
            HWND dlg = g_browseWnd.load(std::memory_order_acquire);
            if (dlg)
                PostMessageA(dlg, WM_CLOSE, 0, 0);
            Sleep(10);
        }
        g_browseThread.join();
    }
    g_browsing.store(false, std::memory_order_release);
}

}  
