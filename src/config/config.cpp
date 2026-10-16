#include "config.hpp"
#include "../module/module.h"
#include "../module/module_manager.h"
#include "../security/session_validator.hpp"
#include "../ui/hud/arraylist.h"
#include "../ui/widgets/widgets.hpp"
#include "../ui/hud/watermark.h"
#include "../ui/hud/notifications.h"
#include "../ui/menu/menu.h"
#include "../ui/widgets/theme.hpp"
#include "../util/team/friends_manager.h"

#include <algorithm>
#include <cstring>
#include <cctype>

using namespace ArrayListRenderer;

namespace {
static void copy_config_text(char* destination, size_t capacity, const std::string& value) {
    if (!destination || capacity == 0)
        return;
    strncpy_s(destination, capacity, value.c_str(), capacity - 1);
}









static void migrate_legacy_watermark(const json::Value& hud) {
    
    auto readFlat = [&hud](const char* base, float rgba[4]) {
        const std::string b(base);
        rgba[0] = hud[b + "R"].as_float(rgba[0]);
        rgba[1] = hud[b + "G"].as_float(rgba[1]);
        rgba[2] = hud[b + "B"].as_float(rgba[2]);
        rgba[3] = hud[b + "A"].as_float(rgba[3]);
    };

    Watermark::scale = hud["watermarkScale"].as_float(Watermark::scale);
    
    Watermark::fontPrimary = hud["watermarkFontStyle"].as_int(Watermark::fontPrimary);
    Watermark::fontSecondary = Watermark::fontPrimary;
    Watermark::secondaryRatio = hud["watermarkMetaScale"].as_float(Watermark::secondaryRatio);
    Watermark::rainbowType = hud["watermarkRainbowType"].as_int(Watermark::rainbowType);
    Watermark::showBackground = hud["watermarkBackground"].as_bool(Watermark::showBackground);
    Watermark::showBorder = hud["watermarkBorder"].as_bool(Watermark::showBorder);
    Watermark::borderWidth = hud["watermarkBorderWidth"].as_float(Watermark::borderWidth);
    Watermark::rounding = hud["watermarkRounding"].as_float(Watermark::rounding);
    Watermark::paddingX = hud["watermarkPaddingX"].as_float(Watermark::paddingX);
    Watermark::paddingY = hud["watermarkPaddingY"].as_float(Watermark::paddingY);
    Watermark::showAccentBar = hud["watermarkBar"].as_bool(Watermark::showAccentBar);
    Watermark::barWidth = hud["watermarkBarWidth"].as_float(Watermark::barWidth);
    Watermark::barSide = Watermark::BarLeft;
    Watermark::barColorMode =
        hud["watermarkBarUseAccent"].as_bool(true) ? Watermark::ColorAccent : Watermark::ColorCustom;
    Watermark::itemGap = hud["watermarkTitleMetaGap"].as_float(Watermark::itemGap);
    Watermark::labelGap = hud["watermarkNumberLabelGap"].as_float(Watermark::labelGap);

    
    if (hud["watermarkShadow"].as_bool(false)) {
        Watermark::textEffect = Watermark::EffectShadow;
        readFlat("watermarkShadow", Watermark::effectColor);
    }
    readFlat("watermarkBackground", Watermark::backgroundColor);
    readFlat("watermarkBorder", Watermark::borderColor);
    readFlat("watermarkBar", Watermark::barColor);
    readFlat("watermarkImage", Watermark::imageTint);
    
    readFlat("watermarkTitle", Watermark::primaryColor);
    readFlat("watermarkMeta", Watermark::secondaryColor);

    
    auto add = [](int kind, const std::string& text, int weight, int colorMode) -> Watermark::Item* {
        if (Watermark::itemCount >= Watermark::kMaxItems)
            return nullptr;
        Watermark::Item& it = Watermark::items[Watermark::itemCount++];
        it = Watermark::Item();
        it.kind = kind;
        it.weight = weight;
        it.colorMode = colorMode;
        copy_config_text(it.text, sizeof(it.text), text);
        return &it;
    };

    Watermark::itemCount = 0;
    const bool legacyImage = hud["watermarkImage"].as_bool(false) && Watermark::imagePath[0] != '\0';
    const bool imageRight = hud["watermarkImageSide"].as_int(0) == 1;
    if (legacyImage && !imageRight)
        add(Watermark::KindImage, "", Watermark::WeightPrimary, Watermark::ColorPrimary);

    if (hud["watermarkShowTitle"].as_bool(true)) {
        
        const int mode = hud["watermarkRainbow"].as_bool(true)
                             ? Watermark::ColorRainbow
                             : (hud["watermarkTitleUseAccent"].as_bool(true) ? Watermark::ColorAccent
                                                                            : Watermark::ColorPrimary);
        add(Watermark::KindText, hud["watermarkTitle"].as_string("Cobble"), Watermark::WeightPrimary, mode);
    }
    if (hud["watermarkShowVersion"].as_bool(true))
        add(Watermark::KindText, hud["watermarkVersion"].as_string(""), Watermark::WeightSecondary,
            Watermark::ColorSecondary);
    if (hud["watermarkShowFps"].as_bool(true)) {
        if (Watermark::Item* sep =
                add(Watermark::KindSeparator, "", Watermark::WeightPrimary, Watermark::ColorSecondary))
            sep->gap = hud["watermarkSeparatorGap"].as_float(-1.0f);
        const bool fpsAccent = hud["watermarkFpsUseAccent"].as_bool(true);
        Watermark::Item* fps =
            add(Watermark::KindFps, hud["watermarkFpsLabel"].as_string("FPS"), Watermark::WeightPrimary,
                fpsAccent ? Watermark::ColorAccent : Watermark::ColorCustom);
        if (fps && !fpsAccent)
            readFlat("watermarkFps", fps->color);
    }
    if (legacyImage && imageRight)
        add(Watermark::KindImage, "", Watermark::WeightPrimary, Watermark::ColorPrimary);

    
    
    Watermark::itemsInitialized = true;
}

static std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string sanitize_user_config_name(const std::string& in) {
    std::string s = trim_copy(in);
    std::string out;
    out.reserve(s.size());

    
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' ||
            c == '-') {
            out.push_back((char)c);
        }
    }

    out = trim_copy(out);
    if (out.size() > 48)  
        out.resize(48);

    return out;
}

