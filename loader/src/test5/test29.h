#pragma once

#include <shlobj.h>
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include "../test6/test30.hpp"
#include "../test6/test35.h"
#include "../test3/test21.h"
#include "generated/keys.h"
#include "test28.h"

#pragma comment(lib, "winhttp.lib")

namespace Updater {

struct FileInfo {
    std::string sha256;
    size_t size = 0;
};

struct Manifest {
    std::string version;
    std::string minAllowed;
    bool mandatory = false;
    FileInfo loader;
    FileInfo dll;
    std::string notes;
};

struct CheckResult {
    bool success = false;
    bool updateAvailable = false;
    bool mandatory = false;
    bool blocked = false;
    Manifest manifest;
    std::string error;
};


inline int compareVersions(const std::string& a, const std::string& b) {
    int aMajor = 0, aMinor = 0, aPatch = 0;
    int bMajor = 0, bMinor = 0, bPatch = 0;
    sscanf_s(a.c_str(), "%d.%d.%d", &aMajor, &aMinor, &aPatch);
    sscanf_s(b.c_str(), "%d.%d.%d", &bMajor, &bMinor, &bPatch);

    if (aMajor != bMajor)
        return aMajor - bMajor;
    if (aMinor != bMinor)
        return aMinor - bMinor;
    return aPatch - bPatch;
}






inline std::string getHighestSeen() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, OBFUSCATE_STRING("Software\\Cobble\\Loader").c_str(), 0,
                      KEY_READ, &key) != ERROR_SUCCESS)
        return "";

    char buf[64] = {0};
    DWORD size = sizeof(buf);
    RegQueryValueExA(key, OBFUSCATE_STRING("HighestSeenVersion").c_str(), nullptr, nullptr, (LPBYTE)buf, &size);
    RegCloseKey(key);
    return buf;
}

inline void setHighestSeen(const std::string& version) {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, OBFUSCATE_STRING("Software\\Cobble\\Loader").c_str(), 0,
                        nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExA(key, OBFUSCATE_STRING("HighestSeenVersion").c_str(), 0, REG_SZ, (LPBYTE)version.c_str(),
                       (DWORD)version.size() + 1);
        RegCloseKey(key);
    }
}


inline std::vector<uint8_t> base64Decode(const std::string& encoded) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '=')
            break;
        const char* p = strchr(b64, c);
        if (!p)
            continue;
        val = (val << 6) + (int)(p - b64);
        valb += 6;
        if (valb >= 0) {
            result.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}


inline CheckResult TryCheck(const std::string& currentVersion, const std::string& serverHost, int port,
                            const std::string& highestSeen) {
    CheckResult result;


    HINTERNET session =
        WinHttpOpen(OBFUSCATE_WSTRING(L"Mozilla/5.0").c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) {
        result.error = OBFUSCATE_STRING("WinHttpOpen failed");
        return result;
    }

    std::wstring host(serverHost.begin(), serverHost.end());
    HINTERNET connect = WinHttpConnect(session, host.c_str(), (INTERNET_PORT)port, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        result.error = OBFUSCATE_STRING("WinHttpConnect failed");
        return result;
    }

    DWORD flags = (port == 443) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", L"/api/update/manifest", nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = OBFUSCATE_STRING("WinHttpOpenRequest failed");
        return result;
    }


    DWORD disableRedirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &disableRedirects, sizeof(disableRedirects));


    DWORD timeout = 2000;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = OBFUSCATE_STRING("Req failed (Err ") + std::to_string(err) + ")";
        return result;
    }


    std::string body;
    DWORD bytesRead = 0;
    char buf[4096];
    while (WinHttpReadData(request, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        body.append(buf, bytesRead);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);


    json::Object response;
    if (!response.parse(body)) {
        result.error = OBFUSCATE_STRING("Invalid JSON");
        return result;
    }

    std::string manifestB64 = response.get("manifest_b64").as_string();
    std::string sigB64 = response.get("sig_b64").as_string();

    if (manifestB64.empty() || sigB64.empty()) {
        result.error = OBFUSCATE_STRING("Missing manifest or signature");
        return result;
    }


    std::vector<uint8_t> manifestBytes = base64Decode(manifestB64);
    std::vector<uint8_t> sigBytes = base64Decode(sigB64);

    if (sigBytes.size() != 64) {
        result.error = OBFUSCATE_STRING("Invalid signature size");
        return result;
    }

    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    bool sigOk = tweetnacl::verify(sigBytes.data(), manifestBytes.data(), manifestBytes.size(), pubkey);
    SecureZeroMemory(pubkey, 32);

    if (!sigOk) {
        result.error = OBFUSCATE_STRING("Signature verification failed");
        return result;
    }


    std::string manifestJson(manifestBytes.begin(), manifestBytes.end());
    json::Object manifest;
    if (!manifest.parse(manifestJson)) {
        result.error = OBFUSCATE_STRING("Invalid manifest JSON");
        return result;
    }

    result.manifest.version = manifest.get("version").as_string();
    result.manifest.minAllowed = manifest.get("min_allowed").as_string();
    result.manifest.mandatory = manifest.get("mandatory").as_bool();
    result.manifest.notes = manifest.get("notes").as_string();


    result.manifest.loader.sha256 = manifest.get("loader_sha256").as_string();
    result.manifest.loader.size = manifest.get("loader_size").as_int();
    result.manifest.dll.sha256 = manifest.get("dll_sha256").as_string();
    result.manifest.dll.size = manifest.get("dll_size").as_int();


    if (result.manifest.mandatory) {
        if (compareVersions(currentVersion, result.manifest.minAllowed) < 0) {
            result.blocked = true;
        }
        if (compareVersions(currentVersion, highestSeen) < 0) {
            result.error = "Downgrade detected";
            return result;
        }
    }

    result.updateAvailable = compareVersions(result.manifest.version, currentVersion) > 0;
    result.mandatory = result.blocked || (result.manifest.mandatory && result.updateAvailable);
    result.success = true;

    return result;
}


inline CheckResult Check(const std::string& currentVersion, const std::string& host, int port) {
    std::string highestSeen = getHighestSeen();
    if (highestSeen.empty()) {
        highestSeen = currentVersion;
        setHighestSeen(highestSeen);
    }


    return TryCheck(currentVersion, host, port, highestSeen);
}

}
