#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "trajectories.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/render_data.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"
#include "../../sdk/player/visual_cache.h"

Trajectories* trajectories = nullptr;


static constexpr int ITEM_ENDER_PEARL = 368;
static constexpr int ITEM_EGG = 344;
static constexpr int ITEM_SNOWBALL = 332;
static constexpr int ITEM_BOW = 261;
static constexpr int ITEM_SPLASH_POTION = 373;
static constexpr int ITEM_FISHING_ROD = 346;
static constexpr int ITEM_EXP_BOTTLE = 384;


static constexpr int MAX_SIMULATION_TICKS = 240;

Trajectories::Trajectories() : Module("Trajectories") {
    keybind = 0;
    renders = true;
    worldVisual = true;
}

void Trajectories::onEnable() {
    log_ok("Trajectories enabled");
    lastWorldRef = nullptr;

    RenderData::initialized = false;
    RenderData::Reset();

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedTrajectory.clear();
    renderedTrajectory.clear();
    lastRenderTime = 0;

    
    prevLocalX = cachedLocalX = 0;
    prevLocalY = cachedLocalY = 0;
    prevLocalZ = cachedLocalZ = 0;
    firstFrameAfterEnable = true;
}

void Trajectories::onDisable() {
    log_ok("Trajectories disabled");
    lastWorldRef = nullptr;

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedTrajectory.clear();
    renderedTrajectory.clear();
    lastRenderTime = 0;
}

