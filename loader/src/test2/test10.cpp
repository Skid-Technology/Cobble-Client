#include "test11.h"
#include <intrin.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <iomanip>
#include <sstream>


#include "../test3/test21.h"

#pragma comment(lib, "iphlpapi.lib")

namespace HWID {


static std::string SimpleHash(const std::string& input) {
    std::string result;


    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32] = {0};
    DWORD hashLen = 32;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        goto Done;
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        goto Done;
    }

    if (!CryptHashData(hHash, (BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
        goto Done;
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        goto Done;
    }


    {
        std::stringstream ss;
        for (int i = 0; i < 32; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        result = ss.str();
    }

Done:
    if (hHash) {
        CryptDestroyHash(hHash);
    }
    if (hProv) {
        CryptReleaseContext(hProv, 0);
    }

    return result;
}

std::string GetCPUID() {
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

std::string GetDiskSerial() {
    DWORD volumeSerial = 0;

    if (GetVolumeInformationW(L"C:\\", NULL, 0, &volumeSerial, NULL, NULL, NULL, 0)) {
        std::stringstream ss;
        ss << std::hex << std::uppercase << volumeSerial;
        return ss.str();
    }

    return "UNKNOWN";
}

std::string GetMACAddress() {
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

std::string Generate() {
    COBBLE_HWID_BEGIN


    std::string combined;
    combined += GetCPUID();
    combined += "|";
    combined += GetDiskSerial();
    combined += "|";
    combined += GetMACAddress();


    std::string result = SimpleHash(combined);

    COBBLE_HWID_END
    return result;
}

}
