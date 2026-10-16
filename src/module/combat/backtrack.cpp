#include "backtrack.h"

#include "../../jni/backtrack/backtrack_gate.hpp"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../../sdk/player/visual_cache.h"
#include "../../ui/core/overlay.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"
#include "aimassist.h"
#include "autoclicker.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

Backtrack* backtrack = nullptr;

namespace {




struct GhostStyle {
    ImVec4 color{0.42f, 0.92f, 1.0f, 1.0f};
    bool filled = true;
    bool glow = true;
    bool corners = true;
    float lineWidth = 1.35f;
};




constexpr double kGhostFollowSpeed = 18.0;

ImVec4 shade(const ImVec4& color, float factor, float alpha) {
    return ImVec4(color.x * factor, color.y * factor, color.z * factor, alpha);
}

ImVec4 lighten(const ImVec4& color, float amount, float alpha) {
    return ImVec4(color.x + (1.0f - color.x) * amount,
                  color.y + (1.0f - color.y) * amount,
                  color.z + (1.0f - color.z) * amount, alpha);
}

bool drawPlayerBox3D(ImDrawList* draw, double x, double y, double z,
                     double renderX, double renderY, double renderZ,
                     float alpha, const GhostStyle& style) {
    if (!draw)
        return false;

    constexpr float halfWidth = 0.3f;
    constexpr float playerHeight = 1.8f;
    const Vector3 corners[8] = {
        {static_cast<float>(x) - halfWidth, static_cast<float>(y), static_cast<float>(z) - halfWidth},
        {static_cast<float>(x) + halfWidth, static_cast<float>(y), static_cast<float>(z) - halfWidth},
        {static_cast<float>(x) + halfWidth, static_cast<float>(y), static_cast<float>(z) + halfWidth},
        {static_cast<float>(x) - halfWidth, static_cast<float>(y), static_cast<float>(z) + halfWidth},
        {static_cast<float>(x) - halfWidth, static_cast<float>(y) + playerHeight, static_cast<float>(z) - halfWidth},
        {static_cast<float>(x) + halfWidth, static_cast<float>(y) + playerHeight, static_cast<float>(z) - halfWidth},
        {static_cast<float>(x) + halfWidth, static_cast<float>(y) + playerHeight, static_cast<float>(z) + halfWidth},
        {static_cast<float>(x) - halfWidth, static_cast<float>(y) + playerHeight, static_cast<float>(z) + halfWidth},
    };
    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };

    Vector2 projected[8]{};
    bool visible[8]{};
    for (int i = 0; i < 8; ++i) {
        const Vector3 relative = {
            corners[i].x - static_cast<float>(renderX),
            corners[i].y - static_cast<float>(renderY),
            corners[i].z - static_cast<float>(renderZ),
        };
        visible[i] = CWorldToScreen::WorldToScreen(
            relative, RenderData::modelView, RenderData::projection,
            RenderData::screenWidth, RenderData::screenHeight, projected[i]);
    }

    bool drew = false;
    
    
    constexpr float faceShade[6] = {0.72f, 0.86f, 0.52f, 0.62f, 0.44f, 0.94f};
    constexpr float faceAlpha[6] = {0.075f, 0.055f, 0.060f, 0.045f, 0.050f, 0.065f};
    constexpr int faces[6][4] = {
        {0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1},
        {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0},
    };
    if (style.filled) {
        for (int i = 0; i < 6; ++i) {
            const int a = faces[i][0];
            const int b = faces[i][1];
            const int c = faces[i][2];
            const int d = faces[i][3];
            if (!visible[a] || !visible[b] || !visible[c] || !visible[d])
                continue;
            const ImVec2 polygon[4] = {
                ImVec2(projected[a].x, projected[a].y),
                ImVec2(projected[b].x, projected[b].y),
                ImVec2(projected[c].x, projected[c].y),
                ImVec2(projected[d].x, projected[d].y),
            };
            draw->AddConvexPolyFilled(
                polygon, 4,
                ImGui::ColorConvertFloat4ToU32(
                    shade(style.color, faceShade[i], alpha * faceAlpha[i])));
            drew = true;
        }
    }