void Trajectories::checkWorldChange() {
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
            cachedTrajectory.clear();
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

bool Trajectories::isThrowableItem(int itemId) {
    switch (itemId) {
        case ITEM_ENDER_PEARL:
            return settings.showEnderPearl;
        case ITEM_EGG:
            return settings.showEgg;
        case ITEM_SNOWBALL:
            return settings.showSnowball;
        case ITEM_BOW:
            return settings.showBow;
        case ITEM_SPLASH_POTION: {
            
            int potionMeta = player.getHeldItemDamage();

            
            
            
            bool isSplash = (potionMeta & 0x4000) != 0;  
            if (!isSplash)
                return false;  

            
            
            
            
            
            

            int effectId = potionMeta & 0x3F;  

            
            bool isHealing = (effectId == 5 || effectId == 6 || effectId == 21 || effectId == 22 || effectId == 37 ||
                              effectId == 38);

            
            bool isPoison = (effectId == 4 || effectId == 20 || effectId == 36 || effectId == 52);

            
            bool isSlowness = (effectId == 2 || effectId == 10 || effectId == 18 || effectId == 26 || effectId == 34 ||
                               effectId == 42);

            if (isHealing && settings.showPotionHealing)
                return true;
            if (isPoison && settings.showPotionPoison)
                return true;
            if (isSlowness && settings.showPotionSlowness)
                return true;

            return false;
        }
        case ITEM_FISHING_ROD:
            return false;  
        case ITEM_EXP_BOTTLE:
            return settings.showExpBottle;
        default:
            return false;
    }
}

Trajectories::ProjectileType Trajectories::getProjectileType(int itemId, float bowCharge) {
    ProjectileType type;

    switch (itemId) {
        case ITEM_BOW:
            
            
            type.gravity = 0.05f;
            type.initialSpeed = 0.3f + (bowCharge * 2.7f);  
            type.drag = 0.99f;
            break;

        case ITEM_ENDER_PEARL:
            
            type.gravity = 0.03f;
            type.initialSpeed = 1.5f;
            type.drag = 0.99f;
            break;

        case ITEM_EGG:
        case ITEM_SNOWBALL:
        case ITEM_EXP_BOTTLE:
            
            type.gravity = 0.03f;
            type.initialSpeed = 1.5f;
            type.drag = 0.99f;
            break;

        case ITEM_SPLASH_POTION:
            
            
            
            type.gravity = 0.05f;      
            type.initialSpeed = 0.4f;  
            type.drag = 0.99f;
            type.upwardAngle = 20.0f;  
            break;

        case ITEM_FISHING_ROD:
            
            type.gravity = 0.04f;
            type.initialSpeed = 1.5f;
            type.drag = 0.92f;
            break;

        default:
            type.gravity = 0.03f;
            type.initialSpeed = 1.5f;
            type.drag = 0.99f;
            break;
    }

    return type;
}

float Trajectories::getBowChargeLevel() {
    if (bowChargeStartTime == 0)
        return 0.0f;

    
    long long chargeTime = GetTickCount64() - bowChargeStartTime;
    float charge = (float)chargeTime / 1000.0f;
    return (std::min)(1.0f, charge);
}

void Trajectories::simulateTrajectory() {
    if (!player.isValid())
        return;
    if (Menu::isOpen)
        return;

    
    int heldItemId = player.getHeldItemId();
    cachedHeldItemId = heldItemId;

    
    if (!isThrowableItem(heldItemId)) {
        
        bowChargeStartTime = 0;
        wasBowCharging = false;
        cachedBowCharge = 0.0f;

        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedTrajectory.clear();
        return;
    }

    
    if (heldItemId == ITEM_BOW) {
        bool isRightClickHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        if (isRightClickHeld && !mc.isInGui()) {
            
            if (!wasBowCharging) {
                bowChargeStartTime = GetTickCount64();
                wasBowCharging = true;
            }
            cachedBowCharge = getBowChargeLevel();
        } else {
            
            bowChargeStartTime = 0;
            wasBowCharging = false;
            cachedBowCharge = 0.0f;

            std::lock_guard<std::mutex> lock(cacheMutex);
            cachedTrajectory.clear();
            return;
        }
    } else {
        
        cachedBowCharge = 1.0f;
    }

    
    ProjectileType physics = getProjectileType(heldItemId, cachedBowCharge);

    
    float eyeHeight = player.isSneaking() ? 1.54f : 1.62f;
    double startX = player.getPosX();
    double startY = player.getPosY() + eyeHeight;
    double startZ = player.getPosZ();

    
    float yaw = player.getYaw();
    float pitch = player.getPitch();

    
    
    pitch -= physics.upwardAngle;

    
    float yawRad = yaw * 0.017453292f;  
    float pitchRad = pitch * 0.017453292f;

    
    
    float dirX = -sinf(yawRad) * cosf(pitchRad);
    float dirY = -sinf(pitchRad);
    float dirZ = cosf(yawRad) * cosf(pitchRad);

    
    float vx = dirX * physics.initialSpeed;
    float vy = dirY * physics.initialSpeed;
    float vz = dirZ * physics.initialSpeed;

    
    
    float rightOffsetAmount = 0.16f;
    float forwardOffset = 0.18f;

    
    float rightX = -cosf(yawRad) * rightOffsetAmount;
    float rightZ = -sinf(yawRad) * rightOffsetAmount;

    
    float forwardX = dirX * forwardOffset;
    float forwardZ = dirZ * forwardOffset;

    
    
    std::vector<TrajectoryPoint> newTrajectory;

    
    double relPosX = rightX + forwardX;
    double relPosY = -0.12 + (dirY * forwardOffset);
    double relPosZ = rightZ + forwardZ;

    
    TrajectoryPoint start;
    start.relX = (float)relPosX;
    start.relY = (float)relPosY;
    start.relZ = (float)relPosZ;
    start.valid = true;
    start.hitBlock = false;
    newTrajectory.push_back(start);

    const auto collisionSnapshot = VisualCache::GetSnapshot();

    for (int tick = 0; tick < MAX_SIMULATION_TICKS; tick++) {
        
        double prevRelX = relPosX;
        double prevRelY = relPosY;
        double prevRelZ = relPosZ;

        
        
        const int subSteps = 4;
        double subDrag = pow(physics.drag, 1.0 / subSteps);  
        double subGravity = physics.gravity / subSteps;

        for (int sub = 0; sub < subSteps; sub++) {
            
            relPosX += vx / subSteps;
            relPosY += vy / subSteps;
            relPosZ += vz / subSteps;

            
            vy -= (float)subGravity;
            vx *= (float)subDrag;
            vy *= (float)subDrag;
            vz *= (float)subDrag;

            
            TrajectoryPoint subPoint;
            subPoint.relX = (float)relPosX;
            subPoint.relY = (float)relPosY;
            subPoint.relZ = (float)relPosZ;
            subPoint.valid = true;
            subPoint.hitBlock = false;
            newTrajectory.push_back(subPoint);
        }

        
        double absFromX = startX + prevRelX;
        double absFromY = startY + prevRelY;
        double absFromZ = startZ + prevRelZ;
        double absToX = startX + relPosX;
        double absToY = startY + relPosY;
        double absToZ = startZ + relPosZ;

        Minecraft::Vec3 from = {(float)absFromX, (float)absFromY, (float)absFromZ};
        Minecraft::Vec3 to = {(float)absToX, (float)absToY, (float)absToZ};
        Minecraft::Vec3 result;

        auto rayResult = mc.rayTraceBlocks(from, to, result, false);

        if (rayResult == Minecraft::RAYTRACE_BLOCKS_RESULT::HIT ||
            rayResult == Minecraft::RAYTRACE_BLOCKS_RESULT::HIT_FROM_AIRBLOCK) {
            if (!newTrajectory.empty()) {
                newTrajectory.back().relX = result.x - (float)startX;
                newTrajectory.back().relY = result.y - (float)startY;
                newTrajectory.back().relZ = result.z - (float)startZ;
                newTrajectory.back().hitBlock = true;
            }
            break;
        }

        
        if (collisionSnapshot.valid) {
            bool hitAnyPlayer = false;
            for (const auto& p : collisionSnapshot.players) {
                
                float halfWidth = 0.3f;
                float height = 1.8f;

                float minX = (float)p.posX - halfWidth;
                float maxX = (float)p.posX + halfWidth;
                float minY = (float)p.posY;
                float maxY = (float)p.posY + height;
                float minZ = (float)p.posZ - halfWidth;
                float maxZ = (float)p.posZ + halfWidth;

                
                if (absToX >= minX && absToX <= maxX && absToY >= minY && absToY <= maxY && absToZ >= minZ &&
                    absToZ <= maxZ) {
                    hitAnyPlayer = true;
                    break;
                }
            }

            if (hitAnyPlayer) {
                if (!newTrajectory.empty()) {
                    newTrajectory.back().hitPlayer = true;
                }
                break;
            }
        }

        
        double absY = startY + relPosY;
        if (absY < -64 || absY > 320)
            break;

        if (sqrt(relPosX * relPosX + relPosZ * relPosZ) > 150.0)
            break;
    }

    
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedTrajectory = std::move(newTrajectory);
    }
}

