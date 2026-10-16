#include "fakelag.h"

#include "blink.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <vector>

FakeLag* fakeLag = nullptr;








static thread_local bool tlSelfRelease = false;

namespace {

constexpr std::uint64_t kPingSampleIntervalMs = 250;
constexpr float kPingSmoothingAlpha = 0.20f;
constexpr int kMaximumUsablePingMs = 2000;
constexpr int kMaximumHoldMs = 400;
constexpr int kSafeTargetPingCeilingMs = 260;
constexpr int kHardPacketLimit = 24;
constexpr int kAirPacketLimit = 6;
constexpr int kFastPacketLimit = 10;
constexpr float kFastMovementSpeed = 0.38f;
constexpr float kVeryFastMovementSpeed = 0.58f;

bool packetClassContains(JNIEnv* env, jobject packet, const char* needle) {
    if (!env || !packet || !needle)
        return false;

    jclass cls = env->GetObjectClass(packet);
    if (!cls) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    jclass classClass = env->GetObjectClass(cls);
    jmethodID getName = classClass
                            ? env->GetMethodID(classClass, OBFUSCATE_STRING("getName").c_str(),
                                               OBFUSCATE_STRING("()Ljava/lang/String;").c_str())
                            : nullptr;
    jstring name = getName ? static_cast<jstring>(env->CallObjectMethod(cls, getName)) : nullptr;
    bool result = false;
    if (name) {
        const char* chars = env->GetStringUTFChars(name, nullptr);
        if (chars) {
            result = std::strstr(chars, needle) != nullptr;
            env->ReleaseStringUTFChars(name, chars);
        }
        env->DeleteLocalRef(name);
    }
    if (classClass)
        env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    return result;
}

}  

FakeLag::FakeLag() : Module("FakeLag") {
    renders = true;
    worldVisual = true;  
    locked = true;  
}

FakeLag::~FakeLag() {
    clearQueuedPackets();
}

std::uint64_t FakeLag::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

void FakeLag::resetRuntimeState() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastPingSampleMs_ = 0;
    rawPingMs_ = -1;
    smoothedPingMs_ = -1.0f;
    currentTargetPingMs_ = 0;
    currentHoldMs_ = 0;
    serverX_ = 0.0;
    serverY_ = 0.0;
    serverZ_ = 0.0;
    hasServerPosition_ = false;
}

void FakeLag::resetAnchor() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!player.isValid()) {
        hasServerPosition_ = false;
        return;
    }
    serverX_ = player.getPosX();
    serverY_ = player.getPosY();
    serverZ_ = player.getPosZ();
    hasServerPosition_ = true;
}

void FakeLag::onEnable() {
    if (blink && blink->enabled)
        blink->setEnabled(false);

    clearQueuedPackets();
    resetRuntimeState();
    active_.store(true, std::memory_order_release);
    flushRequested_.store(false, std::memory_order_release);
    draining_.store(false, std::memory_order_release);
    lastReleaseMs_.store(0, std::memory_order_release);
    cycleStartMs_ = nowMs();
    previousHurtTime_ = player.isValid() ? player.getHurtTime() : 0;
    resetAnchor();
    log_debug("[FakeLag] Enabled (mode=%d, target=%d-%dms)", static_cast<int>(settings.mode),
              settings.minTargetPingMs, settings.maxTargetPingMs);
}

void FakeLag::onDisable() {
    active_.store(false, std::memory_order_release);
    flushRequested_.store(false, std::memory_order_release);
    draining_.store(false, std::memory_order_release);
    if (player.isValid())
        flushQueuedPackets(true);
    else
        clearQueuedPackets();
    resetRuntimeState();
}

bool FakeLag::isMovementPacket(JNIEnv* env, jobject packet) const {
    return packetClassContains(env, packet, OBFUSCATE_STRING("C03PacketPlayer").c_str());
}

bool FakeLag::isAttackPacket(JNIEnv* env, jobject packet) const {
    return packetClassContains(env, packet, OBFUSCATE_STRING("C02PacketUseEntity").c_str());
}