    const ImU32 glow = ImGui::ColorConvertFloat4ToU32(
        lighten(style.color, 0.18f, alpha * 0.22f));
    const ImU32 outline = ImGui::ColorConvertFloat4ToU32(
        ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.82f));
    const ImU32 line = ImGui::ColorConvertFloat4ToU32(
        ImVec4(style.color.x, style.color.y, style.color.z, alpha));

    const auto drawEdges = [&](ImU32 color, float thickness) {
        for (const auto& edge : edges) {
            if (!visible[edge[0]] || !visible[edge[1]])
                continue;
            draw->AddLine(ImVec2(projected[edge[0]].x, projected[edge[0]].y),
                          ImVec2(projected[edge[1]].x, projected[edge[1]].y),
                          color, thickness);
            drew = true;
        }
    };
    
    
    if (style.glow)
        drawEdges(glow, style.lineWidth * 3.70f);
    drawEdges(outline, style.lineWidth * 2.22f);
    drawEdges(line, style.lineWidth);

    if (!style.corners)
        return drew;

    const ImU32 corner = ImGui::ColorConvertFloat4ToU32(
        lighten(style.color, 0.52f, alpha));
    constexpr int cornerEdges[8][3] = {
        {0, 1, 3}, {1, 0, 2}, {2, 1, 3}, {3, 0, 2},
        {4, 5, 7}, {5, 4, 6}, {6, 5, 7}, {7, 4, 6},
    };
    for (const auto& cornerEdge : cornerEdges) {
        const int vertex = cornerEdge[0];
        const int first = cornerEdge[1];
        const int second = cornerEdge[2];
        if (!visible[vertex] || !visible[first] || !visible[second])
            continue;
        const ImVec2 p(projected[vertex].x, projected[vertex].y);
        const ImVec2 pFirst(projected[first].x, projected[first].y);
        const ImVec2 pSecond(projected[second].x, projected[second].y);
        constexpr float cornerLength = 0.16f;
        const ImVec2 firstTip(p.x + (pFirst.x - p.x) * cornerLength,
                              p.y + (pFirst.y - p.y) * cornerLength);
        const ImVec2 secondTip(p.x + (pSecond.x - p.x) * cornerLength,
                               p.y + (pSecond.y - p.y) * cornerLength);
        draw->AddLine(p, firstTip, corner, style.lineWidth * 1.55f);
        draw->AddLine(p, secondTip, corner, style.lineWidth * 1.55f);
    }
    return drew;
}

void clearException(JNIEnv* env) {
    if (env && env->ExceptionCheck())
        env->ExceptionClear();
}

}  

Backtrack::Backtrack() : Module("Backtrack") {
    keybind = 0;
    renders = true;
    worldVisual = true;  
    usesPlayerCache = true;
}

Backtrack::HoldIntent Backtrack::holdIntent() const {
    HoldIntent result;
    for (;;) {
        const std::uint64_t before = intentSequence_.load(std::memory_order_acquire);
        if ((before & 1u) != 0)
            continue;

        result.targetEntityId = intentTarget_.load(std::memory_order_relaxed);
        result.engagementEpoch = intentEpoch_.load(std::memory_order_relaxed);
        result.active = intentActive_.load(std::memory_order_relaxed);
        result.streamHold = intentStreamHold_.load(std::memory_order_relaxed);

        const std::uint64_t after = intentSequence_.load(std::memory_order_acquire);
        if (before == after)
            return result;
    }
}

void Backtrack::publishIntent(const HoldIntent& intent) {
    std::lock_guard<std::recursive_mutex> lock(controlMutex_);
    intentSequence_.fetch_add(1, std::memory_order_acq_rel);
    intentTarget_.store(intent.targetEntityId, std::memory_order_relaxed);
    intentEpoch_.store(intent.engagementEpoch, std::memory_order_relaxed);
    intentActive_.store(intent.active, std::memory_order_relaxed);
    intentStreamHold_.store(intent.streamHold, std::memory_order_relaxed);
    intentSequence_.fetch_add(1, std::memory_order_release);
}

void Backtrack::closeAdmission() {
    HoldIntent intent = holdIntent();
    intent.active = false;
    intent.targetEntityId = -1;
    publishIntent(intent);
}

