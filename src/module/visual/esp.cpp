#include "esp.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/visual_cache.h"
#include "../../sdk/player/render_data.h"
#include "../../util/health_predictor.hpp"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/diagnostics.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"

ESP* esp = nullptr;

ESP::ESP() : Module("ESP") {
    keybind = 0;
    renders = true;
    worldVisual = true;
    usesPlayerCache = true;
}

void ESP::saveSettings(json::Value& node) const {
    node["box"] = settings.box;
    node["box3D"] = settings.box3D;
    node["filled"] = settings.filled;
    node["outline"] = settings.outline;
    node["healthBar"] = settings.healthBar;
    node["teamColors"] = settings.teamColors;
    node["range"] = settings.range;
    json::writeColor(node, "boxColor", settings.boxColor);
    json::writeColor(node, "filledColor", settings.filledColor);
    json::writeColor(node, "outlineColor", settings.outlineColor);
    json::writeColor(node, "friendColor", settings.friendColor);
    json::writeColor(node, "enemyColor", settings.enemyColor);
}

void ESP::loadSettings(const json::Value& node) {
    settings.box = node["box"].as_bool(settings.box);
    settings.box3D = node["box3D"].as_bool(settings.box3D);
    settings.filled = node["filled"].as_bool(settings.filled);
    settings.outline = node["outline"].as_bool(settings.outline);
    settings.healthBar = node["healthBar"].as_bool(settings.healthBar);
    settings.teamColors = node["teamColors"].as_bool(settings.teamColors);
    settings.range = node["range"].as_float(settings.range);
    json::readColor(node, "boxColor", settings.boxColor);
    json::readColor(node, "filledColor", settings.filledColor);
    json::readColor(node, "outlineColor", settings.outlineColor);
    json::readColor(node, "friendColor", settings.friendColor);
    json::readColor(node, "enemyColor", settings.enemyColor);
}

void ESP::onEnable() {
    log_ok("ESP enabled");
    lastWorldRef = nullptr;

    
    RenderData::initialized = false;
    RenderData::Reset();

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedPlayers.clear();
    smoothedPlayers.clear();
    lastRenderTime = 0;
}

void ESP::onDisable() {
    log_ok("ESP disabled");
    lastWorldRef = nullptr;  

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedPlayers.clear();
    smoothedPlayers.clear();
    lastRenderTime = 0;
}

void ESP::checkWorldChange() {
    if (!Java::Env() || !Java::Initialized)
        return;

    jobject currentWorld = mc.getWorld();
    bool worldChanged = false;

    if (lastWorldRef == nullptr && currentWorld != nullptr)
        worldChanged = true;
    else if (lastWorldRef != nullptr && currentWorld == nullptr)
        worldChanged = true;
    else if (lastWorldRef != nullptr && currentWorld != nullptr) {
        if (!Java::Env()->IsSameObject(lastWorldRef, currentWorld))
            worldChanged = true;
    }

    if (worldChanged) {
        
        Diagnostics::beginWorldChange();

        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cachedPlayers.clear();
        }

        RenderData::initialized = false;
        RenderData::Reset();

        if (lastWorldRef) {
            Java::Env()->DeleteGlobalRef(lastWorldRef);
            lastWorldRef = nullptr;
        }

        if (currentWorld) {
            lastWorldRef = Java::Env()->NewGlobalRef(currentWorld);
        }

        Diagnostics::endWorldChange();
    }

    if (currentWorld)
        Java::Env()->DeleteLocalRef(currentWorld);
}

