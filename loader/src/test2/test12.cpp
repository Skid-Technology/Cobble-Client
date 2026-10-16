#include "test13.h"
#include <Windows.h>
#include <wincrypt.h>
#include <wininet.h>
#include <cctype>
#include <ctime>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "../test6/test30.hpp"
#include "../test6/test32.hpp"
#include "../test5/test28.h"
#include "../test6/test31.hpp"
#include "../test6/test35.h"
#include "../test3/test21.h"
#include "test11.h"
#include "generated/keys.h"
#include "generated/version.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")


#define CLIENT_VERSION COBBLE_VERSION



inline void ConfigureSSLFlags(HINTERNET hRequest) {
    DWORD secFlags = SECURITY_FLAG_IGNORE_REVOCATION;
    InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
}





inline BOOL SendWithCertRetry(HINTERNET hRequest, const std::string& headers, const std::string& body) {
    BOOL result = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(),
                                   (LPVOID)body.c_str(), (DWORD)body.length());
    if (result) {
        return TRUE;
    }
    DWORD err = ::GetLastError();
    if (err == ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED) {


        InternetSetOptionA(hRequest, INTERNET_OPTION_CLIENT_CERT_CONTEXT, NULL, 0);
        result = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(),
                                  (LPVOID)body.c_str(), (DWORD)body.length());
    }
    return result;
}
namespace License {

static std::string g_LastError;
static bool g_LastWasTransportError = false;
static DWORD g_LastWinInetError = 0;
static bool g_NeedsUpdate = false;
static bool g_RequiresLogin = false;
static std::string g_Username;
static std::string g_SessionToken;
static std::string g_InjectionSecret;
static std::string g_PayloadKey;
static std::string g_HandshakeSig;
static std::string g_SyncToken;
static int64_t g_HandshakeTimestamp = 0;
static int g_DaysRemaining = -1;

static void LogHttpResponse(const char* operation, HINTERNET request, DWORD statusCode, size_t responseSize) {
#ifdef COBBLE_DEV_BUILD
    char contentType[256] = {};
    DWORD contentTypeSize = sizeof(contentType);
    if (!HttpQueryInfoA(request, HTTP_QUERY_CONTENT_TYPE, contentType, &contentTypeSize, nullptr)) {
        std::snprintf(contentType, sizeof(contentType), "<unavailable>");
    }

    COBBLE_DEV_LOG("[license.%s] HTTP status=%lu, content_type=%s, body_bytes=%zu",
                   operation, statusCode, contentType, responseSize);
#else
    (void)operation;
    (void)request;
    (void)statusCode;
    (void)responseSize;
#endif
}

std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}


std::string GenerateNonce() {
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    uint8_t bytes[16];
    if (!CryptGenRandom(hProv, sizeof(bytes), bytes)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; i++) {
        oss << std::setw(2) << (int)bytes[i];
    }
    return oss.str();
}


std::vector<uint8_t> Base64Decode(const std::string& input) {
    DWORD outLen = 0;
    CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, nullptr, &outLen, nullptr, nullptr);

    std::vector<uint8_t> output(outLen);
    CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, output.data(), &outLen, nullptr,
                         nullptr);
    output.resize(outLen);
    return output;
}

void WipeString(std::string& value) {
    if (!value.empty()) {
        SecureZeroMemory(&value[0], value.size());
    }
    value.clear();
    value.shrink_to_fit();
}

void WipeVector(std::vector<uint8_t>& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size());
    }
    value.clear();
    value.shrink_to_fit();
}

class ScopedStringWipe {
public:
    explicit ScopedStringWipe(std::string& value) : value_(value) {}
    ScopedStringWipe(const ScopedStringWipe&) = delete;
    ScopedStringWipe& operator=(const ScopedStringWipe&) = delete;
    ~ScopedStringWipe() { WipeString(value_); }

private:
    std::string& value_;
};

class ScopedVectorWipe {
public:
    explicit ScopedVectorWipe(std::vector<uint8_t>& value) : value_(value) {}
    ScopedVectorWipe(const ScopedVectorWipe&) = delete;
    ScopedVectorWipe& operator=(const ScopedVectorWipe&) = delete;
    ~ScopedVectorWipe() { WipeVector(value_); }

private:
    std::vector<uint8_t>& value_;
};

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);

    for (char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7] = {};
                    sprintf_s(buf, "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }

    return out;
}

