#include "test3.h"
#include <TlHelp32.h>
#include <fstream>
#include <iostream>
#include <vector>
#include "../test6/test30.hpp"






void __stdcall libraryLoader(MANUAL_MAPPING_DATA* pData) {
    if (!pData) {
        return;
    }


    BYTE* pBase = reinterpret_cast<BYTE*>(pData->hMod);



    auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);

    auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + pDos->e_lfanew);

    auto* pOpt = &pNt->OptionalHeader;






    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {

        auto delta = reinterpret_cast<ULONG_PTR>(pBase) - pOpt->ImageBase;


        if (delta != 0) {

            auto* pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);


            while (pReloc->VirtualAddress) {

                auto* pRelocInfo = reinterpret_cast<WORD*>(pReloc + 1);

                int numEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;

                for (int i = 0; i < numEntries; i++) {


                    int type = pRelocInfo[i] >> 12;

                    int offset = pRelocInfo[i] & 0xFFF;


                    if (type == IMAGE_REL_BASED_DIR64) {

                        *reinterpret_cast<ULONG_PTR*>(pBase + pReloc->VirtualAddress + offset) += delta;
                    } else if (type == IMAGE_REL_BASED_HIGHLOW) {

                        *reinterpret_cast<DWORD*>(pBase + pReloc->VirtualAddress + offset) += (DWORD)delta;
                    }
                }

                pReloc =
                    reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pReloc) + pReloc->SizeOfBlock);
            }
        }
    }






    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {

        auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        while (pImportDescr->Name) {

            char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
            HINSTANCE hDll = pData->pLoadLibraryA(szMod);

            if (!hDll) {


                ++pImportDescr;
                continue;
            }


            ULONG_PTR* pThunkRef =
                reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
            ULONG_PTR* pFuncRef =
                reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);


            if (!pThunkRef)
                pThunkRef = pFuncRef;

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {

                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {

                    *pFuncRef = pData->pGetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF));
                } else {

                    auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
                    *pFuncRef = pData->pGetProcAddress(hDll, pImport->Name);
                }
            }
            ++pImportDescr;
        }
    }





    if (pOpt->AddressOfEntryPoint) {
        auto EntryPoint = reinterpret_cast<f_DllMain>(pBase + pOpt->AddressOfEntryPoint);


        EntryPoint((HINSTANCE)pBase, DLL_PROCESS_ATTACH, pData);
    }
}


void __stdcall libraryLoaderEnd() {}




bool manualMap(DWORD pid, const std::wstring& dllPath) {

    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (file.fail())
        return false;

    size_t fileSize = file.tellg();
    if (fileSize < 0x1000)
        return false;

    BYTE* pSrcData = new BYTE[fileSize];
    if (!pSrcData) {
        file.close();
        return false;
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(pSrcData), fileSize);
    file.close();


    bool result = manualMap(pid, pSrcData, fileSize);

    delete[] pSrcData;
    return result;
}





