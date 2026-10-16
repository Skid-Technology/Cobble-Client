#pragma once

#include <Psapi.h>
#include <Windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "psapi.lib")











namespace LoaderVerify {

inline bool isInModuleList(HMODULE hModule) {
#ifdef _WIN64
    PPEB pPeb = (PPEB)__readgsqword(0x60);
#else
    PPEB pPeb = (PPEB)__readfsdword(0x30);
#endif
    if (!pPeb || !pPeb->Ldr)
        return false;

    PLIST_ENTRY head = &pPeb->Ldr->InMemoryOrderModuleList;
    PLIST_ENTRY current = head->Flink;

    while (current != head) {
        PLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        if (entry->DllBase == hModule) {
            return true;  
        }
        current = current->Flink;
    }
    return false;  
}

inline bool verify(HMODULE hModule) {
    
    return !isInModuleList(hModule);
}



inline bool reverify(HMODULE hModule) {
    return verify(hModule);
}



inline void verifyOrDie(HMODULE hModule) {
    if (!verify(hModule)) {
        DWORD oldProtect;
        if (VirtualProtect(hModule, 0x1000, PAGE_READWRITE, &oldProtect)) {
            memset(hModule, 0xCC, 0x1000);  
            VirtualProtect(hModule, 0x1000, oldProtect, &oldProtect);
        }
        ExitProcess(0);
    }
}

}  
