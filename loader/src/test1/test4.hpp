#pragma once

#include <wincrypt.h>
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")







namespace PayloadCrypto {


struct PayloadHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t originalSize;
    uint32_t encryptedSize;
    uint8_t iv[16];
    uint8_t reserved[8];
};

constexpr uint8_t PAYLOAD_MAGIC[4] = {'C', 'B', 'L', 'P'};
constexpr uint32_t PAYLOAD_VERSION = 1;


inline bool HexToBytes(const std::string& hex, uint8_t* out, size_t outLen) {
    if (hex.length() != outLen * 2)
        return false;
    for (size_t i = 0; i < outLen; i++) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };
        int hv = hexVal(high), lv = hexVal(low);
        if (hv < 0 || lv < 0)
            return false;
        out[i] = (uint8_t)((hv << 4) | lv);
    }
    return true;
}




inline std::vector<uint8_t> DecryptPayloadWithKey(const uint8_t* encryptedData, size_t encryptedSize,
                                                  const std::string& hexKey) {
    if (!encryptedData || encryptedSize < sizeof(PayloadHeader) + 16) {
        return {};
    }


    uint8_t key[32];
    if (hexKey.length() != 64 || !HexToBytes(hexKey, key, 32)) {
        return {};
    }


    const PayloadHeader* header = reinterpret_cast<const PayloadHeader*>(encryptedData);


    if (memcmp(header->magic, PAYLOAD_MAGIC, 4) != 0) {
        return {};
    }


    if (header->version != PAYLOAD_VERSION) {
        return {};
    }


    if (header->encryptedSize > encryptedSize - sizeof(PayloadHeader)) {
        return {};
    }


    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        SecureZeroMemory(key, sizeof(key));
        return {};
    }


    struct {
        BLOBHEADER hdr;
        DWORD keySize;
        BYTE keyData[32];
    } keyBlob;

    keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
    keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
    keyBlob.hdr.reserved = 0;
    keyBlob.hdr.aiKeyAlg = CALG_AES_256;
    keyBlob.keySize = 32;
    memcpy(keyBlob.keyData, key, 32);


    HCRYPTKEY hKey = 0;
    if (!CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey)) {
        CryptReleaseContext(hProv, 0);
        SecureZeroMemory(key, sizeof(key));
        SecureZeroMemory(&keyBlob, sizeof(keyBlob));
        return {};
    }


    if (!CryptSetKeyParam(hKey, KP_IV, (BYTE*)header->iv, 0)) {
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        SecureZeroMemory(key, sizeof(key));
        SecureZeroMemory(&keyBlob, sizeof(keyBlob));
        return {};
    }


    DWORD mode = CRYPT_MODE_CBC;
    if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0)) {
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        SecureZeroMemory(key, sizeof(key));
        SecureZeroMemory(&keyBlob, sizeof(keyBlob));
        return {};
    }


    const uint8_t* ciphertext = encryptedData + sizeof(PayloadHeader);
    std::vector<uint8_t> plaintext(header->encryptedSize);
    memcpy(plaintext.data(), ciphertext, header->encryptedSize);

    DWORD decryptedLen = header->encryptedSize;
    if (!CryptDecrypt(hKey, 0, TRUE, 0, plaintext.data(), &decryptedLen)) {
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        SecureZeroMemory(key, sizeof(key));
        SecureZeroMemory(&keyBlob, sizeof(keyBlob));
        return {};
    }

    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);


    SecureZeroMemory(key, sizeof(key));
    SecureZeroMemory(&keyBlob, sizeof(keyBlob));

    plaintext.resize(decryptedLen);


    if (decryptedLen != header->originalSize) {
        return {};
    }

    return plaintext;
}


inline bool ValidatePE(const std::vector<uint8_t>& data) {
    if (data.size() < 0x1000)
        return false;

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data.data());
    if (dos->e_magic != 0x5A4D)
        return false;

    if ((size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS) > data.size())
        return false;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(data.data() + dos->e_lfanew);
    if (nt->Signature != 0x4550)
        return false;

    return true;
}

}