void Backtrack::closeStream() {
    HoldIntent intent = holdIntent();
    intent.active = false;
    intent.targetEntityId = -1;
    intent.streamHold = false;
    publishIntent(intent);
}

bool Backtrack::flushPending(JNIEnv* env) {
    if (!env)
        return false;
    (void)BacktrackGate::ServiceClientThread(env, true);
    const bool pending = BacktrackGate::HasPending();
    drainRequested_.store(pending, std::memory_order_release);
    return !pending;
}

void Backtrack::onEnable() {
    std::lock_guard<std::recursive_mutex> lock(controlMutex_);
    visualEpoch_.fetch_add(1, std::memory_order_acq_rel);
    closeStream();
    if (BacktrackGate::HasPending())
        drainRequested_.store(true, std::memory_order_release);
    if (!BacktrackGate::IsOperational())
        log_err("[Backtrack] Enabled, but the incoming hook/client pump is unavailable");
}

void Backtrack::onDisable() {
    std::lock_guard<std::recursive_mutex> lock(controlMutex_);
    visualEpoch_.fetch_add(1, std::memory_order_acq_rel);
    closeStream();
    drainRequested_.store(BacktrackGate::HasPending(), std::memory_order_release);
}

void Backtrack::onWorldChange() {
    std::lock_guard<std::recursive_mutex> lock(controlMutex_);
    visualEpoch_.fetch_add(1, std::memory_order_acq_rel);
    closeStream();
    drainRequested_.store(false, std::memory_order_release);

    JNIEnv* env = Java::Env();
    if (env) {
        BacktrackGate::Discard(env);
        discardRequested_.store(false, std::memory_order_release);
    } else {
        discardRequested_.store(true, std::memory_order_release);
    }
}

bool Backtrack::needsClientTick() const {
    return enabled || drainRequested_.load(std::memory_order_acquire) ||
           discardRequested_.load(std::memory_order_acquire) ||
           BacktrackGate::HasPending() || BacktrackGate::NeedsEmergencyDrain();
}

jobject Backtrack::targetedPlayer(JNIEnv* env) const {
    if (!env || !mc.instance || !JniCache::fid_objectMouseOver ||
        !JniCache::fid_typeOfHit || !JniCache::fid_entityHit ||
        !JniCache::cls_MovingObjectType || !JniCache::fid_ENTITY ||
        !JniCache::cls_EntityPlayer) {
        return nullptr;
    }

    jobject mouseOver = env->GetObjectField(mc.instance, JniCache::fid_objectMouseOver);
    if (!mouseOver || env->ExceptionCheck()) {
        clearException(env);
        if (mouseOver)
            env->DeleteLocalRef(mouseOver);
        return nullptr;
    }

    jobject hitType = env->GetObjectField(mouseOver, JniCache::fid_typeOfHit);
    jobject entityType = env->GetStaticObjectField(JniCache::cls_MovingObjectType,
                                                    JniCache::fid_ENTITY);
    const bool entityHitType = hitType && entityType &&
                               env->IsSameObject(hitType, entityType) == JNI_TRUE;
    jobject entity = entityHitType
                         ? env->GetObjectField(mouseOver, JniCache::fid_entityHit)
                         : nullptr;

    if (hitType)
        env->DeleteLocalRef(hitType);
    if (entityType)
        env->DeleteLocalRef(entityType);
    env->DeleteLocalRef(mouseOver);

    if (env->ExceptionCheck() || !entity ||
        env->IsInstanceOf(entity, JniCache::cls_EntityPlayer) != JNI_TRUE) {
        clearException(env);
        if (entity)
            env->DeleteLocalRef(entity);
        return nullptr;
    }
    clearException(env);
    return entity;
}

