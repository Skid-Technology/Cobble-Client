#pragma once
// ============================================================================
// AUTO-GENERATED ??? Payload AES key, XOR-encrypted with per-build random pad.
// Only included by the loader. The DLL never sees this.
// ============================================================================
#include <cstdint>
#include <string>

namespace cobble_payload {

namespace _internal {
    constexpr uint8_t KEY_ENC[32] = { 0x09, 0x00, 0x93, 0x58, 0x56, 0x6A, 0x77, 0xE9, 0xAD, 0x1B, 0x99, 0x53, 0x7F, 0xDE, 0x0F, 0x0C, 0x5E, 0x0B, 0x51, 0xBD, 0x51, 0x0D, 0x7E, 0x03, 0x5C, 0x92, 0x72, 0xF3, 0xB1, 0xA1, 0xAF, 0x89 };
    constexpr uint8_t KEY_PAD[32] = { 0x0C, 0xDA, 0x71, 0xC4, 0xD8, 0x15, 0x2A, 0xFA, 0xAB, 0x12, 0x17, 0xD1, 0x4D, 0xFE, 0xB6, 0x9E, 0xB5, 0x68, 0xD9, 0xA6, 0x61, 0x08, 0x4B, 0x19, 0x57, 0xCC, 0x05, 0x77, 0x5B, 0xB8, 0x76, 0x8E };
}

// Returns the 64-char hex payload key. Call once, use, then zero the string.
inline std::string GetPayloadKeyHex() {
    char hex[65];
    const char* hexchars = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        uint8_t b = _internal::KEY_ENC[i] ^ _internal::KEY_PAD[i];
        hex[i*2]   = hexchars[b >> 4];
        hex[i*2+1] = hexchars[b & 0x0F];
    }
    hex[64] = 0;
    return std::string(hex, 64);
}

} // namespace cobble_payload
