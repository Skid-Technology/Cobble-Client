#include "pings.h"
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/visual_cache.h"
#include "../../sdk/player/render_data.h"
#include "../../security/session_validator.hpp"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/team/friends_manager.h"


#ifndef VK_X
#define VK_X 0x58
#endif

Pings* pingsModule = nullptr;

namespace {
bool RayIntersectsPlayerBox(double eyeX, double eyeY, double eyeZ, double dirX, double dirY, double dirZ,
                            double playerX, double playerY, double playerZ, double maxDistance,
                            double& hitDistance) {
    
    
    constexpr double horizontalRadius = 0.30 + 0.08;
    constexpr double verticalPadding = 0.08;

    const double boundsMin[3] = {playerX - horizontalRadius, playerY - verticalPadding,
                                 playerZ - horizontalRadius};
    const double boundsMax[3] = {playerX + horizontalRadius, playerY + 1.80 + verticalPadding,
                                 playerZ + horizontalRadius};
    const double origin[3] = {eyeX, eyeY, eyeZ};
    const double direction[3] = {dirX, dirY, dirZ};

    double nearDistance = 0.0;
    double farDistance = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1.0e-8) {
            if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis])
                return false;
            continue;
        }

        double axisNear = (boundsMin[axis] - origin[axis]) / direction[axis];
        double axisFar = (boundsMax[axis] - origin[axis]) / direction[axis];
        if (axisNear > axisFar)
            std::swap(axisNear, axisFar);

        nearDistance = (std::max)(nearDistance, axisNear);
        farDistance = (std::min)(farDistance, axisFar);
        if (nearDistance > farDistance)
            return false;
    }

    if (farDistance < 0.0 || nearDistance > maxDistance)
        return false;

    hitDistance = nearDistance;
    return true;
}
}  

Pings::Pings() : Module("Pings") {
    keybind = 0; 
    renders = true;
    worldVisual = true;
    usesPlayerCache = true;
    enabled = true; 
}

void Pings::saveSettings(json::Value& node) const {
    node["pingKeybind"] = settings.pingKeybind;
    node["pingScale"] = (double)settings.pingScale;
    json::writeColor(node, "locationColor", settings.locationColor);
    json::writeColor(node, "chamsColor", settings.chamsColor);
}

void Pings::loadSettings(const json::Value& node) {
    settings.pingKeybind = node["pingKeybind"].as_int(settings.pingKeybind);
    settings.pingScale = (float)node["pingScale"].as_double((double)settings.pingScale);
    json::readColor(node, "locationColor", settings.locationColor);
    json::readColor(node, "chamsColor", settings.chamsColor);
}

void Pings::onEnable() {
    log_ok("Pings module enabled");
    lastFetchTime = 0;
    keyWasDown = false;
    {
        std::lock_guard<std::mutex> lock(pingsMutex);
        activePings.clear();
    }
}

void Pings::onDisable() {
    log_ok("Pings module disabled");
    {
        std::lock_guard<std::mutex> lock(pingsMutex);
        activePings.clear();
    }
}