bool FakeLag::shouldQueueMovement() const {
    if (!enabled || !active_.load(std::memory_order_acquire) || isReleasing() || !player.isValid())
        return false;
    if (draining_.load(std::memory_order_acquire))
        return false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (smoothedPingMs_ < 0.0f || currentHoldMs_ <= 0)
            return false;
    }

    const double motion = std::hypot(player.getMotionX(), player.getMotionZ());
    if (settings.movingOnly && motion < 0.012)
        return false;

    const float distanceLimit = player.isOnGround()
                                    ? (std::max)(0.45f, settings.maxDistance * 0.72f)
                                    : (std::min)(0.35f, settings.maxDistance * 0.35f);
    if (serverDistance() >= distanceLimit ||
        queuedPacketCount() >= static_cast<std::size_t>(effectivePacketLimit()))
        return false;

    if (settings.mode == Mode::Pulse) {
        const int hold = (std::max)(settings.pulseHoldMs, 20);
        const int release = (std::max)(settings.pulseReleaseMs, 20);
        const std::uint64_t phase = (nowMs() - cycleStartMs_) % static_cast<std::uint64_t>(hold + release);
        return phase < static_cast<std::uint64_t>(hold);
    }
    return true;
}

bool FakeLag::handleOutgoingPacket(JNIEnv* env, jobject packet) {
    
    
    
    
    
    if (!enabled || !active_.load(std::memory_order_acquire) || tlSelfRelease)
        return true;

    if (settings.flushOnAttack && isAttackPacket(env, packet)) {
        
        
        
        if (queuedPacketCount() > 0) {
            draining_.store(true, std::memory_order_release);
            flushRequested_.store(true, std::memory_order_release);
            return !queuePacket(env, packet, false);
        }
        flushRequested_.store(true, std::memory_order_release);
        return true;
    }

    if (!isMovementPacket(env, packet))
        return true;

    if (!shouldQueueMovement()) {
        flushRequested_.store(true, std::memory_order_release);
        draining_.store(true, std::memory_order_release);
        
        
        if (queuedPacketCount() > 0)
            return !queuePacket(env, packet);
        return true;
    }

    return !queuePacket(env, packet);
}

bool FakeLag::queuePacket(JNIEnv* env, jobject packet, bool movement) {
    jobject global = env ? env->NewGlobalRef(packet) : nullptr;
    if (!global || (env && env->ExceptionCheck())) {
        if (env && env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    QueuedPacket entry;
    entry.packet = global;
    entry.queuedAtMs = nowMs();
    entry.movement = movement;
    if (player.isValid()) {
        entry.x = player.getPosX();
        entry.y = player.getPosY();
        entry.z = player.getPosZ();
    }

    QueuedPacket poppedEntry;
    bool hasPopped = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        
        const int maxPackets = effectivePacketLimit() + (movement ? 0 : 2);
        if (static_cast<int>(queuedPackets_.size()) >= maxPackets && !queuedPackets_.empty()) {
            poppedEntry = queuedPackets_.front();
            queuedPackets_.pop_front();
            queuedPackets_.push_back(entry);
            hasPopped = true;
        } else {
            queuedPackets_.push_back(entry);
        }
    }

    if (hasPopped && poppedEntry.packet) {
        
        
        
        
        
        
        env->DeleteGlobalRef(poppedEntry.packet);
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }
    return true;
}

int FakeLag::effectivePacketLimit() const {
    int limit = (std::clamp)(settings.maxPackets, 8, kHardPacketLimit);
    if (!player.isValid())
        return limit;

    if (!player.isOnGround())
        limit = (std::min)(limit, kAirPacketLimit);

    const float speed = static_cast<float>(std::hypot(player.getMotionX(), player.getMotionZ()));
    if (speed >= kFastMovementSpeed)
        limit = (std::min)(limit, kFastPacketLimit);
    return limit;
}

std::size_t FakeLag::queuedPacketCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return queuedPackets_.size();
}

