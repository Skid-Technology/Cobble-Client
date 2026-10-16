#pragma once

#include <ShlObj.h>
#include <Windows.h>
#include <wincrypt.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "../util/hwid.hpp"
#include "../util/inject_trace.hpp"
#include "../util/logger.hpp"
#include "../util/aes_string.hpp"
#include "../util/network/http.hpp"
#include "../util/network/json.hpp"
#include "../security/tweetnacl.h"
#include "generated/keys.h"
#include "generated/server_config.h"

#pragma comment(lib, "crypt32.lib")


#define CONFIG_SERVER_PORT 443
#define CONFIG_DEBOUNCE_MS 400


struct Module;
class ModuleManager;
extern ModuleManager* moduleManager;





inline bool sehParse(const std::string& content, json::Value& out) {
    bool ok = false;
    __try {
        ok = json::Parser::parse(content, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    return ok;
}







inline void sehResetValue(json::Value& target) {
    new (&target) json::Value();
}

class ConfigManager {
public:
    static ConfigManager& get() {
        static ConfigManager instance;
        return instance;
    }

    
    void init() {
        COBBLE_TRACE("ConfigMgr:init enter");
        
        COBBLE_TRACE("ConfigMgr:before hwid::generate");
        hwid_ = hwid::generate();
        COBBLE_TRACE("ConfigMgr:after hwid::generate");
        if (hwid_.empty()) {
            log_warn("ConfigManager: Failed to generate HWID, sync disabled");
            syncEnabled_ = false;
        }

        
        COBBLE_TRACE("ConfigMgr:before getConfigPath");
        configPath_ = getConfigPath();
        COBBLE_TRACE("ConfigMgr:after getConfigPath");

        
        COBBLE_TRACE("ConfigMgr:before loadSessionToken");
        loadSessionToken();
        COBBLE_TRACE("ConfigMgr:after loadSessionToken");

        
        
        
        buildConfigFromModules();
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            defaultConfig_ = config_;
            hasDefaultConfig_ = true;
        }

        
        COBBLE_TRACE("ConfigMgr:before loadLocal");
        loadLocal();
        COBBLE_TRACE("ConfigMgr:after loadLocal");

        

        
        if (syncEnabled_ && !sessionToken_.empty()) {
            COBBLE_TRACE("ConfigMgr:spawning fetchServerConfig thread");
            std::thread([this] { fetchServerConfig(); }).detach();
        }

        log_ok("ConfigManager initialized");
        COBBLE_TRACE("ConfigMgr:init exit");
    }

    
    void shutdown() {
        log_raw("[CFG] shutdown() -> saveLocalSync()");
        syncRunning_ = false;
        saveLocalSync();
        log_raw("[CFG] shutdown complete");
    }

    
    void onSettingChanged() {
        log_raw("[CFG] onSettingChanged() -> saveLocal + debounced sync");
        saveLocal();
        enqueueSyncDebounced();
    }

    
    void save() { saveLocal(); }

    
    std::string getUserConfigsDir() const;
    std::vector<std::string> listUserConfigs() const;
    bool saveUserConfig(const std::string& name, std::string* err = nullptr);
    bool loadUserConfig(const std::string& name, std::string* err = nullptr);
    bool deleteUserConfig(const std::string& name, std::string* err = nullptr);

    
    
    bool resetToDefaults(std::string* err = nullptr);

    
    
    
    
    
    
    
    
    
    
    
    
    
    std::string exportPublicConfig();
    bool importPublicConfig(const std::string& jsonStr, std::string* err = nullptr);

    
    
    
    
    
    struct PublicConfigEntry {
        std::string id;
        std::string owner;
        std::string name;
        std::string description;
        int downloads = 0;
        bool owned = false;
    };
    struct BrowseState {
        enum Status { Idle, Loading, Ready, Error };
        Status status = Idle;
        std::vector<PublicConfigEntry> entries;
        std::string message;  
    };

    
    BrowseState getBrowseState() const {
        std::lock_guard<std::mutex> lock(browseMutex_);
        return browseState_;
    }

    
    
    void publishPublicConfigAsync(const std::string& name, const std::string& description, bool anonymous);
    void refreshPublicConfigsAsync();
    void downloadAndApplyPublicConfigAsync(const std::string& id);
    void deletePublicConfigAsync(const std::string& id);

    
    std::string getConfigJson() const {
        std::lock_guard<std::mutex> lock(configMutex_);
        return config_.serialize();
    }

    
    
    
    
    std::string snapshotConfigJson() {
        buildConfigFromModules();
        std::lock_guard<std::mutex> lock(configMutex_);
        return config_.serialize();
    }

    
    bool isSyncEnabled() const { return syncEnabled_; }
    void setSyncEnabled(bool enabled) {
        syncEnabled_ = enabled;
        
        if (enabled && !hwid_.empty()) {
            std::thread([this] { fetchServerConfig(); }).detach();
        }
    }

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::string getConfigPath() {
        wchar_t* appData = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData) != S_OK) {
            return "";
        }

        std::wstring path(appData);
        CoTaskMemFree(appData);

        path += L"\\Cobble";
        CreateDirectoryW(path.c_str(), nullptr);  

        path += L"\\config.json";

        
        int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &result[0], size, nullptr, nullptr);

        return result;
    }

    void loadLocal() {
        COBBLE_TRACE("loadLocal:enter");
        if (configPath_.empty()) {
            COBBLE_TRACE("loadLocal:no path -> applyDefaults");
            log_warn("ConfigManager: No config path, using defaults");
            applyDefaults();
            COBBLE_TRACE("loadLocal:applyDefaults done (no path)");
            return;
        }

        COBBLE_TRACE("loadLocal:opening ifstream");
        std::ifstream file(configPath_);
        COBBLE_TRACE("loadLocal:after ifstream ctor");
        if (!file.is_open()) {
            COBBLE_TRACE("loadLocal:file not open -> applyDefaults");
            log_raw("ConfigManager: No local config found, using defaults");
            applyDefaults();
            COBBLE_TRACE("loadLocal:applyDefaults done (no file)");
            saveLocalSync();  
            COBBLE_TRACE("loadLocal:saveLocal done (no file)");
            return;
        }

        COBBLE_TRACE("loadLocal:reading file content");
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

#ifdef COBBLE_DEV_BUILD
        
        
        {
            char sizeMsg[64];
            wsprintfA(sizeMsg, "loadLocal:content size=%lu", (unsigned long)content.size());
            COBBLE_TRACE(sizeMsg);

            
            char head[160] = {0};
            size_t hn = content.size() < 120 ? content.size() : 120;
            for (size_t i = 0; i < hn; ++i) {
                char c = content[i];
                head[i] = (c >= 32 && c < 127) ? c : '?';
            }
            COBBLE_TRACE(head);

            
            if (content.size() > 120) {
                char tail[160] = {0};
                size_t off = content.size() - 120;
                for (size_t i = 0; i < 120; ++i) {
                    char c = content[off + i];
                    tail[i] = (c >= 32 && c < 127) ? c : '?';
                }
                COBBLE_TRACE(tail);
            }
        }
#endif

        COBBLE_TRACE("loadLocal:content read, parsing");

        
        
        
        
        
        bool parseOk = sehParse(content, config_);

        if (!parseOk) {
            COBBLE_TRACE("loadLocal:parse failed -> applyDefaults");
            
            
            
            
            
            
            size_t ep = json::Parser::s_errPos;
            size_t mp = json::Parser::s_maxPos;
            log_err("[CFG] PARSE FAIL: why='%s' errPos=%d maxPos=%d maxDepth=%d badToken='%s'",
                    json::Parser::s_errWhy, (int)ep, (int)mp, json::Parser::s_maxDepth,
                    json::Parser::s_errBadToken.c_str());
            
            
            size_t at = (ep > 0) ? ep : mp;
            if (at < content.size()) {
                size_t from = at > 40 ? at - 40 : 0;
                std::string ctx = content.substr(from, 80);
                log_err("[CFG] PARSE FAIL context @%d: ...%s...", (int)at, ctx.c_str());
            }
            log_warn("ConfigManager: Failed to parse config, using defaults");
            applyDefaults();
            COBBLE_TRACE("loadLocal:applyDefaults done (parse fail)");
            return;
        }
        COBBLE_TRACE("loadLocal:parse ok");

        
        int version = config_["version"].as_int(0);
        if (version < CONFIG_VERSION) {
            COBBLE_TRACE("loadLocal:migrating");
            log_raw("ConfigManager: Migrating config from v%d to v%d", version, CONFIG_VERSION);
            migrateConfig(version);
            COBBLE_TRACE("loadLocal:migrate done");
        }

        COBBLE_TRACE("loadLocal:before applyConfig");
        applyConfig();
        COBBLE_TRACE("loadLocal:after applyConfig");
        log_ok("ConfigManager: Loaded local config");
    }

    void saveLocalSync() {
        if (configPath_.empty()) {
            log_warn("[CFG] saveLocalSync: no configPath_, skipping");
            return;
        }

        
        buildConfigFromModules();

        std::string content;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            content = config_.serialize();
        }

        log_raw("[CFG] saveLocalSync: writing %d bytes to disk", (int)content.size());

        std::lock_guard<std::mutex> lock(fileWriteMutex_);
        std::ofstream file(configPath_);
        if (file.is_open()) {
            file << content;
            file.close();
            log_raw("[CFG] saveLocalSync: write OK");
        } else {
            log_err("[CFG] saveLocalSync: FAILED to open file for write");
        }
    }

    void saveLocal() {
        if (configPath_.empty()) {
            log_warn("[CFG] saveLocal: no configPath_, skipping");
            return;
        }

        
        buildConfigFromModules();

        std::string content;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            content = config_.serialize();
        }

        log_raw("[CFG] saveLocal: queuing async write of %d bytes", (int)content.size());

        std::thread([this, content] {
            std::lock_guard<std::mutex> lock(fileWriteMutex_);
            std::ofstream file(configPath_);
            if (file.is_open()) {
                file << content;
                file.close();
                log_raw("[CFG] saveLocal: async write OK (%d bytes)", (int)content.size());
            } else {
                log_err("[CFG] saveLocal: async write FAILED to open file");
            }
        }).detach();
    }

    
    static std::vector<uint8_t> b64decode(const std::string& input) {
        DWORD outLen = 0;
        if (!CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, nullptr, &outLen, nullptr, nullptr))
            return {};
        std::vector<uint8_t> output(outLen);
        if (!CryptStringToBinaryA(input.c_str(), (DWORD)input.length(), CRYPT_STRING_BASE64, output.data(), &outLen, nullptr, nullptr))
            return {};
        output.resize(outLen);
        return output;
    }

    void fetchServerConfig() {
        if (!syncEnabled_ || hwid_.empty() || sessionToken_.empty())
            return;

        std::string body = "{\"hwid\":\"" + http::json_escape(hwid_) +
                           "\",\"token\":\"" + http::json_escape(sessionToken_) + "\"}";

        
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/config").c_str(), body, CONFIG_SERVER_PORT);

        if (!response.success) {
            log_debug("ConfigManager: Server fetch failed: %s", response.error.c_str());
            return;
        }

        
        json::Value envelope;
        if (!json::Parser::parse(response.body, envelope)) {
            log_warn("ConfigManager: Invalid server response");
            return;
        }

        std::string payloadB64 = envelope["payload_b64"].as_string("");
        std::string sigB64 = envelope["sig_b64"].as_string("");

        if (payloadB64.empty() || sigB64.empty()) {
            log_warn("ConfigManager: Missing signature - response tampered");
            return;
        }

        auto payloadBytes = b64decode(payloadB64);
        auto sigBytes = b64decode(sigB64);

        if (payloadBytes.empty() || sigBytes.size() != 64) {
            log_warn("ConfigManager: Invalid sig format");
            return;
        }

        uint8_t pubkey[32];
        cobble_keys::GetPublicKey(pubkey);
        bool sigOk = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
        SecureZeroMemory(pubkey, 32);

        if (!sigOk) {
            log_warn("ConfigManager: Signature invalid - response tampered");
            return;
        }

        
        json::Value serverResponse;
        if (!json::Parser::parse(std::string(payloadBytes.begin(), payloadBytes.end()), serverResponse)) {
            log_warn("ConfigManager: Invalid payload JSON");
            return;
        }

        if (!serverResponse["success"].as_bool(false)) {
            log_debug("ConfigManager: Server returned failure");
            return;
        }

        std::string serverConfig = serverResponse["config"].as_string("{}");
        if (serverConfig.empty() || serverConfig == "{}") {
            log_debug("ConfigManager: No server config stored");
            return;
        }

        
        json::Value serverData;
        if (json::Parser::parse(serverConfig, serverData)) {
            mergeServerConfig(serverData);
            applyConfig();
            saveLocalSync();  
            log_ok("ConfigManager: Applied server config");
        }
    }

    void pushServerConfig() {
        if (!syncEnabled_ || hwid_.empty())
            return;

        buildConfigFromModules();

        std::string configJson;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            configJson = config_.serialize();
        }

        
        
        std::string body = "{\"hwid\":\"" + http::json_escape(hwid_) +
                           "\",\"token\":\"" + http::json_escape(sessionToken_) +
                           "\",\"config\":" + configJson + "}";

        
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/config/save").c_str(), body, CONFIG_SERVER_PORT);

        if (!response.success) {
            log_debug("ConfigManager: Sync push failed");
            return;
        }

        
        json::Value envelope;
        if (!json::Parser::parse(response.body, envelope)) return;

        std::string payloadB64 = envelope["payload_b64"].as_string("");
        std::string sigB64 = envelope["sig_b64"].as_string("");
        if (payloadB64.empty() || sigB64.empty()) {
            log_warn("ConfigManager: Save response missing signature");
            return;
        }

        auto payloadBytes = b64decode(payloadB64);
        auto sigBytes = b64decode(sigB64);
        if (payloadBytes.empty() || sigBytes.size() != 64) return;

        uint8_t pubkey[32];
        cobble_keys::GetPublicKey(pubkey);
        bool sigOk = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
        SecureZeroMemory(pubkey, 32);

        if (sigOk) {
            log_debug("ConfigManager: Synced to server");
        } else {
            log_warn("ConfigManager: Save response signature invalid");
        }
    }

    void enqueueSyncDebounced() {
        if (!syncEnabled_)
            return;

        lastChangeTime_ = std::chrono::steady_clock::now();

        if (!pendingSync_.exchange(true)) {
            std::thread([this] {
                std::this_thread::sleep_for(std::chrono::milliseconds(CONFIG_DEBOUNCE_MS));

                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                     lastChangeTime_)
                                   .count();

                
                if (elapsed >= CONFIG_DEBOUNCE_MS) {
                    pushServerConfig();
                }

                pendingSync_ = false;
            }).detach();
        }
    }

    void mergeServerConfig(const json::Value& serverData) {
        std::lock_guard<std::mutex> lock(configMutex_);

        
        if (serverData.has("modules")) {
            const auto& serverModules = serverData["modules"].object();
            for (const auto& kv : serverModules) {
                config_["modules"][kv.first] = kv.second;
            }
        }

        
        if (serverData.has("flags")) {
            config_["flags"] = serverData["flags"];
        }

        
        if (serverData.has("hud")) {
            config_["hud"] = serverData["hud"];
        }
    }

    void applyDefaults();
    void applyConfig();
    void buildConfigFromModules();
    void migrateConfig(int fromVersion);

    
    
    
    bool verifySignedEnvelope(const std::string& responseBody, json::Value& outPayload);

    static constexpr int CONFIG_VERSION = 1;

    
    
    static constexpr int PUBLIC_CONFIG_VERSION = 1;

    mutable std::mutex configMutex_;
    mutable std::mutex fileWriteMutex_;
    json::Value config_;
    json::Value defaultConfig_;
    bool hasDefaultConfig_ = false;
    std::string configPath_;
    std::string hwid_;
    std::string sessionToken_;
    bool syncEnabled_ = false;  
    std::atomic<bool> pendingSync_{false};
    std::atomic<bool> syncRunning_{true};
    std::chrono::steady_clock::time_point lastChangeTime_;

    
    mutable std::mutex browseMutex_;
    BrowseState browseState_;

    void loadSessionToken() {
        if (configPath_.empty())
            return;

        
        std::string configDir = configPath_.substr(0, configPath_.find_last_of("\\/"));
        std::string sessionPath = configDir + "\\.session";

        std::ifstream sessionFile(sessionPath);
        if (sessionFile.is_open()) {
            std::getline(sessionFile, sessionToken_);
            sessionFile.close();

            
            DeleteFileA(sessionPath.c_str());

            if (!sessionToken_.empty()) {
                log_debug("ConfigManager: Loaded session token");
            }
        }
    }
};