void Pings::onUpdate() {
    if (!player.isValid() || !mc.isValid())
        return;

    
    bool keyDown = (GetAsyncKeyState(settings.pingKeybind) & 0x8000) != 0;
    if (keyDown) {
        if (!keyWasDown && !mc.isInGui()) {
            keyWasDown = true;
            long long nowMs = (long long)GetTickCount64();
            if (nowMs - lastPingSentTime >= 500) {
                lastPingSentTime = nowMs;

                
                double eyeX = player.getPosX();
                double eyeY = player.getPosY() + (player.isSneaking() ? 1.54 : 1.62);
                double eyeZ = player.getPosZ();

                float yaw = player.getYaw();
                float pitch = player.getPitch();

                float yawRad = yaw * (3.1415926535f / 180.0f);
                float pitchRad = pitch * (3.1415926535f / 180.0f);

                float dirX = -std::sin(yawRad) * std::cos(pitchRad);
                float dirY = -std::sin(pitchRad);
                float dirZ = std::cos(yawRad) * std::cos(pitchRad);

                
                float maxDist = 150.0f;
                float stepSize = 0.1f;
                int steps = (int)(maxDist / stepSize);
                bool hitBlock = false;
                double hitBlockX = 0, hitBlockY = 0, hitBlockZ = 0;
                double blockDist = maxDist;

                for (int i = 0; i < steps; i++) {
                    double cx = eyeX + dirX * (i * stepSize);
                    double cy = eyeY + dirY * (i * stepSize);
                    double cz = eyeZ + dirZ * (i * stepSize);

                    int bx = (int)std::floor(cx);
                    int by = (int)std::floor(cy);
                    int bz = (int)std::floor(cz);

                    int blockId = mc.getBlockId(bx, by, bz);
                    if (blockId != 0) {
                        hitBlock = true;
                        hitBlockX = bx;
                        hitBlockY = by;
                        hitBlockZ = bz;
                        blockDist = i * stepSize;
                        break;
                    }
                }

                
                auto snap = VisualCache::GetSnapshot();
                std::string localName = mc.getLocalPlayerName();
                std::string cleanLocalName = FriendsManager::stripColorCodes(localName);

                bool hitPlayer = false;
                std::string hitPlayerUUID;
                double hitPlayerX = 0, hitPlayerY = 0, hitPlayerZ = 0;
                double bestPlayerDist = blockDist; 

                if (snap.valid) {
                    for (const auto& p : snap.players) {
                        if (p.uuid.empty())
                            continue; 

                        std::string cleanPName = FriendsManager::stripColorCodes(p.profileName.empty() ? p.name : p.profileName);
                        if (cleanPName == cleanLocalName)
                            continue; 

                        double hitDistance = 0.0;
                        if (!RayIntersectsPlayerBox(eyeX, eyeY, eyeZ, dirX, dirY, dirZ, p.posX, p.posY,
                                                    p.posZ, bestPlayerDist, hitDistance))
                            continue;

                        hitPlayer = true;
                        hitPlayerUUID = p.uuid;
                        hitPlayerX = p.posX;
                        hitPlayerY = p.posY;
                        hitPlayerZ = p.posZ;
                        bestPlayerDist = hitDistance;
                    }
                }

                if (hitPlayer) {
                    
                    sendPing(1, hitPlayerX, hitPlayerY + 2.2, hitPlayerZ, hitPlayerUUID);
                    log_ok("[Pings] Aim targeted player UUID: %s", hitPlayerUUID.c_str());
                } else if (hitBlock) {
                    sendPing(0, hitBlockX + 0.5, hitBlockY + 1.0, hitBlockZ + 0.5, "");
                    log_debug("[Pings] Sent Location ping via raycast at: %.1f, %.1f, %.1f", hitBlockX, hitBlockY, hitBlockZ);
                }
            }
        }
    } else {
        keyWasDown = false;
    }

    
    long long now = GetTickCount64();
    if (now - lastFetchTime >= 1000) {
        lastFetchTime = now;
        fetchPings();
    }
}

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static std::vector<uint8_t> b64decode(const std::string& in) {
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

static std::string normalizeIP(const std::string& ip) {
    std::string normalized = ip;
    
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    size_t colon = normalized.find(':');
    if (colon != std::string::npos) {
        normalized = normalized.substr(0, colon);
    }
    
    for (const std::string& prefix : {"play.", "mc.", "us.", "eu.", "server."}) {
        if (normalized.rfind(prefix, 0) == 0) { 
            normalized = normalized.substr(prefix.length());
        }
    }
    return normalized;
}

static bool parseEnvelope(const std::string& body, json::Value& out) {
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

void Pings::sendPing(int type, double x, double y, double z, const std::string& targetPlayer) {
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    std::string server = normalizeIP(mc.getCurrentServerIP());

    if (hwid.empty() || token.empty() || server.empty()) {
        log_debug("[Pings] sendPing aborted: hwid.empty=%d, token.empty=%d, server.empty=%d", 
                  (int)hwid.empty(), (int)token.empty(), (int)server.empty());
        return;
    }

    std::string sessionUser = SessionValidator::snapshotToken(SessionValidator::g_Username);
    std::string localName = mc.getLocalPlayerName();

    
    ActivePing localPing;
    localPing.sender = sessionUser.empty() ? localName : sessionUser;
    localPing.type = type;
    localPing.x = x;
    localPing.y = y;
    localPing.z = z;
    localPing.targetPlayer = targetPlayer;
    localPing.timestamp = (long long)GetTickCount64();

    {
        std::lock_guard<std::mutex> lock(pingsMutex);
        
        activePings.erase(std::remove_if(activePings.begin(), activePings.end(), [&](const ActivePing& p) {
            std::string cleanP = FriendsManager::stripColorCodes(p.sender);
            return cleanP == sessionUser || cleanP == FriendsManager::stripColorCodes(localName);
        }), activePings.end());
        activePings.push_back(localPing);
    }

    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) +
                       "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"server\":\"" + http::json_escape(server) + "\"";
    body += ",\"type\":" + std::to_string(type) +
            ",\"x\":" + std::to_string(x) +
            ",\"y\":" + std::to_string(y) +
            ",\"z\":" + std::to_string(z) +
            ",\"targetPlayer\":\"" + http::json_escape(targetPlayer) + "\"" +
            ",\"mcName\":\"" + http::json_escape(localName) + "\"}";

    std::thread([body]() {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/pings/create").c_str(), body,
                                      cobble_config::SERVER_PORT);
        if (!resp.success) {
            log_debug("[Pings] sendPing HTTP request failed: %s", resp.error.c_str());
        } else {
            json::Value payload;
            if (parseEnvelope(resp.body, payload)) {
                log_debug("[Pings] sendPing response (decoded): %s", payload.serialize().c_str());
            } else {
                log_debug("[Pings] sendPing response (raw): %s", resp.body.c_str());
            }
        }
    }).detach();
}

