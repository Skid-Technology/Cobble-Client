#include "blink.h"
#include "fakelag.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../../util/math/worldToScreen.h"
#include <imgui.h>
#include <windows.h>

Blink* blink = nullptr;

namespace {

static jmethodID getSendPacketMethod(JNIEnv* env, jobject netHandler) {
    static jmethodID mid_sendPacket = nullptr;
    if (mid_sendPacket || !env || !netHandler)
        return mid_sendPacket;

    jclass cls = env->GetObjectClass(netHandler);
    if (!cls)
        return nullptr;

    mid_sendPacket = env->GetMethodID(cls, OBFUSCATE_STRING("addToSendQueue").c_str(),
                                      OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    if (!mid_sendPacket) {
        env->ExceptionClear();
        mid_sendPacket = env->GetMethodID(cls, OBFUSCATE_STRING("func_147297_a").c_str(),
                                          OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    }
    if (!mid_sendPacket) {
        env->ExceptionClear();
        mid_sendPacket = env->GetMethodID(cls, OBFUSCATE_STRING("sendPacket").c_str(),
                                          OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    }
    env->DeleteLocalRef(cls);
    return mid_sendPacket;
}

static jobject getNetHandler(JNIEnv* env) {
    if (!env || !mc.isValid() || !JniCache::mid_getNetHandler)
        return nullptr;
    jobject netHandler = env->CallObjectMethod(mc.instance, JniCache::mid_getNetHandler);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (netHandler)
            env->DeleteLocalRef(netHandler);
        return nullptr;
    }
    return netHandler;
}

static bool sendPacket(JNIEnv* env, jobject packet) {
    if (!env || !packet)
        return false;

    jobject netHandler = getNetHandler(env);
    if (!netHandler)
        return false;

    jmethodID mid = getSendPacketMethod(env, netHandler);
    if (!mid) {
        env->DeleteLocalRef(netHandler);
        return false;
    }

    env->CallVoidMethod(netHandler, mid, packet);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(netHandler);
    return true;
}

}  

Blink::Blink() : Module("Blink") {
    keybind = 0;                   
    renders = true;                
    worldVisual = true;            
    settings.movementOnly = true;  
}

void Blink::onEnable() {
    if (fakeLag && fakeLag->enabled)
        fakeLag->setEnabled(false);
    isBlinking = true;
    blinkStartTime = GetTickCount64();
    pathPositions.clear();
    clearQueuedPackets();
    log_debug("[Blink] Enabled - blocking packets (movementOnly=%s, timer=%.1fs)",
              settings.movementOnly ? "true" : "false", settings.timerLimit);
}

void Blink::onDisable() {
    isBlinking = false;
    blinkStartTime = 0;
    
    
    log_debug("[Blink] Disabled - flushing queued packets");
    flushQueuedPackets();
    log_debug("[Blink] Disabled - packets released");
}

void Blink::onUpdate() {
    if (!enabled)
        return;

    
    if (isBlinking && player.isValid()) {
        Vec3d pos = {player.getPosX(), player.getPosY(), player.getPosZ()};
        cachedRenderPos = mc.getRenderPos();
        hasRenderPos = true;

        
        {
            std::lock_guard<std::mutex> lock(pathMutex);
            if (pathPositions.empty()) {
                pathPositions.push_back(pos);
            } else {
                Vec3d& last = pathPositions.back();
                double dx = pos.x - last.x;
                double dy = pos.y - last.y;
                double dz = pos.z - last.z;
                double distSq = dx * dx + dy * dy + dz * dz;
                
                if (distSq > 0.25) {
                    pathPositions.push_back(pos);
                }
            }
        }
    }

    
    if (blinkStartTime != 0) {
        ULONGLONG elapsed = GetTickCount64() - blinkStartTime;
        if (elapsed > (ULONGLONG)(settings.timerLimit * 1000.0f)) {
            log_debug("[Blink] Timer limit reached, auto-disabling");
            setEnabled(false);
        }
    }
}

bool Blink::shouldBlockPacket() {
    if (!enabled || !isBlinking)
        return false;

    
    if (blinkStartTime != 0) {
        ULONGLONG elapsed = GetTickCount64() - blinkStartTime;
        if (elapsed > (ULONGLONG)(settings.timerLimit * 1000.0f)) {
            log_debug("[Blink] Timer limit reached in gate, auto-disabling");
            setEnabled(false);
            return false;
        }
    }

    return true;  
}

void Blink::onRender() {
    if (!enabled || !isBlinking)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    
    float elapsedSec = 0.0f;
    if (blinkStartTime != 0) {
        elapsedSec = (float)(GetTickCount64() - blinkStartTime) / 1000.0f;
    }

    
    ImU32 blinkCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(settings.blinkColor[0], settings.blinkColor[1], settings.blinkColor[2], settings.blinkColor[3]));
    ImU32 pathColor = blinkCol;
    ImU32 timerFg = blinkCol;

    
    if (settings.showBreadcrumbs && RenderData::updated && hasRenderPos) {
        std::vector<Vec3d> pathCopy;
        {
            std::lock_guard<std::mutex> lock(pathMutex);
            pathCopy = pathPositions;
        }

        if (!pathCopy.empty()) {
            ImVec2 lastPoint{};
            bool hasLast = false;
            float radius = 2.5f;  

            for (const auto& pos : pathCopy) {
                Vector3 worldPos{(float)(pos.x - cachedRenderPos.x), (float)(pos.y - cachedRenderPos.y),
                                 (float)(pos.z - cachedRenderPos.z)};
                Vector2 screen{};
                if (CWorldToScreen::WorldToScreen(worldPos, RenderData::modelView, RenderData::projection,
                                                  RenderData::screenWidth, RenderData::screenHeight, screen)) {
                    ImVec2 pt(screen.x, screen.y);
                    draw->AddCircleFilled(pt, radius, pathColor);
                    if (hasLast) {
                        draw->AddLine(lastPoint, pt, pathColor, 1.0f);
                    }
                    lastPoint = pt;
                    hasLast = true;
                }
            }
        }
    }

    
    if (settings.showTimerHud) {
        ImGuiIO& io = ImGui::GetIO();
        float screenW = io.DisplaySize.x;

        
        float barW = 220.0f;  
        float barH = 7.0f;    
        float barX = (screenW - barW) / 2.0f;
        float barY = 60.0f;
        float progress = elapsedSec / settings.timerLimit;
        if (progress > 1.0f)
            progress = 1.0f;

        ImU32 timerBg = IM_COL32(0, 0, 0, 150);
        ImU32 progressColor = timerFg;

        
        draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), timerBg, 3.0f);
        
        draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * progress, barY + barH), progressColor, 3.0f);
        
        draw->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(255, 255, 255, 100), 3.0f);

        
        char buf[32];
        snprintf(buf, sizeof(buf), "Blink: %.1fs / %.1fs", elapsedSec, settings.timerLimit);

        
        float fontScale = 1.1f;
        ImGui::SetWindowFontScale(fontScale);
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        ImGui::SetWindowFontScale(1.0f);  

        float textX = (screenW - textSize.x) / 2.0f;
        float textY = barY + barH + 5.0f;

        
        ImFont* font = ImGui::GetFont();
        float fontSize = font->FontSize * fontScale;
        draw->AddText(font, fontSize, ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 200), buf);
        draw->AddText(font, fontSize, ImVec2(textX, textY), timerFg, buf);
    }
}

void Blink::onRenderMenu() {
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
            ui::SliderFloat("Timer Limit", &settings.timerLimit, 0.5f, 5.0f, "%.1fs");
        },
        "Visuals",
        [this]() {
            ui::Checkbox("Show Timer", &settings.showTimerHud);
            ui::Checkbox("Show Breadcrumbs", &settings.showBreadcrumbs);
            ImGui::Spacing();
            ui::color("Color", settings.blinkColor);
        });
}

void Blink::queuePacket(JNIEnv* env, jobject packet) {
    if (!env || !packet)
        return;

    jobject global = env->NewGlobalRef(packet);
    if (!global)
        return;

    std::lock_guard<std::mutex> lock(queueMutex);
    queuedPackets.push_back(global);
    log_debug("[Blink] Queued packet (total=%zu)", queuedPackets.size());
}

void Blink::flushQueuedPackets() {
    JNIEnv* env = Java::Env();
    if (!env) {
        return;
    }

    std::vector<jobject> toSend;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        toSend.swap(queuedPackets);
    }

    if (toSend.empty())
        return;

    log_debug("[Blink] Flushing %zu queued packets", toSend.size());

    for (jobject pkt : toSend) {
        sendPacket(env, pkt);
        env->DeleteGlobalRef(pkt);
    }
    log_debug("[Blink] Flush complete");
}

void Blink::clearQueuedPackets() {
    JNIEnv* env = Java::Env();
    if (!env)
        return;

    std::vector<jobject> toDelete;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        toDelete.swap(queuedPackets);
    }

    for (jobject pkt : toDelete) {
        if (pkt)
            env->DeleteGlobalRef(pkt);
    }

    if (!toDelete.empty()) {
        log_debug("[Blink] Cleared queued packets (%zu)", toDelete.size());
    }
}



void Blink::saveSettings(json::Value& node) const {
    node["timerLimit"] = settings.timerLimit;
    node["showTimerHud"] = settings.showTimerHud;
    node["showBreadcrumbs"] = settings.showBreadcrumbs;
    json::writeColor(node, "blinkColor", settings.blinkColor);
}

void Blink::loadSettings(const json::Value& node) {
    settings.timerLimit = node["timerLimit"].as_float(settings.timerLimit);
    settings.showTimerHud = node["showTimerHud"].as_bool(settings.showTimerHud);
    settings.showBreadcrumbs = node["showBreadcrumbs"].as_bool(settings.showBreadcrumbs);
    json::readColor(node, "blinkColor", settings.blinkColor);
}
