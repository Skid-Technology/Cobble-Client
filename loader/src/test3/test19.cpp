#include "test20.h"
#include <DbgHelp.h>
#include <intrin.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include "../test2/test14.hpp"
#include "../test6/test30.hpp"
#include "test21.h"

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "dbghelp.lib")


#pragma optimize("", off)





extern "C" NTSTATUS NTAPI NtSetInformationThread(HANDLE ThreadHandle, THREADINFOCLASS ThreadInformationClass,
                                                 PVOID ThreadInformation, ULONG ThreadInformationLength);

const THREADINFOCLASS ThreadHideFromDebugger = (THREADINFOCLASS)0x11;

namespace Security {

#pragma region AntiAttach

void PatchDbgBreakPoint() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll)
        return;

    FARPROC pDbgBreakPoint = GetProcAddress(hNtdll, "DbgBreakPoint");
    if (!pDbgBreakPoint)
        return;

    DWORD dwOldProtect;
    if (!VirtualProtect(pDbgBreakPoint, 1, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        return;

    *(PBYTE)pDbgBreakPoint = 0xC3;

    VirtualProtect(pDbgBreakPoint, 1, dwOldProtect, &dwOldProtect);
}

void PatchDbgUiRemoteBreakin() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll)
        return;

    FARPROC pDbgUiRemoteBreakin = GetProcAddress(hNtdll, "DbgUiRemoteBreakin");
    if (!pDbgUiRemoteBreakin)
        return;

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32)
        return;

    FARPROC pTerminateProcess = GetProcAddress(hKernel32, "TerminateProcess");
    if (!pTerminateProcess)
        return;

    DbgUiRemoteBreakinPatch patch = {0};
    patch.push_0 = 0x006A;
    patch.push = 0x68;
    patch.CurrentPorcessHandle = 0xFFFFFFFF;
    patch.mov_eax = 0xB8;
    patch.TerminateProcess = (DWORD)(DWORD_PTR)pTerminateProcess;
    patch.call_eax = 0xD0FF;

    DWORD dwOldProtect;
    if (!VirtualProtect(pDbgUiRemoteBreakin, sizeof(patch), PAGE_EXECUTE_READWRITE, &dwOldProtect))
        return;

    memcpy(pDbgUiRemoteBreakin, &patch, sizeof(patch));

    VirtualProtect(pDbgUiRemoteBreakin, sizeof(patch), dwOldProtect, &dwOldProtect);
}

void HideThread(HANDLE hThread) {
    NtSetInformationThread(hThread, ThreadHideFromDebugger, NULL, 0);
}

bool ShouldApplyAntiAttach() {



    int protectedResult = 0;
    CHECK_PROTECTION(protectedResult, 1);
    if (protectedResult != 1) {
        return false;
    }

    int debugResult = 0;
    CHECK_DEBUGGER(debugResult, 0);
    return debugResult != 0;
}

#pragma endregion

#pragma region AntiVM

bool HasVMRegistry() {
    std::vector<std::wstring> vmKeys;
    vmKeys.reserve(6);


    COBBLE_STR_ENCRYPTW_BEGIN
    vmKeys.push_back(OBFUSCATE_WSTRING(L"System\\ControlSet001\\Services\\VMTools"));
    vmKeys.push_back(OBFUSCATE_WSTRING(L"System\\ControlSet001\\Services\\VBoxGuest"));
    vmKeys.push_back(OBFUSCATE_WSTRING(L"System\\ControlSet001\\Services\\VBoxMouse"));
    vmKeys.push_back(OBFUSCATE_WSTRING(L"System\\ControlSet001\\Services\\VBoxService"));
    vmKeys.push_back(OBFUSCATE_WSTRING(L"SOFTWARE\\Oracle\\VirtualBox Guest Additions"));
    vmKeys.push_back(OBFUSCATE_WSTRING(L"SOFTWARE\\VMware, Inc.\\VMware Tools"));
    COBBLE_STR_ENCRYPTW_END

    for (const auto& keyPath : vmKeys) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
    }

    return false;
}