void ESP::onUpdate() {
    lastUpdateTick = GetTickCount64();

    

    if (!player.isValid()) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedPlayers.clear();
        return;
    }

    auto snap = VisualCache::GetSnapshot();
    if (!snap.valid) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedPlayers.clear();
        return;
    }

    RenderPos renderPos = snap.renderPos;
    float partialTicks = snap.partialTicks;

    double localX = snap.localX;
    double localY = snap.localY;
    double localZ = snap.localZ;

    auto& players = snap.players;
    std::vector<EspPlayer> newCache;

    for (auto& p : players) {
        double dx = p.posX - localX;
        double dy = p.posY - localY;
        double dz = p.posZ - localZ;
        float dist = (float)std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist > settings.range || dist < 0.5f)
            continue;

        EspPlayer cached;
        cached.entityId = p.entityId;
        cached.name = p.name;
        cached.profileName = p.profileName;
        cached.uuid = p.uuid;            
        cached.teamColor = p.teamColor;  
        cached.isOnLocalTeam = p.isOnLocalTeam;
        cached.posX = p.posX;
        cached.posY = p.posY;
        cached.posZ = p.posZ;
        cached.lastPosX = p.lastPosX;
        cached.lastPosY = p.lastPosY;
        cached.lastPosZ = p.lastPosZ;
        cached.distance = dist;
        cached.health = p.health;
        cached.maxHealth = p.maxHealth;
        cached.valid = true;

        newCache.push_back(cached);
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedPlayers = std::move(newCache);
        cachedRenderX = renderPos.x;
        cachedRenderY = renderPos.y;
        cachedRenderZ = renderPos.z;
        cachedPartialTicks = partialTicks;
        cachedLocalX = localX;
        cachedLocalY = localY;
        cachedLocalZ = localZ;
        cachedLocalLastX = snap.localLastX;
        cachedLocalLastY = snap.localLastY;
        cachedLocalLastZ = snap.localLastZ;
    }
}


static ImU32 GetHealthColor(float health, float maxHealth) {
    if (maxHealth <= 0.0f)
        maxHealth = 1.0f;
    float percent = health / maxHealth;
    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 1.0f)
        percent = 1.0f;

    int r, g;
    if (percent > 0.5f) {
        r = (int)((1.0f - percent) * 2.0f * 255.0f);
        g = 255;
    } else {
        r = 255;
        g = (int)(percent * 2.0f * 255.0f);
    }

    return IM_COL32(r, g, 0, 255);
}


static void Draw3DLine(ImDrawList* draw, const Vector3& p1, const Vector3& p2, double renderX, double renderY,
                       double renderZ, ImU32 color, float thickness = 1.5f) {
    Vector3 v1 = {p1.x - (float)renderX, p1.y - (float)renderY, p1.z - (float)renderZ};
    Vector3 v2 = {p2.x - (float)renderX, p2.y - (float)renderY, p2.z - (float)renderZ};

    Vector2 s1, s2;
    bool vis1 = CWorldToScreen::WorldToScreen(v1, RenderData::modelView, RenderData::projection,
                                              RenderData::screenWidth, RenderData::screenHeight, s1);
    bool vis2 = CWorldToScreen::WorldToScreen(v2, RenderData::modelView, RenderData::projection,
                                              RenderData::screenWidth, RenderData::screenHeight, s2);

    if (vis1 && vis2) {
        draw->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), color, thickness);
    }
}


