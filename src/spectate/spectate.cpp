#include "spectate.hpp"

#include <cmath>
#include <thread>

#include "../config/config.hpp"  
#include "../sdk/core/minecraft.h"
#include "../sdk/player/player.h"
#include "../security/session_validator.hpp"
#include "../util/aes_string.hpp"  
#include "../util/network/http.hpp"
#include "../util/network/json.hpp"

namespace {


const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64encode(const std::vector<uint8_t>& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 3 <= in.size(); i += 3) {
        uint32_t n = (uint32_t(in[i]) << 16) | (uint32_t(in[i + 1]) << 8) | in[i + 2];
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >> 6) & 63]);
        out.push_back(kB64[n & 63]);
    }
    size_t rem = in.size() - i;
    if (rem == 1) {
        uint32_t n = uint32_t(in[i]) << 16;
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        uint32_t n = (uint32_t(in[i]) << 16) | (uint32_t(in[i + 1]) << 8);
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> b64decode(const std::string& in) {
    std::vector<uint8_t> out;
    out.reserve(in.size() * 3 / 4);
    int buf = 0, bits = 0;
    for (char c : in) {
        int v = b64val(c);
        if (v < 0) continue;  
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((buf >> bits) & 0xFF));
        }
    }
    return out;
}





bool parseEnvelope(const std::string& body, json::Value& out) {
    json::Value env;
    if (!json::Parser::parse(body, env))
        return false;
    std::string p64 = env["payload_b64"].as_string("");
    if (p64.empty())
        return false;
    auto bytes = b64decode(p64);
    if (bytes.empty())
        return false;
    return json::Parser::parse(std::string(bytes.begin(), bytes.end()), out);
}


bool session(std::string& hwid, std::string& token) {
    hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    return !hwid.empty() && !token.empty();
}

std::wstring host() { return cobble_config::GetServerHostW(); }

}  

SpectateManager& SpectateManager::get() {
    static SpectateManager inst;
    return inst;
}

void SpectateManager::heartbeatPresenceAsync(const std::string& activity) {
    std::string hwid, token;
    if (!session(hwid, token))
        return;
    bool as = allowSpectate_, acv = allowConfigView_;
    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"allow_spectate\":" + (as ? "true" : "false") + ",\"allow_config_view\":" +
                       (acv ? "true" : "false") + ",\"activity\":\"" + http::json_escape(activity) + "\"}";
    std::thread([this, body] {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/presence").c_str(), body, CONFIG_SERVER_PORT);
        json::Value payload;
        if (resp.success && parseEnvelope(resp.body, payload) && payload["success"].as_bool(false))
            spectatorCount_ = payload["spectator_count"].as_int(0);
    }).detach();
}

void SpectateManager::refreshListAsync() {
    std::string hwid, token;
    if (!session(hwid, token)) {
        std::lock_guard<std::mutex> lk(browseMutex_);
        browseState_.status = BrowseState::Error;
        browseState_.message = "Not signed in";
        return;
    }
    {
        std::lock_guard<std::mutex> lk(browseMutex_);
        browseState_.status = BrowseState::Loading;
        browseState_.message = "Loading...";
    }
    std::string body =
        "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) + "\"}";
    std::thread([this, body] {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/list").c_str(), body, CONFIG_SERVER_PORT);
        json::Value payload;
        bool ok = resp.success && parseEnvelope(resp.body, payload) && payload["success"].as_bool(false);

        std::lock_guard<std::mutex> lk(browseMutex_);
        if (!ok) {
            browseState_.status = BrowseState::Error;
            browseState_.message = "Load failed";
            return;
        }
        browseState_.entries.clear();
        if (payload.has("players")) {
            for (const auto& kv : payload["players"].object()) {
                const json::Value& e = kv.second;
                BrowseEntry be;
                be.username = e["username"].as_string("");
                be.activity = e["activity"].as_string("");
                be.spectatorCount = e["spectator_count"].as_int(0);
                be.isSpectator = e["is_spectator"].as_bool(false);
                if (!be.username.empty())
                    browseState_.entries.push_back(std::move(be));
            }
        }
        browseState_.status = BrowseState::Ready;
        browseState_.message = browseState_.entries.empty() ? "No players available" : "";
    }).detach();
}

void SpectateManager::startSpectatingAsync(const std::string& target) {
    std::string hwid, token;
    if (!session(hwid, token) || target.empty())
        return;
    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"target\":\"" + http::json_escape(target) + "\"}";
    std::thread([this, body, target] {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/start").c_str(), body, CONFIG_SERVER_PORT);
        json::Value payload;
        bool ok = resp.success && parseEnvelope(resp.body, payload) && payload["success"].as_bool(false);
        if (ok) {
            {
                std::lock_guard<std::mutex> lk(targetMutex_);
                target_ = target;
            }
            targetAllowsConfigView_ = payload["allow_config_view"].as_bool(false);
            {
                std::lock_guard<std::mutex> lk(snapMutex_);
                haveSnapshot_ = false;
            }
            spectating_ = true;
        } else {
            std::lock_guard<std::mutex> lk(browseMutex_);
            browseState_.message = "Cannot spectate that player";
        }
    }).detach();
}

