#pragma once

#include "../module.h"
#include "../../jni/java.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>

class TimerRange : public Module {
public:
    enum class PacketPhase {
        PRE = 0,
        POST = 1
    };

    enum class TriggerMode {
        BeforeHit = 0,
        AfterHit = 1,
        PerHit = 2,
        Constant = 3
    };

    struct Settings {
        TriggerMode mode = TriggerMode::PerHit;
        float minSpeed = 0.8f;
        float maxSpeed = 1.2f;
        int offsetMs = 40;
        int durationMs = 120;
        bool targetOnly = true;
        bool weaponOnly = false;
    } settings;

    TimerRange();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void onOutgoingPacket(JNIEnv* env, jobject packet, PacketPhase phase);
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

private:
    static std::uint64_t nowMs();
    bool conditionsMet() const;
    void activate(std::uint64_t atMs);
    void selectSpeed();

    std::mt19937 rng_;
    std::mutex rngMutex_;
    std::atomic<float> currentSpeed_{1.0f};
    std::atomic<std::uint64_t> pendingAtMs_{0};
    std::atomic<std::uint64_t> activeUntilMs_{0};
    bool wasAttacking_ = false;
};

extern TimerRange* timerRange;
