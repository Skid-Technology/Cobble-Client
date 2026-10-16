#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "../../sdk/core/minecraft.h"
#include "../module.h"

class Backtrack : public Module {
public:
    
    
    
    enum class Mode { Always = 0, OnlyOnHit = 1 };
    struct HoldIntent {
        int targetEntityId = -1;
        std::uint64_t engagementEpoch = 0;
        bool active = false;
        
        
        
        
        bool streamHold = false;
    };

    struct HeldTargetSnapshot {
        bool valid = false;
        bool active = false;
        int entityId = -1;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        std::uint64_t engagementEpoch = 0;
        std::uint64_t connectionEpoch = 0;
        std::uint64_t sequence = 0;
        std::uint32_t pendingPackets = 0;
    };

    Backtrack();

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onRender() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;
    std::string getDisplayInfo() override;

    void onClientTick(JNIEnv* env);

    
    
    void notifyHitLanded(int entityId);
    bool needsClientTick() const;

    HoldIntent holdIntent() const;
    HeldTargetSnapshot heldTargetSnapshot() const;

    struct Settings {
        
        
        
        
        
        Mode mode = Mode::Always;

        
        
        
        
        int holdMs = 180;
        
        
        int maxPackets = 32;

        
        
        
        
        
        
        bool wholeStream = true;
        
        
        float minRange = 0.0f;
        float maxRange = 6.0f;
        
        
        
        bool onlyWhileAttacking = false;

        
        
        
        
        
        
        float color[4] = {0.42f, 0.92f, 1.0f, 1.0f};
    } settings;

private:
    struct BoxAnimation {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float alpha = 0.0f;
        int entityId = -1;
        std::uint64_t engagementEpoch = 0;
        std::uint64_t connectionEpoch = 0;
        std::uint64_t visualEpoch = 0;
        bool initialized = false;
    };

    jobject targetedPlayer(JNIEnv* env) const;
    
    
    bool attackSourceActive() const;
    
    
    
    bool targetWithinRange(JNIEnv* env, jobject target) const;
    void publishIntent(const HoldIntent& intent);
    void closeAdmission();
    
    
    void closeStream();
    
    
    void tickWholeStream(JNIEnv* env);
    bool flushPending(JNIEnv* env);

    mutable std::recursive_mutex controlMutex_;
    std::atomic<std::uint64_t> intentSequence_{0};
    std::atomic<int> intentTarget_{-1};
    std::atomic<std::uint64_t> intentEpoch_{0};
    std::atomic<bool> intentActive_{false};
    std::atomic<bool> intentStreamHold_{false};
    std::atomic<bool> drainRequested_{false};
    std::atomic<bool> discardRequested_{false};
    std::atomic<std::uint64_t> visualEpoch_{1};
    BoxAnimation boxAnimation_;

    
    
    
    std::atomic<int> hitEntityId_{-1};
    std::atomic<std::uint64_t> hitTimestamp_{0};  
};

extern Backtrack* backtrack;
