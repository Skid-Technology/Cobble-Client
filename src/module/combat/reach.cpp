#include "reach.h"
#include "antibot.h"
#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cmath>

Reach* reach = nullptr;

static constexpr float PI = 3.14159265358979323846f;

Reach::Reach() : Module("Reach") {
    keybind = 0;
    rngState = static_cast<unsigned int>(GetTickCount());
}

void Reach::onEnable() {
    tickCounter = 0;
    rngState ^= static_cast<unsigned int>(GetTickCount());
}

void Reach::onDisable() {
}

float Reach::randomFloat(float min, float max) {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    float normalized = static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFF);
    return min + normalized * (max - min);
}

bool Reach::shouldApplyThisTick() {
    
    if (settings.tickDelay > 0) {
        tickCounter++;
        if (tickCounter <= settings.tickDelay) {
            return false;
        }
        tickCounter = 0;
    }
    
    
    if (settings.hitChance < 100) {
        int roll = static_cast<int>(randomFloat(0.0f, 100.0f));
        if (roll >= settings.hitChance) {
            return false;
        }
    }
    
    return true;
}

void Reach::onUpdate() {
    if (!enabled) return;
    if (Menu::isOpen) return;
    if (mc.isInGui()) return;
    if (!player.isValid()) return;
    if (!mc.instance) return;
    
    JNIEnv* env = Java::Env();
    if (!env) return;
    
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    
    
    if (!shouldApplyThisTick()) return;
    
    
    if (settings.weaponsOnly) {
        int itemId = player.getHeldItemId();
        
        
        bool isSword = (itemId == 268 || itemId == 272 || itemId == 267 || itemId == 276 || itemId == 283);
        bool isAxe = (itemId == 271 || itemId == 275 || itemId == 258 || itemId == 279 || itemId == 286);
        if (!isSword && !isAxe) return;
    }
    
    
    if (settings.sprintingOnly && !player.isSprinting()) return;
    
    
    if (settings.movingOnly) {
        double motionX = player.getMotionX();
        double motionZ = player.getMotionZ();
        if (motionX * motionX + motionZ * motionZ < 0.0001) return;
    }
    
    
    auto playerList = mc.getPlayerList();
    if (playerList.empty()) return;
    
    
    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();
    
    
    PlayerInfo* target = nullptr;
    float finalDist = 9999.0f;
    
    for (auto& p : playerList) {
        if (!p.instance) continue;
        if (p.isInvisible) continue;
        if (AntiBot::Filtered(p)) continue;
        if (friendsManager && friendsManager->isFriendByUUID(p.uuid)) continue;
        if (friendsManager && friendsManager->isApolloTeammate(p.uuid)) continue;
        if (friendsManager && friendsManager->isLunarTeammate(p.uuid)) continue;
        
        double dx = localX - p.posX;
        double dy = localY - p.posY;
        double dz = localZ - p.posZ;
        float dist = (float)sqrt(dx * dx + dy * dy + dz * dz);
        
        if (dist <= finalDist && dist <= 6.0f) {
            finalDist = dist;
            target = &p;
        }
    }
    
    
    for (auto& p : playerList) {
        if (p.instance && (&p != target)) {
            env->DeleteGlobalRef(p.instance);
            p.instance = nullptr;
        }
    }
    
    if (!target || !target->instance) return;
    
    
    float distBetween = finalDist;
    if (distBetween > settings.distance + 0.5f || distBetween <= 3.0f) {
        env->DeleteGlobalRef(target->instance);
        return;
    }
    
    
    float targetX = (float)target->posX;
    float targetZ = (float)target->posZ;
    
    float hypothenuseDistance = hypotf((float)(localX - target->posX), (float)(localZ - target->posZ));
    float dist = settings.distance - 3.0f;
    
    while (distBetween > 3.0f && (distBetween < (dist + 3.0f)) && dist > 0.05f) {
        dist -= 0.05f;
    }
    
    if (hypothenuseDistance < dist) {
        dist -= hypothenuseDistance;
    }
    
    float angleX = targetX - (float)localX;
    float angleZ = targetZ - (float)localZ;
    
    float angle = (float)(-atan2f(angleX, angleZ) * 180.0f / PI);
    if (angleZ < 0.0f && angleX < 0.0f) {
        angle = 90.0f + (float)(atan2f(angleZ, angleX) * 180.0f / PI);
    }
    else if (angleZ < 0.0f && angleX > 0.0f) {
        angle = -90.0f + (float)(atan2f(angleZ, angleX) * 180.0f / PI);
    }
    
    float ax = cosf((angle + 90.0f) * PI / 180.0f);
    float az = sinf((angle + 90.0f) * PI / 180.0f);
    
    float newX = targetX - ax * dist;
    float newZ = targetZ - az * dist;
    
    
    if (!JniCache::fid_boundingBox) {
        env->DeleteGlobalRef(target->instance);
        return;
    }
    
    jobject aabb = env->GetObjectField(target->instance, JniCache::fid_boundingBox);
    if (!aabb) {
        env->DeleteGlobalRef(target->instance);
        return;
    }
    
    if (!JniCache::fid_aabb_minX || !JniCache::fid_aabb_maxX) {
        env->DeleteLocalRef(aabb);
        env->DeleteGlobalRef(target->instance);
        return;
    }
    
    double curMinY = env->GetDoubleField(aabb, JniCache::fid_aabb_minY);
    double curMaxY = env->GetDoubleField(aabb, JniCache::fid_aabb_maxY);
    
    float newWidth = 0.6f / 2.0f;
    
    
    env->SetDoubleField(aabb, JniCache::fid_aabb_minX, (double)(newX - newWidth));
    env->SetDoubleField(aabb, JniCache::fid_aabb_minZ, (double)(newZ - newWidth));
    env->SetDoubleField(aabb, JniCache::fid_aabb_maxX, (double)(newX + newWidth));
    env->SetDoubleField(aabb, JniCache::fid_aabb_maxZ, (double)(newZ + newWidth));
    env->SetDoubleField(aabb, JniCache::fid_aabb_minY, curMinY);
    env->SetDoubleField(aabb, JniCache::fid_aabb_maxY, curMaxY);
    
    env->DeleteLocalRef(aabb);
    env->DeleteGlobalRef(target->instance);
    
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

void Reach::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();
            
            ui::SliderFloat("Distance", &settings.distance, 3.0f, 6.0f, "%.2f");
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::Text("Conditions");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ui::Checkbox("Weapons Only", &settings.weaponsOnly);
            ui::Checkbox("Sprinting Only", &settings.sprintingOnly);
            ui::Checkbox("Moving Only", &settings.movingOnly);
        },
        "Anti-Detection",
        [this]() {
            int chance = settings.hitChance;
            if (ui::SliderInt("Hit Chance", &chance, 0, 100, "%d%%")) {
                settings.hitChance = chance;
            }
            
            int delay = settings.tickDelay;
            if (ui::SliderInt("Tick Delay", &delay, 0, 10, "%d")) {
                settings.tickDelay = delay;
            }
            ui::TickTooltip();
        }
    );
}

std::string Reach::getDisplayInfo() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", settings.distance);
    return buf;
}



void Reach::saveSettings(json::Value& node) const {
    node["distance"] = settings.distance;
    node["weaponsOnly"] = settings.weaponsOnly;
    node["sprintingOnly"] = settings.sprintingOnly;
    node["movingOnly"] = settings.movingOnly;
    node["hitChance"] = settings.hitChance;
    node["tickDelay"] = settings.tickDelay;
}

void Reach::loadSettings(const json::Value& node) {
    settings.distance = node["distance"].as_float(settings.distance);
    settings.weaponsOnly = node["weaponsOnly"].as_bool(settings.weaponsOnly);
    settings.sprintingOnly = node["sprintingOnly"].as_bool(settings.sprintingOnly);
    settings.movingOnly = node["movingOnly"].as_bool(settings.movingOnly);
    settings.hitChance = node["hitChance"].as_int(settings.hitChance);
    settings.tickDelay = node["tickDelay"].as_int(settings.tickDelay);
}