static bool ends_with_case_insensitive(const std::string& s, const char* suffix) {
    size_t n = s.size();
    size_t m = strlen(suffix);
    if (m > n)
        return false;
    for (size_t i = 0; i < m; i++) {
        char a = (char)std::tolower((unsigned char)s[n - m + i]);
        char b = (char)std::tolower((unsigned char)suffix[i]);
        if (a != b)
            return false;
    }
    return true;
}
}  

std::string ConfigManager::getUserConfigsDir() const {
    if (configPath_.empty())
        return "";

    std::string configDir = configPath_.substr(0, configPath_.find_last_of("\\/"));
    std::string userDir = configDir + "\\configs";

    CreateDirectoryA(userDir.c_str(), nullptr);  
    return userDir;
}

std::vector<std::string> ConfigManager::listUserConfigs() const {
    std::vector<std::string> out;
    std::string dir = getUserConfigsDir();
    if (dir.empty())
        return out;

    std::string pattern = dir + "\\*.json";

    WIN32_FIND_DATAA ffd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return out;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::string file = ffd.cFileName;
        if (!ends_with_case_insensitive(file, ".json"))
            continue;

        
        file.resize(file.size() - 5);
        if (file.empty())
            continue;

        
        if (sanitize_user_config_name(file) != file)
            continue;

        out.push_back(file);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);

    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        auto lower = [](unsigned char c) { return (unsigned char)std::tolower(c); };
        size_t n = (std::min)(a.size(), b.size());
        for (size_t i = 0; i < n; i++) {
            unsigned char ca = lower((unsigned char)a[i]);
            unsigned char cb = lower((unsigned char)b[i]);
            if (ca < cb)
                return true;
            if (ca > cb)
                return false;
        }
        return a.size() < b.size();
    });

    return out;
}

bool ConfigManager::saveUserConfig(const std::string& name, std::string* err) {
    std::string safe = sanitize_user_config_name(name);
    if (safe.empty()) {
        if (err)
            *err = "Invalid name";
        return false;
    }

    std::string dir = getUserConfigsDir();
    if (dir.empty()) {
        if (err)
            *err = "No config directory";
        return false;
    }

    std::string path = dir + "\\" + safe + ".json";

    
    buildConfigFromModules();

    std::string content;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        content = config_.serialize();
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        log_err("[CFG] saveUserConfig('%s'): FAILED to open file", safe.c_str());
        if (err)
            *err = "Failed to open file for write";
        return false;
    }

    file << content;
    file.close();

    log_raw("[CFG] saveUserConfig('%s'): wrote %d bytes", safe.c_str(), (int)content.size());
    return true;
}

bool ConfigManager::loadUserConfig(const std::string& name, std::string* err) {
    std::string safe = sanitize_user_config_name(name);
    if (safe.empty()) {
        if (err)
            *err = "Invalid name";
        return false;
    }

    std::string dir = getUserConfigsDir();
    if (dir.empty()) {
        if (err)
            *err = "No config directory";
        return false;
    }

    std::string path = dir + "\\" + safe + ".json";

    std::ifstream file(path);
    if (!file.is_open()) {
        if (err)
            *err = "Config not found";
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    log_raw("[CFG] loadUserConfig('%s'): read %d bytes, parsing", safe.c_str(), (int)content.size());

    json::Value loaded;
    if (!json::Parser::parse(content, loaded)) {
        log_err("[CFG] loadUserConfig('%s'): PARSE FAILED", safe.c_str());
        if (err)
            *err = "Failed to parse JSON";
        return false;
    }

    int version = loaded["version"].as_int(0);

    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_ = loaded;
    }

    if (version < CONFIG_VERSION) {
        migrateConfig(version);
    }

    log_raw("[CFG] loadUserConfig('%s'): parse OK, applying", safe.c_str());
    applyConfig();

    
    onSettingChanged();

    return true;
}

bool ConfigManager::deleteUserConfig(const std::string& name, std::string* err) {
    std::string safe = sanitize_user_config_name(name);
    if (safe.empty()) {
        if (err)
            *err = "Invalid name";
        return false;
    }

    std::string dir = getUserConfigsDir();
    if (dir.empty()) {
        if (err)
            *err = "No config directory";
        return false;
    }

    std::string path = dir + "\\" + safe + ".json";
    if (!DeleteFileA(path.c_str())) {
        log_err("[CFG] deleteUserConfig('%s'): DeleteFile FAILED (err=%lu)", safe.c_str(), GetLastError());
        if (err)
            *err = "Failed to delete file";
        return false;
    }

    log_raw("[CFG] deleteUserConfig('%s'): deleted", safe.c_str());
    return true;
}

bool ConfigManager::resetToDefaults(std::string* err) {
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        if (!hasDefaultConfig_) {
            if (err)
                *err = "Factory defaults are unavailable";
            return false;
        }
        config_ = defaultConfig_;
    }

    
    
    
    applyConfig();
    onSettingChanged();
    log_ok("[CFG] Restored factory defaults");
    return true;
}