bool manualMap(DWORD pid, BYTE* pSrcData, size_t dataSize) {
    if (!pSrcData || dataSize < 0x1000)
        return false;


    auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData);
    if (pDosHeader->e_magic != 0x5A4D) {
        return false;
    }

    auto* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != 0x4550) {
        return false;
    }


    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        return false;
    }


    BYTE* pTargetBase =
        reinterpret_cast<BYTE*>(VirtualAllocEx(hProc,
                                               nullptr,
                                               pNtHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE
                                               ));

    if (!pTargetBase) {
        CloseHandle(hProc);
        return false;
    }


    if (!WriteProcessMemory(hProc, pTargetBase, pSrcData, pNtHeaders->OptionalHeader.SizeOfHeaders, nullptr)) {
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }


    auto* pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++) {
        if (pSectionHeader[i].SizeOfRawData) {

            if (!WriteProcessMemory(hProc, pTargetBase + pSectionHeader[i].VirtualAddress,
                                    pSrcData + pSectionHeader[i].PointerToRawData, pSectionHeader[i].SizeOfRawData,
                                    nullptr)) {
                VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
                CloseHandle(hProc);
                return false;
            }
        }
    }


    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++) {
        DWORD protection = 0;
        DWORD oldProtection = 0;
        DWORD size = pSectionHeader[i].Misc.VirtualSize;
        if (size == 0)
            size = pSectionHeader[i].SizeOfRawData;

        if (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE) {
            continue;
        }

        bool executable = (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_EXECUTE);
        bool readable = (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_READ);
        bool writeable = (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_WRITE);

        if (!executable && !readable && !writeable)
            protection = PAGE_NOACCESS;
        else if (!executable && !readable && writeable)
            protection = PAGE_WRITECOPY;
        else if (!executable && readable && !writeable)
            protection = PAGE_READONLY;
        else if (!executable && readable && writeable)
            protection = PAGE_READWRITE;
        else if (executable && !readable && !writeable)
            protection = PAGE_EXECUTE;
        else if (executable && !readable && writeable)
            protection = PAGE_EXECUTE_WRITECOPY;
        else if (executable && readable && !writeable)
            protection = PAGE_EXECUTE_READ;
        else if (executable && readable && writeable)
            protection = PAGE_EXECUTE_READWRITE;

        if (pSectionHeader[i].Characteristics & IMAGE_SCN_MEM_NOT_CACHED)
            protection |= PAGE_NOCACHE;

        VirtualProtectEx(hProc, pTargetBase + pSectionHeader[i].VirtualAddress, size, protection, &oldProtection);
    }


    MANUAL_MAPPING_DATA data{0};
    data.pLoadLibraryA = reinterpret_cast<f_LoadLibraryA>(LoadLibraryA);
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress);
    data.hMod = (HINSTANCE)pTargetBase;


    size_t shellcodeSize = (DWORD64)libraryLoaderEnd - (DWORD64)libraryLoader;
    
    if ((DWORD64)libraryLoaderEnd <= (DWORD64)libraryLoader || shellcodeSize > 0x20000) {
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }
    size_t totalAllocSize = shellcodeSize + sizeof(MANUAL_MAPPING_DATA);

    BYTE* pShellcodeMem = reinterpret_cast<BYTE*>(
        VirtualAllocEx(hProc, nullptr, totalAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

    if (!pShellcodeMem) {
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }


    WriteProcessMemory(hProc, pShellcodeMem + shellcodeSize, &data, sizeof(MANUAL_MAPPING_DATA), nullptr);
    WriteProcessMemory(hProc, pShellcodeMem, libraryLoader, shellcodeSize, nullptr);


    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellcodeMem),
                                        pShellcodeMem + shellcodeSize,
                                        0, nullptr);

    if (!hThread) {
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, pShellcodeMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }


    WaitForSingleObject(hThread, 10000);


    CloseHandle(hThread);
    VirtualFreeEx(hProc, pShellcodeMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    return true;
}





bool injectDllLoadLibrary(DWORD pid, const std::wstring& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess)
        return false;


    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    void* pRemotePath = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) {
        CloseHandle(hProcess);
        return false;
    }


    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }


    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }


    HANDLE hThread =
        CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pRemotePath, 0, nullptr);

    if (!hThread) {
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }


    WaitForSingleObject(hThread, INFINITE);


    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

HMODULE getRemoteModuleHandle(DWORD pid, const std::wstring& moduleName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return nullptr;

    MODULEENTRY32W entry;
    entry.dwSize = sizeof(entry);

    HMODULE result = nullptr;

    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0) {
                result = entry.hModule;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

bool isDllInjected(DWORD pid, const std::wstring& dllName) {


    std::wstring mutexName = std::wstring(OBFUSCATE_WSTRING(L"Global\\Cobble_Instance_")) + std::to_wstring(pid);

    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, mutexName.c_str());
    if (hMutex) {
        CloseHandle(hMutex);
        return true;
    }

    return false;
}

bool ejectDll(DWORD pid, const std::wstring& dllName) {
    HMODULE remoteModule = getRemoteModuleHandle(pid, dllName);
    if (!remoteModule)
        return false;

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, pid);

    if (!process)
        return false;

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE freeLibrary =
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "FreeLibrary"));

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, freeLibrary, remoteModule, 0, nullptr);

    if (!thread) {
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, 5000);

    CloseHandle(thread);
    CloseHandle(process);

    return true;
}