void FakeLag::releaseEligible(std::uint64_t ageMs, int limit) {
    std::lock_guard<std::mutex> releaseLock(releaseMutex_);
    JNIEnv* env = Java::Env();
    if (!env || limit <= 0)
        return;

    
    
    if (!player.isValid()) {
        std::deque<QueuedPacket> discard;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            discard.swap(queuedPackets_);
        }
        for (auto& e : discard)
            if (e.packet)
                env->DeleteGlobalRef(e.packet);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return;
    }

    std::vector<QueuedPacket> send;
    const std::uint64_t now = nowMs();
    if (limit != INT_MAX) {
        const int releaseRate = (std::clamp)(settings.releaseRate, 1, 6);
        const std::uint64_t spacingMs =
            static_cast<std::uint64_t>((std::max)(8, 50 / releaseRate));
        const std::uint64_t lastRelease = lastReleaseMs_.load(std::memory_order_acquire);
        if (lastRelease != 0 && now - lastRelease < spacingMs)
            return;
        
        
        limit = 1;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!queuedPackets_.empty() && static_cast<int>(send.size()) < limit) {
            if (now - queuedPackets_.front().queuedAtMs < ageMs)
                break;
            send.push_back(queuedPackets_.front());
            queuedPackets_.pop_front();
        }
    }

    if (send.empty())
        return;

    releasing_.store(true, std::memory_order_release);
    for (const auto& entry : send) {
        if (entry.packet) {
            
            
            
            tlSelfRelease = true;
            mc.sendPacket(entry.packet);
            tlSelfRelease = false;
        }
        if (entry.movement) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            serverX_ = entry.x;
            serverY_ = entry.y;
            serverZ_ = entry.z;
            hasServerPosition_ = true;
        }
        if (entry.packet)
            env->DeleteGlobalRef(entry.packet);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        lastReleaseMs_.store(nowMs(), std::memory_order_release);
    }
    releasing_.store(false, std::memory_order_release);
}

void FakeLag::flushQueuedPackets(bool all) {
    const int limit = all ? INT_MAX : (std::clamp)(settings.releaseRate, 1, 6);
    releaseEligible(0, limit);
}

void FakeLag::clearQueuedPackets() {
    std::lock_guard<std::mutex> releaseLock(releaseMutex_);
    JNIEnv* env = Java::Env();
    if (!env)
        return;

    std::deque<QueuedPacket> discarded;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        discarded.swap(queuedPackets_);
    }
    for (const auto& entry : discarded) {
        if (entry.packet)
            env->DeleteGlobalRef(entry.packet);
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

void FakeLag::samplePing(std::uint64_t now) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (lastPingSampleMs_ != 0 && now - lastPingSampleMs_ < kPingSampleIntervalMs)
            return;
        lastPingSampleMs_ = now;
    }

    const int ping = mc.getLocalPlayerPing();
    if (ping < 0 || ping > kMaximumUsablePingMs)
        return;

    std::lock_guard<std::mutex> lock(stateMutex_);
    rawPingMs_ = ping;
    if (smoothedPingMs_ < 0.0f)
        smoothedPingMs_ = static_cast<float>(ping);
    else
        smoothedPingMs_ += (static_cast<float>(ping) - smoothedPingMs_) * kPingSmoothingAlpha;
}

float FakeLag::serverDistance() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!hasServerPosition_ || !player.isValid())
        return 0.0f;
    const double dx = player.getPosX() - serverX_;
    const double dy = player.getPosY() - serverY_;
    const double dz = player.getPosZ() - serverZ_;
    return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

int FakeLag::calculateTargetPing() {
    const int minimum = (std::clamp)(settings.minTargetPingMs, 40, kSafeTargetPingCeilingMs);
    const int maximum =
        (std::clamp)(settings.maxTargetPingMs, minimum, kSafeTargetPingCeilingMs);
    if (settings.mode != Mode::Adaptive || !player.isValid())
        return maximum;

    const float speed = static_cast<float>(std::hypot(player.getMotionX(), player.getMotionZ()));
    float factor = std::clamp(speed / 0.30f, 0.0f, 1.0f) * 0.72f;
    if (player.isSprinting())
        factor += 0.12f;
    if (!player.isOnGround())
        factor *= 0.20f;
    if (mc.isTargetingEntity())
        factor -= 0.30f;
    if (player.getHurtTime() > 0 || player.isUsingItem())
        factor = 0.0f;

    std::size_t queueSize = 0;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueSize = queuedPackets_.size();
    }
    const float queueLimit = static_cast<float>(effectivePacketLimit());
    factor -= std::clamp(static_cast<float>(queueSize) / queueLimit - 0.45f, 0.0f, 0.55f);
    factor -= std::clamp(serverDistance() / (std::max)(settings.maxDistance, 0.5f) - 0.50f, 0.0f, 0.55f);
    factor = std::clamp(factor, 0.0f, 1.0f) * std::clamp(settings.adaptiveStrength, 0.0f, 1.0f);

    return minimum + static_cast<int>(std::lround(static_cast<float>(maximum - minimum) * factor));
}

