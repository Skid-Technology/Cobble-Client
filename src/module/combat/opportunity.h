#pragma once

#include <jni.h>

#include "../module.h"

#include <atomic>
#include <cstdint>
#include <random>
#include <string>
































class Opportunity : public Module {
public:
    
    enum class Play : int { Cobweb = 0, Lava = 1, Fire = 2, Water = 3, Count = 4 };
    static constexpr int kPlayCount = static_cast<int>(Play::Count);

    struct PlaySettings {
        bool enabled = true;
        float minDist = 1.0f;
        float maxDist = 4.0f;
        float maxTargetHealth = 20.0f;  
        int chance = 100;               
        int cooldownMs = 1500;          
    };

    struct Settings {
        PlaySettings play[kPlayCount];

        float reach = 4.2f;          
        float selfSafeDist = 2.5f;   
        bool requireLineOfSight = true;
        bool ignoreTeam = true;      
        bool ignoreInvisible = true;

        int globalCooldownMs = 700;  

        
        int reactMinMs = 140;
        int reactMaxMs = 320;
        int aimTicks = 3;
        float aimNoise = 0.7f;
        bool overshoot = true;
        bool snapBack = true;
        bool restoreSlot = true;
    } settings;

    Opportunity();

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    
    void onClientTick(JNIEnv* env);
    bool needsClientTick() const {
        return enabled || owningRotation_.load(std::memory_order_acquire);
    }

    
    bool isRewritingRotation() const { return owningRotation_.load(std::memory_order_acquire); }

    static const char* playName(Play play);

private:
    struct Shot {
        Play play = Play::Cobweb;
        int entityId = 0;  
        int cellX = 0;  
        int cellY = 0;
        int cellZ = 0;
        int supportX = 0;  
        int supportY = 0;
        int supportZ = 0;
        double hitX = 0.0;
        double hitY = 0.0;
        double hitZ = 0.0;
        int slot = -1;
        bool valid = false;
    };

    enum class Phase { Idle, React, Aim, Place, Settle };

    static std::uint64_t nowMs();
    static float wrapDegrees(float angle);
    static float smoothstep(float t);

    
    bool anchorFor(int cellX, int cellY, int cellZ, Shot& out) const;
    int findItemSlot(Play play) const;
    
    bool evaluate(Shot& out);
    
    
    bool refreshShot(Shot& shot) const;
    bool stillValid(const Shot& shot) const;
    bool hasLineOfSight(double toX, double toY, double toZ) const;

    void aimAt(const Shot& shot, bool freshOpportunity);
    void beginSettle();
    void abort(JNIEnv* env);
    void restoreSlot();
    void applyRotation(float yaw, float pitch);
    int rollReactionMs();
    bool rollChance(int percent);

    
    Phase phase_ = Phase::Idle;
    Shot shot_;
    int originalSlot_ = -1;
    
    
    int hotbar_[9] = {};
    int heldSlot_ = -1;
    std::uint64_t reactDeadlineMs_ = 0;
    std::uint64_t restoreSlotAtMs_ = 0;
    std::uint64_t nextAnyPlayMs_ = 0;
    std::uint64_t nextPlayMs_[kPlayCount] = {};
    std::uint64_t declinedUntilMs_[kPlayCount] = {};
    int aimTick_ = 0;
    int aimTotalTicks_ = 0;
    float aimFromYaw_ = 0.0f;
    float aimFromPitch_ = 0.0f;
    float aimToYaw_ = 0.0f;
    float aimToPitch_ = 0.0f;
    float returnYaw_ = 0.0f;
    float returnPitch_ = 0.0f;
    bool haveReturnView_ = false;
    std::mt19937 rng_{std::random_device{}()};

    std::atomic<bool> owningRotation_{false};
    std::atomic<const char*> status_{nullptr};

    
    int editPlay_ = 0;
};

extern Opportunity* opportunity;
