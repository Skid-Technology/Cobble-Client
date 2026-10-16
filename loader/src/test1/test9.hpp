#pragma once

#include <Windows.h>
#include <cstdint>






namespace nt {


constexpr int32_t STATUS_SUCCESS = 0x00000000;
constexpr int32_t STATUS_IMAGE_NOT_AT_BASE = 0x40000003;


constexpr uint32_t NT_SEC_COMMIT = 0x8000000;
constexpr uint32_t NT_SEC_IMAGE = 0x1000000;


constexpr uint32_t NT_THREAD_ALL_ACCESS = 0x1FFFFF;


constexpr uint32_t NT_SECTION_ALL_ACCESS = 0xF001F;


struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    wchar_t* Buffer;
};

struct OBJECT_ATTRIBUTES {
    uint32_t Length;
    void* RootDirectory;
    UNICODE_STRING* ObjectName;
    uint32_t Attributes;
    void* SecurityDescriptor;
    void* SecurityQualityOfService;
};

struct CLIENT_ID {
    void* UniqueProcess;
    void* UniqueThread;
};

struct PS_ATTRIBUTE {
    uint64_t Attribute;
    size_t Size;
    union {
        uint64_t Value;
        void* ValuePtr;
    };
    size_t* ReturnLength;
};

struct PS_ATTRIBUTE_LIST {
    size_t TotalLength;
    PS_ATTRIBUTE Attributes[1];
};


using NtCreateSection_t = int32_t(__stdcall*)(void** SectionHandle, uint32_t DesiredAccess,
                                              OBJECT_ATTRIBUTES* ObjectAttributes, LARGE_INTEGER* MaximumSize,
                                              uint32_t SectionPageProtection, uint32_t AllocationAttributes,
                                              void* FileHandle);

using NtMapViewOfSection_t = int32_t(__stdcall*)(void* SectionHandle, void* ProcessHandle, void** BaseAddress,
                                                 size_t ZeroBits, size_t CommitSize, LARGE_INTEGER* SectionOffset,
                                                 size_t* ViewSize, uint32_t InheritDisposition, uint32_t AllocationType,
                                                 uint32_t Win32Protect);

using NtUnmapViewOfSection_t = int32_t(__stdcall*)(void* ProcessHandle, void* BaseAddress);

using NtClose_t = int32_t(__stdcall*)(void* Handle);

using NtCreateThreadEx_t = int32_t(__stdcall*)(void** ThreadHandle, uint32_t DesiredAccess,
                                               OBJECT_ATTRIBUTES* ObjectAttributes, void* ProcessHandle,
                                               void* StartRoutine, void* Argument, uint32_t CreateFlags,
                                               size_t ZeroBits, size_t StackSize, size_t MaximumStackSize,
                                               PS_ATTRIBUTE_LIST* AttributeList);

using NtWaitForSingleObject_t = int32_t(__stdcall*)(void* Handle, uint8_t Alertable, LARGE_INTEGER* Timeout);


using NtSuspendThread_t          = int32_t(__stdcall*)(void* ThreadHandle, uint32_t* PreviousSuspendCount);
using NtResumeThread_t           = int32_t(__stdcall*)(void* ThreadHandle, uint32_t* PreviousSuspendCount);
using NtGetContextThread_t       = int32_t(__stdcall*)(void* ThreadHandle, CONTEXT* ThreadContext);
using NtSetContextThread_t       = int32_t(__stdcall*)(void* ThreadHandle, CONTEXT* ThreadContext);
using NtOpenThread_t             = int32_t(__stdcall*)(void** ThreadHandle, uint32_t DesiredAccess,
                                                       OBJECT_ATTRIBUTES* ObjectAttributes, CLIENT_ID* ClientId);
using NtQueryInformationThread_t = int32_t(__stdcall*)(void* ThreadHandle, uint32_t ThreadInformationClass,
                                                       void* ThreadInformation, uint32_t ThreadInformationLength,
                                                       uint32_t* ReturnLength);

constexpr uint32_t THREAD_HIJACK_ACCESS = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION;

using RtlInitUnicodeString_t = void(__stdcall*)(UNICODE_STRING* DestinationString, const wchar_t* SourceString);


class Syscalls {
private:
    HMODULE m_ntdll = nullptr;
    bool m_initialized = false;

public:
    NtCreateSection_t NtCreateSection = nullptr;
    NtMapViewOfSection_t NtMapViewOfSection = nullptr;
    NtUnmapViewOfSection_t NtUnmapViewOfSection = nullptr;
    NtClose_t NtClose = nullptr;
    NtCreateThreadEx_t NtCreateThreadEx = nullptr;
    NtWaitForSingleObject_t NtWaitForSingleObject = nullptr;
    RtlInitUnicodeString_t RtlInitUnicodeString = nullptr;

    NtSuspendThread_t          NtSuspendThread = nullptr;
    NtResumeThread_t           NtResumeThread = nullptr;
    NtGetContextThread_t       NtGetContextThread = nullptr;
    NtSetContextThread_t       NtSetContextThread = nullptr;
    NtOpenThread_t             NtOpenThread = nullptr;
    NtQueryInformationThread_t NtQueryInformationThread = nullptr;

    bool Initialize() {
        if (m_initialized)
            return true;

        m_ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!m_ntdll)
            return false;

        NtCreateSection = (NtCreateSection_t)GetProcAddress(m_ntdll, "NtCreateSection");
        NtMapViewOfSection = (NtMapViewOfSection_t)GetProcAddress(m_ntdll, "NtMapViewOfSection");
        NtUnmapViewOfSection = (NtUnmapViewOfSection_t)GetProcAddress(m_ntdll, "NtUnmapViewOfSection");
        NtClose = (NtClose_t)GetProcAddress(m_ntdll, "NtClose");
        NtCreateThreadEx = (NtCreateThreadEx_t)GetProcAddress(m_ntdll, "NtCreateThreadEx");
        NtWaitForSingleObject = (NtWaitForSingleObject_t)GetProcAddress(m_ntdll, "NtWaitForSingleObject");
        RtlInitUnicodeString = (RtlInitUnicodeString_t)GetProcAddress(m_ntdll, "RtlInitUnicodeString");

        NtSuspendThread          = (NtSuspendThread_t)GetProcAddress(m_ntdll, "NtSuspendThread");
        NtResumeThread           = (NtResumeThread_t)GetProcAddress(m_ntdll, "NtResumeThread");
        NtGetContextThread       = (NtGetContextThread_t)GetProcAddress(m_ntdll, "NtGetContextThread");
        NtSetContextThread       = (NtSetContextThread_t)GetProcAddress(m_ntdll, "NtSetContextThread");
        NtOpenThread             = (NtOpenThread_t)GetProcAddress(m_ntdll, "NtOpenThread");
        NtQueryInformationThread = (NtQueryInformationThread_t)GetProcAddress(m_ntdll, "NtQueryInformationThread");

        m_initialized = NtCreateSection && NtMapViewOfSection && NtUnmapViewOfSection && NtClose && NtCreateThreadEx &&
                        NtWaitForSingleObject;

        return m_initialized;
    }

    bool IsInitialized() const { return m_initialized; }
};


inline Syscalls g_Syscalls;

}
