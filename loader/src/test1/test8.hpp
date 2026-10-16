#pragma once

#include <Windows.h>
#include <cstdint>






namespace SecureMapper {


struct MAPPER_CONTEXT {
    uint64_t moduleBase;
    uint64_t ntdllBase;
    uint64_t kernel32Base;




    void* pRtlAddFunctionTable;
    void* pLdrGetProcedureAddress;
    void* pRtlInitAnsiString;
    void* pRtlInitUnicodeString;


    volatile uint64_t completionFlag;
    volatile uint32_t lastError;
};


bool SecureManualMap(DWORD pid, BYTE* pData, size_t dataSize);


bool IsAlreadyInjected(DWORD pid);

}
