#pragma once

#include <intrin.h>
#include <iphlpapi.h>
#include <wincrypt.h>
#include <Windows.h>
#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")




namespace hwid {

inline std::string hash(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hashData[32] = {0};
    DWORD hashLen = 32;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, (BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hashData[i];
    }

    return ss.str();
}

inline std::string getCpuId() {
    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 0);
    char vendor[13] = {0};
    memcpy(vendor, &cpuInfo[1], 4);
    memcpy(vendor + 4, &cpuInfo[3], 4);
    memcpy(vendor + 8, &cpuInfo[2], 4);

    __cpuid(cpuInfo, 1);

    std::stringstream ss;
    ss << vendor << "-" << std::hex << cpuInfo[0] << cpuInfo[3];

    return ss.str();
}

inline std::string getDiskSerial() {
    DWORD volumeSerial = 0;

    if (GetVolumeInformationW(L"C:\\", NULL, 0, &volumeSerial, NULL, NULL, NULL, 0)) {
        std::stringstream ss;
        ss << std::hex << std::uppercase << volumeSerial;
        return ss.str();
    }

    return "UNKNOWN";
}

inline std::string getMacAddress() {
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufferSize = sizeof(adapterInfo);

    if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapter = adapterInfo;

        while (adapter) {
            if (adapter->AddressLength == 6) {
                bool isZero = true;
                for (int i = 0; i < 6; i++) {
                    if (adapter->Address[i] != 0) {
                        isZero = false;
                        break;
                    }
                }

                if (!isZero) {
                    std::stringstream ss;
                    for (int i = 0; i < 6; i++) {
                        ss << std::hex << std::setw(2) << std::setfill('0') << (int)adapter->Address[i];
                    }
                    return ss.str();
                }
            }
            adapter = adapter->Next;
        }
    }

    return "UNKNOWN";
}

inline std::string generate() {
    std::string combined;
    combined += getCpuId();
    combined += "|";
    combined += getDiskSerial();
    combined += "|";
    combined += getMacAddress();

    return hash(combined);
}

}  