void SpectateManager::stopSpectating() {
    if (!spectating_.exchange(false))
        return;
    std::string target;
    {
        std::lock_guard<std::mutex> lk(targetMutex_);
        target = target_;
        target_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        haveSnapshot_ = false;
    }
    std::string hwid, token;
    if (!session(hwid, token) || target.empty())
        return;
    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"target\":\"" + http::json_escape(target) + "\"}";
    std::thread([body] {
        http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/stop").c_str(), body, CONFIG_SERVER_PORT);
    }).detach();
}

void SpectateManager::uploadFrameAsync(std::vector<uint8_t> blob) {
    std::string hwid, token;
    if (!session(hwid, token))
        return;
    if (uploadInFlight_.exchange(true))
        return;  
    std::string frame = b64encode(blob);
    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"frame\":\"" + frame + "\"}";
    std::thread([this, body] {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/frame/put").c_str(), body, CONFIG_SERVER_PORT);
        json::Value payload;
        if (resp.success && parseEnvelope(resp.body, payload) && payload["success"].as_bool(false))
            spectatorCount_ = payload["spectator_count"].as_int(0);
        uploadInFlight_ = false;
    }).detach();
}

void SpectateManager::fetchFrameAsync(const std::string& target) {
    std::string hwid, token;
    if (!session(hwid, token) || target.empty())
        return;
    if (fetchInFlight_.exchange(true))
        return;
    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) + "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"target\":\"" + http::json_escape(target) + "\"}";
    std::thread([this, body] {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/spectate/frame/get").c_str(), body, CONFIG_SERVER_PORT);
        json::Value payload;
        if (resp.success && parseEnvelope(resp.body, payload) && payload["success"].as_bool(false)) {
            std::string f = payload["frame"].as_string("");
            if (!f.empty()) {
                auto bytes = b64decode(f);
                spectate::SpectateSnapshot snap;
                if (spectate::decode(bytes, snap)) {
                    {
                        std::lock_guard<std::mutex> lk(snapMutex_);
                        latestSnapshot_ = std::move(snap);
                        haveSnapshot_ = true;
                    }
                    ++framesReceived_;
                }
            }
        }
        fetchInFlight_ = false;
    }).detach();
}

spectate::SpectateSnapshot SpectateManager::captureSnapshot() {
    spectate::SpectateSnapshot s;

    if (player.isValid()) {
        s.x = (float)player.getPosX();
        s.y = (float)player.getPosY();
        s.z = (float)player.getPosZ();
        s.yaw = player.getYaw();
        s.pitch = player.getPitch();
        s.health = player.getHealth();
    }
    s.originX = (int32_t)std::floor(s.x);
    s.originY = (int32_t)std::floor(s.y);
    s.originZ = (int32_t)std::floor(s.z);

    
    for (const auto& p : mc.getPlayerList()) {
        if (s.entities.size() >= spectate::kMaxEntities)
            break;
        spectate::SpectateEntity e;
        e.type = 0;
        e.x = (float)p.posX;
        e.y = (float)p.posY;
        e.z = (float)p.posZ;
        e.name = p.name;
        s.entities.push_back(std::move(e));
    }
    
    for (const auto& it : mc.getItemEntityList()) {
        if (s.entities.size() >= spectate::kMaxEntities)
            break;
        spectate::SpectateEntity e;
        e.type = 1;
        e.x = (float)it.posX;
        e.y = (float)it.posY;
        e.z = (float)it.posZ;
        e.itemId = (uint16_t)it.itemId;
        s.entities.push_back(std::move(e));
    }

    
    
    
    
    const auto now = clock::now();
    if (cachedBlocks_.empty() || now - lastBlockCapture_ >= std::chrono::milliseconds(1000)) {
        lastBlockCapture_ = now;
        blockOriginX_ = (int32_t)std::floor(s.x);
        blockOriginY_ = (int32_t)std::floor(s.y);
        blockOriginZ_ = (int32_t)std::floor(s.z);
        std::vector<BlockSample> blocks;
        mc.captureBlocks(16, 16, blockOriginX_, blockOriginY_, blockOriginZ_, spectate::kMaxBlocks, blocks);
        cachedBlocks_.clear();
        cachedBlocks_.reserve(blocks.size());
        for (const auto& b : blocks) {
            spectate::SpectateBlock sb;
            sb.dx = b.dx;
            sb.dy = b.dy;
            sb.dz = b.dz;
            sb.id = b.id;
            cachedBlocks_.push_back(sb);
        }
    }
    
    
    s.originX = blockOriginX_;
    s.originY = blockOriginY_;
    s.originZ = blockOriginZ_;
    s.blocks = cachedBlocks_;
    return s;
}

void SpectateManager::tick() {
    const bool host = allowSpectate_;
    const bool spec = spectating_;
    if (!host && !spec)
        return;

    std::string hwid, token;
    if (!session(hwid, token))
        return;

    const auto now = clock::now();

    if (host) {
        
        
        if (now - lastPresence_ >= std::chrono::milliseconds(1000)) {
            lastPresence_ = now;
            std::string activity = mc.isValid() ? mc.getLocalPlayerName() : std::string();
            heartbeatPresenceAsync(activity);
        }
        if (spectatorCount_ > 0 && !spec && now - lastUpload_ >= std::chrono::milliseconds(200)) {
            lastUpload_ = now;
            uploadFrameAsync(spectate::encode(captureSnapshot()));
        }
    }

    if (spec) {
        const std::string target = currentTarget();
        if (!target.empty() && now - lastFetch_ >= std::chrono::milliseconds(200)) {
            lastFetch_ = now;
            fetchFrameAsync(target);
        }
    }
}