std::string JsonHeaders() {
    return "Content-Type: application/json\r\n"
           "Accept: application/json\r\n"
           "Accept-Encoding: identity\r\n"
           "Cache-Control: no-store\r\n"
           "Pragma: no-cache\r\n";
}

bool RequireHttps(const URL_COMPONENTSA& urlComp) {
    if (urlComp.nScheme != INTERNET_SCHEME_HTTPS) {
        g_LastError = "HTTPS required";
        g_LastWasTransportError = false;
        return false;
    }
    return true;
}

void ClearTransportErrorState() {
    g_LastWasTransportError = false;
    g_LastWinInetError = 0;
}

void MarkTransportError(const std::string& message, DWORD winInetError = 0) {
    g_LastWasTransportError = true;
    g_LastWinInetError = winInetError;
    g_LastError = message;
}

void MarkLogicalError(const std::string& message) {
    g_LastWasTransportError = false;
    g_LastWinInetError = 0;
    g_LastError = message;
}

void ConfigureInternet(HINTERNET hInternet, DWORD timeout) {
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    constexpr DWORD kInternetOptionSecureProtocols = 84;
    constexpr DWORD kSecureProtocolTls12 = 0x00000800;
    DWORD protocols = kSecureProtocolTls12;
    InternetSetOptionA(hInternet, kInternetOptionSecureProtocols, &protocols, sizeof(protocols));
}

std::string LowerAscii(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        out += static_cast<char>(std::tolower(c));
    }
    return out;
}

bool HasLoginHint(const std::string& value) {
    std::string lower = LowerAscii(value);
    return lower.find("not registered") != std::string::npos ||
           lower.find("no license") != std::string::npos ||
           lower.find("login") != std::string::npos ||
           lower.find("hwid_not_registered") != std::string::npos;
}

bool LooksLikeHtmlOrChallenge(const std::string& value) {
    std::string lower = LowerAscii(value);
    return lower.find("<html") != std::string::npos ||
           lower.find("<!doctype html") != std::string::npos ||
           lower.find("cloudflare") != std::string::npos ||
           lower.find("cf-ray") != std::string::npos ||
           lower.find("just a moment") != std::string::npos ||
           lower.find("attention required") != std::string::npos;
}

std::string NormalizeJsonResponse(const std::string& response) {
    if (response.empty()) {
        return "";
    }

    size_t start = 0;
    if (response.size() >= 3 &&
        static_cast<unsigned char>(response[0]) == 0xEF &&
        static_cast<unsigned char>(response[1]) == 0xBB &&
        static_cast<unsigned char>(response[2]) == 0xBF) {
        start = 3;
    }

    while (start < response.size() && std::isspace(static_cast<unsigned char>(response[start]))) {
        start++;
    }

    size_t firstObject = response.find('{', start);
    if (firstObject == std::string::npos) {
        return response.substr(start);
    }

    size_t end = response.size();
    while (end > firstObject && std::isspace(static_cast<unsigned char>(response[end - 1]))) {
        end--;
    }

    size_t lastObject = response.rfind('}', end - 1);
    if (lastObject == std::string::npos || lastObject < firstObject) {
        return response.substr(firstObject, end - firstObject);
    }

    return response.substr(firstObject, lastObject - firstObject + 1);
}

bool ParseResponseObject(const std::string& response, json::Object& out) {
    std::string normalized = NormalizeJsonResponse(response);
    ScopedStringWipe normalizedWipe(normalized);
    return !normalized.empty() && out.parse(normalized);
}

void SetUnparseableResponseError(int statusCode, const std::string& response) {
    if (HasLoginHint(response)) {
        g_RequiresLogin = true;
        MarkLogicalError("Device not registered");
        return;
    }



    if (LooksLikeHtmlOrChallenge(response)) {
        MarkTransportError(statusCode == 0
                               ? "Server returned web challenge"
                               : "Server returned web challenge (HTTP " + std::to_string(statusCode) + ")");
        return;
    }

    if (statusCode == 401 || statusCode == 403) {
        MarkLogicalError("Server rejected request (HTTP " + std::to_string(statusCode) + ")");
        return;
    }
    if (statusCode == 429) {
        MarkLogicalError("Server rate limited request");
        return;
    }

    if (statusCode >= 500) {
        MarkTransportError("Server unavailable (HTTP " + std::to_string(statusCode) + ")");
        return;
    }

    MarkLogicalError(statusCode > 0 ? "Invalid server response (HTTP " + std::to_string(statusCode) + ")"
                                    : "Invalid server response");
}

