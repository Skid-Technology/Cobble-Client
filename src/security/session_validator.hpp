#pragma once

#include <wincrypt.h>
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "../util/aes_string.hpp"

#pragma comment(lib, "crypt32.lib")

#include "../util/network/http.hpp"
#include "../util/network/json.hpp"
#include "anti_attach.hpp"     
#include "dll_integrity.hpp"   
#include "loader_verify.hpp"   
#include "tweetnacl.h"         
#include "generated/server_config.h"
#include "generated/keys.h"







namespace SessionValidator {


struct InjectionData {
    char signature[16];      
    char secret[65];         
    char username[64];       
    char hwid[65];           
    uint32_t timestamp;      
    uint32_t expiresIn;      
    char handshakeSig[128];  
    char sessionToken[128];  
    char syncToken[65];      
    char apiHost[64];        
    
};






inline std::string g_Username;
inline std::string g_InjectionSecret;
inline std::string g_HWID;
inline std::string g_HandshakeSig;  
inline std::string g_SessionToken;  
inline std::string g_SyncToken;     
inline std::mutex g_SessionMutex;
inline std::atomic<bool> g_SessionValid{false};



inline void zeroAndClear(std::string& s) {
    if (!s.empty()) {
        SecureZeroMemory(&s[0], s.size());
    }
    s.clear();
    s.shrink_to_fit();
}


inline std::string snapshotToken(const std::string& src) {
    std::lock_guard<std::mutex> lock(g_SessionMutex);
    return src;
}


inline std::atomic<bool> g_stateCorrupted{false};
inline uint64_t g_initTimestamp = 0;
inline uint64_t g_lastValidation = 0;



inline bool ReadInjectionData(InjectionData* outData) {
    if (!outData)
        return false;

    
    
    wchar_t sharedMemName[64];
    std::wstring sharedMemFmt = OBFUSCATE_WSTRING(L"Local\\Cobble_%lu");
    swprintf_s(sharedMemName, sharedMemFmt.c_str(), GetCurrentProcessId());
    HANDLE hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);

    if (!hMapFile) {
        
        return false;
    }

    void* pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(InjectionData));
    if (!pBuf) {
        CloseHandle(hMapFile);
        return false;
    }

    
    memcpy(outData, pBuf, sizeof(InjectionData));

    
    SecureZeroMemory(pBuf, sizeof(InjectionData));

    
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    return true;
}

inline bool hasNullTerminator(const char* value, size_t len) {
    return memchr(value, '\0', len) != nullptr;
}

inline bool isHexString(const char* value, size_t expectedLen) {
    if (!hasNullTerminator(value, expectedLen + 1)) {
        return false;
    }
    for (size_t i = 0; i < expectedLen; i++) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        bool digit = c >= '0' && c <= '9';
        bool lower = c >= 'a' && c <= 'f';
        bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return value[expectedLen] == '\0';
}

inline bool isAsciiToken(const char* value, size_t maxLen) {
    if (!hasNullTerminator(value, maxLen + 1) || value[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < maxLen && value[i] != '\0'; i++) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x21 || c > 0x7e) {
            return false;
        }
    }
    return true;
}




inline bool ValidateInjection() {
    InjectionData data = {};

    
    if (!ReadInjectionData(&data)) {
        
        return false;
    }

    
    std::string injSig = OBFUSCATE_STRING("COBBLE_INJ_V1");
    if (memcmp(data.signature, injSig.c_str(), injSig.size()) != 0) {
        
        return false;
    }

    
    uint32_t now = (uint32_t)time(nullptr);
    if (now > data.timestamp + data.expiresIn) {
        
        return false;
    }

    
    if (data.secret[0] == '\0') {
        
        return false;
    }

    if (!isHexString(data.secret, 64) || !isAsciiToken(data.username, 63) || !isHexString(data.hwid, 64) ||
        !isAsciiToken(data.handshakeSig, 127) || !isAsciiToken(data.sessionToken, 127) ||
        !isHexString(data.syncToken, 64)) {
        return false;
    }
    
    if (data.apiHost[0] != '\0' && !isAsciiToken(data.apiHost, 63)) {
        return false;
    }

    
    
    
    if (data.handshakeSig[0] == '\0') {
        
        return false;
    }

    
    DWORD sigLen = 0;
    CryptStringToBinaryA(data.handshakeSig, 0, CRYPT_STRING_BASE64, nullptr, &sigLen, nullptr, nullptr);

    if (sigLen != 64) {
        
        return false;
    }

    std::vector<uint8_t> sigBytes(sigLen);
    CryptStringToBinaryA(data.handshakeSig, 0, CRYPT_STRING_BASE64, sigBytes.data(), &sigLen, nullptr, nullptr);

    
    
    char handshakeData[512];
    std::string handshakeFmt = OBFUSCATE_STRING("COBBLE_HANDSHAKE|%s|%s|%lld|%s");
    snprintf(handshakeData, sizeof(handshakeData), handshakeFmt.c_str(), data.username, data.hwid,
             (long long)data.timestamp, data.secret);

    
    uint8_t publicKey[32];
    cobble_keys::GetPublicKey(publicKey);

    
    bool sigValid = tweetnacl::verify(sigBytes.data(), (const uint8_t*)handshakeData, strlen(handshakeData), publicKey);

    
    SecureZeroMemory(publicKey, sizeof(publicKey));
    SecureZeroMemory(handshakeData, sizeof(handshakeData));

    if (!sigValid) {
        
        return false;
    }

    
    {
        std::lock_guard<std::mutex> lock(g_SessionMutex);
        g_Username = data.username;
        g_InjectionSecret = data.secret;
        g_HWID = data.hwid;
        g_HandshakeSig = data.handshakeSig;
        g_SessionToken = data.sessionToken;
        g_SyncToken = data.syncToken;
    }
    
    if (data.apiHost[0] != '\0') {
        cobble_config::PreferServerHost(data.apiHost);
    }
    g_SessionValid = true;

    
    g_initTimestamp = GetTickCount64();
    g_lastValidation = g_initTimestamp;

    
    SecureZeroMemory(&data, sizeof(data));

    return true;
}