void Pings::fetchPings() {
    std::string hwid = SessionValidator::snapshotToken(SessionValidator::g_HWID);
    std::string token = SessionValidator::snapshotToken(SessionValidator::g_SessionToken);
    std::string server = normalizeIP(mc.getCurrentServerIP());

    if (hwid.empty() || token.empty() || server.empty()) {
        log_debug("[Pings] fetchPings aborted: hwid.empty=%d, token.empty=%d, server.empty=%d", 
                  (int)hwid.empty(), (int)token.empty(), (int)server.empty());
        return;
    }

    std::string sessionUser = SessionValidator::snapshotToken(SessionValidator::g_Username);
    std::string localName = mc.getLocalPlayerName();

    std::string body = "{\"hwid\":\"" + http::json_escape(hwid) +
                       "\",\"token\":\"" + http::json_escape(token) +
                       "\",\"server\":\"" + http::json_escape(server) + "\"}";

    std::thread([this, body, sessionUser, localName]() {
        auto resp = http::post_cobble(OBFUSCATE_WSTRING(L"/api/pings/active").c_str(), body,
                                      cobble_config::SERVER_PORT);
        if (!resp.success) {
            log_debug("[Pings] fetchPings HTTP request failed: %s", resp.error.c_str());
            return;
        }

        json::Value payload;
        if (parseEnvelope(resp.body, payload)) {
            bool success = payload["success"].as_bool(false);
            if (success) {
                std::vector<ActivePing> fetchedPings;
                if (payload.has("pings") && payload["pings"].is_array()) {
                    const auto& array = payload["pings"].array();
                    for (const auto& item : array) {
                        ActivePing p;
                        
                        p.sender = item["mcName"].as_string("");
                        if (p.sender.empty()) {
                            p.sender = item["sender"].as_string("");
                        }
                        p.type = item["type"].as_int(0);
                        p.x = item["x"].as_double(0.0);
                        p.y = item["y"].as_double(0.0);
                        p.z = item["z"].as_double(0.0);
                        p.targetPlayer = item["targetPlayer"].as_string("");
                        p.timestamp = item["timestamp"].as_int(0);
                        fetchedPings.push_back(p);
                        
                        log_debug("[Pings] Fetched ping: sender=%s, type=%d, x_int=%d, y_int=%d, z_int=%d",
                                  p.sender.c_str(), p.type, (int)p.x, (int)p.y, (int)p.z);
                    }
                }

                
                std::lock_guard<std::mutex> lock(pingsMutex);
                long long now = (long long)GetTickCount64();

                for (const auto& oldPing : activePings) {
                    std::string cleanOldSender = FriendsManager::stripColorCodes(oldPing.sender);
                    if (cleanOldSender == sessionUser || cleanOldSender == FriendsManager::stripColorCodes(localName)) {
                        
                        if (now - oldPing.timestamp < 3500) {
                            bool alreadyInFetched = false;
                            for (const auto& fp : fetchedPings) {
                                std::string cleanFP = FriendsManager::stripColorCodes(fp.sender);
                                if ((cleanFP == sessionUser || cleanFP == FriendsManager::stripColorCodes(localName)) &&
                                    fp.type == oldPing.type &&
                                    (fp.type == 0 ? (abs(fp.x - oldPing.x) < 0.1 && abs(fp.z - oldPing.z) < 0.1)
                                                  : (fp.targetPlayer == oldPing.targetPlayer))) {
                                    alreadyInFetched = true;
                                    break;
                                }
                            }
                            if (!alreadyInFetched) {
                                fetchedPings.push_back(oldPing);
                            }
                        }
                    }
                }

                activePings = std::move(fetchedPings);
            } else {
                log_debug("[Pings] fetchPings success field is false. Body: %s", resp.body.c_str());
            }
        } else {
            log_debug("[Pings] fetchPings failed to parse JSON response. Body: %s", resp.body.c_str());
        }
    }).detach();
}