bool IsHexString(const std::string& value, size_t expectedLen) {
    if (value.size() != expectedLen) {
        return false;
    }
    for (unsigned char c : value) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    return true;
}

bool IsAsciiToken(const std::string& value, size_t maxLen) {
    if (value.empty() || value.size() > maxLen) {
        return false;
    }
    for (unsigned char c : value) {
        if (c < 0x21 || c > 0x7e) {
            return false;
        }
    }
    return true;
}

bool IsBase64Signature64(const std::string& value) {
    if (value.empty() || value.size() > 127) {
        return false;
    }
    std::vector<uint8_t> decoded = Base64Decode(value);
    ScopedVectorWipe decodedWipe(decoded);
    return decoded.size() == 64;
}

void StoreProtectedString(std::string& dst, const std::string& src) {
    WipeString(dst);
    dst = src;
}

bool StoreSessionMaterial(const json::Object& payload) {
    std::string username = payload.get_string("username", "");
    std::string sessionToken = payload.get_string("session_token", "");
    std::string injectionSecret = payload.get_string("injection_secret", "");
    std::string handshakeSig = payload.get_string("handshake_sig", "");
    std::string syncToken = payload.get_string("sync_token", "");
    int64_t issuedAt = payload.get_int("issued_at", 0);

    ScopedStringWipe sessionTokenWipe(sessionToken);
    ScopedStringWipe injectionSecretWipe(injectionSecret);
    ScopedStringWipe syncTokenWipe(syncToken);

    if (!IsAsciiToken(username, 63) || !IsAsciiToken(sessionToken, 127) ||
        !IsHexString(injectionSecret, 64) || !IsBase64Signature64(handshakeSig) ||
        !IsHexString(syncToken, 64) || issuedAt <= 0) {
        g_LastError = "Invalid session";
        return false;
    }

    g_Username = username;
    g_DaysRemaining = payload.get_int("days_remaining", -1);
    StoreProtectedString(g_SessionToken, sessionToken);
    StoreProtectedString(g_InjectionSecret, injectionSecret);
    g_HandshakeSig = handshakeSig;
    g_HandshakeTimestamp = issuedAt;
    StoreProtectedString(g_SyncToken, syncToken);
    return true;
}

std::string GetLastError() {
    return g_LastError;
}

bool IsTransportError() {
    return g_LastWasTransportError;
}

bool NeedsUpdate() {
    return g_NeedsUpdate;
}

std::string GetUsername() {
    return g_Username;
}

int GetDaysRemaining() {
    return g_DaysRemaining;
}

std::string GetSessionToken() {
    return g_SessionToken;
}

std::string GetInjectionSecret() {
    return g_InjectionSecret;
}

std::string GetPayloadKey() {
    return g_PayloadKey;
}

std::string GetHandshakeSig() {
    return g_HandshakeSig;
}

int64_t GetHandshakeTimestamp() {
    return g_HandshakeTimestamp;
}

std::string GetSyncToken() {
    return g_SyncToken;
}