std::string ConfigManager::exportPublicConfig() {
    
    buildConfigFromModules();

    std::lock_guard<std::mutex> lock(configMutex_);

    json::Value pub;
    pub["version"] = PUBLIC_CONFIG_VERSION;

    if (config_.has("modules")) {
        const auto& modules = config_["modules"].object();
        for (const auto& kv : modules) {
            json::Value node = kv.second;  
            node.remove("keybind");
            if (kv.first == "AltManager")
                node.remove("spoofedName");
            pub["modules"][kv.first] = node;
        }
    }

    return pub.serialize();
}







bool ConfigManager::importPublicConfig(const std::string& jsonStr, std::string* err) {
    if (!moduleManager) {
        if (err)
            *err = "Modules not ready";
        return false;
    }

    json::Value parsed;
    if (!sehParse(jsonStr, parsed)) {
        if (err)
            *err = "Failed to parse config";
        return false;
    }

    if (!parsed.has("modules") || !parsed["modules"].is_object()) {
        if (err)
            *err = "Not a valid public config";
        return false;
    }

    const auto& modules = parsed["modules"].object();
    int applied = 0;
    for (const auto& kv : modules) {
        const std::string& name = kv.first;
        const json::Value& data = kv.second;
        for (auto* mod : moduleManager->getModules()) {
            if (std::string(mod->name) != name)
                continue;

            
            bool shouldEnable = data["enabled"].as_bool(false);
            if (shouldEnable != mod->enabled && mod->isAccessible())
                mod->setEnabled(shouldEnable);

            
            mod->loadSettings(data);
            applied++;
            break;
        }
    }

    if (applied == 0) {
        if (err)
            *err = "No matching modules in config";
        return false;
    }

    
    onSettingChanged();
    return true;
}




bool ConfigManager::verifySignedEnvelope(const std::string& responseBody, json::Value& outPayload) {
    json::Value envelope;
    if (!json::Parser::parse(responseBody, envelope))
        return false;

    std::string payloadB64 = envelope["payload_b64"].as_string("");
    std::string sigB64 = envelope["sig_b64"].as_string("");
    if (payloadB64.empty() || sigB64.empty())
        return false;

    auto payloadBytes = b64decode(payloadB64);
    auto sigBytes = b64decode(sigB64);
    if (payloadBytes.empty() || sigBytes.size() != 64)
        return false;

    uint8_t pubkey[32];
    cobble_keys::GetPublicKey(pubkey);
    bool sigOk = tweetnacl::verify(sigBytes.data(), payloadBytes.data(), payloadBytes.size(), pubkey);
    SecureZeroMemory(pubkey, 32);
    if (!sigOk)
        return false;

    return json::Parser::parse(std::string(payloadBytes.begin(), payloadBytes.end()), outPayload);
}




void ConfigManager::publishPublicConfigAsync(const std::string& name, const std::string& description, bool anonymous) {
    
    
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    if (hwid.empty() || token.empty()) {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Error;
        browseState_.message = "Not signed in";
        return;
    }

    std::string publicJson = exportPublicConfig();
    {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Loading;
        browseState_.message = "Publishing...";
    }

    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" +
                       http::json_escape(token) + "\",\"name\":\"" + http::json_escape(name) +
                       "\",\"description\":\"" + http::json_escape(description) +
                       "\",\"anonymous\":" + (anonymous ? "true" : "false") +
                       ",\"config\":" + publicJson + "}";

    std::thread([this, body] {
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/configs/public/publish").c_str(), body,
                                          CONFIG_SERVER_PORT);

        json::Value payload;
        bool verified = response.success && verifySignedEnvelope(response.body, payload);
        bool ok = verified && payload["success"].as_bool(false);

        {
            std::lock_guard<std::mutex> lock(browseMutex_);
            browseState_.status = ok ? BrowseState::Ready : BrowseState::Error;
            if (ok)
                browseState_.message = "Published";
            else if (!response.success)
                browseState_.message = "Publish failed (HTTP " + std::to_string(response.statusCode) + ")";
            else if (!verified)
                browseState_.message = "Publish failed (bad signature)";
            else
                browseState_.message = "Publish rejected: " + payload["message"].as_string("server error");
        }

        
        if (ok)
            refreshPublicConfigsAsync();
    }).detach();
}


void ConfigManager::refreshPublicConfigsAsync() {
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    if (hwid.empty() || token.empty()) {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Error;
        browseState_.message = "Not signed in";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Loading;
        browseState_.message = "Loading...";
    }

    std::string body =
        "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) + "\"}";

    std::thread([this, body] {
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/configs/public/list").c_str(), body,
                                          CONFIG_SERVER_PORT);

        json::Value payload;
        bool verified = response.success && verifySignedEnvelope(response.body, payload);
        bool ok = verified && payload["success"].as_bool(false);

        std::lock_guard<std::mutex> lock(browseMutex_);
        if (!ok) {
            browseState_.status = BrowseState::Error;
            if (!response.success)
                browseState_.message = "Load failed (HTTP " + std::to_string(response.statusCode) + ")";
            else if (!verified)
                browseState_.message = "Load failed (bad signature)";
            else
                browseState_.message = "Load rejected: " + payload["message"].as_string("server error");
            return;
        }

        browseState_.entries.clear();
        if (payload.has("configs")) {
            
            for (const auto& kv : payload["configs"].object()) {
                const json::Value& e = kv.second;
                PublicConfigEntry entry;
                entry.id = e["id"].as_string("");
                entry.owner = e["owner"].as_string("");
                entry.name = e["name"].as_string("");
                entry.description = e["description"].as_string("");
                entry.downloads = e["downloads"].as_int(0);
                entry.owned = e["owned"].as_bool(false);
                if (!entry.id.empty())
                    browseState_.entries.push_back(std::move(entry));
            }
        }
        
        std::sort(browseState_.entries.begin(), browseState_.entries.end(),
                  [](const PublicConfigEntry& a, const PublicConfigEntry& b) { return a.downloads > b.downloads; });

        browseState_.status = BrowseState::Ready;
        browseState_.message = browseState_.entries.empty() ? "No configs found" : "";
    }).detach();
}