inline std::string GetUsername() {
    if (!g_SessionValid) return "";
    std::lock_guard<std::mutex> lock(g_SessionMutex);
    return g_Username;
}


inline std::string GetInjectionSecret() {
    if (!g_SessionValid) return "";
    std::lock_guard<std::mutex> lock(g_SessionMutex);
    return g_InjectionSecret;
}


inline std::string GetHWID() {
    if (!g_SessionValid) return "";
    std::lock_guard<std::mutex> lock(g_SessionMutex);
    return g_HWID;
}


inline bool IsSessionValid() {
    return g_SessionValid;
}



inline void InvalidateSession() {
    g_SessionValid = false;
    std::lock_guard<std::mutex> lock(g_SessionMutex);
    zeroAndClear(g_Username);
    zeroAndClear(g_InjectionSecret);
    zeroAndClear(g_HWID);
    zeroAndClear(g_HandshakeSig);
    zeroAndClear(g_SessionToken);
    zeroAndClear(g_SyncToken);
}


inline std::vector<uint8_t> Base64Decode(const std::string& input) {
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, nullptr, &outLen, nullptr,
                              nullptr)) {
        return {};
    }

    std::vector<uint8_t> output(outLen);
    if (!CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, output.data(), &outLen,
                              nullptr, nullptr)) {
        return {};
    }
    output.resize(outLen);
    return output;
}


inline std::string sha256hex(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32] = {0};
    DWORD hashLen = 32;

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    
    char hexBuf[65] = {0};
    for (DWORD i = 0; i < hashLen; i++) {
        sprintf_s(hexBuf + (i * 2), 3, "%02x", hash[i]);
    }
    return std::string(hexBuf);
}




inline std::string computeSyncHash() {
    std::string syncTokenCopy, hwidCopy;
    {
        std::lock_guard<std::mutex> lock(g_SessionMutex);
        syncTokenCopy = g_SyncToken;
        hwidCopy = g_HWID;
    }
    if (syncTokenCopy.empty() || hwidCopy.empty()) {
        return "";
    }
    int64_t bucket = std::time(nullptr) / 60;  
    std::string input = syncTokenCopy + hwidCopy + std::to_string(bucket);
    return sha256hex(input);
}







inline std::atomic<bool> g_HeartbeatRunning{false};
inline HANDLE g_HeartbeatThread = nullptr;
inline HMODULE g_DllModule = nullptr;          
inline std::atomic<int> g_AttachStrikes{0};    




constexpr int ATTACH_STRIKE_THRESHOLD = 4;


inline std::string GetServerUrl() {
    return cobble_config::GetServerUrl();
}