int FakeLag::calculateHoldMs(int targetPingMs) const {
    float measuredPing = -1.0f;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        measuredPing = smoothedPingMs_;
    }
    if (measuredPing < 0.0f)
        return 0;

    
    
    const float extraOneWay = (static_cast<float>(targetPingMs) - measuredPing) * 0.5f;
    int holdMs =
        (std::clamp)(static_cast<int>(std::lround(extraOneWay)), 0, kMaximumHoldMs);

    if (!player.isValid())
        return holdMs;

    const float speed = static_cast<float>(std::hypot(player.getMotionX(), player.getMotionZ()));
    if (!player.isOnGround())
        holdMs = (std::min)(holdMs, 25);
    else if (speed >= kVeryFastMovementSpeed)
        holdMs = (std::min)(holdMs, 20);
    else if (speed >= kFastMovementSpeed)
        holdMs = (std::min)(holdMs, 45);
    return holdMs;
}

void FakeLag::onUpdate() {
    if (!enabled)
        return;

    JNIEnv* env = Java::Env();
    jobject world = env ? mc.getWorld() : nullptr;
    const bool hasWorld = world != nullptr;
    if (env && world)
        env->DeleteLocalRef(world);
    if (!env || !player.isValid() || !hasWorld) {
        clearQueuedPackets();
        draining_.store(false, std::memory_order_release);
        flushRequested_.store(false, std::memory_order_release);
        lastReleaseMs_.store(0, std::memory_order_release);
        resetRuntimeState();
        return;
    }

    const std::uint64_t now = nowMs();
    samplePing(now);
    const int targetPing = calculateTargetPing();
    const int holdMs = calculateHoldMs(targetPing);
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentTargetPingMs_ = targetPing;
        currentHoldMs_ = holdMs;
    }

    if (flushRequested_.exchange(false, std::memory_order_acq_rel)) {
        draining_.store(true, std::memory_order_release);
    }

    if (draining_.load(std::memory_order_acquire)) {
        releaseEligible(0, 1);
        if (queuedPacketCount() == 0) {
            draining_.store(false, std::memory_order_release);
            resetAnchor();
        }
        return;
    }

    const int hurtTime = player.getHurtTime();
    if (settings.flushOnDamage && hurtTime > previousHurtTime_) {
        draining_.store(true, std::memory_order_release);
        releaseEligible(0, 1);
    }
    previousHurtTime_ = hurtTime;

    if (settings.flushOnItemUse && player.isUsingItem()) {
        draining_.store(true, std::memory_order_release);
        releaseEligible(0, 1);
        return;
    }

    if (holdMs <= 0) {
        draining_.store(true, std::memory_order_release);
        releaseEligible(0, 1);
        return;
    }

    const float safeDistance = (std::max)(settings.maxDistance, 0.5f);
    const float distance = serverDistance();
    if (settings.smoothRelease && distance >= safeDistance * 0.65f)
        flushQueuedPackets(false);
    if (distance >= safeDistance) {
        draining_.store(true, std::memory_order_release);
        releaseEligible(0, 1);
        return;
    }

    const int releaseRate = (std::clamp)(settings.releaseRate, 1, 6);
    switch (settings.mode) {
        case Mode::Pulse: {
            const int pulseHold = (std::max)(settings.pulseHoldMs, 20);
            const int pulseRelease = (std::max)(settings.pulseReleaseMs, 20);
            const std::uint64_t phase = (now - cycleStartMs_) %
                                        static_cast<std::uint64_t>(pulseHold + pulseRelease);
            if (phase >= static_cast<std::uint64_t>(pulseHold)) {
                draining_.store(true, std::memory_order_release);
                releaseEligible(0, 1);
            } else {
                
                
                releaseEligible(static_cast<std::uint64_t>(holdMs), releaseRate);
            }
            break;
        }
        case Mode::Constant:
        case Mode::Distance:
        case Mode::Adaptive:
            releaseEligible(static_cast<std::uint64_t>(holdMs), releaseRate);
            break;
    }
}