void Trajectories::onUpdate() {
    lastUpdateTick = GetTickCount64();

    if (!player.isValid()) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedTrajectory.clear();
        return;
    }

    checkWorldChange();

    const auto snap = VisualCache::GetSnapshot();
    if (!snap.valid) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedTrajectory.clear();
        return;
    }

    
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedRenderX = snap.renderPos.x;
        cachedRenderY = snap.renderPos.y;
        cachedRenderZ = snap.renderPos.z;
        cachedPartialTicks = snap.partialTicks;
        prevLocalX = snap.localLastX;
        prevLocalY = snap.localLastY;
        prevLocalZ = snap.localLastZ;
        cachedLocalX = snap.localX;
        cachedLocalY = snap.localY;
        cachedLocalZ = snap.localZ;
        cachedYaw = player.getYaw();
        cachedPitch = player.getPitch();
        cachedSneaking = player.isSneaking();
        firstFrameAfterEnable = false;
    }

    
    simulateTrajectory();
}

void Trajectories::onRender() {
    if (!enabled)
        return;
    if (!RenderData::updated)
        return;
    if (Menu::isOpen)
        return;

    
    if (GetTickCount64() - lastUpdateTick > 300)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<TrajectoryPoint> localTrajectory;
    double renderX, renderY, renderZ;
    double localX, localY, localZ;
    double prevX, prevY, prevZ;
    float partialTicks;
    float eyeHeight;
    bool isSneaking;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        localTrajectory = cachedTrajectory;
        renderX = cachedRenderX;
        renderY = cachedRenderY;
        renderZ = cachedRenderZ;
        localX = cachedLocalX;
        localY = cachedLocalY;
        localZ = cachedLocalZ;
        prevX = prevLocalX;
        prevY = prevLocalY;
        prevZ = prevLocalZ;
        partialTicks = cachedPartialTicks;
        isSneaking = cachedSneaking;
    }

    if (localTrajectory.size() < 2) {
        renderedTrajectory.clear();
        return;
    }

    const long long now = GetTickCount64();
    const float frameSeconds =
        lastRenderTime > 0 ? (std::min)(0.05f, (float)(now - lastRenderTime) / 1000.0f) : 1.0f / 60.0f;
    lastRenderTime = now;
    const float trajectoryAlpha = 1.0f - std::exp(-18.0f * frameSeconds);

    const size_t oldSize = renderedTrajectory.size();
    renderedTrajectory.resize(localTrajectory.size());
    for (size_t i = 0; i < localTrajectory.size(); ++i) {
        const auto& target = localTrajectory[i];
        auto& rendered = renderedTrajectory[i];
        if (i >= oldSize || !rendered.valid) {
            rendered = target;
            continue;
        }
        rendered.relX += (target.relX - rendered.relX) * trajectoryAlpha;
        rendered.relY += (target.relY - rendered.relY) * trajectoryAlpha;
        rendered.relZ += (target.relZ - rendered.relZ) * trajectoryAlpha;
        rendered.valid = target.valid;
        rendered.hitBlock = target.hitBlock;
        rendered.hitPlayer = target.hitPlayer;
    }

    
    
    double interpX = prevX + (localX - prevX) * partialTicks;
    double interpY = prevY + (localY - prevY) * partialTicks;
    double interpZ = prevZ + (localZ - prevZ) * partialTicks;
    
    eyeHeight = isSneaking ? 1.54f : 1.62f;

    
    ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(settings.lineColor[0], settings.lineColor[1], settings.lineColor[2], settings.lineColor[3]));

    
    
    

    
    std::vector<ImVec2> polylinePoints;
    polylinePoints.reserve(renderedTrajectory.size());

    
    ImVec2 hitMarkerPos;
    bool hasHitMarker = false;

    const auto flushSegment = [&]() {
        if (polylinePoints.size() >= 2) {
            draw->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), IM_COL32(0, 0, 0, 170),
                              ImDrawFlags_None, settings.thickness + 2.5f);
            draw->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), lineColor, ImDrawFlags_None,
                              settings.thickness);
        }
        polylinePoints.clear();
    };

    for (const auto& point : renderedTrajectory) {
        if (!point.valid)
            continue;

        
        double worldX = interpX + point.relX;
        double worldY = interpY + eyeHeight + point.relY;
        double worldZ = interpZ + point.relZ;

        Vector3 cameraRelPos{(float)(worldX - renderX), (float)(worldY - renderY), (float)(worldZ - renderZ)};

        
        Vector2 screenPos;
        bool visible = CWorldToScreen::WorldToScreen(cameraRelPos, RenderData::modelView, RenderData::projection,
                                                     RenderData::screenWidth, RenderData::screenHeight, screenPos);

        if (visible) {
            const ImVec2 projected(screenPos.x, screenPos.y);
            if (polylinePoints.empty()) {
                polylinePoints.push_back(projected);
            } else {
                const float dx = projected.x - polylinePoints.back().x;
                const float dy = projected.y - polylinePoints.back().y;
                if (dx * dx + dy * dy >= 1.0f || point.hitBlock || point.hitPlayer)
                    polylinePoints.push_back(projected);
            }

            if (point.hitBlock && !hasHitMarker) {
                hitMarkerPos = projected;
                hasHitMarker = true;
            }
        } else {
            flushSegment();
        }
    }

    flushSegment();

    
    if (hasHitMarker) {
        const float markerRadius = (std::max)(3.5f, settings.thickness * 2.2f);
        draw->AddCircleFilled(hitMarkerPos, markerRadius + 2.0f, IM_COL32(0, 0, 0, 170), 16);
        draw->AddCircleFilled(hitMarkerPos, markerRadius, lineColor, 16);
        draw->AddCircle(hitMarkerPos, markerRadius + 3.5f, lineColor, 16, 1.0f);
    }
}

