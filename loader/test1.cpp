#include <aclapi.h>
#include <objbase.h>
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#include "test11.h"
#include "test3.h"
#include "test13.h"
#include "test6.h"
#include "test43.h"
#include "test20.h"
#include "src/test1/test4.hpp"
#include "src/test1/test8.hpp"
#include "src/test4/test26.hpp"
#include "src/test5/test27.h"
#include "src/test5/test29.h"
#include "src/test6/test32.hpp"
#include "src/test6/test33.h"
#include "src/test6/test34.h"
#include "test22.h"


#include "generated/server_config.h"
#include "generated/keys.h"
#include "generated/version.h"


#include "src/test2/test14.hpp"
#include "src/test3/test17.hpp"
#include "src/test3/test18.hpp"
#include "src/test3/test21.h"
#include "src/test6/test30.hpp"
#include "src/test6/test31.hpp"



struct InjectionData {
    char signature[16];
    char secret[65];
    char username[64];
    char hwid[65];
    uint32_t timestamp;
    uint32_t expiresIn;
    char handshakeSig[128];
    char sessionToken[128];
    char syncToken[65];
    char apiHost[64];

};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    DevConsole::Initialize();
    COBBLE_DEV_LOG("Loader startup, version=%s, server=%s", COBBLE_VERSION, cobble_config::GetServerHost().c_str());


    {
        Telemetry::Init(cobble_config::GetServerHost());
        auto toolDetections = Security::CheckRunningTools();
        for (auto& d : toolDetections) {
            Telemetry::Severity sev = (d.score >= 40) ? Telemetry::Severity::WARN : Telemetry::Severity::INFO;
            Telemetry::ReportImmediate(d.code, sev, d.score, d.details);
        }
    }






    {
        int protectionCheck = 0;
        CHECK_PROTECTION(protectionCheck, 1);

        if (protectionCheck == 1) {

            int integrityCheck = 0;
            CHECK_CODE_INTEGRITY(integrityCheck, 1);
            if (integrityCheck != 1) {


                Telemetry::ReportImmediate("VMPROTECT_CRC_MISMATCH", Telemetry::Severity::WARN, 30,
                                           "CHECK_CODE_INTEGRITY");
            }


            int debuggerCheck = 0;
            CHECK_DEBUGGER(debuggerCheck, 0);
            if (debuggerCheck != 0) {
                Telemetry::ReportImmediate("VMPROTECT_DEBUGGER", Telemetry::Severity::CRITICAL, 100, "CHECK_DEBUGGER");
                return 1;
            }
        }
    }


    Security::Initialize();


    {
        std::vector<Security::Detection> detections;
        Security::SecurityDecision decision = Security::RunSecurityChecksWithDecision(detections);

        if (decision == Security::SecurityDecision::BLOCK) {
            return 1;
        }

        if (decision == Security::SecurityDecision::WARN) {
            int userChoice = MessageBoxA(
                nullptr,
                OBFUSCATE_STRING(
                    "Analysis tools detected.\n\nPlease close any debugging or analysis software and try again.")
                    .c_str(),
                OBFUSCATE_STRING("Cobble Loader").c_str(), MB_RETRYCANCEL | MB_ICONWARNING);

            if (userChoice == IDRETRY) {
                detections.clear();
                decision = Security::RunSecurityChecksWithDecision(detections);
                if (decision != Security::SecurityDecision::ALLOW) {
                    return 1;
                }
            } else {
                return 0;
            }
        }
    }


    if (!LoaderUI::Init()) {
        MessageBoxA(nullptr, OBFUSCATE_STRING("Failed to initialize UI").c_str(),
                    OBFUSCATE_STRING("Cobble Loader").c_str(), MB_ICONERROR);
        return 1;
    }

    LoaderUI::SetState(LoaderUI::State::INITIALIZING);
    LoaderUI::SetMessage(OBFUSCATE_STRING("Cobble"));

    auto renderFrame = []() -> bool {
        return LoaderUI::Render();
    };

    LoaderUI::Show();
    if (!renderFrame()) {
        LoaderUI::Shutdown();
        return 0;
    }









