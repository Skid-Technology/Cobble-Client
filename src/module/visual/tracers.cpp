#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "tracers.h"
#include <algorithm>
#include <cmath>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/visual_cache.h"
#include "../../sdk/player/render_data.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"

Tracers* tracers = nullptr;

Tracers::Tracers() : Module("Tracers") {
    keybind = 0;
    renders = true;
    worldVisual = true;
    usesPlayerCache = true;
}

void Tracers::onEnable() {
    log_ok("Tracers enabled");
    lastWorldRef = nullptr;

    
    RenderData::initialized = false;
    RenderData::Reset();

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedPlayers.clear();
    smoothedPlayers.clear();
    lastRenderTime = 0;
}

void Tracers::onDisable() {
    log_ok("Tracers disabled");
    lastWorldRef = nullptr;

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedPlayers.clear();
    smoothedPlayers.clear();
    lastRenderTime = 0;
}

void Tracers::checkWorldChange() {
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
    }

    if (currentWorld)
        Java::Env()->DeleteLocalRef(currentWorld);
}

void Tracers::onUpdate() {
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
    std::vector<TracerPlayer> newCache;

    for (auto& p : players) {
        double dx = p.posX - localX;
        double dy = p.posY - localY;
        double dz = p.posZ - localZ;
        float dist = (float)std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist > settings.range || dist < 0.5f)
            continue;

        TracerPlayer cached;
        cached.entityId = p.entityId;
        cached.name = p.name;
        cached.posX = p.posX;
        cached.posY = p.posY;
        cached.posZ = p.posZ;
        cached.lastPosX = p.lastPosX;
        cached.lastPosY = p.lastPosY;
        cached.lastPosZ = p.lastPosZ;
        cached.distance = dist;
        cached.health = p.health;
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


static ImU32 GetDistanceColor(float distance, float maxRange) {
    float percent = 1.0f - (distance / maxRange);
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


static ImU32 GetHealthColor(float health) {
    float percent = health / 20.0f;
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

static ImU32 WithAlpha(ImU32 color, float alpha) {
    const ImU32 byteAlpha = (ImU32)((std::clamp)(alpha, 0.0f, 1.0f) * 255.0f);
    return (color & 0x00FFFFFFu) | (byteAlpha << 24);
}





static ImVec2 BezierPoint(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t) {
    const float u = 1.0f - t;
    const float w0 = u * u * u;
    const float w1 = 3.0f * u * u * t;
    const float w2 = 3.0f * u * t * t;
    const float w3 = t * t * t;
    return ImVec2(p0.x * w0 + p1.x * w1 + p2.x * w2 + p3.x * w3, p0.y * w0 + p1.y * w1 + p2.y * w2 + p3.y * w3);
}

void Tracers::onRender() {
    if (!RenderData::updated)
        return;

    
    if (GetTickCount64() - lastUpdateTick > 300)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<TracerPlayer> localCache;
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

    
    ImVec2 startPos;
    if (settings.lineFrom == 0) {
        
        startPos = ImVec2(RenderData::screenWidth / 2.0f, RenderData::screenHeight / 2.0f);
    } else {
        
        startPos = ImVec2(RenderData::screenWidth / 2.0f, RenderData::screenHeight - 8.0f);
    }

    
    
    const float pixelScale = (std::clamp)((float)RenderData::screenHeight / 1080.0f, 0.6f, 3.0f);

    const long long now = GetTickCount64();
    const float frameSeconds =
        lastRenderTime > 0 ? (std::min)(0.05f, (float)(now - lastRenderTime) / 1000.0f) : 1.0f / 60.0f;
    lastRenderTime = now;
    const float positionAlpha = 1.0f - std::exp(-20.0f * frameSeconds);

    
    for (const auto& p : localCache) {
        if (!p.valid)
            continue;

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
            smooth.initialized = true;
        } else {
            smooth.x += (targetX - smooth.x) * positionAlpha;
            smooth.y += (targetY - smooth.y) * positionAlpha;
            smooth.z += (targetZ - smooth.z) * positionAlpha;
        }
        smooth.lastSeen = now;

        const float interpX = (float)smooth.x;
        const float interpY = (float)smooth.y + 0.9f;
        const float interpZ = (float)smooth.z;

        
        Vector3 targetPos{interpX - (float)renderX, interpY - (float)renderY, interpZ - (float)renderZ};

        
        Vector2 screenPos;
        bool visible = CWorldToScreen::WorldToScreen(targetPos, RenderData::modelView, RenderData::projection,
                                                     RenderData::screenWidth, RenderData::screenHeight, screenPos);

        if (!visible)
            continue;

        
        ImU32 color;
        if (settings.colorByHealth) {
            color = GetHealthColor(p.health);
        } else if (settings.colorByDistance) {
            color = GetDistanceColor(p.distance, settings.range);
        } else {
            
            color = ImGui::ColorConvertFloat4ToU32(ImVec4(settings.tracerColor[0], settings.tracerColor[1],
                                                          settings.tracerColor[2], settings.tracerColor[3]));
        }

        const float distanceFade = (std::clamp)(1.0f - p.distance / (std::max)(settings.range, 1.0f), 0.25f, 0.9f);

        
        
        
        const float pickedAlpha = (float)((color >> 24) & 0xFF) / 255.0f;
        const float baseAlpha = distanceFade * pickedAlpha * (std::clamp)(settings.opacity, 0.05f, 1.0f);

        const ImVec2 endPos(screenPos.x, screenPos.y);
        const float verticalSpan = endPos.y - startPos.y;
        const float bend = (std::min)(std::abs(endPos.x - startPos.x) * 0.08f, 24.0f);
        const float bendDirection = endPos.x < startPos.x ? -bend : bend;
        const ImVec2 controlA(startPos.x + bendDirection, startPos.y + verticalSpan * 0.33f);
        const ImVec2 controlB(endPos.x - bendDirection, startPos.y + verticalSpan * 0.72f);

        
        
        
        constexpr int kSegments = 32;
        ImVec2 pts[kSegments + 1];
        float arc[kSegments + 1];
        pts[0] = startPos;
        arc[0] = 0.0f;
        for (int i = 1; i <= kSegments; ++i) {
            pts[i] = BezierPoint(startPos, controlA, controlB, endPos, (float)i / (float)kSegments);
            const float sx = pts[i].x - pts[i - 1].x;
            const float sy = pts[i].y - pts[i - 1].y;
            arc[i] = arc[i - 1] + std::sqrt(sx * sx + sy * sy);
        }
        const float totalArc = arc[kSegments];

        
        
        
        
        float gapLen = (std::max)(0.0f, settings.startGap) * pixelScale;
        if (settings.lineFrom != 0)
            gapLen *= 0.35f;
        float rampLen = (std::max)(1.0f, gapLen * 1.3f);

        
        
        if (gapLen + rampLen > totalArc * 0.55f) {
            const float budget = totalArc * 0.55f;
            const float shrink = budget / (std::max)(1.0f, gapLen + rampLen);
            gapLen *= shrink;
            rampLen = (std::max)(1.0f, rampLen * shrink);
        }

        
        float segAlpha[kSegments];
        for (int i = 1; i <= kSegments; ++i) {
            const float mid = (arc[i - 1] + arc[i]) * 0.5f;
            float a = (std::clamp)((mid - gapLen) / rampLen, 0.0f, 1.0f);
            a = a * a * (3.0f - 2.0f * a);  
            segAlpha[i - 1] = baseAlpha * a;
        }

        
        
        
        
        if (settings.outline) {
            const float outlineW = settings.thickness + 1.5f;
            for (int i = 0; i < kSegments; ++i) {
                if (segAlpha[i] <= 0.004f)
                    continue;
                draw->AddLine(pts[i], pts[i + 1], WithAlpha(IM_COL32_BLACK, segAlpha[i] * 0.42f), outlineW);
            }
        }

        for (int i = 0; i < kSegments; ++i) {
            if (segAlpha[i] <= 0.004f)
                continue;
            draw->AddLine(pts[i], pts[i + 1], WithAlpha(color, segAlpha[i]), settings.thickness);
        }

        if (settings.endDot) {
            const float dotR = (std::max)(1.25f, settings.thickness * 0.65f);
            draw->AddCircleFilled(endPos, dotR, WithAlpha(color, baseAlpha), 10);
        }
    }

    for (auto it = smoothedPlayers.begin(); it != smoothedPlayers.end();) {
        if (now - it->second.lastSeen > 500)
            it = smoothedPlayers.erase(it);
        else
            ++it;
    }
}

void Tracers::onRenderMenu() {
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

            
            const char* lineFromOptions[] = {"Center", "Bottom"};
            ui::Combo("Line From", &settings.lineFrom, lineFromOptions, 2);

            ui::SliderFloat("Thickness", &settings.thickness, 1.0f, 5.0f, "%.1fpx");
            ui::SliderFloat("Range", &settings.range, 16.0f, 256.0f, "%.0fm");
            ui::SliderFloat("Opacity", &settings.opacity, 0.1f, 1.0f, "%.2f");
            ui::SliderFloat("Crosshair Gap", &settings.startGap, 0.0f, 160.0f, "%.0fpx");
            ui::Checkbox("Outline", &settings.outline);
            ui::Checkbox("End Dot", &settings.endDot);
        },
        "Colors",
        [this]() {
            ui::Checkbox("Color by Distance", &settings.colorByDistance);
            ui::Checkbox("Color by Health", &settings.colorByHealth);

            if (!settings.colorByDistance && !settings.colorByHealth) {
                ui::color("Tracer Color", settings.tracerColor);
            }
        });

    
    if (settings.colorByDistance) {
        settings.colorByHealth = false;
    }
}