static void Draw3DBoxHelper(ImDrawList* draw, float x, float y, float z, double renderX, double renderY, double renderZ, ImU32 boxCol, ImU32 outlineCol) {
    static constexpr float HALF_WIDTH = 0.3f;
    static constexpr float HEIGHT = 1.8f;

    Vector3 corners[8] = {
        {x - HALF_WIDTH, y, z - HALF_WIDTH},
        {x + HALF_WIDTH, y, z - HALF_WIDTH},
        {x + HALF_WIDTH, y, z + HALF_WIDTH},
        {x - HALF_WIDTH, y, z + HALF_WIDTH},
        {x - HALF_WIDTH, y + HEIGHT, z - HALF_WIDTH},
        {x + HALF_WIDTH, y + HEIGHT, z - HALF_WIDTH},
        {x + HALF_WIDTH, y + HEIGHT, z + HALF_WIDTH},
        {x - HALF_WIDTH, y + HEIGHT, z + HALF_WIDTH}
    };

    Vector2 screenCorners[8];
    bool visible[8];
    for (int i = 0; i < 8; i++) {
        Vector3 v = {corners[i].x - (float)renderX, corners[i].y - (float)renderY, corners[i].z - (float)renderZ};
        visible[i] = CWorldToScreen::WorldToScreen(v, RenderData::modelView, RenderData::projection,
                                                   RenderData::screenWidth, RenderData::screenHeight, screenCorners[i]);
    }

    static constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    
    for (int i = 0; i < 12; i++) {
        int a = edges[i][0], b = edges[i][1];
        if (visible[a] && visible[b]) {
            draw->AddLine(ImVec2(screenCorners[a].x, screenCorners[a].y),
                          ImVec2(screenCorners[b].x, screenCorners[b].y), outlineCol, 3.5f);
        }
    }

    
    for (int i = 0; i < 12; i++) {
        int a = edges[i][0], b = edges[i][1];
        if (visible[a] && visible[b]) {
            draw->AddLine(ImVec2(screenCorners[a].x, screenCorners[a].y),
                          ImVec2(screenCorners[b].x, screenCorners[b].y), boxCol, 1.5f);
        }
    }
}



static bool GetFocusScreenBounds(float x, float y, float z, double renderX, double renderY, double renderZ,
                                 ImVec2& outMin, ImVec2& outMax) {
    static constexpr float HALF_WIDTH = 0.34f;
    static constexpr float HEIGHT = 1.86f;
    const Vector3 corners[8] = {
        {x - HALF_WIDTH, y, z - HALF_WIDTH}, {x + HALF_WIDTH, y, z - HALF_WIDTH},
        {x + HALF_WIDTH, y, z + HALF_WIDTH}, {x - HALF_WIDTH, y, z + HALF_WIDTH},
        {x - HALF_WIDTH, y + HEIGHT, z - HALF_WIDTH}, {x + HALF_WIDTH, y + HEIGHT, z - HALF_WIDTH},
        {x + HALF_WIDTH, y + HEIGHT, z + HALF_WIDTH}, {x - HALF_WIDTH, y + HEIGHT, z + HALF_WIDTH}
    };

    float minX = 100000.0f, minY = 100000.0f;
    float maxX = -100000.0f, maxY = -100000.0f;
    int projectedCorners = 0;
    for (const Vector3& corner : corners) {
        Vector2 screen;
        Vector3 relative = {corner.x - (float)renderX, corner.y - (float)renderY, corner.z - (float)renderZ};
        if (!CWorldToScreen::WorldToScreen(relative, RenderData::modelView, RenderData::projection,
                                           RenderData::screenWidth, RenderData::screenHeight, screen)) {
            continue;
        }

        minX = (std::min)(minX, screen.x);
        minY = (std::min)(minY, screen.y);
        maxX = (std::max)(maxX, screen.x);
        maxY = (std::max)(maxY, screen.y);
        ++projectedCorners;
    }

    if (projectedCorners < 2 || maxX - minX < 4.0f || maxY - minY < 8.0f)
        return false;

    outMin = ImVec2(minX, minY);
    outMax = ImVec2(maxX, maxY);
    return true;
}