bool Backtrack::attackSourceActive() const {
    const HWND foreground = GetForegroundWindow();
    const bool focused = foreground != nullptr &&
                         (Overlay::gameWindow == nullptr || foreground == Overlay::gameWindow);
    if (focused && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
        return true;
    if (autoClicker && autoClicker->enabled)
        return true;
    
    
    if (aimAssist && aimAssist->isRewritingRotation())
        return true;
    return false;
}

void Backtrack::notifyHitLanded(int entityId) {
    if (settings.mode != Mode::OnlyOnHit)
        return;
    hitEntityId_.store(entityId, std::memory_order_relaxed);
    hitTimestamp_.store(GetTickCount64(), std::memory_order_release);
}

bool Backtrack::targetWithinRange(JNIEnv* env, jobject target) const {
    if (!env || !target || !player.isValid() || !JniCache::fid_posX ||
        !JniCache::fid_posY || !JniCache::fid_posZ) {
        return true;
    }

    const double tx = env->GetDoubleField(target, JniCache::fid_posX);
    const double ty = env->GetDoubleField(target, JniCache::fid_posY);
    const double tz = env->GetDoubleField(target, JniCache::fid_posZ);
    if (env->ExceptionCheck()) {
        clearException(env);
        return true;
    }

    const double dx = tx - player.getPosX();
    const double dy = ty - player.getPosY();
    const double dz = tz - player.getPosZ();
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance))
        return true;

    const float low = (std::min)(settings.minRange, settings.maxRange);
    const float high = (std::max)(settings.minRange, settings.maxRange);
    return distance >= static_cast<double>(low) && distance <= static_cast<double>(high);
}






void Backtrack::tickWholeStream(JNIEnv* env) {
    if (!player.isValid()) {
        closeStream();
        if (BacktrackGate::HasPending()) {
            drainRequested_.store(true, std::memory_order_release);
            flushPending(env);
        }
        return;
    }

    HoldIntent intent = holdIntent();
    if (!intent.streamHold || intent.engagementEpoch == 0) {
        
        
        
        std::uint64_t epoch = intent.engagementEpoch;
        if (!BacktrackGate::HasPending() || epoch == 0) {
            ++epoch;
            if (epoch == 0)
                epoch = 1;
        }
        intent.engagementEpoch = epoch;
        intent.streamHold = true;
        publishIntent(intent);
    }

    const bool mayEngage = !Menu::isOpen && !mc.isInGui() &&
                           !(settings.onlyWhileAttacking && !attackSourceActive());

    
    
    const bool hitGateOpen = [&]() {
        if (settings.mode != Mode::OnlyOnHit)
            return true;  
        const std::uint64_t ts = hitTimestamp_.load(std::memory_order_acquire);
        if (ts == 0)
            return false;
        const std::uint64_t elapsed = GetTickCount64() - ts;
        return elapsed <= static_cast<std::uint64_t>((std::max)(settings.holdMs, 20));
    }();

    bool engaged = false;
    if (mayEngage && JniCache::mid_getEntityId) {
        jobject target = targetedPlayer(env);
        if (target) {
            const int entityId = env->CallIntMethod(target, JniCache::mid_getEntityId);
            const bool readId = !env->ExceptionCheck() && entityId >= 0;
            clearException(env);
            
            const bool hitMatch = (settings.mode != Mode::OnlyOnHit) ||
                                  (hitEntityId_.load(std::memory_order_relaxed) == entityId);
            if (readId && hitMatch && hitGateOpen && targetWithinRange(env, target)) {
                intent = holdIntent();
                
                
                
                
                if (BacktrackGate::PrimeTarget(env, target, entityId,
                                               intent.engagementEpoch)) {
                    engaged = true;
                    if (!intent.active || intent.targetEntityId != entityId) {
                        intent.active = true;
                        intent.targetEntityId = entityId;
                        publishIntent(intent);
                    }
                }
            }
            env->DeleteLocalRef(target);
            clearException(env);
        }
    }

    
    if (!engaged && holdIntent().active)
        closeAdmission();

    (void)BacktrackGate::ServiceClientThread(env, false);

    if (BacktrackGate::NeedsEmergencyDrain()) {
        closeStream();
        drainRequested_.store(true, std::memory_order_release);
        flushPending(env);
    }
}