void FakeLag::renderPlayerBox(double x, double y, double z, unsigned int outline, unsigned int fill) {
    if (!RenderData::updated)
        return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    RenderPos render = mc.getRenderPos();
    constexpr float half = 0.3f;
    constexpr float height = 1.8f;
    Vector3 corners[8] = {
        {(float)(x - half - render.x), (float)(y - render.y), (float)(z - half - render.z)},
        {(float)(x + half - render.x), (float)(y - render.y), (float)(z - half - render.z)},
        {(float)(x + half - render.x), (float)(y - render.y), (float)(z + half - render.z)},
        {(float)(x - half - render.x), (float)(y - render.y), (float)(z + half - render.z)},
        {(float)(x - half - render.x), (float)(y + height - render.y), (float)(z - half - render.z)},
        {(float)(x + half - render.x), (float)(y + height - render.y), (float)(z - half - render.z)},
        {(float)(x + half - render.x), (float)(y + height - render.y), (float)(z + half - render.z)},
        {(float)(x - half - render.x), (float)(y + height - render.y), (float)(z + half - render.z)}};
    Vector2 screen[8]{};
    bool visible[8]{};
    int visibleCount = 0;
    for (int i = 0; i < 8; ++i) {
        visible[i] = CWorldToScreen::WorldToScreen(corners[i], RenderData::modelView, RenderData::projection,
                                                   RenderData::screenWidth, RenderData::screenHeight, screen[i]);
        visibleCount += visible[i] ? 1 : 0;
    }
    if (visibleCount < 3)
        return;

    static constexpr int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                                        {2, 3, 7, 6}, {0, 3, 7, 4}, {1, 2, 6, 5}};
    if (settings.filledRender) {
        for (const auto& face : faces) {
            if (!visible[face[0]] || !visible[face[1]] || !visible[face[2]] || !visible[face[3]])
                continue;
            ImVec2 points[4] = {{screen[face[0]].x, screen[face[0]].y}, {screen[face[1]].x, screen[face[1]].y},
                                {screen[face[2]].x, screen[face[2]].y}, {screen[face[3]].x, screen[face[3]].y}};
            draw->AddConvexPolyFilled(points, 4, fill);
        }
    }
    static constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                         {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
        if (visible[edge[0]] && visible[edge[1]])
            draw->AddLine({screen[edge[0]].x, screen[edge[0]].y}, {screen[edge[1]].x, screen[edge[1]].y},
                          outline, 2.0f);
    }
}