void ConfigManager::downloadAndApplyPublicConfigAsync(const std::string& id) {
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    if (hwid.empty() || token.empty() || id.empty()) {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Error;
        browseState_.message = "Not signed in";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Loading;
        browseState_.message = "Downloading...";
    }

    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" +
                       http::json_escape(token) + "\",\"id\":\"" + http::json_escape(id) + "\"}";

    std::thread([this, body] {
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/configs/public/get").c_str(), body,
                                          CONFIG_SERVER_PORT);

        json::Value payload;
        bool verified = response.success && verifySignedEnvelope(response.body, payload);
        bool ok = verified && payload["success"].as_bool(false);

        std::string resultMsg;
        if (ok) {
            std::string cfg = payload["config"].as_string("");
            std::string err;
            if (importPublicConfig(cfg, &err))
                resultMsg = "Applied: " + payload["name"].as_string("config");
            else
                resultMsg = "Apply failed: " + err;
        } else if (!response.success) {
            resultMsg = "Download failed (HTTP " + std::to_string(response.statusCode) + ")";
        } else if (!verified) {
            resultMsg = "Download failed (bad signature)";
        } else {
            resultMsg = "Download rejected: " + payload["message"].as_string("server error");
        }

        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = ok ? BrowseState::Ready : BrowseState::Error;
        browseState_.message = resultMsg;
    }).detach();
}



void ConfigManager::deletePublicConfigAsync(const std::string& id) {
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    if (hwid.empty() || token.empty() || id.empty()) {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Error;
        browseState_.message = "Not signed in";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(browseMutex_);
        browseState_.status = BrowseState::Loading;
        browseState_.message = "Deleting...";
    }

    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" +
                       http::json_escape(token) + "\",\"id\":\"" + http::json_escape(id) + "\"}";

    std::thread([this, body] {
        auto response = http::post_cobble(OBFUSCATE_WSTRING(L"/api/configs/public/delete").c_str(), body,
                                          CONFIG_SERVER_PORT);

        json::Value payload;
        bool verified = response.success && verifySignedEnvelope(response.body, payload);
        bool ok = verified && payload["success"].as_bool(false);

        {
            std::lock_guard<std::mutex> lock(browseMutex_);
            browseState_.status = ok ? BrowseState::Ready : BrowseState::Error;
            if (ok)
                browseState_.message = "Deleted";
            else if (!response.success)
                browseState_.message = "Delete failed (HTTP " + std::to_string(response.statusCode) + ")";
            else if (!verified)
                browseState_.message = "Delete failed (bad signature)";
            else
                browseState_.message = "Delete rejected: " + payload["message"].as_string("server error");
        }

        if (ok)
            refreshPublicConfigsAsync();
    }).detach();
}


void ConfigManager::applyDefaults() {
    COBBLE_TRACE("applyDefaults:enter");
    std::lock_guard<std::mutex> lock(configMutex_);
    COBBLE_TRACE("applyDefaults:got mutex");

    
    
    
    
    sehResetValue(config_);
    COBBLE_TRACE("applyDefaults:config_ reset");

    config_["version"] = CONFIG_VERSION;
    COBBLE_TRACE("applyDefaults:version set");

    
    config_["settings"]["cloudSync"] = false;
    COBBLE_TRACE("applyDefaults:cloudSync set");

    
    config_["hud"]["arrayList"] = true;
    config_["hud"]["watermark"] = true;
    config_["hud"]["notifications"] = true;
    COBBLE_TRACE("applyDefaults:hud set");

    
    COBBLE_TRACE("applyDefaults:exit");
}