void Backtrack::onClientTick(JNIEnv* env) {
    if (!env)
        return;

    std::lock_guard<std::recursive_mutex> lock(controlMutex_);

    
    
    const bool wholeStream = settings.wholeStream;
    BacktrackGate::Configure(static_cast<unsigned>((std::max)(settings.holdMs, 0)),
                             static_cast<unsigned>((std::max)(settings.maxPackets, 1)),
                             wholeStream);

    if (discardRequested_.exchange(false, std::memory_order_acq_rel)) {
        closeStream();
        BacktrackGate::Discard(env);
        drainRequested_.store(false, std::memory_order_release);
    }

    if (!BacktrackGate::IsOperational()) {
        closeStream();
        if (BacktrackGate::HasPending())
            flushPending(env);
        return;
    }

    if (BacktrackGate::NeedsEmergencyDrain()) {
        closeStream();
        drainRequested_.store(true, std::memory_order_release);
    }

    if (drainRequested_.load(std::memory_order_acquire) && !flushPending(env))
        return;

    if (!enabled) {
        closeStream();
        if (BacktrackGate::HasPending())
            flushPending(env);
        return;
    }

    if (wholeStream) {
        tickWholeStream(env);
        return;
    }

    if (Menu::isOpen || mc.isInGui() || !player.isValid() ||
        (settings.onlyWhileAttacking && !attackSourceActive())) {
        closeAdmission();
        if (BacktrackGate::HasPending()) {
            drainRequested_.store(true, std::memory_order_release);
            flushPending(env);
        }
        return;
    }

    
    if (settings.mode == Mode::OnlyOnHit) {
        const std::uint64_t ts = hitTimestamp_.load(std::memory_order_acquire);
        const bool windowExpired = (ts == 0) ||
            (GetTickCount64() - ts > static_cast<std::uint64_t>((std::max)(settings.holdMs, 20)));
        if (windowExpired) {
            closeAdmission();
            if (BacktrackGate::HasPending()) {
                drainRequested_.store(true, std::memory_order_release);
                flushPending(env);
            }
            return;
        }
    }

    jobject target = targetedPlayer(env);
    if (!target || !JniCache::mid_getEntityId) {
        if (target)
            env->DeleteLocalRef(target);
        closeAdmission();
        if (BacktrackGate::HasPending()) {
            drainRequested_.store(true, std::memory_order_release);
            flushPending(env);
        }
        return;
    }

    const int entityId = env->CallIntMethod(target, JniCache::mid_getEntityId);
    if (env->ExceptionCheck() || entityId < 0) {
        clearException(env);
        env->DeleteLocalRef(target);
        closeAdmission();
        if (BacktrackGate::HasPending()) {
            drainRequested_.store(true, std::memory_order_release);
            flushPending(env);
        }
        return;
    }

    
    
    if (!targetWithinRange(env, target)) {
        env->DeleteLocalRef(target);
        closeAdmission();
        if (BacktrackGate::HasPending()) {
            drainRequested_.store(true, std::memory_order_release);
            flushPending(env);
        }
        return;
    }

    HoldIntent intent = holdIntent();
    if (!intent.active || intent.targetEntityId != entityId) {
        closeAdmission();
        if (BacktrackGate::HasPending() && !flushPending(env)) {
            env->DeleteLocalRef(target);
            return;
        }

        std::uint64_t nextEpoch = intent.engagementEpoch + 1;
        if (nextEpoch == 0)
            nextEpoch = 1;
        if (!BacktrackGate::PrimeTarget(env, target, entityId, nextEpoch)) {
            log_warn("[Backtrack] Cannot read the targeted player's position; engagement skipped");
            env->DeleteLocalRef(target);
            return;
        }
        publishIntent({entityId, nextEpoch, true});
    } else if (!BacktrackGate::PrimeTarget(env, target, entityId,
                                           intent.engagementEpoch)) {
        closeAdmission();
        drainRequested_.store(BacktrackGate::HasPending(), std::memory_order_release);
    }

    env->DeleteLocalRef(target);
    clearException(env);
    (void)BacktrackGate::ServiceClientThread(env, false);

    if (BacktrackGate::NeedsEmergencyDrain()) {
        closeAdmission();
        drainRequested_.store(true, std::memory_order_release);
        flushPending(env);
    }
}

