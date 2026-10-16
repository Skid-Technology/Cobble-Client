#pragma once

#include <jni.h>
#include <atomic>
#include <chrono>
#include <random>
#include "../module.h"

struct PlayerInfo;












































class AutoRod : public Module {
public:
    AutoRod();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    bool isSequencing() const { return phase_ != Phase::Idle; }
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    void onClientTick(JNIEnv* env);
    bool needsClientTick() const { return enabled; }

    struct Settings {
        bool onlyWhileAttacking = true;
        bool requireWeapon = true;
        bool autoHit = true;
        
        
        
        float minDistance = 3.6f;
        float maxDistance = 9.0f;
        int cooldownMs = 1600;   
        int jitterPercent = 35;  
    } settings;

private:
    
    enum class Phase { Idle, RodSelected, HookFlying, WeaponSelected };

    
    Phase phase_ = Phase::Idle;
    int waitTicks_ = 0;
    int rodSlot_ = -1;
    int weaponSlot_ = -1;
    int targetId_ = 0;  
    std::chrono::steady_clock::time_point lastSequence_{};
    int cooldownMsThisRound_ = 0;
    std::mt19937 rng_;

    
    bool prevAttackDown_ = false;
    std::chrono::steady_clock::time_point observedAutoClick_{};

    
    std::atomic<bool> requestPending_{false};

    static bool isWeapon(int itemId);
    int findRodSlot() const;
    int jitterTicks(int baseTicks);
    int rollCooldownMs();
    void abortSequence(bool restoreSlot);

    
    
    static int hookFlightTicks(double distance);
    
    
    bool measure(const PlayerInfo& target, double& distanceOut, double& angleOut) const;
    
    int pickTarget(double& distanceOut) const;
    
    double distanceToId(int entityId) const;
};

extern AutoRod* autoRod;