bool FetchPayloadKey(const std::string& serverUrl) {
    ClearTransportErrorState();
    if (g_SessionToken.empty()) {
        MarkLogicalError("No session token");
        return false;
    }

    std::string hwid = HWID::Generate();
    if (hwid.empty()) {
        MarkLogicalError("HWID generation failed");
        return false;
    }
    ScopedStringWipe hwidWipe(hwid);

    std::string url = serverUrl + "/api/loader/payload-key";

    std::string postData =
        "{\"hwid\":\"" + JsonEscape(hwid) + "\",\"session_token\":\"" + JsonEscape(g_SessionToken) + "\"}";
    ScopedStringWipe postDataWipe(postData);

    HINTERNET hInternet =
        InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

    if (!hInternet) {
        MarkTransportError("Network error");
        return false;
    }

    DWORD timeout = 15000;
    ConfigureInternet(hInternet, timeout);

    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        MarkLogicalError("Invalid URL");
        return false;
    }

    if (!RequireHttps(urlComp)) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD flags = INTERNET_FLAG_SECURE;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);

    if (!hConnect) {
        InternetCloseHandle(hInternet);
        MarkTransportError("Connection failed", ::GetLastError());
        return false;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath, NULL, NULL, NULL,
                                          flags | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);

    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        MarkTransportError("Request failed", ::GetLastError());
        return false;
    }

    std::string headers = JsonHeaders();

    ConfigureSSLFlags(hRequest);
    BOOL result = SendWithCertRetry(hRequest, headers, postData);

    if (!result) {
        DWORD err = ::GetLastError();
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        MarkTransportError("Server error (E" + std::to_string(err) + ")", err);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);

    if (statusCode != 200) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        if (statusCode >= 500 || statusCode == 0) {
            MarkTransportError("Key fetch failed (HTTP " + std::to_string(statusCode) + ")");
        } else {
            MarkLogicalError("Key fetch failed");
        }
        return false;
    }

    char buffer[4096] = {0};
    DWORD bytesRead = 0;
    std::string response;
    ScopedStringWipe responseWipe(response);

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    json::Object envelope;
    if (!ParseResponseObject(response, envelope)) {
        SetUnparseableResponseError(static_cast<int>(statusCode), response);
        return false;
    }

    std::string payloadB64 = envelope.get_string("payload_b64", "");
    std::string sigB64 = envelope.get_string("sig_b64", "");

    if (payloadB64.empty() || sigB64.empty()) {
        g_LastError = "Invalid response";
        return false;
    }

    std::vector<uint8_t> payloadBytes = Base64Decode(payloadB64);
    ScopedVectorWipe payloadBytesWipe(payloadBytes);
    std::vector<uint8_t> sigBytes = Base64Decode(sigB64);

    if (payloadBytes.empty() || sigBytes.size() != 64) {
        g_LastError = "Invalid response";
        return false;
    }

    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    bool sigValid = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
    SecureZeroMemory(pubkey, 32);

    if (!sigValid) {
        g_LastError = "Invalid response";
        return false;
    }

    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());
    ScopedStringWipe payloadStrWipe(payloadStr);
    json::Object json;
    if (!ParseResponseObject(payloadStr, json)) {
        g_LastError = "Invalid response";
        return false;
    }

    std::string payloadKey = json.get_string("key", "");
    ScopedStringWipe payloadKeyWipe(payloadKey);
    if (!IsHexString(payloadKey, 64)) {
        g_LastError = "Invalid key";
        return false;
    }
    StoreProtectedString(g_PayloadKey, payloadKey);

    return true;
}