inline bool DoHeartbeat() {
    
    
    std::string sessionTokenCopy, hwidCopy, usernameCopy, syncHashLocal;
    {
        std::lock_guard<std::mutex> lock(g_SessionMutex);
        if (g_SessionToken.empty() || g_HWID.empty()) {
            return false;
        }
        sessionTokenCopy = g_SessionToken;
        hwidCopy = g_HWID;
        usernameCopy = g_Username;
    }
    syncHashLocal = computeSyncHash();

    
    std::string body = std::string("{\"session_token\":\"") + http::json_escape(sessionTokenCopy) +
                       "\",\"hwid\":\"" + http::json_escape(hwidCopy) +
                       "\",\"username\":\"" + http::json_escape(usernameCopy) +
                       "\",\"version\":\"" + http::json_escape(cobble_config::VERSION) + "\"";
    if (!syncHashLocal.empty()) {
        body += ",\"sync_hash\":\"" + http::json_escape(syncHashLocal) + "\"";
    }
    body += "}";

    log_debug("Sending heartbeat");
    http::Response response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/heartbeat").c_str(), body, 443);

    
    
    if (!response.success) {
        log_err("Heartbeat network fail: %d", response.statusCode);
        return false;  
    }

    
    json::Value envelope;
    if (!json::Parser::parse(response.body, envelope)) {
        log_err("Heartbeat JSON parse fail");
        return false;
    }

    std::string payloadB64 = envelope["payload_b64"].as_string("");
    std::string sigB64 = envelope["sig_b64"].as_string("");

    if (payloadB64.empty() || sigB64.empty()) {
        log_err("Heartbeat missing signature envelope");
        InvalidateSession();
        return false;  
    }

    auto payloadBytes = Base64Decode(payloadB64);
    auto sigBytes = Base64Decode(sigB64);

    if (payloadBytes.empty() || sigBytes.size() != 64) {
        log_err("Heartbeat invalid sig format");
        InvalidateSession();
        return false;
    }

    
    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    bool sigOk = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
    SecureZeroMemory(pubkey, 32);

    if (!sigOk) {
        log_err("Heartbeat signature invalid - response tampered");
        InvalidateSession();
        return false;  
    }

    
    json::Value resp;
    if (!json::Parser::parse(std::string(payloadBytes.begin(), payloadBytes.end()), resp)) {
        log_err("Heartbeat payload parse fail");
        return false;
    }

    
    bool ok = resp["ok"].as_bool(false);
    if (!ok) {
        std::string reason = resp["reason"].as_string("");
        log_err("Heartbeat rejected: %s", reason.c_str());

        
        
        if (reason == "sync_error") {
            g_stateCorrupted = true;
            g_SessionValid = false;
        }
    } else {
        log_ok("Heartbeat success");

        
        
        
        std::string newToken = resp["new_token"].as_string("");
        std::string newSyncToken = resp["new_sync_token"].as_string("");
        if (!isAsciiToken(newToken.c_str(), 127) || !isHexString(newSyncToken.c_str(), 64)) {
            InvalidateSession();
            return false;
        }
        std::lock_guard<std::mutex> lock(g_SessionMutex);
        zeroAndClear(g_SessionToken);
        zeroAndClear(g_SyncToken);
        g_SessionToken = newToken;
        g_SyncToken = newSyncToken;
    }
    return ok;
}


inline DWORD WINAPI HeartbeatThreadProc(LPVOID lpParam) {
    (void)lpParam;

    
    srand((unsigned int)(GetTickCount64() ^ GetCurrentProcessId()));

    
    Sleep(30000);

    int consecutiveFailures = 0;
    const int maxFailures = 3;  

    while (g_HeartbeatRunning && g_SessionValid) {
        
        
        
        DllIntegrity::Check();  

        
        
        
        if (g_DllModule && !LoaderVerify::reverify(g_DllModule)) {
            g_AttachStrikes.fetch_add(2, std::memory_order_relaxed);
        }

        
        
        
        
        if (AntiAttach::quickCheck()) {
            g_AttachStrikes.fetch_add(1, std::memory_order_relaxed);
        }
        if (g_AttachStrikes.load(std::memory_order_relaxed) >= ATTACH_STRIKE_THRESHOLD) {
            g_stateCorrupted = true;
        }

        bool success = DoHeartbeat();

        if (!success) {
            consecutiveFailures++;
            if (consecutiveFailures >= maxFailures) {
                
                g_SessionValid = false;
                InvalidateSession();

                
                
                g_stateCorrupted = true;

                
                return 0;
            }
            
            Sleep(30000);
        } else {
            consecutiveFailures = 0;
            
            DWORD baseInterval = 120000;      
            DWORD jitter = (rand() % 60000);  
            Sleep(baseInterval + jitter);
        }
    }

    return 0;
}




inline void StartHeartbeatThread(HMODULE hModule, const std::string& optionalToken = "") {
    if (g_HeartbeatRunning)
        return;  

    g_DllModule = hModule;

    
    if (!optionalToken.empty()) {
        g_SessionToken = optionalToken;
    }
    

    g_HeartbeatRunning = true;

    g_HeartbeatThread = CreateThread(nullptr, 0, HeartbeatThreadProc, nullptr, 0, nullptr);
}


inline void StopHeartbeatThread() {
    g_HeartbeatRunning = false;
    if (g_HeartbeatThread) {
        WaitForSingleObject(g_HeartbeatThread, 500);
        CloseHandle(g_HeartbeatThread);
        g_HeartbeatThread = nullptr;
    }
}







inline void refreshState() {
    g_lastValidation = GetTickCount64();
}



inline bool isStateStale() {
    if (g_initTimestamp == 0)
        return true;
    uint64_t now = GetTickCount64();
    
    constexpr uint64_t MAX_STALENESS = 318000;
    return (now - g_lastValidation) > MAX_STALENESS;
}


inline bool isStateCorrupted() {
    return g_stateCorrupted.load();
}

}  
