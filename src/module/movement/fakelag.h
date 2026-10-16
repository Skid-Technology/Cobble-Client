#pragma once

#include "../module.h"
#include "../../jni/java.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>

class FakeLag : public Module {
public:
    enum class Mode {
        Pulse = 0,
        Constant = 1,
        Distance = 2,
        Adaptive = 3
    };

    struct Settings {
        Mode mode = Mode::Adaptive;
        int minTargetPingMs = 90;
        int maxTargetPingMs = 180;
        int pulseHoldMs = 120;
        int pulseReleaseMs = 140;
        float maxDistance = 1.8f;
        float adaptiveStrength = 0.55f;
        int maxPackets = 24;
        int releaseRate = 4;
        bool movingOnly = true;
        bool smoothRelease = true;
        bool flushOnAttack = true;
        bool flushOnDamage = true;
        bool flushOnItemUse = true;
        bool renderServerPlayer = true;
        bool filledRender = true;
        bool renderLine = true;
        bool showStatus = true;
        float color[4] = {0.36f, 0.72f, 1.0f, 0.82f};
    } settings;

    FakeLag();
    ~FakeLag() override;

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    bool handleOutgoingPacket(JNIEnv* env, jobject packet);
    void flushQueuedPackets(bool all = true);
    void clearQueuedPackets();
    bool isReleasing() const { return releasing_.load(std::memory_order_acquire); }

private:
    struct QueuedPacket {
        jobject packet = nullptr;
        std::uint64_t queuedAtMs = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        bool movement = true;
    };

    static std::uint64_t nowMs();
    bool isMovementPacket(JNIEnv* env, jobject packet) const;
    bool isAttackPacket(JNIEnv* env, jobject packet) const;
    bool shouldQueueMovement() const;
    bool queuePacket(JNIEnv* env, jobject packet, bool movement = true);
    int effectivePacketLimit() const;
    std::size_t queuedPacketCount() const;
    void releaseEligible(std::uint64_t ageMs, int limit);
    void samplePing(std::uint64_t now);
    int calculateTargetPing();
    int calculateHoldMs(int targetPingMs) const;
    void renderPlayerBox(double x, double y, double z, unsigned int outline, unsigned int fill);
    float serverDistance() const;
    void resetAnchor();
    void resetRuntimeState();

    mutable std::mutex queueMutex_;
    mutable std::mutex stateMutex_;
    mutable std::mutex releaseMutex_;
    std::deque<QueuedPacket> queuedPackets_;
    std::atomic<bool> releasing_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> flushRequested_{false};
    std::atomic<bool> draining_{false};
    std::atomic<std::uint64_t> lastReleaseMs_{0};

    std::uint64_t cycleStartMs_ = 0;
    std::uint64_t lastPingSampleMs_ = 0;
    int rawPingMs_ = -1;
    float smoothedPingMs_ = -1.0f;
    int currentTargetPingMs_ = 0;
    int currentHoldMs_ = 0;
    int previousHurtTime_ = 0;
    double serverX_ = 0.0;
    double serverY_ = 0.0;
    double serverZ_ = 0.0;
    bool hasServerPosition_ = false;
};

extern FakeLag* fakeLag;