Backtrack::HeldTargetSnapshot Backtrack::heldTargetSnapshot() const {
    const HoldIntent intent = holdIntent();
    const BacktrackGate::TargetSnapshot source = BacktrackGate::GetTargetSnapshot();

    HeldTargetSnapshot result;
    result.valid = source.valid;
    result.active = intent.active && source.valid &&
                    intent.targetEntityId == source.entityId &&
                    intent.engagementEpoch == source.engagementEpoch;
    result.entityId = source.entityId;
    result.x = static_cast<double>(source.serverX) / 32.0;
    result.y = static_cast<double>(source.serverY) / 32.0;
    result.z = static_cast<double>(source.serverZ) / 32.0;
    result.engagementEpoch = source.engagementEpoch;
    result.connectionEpoch = source.connectionEpoch;
    result.sequence = source.sequence;
    result.pendingPackets = source.pendingPackets;
    return result;
}

void Backtrack::onRender() {
    if (!enabled)
        return;
    ImGuiIO& io = ImGui::GetIO();
    float frameSeconds = io.DeltaTime;
    if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0f ||
        frameSeconds > 0.25f) {
        frameSeconds = 1.0f / 60.0f;
    }
    frameSeconds = (std::min)(frameSeconds, 0.05f);

    const std::uint64_t visualEpoch = visualEpoch_.load(std::memory_order_acquire);
    if (boxAnimation_.visualEpoch != visualEpoch) {
        boxAnimation_ = {};
        boxAnimation_.visualEpoch = visualEpoch;
    }

    const HeldTargetSnapshot snapshot = heldTargetSnapshot();
    const bool boxActive = snapshot.active && snapshot.valid &&
                           snapshot.pendingPackets != 0 && RenderData::updated;

    if (boxActive) {
        const bool streamChanged = !boxAnimation_.initialized ||
                                   boxAnimation_.entityId != snapshot.entityId ||
                                   boxAnimation_.engagementEpoch != snapshot.engagementEpoch ||
                                   boxAnimation_.connectionEpoch != snapshot.connectionEpoch;
        const double dx = snapshot.x - boxAnimation_.x;
        const double dy = snapshot.y - boxAnimation_.y;
        const double dz = snapshot.z - boxAnimation_.z;
        const bool teleported = dx * dx + dy * dy + dz * dz > 36.0;

        if (streamChanged || teleported) {
            boxAnimation_.x = snapshot.x;
            boxAnimation_.y = snapshot.y;
            boxAnimation_.z = snapshot.z;
            boxAnimation_.entityId = snapshot.entityId;
            boxAnimation_.engagementEpoch = snapshot.engagementEpoch;
            boxAnimation_.connectionEpoch = snapshot.connectionEpoch;
            boxAnimation_.initialized = true;
            if (streamChanged)
                boxAnimation_.alpha = 0.0f;
        } else {
            const double blend = 1.0 - std::exp(-kGhostFollowSpeed * frameSeconds);
            boxAnimation_.x += dx * blend;
            boxAnimation_.y += dy * blend;
            boxAnimation_.z += dz * blend;
        }
    }

    const float alphaBlend = 1.0f - std::exp(-(boxActive ? 14.0f : 10.0f) * frameSeconds);
    boxAnimation_.alpha += ((boxActive ? 1.0f : 0.0f) - boxAnimation_.alpha) * alphaBlend;
    boxAnimation_.alpha = std::clamp(boxAnimation_.alpha, 0.0f, 1.0f);
    if (!boxAnimation_.initialized || boxAnimation_.alpha <= 0.01f ||
        !RenderData::updated)
        return;

    const VisualSnapshot visual = VisualCache::GetSnapshot();
    if (!visual.valid)
        return;

    double renderX = visual.renderPos.x;
    double renderY = visual.renderPos.y;
    double renderZ = visual.renderPos.z;
    if (renderX == 0.0 && renderY == 0.0 && renderZ == 0.0) {
        const double partial = std::clamp(static_cast<double>(visual.partialTicks), 0.0, 1.0);
        renderX = visual.localLastX + (visual.localX - visual.localLastX) * partial;
        renderY = visual.localLastY + (visual.localY - visual.localLastY) * partial + 1.62;
        renderZ = visual.localLastZ + (visual.localZ - visual.localLastZ) * partial;
    }

    GhostStyle style;
    style.color = ImVec4(settings.color[0], settings.color[1], settings.color[2],
                         settings.color[3]);

    
    
    const float alpha = boxAnimation_.alpha * std::clamp(settings.color[3], 0.0f, 1.0f);
    if (alpha <= 0.01f)
        return;

    (void)drawPlayerBox3D(ImGui::GetBackgroundDrawList(), boxAnimation_.x,
                          boxAnimation_.y, boxAnimation_.z, renderX, renderY,
                          renderZ, alpha, style);
}