bool HasVMFiles() {
    std::vector<std::wstring> vmFiles;
    vmFiles.reserve(6);


    COBBLE_STR_ENCRYPTW_BEGIN
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\vm3dgl.dll"));
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\vboxdisp.dll"));
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\vboxhook.dll"));
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\vboxogl.dll"));
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\drivers\\vboxmouse.sys"));
    vmFiles.push_back(OBFUSCATE_WSTRING(L"C:\\Windows\\System32\\drivers\\vmhgfs.sys"));
    COBBLE_STR_ENCRYPTW_END

    for (const auto& filePath : vmFiles) {
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            return true;
        }
    }

    return false;
}

bool HasVMWareACPI() {
    DWORD acpiDataSize = GetSystemFirmwareTable('ACPI', 'TEPH', NULL, 0);
    if (acpiDataSize == 0)
        return false;

    BYTE* acpiData = (BYTE*)HeapAlloc(GetProcessHeap(), 0, acpiDataSize);
    if (!acpiData)
        return false;

    DWORD bytesWritten = GetSystemFirmwareTable('ACPI', 'TEPH', acpiData, acpiDataSize);

    if (bytesWritten != acpiDataSize) {
        HeapFree(GetProcessHeap(), 0, acpiData);
        return false;
    }


    std::string haystack((char*)acpiData, bytesWritten);
    bool found = (haystack.find("VMWARE") != std::string::npos) || (haystack.find("VBOX") != std::string::npos) ||
                 (haystack.find("QEMU") != std::string::npos);

    HeapFree(GetProcessHeap(), 0, acpiData);
    return found;
}

bool IsOnVM() {

    if (HasVMRegistry() || HasVMFiles() || HasVMWareACPI()) {
        return true;
    }

    return false;
}

#pragma endregion

#pragma region AntiHook

bool HasAPIHooks() {

    struct APICheck {
        const wchar_t* module;
        const char* function;
    };

    APICheck apis[] = {

        {L"ntdll.dll", "NtQueryInformationProcess"}, {L"ntdll.dll", "NtSetInformationThread"},
        {L"ntdll.dll", "DbgUiRemoteBreakin"},        {L"ntdll.dll", "DbgBreakPoint"},
        {L"ntdll.dll", "NtReadVirtualMemory"},
    };

    for (const auto& api : apis) {
        HMODULE hMod = GetModuleHandleW(api.module);
        if (!hMod)
            continue;

        FARPROC pFunc = GetProcAddress(hMod, api.function);
        if (!pFunc)
            continue;

        BYTE* pBytes = (BYTE*)pFunc;


        while (*pBytes == 0x90)
            pBytes++;


        bool hasJmp = false;
        void* jumpTarget = nullptr;

        if (*pBytes == 0xE9) {

            int32_t offset = *(int32_t*)(pBytes + 1);
            jumpTarget = pBytes + 5 + offset;
            hasJmp = true;
        } else if (*pBytes == 0xFF && pBytes[1] == 0x25) {

            int32_t offset = *(int32_t*)(pBytes + 2);
            void** pTarget = (void**)(pBytes + 6 + offset);
            jumpTarget = *pTarget;
            hasJmp = true;
        } else if (*pBytes == 0x48 && pBytes[1] == 0xB8 && pBytes[10] == 0xFF && pBytes[11] == 0xE0) {

            jumpTarget = *(void**)(pBytes + 2);
            hasJmp = true;
        }

        if (!hasJmp)
            continue;




        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(jumpTarget, &mbi, sizeof(mbi)) == 0)
            continue;


        if (mbi.Type == MEM_IMAGE)
            continue;


        bool isRWX = (mbi.Protect & PAGE_EXECUTE_READWRITE) || (mbi.Protect & PAGE_EXECUTE_WRITECOPY);
        bool isPrivate = (mbi.Type == MEM_PRIVATE);

        if (isRWX || isPrivate) {
            return true;
        }
    }

    return false;
}