static void DrawFocusOutline(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 color) {
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float padding = (std::max)(3.0f, (std::min)(width, height) * 0.06f);
    min.x -= padding;
    min.y -= padding;
    max.x += padding;
    max.y += padding;

    const float rounding = (std::min)(5.0f, (std::min)(width, height) * 0.12f);
    const ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);
    const auto alpha = [&](float multiplier) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(base.x, base.y, base.z, base.w * multiplier));
    };

    draw->AddRect(min, max, alpha(0.10f), rounding, 0, 7.0f);
    draw->AddRect(min, max, alpha(0.25f), rounding, 0, 3.5f);
    draw->AddRect(min, max, alpha(0.95f), rounding, 0, 1.25f);
}


static void DrawMapsPin(ImDrawList* draw, ImVec2 pos, ImU32 color, float userScale = 1.0f) {
    float scale = 1.8f * userScale;
    float radius = 10.0f * scale;
    float holeRadius = 4.0f * scale;
    ImVec2 center = ImVec2(pos.x, pos.y - radius - 8.0f * scale);
    float tipY = pos.y;

    
    for (int i = 3; i >= 1; i--) {
        float r = radius + (float)i * 2.0f;
        draw->AddCircleFilled(center, r, IM_COL32(0, 0, 0, 20 * i), 32);
    }

    
    draw->AddCircleFilled(center, radius, color, 32);

    
    float triHalf = 6.5f * scale;
    ImVec2 p1 = ImVec2(pos.x - triHalf, center.y + radius * 0.45f);
    ImVec2 p2 = ImVec2(pos.x + triHalf, center.y + radius * 0.45f);
    ImVec2 p3 = ImVec2(pos.x, tipY);
    draw->AddTriangleFilled(p1, p2, p3, color);

    
    draw->AddCircleFilled(center, holeRadius, IM_COL32(15, 15, 15, 240), 32);

    
    ImU32 highlight = IM_COL32(255, 255, 255, 40);
    draw->AddCircleFilled(ImVec2(center.x - radius * 0.25f, center.y - radius * 0.25f), radius * 0.5f, highlight, 32);
}

