#pragma once
#include <Windows.h>
#include <winternl.h>
#include <cstdint>

#pragma comment(lib, "ntdll.lib")

class AdvancedSecurity {
private:

    typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
    typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    typedef NTSTATUS(NTAPI* NtQueryInformationThread_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    typedef NTSTATUS(NTAPI* NtResumeThread_t)(HANDLE, PULONG);

    static inline NtQuerySystemInformation_t pNtQuerySystemInformation = nullptr;
    static inline NtQueryInformationProcess_t pNtQueryInformationProcess = nullptr;
    static inline NtQueryInformationThread_t pNtQueryInformationThread = nullptr;
    static inline NtResumeThread_t pNtResumeThread = nullptr;

    static bool initNtFunctions() {
        if (pNtQuerySystemInformation)
            return true;

        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;

        pNtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(ntdll, "NtQuerySystemInformation");
        pNtQueryInformationProcess = (NtQueryInformationProcess_t)GetProcAddress(ntdll, "NtQueryInformationProcess");
        pNtQueryInformationThread = (NtQueryInformationThread_t)GetProcAddress(ntdll, "NtQueryInformationThread");
        pNtResumeThread = (NtResumeThread_t)GetProcAddress(ntdll, "NtResumeThread");

        return pNtQuerySystemInformation && pNtQueryInformationProcess;
    }

public:

    static bool isVirtualMachine() {
        if (!initNtFunctions())
            return false;


        struct SYSTEM_HYPERVISOR_DETAIL_INFORMATION {
            ULONG HvVendorAndMaxFunction[4];
            ULONG HvInterface[4];
        } shdi = {};

        NTSTATUS status = pNtQuerySystemInformation(0x9F, &shdi, sizeof(shdi), nullptr);


        if (status == 0) {

            bool isZeroed = true;
            for (int i = 0; i < 4 && isZeroed; i++) {
                if (shdi.HvVendorAndMaxFunction[i] != 0)
                    isZeroed = false;
            }

            if (!isZeroed) {

                char vendor[13] = {};
                memcpy(vendor, &shdi.HvVendorAndMaxFunction[1], 12);



                if (strcmp(vendor, "Microsoft Hv") != 0 && vendor[0] != '\0') {
                    return true;
                }
            }
        }

        return false;
    }


    static bool hasKernelDebugger() {
        if (!initNtFunctions())
            return false;


        struct SYSTEM_KERNEL_DEBUGGER_INFORMATION {
            BOOLEAN DebuggerEnabled;
            BOOLEAN DebuggerNotPresent;
        } skdi = {};

        NTSTATUS status = pNtQuerySystemInformation(0x23, &skdi, sizeof(skdi), nullptr);

        if (status == 0) {

            if (skdi.DebuggerEnabled || !skdi.DebuggerNotPresent) {
                return true;
            }
        }


        HANDLE debugPort = nullptr;

        if (pNtQueryInformationProcess) {
            status = pNtQueryInformationProcess(GetCurrentProcess(), 0x7, &debugPort, sizeof(debugPort), nullptr);
            if (status == 0 && debugPort != nullptr) {
                return true;
            }
        }


        HANDLE debugObject = nullptr;
        if (pNtQueryInformationProcess) {
            status = pNtQueryInformationProcess(GetCurrentProcess(), 0x1E, &debugObject, sizeof(debugObject), nullptr);

            if (status == 0) {
                return true;
            }
        }

        return false;
    }


    static bool isThreadSuspended(HANDLE hThread) {
        if (!hThread || !pNtResumeThread)
            return false;

        ULONG suspendCount = 0;
        NTSTATUS status = pNtResumeThread(hThread, &suspendCount);

        if (status == 0 && suspendCount > 0) {

            SuspendThread(hThread);
            return true;
        }

        return false;
    }


    static bool hasSyscallHooks() {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return true;


        const char* criticalFunctions[] = {
            "NtQuerySystemInformation", "NtQueryInformationProcess", "NtOpenProcess",           "NtReadVirtualMemory",
            "NtWriteVirtualMemory",     "NtProtectVirtualMemory",    "NtAllocateVirtualMemory", "NtCreateThreadEx"};

        for (const char* funcName : criticalFunctions) {
            FARPROC func = GetProcAddress(ntdll, funcName);
            if (!func)
                continue;

            uint8_t* bytes = (uint8_t*)func;







            bool isStandardStub = (bytes[0] == 0x4C && bytes[1] == 0x8B && bytes[2] == 0xD1 && bytes[3] == 0xB8);


            bool isJmpHook = (bytes[0] == 0xE9);
            bool isCallHook = (bytes[0] == 0xE8);
            bool isPushRetHook = (bytes[0] == 0x68 && bytes[5] == 0xC3);
            bool isMovRaxHook = (bytes[0] == 0x48 && bytes[1] == 0xB8);

            if (!isStandardStub || isJmpHook || isCallHook || isPushRetHook || isMovRaxHook) {
                return true;
            }
        }

        return false;
    }


    static bool isCompromised() { return isVirtualMachine() || hasKernelDebugger() || hasSyscallHooks(); }
};
