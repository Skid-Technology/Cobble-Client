#pragma once
#include <Psapi.h>
#include <Windows.h>
#include <atomic>
#include <cstdint>




namespace DllIntegrity {


inline uint32_t g_ExpectedHash = 0;
inline uintptr_t g_TextBase = 0;
inline size_t g_TextSize = 0;
inline std::atomic<bool> g_Initialized{false};


inline uint32_t hashMemory(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}


inline bool findTextSection(HMODULE hModule) {
    if (!hModule)
        return false;

    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uintptr_t)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
        return false;

    
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, section++) {
        if (memcmp(section->Name, ".text", 5) == 0) {
            g_TextBase = (uintptr_t)hModule + section->VirtualAddress;
            g_TextSize = section->Misc.VirtualSize;
            return true;
        }
    }

    return false;
}


inline bool Initialize(HMODULE hModule) {
    if (g_Initialized)
        return true;

    if (!findTextSection(hModule)) {
        return false;
    }

    
    g_ExpectedHash = hashMemory((const uint8_t*)g_TextBase, g_TextSize);
    g_Initialized = true;

    return true;
}



inline bool Verify() {
    if (!g_Initialized || g_TextBase == 0 || g_TextSize == 0) {
        return false;  
    }

    uint32_t currentHash = hashMemory((const uint8_t*)g_TextBase, g_TextSize);
    return (currentHash == g_ExpectedHash);
}


inline void OnTamperDetected() {
    
    volatile int* p = nullptr;
    *p = 0xDEAD;  
}




inline void Check() {
    if (Verify()) {
        return;  
    }

    
    
    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_MS = 50;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        Sleep(RETRY_DELAY_MS);
        if (Verify()) {
            return;  
        }
    }

    
    OnTamperDetected();
}

}  
