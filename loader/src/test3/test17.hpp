#pragma once
#include <Psapi.h>
#include <Windows.h>
#include <cstdint>
#include <cstdlib>

namespace integrity_data {
extern const uint32_t EXPECTED_HASHES[];
extern const size_t NUM_CHUNKS;
}

class IntegrityGuard {
private:

    static uint32_t hashRegion(const uint8_t* data, size_t size) {

        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        BYTE hash[32];
        DWORD hashLen = 32;
        uint32_t result = 0;

        if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
                if (CryptHashData(hHash, data, static_cast<DWORD>(size), 0)) {
                    if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {

                        result = *reinterpret_cast<uint32_t*>(hash);
                    }
                }
                CryptDestroyHash(hHash);
            }
            CryptReleaseContext(hProv, 0);
        }

        return result;
    }


    static bool getModuleInfo(HMODULE* outModule, uint8_t** outBase, size_t* outSize) {
        *outModule = GetModuleHandle(nullptr);
        if (!*outModule)
            return false;

        MODULEINFO mi;
        if (!GetModuleInformation(GetCurrentProcess(), *outModule, &mi, sizeof(mi))) {
            return false;
        }

        *outBase = static_cast<uint8_t*>(mi.lpBaseOfDll);
        *outSize = mi.SizeOfImage;
        return true;
    }

public:

    static bool verify() {
        HMODULE hModule;
        uint8_t* base;
        size_t totalSize;

        if (!getModuleInfo(&hModule, &base, &totalSize)) {
            return false;
        }


        size_t chunkSize = totalSize / integrity_data::NUM_CHUNKS;


        for (size_t i = 0; i < integrity_data::NUM_CHUNKS; i++) {
            uint8_t* chunkStart = base + (i * chunkSize);
            size_t currentChunkSize = (i == integrity_data::NUM_CHUNKS - 1)
                                          ? (totalSize - (i * chunkSize))
                                          : chunkSize;

            uint32_t hash = hashRegion(chunkStart, currentChunkSize);

            if (hash != integrity_data::EXPECTED_HASHES[i]) {

                *(volatile int*)nullptr = 0;
                return false;
            }
        }

        return true;
    }


    static inline void checkpoint() {
        static int counter = 0;


        if (++counter % 17 == 0) {
            if (!verify()) {
                std::abort();
            }
        }
    }


    static inline void forceCheck() {
        if (!verify()) {
            *(volatile int*)nullptr = 0;
        }
    }
};


#define INTEGRITY_CHECK() IntegrityGuard::checkpoint()
#define INTEGRITY_FORCE_CHECK() IntegrityGuard::forceCheck()