bool Verify(const std::string& serverUrl) {
    g_RequiresLogin = false;
    g_LastError.clear();
    ClearTransportErrorState();
    COBBLE_DEV_LOG("[license.verify] Begin verification");


    std::string hwid = HWID::Generate();
    if (hwid.empty()) {
        MarkLogicalError("Failed to generate HWID");
        COBBLE_DEV_LOG("[license.verify] HWID generation failed");
        return false;
    }
    ScopedStringWipe hwidWipe(hwid);

    std::string nonce = GenerateNonce();
    if (nonce.empty()) {
        MarkLogicalError("Failed to generate nonce");
        COBBLE_DEV_LOG("[license.verify] Nonce generation failed");
        return false;
    }

    std::string url = serverUrl + "/api/verify";

    std::string postData =
        "{\"hwid\":\"" + JsonEscape(hwid) + "\",\"nonce\":\"" + JsonEscape(nonce) + "\",\"version\":\"" +
        JsonEscape(std::string(CLIENT_VERSION)) + "\"}";
    ScopedStringWipe postDataWipe(postData);

    HINTERNET hInternet =
        InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

    if (!hInternet) {
        MarkTransportError("Failed to initialize internet");
        return false;
    }

    DWORD timeout = 15000;
    ConfigureInternet(hInternet, timeout);

    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        MarkLogicalError("Invalid server URL");
        return false;
    }

    if (!RequireHttps(urlComp)) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD flags = INTERNET_FLAG_SECURE;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);

    if (!hConnect) {
        InternetCloseHandle(hInternet);
        MarkTransportError("Failed to connect to server", ::GetLastError());
        return false;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath, NULL, NULL, NULL,
                                          flags | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);

    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        MarkTransportError("Failed to create request", ::GetLastError());
        return false;
    }

    std::string headers = JsonHeaders();

    ConfigureSSLFlags(hRequest);
    BOOL result = SendWithCertRetry(hRequest, headers, postData);

    if (!result) {
        DWORD err = ::GetLastError();
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        MarkTransportError("Failed to send request (E" + std::to_string(err) + ")", err);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);




    char buffer[4096] = {0};
    DWORD bytesRead = 0;
    std::string response;
    ScopedStringWipe responseWipe(response);

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    LogHttpResponse("verify", hRequest, statusCode, response.size());


    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);


    if (response.empty()) {
        g_LastError =
            "No response from server (Status: " + std::to_string(statusCode) + ")";

        if (statusCode >= 400 && statusCode < 500) {
            g_RequiresLogin = true;
        }
        COBBLE_DEV_LOG("[license.verify] Empty response: requires_login=%s",
                       g_RequiresLogin ? "true" : "false");
        return false;
    }


    json::Object json;
    if (!ParseResponseObject(response, json)) {
        COBBLE_DEV_LOG("[license.verify] Top-level JSON parsing failed");
        if (statusCode == 401 || statusCode == 403) {
            g_RequiresLogin = true;
            g_LastError.clear();
        } else {
            SetUnparseableResponseError(static_cast<int>(statusCode), response);
        }
        COBBLE_DEV_LOG("[license.verify] Parse failure classified: requires_login=%s, error=\"%s\"",
                       g_RequiresLogin ? "true" : "false", g_LastError.c_str());
        return false;
    }


    std::string payloadB64 = json.get_string("payload_b64", "");
    std::string sigB64 = json.get_string("sig_b64", "");
    COBBLE_DEV_LOG("[license.verify] Envelope parsed: payload_b64_chars=%zu, sig_b64_chars=%zu",
                   payloadB64.size(), sigB64.size());

    if (payloadB64.empty() || sigB64.empty()) {

        std::string errorMsg = json.get_string("error", "");
        std::string message = json.get_string("message", "");
        std::string code = json.get_string("code", "");
        const bool envelopeMissing = payloadB64.empty() && sigB64.empty();
        const bool unauthenticated = statusCode == 401 || statusCode == 403;
        const bool emptyBootstrapResponse =
            envelopeMissing && errorMsg.empty() && message.empty() && code.empty() &&
            statusCode >= 200 && statusCode < 300;

        if (HasLoginHint(errorMsg) || HasLoginHint(message) || HasLoginHint(code) ||
            unauthenticated || emptyBootstrapResponse) {
            g_RequiresLogin = true;
            g_LastError.clear();
            ClearTransportErrorState();
        } else if (statusCode >= 500 || LooksLikeHtmlOrChallenge(response)) {
            MarkTransportError(errorMsg.empty() ? (message.empty() ? "Invalid response format" : message) : errorMsg);
        } else {
            MarkLogicalError(errorMsg.empty() ? (message.empty() ? "Invalid response format" : message) : errorMsg);
        }
        COBBLE_DEV_LOG("[license.verify] Unsigned response classified: status=%lu, requires_login=%s, error=\"%s\"",
                       statusCode, g_RequiresLogin ? "true" : "false", g_LastError.c_str());
        return false;
    }

    std::vector<uint8_t> payloadBytes = Base64Decode(payloadB64);
    ScopedVectorWipe payloadBytesWipe(payloadBytes);
    std::vector<uint8_t> sigBytes = Base64Decode(sigB64);
    COBBLE_DEV_LOG("[license.verify] Envelope decoded: payload_bytes=%zu, signature_bytes=%zu",
                   payloadBytes.size(), sigBytes.size());

    if (payloadBytes.empty() || sigBytes.size() != 64) {
        MarkLogicalError("Invalid response format");
        COBBLE_DEV_LOG("[license.verify] Rejected decoded envelope dimensions");
        return false;
    }




    bool sigValid = false;

    COBBLE_VM_CRITICAL_START

    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    CF_CRITICAL_BEGIN();
    sigValid = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
    CF_NOP();
    CF_CRITICAL_END();
    SecureZeroMemory(pubkey, 32);

    COBBLE_VM_CRITICAL_END

    if (!sigValid) {
        g_LastError = "Invalid signature - response may have been tampered";
        COBBLE_DEV_LOG("[license.verify] Signature verification failed");
        return false;
    }
    COBBLE_DEV_LOG("[license.verify] Signature verification passed");

    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());
    ScopedStringWipe payloadStrWipe(payloadStr);
    json::Object payload;
    if (!ParseResponseObject(payloadStr, payload)) {
        g_LastError = "Invalid payload format";
        COBBLE_DEV_LOG("[license.verify] Signed payload JSON parsing failed");
        return false;
    }


    std::string responseNonce = payload.get_string("nonce", "");
    CF_NOP();
    if (!CF_CHECK(responseNonce == nonce)) {
        CF_JUNK();
        g_LastError = "Nonce mismatch - possible replay attack";
        COBBLE_DEV_LOG("[license.verify] Nonce verification failed");
        return false;
    }


    int64_t expiresAt = payload.get_int("expires_at", 0);
    int64_t now = (int64_t)time(nullptr);
    if (now > expiresAt) {
        g_LastError = "Response expired";
        COBBLE_DEV_LOG("[license.verify] Response expired: expires_at=%lld, now=%lld",
                       static_cast<long long>(expiresAt), static_cast<long long>(now));
        return false;
    }


    if (payload.get_bool("valid", false)) {
        const bool stored = StoreSessionMaterial(payload);
        COBBLE_DEV_LOG("[license.verify] Valid license payload: session_material_stored=%s",
                       stored ? "true" : "false");
        return stored;
    }


    g_LastError = payload.get_string("message", "License not valid");


    std::string errCode = payload.get_string("code", "");
    if (HasLoginHint(errCode) || HasLoginHint(g_LastError)) {
        g_RequiresLogin = true;
    }
    COBBLE_DEV_LOG("[license.verify] License rejected: code=\"%s\", requires_login=%s, error=\"%s\"",
                   errCode.c_str(), g_RequiresLogin ? "true" : "false", g_LastError.c_str());
    return false;
}