void FakeLag::onRender() {
    if (!enabled || !player.isValid())
        return;

    double serverX = 0.0;
    double serverY = 0.0;
    double serverZ = 0.0;
    int ping = -1;
    int targetPing = 0;
    int holdMs = 0;
    bool hasServerPosition = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        hasServerPosition = hasServerPosition_;
        serverX = serverX_;
        serverY = serverY_;
        serverZ = serverZ_;
        ping = smoothedPingMs_ >= 0.0f ? static_cast<int>(std::lround(smoothedPingMs_)) : rawPingMs_;
        targetPing = currentTargetPingMs_;
        holdMs = currentHoldMs_;
    }

    std::size_t queued = 0;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queued = queuedPackets_.size();
    }

    const ImVec4 base(settings.color[0], settings.color[1], settings.color[2], settings.color[3]);
    const ImU32 outline = ImGui::ColorConvertFloat4ToU32(base);
    const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x, base.y, base.z, base.w * 0.20f));

    
    
    if (queued > 0 && hasServerPosition && settings.renderServerPlayer && !mc.isFirstPerson())
        renderPlayerBox(serverX, serverY, serverZ, outline, fill);

    if (queued > 0 && hasServerPosition && settings.renderLine && RenderData::updated) {
        RenderPos render = mc.getRenderPos();
        Vector2 a{}, b{};
        const bool va = CWorldToScreen::WorldToScreen(
            {(float)(serverX - render.x), (float)(serverY + 0.9 - render.y), (float)(serverZ - render.z)},
            RenderData::modelView, RenderData::projection, RenderData::screenWidth, RenderData::screenHeight, a);
        const bool vb = CWorldToScreen::WorldToScreen(
            {(float)(player.getPosX() - render.x), (float)(player.getPosY() + 0.9 - render.y),
             (float)(player.getPosZ() - render.z)},
            RenderData::modelView, RenderData::projection, RenderData::screenWidth, RenderData::screenHeight, b);
        if (va && vb) {
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw)
                draw->AddLine({a.x, a.y}, {b.x, b.y}, outline, 1.5f);
        }
    }

    if (settings.showStatus) {
        static const char* modes[] = {"Pulse", "Constant", "Distance", "Adaptive"};
        const int mode = (std::clamp)(static_cast<int>(settings.mode), 0, 3);
        char text[144];
        if (ping >= 0) {
            std::snprintf(text, sizeof(text), "FakeLag %s | RTT %d ms | target %d ms | hold %d ms | %zu pkt",
                          modes[mode], ping, targetPing, holdMs, queued);
        } else {
            std::snprintf(text, sizeof(text), "FakeLag %s | measuring RTT...", modes[mode]);
        }

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw)
            return;
        ImVec2 size = ImGui::CalcTextSize(text);
        const float x = (ImGui::GetIO().DisplaySize.x - size.x) * 0.5f;
        draw->AddRectFilled({x - 8.0f, 48.0f}, {x + size.x + 8.0f, 70.0f}, IM_COL32(10, 12, 16, 175), 5.0f);
        draw->AddText({x, 52.0f}, outline, text);
    }
}

void FakeLag::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Fake Lag",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);

            int mode = static_cast<int>(settings.mode);
            const char* modes[] = {"Pulse", "Constant", "Distance", "Adaptive"};
            if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                settings.mode = static_cast<Mode>(mode);

            if (settings.mode == Mode::Adaptive) {
                ui::SliderInt("Minimum Target Ping", &settings.minTargetPingMs, 40,
                              kSafeTargetPingCeilingMs, "%d ms");
                ui::SliderInt("Maximum Target Ping", &settings.maxTargetPingMs, 40,
                              kSafeTargetPingCeilingMs, "%d ms");
                ui::SliderFloat("Adaptive Strength", &settings.adaptiveStrength, 0.0f, 1.0f, "%.2f");
            } else {
                ui::SliderInt("Target Ping", &settings.maxTargetPingMs, 40,
                              kSafeTargetPingCeilingMs, "%d ms");
            }

            if (settings.mode == Mode::Pulse) {
                ui::SliderInt("Hold Phase", &settings.pulseHoldMs, 40, 1000, "%d ms");
                ui::SliderInt("Release Phase", &settings.pulseReleaseMs, 20, 500, "%d ms");
            }
            if (settings.mode == Mode::Distance || settings.mode == Mode::Adaptive)
                ui::SliderFloat("Maximum Distance", &settings.maxDistance, 0.5f, 3.0f, "%.1f blocks");

            ui::SliderInt("Packet Limit", &settings.maxPackets, 8, kHardPacketLimit, "%d");
            ui::SliderInt("Release Rate", &settings.releaseRate, 1, 6, "%d/tick");
            ui::TickTooltip();
            ui::Checkbox("Moving Only", &settings.movingOnly);
            ui::Checkbox("Smooth Release", &settings.smoothRelease);
            ui::Checkbox("Flush On Attack", &settings.flushOnAttack);
            ui::Checkbox("Flush On Damage", &settings.flushOnDamage);
            ui::Checkbox("Flush On Item Use", &settings.flushOnItemUse);
            ImGui::Spacing();
            ImGui::TextDisabled("Hold = (target ping - measured RTT) / 2");
        },
        "Visuals",
        [this]() {
            ui::Checkbox("Render Server Player", &settings.renderServerPlayer);
            ui::Checkbox("Filled Render", &settings.filledRender);
            ui::Checkbox("Render Line", &settings.renderLine);
            ui::Checkbox("Show Status", &settings.showStatus);
            ImGui::Spacing();
            ImGui::TextDisabled("Player box is hidden in first person.");
            ui::color("Color", settings.color);
        });
}