static void Render3DBox(ImDrawList* draw, float x, float y, float z, double renderX, double renderY, double renderZ,
                        ImU32 boxCol, ImU32 outlineCol, ImU32 fillCol, bool drawOutline, bool drawFilled) {
    
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
        {x - HALF_WIDTH, y + HEIGHT, z + HALF_WIDTH},  
    };

    
    Vector2 screenCorners[8];
    bool visible[8];
    int visibleCount = 0;
    for (int i = 0; i < 8; i++) {
        Vector3 v = {corners[i].x - (float)renderX, corners[i].y - (float)renderY, corners[i].z - (float)renderZ};
        visible[i] = CWorldToScreen::WorldToScreen(v, RenderData::modelView, RenderData::projection,
                                                   RenderData::screenWidth, RenderData::screenHeight, screenCorners[i]);
        if (visible[i])
            visibleCount++;
    }

    
    if (visibleCount < 3)
        return;

    
    
    static constexpr int faces[6][4] = {
        {0, 1, 2, 3},  
        {7, 6, 5, 4},  
        {3, 2, 6, 7},  
        {1, 0, 4, 5},  
        {0, 3, 7, 4},  
        {2, 1, 5, 6},  
    };

    
    if (drawFilled) {
        for (int f = 0; f < 6; f++) {
            int i0 = faces[f][0], i1 = faces[f][1], i2 = faces[f][2], i3 = faces[f][3];

            
            if (!visible[i0] || !visible[i1] || !visible[i2] || !visible[i3])
                continue;

            
            
            float ax = screenCorners[i1].x - screenCorners[i0].x;
            float ay = screenCorners[i1].y - screenCorners[i0].y;
            float bx = screenCorners[i2].x - screenCorners[i0].x;
            float by = screenCorners[i2].y - screenCorners[i0].y;
            float cross = ax * by - ay * bx;

            
            if (cross > 0) {
                ImVec2 points[4] = {
                    ImVec2(screenCorners[i0].x, screenCorners[i0].y),
                    ImVec2(screenCorners[i1].x, screenCorners[i1].y),
                    ImVec2(screenCorners[i2].x, screenCorners[i2].y),
                    ImVec2(screenCorners[i3].x, screenCorners[i3].y),
                };
                draw->AddConvexPolyFilled(points, 4, fillCol);
            }
        }
    }

    
    static constexpr int edges[12][2] = {
                                         {0, 1},
                                         {1, 2},
                                         {2, 3},
                                         {3, 0},
                                         
                                         {4, 5},
                                         {5, 6},
                                         {6, 7},
                                         {7, 4},
                                         
                                         {0, 4},
                                         {1, 5},
                                         {2, 6},
                                         {3, 7}};

    
    if (drawOutline) {
        for (int i = 0; i < 12; i++) {
            int a = edges[i][0], b = edges[i][1];
            if (visible[a] && visible[b]) {
                draw->AddLine(ImVec2(screenCorners[a].x, screenCorners[a].y),
                              ImVec2(screenCorners[b].x, screenCorners[b].y), outlineCol, 3.5f);
            }
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

void ESP::onRender() {
    if (!RenderData::updated)
        return;

    
    if (GetTickCount64() - lastUpdateTick > 300)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<EspPlayer> localCache;
    double renderX, renderY, renderZ;
    float partialTicks;
    double localX, localY, localZ;
    double localLastX, localLastY, localLastZ;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        localCache = cachedPlayers;
        renderX = cachedRenderX;
        renderY = cachedRenderY;
        renderZ = cachedRenderZ;
        partialTicks = cachedPartialTicks;
        localX = cachedLocalX;
        localY = cachedLocalY;
        localZ = cachedLocalZ;
        localLastX = cachedLocalLastX;
        localLastY = cachedLocalLastY;
        localLastZ = cachedLocalLastZ;
    }

    
    bool useFallback = (renderX == 0.0 && renderY == 0.0 && renderZ == 0.0);
    if (useFallback) {
        renderX = localLastX + (localX - localLastX) * partialTicks;
        renderY = localLastY + (localY - localLastY) * partialTicks + 1.62;
        renderZ = localLastZ + (localZ - localLastZ) * partialTicks;
    }

    
    auto outlineCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(settings.outlineColor[0], settings.outlineColor[1], settings.outlineColor[2], settings.outlineColor[3]));

    const long long now = GetTickCount64();
    const float frameSeconds =
        lastRenderTime > 0 ? (std::min)(0.05f, (float)(now - lastRenderTime) / 1000.0f) : 1.0f / 60.0f;
    lastRenderTime = now;
    const float positionAlpha = 1.0f - std::exp(-22.0f * frameSeconds);
    const float healthAlpha = 1.0f - std::exp(-12.0f * frameSeconds);

    for (const auto& p : localCache) {
        if (!p.valid)
            continue;

        
        
        
        
        
        ImU32 boxCol, fillCol;
        bool isFriendOrTeam = false;

        if (settings.teamColors && friendsManager && !p.uuid.empty()) {
            
            if (p.isOnLocalTeam || friendsManager->isLunarTeammate(p.uuid)) {
                isFriendOrTeam = true;
            }
            
            else if (friendsManager->isApolloTeammate(p.uuid)) {
                isFriendOrTeam = true;
            }
            
            else if (friendsManager->isFriendByUUID(p.uuid) ||
                     friendsManager->isFriend(FriendsManager::stripColorCodes(p.name))) {
                isFriendOrTeam = true;
            }
            
            else {
                static int localTeamColor = -1;
                static ULONGLONG lastColorRefresh = 0;
                ULONGLONG now = GetTickCount64();
                if (now - lastColorRefresh > 1000) {  
                    localTeamColor = mc.getLocalPlayerTeamColor();
                    lastColorRefresh = now;
                }

                ChatColor playerColor = static_cast<ChatColor>(p.teamColor);
                ChatColor localColor = static_cast<ChatColor>(localTeamColor);
                static std::string localPlayerName;
                if (localPlayerName.empty())
                    localPlayerName = mc.getLocalPlayerName();
                const std::string& scoreboardName = p.profileName.empty() ? p.name : p.profileName;
                if (friendsManager->isScoreboardTeammate(localPlayerName, scoreboardName) ||
                    friendsManager->isTeammate(p.uuid, playerColor, localColor)) {
                    isFriendOrTeam = true;
                }
            }
        }

        if (isFriendOrTeam) {
            
            boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(settings.friendColor[0], settings.friendColor[1],
                                                           settings.friendColor[2], settings.friendColor[3]));
            fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(settings.friendColor[0], settings.friendColor[1],
                                                            settings.friendColor[2], settings.filledColor[3]));
        } else if (settings.teamColors) {
            
            boxCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(settings.enemyColor[0], settings.enemyColor[1], settings.enemyColor[2], settings.enemyColor[3]));
            fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(settings.enemyColor[0], settings.enemyColor[1],
                                                            settings.enemyColor[2], settings.filledColor[3]));
        } else {
            
            boxCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(settings.boxColor[0], settings.boxColor[1], settings.boxColor[2], settings.boxColor[3]));
            fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(settings.filledColor[0], settings.filledColor[1],
                                                            settings.filledColor[2], settings.filledColor[3]));
        }

        const double targetX = p.lastPosX + (p.posX - p.lastPosX) * partialTicks;
        const double targetY = p.lastPosY + (p.posY - p.lastPosY) * partialTicks;
        const double targetZ = p.lastPosZ + (p.posZ - p.lastPosZ) * partialTicks;
        auto& smooth = smoothedPlayers[p.entityId];
        const double jumpX = targetX - smooth.x;
        const double jumpY = targetY - smooth.y;
        const double jumpZ = targetZ - smooth.z;
        const bool teleported = jumpX * jumpX + jumpY * jumpY + jumpZ * jumpZ > 36.0;

        if (!smooth.initialized || teleported) {
            smooth.x = targetX;
            smooth.y = targetY;
            smooth.z = targetZ;
            smooth.health = p.health;
            smooth.initialized = true;
        } else {
            smooth.x += (targetX - smooth.x) * positionAlpha;
            smooth.y += (targetY - smooth.y) * positionAlpha;
            smooth.z += (targetZ - smooth.z) * positionAlpha;
            smooth.health += (p.health - smooth.health) * healthAlpha;
        }
        smooth.lastSeen = now;

        const float interpX = (float)smooth.x;
        const float interpY = (float)smooth.y;
        const float interpZ = (float)smooth.z;

        
        if (settings.box3D) {
            Render3DBox(draw, interpX, interpY, interpZ, renderX, renderY, renderZ, boxCol, outlineCol, fillCol,
                        settings.outline, settings.filled);
        }

        
        bool needs2DBox = settings.box;
        bool needs2DFilled = settings.filled && !settings.box3D;  
        bool needs2DHealth = settings.healthBar;

        if (!needs2DBox && !needs2DFilled && !needs2DHealth)
            continue;

        constexpr float halfWidth = 0.3f;
        constexpr float playerHeight = 1.8f;
        Vector3 bounds[8] = {
            {interpX - halfWidth, interpY, interpZ - halfWidth},
            {interpX + halfWidth, interpY, interpZ - halfWidth},
            {interpX + halfWidth, interpY, interpZ + halfWidth},
            {interpX - halfWidth, interpY, interpZ + halfWidth},
            {interpX - halfWidth, interpY + playerHeight, interpZ - halfWidth},
            {interpX + halfWidth, interpY + playerHeight, interpZ - halfWidth},
            {interpX + halfWidth, interpY + playerHeight, interpZ + halfWidth},
            {interpX - halfWidth, interpY + playerHeight, interpZ + halfWidth}};
        float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
        int projected = 0;
        for (auto& point : bounds) {
            point.x -= (float)renderX;
            point.y -= (float)renderY;
            point.z -= (float)renderZ;
            Vector2 screen{};
            if (CWorldToScreen::WorldToScreen(point, RenderData::modelView, RenderData::projection,
                                              RenderData::screenWidth, RenderData::screenHeight, screen)) {
                left = (std::min)(left, screen.x);
                top = (std::min)(top, screen.y);
                right = (std::max)(right, screen.x);
                bottom = (std::max)(bottom, screen.y);
                ++projected;
            }
        }
        const float height = bottom - top;
        if (projected < 4 || height < 1.0f || right <= left)
            continue;

        
        if (settings.filled && !settings.box3D) {
            draw->AddRectFilled(ImVec2(left, top), ImVec2(right, bottom), fillCol, 3.0f);
        }

        
        if (settings.box) {
            if (settings.outline) {
                draw->AddRect(ImVec2(left - 1.0f, top - 1.0f), ImVec2(right + 1.0f, bottom + 1.0f), outlineCol,
                              3.0f, ImDrawFlags_None, 3.0f);
            }
            draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), boxCol, 3.0f, ImDrawFlags_None, 1.25f);
        }

        
        if (settings.healthBar) {
            float barWidth = 2.5f;
            float barDist = 4.0f;

            float fillHeight = height * (smooth.health / (std::max)(p.maxHealth, 1.0f));
            if (fillHeight > height)
                fillHeight = height;
            if (fillHeight < 0)
                fillHeight = 0;

            float barLeft = left - barDist - barWidth;
            float barRight = left - barDist;

            
            draw->AddRectFilled(ImVec2(barLeft - 1, top - 1), ImVec2(barRight + 1, bottom + 1), IM_COL32(0, 0, 0, 150),
                                4.0f);

            
            ImU32 hpColor = GetHealthColor(smooth.health, p.maxHealth);
            draw->AddRectFilled(ImVec2(barLeft, bottom - fillHeight), ImVec2(barRight, bottom), hpColor, 4.0f);
        }
    }

    for (auto it = smoothedPlayers.begin(); it != smoothedPlayers.end();) {
        if (now - it->second.lastSeen > 500)
            it = smoothedPlayers.erase(it);
        else
            ++it;
    }
}

