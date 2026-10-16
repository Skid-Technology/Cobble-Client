#include "test6.h"
#include <Psapi.h>
#include "../test6/test30.hpp"

#pragma comment(lib, "psapi.lib")


struct EnumWindowsData {
    DWORD targetPid;
    std::wstring windowTitle;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    if (pid == data->targetPid) {
        wchar_t title[256];
        if (GetWindowTextW(hwnd, title, 256) > 0) {
            data->windowTitle = title;
            return FALSE;
        }
    }
    return TRUE;
}

std::wstring getWindowTitle(DWORD pid) {
    EnumWindowsData data = {pid, L""};
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.windowTitle;
}

std::vector<ProcessInfo> findProcessesByName(const std::wstring& name) {
    std::vector<ProcessInfo> results;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return results;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
                ProcessInfo info;
                info.pid = entry.th32ProcessID;
                info.name = entry.szExeFile;
                info.windowTitle = getWindowTitle(info.pid);
                results.push_back(info);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return results;
}

ProcessInfo findLunarClient() {

    auto javaProcesses = findProcessesByName(OBFUSCATE_WSTRING(L"javaw.exe"));
    auto javaExeProc = findProcessesByName(OBFUSCATE_WSTRING(L"java.exe"));
    javaProcesses.insert(javaProcesses.end(), javaExeProc.begin(), javaExeProc.end());


    for (const auto& proc : javaProcesses) {
        if (proc.windowTitle.find(OBFUSCATE_WSTRING(L"MuzCraft")) != std::wstring::npos ||
            proc.windowTitle.find(OBFUSCATE_WSTRING(L"Lunar Client")) != std::wstring::npos ||
            proc.windowTitle.find(OBFUSCATE_WSTRING(L"Badlion Minecraft")) != std::wstring::npos ||
            proc.windowTitle.find(OBFUSCATE_WSTRING(L"Badlion Client")) != std::wstring::npos ||
            proc.windowTitle.find(OBFUSCATE_WSTRING(L"Minecraft")) != std::wstring::npos) {
            return proc;
        }
    }



    if (!javaProcesses.empty()) {
        return javaProcesses[0];
    }


    return {0, L"", L""};
}

bool isProcessRunning(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return false;

    DWORD exitCode;
    if (GetExitCodeProcess(process, &exitCode)) {
        CloseHandle(process);
        return exitCode == STILL_ACTIVE;
    }

    CloseHandle(process);
    return false;
}