std::string FakeLag::getDisplayInfo() {
    static const char* names[] = {"Pulse", "Constant", "Distance", "Adaptive"};
    const int mode = (std::clamp)(static_cast<int>(settings.mode), 0, 3);
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (smoothedPingMs_ < 0.0f)
        return std::string(names[mode]) + " measuring";
    return std::string(names[mode]) + " " + std::to_string(currentHoldMs_) + "ms";
}



void FakeLag::saveSettings(json::Value& node) const {
    node["mode"] = static_cast<int>(settings.mode);
    node["minTargetPingMs"] = settings.minTargetPingMs;
    node["maxTargetPingMs"] = settings.maxTargetPingMs;
    node["pulseHoldMs"] = settings.pulseHoldMs;
    node["pulseReleaseMs"] = settings.pulseReleaseMs;
    node["maxDistance"] = settings.maxDistance;
    node["adaptiveStrength"] = settings.adaptiveStrength;
    node["maxPackets"] = settings.maxPackets;
    node["releaseRate"] = settings.releaseRate;
    node["movingOnly"] = settings.movingOnly;
    node["smoothRelease"] = settings.smoothRelease;
    node["flushOnAttack"] = settings.flushOnAttack;
    node["flushOnDamage"] = settings.flushOnDamage;
    node["flushOnItemUse"] = settings.flushOnItemUse;
    node["renderServerPlayer"] = settings.renderServerPlayer;
    node["filledRender"] = settings.filledRender;
    node["renderLine"] = settings.renderLine;
    node["showStatus"] = settings.showStatus;
    json::writeColor(node, "color", settings.color);
}

void FakeLag::loadSettings(const json::Value& node) {
    settings.mode = static_cast<Mode>(
        (std::clamp)(node["mode"].as_int(static_cast<int>(settings.mode)), 0, 3));
    settings.minTargetPingMs =
        (std::clamp)(node["minTargetPingMs"].as_int(settings.minTargetPingMs), 40, 260);
    settings.maxTargetPingMs =
        (std::clamp)(node["maxTargetPingMs"].as_int(settings.maxTargetPingMs),
                     settings.minTargetPingMs, 260);
    settings.pulseHoldMs = (std::clamp)(node["pulseHoldMs"].as_int(settings.pulseHoldMs), 40, 1000);
    settings.pulseReleaseMs = (std::clamp)(node["pulseReleaseMs"].as_int(settings.pulseReleaseMs), 20, 500);
    settings.maxDistance = (std::clamp)(node["maxDistance"].as_float(settings.maxDistance), 0.5f, 3.0f);
    settings.adaptiveStrength =
        (std::clamp)(node["adaptiveStrength"].as_float(settings.adaptiveStrength), 0.0f, 1.0f);
    settings.maxPackets = (std::clamp)(node["maxPackets"].as_int(settings.maxPackets), 8, 24);
    settings.releaseRate = (std::clamp)(node["releaseRate"].as_int(settings.releaseRate), 1, 6);
    settings.movingOnly = node["movingOnly"].as_bool(settings.movingOnly);
    settings.smoothRelease = node["smoothRelease"].as_bool(settings.smoothRelease);
    settings.flushOnAttack = node["flushOnAttack"].as_bool(settings.flushOnAttack);
    settings.flushOnDamage = node["flushOnDamage"].as_bool(settings.flushOnDamage);
    settings.flushOnItemUse = node["flushOnItemUse"].as_bool(settings.flushOnItemUse);
    settings.renderServerPlayer = node["renderServerPlayer"].as_bool(settings.renderServerPlayer);
    settings.filledRender = node["filledRender"].as_bool(settings.filledRender);
    settings.renderLine = node["renderLine"].as_bool(settings.renderLine);
    settings.showStatus = node["showStatus"].as_bool(settings.showStatus);
    json::readColor(node, "color", settings.color);
}
