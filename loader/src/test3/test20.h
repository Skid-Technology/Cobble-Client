#pragma once
#include <Windows.h>
#include <winternl.h>
#include <string>
#include <vector>


typedef NTSTATUS(NTAPI* NtQueryInformationProcessTypedef)(HANDLE ProcessHandle,
                                                          PROCESSINFOCLASS ProcessInformationClass,
                                                          PVOID ProcessInformation, ULONG ProcessInformationLength,
                                                          PULONG ReturnLength);


#pragma pack(push, 1)
struct DbgUiRemoteBreakinPatch {
    WORD push_0;
    BYTE push;
    DWORD CurrentPorcessHandle;
    BYTE mov_eax;
    DWORD TerminateProcess;
    WORD call_eax;
};
#pragma pack(pop)


struct SECTIONINFO {
    LPVOID lpVirtualAddress;
    DWORD dwSizeOfRawData;
};

struct HASHSET {
    DWORD64 dwRealHash;
    SECTIONINFO SectionInfo;
};

namespace Security {





struct Detection {
    std::string code;
    int score;
    std::string details;
};


inline const char* ScoreToSeverity(int score) {
    if (score >= 100)
        return "critical";
    if (score >= 40)
        return "warn";
    return "info";
}


enum class SecurityDecision {
    ALLOW,
    WARN,
    BLOCK
};


std::vector<Detection> RunSecurityChecks();


int CalculateRiskScore(const std::vector<Detection>& detections);


SecurityDecision Decide(int score);




std::vector<Detection> CheckRunningTools();


std::vector<Detection> CheckVMIndicators();




void PatchDbgBreakPoint();


void PatchDbgUiRemoteBreakin();


void HideThread(HANDLE hThread);




bool HasVMRegistry();


bool HasVMFiles();


bool HasVMWareACPI();


bool IsOnVM();




bool HasAPIHooks();




DWORD64 HashSection(LPVOID lpAddress, DWORD dwSize);


std::vector<HASHSET> GetModuleSectionHashes();


bool VerifyIntegrity(const std::vector<HASHSET>& originalHashes);




void Initialize();



SecurityDecision RunSecurityChecksWithDecision(std::vector<Detection>& outDetections);

}