void Tracers::saveSettings(json::Value& node) const {
    node["colorByDistance"] = settings.colorByDistance;
    node["colorByHealth"] = settings.colorByHealth;
    node["colorByTeam"] = settings.colorByTeam;
    node["thickness"] = settings.thickness;
    node["range"] = settings.range;
    node["lineFrom"] = settings.lineFrom;
    node["opacity"] = settings.opacity;
    node["startGap"] = settings.startGap;
    node["outline"] = settings.outline;
    node["endDot"] = settings.endDot;
    json::writeColor(node, "tracerColor", settings.tracerColor);
}

void Tracers::loadSettings(const json::Value& node) {
    settings.colorByDistance = node["colorByDistance"].as_bool(settings.colorByDistance);
    settings.colorByHealth = node["colorByHealth"].as_bool(settings.colorByHealth);
    settings.colorByTeam = node["colorByTeam"].as_bool(settings.colorByTeam);
    settings.thickness = node["thickness"].as_float(settings.thickness);
    settings.range = node["range"].as_float(settings.range);
    settings.lineFrom = node["lineFrom"].as_int(settings.lineFrom);
    settings.opacity = node["opacity"].as_float(settings.opacity);
    settings.startGap = node["startGap"].as_float(settings.startGap);
    settings.outline = node["outline"].as_bool(settings.outline);
    settings.endDot = node["endDot"].as_bool(settings.endDot);
    json::readColor(node, "tracerColor", settings.tracerColor);
}