void ESP::onRenderMenu() {
    
    bool prevBox2D = settings.box;
    bool prevBox3D = settings.box3D;
    bool enabledValue = enabled;

    ui::two_column(
        "Settings",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();
            ui::Checkbox("Box 2D", &settings.box);
            ui::Checkbox("Box 3D", &settings.box3D);
            ui::Checkbox("Filled", &settings.filled);
            ui::Checkbox("Outline", &settings.outline);
            ui::Checkbox("Health Bar", &settings.healthBar);
            ui::Checkbox("Team Colors", &settings.teamColors);
            ui::SliderFloat("Range", &settings.range, 16.0f, 256.0f, "%.0fm");
        },
        "Colors",
        [this]() {
            if (settings.teamColors) {
                ui::color("Friend Color", settings.friendColor);
                ui::color("Enemy Color", settings.enemyColor);
            } else {
                ui::color("Box Color", settings.boxColor);
            }
            ui::color("Fill Color", settings.filledColor);
            ui::color("Outline Color", settings.outlineColor);
        });

    
    if (settings.box && !prevBox2D) {
        settings.box3D = false;
    }
    if (settings.box3D && !prevBox3D) {
        settings.box = false;
    }
}

std::string ESP::getDisplayInfo() {
    if (settings.box3D)
        return "3D";
    if (settings.box)
        return "2D";
    return "";
}
