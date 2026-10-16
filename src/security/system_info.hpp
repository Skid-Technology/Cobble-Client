#pragma once






#include <intrin.h>
#include <TlHelp32.h>
#include <Windows.h>
#include <string>
#include <vector>

namespace SystemInfo {


inline std::string GetPCUsername() {
    char username[256] = {0};
    DWORD size = sizeof(username);
    if (GetUserNameA(username, &size)) {
        return std::string(username);
    }
    return "unknown";
}


inline std::string GetOSVersion() {
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS) {
        char productName[256] = {0};
        char displayVersion[64] = {0};
        DWORD size = sizeof(productName);
        DWORD type = REG_SZ;

        RegQueryValueExA(hKey, "ProductName", nullptr, &type, (LPBYTE)productName, &size);

        size = sizeof(displayVersion);
        RegQueryValueExA(hKey, "DisplayVersion", nullptr, &type, (LPBYTE)displayVersion, &size);

        RegCloseKey(hKey);

        std::string result = productName;
        if (displayVersion[0] != '\0') {
            result += " " + std::string(displayVersion);
        }
        return result;
    }
    return "Windows (unknown version)";
}


inline std::string GetCPUName() {
    char brand[49] = {0};
    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 0x80000000);
    unsigned int maxExtendedId = cpuInfo[0];

    if (maxExtendedId >= 0x80000004) {
        __cpuid(cpuInfo, 0x80000002);
        memcpy(brand, cpuInfo, sizeof(cpuInfo));

        __cpuid(cpuInfo, 0x80000003);
        memcpy(brand + 16, cpuInfo, sizeof(cpuInfo));

        __cpuid(cpuInfo, 0x80000004);
        memcpy(brand + 32, cpuInfo, sizeof(cpuInfo));

        
        std::string result = brand;
        size_t start = result.find_first_not_of(" ");
        if (start != std::string::npos) {
            result = result.substr(start);
        }
        return result;
    }
    return "Unknown CPU";
}


inline int GetRAMGB() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<int>(memInfo.ullTotalPhys / (1024 * 1024 * 1024));
    }
    return 0;
}


inline std::string GetGPUName() {
    HKEY hKey;
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        char gpuName[256] = {0};
        DWORD size = sizeof(gpuName);
        DWORD type = REG_SZ;

        
        if (RegQueryValueExA(hKey, "DriverDesc", nullptr, &type, (LPBYTE)gpuName, &size) != ERROR_SUCCESS) {
            size = sizeof(gpuName);
            RegQueryValueExA(hKey, "HardwareInformation.AdapterString", nullptr, &type, (LPBYTE)gpuName, &size);
        }

        RegCloseKey(hKey);

        if (gpuName[0] != '\0') {
            return std::string(gpuName);
        }
    }
    return "Unknown GPU";
}


inline std::vector<std::string> GetProcessList() {
    std::vector<std::string> processes;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            processes.push_back(pe32.szExeFile);
        } while (Process32Next(hSnapshot, &pe32) && processes.size() < 50);  
    }

    CloseHandle(hSnapshot);
    return processes;
}


inline std::string GetParentProcessName() {
    DWORD currentPid = GetCurrentProcessId();
    DWORD parentPid = 0;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return "unknown";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);

    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentPid) {
                parentPid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    
    std::string parentName = "unknown";
    if (parentPid != 0) {
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == parentPid) {
                    parentName = pe32.szExeFile;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
    }

    CloseHandle(hSnapshot);
    return parentName;
}


inline std::string GetMACAddress() {
    
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\0001", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        char mac[32] = {0};
        DWORD size = sizeof(mac);
        DWORD type = REG_SZ;

        if (RegQueryValueExA(hKey, "NetworkAddress", nullptr, &type, (LPBYTE)mac, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(mac);
        }
        RegCloseKey(hKey);
    }
    return "unknown";
}


struct SystemData {
    std::string pcUsername;
    std::string osVersion;
    std::string cpuName;
    int ramGB;
    std::string gpuName;
    std::string parentProcess;
    std::string macAddress;
    std::vector<std::string> processList;

    void Collect() {
        pcUsername = GetPCUsername();
        osVersion = GetOSVersion();
        cpuName = GetCPUName();
        ramGB = GetRAMGB();
        gpuName = GetGPUName();
        parentProcess = GetParentProcessName();
        macAddress = GetMACAddress();
        processList = GetProcessList();
    }
};


inline SystemData g_SystemData;


inline void Initialize() {
    g_SystemData.Collect();
}

}  
