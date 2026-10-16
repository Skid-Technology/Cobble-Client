#pragma once

#include <wincrypt.h>
#include <Windows.h>
#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

namespace Hash {


inline std::string SHA256(const BYTE* data, DWORD size) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, data, size, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    DWORD hashLen = 32;
    BYTE hashBytes[32];
    if (CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0)) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (DWORD i = 0; i < hashLen; i++) {
            oss << std::setw(2) << (int)hashBytes[i];
        }
        result = oss.str();
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return result;
}


inline bool Verify(const BYTE* data, DWORD size, const std::string& expectedHash) {
    std::string actualHash = SHA256(data, size);
    if (actualHash.empty() || actualHash.length() != expectedHash.length()) {
        return false;
    }


    for (size_t i = 0; i < actualHash.length(); i++) {
        if (tolower(actualHash[i]) != tolower(expectedHash[i])) {
            return false;
        }
    }

    return true;
}



inline bool VerifyChunked(const BYTE* data, DWORD size, const char* chunk1, const char* chunk2, const char* chunk3,
                          const char* chunk4) {
    std::string actualHash = SHA256(data, size);
    if (actualHash.length() != 64)
        return false;





    for (int i = 0; i < 16; i++) {
        if (tolower(actualHash[32 + i]) != tolower(chunk3[i]))
            return false;
    }


    for (int i = 0; i < 16; i++) {
        if (tolower(actualHash[i]) != tolower(chunk1[i]))
            return false;
    }


    for (int i = 0; i < 16; i++) {
        if (tolower(actualHash[48 + i]) != tolower(chunk4[i]))
            return false;
    }


    for (int i = 0; i < 16; i++) {
        if (tolower(actualHash[16 + i]) != tolower(chunk2[i]))
            return false;
    }

    return true;
}

}