void Backtrack::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Engagement",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) &&
                enabledValue != enabled) {
                setEnabled(enabledValue);
            }
            ImGui::Spacing();
            {
                int mode = static_cast<int>(settings.mode);
                const char* modeLabels[] = {"Always", "Only on Hit"};
                if (ui::Combo("Mode", &mode, modeLabels, IM_ARRAYSIZE(modeLabels))) {
                    settings.mode = static_cast<Mode>(mode);
                }
            }
            ui::SliderInt("Hold Time (ms)", &settings.holdMs, 20, 400, "%d");
            ui::SliderInt("Packet Limit", &settings.maxPackets, 2, 64, "%d");
            ui::Checkbox("Whole Stream", &settings.wholeStream);
            if (ui::SliderFloat("Min Range", &settings.minRange, 0.0f, 6.0f, "%.1f") &&
                settings.maxRange < settings.minRange) {
                settings.maxRange = settings.minRange;
            }
            if (ui::SliderFloat("Max Range", &settings.maxRange, 0.0f, 6.0f, "%.1f") &&
                settings.minRange > settings.maxRange) {
                settings.minRange = settings.maxRange;
            }
            ui::Checkbox("Only While Attacking", &settings.onlyWhileAttacking);
        },
        "Ghost",
        [this]() {
            ui::color("Color", settings.color);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("The picker's alpha is the ghost's opacity.");
            ImGui::PopStyleColor();
        });

    ImGui::Spacing();
    const BacktrackGate::Status status = BacktrackGate::GetStatus();
    if (!status.ready) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
                           "Unavailable: native bridge failed");
    } else if (!status.operational) {
        ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                           "Unavailable: bytecode hooks incomplete");
    } else if (status.pendingPackets != 0) {
        ImGui::TextColored(ImVec4(settings.color[0], settings.color[1], settings.color[2], 1.0f),
                           "Tracking %u packet(s)", status.pendingPackets);
    } else {
        ImGui::TextDisabled("Aim at a player to engage");
    }
}

std::string Backtrack::getDisplayInfo() {
    const BacktrackGate::Status status = BacktrackGate::GetStatus();
    if (!status.ready || !status.operational)
        return "Offline";
    if (status.pendingPackets != 0)
        return std::to_string(status.pendingPackets);
    return std::to_string(settings.holdMs) + "ms";
}

void Backtrack::saveSettings(json::Value& node) const {
    const bool enabledValue = node["enabled"].as_bool(enabled);
    const int keybindValue = node["keybind"].as_int(keybind);
    node = json::Value();
    node["enabled"] = enabledValue;
    node["keybind"] = keybindValue;
    node["mode"] = static_cast<int>(settings.mode);
    node["holdMs"] = settings.holdMs;
    node["maxPackets"] = settings.maxPackets;
    node["wholeStream"] = settings.wholeStream;
    node["minRange"] = settings.minRange;
    node["maxRange"] = settings.maxRange;
    node["onlyWhileAttacking"] = settings.onlyWhileAttacking;
    json::writeColor(node, "color", settings.color);
}

void Backtrack::loadSettings(const json::Value& node) {
    settings.mode = static_cast<Mode>(std::clamp(node["mode"].as_int(static_cast<int>(settings.mode)), 0, 1));
    settings.holdMs = std::clamp(node["holdMs"].as_int(settings.holdMs), 20, 400);
    settings.maxPackets = std::clamp(node["maxPackets"].as_int(settings.maxPackets), 2, 64);
    settings.wholeStream = node["wholeStream"].as_bool(settings.wholeStream);
    settings.minRange = std::clamp(node["minRange"].as_float(settings.minRange), 0.0f, 6.0f);
    settings.maxRange = std::clamp(node["maxRange"].as_float(settings.maxRange),
                                   settings.minRange, 6.0f);
    settings.onlyWhileAttacking =
        node["onlyWhileAttacking"].as_bool(settings.onlyWhileAttacking);
    
    
    
    json::readColor(node, "color", settings.color);
}
