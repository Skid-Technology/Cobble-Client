#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    std::wstring windowTitle;
};

std::vector<ProcessInfo> findProcessesByName(const std::wstring& name);
ProcessInfo findLunarClient();
bool isProcessRunning(DWORD pid);