bool RequiresLogin() {
    return g_RequiresLogin;
}

bool LoginWithCredentials(const std::string& serverUrl, const std::string& username, const std::string& password) {
    g_RequiresLogin = false;
    g_LastError.clear();
    ClearTransportErrorState();
    COBBLE_DEV_LOG("[license.login] Begin login");


    std::string hwid = HWID::Generate();
    if (hwid.empty()) {
        MarkLogicalError("Failed to generate HWID");
        return false;
    }
    ScopedStringWipe hwidWipe(hwid);

    std::string nonce = GenerateNonce();
    if (nonce.empty()) {
        MarkLogicalError("Failed to generate nonce");
        return false;
    }

    std::string url = serverUrl + "/api/loader/login";

    std::string postData = "{\"username\":\"" + JsonEscape(username) + "\",\"password\":\"" + JsonEscape(password) +
                           "\",\"hwid\":\"" + JsonEscape(hwid) + "\",\"nonce\":\"" + JsonEscape(nonce) +
                           "\",\"version\":\"" + JsonEscape(std::string(CLIENT_VERSION)) + "\"}";
    ScopedStringWipe postDataWipe(postData);

    HINTERNET hInternet =
        InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

    if (!hInternet) {
        MarkTransportError("Network error");
        return false;
    }

    DWORD timeout = 15000;
    ConfigureInternet(hInternet, timeout);

    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        MarkLogicalError("Invalid server URL");
        return false;
    }

    if (!RequireHttps(urlComp)) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD flags = INTERNET_FLAG_SECURE;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);

    if (!hConnect) {
        InternetCloseHandle(hInternet);
        MarkTransportError("Connection failed", ::GetLastError());
        return false;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath, NULL, NULL, NULL,
                                          flags | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);

    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        MarkTransportError("Request failed", ::GetLastError());
        return false;
    }

    std::string headers = JsonHeaders();

    ConfigureSSLFlags(hRequest);
    BOOL result = SendWithCertRetry(hRequest, headers, postData);

    if (!result) {
        DWORD err = ::GetLastError();
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        MarkTransportError("Request failed (E" + std::to_string(err) + ")", err);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);

    char buffer[4096] = {0};
    DWORD bytesRead = 0;
    std::string response;
    ScopedStringWipe responseWipe(response);

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    LogHttpResponse("login", hRequest, statusCode, response.size());

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);


    json::Object json;
    if (!ParseResponseObject(response, json)) {
        COBBLE_DEV_LOG("[license.login] Top-level JSON parsing failed");
        SetUnparseableResponseError(static_cast<int>(statusCode), response);
        COBBLE_DEV_LOG("[license.login] Parse error=\"%s\"", g_LastError.c_str());
        return false;
    }


    std::string payloadB64 = json.get_string("payload_b64", "");
    std::string sigB64 = json.get_string("sig_b64", "");
    COBBLE_DEV_LOG("[license.login] Envelope parsed: payload_b64_chars=%zu, sig_b64_chars=%zu",
                   payloadB64.size(), sigB64.size());

    if (payloadB64.empty() || sigB64.empty()) {

        if (statusCode != 200) {
            std::string errorMsg = json.get_string("error", "");
            std::string message = json.get_string("message", "");
            std::string code = json.get_string("code", "");
            if (HasLoginHint(errorMsg) || HasLoginHint(message) || HasLoginHint(code)) {
                g_RequiresLogin = true;
                g_LastError = "Device not registered";
            } else {
                g_LastError = errorMsg.empty() ? (message.empty() ? "Authentication failed" : message) : errorMsg;
            }
            return false;
        }
        g_LastError = "Invalid response format";
        COBBLE_DEV_LOG("[license.login] Successful HTTP response did not contain a signed envelope");
        return false;
    }

    std::vector<uint8_t> payloadBytes = Base64Decode(payloadB64);
    ScopedVectorWipe payloadBytesWipe(payloadBytes);
    std::vector<uint8_t> sigBytes = Base64Decode(sigB64);
    COBBLE_DEV_LOG("[license.login] Envelope decoded: payload_bytes=%zu, signature_bytes=%zu",
                   payloadBytes.size(), sigBytes.size());

    if (payloadBytes.empty() || sigBytes.size() != 64) {
        g_LastError = "Invalid response format";
        COBBLE_DEV_LOG("[license.login] Rejected decoded envelope dimensions");
        return false;
    }




    bool sigValid = false;

    COBBLE_VM_CRITICAL_START

    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    CF_CRITICAL_BEGIN();
    sigValid = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
    CF_NOP();
    CF_CRITICAL_END();
    SecureZeroMemory(pubkey, 32);

    COBBLE_VM_CRITICAL_END

    if (!sigValid) {
        g_LastError = "Signature verification failed";
        COBBLE_DEV_LOG("[license.login] Signature verification failed");
        return false;
    }
    COBBLE_DEV_LOG("[license.login] Signature verification passed");

    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());
    ScopedStringWipe payloadStrWipe(payloadStr);
    json::Object payload;
    if (!ParseResponseObject(payloadStr, payload)) {
        g_LastError = "Invalid payload";
        COBBLE_DEV_LOG("[license.login] Signed payload JSON parsing failed");
        return false;
    }


    std::string responseNonce = payload.get_string("nonce", "");
    CF_NOP();
    if (!CF_CHECK(responseNonce == nonce)) {
        CF_JUNK();
        g_LastError = "Security check failed";
        COBBLE_DEV_LOG("[license.login] Nonce verification failed");
        return false;
    }


    int64_t expiresAt = payload.get_int("expires_at", 0);
    int64_t now = (int64_t)time(nullptr);
    if (now > expiresAt) {
        g_LastError = "Response expired";
        COBBLE_DEV_LOG("[license.login] Response expired: expires_at=%lld, now=%lld",
                       static_cast<long long>(expiresAt), static_cast<long long>(now));
        return false;
    }


    if (payload.get_bool("valid", false)) {
        const bool stored = StoreSessionMaterial(payload);
        COBBLE_DEV_LOG("[license.login] Login accepted: session_material_stored=%s",
                       stored ? "true" : "false");
        return stored;
    }

    g_LastError = payload.get_string("message", "Authentication failed");
    COBBLE_DEV_LOG("[license.login] Login rejected: error=\"%s\"", g_LastError.c_str());
    return false;
}


}