#if !defined(COBBLE_DEV_BUILD)
    LoaderUI::SetMessage(OBFUSCATE_STRING("Update"));
    renderFrame();

    COBBLE_DEV_LOG("Checking for loader updates");

    Updater::CheckResult updateResult;
    std::atomic<bool> updateCheckDone{false};
    std::thread updateCheckWorker([&]() {
        updateResult.success = false;
        for (const auto& host : cobble_config::GetServerHosts()) {
            updateResult = Updater::Check(COBBLE_VERSION, host, cobble_config::SERVER_PORT);
            if (updateResult.success) {
                cobble_config::PreferServerHost(host);
                break;
            }
        }
        updateCheckDone.store(true, std::memory_order_release);
    });
    while (!updateCheckDone.load(std::memory_order_acquire)) {
        if (!renderFrame()) {
            updateCheckWorker.join();
            LoaderUI::Shutdown();
            return 0;
        }
    }
    updateCheckWorker.join();
    COBBLE_DEV_LOG("Update check complete: success=%s, available=%s, mandatory=%s",
                   updateResult.success ? "true" : "false",
                   updateResult.updateAvailable ? "true" : "false",
                   updateResult.mandatory ? "true" : "false");

    if (updateResult.success && updateResult.mandatory) {

        LoaderUI::SetMessage(OBFUSCATE_STRING("Update"));
        renderFrame();


        std::string hwid = HWID::Generate();


        std::string tempDir = Updater::getTempDownloadDir();
        std::string newLoaderPath = tempDir + "\\CobbleLoader.exe";


        std::string downloadUrl = "https://" + cobble_config::GetServerHost() + OBFUSCATE_STRING("/api/update/loader");

        bool downloadSuccess = false;
        std::atomic<bool> downloadDone{false};
        std::thread downloadWorker([&]() {
            downloadSuccess = Updater::downloadAndVerify(downloadUrl, newLoaderPath, updateResult.manifest.loader.sha256,
                                                         updateResult.manifest.loader.size, hwid, nullptr);
            downloadDone.store(true, std::memory_order_release);
        });
        while (!downloadDone.load(std::memory_order_acquire)) {
            if (!renderFrame()) {
                downloadWorker.join();
                LoaderUI::Shutdown();
                return 0;
            }
        }
        downloadWorker.join();

        if (downloadSuccess) {

            std::string selfPath = Updater::getSelfPath();
            if (Updater::spawnStub(selfPath, newLoaderPath)) {

                LoaderUI::Shutdown();
                return 0;
            }
        }


        Updater::cleanupTempDir(tempDir);
        LoaderUI::SetError(OBFUSCATE_STRING("Update failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }


    if (updateResult.success && updateResult.updateAvailable && !updateResult.mandatory) {
        COBBLE_DEV_LOG("Optional update available: version=%s", updateResult.manifest.version.c_str());
    }
#else
    COBBLE_DEV_LOG("Development build: automatic loader update skipped");
#endif


    HMODULE hModule = GetModuleHandle(nullptr);
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(IDR_COBBLE_DLL), RT_RCDATA);
    if (!hRes) {
        LoaderUI::SetError(OBFUSCATE_STRING("Injection failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }

    HGLOBAL hData = LoadResource(hModule, hRes);
    BYTE* encryptedData = (BYTE*)LockResource(hData);
    DWORD encryptedSize = SizeofResource(hModule, hRes);


    std::vector<uint8_t> decryptedDll;
    BYTE* dllData = nullptr;
    size_t dllSize = 0;


    ProcessInfo lunar = {};

    bool licenseValid = false;

    COBBLE_DEV_LOG("Starting startup license verification");
    bool startupLicenseValid = false;
    bool needsLogin = false;
    LoaderUI::SetMessage(OBFUSCATE_STRING("Session"));
    std::atomic<bool> licenseCheckDone{false};
    std::thread licenseCheckWorker([&]() {
        for (const auto& url : cobble_config::GetServerUrls()) {
            COBBLE_DEV_LOG("License verify attempt host=%s", url.c_str());
            if (License::Verify(url.c_str())) {
                cobble_config::PreferServerHost(url);
                startupLicenseValid = true;
                break;
            }
            if (License::RequiresLogin()) {
                if (!License::IsTransportError()) {
                    cobble_config::PreferServerHost(url);
                }
                needsLogin = true;
                break;
            }
            if (!License::IsTransportError()) {
                break;
            }
            COBBLE_DEV_LOG("Transport error on %s (%s), trying next host", url.c_str(),
                           License::GetLastError().c_str());
        }
        licenseCheckDone.store(true, std::memory_order_release);
    });
    while (!licenseCheckDone.load(std::memory_order_acquire)) {
        if (!renderFrame()) {
            licenseCheckWorker.join();
            LoaderUI::Shutdown();
            return 0;
        }
    }
    licenseCheckWorker.join();
    COBBLE_DEV_LOG("Startup verification complete: valid=%s, requires_login=%s, error=\"%s\", host=%s",
                   startupLicenseValid ? "true" : "false",
                   needsLogin || License::RequiresLogin() ? "true" : "false",
                   License::GetLastError().c_str(),
                   cobble_config::GetServerHost().c_str());

    if (startupLicenseValid) {
        licenseValid = true;
        LoaderUI::SetState(LoaderUI::State::LICENSE_VALID);
    } else if (needsLogin || License::RequiresLogin()) {

        LoaderUI::SetState(LoaderUI::State::LOGIN_REQUIRED);


        auto nextMcScan = std::chrono::steady_clock::now();

        while (renderFrame()) {

            auto now = std::chrono::steady_clock::now();
            if (now >= nextMcScan) {
                lunar = findLunarClient();
                nextMcScan = now + std::chrono::milliseconds(100);
            }

            if (LoaderUI::IsLoginButtonPressed()) {
                std::string username = LoaderUI::GetUsername();
                std::string password = LoaderUI::GetPassword();
                LoaderUI::ClearLoginButton();

                if (username.empty() || password.empty()) {
                    LoaderUI::SetError(OBFUSCATE_STRING("Enter username and password"));
                    continue;
                }

                LoaderUI::SetState(LoaderUI::State::AUTHENTICATING);
                LoaderUI::SetMessage(OBFUSCATE_STRING("Account"));
                renderFrame();

                COBBLE_DEV_LOG("Submitting login request (username_length=%zu)", username.size());
                bool loginValid = false;
                std::atomic<bool> loginDone{false};
                std::thread loginWorker([&]() {
                    for (const auto& url : cobble_config::GetServerUrls()) {
                        loginValid = License::LoginWithCredentials(url.c_str(), username, password);
                        if (loginValid) {
                            cobble_config::PreferServerHost(url);
                            break;
                        }
                        if (!License::IsTransportError()) {
                            break;
                        }
                    }
                    loginDone.store(true, std::memory_order_release);
                });
                bool loginWindowOpen = true;
                while (!loginDone.load(std::memory_order_acquire)) {
                    if (!renderFrame()) {
                        loginWindowOpen = false;
                        break;
                    }
                }
                loginWorker.join();
                if (!loginWindowOpen) {
                    LoaderUI::Shutdown();
                    return 0;
                }
                COBBLE_DEV_LOG("Login request complete: valid=%s, error=\"%s\", host=%s",
                               loginValid ? "true" : "false", License::GetLastError().c_str(),
                               cobble_config::GetServerHost().c_str());

                if (loginValid) {
                    licenseValid = true;

                    if (lunar.pid == 0) {
                        LoaderUI::SetState(LoaderUI::State::LICENSE_VALID);
                    }
                    break;
                } else {
                    LoaderUI::SetError(License::GetLastError());
                }
            }
        }

        if (!licenseValid) {
            LoaderUI::Shutdown();
            return 0;
        }
    }

    if (!licenseValid) {
        COBBLE_DEV_LOG("Displaying terminal license error: \"%s\"", License::GetLastError().c_str());
        LoaderUI::SetError(License::GetLastError());
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }


    if (lunar.pid == 0) {
        LoaderUI::SetMessage(OBFUSCATE_STRING("Minecraft"));

        auto nextScan = std::chrono::steady_clock::now();
        while (true) {
            if (!renderFrame()) {
                LoaderUI::Shutdown();
                return 0;
            }


            auto now = std::chrono::steady_clock::now();
            if (now >= nextScan) {
                lunar = findLunarClient();
                if (lunar.pid != 0)
                    break;
                nextScan = now + std::chrono::milliseconds(50);
            }
        }
    }


    if (isDllInjected(lunar.pid, L"Cobble.dll")) {
        LoaderUI::SetError(OBFUSCATE_STRING("Already injected"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 0;
    }





    LoaderUI::SetState(LoaderUI::State::INITIALIZING);
    LoaderUI::SetMessage(OBFUSCATE_STRING("Session"));
    renderFrame();

    std::string payloadKey;
    bool keyOk = false;
    std::atomic<bool> keyFetchDone{false};
    std::thread keyFetchWorker([&]() {
        for (const auto& url : cobble_config::GetServerUrls()) {
            if (License::FetchPayloadKey(url.c_str())) {
                cobble_config::PreferServerHost(url);
                keyOk = true;
                break;
            }
            if (!License::IsTransportError()) {
                break;
            }
        }
        keyFetchDone.store(true, std::memory_order_release);
    });
    while (!keyFetchDone.load(std::memory_order_acquire)) {
        if (!renderFrame()) {
            keyFetchWorker.join();
            LoaderUI::Shutdown();
            return 0;
        }
    }
    keyFetchWorker.join();
    if (!keyOk) {
        LoaderUI::SetError(OBFUSCATE_STRING("Server connection failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }
    payloadKey = License::GetPayloadKey();

    if (payloadKey.empty() || payloadKey.length() != 64) {
        LoaderUI::SetError(OBFUSCATE_STRING("Key fetch failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }


    CF_CRITICAL_BEGIN();
    if (CF_CHECK(false)) {
        CF_JUNK();
        Telemetry::Report("DBG_FINAL_CHECK", Telemetry::Severity::CRITICAL, 100,
                          OBFUSCATE_STRING("Pre-decrypt security check failed"));
        Sleep(500);
        SecureZeroMemory(&payloadKey[0], payloadKey.size());
        LoaderUI::Shutdown();
        return 1;
    }
    CF_CRITICAL_END();


    decryptedDll = PayloadCrypto::DecryptPayloadWithKey(encryptedData, encryptedSize, payloadKey);


    SecureZeroMemory(&payloadKey[0], payloadKey.size());

    if (decryptedDll.empty() || !PayloadCrypto::ValidatePE(decryptedDll)) {
        LoaderUI::SetError(OBFUSCATE_STRING("Injection failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }

    dllData = decryptedDll.data();
    dllSize = decryptedDll.size();


    LoaderUI::SetState(LoaderUI::State::INJECTING);
    LoaderUI::SetMessage(OBFUSCATE_STRING("Minecraft"));
    renderFrame();



    HANDLE hMapFile = nullptr;
    std::wstring sharedMemNameW;
    {


        COBBLE_VM_HIGH_START

        InjectionData injData = {};
        memcpy(injData.signature, "COBBLE_INJ_V1\0\0\0", 16);

        std::string secret = License::GetInjectionSecret();
        std::string username = License::GetUsername();
        std::string hwid = HWID::Generate();

        strncpy_s(injData.secret, secret.c_str(), 64);
        strncpy_s(injData.username, username.c_str(), 63);
        strncpy_s(injData.hwid, hwid.c_str(), 64);

        injData.timestamp = (uint32_t)License::GetHandshakeTimestamp();
        injData.expiresIn = 300;


        std::string handshakeSig = License::GetHandshakeSig();
        strncpy_s(injData.handshakeSig, handshakeSig.c_str(), 127);


        std::string sessionToken = License::GetSessionToken();
        strncpy_s(injData.sessionToken, sessionToken.c_str(), 127);


        std::string syncToken = License::GetSyncToken();
        strncpy_s(injData.syncToken, syncToken.c_str(), 64);


        std::string apiHost = cobble_config::GetServerHost();
        strncpy_s(injData.apiHost, apiHost.c_str(), 63);


        wchar_t pidStr[64];
        swprintf_s(pidStr, L"Local\\Cobble_%lu", lunar.pid);
        sharedMemNameW = pidStr;

        COBBLE_VM_HIGH_END



        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;


        PSECURITY_DESCRIPTOR pSD = nullptr;
        PSID pCurrentUserSid = nullptr;


        HANDLE hToken = nullptr;
        PTOKEN_USER pUserToken = nullptr;
        PACL pAcl = nullptr;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            DWORD dwSize = 0;
            GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize);
            if (dwSize > 0) {
                pUserToken = (PTOKEN_USER)LocalAlloc(LPTR, dwSize);
                if (pUserToken && GetTokenInformation(hToken, TokenUser, pUserToken, dwSize, &dwSize)) {
                    pCurrentUserSid = pUserToken->User.Sid;


                    EXPLICIT_ACCESS ea = {};
                    ea.grfAccessPermissions = FILE_MAP_ALL_ACCESS;
                    ea.grfAccessMode = SET_ACCESS;
                    ea.grfInheritance = NO_INHERITANCE;
                    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
                    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
                    ea.Trustee.ptstrName = (LPWSTR)pCurrentUserSid;

                    if (SetEntriesInAclW(1, &ea, nullptr, &pAcl) == ERROR_SUCCESS) {
                        pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
                        if (pSD) {
                            InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
                            SetSecurityDescriptorDacl(pSD, TRUE, pAcl, FALSE);
                            sa.lpSecurityDescriptor = pSD;
                        }
                    }
                }
            }
            CloseHandle(hToken);
        }


        if (!sa.lpSecurityDescriptor) {
            sa.lpSecurityDescriptor = nullptr;
        }


        hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(InjectionData),
                                      sharedMemNameW.c_str());

        if (hMapFile) {
            void* pBuf = MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, sizeof(InjectionData));
            if (pBuf) {
                memcpy(pBuf, &injData, sizeof(InjectionData));
                UnmapViewOfFile(pBuf);
            }
        }


        if (pAcl)
            LocalFree(pAcl);
        if (pUserToken)
            LocalFree(pUserToken);
        if (pSD)
            LocalFree(pSD);


        SecureZeroMemory(&injData, sizeof(injData));
    }




    struct InjectionWorkerContext {
        DWORD pid;
        BYTE* data;
        size_t size;
        bool succeeded;
    } workerContext{lunar.pid, dllData, dllSize, false};

    auto injectionEntry = [](LPVOID parameter) -> DWORD {
        auto* worker = static_cast<InjectionWorkerContext*>(parameter);
        try {
            worker->succeeded = SecureMapper::SecureManualMap(worker->pid, worker->data, worker->size);
        } catch (...) {
            worker->succeeded = false;
        }
        return 0;
    };

    HANDLE injectionWorker = CreateThread(nullptr, 0, injectionEntry, &workerContext, 0, nullptr);
    if (!injectionWorker) {
        if (hMapFile)
            CloseHandle(hMapFile);
        SecureZeroMemory(decryptedDll.data(), decryptedDll.size());
        LoaderUI::SetError(OBFUSCATE_STRING("Injection failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }

    bool loaderWindowOpen = true;
    while (WaitForSingleObject(injectionWorker, 0) == WAIT_TIMEOUT) {
        if (loaderWindowOpen)
            loaderWindowOpen = renderFrame();
        else
            Sleep(10);
    }

    WaitForSingleObject(injectionWorker, INFINITE);
    CloseHandle(injectionWorker);
    const bool injectionSucceeded = workerContext.succeeded;

    if (!loaderWindowOpen) {
        if (hMapFile)
            CloseHandle(hMapFile);
        SecureZeroMemory(decryptedDll.data(), decryptedDll.size());
        LoaderUI::Shutdown();
        return 0;
    }

    if (!injectionSucceeded) {
        if (hMapFile)
            CloseHandle(hMapFile);
        SecureZeroMemory(decryptedDll.data(), decryptedDll.size());
        LoaderUI::SetError(OBFUSCATE_STRING("Injection failed"));
        while (renderFrame())
            ;
        LoaderUI::Shutdown();
        return 1;
    }


    SecureZeroMemory(decryptedDll.data(), decryptedDll.size());

    LoaderUI::SetState(LoaderUI::State::INJECTING_SUCCESS);


    while (renderFrame())
        ;


    if (hMapFile)
        CloseHandle(hMapFile);

    LoaderUI::Shutdown();
    return 0;
}
