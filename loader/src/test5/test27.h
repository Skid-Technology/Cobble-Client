#pragma once

#include <shellapi.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include "../test6/test30.hpp"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace Updater {


inline std::string sha256File(const std::string& path) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return "";

    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;

    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        CloseHandle(file);
        return "";
    }

    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        CloseHandle(file);
        return "";
    }

    BYTE buffer[65536];
    DWORD bytesRead;
    while (ReadFile(file, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        CryptHashData(hash, buffer, bytesRead, 0);
    }

    BYTE hashValue[32];
    DWORD hashLen = 32;
    CryptGetHashParam(hash, HP_HASHVAL, hashValue, &hashLen, 0);

    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    CloseHandle(file);


    static const char* hex = "0123456789abcdef";
    std::string result;
    for (DWORD i = 0; i < hashLen; i++) {
        result += hex[hashValue[i] >> 4];
        result += hex[hashValue[i] & 0xf];
    }
    return result;
}


inline std::string getTempDownloadDir() {
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);


    GUID guid;
    CoCreateGuid(&guid);
    char guidStr[64];
    sprintf_s(guidStr, "%08x%04x%04x", guid.Data1, guid.Data2, guid.Data3);

    std::string dir = std::string(temp) + "cb_" + guidStr;
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}


using ProgressCallback = void (*)(size_t downloaded, size_t total);



inline bool downloadAndVerify(const std::string& url, const std::string& destPath, const std::string& expectedHash,
                              size_t expectedSize, const std::string& hwid, ProgressCallback progress = nullptr) {

    std::string tmpPath = destPath + ".tmp";


    bool isHttps = (url.find("https://") == 0);
    std::string host, path;
    size_t start = isHttps ? 8 : 7;
    size_t slashPos = url.find('/', start);
    if (slashPos == std::string::npos) {
        host = url.substr(start);
        path = "/";
    } else {
        host = url.substr(start, slashPos - start);
        path = url.substr(slashPos);
    }


    path += "?hwid=" + hwid;


    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());


    HINTERNET session = WinHttpOpen(OBFUSCATE_WSTRING(L"Mozilla/5.0").c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session)
        return false;


    DWORD timeout = 10000;
    WinHttpSetOption(session, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));


    INTERNET_PORT port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET connect = WinHttpConnect(session, wHost.c_str(), port, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }


    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", wPath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }


    if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }


    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode,
                        &statusSize, nullptr);
    if (statusCode != 200) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }


    HANDLE file =
        CreateFileA(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }


    size_t totalDownloaded = 0;
    BYTE buffer[8192];
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;

    while (true) {

        if (!WinHttpQueryDataAvailable(request, &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) {
            break;
        }


        DWORD toRead = (bytesAvailable > sizeof(buffer)) ? sizeof(buffer) : bytesAvailable;
        if (!WinHttpReadData(request, buffer, toRead, &bytesRead) || bytesRead == 0) {
            break;
        }


        DWORD bytesWritten;
        WriteFile(file, buffer, bytesRead, &bytesWritten, nullptr);
        totalDownloaded += bytesRead;


        if (progress) {
            progress(totalDownloaded, expectedSize);
        }
    }

    FlushFileBuffers(file);
    CloseHandle(file);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);


    if (totalDownloaded != expectedSize) {
        DeleteFileA(tmpPath.c_str());
        return false;
    }


    std::string actualHash = sha256File(tmpPath);
    if (actualHash != expectedHash) {
        DeleteFileA(tmpPath.c_str());
        return false;
    }


    if (!MoveFileExA(tmpPath.c_str(), destPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmpPath.c_str());
        return false;
    }

    return true;
}


inline std::string getSelfPath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return path;
}



inline bool spawnStub(const std::string& currentPath, const std::string& newPath) {

    char cmdLine[2048];
    sprintf_s(cmdLine, "\"%s\" --update-pid %lu --update-old \"%s\" --update-new \"%s\"", newPath.c_str(),
              GetCurrentProcessId(), currentPath.c_str(), newPath.c_str());

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;


    if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }



    SHELLEXECUTEINFOA sei = {sizeof(sei)};
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = "open";
    sei.lpFile = newPath.c_str();
    
    char params[2048];
    sprintf_s(params, "--update-pid %lu --update-old \"%s\" --update-new \"%s\"",
              GetCurrentProcessId(), currentPath.c_str(), newPath.c_str());
    sei.lpParameters = params;
    sei.nShow = SW_HIDE;

    if (ShellExecuteExA(&sei)) {
        if (sei.hProcess) {
            CloseHandle(sei.hProcess);
        }
        return true;
    }

    return false;
}


inline void cleanupTempDir(const std::string& dir) {

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                DeleteFileA((dir + "\\" + fd.cFileName).c_str());
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    RemoveDirectoryA(dir.c_str());
}

}