void Pings::onRender() {
    if (!RenderData::updated || activePings.empty())
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<ActivePing> pingsCopy;
    {
        std::lock_guard<std::mutex> lock(pingsMutex);
        pingsCopy = activePings;
    }

    RenderPos renderPos = mc.getRenderPos();
    double renderX = renderPos.x;
    double renderY = renderPos.y;
    double renderZ = renderPos.z;

    auto snap = VisualCache::GetSnapshot();
    if (!snap.valid)
        return;

    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();

    
    ImU32 pinColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(settings.locationColor[0], settings.locationColor[1], settings.locationColor[2], settings.locationColor[3]));

    std::string localName = mc.getLocalPlayerName();
    std::string sessionUser = SessionValidator::snapshotToken(SessionValidator::g_Username); 

    for (const auto& ping : pingsCopy) {
        
        bool isSelf = false;
        bool isFriend = false;
        std::string cleanSender = FriendsManager::stripColorCodes(ping.sender);

        
        if (cleanSender == sessionUser || cleanSender == FriendsManager::stripColorCodes(localName)) {
            isSelf = true;
        } else if (friendsManager) {
            if (friendsManager->isFriendByUUID(ping.sender) || friendsManager->isFriend(cleanSender) ||
                friendsManager->isScoreboardTeammate(localName, cleanSender)) {
                isFriend = true;
            } else {
                
                for (const auto& p : snap.players) {
                    std::string cleanPName = FriendsManager::stripColorCodes(p.profileName.empty() ? p.name : p.profileName);
                    if (cleanPName == cleanSender) {
                        if (friendsManager->isApolloTeammate(p.uuid) || friendsManager->isLunarTeammate(p.uuid) || friendsManager->isVisualFriend(p.uuid)) {
                            isFriend = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!isSelf && !isFriend)
            continue;

        if (ping.type == 0) {
            
            Vector3 relativePos = { (float)(ping.x - renderX), (float)(ping.y - renderY), (float)(ping.z - renderZ) };
            Vector2 screenPos;
            bool onScreen = CWorldToScreen::WorldToScreen(relativePos, RenderData::modelView, RenderData::projection,
                                                          RenderData::screenWidth, RenderData::screenHeight, screenPos);

            if (onScreen) {
                
                DrawMapsPin(draw, ImVec2(screenPos.x, screenPos.y), pinColor, settings.pingScale);
            }
        }
        else if (ping.type == 1) {
            const ImU32 focusColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(settings.chamsColor[0], settings.chamsColor[1], settings.chamsColor[2], settings.chamsColor[3]));
            for (const auto& p : snap.players) {
                if (p.uuid != ping.targetPlayer)
                    continue;

                const float targetX = (float)(p.lastPosX + (p.posX - p.lastPosX) * snap.partialTicks);
                const float targetY = (float)(p.lastPosY + (p.posY - p.lastPosY) * snap.partialTicks);
                const float targetZ = (float)(p.lastPosZ + (p.posZ - p.lastPosZ) * snap.partialTicks);
                ImVec2 boundsMin, boundsMax;
                if (GetFocusScreenBounds(targetX, targetY, targetZ, renderX, renderY, renderZ, boundsMin, boundsMax)) {
                    DrawFocusOutline(draw, boundsMin, boundsMax, focusColor);
                }
                break;
            }
        }
    }
}

void Pings::onRenderMenu() {
    ImGui::TextDisabled("Look at a block and press X to ping");
    ImGui::Separator();
    
    
    auto getKeyName = [](int vk) -> std::string {
        switch (vk) {
            case 0x04: return "Mouse3";   
            case 0x05: return "Mouse4";   
            case 0x06: return "Mouse5";   
            default: {
                char name[32] = {};
                UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
                if (scanCode) {
                    LONG lParam = (scanCode << 16);
                    if (GetKeyNameTextA(lParam, name, sizeof(name)) > 0)
                        return std::string(name);
                }
                
                if (vk >= 0x30 && vk <= 0x5A)
                    return std::string(1, (char)vk);
                return "0x" + std::to_string(vk);
            }
        }
    };

    ImGui::Text("Keybind:");
    ImGui::SameLine();
    static bool waitingForKey = false;
    std::string btnText = waitingForKey ? "Press any key/mouse..." : "[" + getKeyName(settings.pingKeybind) + "]";
    if (ImGui::Button(btnText.c_str(), ImVec2(160, 0))) {
        waitingForKey = true;
    }
    if (waitingForKey) {
        
        for (int mb = 0x04; mb <= 0x06; mb++) {
            if (GetAsyncKeyState(mb) & 0x8000) {
                settings.pingKeybind = mb;
                waitingForKey = false;
                break;
            }
        }
        
        if (waitingForKey) {
            for (int key = 8; key < 255; key++) {
                if (GetAsyncKeyState(key) & 0x8000) {
                    settings.pingKeybind = key;
                    waitingForKey = false;
                    break;
                }
            }
        }
    }

    ImGui::Spacing();

    ImGui::TextDisabled("Player pings use a clean violet outline without an extra nametag.");

    ImGui::Spacing();

    
    ImGui::Text("Ping Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::SliderFloat("##pingScale", &settings.pingScale, 0.3f, 3.0f, "%.1fx");

    ImGui::Spacing();

    
    ImGui::ColorEdit3("Ping Color", settings.locationColor, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Outline Color", settings.chamsColor, ImGuiColorEditFlags_NoInputs);
}

std::string Pings::getDisplayInfo() {
    return "";
}

void Pings::onRender3D() {
    
    
}