void ConfigManager::applyConfig() {
    if (!moduleManager)
        return;

    
    if (config_.has("settings")) {
        syncEnabled_ = config_["settings"]["cloudSync"].as_bool(false);
        Menu::menuKey = config_["settings"]["menuKey"].as_int(0x2D);  
        
        if (config_["settings"].has("accentColor")) {
            ui::theme::accentColor[0] = config_["settings"]["accentColor"]["r"].as_float(0.9412f);
            ui::theme::accentColor[1] = config_["settings"]["accentColor"]["g"].as_float(0.4667f);
            ui::theme::accentColor[2] = config_["settings"]["accentColor"]["b"].as_float(0.0549f);
            ui::theme::accentColor[3] = config_["settings"]["accentColor"]["a"].as_float(1.0f);
        }
        ui::theme::accentRainbow = config_["settings"]["accentRainbow"].as_bool(false);
        ui::theme::rainbowType = config_["settings"]["rainbowType"].as_int(0);
        ui::theme::glowAccent = config_["settings"]["glowAccent"].as_bool(false);
    }

    
    if (config_.has("hud")) {
        enabled = config_["hud"]["arrayList"].as_bool(true);
        rainbow = config_["hud"]["arrayListRainbow"].as_bool(false);
        rainbowType = config_["hud"]["arrayListRainbowType"].as_int(0);
        colorBar = config_["hud"]["arrayListColorBar"].as_bool(true);
        showBackground = config_["hud"]["arrayListBackground"].as_bool(false);
        showInfo = config_["hud"]["arrayListShowInfo"].as_bool(true);
        sortByLength = config_["hud"]["arrayListSortByLength"].as_bool(true);
        fontStyle = config_["hud"]["arrayListFontStyle"].as_int(0);
        scale = config_["hud"]["arrayListScale"].as_float(1.0f);
        rainbowSaturation = config_["hud"]["arrayListSaturation"].as_float(0.7f);
        anchor = config_["hud"]["arrayListAnchor"].as_int(0);
        useAccent = config_["hud"]["arrayListUseAccent"].as_bool(true);
        color[0] = config_["hud"]["arrayListColorR"].as_float(color[0]);
        color[1] = config_["hud"]["arrayListColorG"].as_float(color[1]);
        color[2] = config_["hud"]["arrayListColorB"].as_float(color[2]);
        ui::gui_mode = config_["hud"]["guiMode"].as_int(ui::gui_mode);
        
        
        
        
        relX = config_["hud"]["arrayListRelX"].as_float(0.0f);
        relY = config_["hud"]["arrayListRelY"].as_float(0.0f);
        userPlaced = config_["hud"]["arrayListPlaced"].as_bool(false);
        Watermark::enabled = config_["hud"]["watermark"].as_bool(true);
        Watermark::scale = config_["hud"]["watermarkScale"].as_float(Watermark::scale);
        Watermark::opacity = config_["hud"]["watermarkOpacity"].as_float(Watermark::opacity);
        Watermark::fontPrimary = config_["hud"]["watermarkFontPrimary"].as_int(Watermark::fontPrimary);
        Watermark::fontSecondary = config_["hud"]["watermarkFontSecondary"].as_int(Watermark::fontSecondary);
        Watermark::fontSize = config_["hud"]["watermarkFontSize"].as_float(Watermark::fontSize);
        Watermark::secondaryRatio = config_["hud"]["watermarkSecondaryRatio"].as_float(Watermark::secondaryRatio);
        Watermark::rainbowType = config_["hud"]["watermarkRainbowType"].as_int(Watermark::rainbowType);
        Watermark::rainbowSaturation = config_["hud"]["watermarkSaturation"].as_float(Watermark::rainbowSaturation);
        Watermark::layout = config_["hud"]["watermarkLayout"].as_int(Watermark::layout);
        Watermark::align = config_["hud"]["watermarkAlign"].as_int(Watermark::align);
        Watermark::itemGap = config_["hud"]["watermarkItemGap"].as_float(Watermark::itemGap);
        Watermark::labelGap = config_["hud"]["watermarkLabelGap"].as_float(Watermark::labelGap);
        Watermark::showBackground = config_["hud"]["watermarkBackground"].as_bool(Watermark::showBackground);
        Watermark::backgroundGradient = config_["hud"]["watermarkGradient"].as_bool(Watermark::backgroundGradient);
        Watermark::gradientVertical =
            config_["hud"]["watermarkGradientVertical"].as_bool(Watermark::gradientVertical);
        Watermark::rounding = config_["hud"]["watermarkRounding"].as_float(Watermark::rounding);
        Watermark::paddingX = config_["hud"]["watermarkPaddingX"].as_float(Watermark::paddingX);
        Watermark::paddingY = config_["hud"]["watermarkPaddingY"].as_float(Watermark::paddingY);
        Watermark::showBorder = config_["hud"]["watermarkBorder"].as_bool(Watermark::showBorder);
        Watermark::borderWidth = config_["hud"]["watermarkBorderWidth"].as_float(Watermark::borderWidth);
        Watermark::showGlow = config_["hud"]["watermarkGlow"].as_bool(Watermark::showGlow);
        Watermark::glowUseAccent = config_["hud"]["watermarkGlowAccent"].as_bool(Watermark::glowUseAccent);
        Watermark::glowSize = config_["hud"]["watermarkGlowSize"].as_float(Watermark::glowSize);
        Watermark::showAccentBar = config_["hud"]["watermarkBar"].as_bool(Watermark::showAccentBar);
        Watermark::barSide = config_["hud"]["watermarkBarSide"].as_int(Watermark::barSide);
        Watermark::barColorMode = config_["hud"]["watermarkBarColorMode"].as_int(Watermark::barColorMode);
        Watermark::barWidth = config_["hud"]["watermarkBarWidth"].as_float(Watermark::barWidth);
        Watermark::barGap = config_["hud"]["watermarkBarGap"].as_float(Watermark::barGap);
        Watermark::barInset = config_["hud"]["watermarkBarInset"].as_float(Watermark::barInset);
        Watermark::barRounding = config_["hud"]["watermarkBarRounding"].as_float(Watermark::barRounding);
        Watermark::textEffect = config_["hud"]["watermarkEffect"].as_int(Watermark::textEffect);
        Watermark::effectSize = config_["hud"]["watermarkEffectSize"].as_float(Watermark::effectSize);
        Watermark::separatorThickness =
            config_["hud"]["watermarkSeparatorThickness"].as_float(Watermark::separatorThickness);
        Watermark::separatorLength = config_["hud"]["watermarkSeparatorLength"].as_float(Watermark::separatorLength);
        Watermark::clock24h = config_["hud"]["watermarkClock24h"].as_bool(Watermark::clock24h);
        Watermark::clockSeconds = config_["hud"]["watermarkClockSeconds"].as_bool(Watermark::clockSeconds);
        Watermark::uptimeHours = config_["hud"]["watermarkUptimeHours"].as_bool(Watermark::uptimeHours);
        
        
        
        json::readColor(config_["hud"], "watermarkPrimaryColor", Watermark::primaryColor);
        json::readColor(config_["hud"], "watermarkSecondaryColor", Watermark::secondaryColor);
        json::readColor(config_["hud"], "watermarkBackgroundColor", Watermark::backgroundColor);
        json::readColor(config_["hud"], "watermarkBackgroundColor2", Watermark::backgroundColor2);
        json::readColor(config_["hud"], "watermarkBorderColor", Watermark::borderColor);
        json::readColor(config_["hud"], "watermarkGlowColor", Watermark::glowColor);
        json::readColor(config_["hud"], "watermarkBarColor", Watermark::barColor);
        json::readColor(config_["hud"], "watermarkEffectColor", Watermark::effectColor);
        json::readColor(config_["hud"], "watermarkSeparatorColor", Watermark::separatorColor);
        json::readColor(config_["hud"], "watermarkImageTint", Watermark::imageTint);

        Watermark::anchor = config_["hud"]["watermarkAnchor"].as_int(Watermark::anchor);
        Watermark::margin = config_["hud"]["watermarkMargin"].as_float(Watermark::margin);
        Watermark::relX = config_["hud"]["watermarkRelX"].as_float(Watermark::relX);
        Watermark::relY = config_["hud"]["watermarkRelY"].as_float(Watermark::relY);
        Watermark::userPlaced = config_["hud"]["watermarkPlaced"].as_bool(false);

        
        
        copy_config_text(Watermark::imagePath, sizeof(Watermark::imagePath),
                         config_["hud"]["watermarkImagePath"].as_string(Watermark::imagePath));
        Watermark::imageScale = config_["hud"]["watermarkImageScale"].as_float(Watermark::imageScale);
        Watermark::imageOpacity = config_["hud"]["watermarkImageOpacity"].as_float(Watermark::imageOpacity);
        Watermark::imageRounding = config_["hud"]["watermarkImageRounding"].as_float(Watermark::imageRounding);
        Watermark::imageAnimationSpeed =
            config_["hud"]["watermarkImageAnimationSpeed"].as_float(Watermark::imageAnimationSpeed);
        
        
        
        if (config_["hud"].has("watermarkItems")) {
            const json::Value& wmItems = config_["hud"]["watermarkItems"];
            Watermark::itemCount = 0;
            for (const json::Value& node : wmItems.array()) {
                if (Watermark::itemCount >= Watermark::kMaxItems)
                    break;
                const int kind = node["kind"].as_int(Watermark::KindText);
                if (kind < 0 || kind >= Watermark::KindCount)
                    continue;  
                Watermark::Item item;
                item.kind = kind;
                item.enabled = node["on"].as_bool(true);
                item.colorMode =
                    (std::clamp)(node["color"].as_int(Watermark::ColorPrimary), 0, Watermark::ColorModeCount - 1);
                item.weight = (std::clamp)(node["weight"].as_int(Watermark::WeightPrimary), 0, 1);
                item.labelBefore = node["labelFirst"].as_bool(false);
                
                
                item.gap = node["gap"].as_float(-1.0f);
                item.size = node["size"].as_float(item.size);
                copy_config_text(item.text, sizeof(item.text), node["text"].as_string(""));
                json::readColor(node, "custom", item.color);
                Watermark::items[Watermark::itemCount++] = item;
            }
            Watermark::itemsInitialized = true;
        } else {
            migrate_legacy_watermark(config_["hud"]);
        }
        NotificationManager::enabled = config_["hud"]["notifications"].as_bool(true);
    }

    
    if (config_.has("friends") && friendsManager) {
        
        if (config_["friends"].has("entries")) {
            friendsManager->clearFriends();
            const auto& entries = config_["friends"]["entries"].object();
            for (const auto& entry : entries) {
                const json::Value& data = entry.second;
                friendsManager->restoreFriend(data["uuid"].as_string(""), data["name"].as_string(""));
            }
        } else if (config_["friends"].has("list")) {
            
            friendsManager->clearFriends();
            std::string friendsList = config_["friends"]["list"].as_string("");
            if (!friendsList.empty()) {
                size_t start = 0, end;
                while ((end = friendsList.find(',', start)) != std::string::npos) {
                    std::string name = friendsList.substr(start, end - start);
                    if (!name.empty())
                        friendsManager->addFriend(name);
                    start = end + 1;
                }
                std::string last = friendsList.substr(start);
                if (!last.empty())
                    friendsManager->addFriend(last);
            }
        }
        
        friendsManager->teams.enabled = config_["friends"]["teamsEnabled"].as_bool(true);
        friendsManager->teams.matchOwnNametag = config_["friends"]["matchOwnNametag"].as_bool(true);
        friendsManager->applyToCombat = config_["friends"]["applyToCombat"].as_bool(true);
        friendsManager->applyToVisual = config_["friends"]["applyToVisual"].as_bool(false);
        
        friendsManager->quickAddEnabled = config_["friends"]["quickAddEnabled"].as_bool(true);
        friendsManager->quickAddKey = config_["friends"]["quickAddKey"].as_int(0x04);  
    }

    
    if (!config_.has("modules")) {
        log_warn("[CFG] applyConfig: config has NO 'modules' key -> nothing applied");
        return;
    }

    if (!config_["modules"].has("AntiDebuff") && config_["modules"].has("NoVisualEffects"))
        config_["modules"]["AntiDebuff"] = config_["modules"]["NoVisualEffects"];

    const auto& modules = config_["modules"].object();
    log_raw("[CFG] applyConfig: %d module nodes in config, AimAssist.speed(json)=%d enabled(json)=%d",
            (int)modules.size(), (int)config_["modules"]["AimAssist"]["speed"].as_float(-1.0f),
            (int)config_["modules"]["AimAssist"]["enabled"].as_bool(false));

    int matched = 0, enabledCount = 0;
    for (const auto& kv : modules) {
        const std::string& name = kv.first;
        const json::Value& data = kv.second;

        
        for (auto* mod : moduleManager->getModules()) {
            if (std::string(mod->name) != name)
                continue;

            matched++;
            
            mod->keybind = data["keybind"].as_int(0);
            bool shouldEnable = data["enabled"].as_bool(false);
            if (shouldEnable != mod->enabled && mod->isAccessible()) {
                mod->setEnabled(shouldEnable);
            }
            if (mod->enabled)
                enabledCount++;

            
            
            
            mod->loadSettings(data);

            break;
        }
    }
    log_raw("[CFG] applyConfig done: matched %d/%d nodes, %d modules now enabled",
            matched, (int)modules.size(), enabledCount);
}


