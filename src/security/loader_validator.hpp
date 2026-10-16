#pragma once






#include <TlHelp32.h>
#include <Windows.h>
#include <string>

#include "../util/aes_string.hpp"
#include "audit_reporter.hpp"
#include "system_info.hpp"

namespace LoaderValidator {


struct ValidationResult {
    bool passed = false;
    std::string failedValidator;
    std::string details;
};





inline bool ValidateParentProcess() {
    std::string parent = SystemInfo::GetParentProcessName();

    
    std::string parentLower = parent;
    for (char& c : parentLower)
        c = tolower(c);

    
    
    if (parentLower.find(OBFUSCATE_STRING("javaw")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("java")) != std::string::npos) {
        return true;
    }

    
    
    if (parentLower.find(OBFUSCATE_STRING("explorer")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("cmd")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("powershell")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("x64dbg")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("ida")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("ollydbg")) != std::string::npos ||
        parentLower.find(OBFUSCATE_STRING("windbg")) != std::string::npos) {
        return false;
    }

    return true;  
}





inline bool ValidateLoaderProcessExists() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);

    bool found = false;
    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string procName = pe32.szExeFile;
            for (char& c : procName)
                c = tolower(c);

            if (procName.find(OBFUSCATE_STRING("cobbleloader")) != std::string::npos) {
                found = true;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return found;
}





inline bool ValidateInjectionTiming() {
    
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
        return false;
    }

    
    FILETIME nowTime;
    GetSystemTimeAsFileTime(&nowTime);

    
    ULARGE_INTEGER create, now;
    create.LowPart = createTime.dwLowDateTime;
    create.HighPart = createTime.dwHighDateTime;
    now.LowPart = nowTime.dwLowDateTime;
    now.HighPart = nowTime.dwHighDateTime;

    
    ULONGLONG diffSeconds = (now.QuadPart - create.QuadPart) / 10000000ULL;

    
    if (diffSeconds > 900) {
        return false;
    }

    return true;
}





inline bool ValidateNoDebugger() {
    if (IsDebuggerPresent()) {
        return false;
    }

    
    BOOL isRemoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebugger);
    if (isRemoteDebugger) {
        return false;
    }

    return true;
}





inline bool ValidateNoSuspiciousModules() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return true;  
    }

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(me32);

    bool clean = true;
    if (Module32First(hSnapshot, &me32)) {
        do {
            std::string modName = me32.szModule;
            for (char& c : modName)
                c = tolower(c);

            
            if (modName.find(OBFUSCATE_STRING("minhook")) != std::string::npos ||
                modName.find(OBFUSCATE_STRING("detours")) != std::string::npos ||
                modName.find(OBFUSCATE_STRING("frida")) != std::string::npos ||
                modName.find(OBFUSCATE_STRING("cheatengine")) != std::string::npos) {
                clean = false;
                break;
            }
        } while (Module32Next(hSnapshot, &me32));
    }

    CloseHandle(hSnapshot);
    return clean;
}





inline ValidationResult ValidateAll() {
    ValidationResult result;
    result.passed = true;

    
    if (!ValidateLoaderProcessExists()) {
        result.passed = false;
        result.failedValidator = OBFUSCATE_STRING("LoaderProcessValidator");
        result.details = OBFUSCATE_STRING("Loader process not found (manual injection suspected)");
        AuditReporter::ReportLoaderValidationFailed(result.failedValidator, result.details);
        return result;
    }

    
    if (!ValidateParentProcess()) {
        std::string parent = SystemInfo::GetParentProcessName();
        result.passed = false;
        result.failedValidator = OBFUSCATE_STRING("ProcessChainValidator");
        result.details = OBFUSCATE_STRING("Suspicious parent process: ") + parent;
        AuditReporter::ReportLoaderValidationFailed(result.failedValidator, result.details);
        return result;
    }

    
    if (!ValidateInjectionTiming()) {
        result.passed = false;
        result.failedValidator = OBFUSCATE_STRING("TimestampValidator");
        result.details = OBFUSCATE_STRING("Injection timing suspicious (>5 min after process start)");
        AuditReporter::ReportLoaderValidationFailed(result.failedValidator, result.details);
        return result;
    }

    
    if (!ValidateNoDebugger()) {
        
        AuditReporter::ReportSuspiciousActivity(OBFUSCATE_STRING("DebuggerValidator"),
                                                OBFUSCATE_STRING("Debugger detected"));
        
    }

    
    if (!ValidateNoSuspiciousModules()) {
        result.passed = false;
        result.failedValidator = OBFUSCATE_STRING("ModuleValidator");
        result.details = OBFUSCATE_STRING("Suspicious modules detected (injection tools)");
        AuditReporter::ReportLoaderValidationFailed(result.failedValidator, result.details);
        return result;
    }

    return result;
}



inline bool RunStartupValidation() {
    
    SystemInfo::Initialize();

    auto result = ValidateAll();

    if (!result.passed) {
        
        return false;
    }

    return true;
}

}  
