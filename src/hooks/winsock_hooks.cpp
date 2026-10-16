#include "winsock_hooks.h"


#include <WinSock2.h>

#include "../sdk/core/minecraft.h"
#include "../util/logger.hpp"

#include <Windows.h>
#include <winternl.h>  
#include "../../infra/ext/minhook/minhook.h"

#pragma comment(lib, "ws2_32.lib")


#ifndef NTAPI
#define NTAPI __stdcall
#endif


#define IOCTL_AFD_SEND 0x1201F


typedef NTSTATUS(NTAPI* NtDeviceIoControlFile_t)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                                                 PVOID IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer,
                                                 ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength);

namespace {

typedef int(WINAPI* WSASend_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED,
                               LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int(WINAPI* WSARecv_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED,
                               LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int(WINAPI* send_t)(SOCKET, const char*, int, int);

WSASend_t OriginalWSASend = nullptr;
WSARecv_t OriginalWSARecv = nullptr;
send_t OriginalSend = nullptr;
NtDeviceIoControlFile_t OriginalNtDeviceIoControlFile = nullptr;

bool hooksInitialized = false;
std::atomic<int> blockedPacketCount{0};
std::atomic<int> sendCallCount{0};
std::atomic<int> afdSendBlockedCount{0};  



std::atomic<DWORD> exemptThreadId{0};

bool callerIsExempt() {
    const DWORD id = exemptThreadId.load(std::memory_order_relaxed);
    return id != 0 && id == GetCurrentThreadId();
}


NTSTATUS NTAPI HookedNtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                                           PVOID IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer,
                                           ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength) {
    
    if (IoControlCode == IOCTL_AFD_SEND && WinsockHooks::StopOutbound.load() && !callerIsExempt()) {
        afdSendBlockedCount++;

        
        while (WinsockHooks::StopOutbound.load()) {
            Sleep(1);
        }
    }

    return OriginalNtDeviceIoControlFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, IoControlCode,
                                        InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}


int WINAPI HookedWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
                         LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    if (WinsockHooks::StopOutbound.load() && !callerIsExempt()) {
        blockedPacketCount++;
        while (WinsockHooks::StopOutbound.load()) {
            Sleep(1);
        }
    }

    return OriginalWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped,
                           lpCompletionRoutine);
}


int WINAPI HookedSend(SOCKET s, const char* buf, int len, int flags) {
    if (WinsockHooks::StopOutbound.load()) {
        sendCallCount++;
    }

    return OriginalSend(s, buf, len, flags);
}


int WINAPI HookedWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd,
                         LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped,
                         LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    if (!callerIsExempt()) {
        while (WinsockHooks::StopInbound.load()) {
            Sleep(1);
        }
    }

    return OriginalWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped,
                           lpCompletionRoutine);
}
}  

void WinsockHooks::ExemptCurrentThread() {
    exemptThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
}

void WinsockHooks::UnexemptCurrentThread() {
    DWORD self = GetCurrentThreadId();
    exemptThreadId.compare_exchange_strong(self, 0, std::memory_order_relaxed);
}

void WinsockHooks::ReleaseStalls() {
    StopOutbound.store(false);
    StopInbound.store(false);
}

bool WinsockHooks::Init() {
    if (hooksInitialized)
        return true;

    
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        log_err("Failed to initialize MinHook: %d", status);
        return false;
    }

    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        log_err("Failed to get ntdll.dll");
        return false;
    }

    LPVOID pNtDeviceIoControlFile = (LPVOID)GetProcAddress(ntdll, "NtDeviceIoControlFile");
    if (!pNtDeviceIoControlFile) {
        log_err("Failed to find NtDeviceIoControlFile");
        return false;
    }

    
    if (MH_CreateHook(pNtDeviceIoControlFile, &HookedNtDeviceIoControlFile, (LPVOID*)&OriginalNtDeviceIoControlFile) !=
        MH_OK) {
        log_err("Failed to create NtDeviceIoControlFile hook");
        return false;
    }

    if (MH_EnableHook(pNtDeviceIoControlFile) != MH_OK) {
        log_err("Failed to enable NtDeviceIoControlFile hook");
        MH_RemoveHook(pNtDeviceIoControlFile);
        return false;
    }

    log_ok("NtDeviceIoControlFile hook installed (Java NIO interception)");

    
    HMODULE ws2_32 = GetModuleHandleA("WS2_32.dll");
    if (ws2_32) {
        LPVOID pWSASend = (LPVOID)GetProcAddress(ws2_32, "WSASend");
        LPVOID pWSARecv = (LPVOID)GetProcAddress(ws2_32, "WSARecv");
        LPVOID pSend = (LPVOID)GetProcAddress(ws2_32, "send");

        log_debug("[WSA] hooking WSASend @ 0x%p", pWSASend);
        if (pWSASend) {
            if (MH_CreateHook(pWSASend, &HookedWSASend, (LPVOID*)&OriginalWSASend) == MH_OK) {
                MH_EnableHook(pWSASend);
                log_debug("[WSA] WSASend hooked");
            } else {
                log_warn("[WSA] WSASend hook FAILED");
            }
        }

        
        
        
        

        log_debug("[WSA] hooking WSARecv @ 0x%p", pWSARecv);
        if (pWSARecv) {
            if (MH_CreateHook(pWSARecv, &HookedWSARecv, (LPVOID*)&OriginalWSARecv) == MH_OK) {
                MH_EnableHook(pWSARecv);
                log_debug("[WSA] WSARecv hooked");
            } else {
                log_warn("[WSA] WSARecv hook FAILED");
            }
        }
    } else {
        log_warn("[WSA] WS2_32.dll not found");
    }

    hooksInitialized = true;
    log_ok("Network hooks installed (NtDeviceIoControlFile + Winsock)");
    return true;
}

void WinsockHooks::Shutdown() {
    if (!hooksInitialized)
        return;

    
    StopOutbound = false;
    StopInbound = false;

    Sleep(50);  

    
    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);

    hooksInitialized = false;

    MH_Uninitialize();

    log_ok("Network hooks removed");
}

bool WinsockHooks::IsInitialized() {
    return hooksInitialized;
}

int WinsockHooks::GetBlockedCount() {
    return blockedPacketCount.load();
}

int WinsockHooks::GetSendCount() {
    return sendCallCount.load();
}

int WinsockHooks::GetAfdSendBlockedCount() {
    return afdSendBlockedCount.load();
}

void WinsockHooks::ResetBlockedCount() {
    blockedPacketCount = 0;
    sendCallCount = 0;
    afdSendBlockedCount = 0;
}