void ConfigManager::buildConfigFromModules() {
    if (!moduleManager)
        return;

    std::lock_guard<std::mutex> lock(configMutex_);

    config_["version"] = CONFIG_VERSION;

    
    config_["settings"]["cloudSync"] = syncEnabled_;
    config_["settings"]["menuKey"] = Menu::menuKey;
    
    config_["settings"]["accentColor"]["r"] = ui::theme::accentColor[0];
    config_["settings"]["accentColor"]["g"] = ui::theme::accentColor[1];
    config_["settings"]["accentColor"]["b"] = ui::theme::accentColor[2];
    config_["settings"]["accentColor"]["a"] = ui::theme::accentColor[3];
    config_["settings"]["accentRainbow"] = ui::theme::accentRainbow;
    config_["settings"]["rainbowType"] = ui::theme::rainbowType;
    config_["settings"]["glowAccent"] = ui::theme::glowAccent;

    
    config_["hud"]["arrayList"] = enabled;
    config_["hud"]["arrayListRainbow"] = rainbow;
    config_["hud"]["arrayListRainbowType"] = rainbowType;
    config_["hud"]["arrayListColorBar"] = colorBar;
    config_["hud"]["arrayListBackground"] = showBackground;
    config_["hud"]["arrayListShowInfo"] = showInfo;
    config_["hud"]["arrayListSortByLength"] = sortByLength;
    config_["hud"]["arrayListFontStyle"] = fontStyle;
    config_["hud"]["arrayListScale"] = scale;
    config_["hud"]["arrayListSaturation"] = rainbowSaturation;
    config_["hud"]["arrayListAnchor"] = anchor;
    config_["hud"]["arrayListUseAccent"] = useAccent;
    config_["hud"]["arrayListColorR"] = color[0];
    config_["hud"]["arrayListColorG"] = color[1];
    config_["hud"]["arrayListColorB"] = color[2];
    config_["hud"]["guiMode"] = ui::gui_mode;
    
    
    config_["hud"]["arrayListRelX"] = relX;
    config_["hud"]["arrayListRelY"] = relY;
    config_["hud"]["arrayListPlaced"] = userPlaced;
    config_["hud"]["watermark"] = Watermark::enabled;
    config_["hud"]["watermarkScale"] = Watermark::scale;
    config_["hud"]["watermarkOpacity"] = Watermark::opacity;
    config_["hud"]["watermarkFontPrimary"] = Watermark::fontPrimary;
    config_["hud"]["watermarkFontSecondary"] = Watermark::fontSecondary;
    config_["hud"]["watermarkFontSize"] = Watermark::fontSize;
    config_["hud"]["watermarkSecondaryRatio"] = Watermark::secondaryRatio;
    config_["hud"]["watermarkRainbowType"] = Watermark::rainbowType;
    config_["hud"]["watermarkSaturation"] = Watermark::rainbowSaturation;
    config_["hud"]["watermarkLayout"] = Watermark::layout;
    config_["hud"]["watermarkAlign"] = Watermark::align;
    config_["hud"]["watermarkItemGap"] = Watermark::itemGap;
    config_["hud"]["watermarkLabelGap"] = Watermark::labelGap;
    config_["hud"]["watermarkBackground"] = Watermark::showBackground;
    config_["hud"]["watermarkGradient"] = Watermark::backgroundGradient;
    config_["hud"]["watermarkGradientVertical"] = Watermark::gradientVertical;
    config_["hud"]["watermarkRounding"] = Watermark::rounding;
    config_["hud"]["watermarkPaddingX"] = Watermark::paddingX;
    config_["hud"]["watermarkPaddingY"] = Watermark::paddingY;
    config_["hud"]["watermarkBorder"] = Watermark::showBorder;
    config_["hud"]["watermarkBorderWidth"] = Watermark::borderWidth;
    config_["hud"]["watermarkGlow"] = Watermark::showGlow;
    config_["hud"]["watermarkGlowAccent"] = Watermark::glowUseAccent;
    config_["hud"]["watermarkGlowSize"] = Watermark::glowSize;
    config_["hud"]["watermarkBar"] = Watermark::showAccentBar;
    config_["hud"]["watermarkBarSide"] = Watermark::barSide;
    config_["hud"]["watermarkBarColorMode"] = Watermark::barColorMode;
    config_["hud"]["watermarkBarWidth"] = Watermark::barWidth;
    config_["hud"]["watermarkBarGap"] = Watermark::barGap;
    config_["hud"]["watermarkBarInset"] = Watermark::barInset;
    config_["hud"]["watermarkBarRounding"] = Watermark::barRounding;
    config_["hud"]["watermarkEffect"] = Watermark::textEffect;
    config_["hud"]["watermarkEffectSize"] = Watermark::effectSize;
    config_["hud"]["watermarkSeparatorThickness"] = Watermark::separatorThickness;
    config_["hud"]["watermarkSeparatorLength"] = Watermark::separatorLength;
    config_["hud"]["watermarkClock24h"] = Watermark::clock24h;
    config_["hud"]["watermarkClockSeconds"] = Watermark::clockSeconds;
    config_["hud"]["watermarkUptimeHours"] = Watermark::uptimeHours;
    config_["hud"]["watermarkAnchor"] = Watermark::anchor;
    config_["hud"]["watermarkMargin"] = Watermark::margin;
    config_["hud"]["watermarkRelX"] = Watermark::relX;
    config_["hud"]["watermarkRelY"] = Watermark::relY;
    config_["hud"]["watermarkPlaced"] = Watermark::userPlaced;
    config_["hud"]["watermarkImagePath"] = Watermark::imagePath;
    config_["hud"]["watermarkImageScale"] = Watermark::imageScale;
    config_["hud"]["watermarkImageOpacity"] = Watermark::imageOpacity;
    config_["hud"]["watermarkImageRounding"] = Watermark::imageRounding;
    config_["hud"]["watermarkImageAnimationSpeed"] = Watermark::imageAnimationSpeed;
    json::writeColor(config_["hud"], "watermarkPrimaryColor", Watermark::primaryColor);
    json::writeColor(config_["hud"], "watermarkSecondaryColor", Watermark::secondaryColor);
    json::writeColor(config_["hud"], "watermarkBackgroundColor", Watermark::backgroundColor);
    json::writeColor(config_["hud"], "watermarkBackgroundColor2", Watermark::backgroundColor2);
    json::writeColor(config_["hud"], "watermarkBorderColor", Watermark::borderColor);
    json::writeColor(config_["hud"], "watermarkGlowColor", Watermark::glowColor);
    json::writeColor(config_["hud"], "watermarkBarColor", Watermark::barColor);
    json::writeColor(config_["hud"], "watermarkEffectColor", Watermark::effectColor);
    json::writeColor(config_["hud"], "watermarkSeparatorColor", Watermark::separatorColor);
    json::writeColor(config_["hud"], "watermarkImageTint", Watermark::imageTint);

    
    
    
    json::Value wmItems;
    wmItems.array();
    for (int i = 0; i < Watermark::itemCount; ++i) {
        const Watermark::Item& item = Watermark::items[i];
        json::Value node;
        node["kind"] = item.kind;
        node["on"] = item.enabled;
        node["color"] = item.colorMode;
        node["weight"] = item.weight;
        node["labelFirst"] = item.labelBefore;
        node["gap"] = item.gap;
        node["size"] = item.size;
        node["text"] = item.text;
        json::writeColor(node, "custom", item.color);
        wmItems.push_back(node);
    }
    config_["hud"]["watermarkItems"] = wmItems;

    
    
    static const char* const kRetiredHudKeys[] = {
        "arrayListPosX",        "arrayListPosY",        "watermarkPosX",          "watermarkPosY",
        "watermarkRainbow",     "watermarkFontStyle",   "watermarkShowTitle",     "watermarkShowVersion",
        "watermarkShowFps",     "watermarkTitle",       "watermarkVersion",       "watermarkFpsLabel",
        "watermarkMetaScale",   "watermarkTitleUseAccent", "watermarkFpsUseAccent", "watermarkShadow",
        "watermarkBarUseAccent", "watermarkTitleMetaGap", "watermarkSeparatorGap", "watermarkNumberLabelGap",
        "watermarkImage",       "watermarkImageSide",   "watermarkImageGap",      "watermarkBackgroundR",
        "watermarkBackgroundG", "watermarkBackgroundB", "watermarkBackgroundA",   "watermarkBorderR",
        "watermarkBorderG",     "watermarkBorderB",     "watermarkBorderA",       "watermarkShadowR",
        "watermarkShadowG",     "watermarkShadowB",     "watermarkShadowA",       "watermarkBarR",
        "watermarkBarG",        "watermarkBarB",        "watermarkBarA",          "watermarkTitleR",
        "watermarkTitleG",      "watermarkTitleB",      "watermarkTitleA",        "watermarkMetaR",
        "watermarkMetaG",       "watermarkMetaB",       "watermarkMetaA",         "watermarkFpsR",
        "watermarkFpsG",        "watermarkFpsB",        "watermarkFpsA",          "watermarkImageR",
        "watermarkImageG",      "watermarkImageB",      "watermarkImageA",
    };
    for (const char* key : kRetiredHudKeys)
        config_["hud"].remove(key);
    config_["hud"]["notifications"] = NotificationManager::enabled;

    
    if (friendsManager) {
        auto friendRecords = friendsManager->getFriends();
        config_["friends"]["entries"] = json::Value();
        for (size_t i = 0; i < friendRecords.size(); ++i) {
            const std::string key = std::to_string(i);
            config_["friends"]["entries"][key]["uuid"] = friendRecords[i].uuid;
            config_["friends"]["entries"][key]["name"] = friendRecords[i].displayName;
        }

        auto friends = friendsManager->getFriendNames();
        std::string friendsList;
        for (size_t i = 0; i < friends.size(); ++i) {
            if (i > 0)
                friendsList += ",";
            friendsList += friends[i];
        }
        config_["friends"]["list"] = friendsList;
        config_["friends"]["teamsEnabled"] = friendsManager->teams.enabled;
        config_["friends"]["matchOwnNametag"] = friendsManager->teams.matchOwnNametag;
        config_["friends"]["applyToCombat"] = friendsManager->applyToCombat;
        config_["friends"]["applyToVisual"] = friendsManager->applyToVisual;
        
        config_["friends"]["quickAddEnabled"] = friendsManager->quickAddEnabled;
        config_["friends"]["quickAddKey"] = friendsManager->quickAddKey;
    }

    
    int enabledCount = 0;
    for (auto* mod : moduleManager->getModules()) {
        std::string name = mod->name;

        config_["modules"][name]["enabled"] = mod->enabled;
        config_["modules"][name]["keybind"] = mod->keybind;
        if (mod->enabled)
            enabledCount++;

        
        
        
        mod->saveSettings(config_["modules"][name]);
    }
    log_raw("[CFG] buildConfigFromModules: %d modules enabled, AimAssist.speed(live)=%d enabled(live)=%d",
            enabledCount, (int)config_["modules"]["AimAssist"]["speed"].as_float(-1.0f),
            (int)config_["modules"]["AimAssist"]["enabled"].as_bool(false));
}


void ConfigManager::migrateConfig(int fromVersion) {
    std::lock_guard<std::mutex> lock(configMutex_);

    
    

    config_["version"] = CONFIG_VERSION;
}