#pragma endregion

#pragma region AntiTamper

static int GetTextSectionInfo(LPVOID lpModBase, SECTIONINFO* info) {
    PIMAGE_NT_HEADERS pNtHdr = ImageNtHeader(lpModBase);
    if (!pNtHdr)
        return -1;

    PIMAGE_SECTION_HEADER pSection = (PIMAGE_SECTION_HEADER)(pNtHdr + 1);

    for (int i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++) {
        if (strcmp((char*)pSection->Name, ".text") == 0) {
            info->lpVirtualAddress = (LPVOID)((DWORD_PTR)lpModBase + pSection->VirtualAddress);
            info->dwSizeOfRawData = pSection->SizeOfRawData;
            return 0;
        }
        pSection++;
    }

    return -1;
}

DWORD64 HashSection(LPVOID lpAddress, DWORD dwSize) {
    DWORD64 hash = 0;
    BYTE* pData = (BYTE*)lpAddress;

    for (DWORD i = 0; i < dwSize; i++, pData++) {
        hash = *pData + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

std::vector<HASHSET> GetModuleSectionHashes() {
    std::vector<HASHSET> hashes;

    MODULEENTRY32W mEntry = {sizeof(MODULEENTRY32W)};
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
        return hashes;

    if (Module32FirstW(hSnapshot, &mEntry)) {
        do {
            SECTIONINFO info = {0};
            if (GetTextSectionInfo(mEntry.modBaseAddr, &info) == 0) {
                DWORD64 hash = HashSection(info.lpVirtualAddress, info.dwSizeOfRawData);
                hashes.push_back({hash, info});
            }
        } while (Module32NextW(hSnapshot, &mEntry));
    }

    CloseHandle(hSnapshot);
    return hashes;
}

bool VerifyIntegrity(const std::vector<HASHSET>& originalHashes) {
    for (const auto& entry : originalHashes) {
        DWORD64 currentHash = HashSection(entry.SectionInfo.lpVirtualAddress, entry.SectionInfo.dwSizeOfRawData);
        if (currentHash != entry.dwRealHash) {
            return false;
        }
    }
    return true;
}

#pragma endregion

#pragma region Initialization

void Initialize() {

    if (ShouldApplyAntiAttach()) {
        PatchDbgBreakPoint();
        PatchDbgUiRemoteBreakin();
        HideThread(GetCurrentThread());
    }
}

#pragma endregion

#pragma region DetectionSystem


std::vector<Detection> CheckRunningTools() {
    std::vector<Detection> detections;


    struct ToolDef {
        const wchar_t* name;
        const char* code;
        int score;
    };


    COBBLE_STR_ENCRYPTW_BEGIN


    ToolDef tools[] = {
        {L"x64dbg.exe", "TOOL_X64DBG", 10},
        {L"x32dbg.exe", "TOOL_X32DBG", 10},
        {L"ollydbg.exe", "TOOL_OLLYDBG", 10},
        {L"windbg.exe", "TOOL_WINDBG", 10},
        {L"ida.exe", "TOOL_IDA", 10},
        {L"ida64.exe", "TOOL_IDA64", 10},
        {L"idaq.exe", "TOOL_IDAQ", 10},
        {L"idaq64.exe", "TOOL_IDAQ64", 10},
        {L"dnspy.exe", "TOOL_DNSPY", 10},
        {L"cheatengine-x86_64.exe", "TOOL_CE", 10},
        {L"cheatengine.exe", "TOOL_CE", 10},
        {L"ce.exe", "TOOL_CE", 10},
        {L"processhacker.exe", "TOOL_PROCHACKER", 10},
        {L"extremeinjector.exe", "TOOL_INJECTOR", 10},
        {L"xenos.exe", "TOOL_XENOS", 10},
        {L"wireshark.exe", "TOOL_WIRESHARK", 10},
        {L"fiddler.exe", "TOOL_FIDDLER", 10},
        {L"charles.exe", "TOOL_CHARLES", 10},
        {L"httpdebugger.exe", "TOOL_HTTPDEBUG", 10},
        {L"mitmproxy.exe", "TOOL_MITMPROXY", 10},
    };

    COBBLE_STR_ENCRYPTW_END

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return detections;

    PROCESSENTRY32W pe = {sizeof(PROCESSENTRY32W)};

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            for (const auto& tool : tools) {
                if (_wcsicmp(pe.szExeFile, tool.name) == 0) {
                    Detection d;
                    d.code = tool.code;
                    d.score = tool.score;

                    d.details = OBFUSCATE_STRING("report");
                    detections.push_back(d);
                    break;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return detections;
}


std::vector<Detection> CheckVMProtect() {
    std::vector<Detection> detections;



    int protectedResult = 0;
    CHECK_PROTECTION(protectedResult, 1);
    if (protectedResult != 1) {
        return detections;
    }

    int result = 0;
    CHECK_DEBUGGER(result, 0);
    if (result != 0) {
        detections.push_back({OBFUSCATE_STRING("VMPROTECT_DEBUGGER"), 40, OBFUSCATE_STRING("debugger")});
    }

    result = 0;
    CHECK_VIRTUAL_PC(result, 0);
    if (result != 0) {
        detections.push_back({OBFUSCATE_STRING("VMPROTECT_VM"), 30, OBFUSCATE_STRING("vm")});
    }

    return detections;
}


std::vector<Detection> CheckVMIndicators() {
    std::vector<Detection> detections;

    if (HasVMRegistry()) {
        detections.push_back({OBFUSCATE_STRING("VM_REGISTRY"), 30, OBFUSCATE_STRING("registry")});
    }
    if (HasVMFiles()) {
        detections.push_back({OBFUSCATE_STRING("VM_FILES"), 30, OBFUSCATE_STRING("files")});
    }
    if (HasVMWareACPI()) {
        detections.push_back({OBFUSCATE_STRING("VM_ACPI"), 30, OBFUSCATE_STRING("acpi")});
    }

    return detections;
}


std::vector<Detection> RunSecurityChecks() {
    std::vector<Detection> all;


    auto vmProtectDetections = CheckVMProtect();
    for (auto& d : vmProtectDetections) {
        all.push_back(d);
    }


    auto toolDetections = CheckRunningTools();
    for (auto& d : toolDetections) {
        all.push_back(d);
    }


    auto vmDetections = CheckVMIndicators();
    for (auto& d : vmDetections) {
        all.push_back(d);
    }

    return all;
}


int CalculateRiskScore(const std::vector<Detection>& detections) {
    int maxScore = 0;
    for (const auto& d : detections) {
        if (d.score > maxScore) {
            maxScore = d.score;
        }
    }
    return maxScore;
}



SecurityDecision Decide(int score) {
    COBBLE_VM_CRITICAL_START

    SecurityDecision result;
    if (score >= 100)
        result = SecurityDecision::BLOCK;
    else if (score >= 40)
        result = SecurityDecision::WARN;
    else
        result = SecurityDecision::ALLOW;

    COBBLE_VM_CRITICAL_END
    return result;
}



SecurityDecision RunSecurityChecksWithDecision(std::vector<Detection>& outDetections) {

    outDetections = RunSecurityChecks();



    for (const auto& d : outDetections) {
        Telemetry::Severity sev = Telemetry::Severity::INFO;
        if (d.score >= 100)
            sev = Telemetry::Severity::CRITICAL;
        else if (d.score >= 40)
            sev = Telemetry::Severity::WARN;

        Telemetry::Report(d.code, sev, d.score, d.details);
    }


    int score = CalculateRiskScore(outDetections);
    return Decide(score);
}

#pragma endregion

}

#pragma optimize("", on)
