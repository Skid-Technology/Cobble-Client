#pragma once

#include <Windows.h>
#include <winternl.h>
#include <cstdint>

#include "../util/aes_string.hpp"










namespace AntiAttach {




using NtQueryInformationProcess_t =
    NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

inline NtQueryInformationProcess_t resolveNtQip() {
    static NtQueryInformationProcess_t fn = nullptr;
    if (fn)
        return fn;
    HMODULE hNtdll = GetModuleHandleW(OBFUSCATE_WSTRING(L"ntdll.dll").c_str());
    if (!hNtdll)
        return nullptr;
    fn = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(hNtdll, OBFUSCATE_STRING("NtQueryInformationProcess").c_str()));
    return fn;
}




inline bool hasDebugPort() {
    auto fn = resolveNtQip();
    if (!fn)
        return false;
    DWORD_PTR debugPort = 0;
    NTSTATUS s = fn(GetCurrentProcess(), 7  , &debugPort, sizeof(debugPort), nullptr);
    return (s == 0  ) && (debugPort != 0);
}




inline bool hasDebugObject() {
    auto fn = resolveNtQip();
    if (!fn)
        return false;
    HANDLE dbgObj = nullptr;
    NTSTATUS s = fn(GetCurrentProcess(), 0x1E  , &dbgObj, sizeof(dbgObj), nullptr);
    return (s == 0) && (dbgObj != nullptr);
}



inline bool hasDebugFlags() {
    auto fn = resolveNtQip();
    if (!fn)
        return false;
    ULONG noDebug = 1;
    NTSTATUS s = fn(GetCurrentProcess(), 0x1F  , &noDebug, sizeof(noDebug), nullptr);
    return (s == 0) && (noDebug == 0);
}






inline bool hasHardwareBreakpoint() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;
    return (ctx.Dr0 | ctx.Dr1 | ctx.Dr2 | ctx.Dr3) != 0;
}



inline bool isBeingDebugged() {
#ifdef _WIN64
    PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
#else
    PPEB pPeb = reinterpret_cast<PPEB>(__readfsdword(0x30));
#endif
    return pPeb && pPeb->BeingDebugged;
}




inline bool quickCheck() {
    if (isBeingDebugged())
        return true;
    if (hasHardwareBreakpoint())
        return true;
    if (hasDebugPort())
        return true;
    if (hasDebugObject())
        return true;
    if (hasDebugFlags())
        return true;
    return false;
}

}  
