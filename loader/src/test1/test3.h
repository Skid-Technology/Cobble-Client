#pragma once

#include <Windows.h>
#include <string>
#include <vector>

using f_LoadLibraryA = HINSTANCE(WINAPI*)(const char* lpLibFilename);
using f_GetProcAddress = UINT_PTR(WINAPI*)(HINSTANCE hModule, const char* lpProcName);
using f_DllMain = BOOL(WINAPI*)(void* hDll, DWORD dwReason, void* pReserved);

struct MANUAL_MAPPING_DATA {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    HINSTANCE hMod;
};


bool manualMap(DWORD pid, const std::wstring& dllPath);


bool manualMap(DWORD pid, BYTE* pData, size_t dataSize);


bool injectDllLoadLibrary(DWORD pid, const std::wstring& dllPath);


bool isDllInjected(DWORD pid, const std::wstring& dllName);


bool ejectDll(DWORD pid, const std::wstring& dllName);