void Trajectories::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ui::SliderFloat("Line Thickness", &settings.thickness, 1.0f, 5.0f, "%.1f");

            ImGui::Spacing();
            ui::color("Line Color", settings.lineColor);
        },
        "Projectiles",
        [this]() {
            ui::Checkbox("Bow", &settings.showBow);
            ui::Checkbox("Ender Pearl", &settings.showEnderPearl);
            ui::Checkbox("Egg", &settings.showEgg);
            ui::Checkbox("Snowball", &settings.showSnowball);
            ui::Checkbox("Healing II", &settings.showPotionHealing);
            ui::Checkbox("Poison", &settings.showPotionPoison);
            ui::Checkbox("Slowness", &settings.showPotionSlowness);
        });
}



void Trajectories::saveSettings(json::Value& node) const {
    node["thickness"] = settings.thickness;
    json::writeColor(node, "lineColor", settings.lineColor);
    node["showBow"] = settings.showBow;
    node["showEnderPearl"] = settings.showEnderPearl;
    node["showEgg"] = settings.showEgg;
    node["showSnowball"] = settings.showSnowball;
    node["showPotionHealing"] = settings.showPotionHealing;
    node["showPotionPoison"] = settings.showPotionPoison;
    node["showPotionSlowness"] = settings.showPotionSlowness;
    node["showExpBottle"] = settings.showExpBottle;
}

void Trajectories::loadSettings(const json::Value& node) {
    settings.thickness = node["thickness"].as_float(settings.thickness);
    json::readColor(node, "lineColor", settings.lineColor);
    settings.showBow = node["showBow"].as_bool(settings.showBow);
    settings.showEnderPearl = node["showEnderPearl"].as_bool(settings.showEnderPearl);
    settings.showEgg = node["showEgg"].as_bool(settings.showEgg);
    settings.showSnowball = node["showSnowball"].as_bool(settings.showSnowball);
    settings.showPotionHealing = node["showPotionHealing"].as_bool(settings.showPotionHealing);
    settings.showPotionPoison = node["showPotionPoison"].as_bool(settings.showPotionPoison);
    settings.showPotionSlowness = node["showPotionSlowness"].as_bool(settings.showPotionSlowness);
    settings.showExpBottle = node["showExpBottle"].as_bool(settings.showExpBottle);
}
