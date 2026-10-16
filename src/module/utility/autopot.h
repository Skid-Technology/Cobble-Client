#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

#include "../module.h"


enum class AutoPotAim {
    Silent = 0,  
    Snap = 1,    
    Smooth = 2,  
    None = 3     
};




enum class AutoPotPreset {
    Instant = 0,  
    Rapid = 1,    
    Human = 2,    
    Legit = 3,    
    Custom = 4
};























class AutoPot : public Module {
public:
    AutoPot();

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    
    
    void onOutgoingPacket(JNIEnv* env, jobject packet);
    bool needsOutgoingPacket() const;
    bool isRewritingRotation() const;

    
    
    bool isBusy() const { return state_ != State::Idle; }

    struct Settings {
        
        int healthPercent = 55;   
        int targetPercent = 95;   
        int maxPots = 3;          
        bool allowHealI = true;   
        bool preferStrong = true; 
        bool requireGround = true;
        bool onlyInCombat = false;
        int combatWindowMs = 4000;

        
        int preset = static_cast<int>(AutoPotPreset::Human);
        int reactionMs = 140;    
        int reactionJitter = 45; 
        int switchMs = 55;       
        int throwGapMs = 210;    
        int restoreMs = 90;      
        bool fatigue = true;     

        
        int aim = static_cast<int>(AutoPotAim::Silent);
        int pitch = 87;      
        int pitchJitter = 4; 
        int flickMs = 90;    
        bool switchBack = true;

        
        bool retryOnFail = true;
        int inFlightMs = 450;   
        int misclickChance = 0; 
    } settings;

    
    
    void applyPreset(AutoPotPreset preset);

private:
    enum class State {
        Idle,
        Arming,      
        Switching,   
        Aiming,      
        Throwing,    
        Confirming,  
        Cadence,     
        Restoring    
    };

    struct PotionChoice {
        int slot = -1;
        int damage = -1;
        int stackSize = 0;
        float heal = 0.0f;
    };

    static constexpr int kPotionItemId = 373;
    static constexpr int kPollIntervalMs = 6;
    static constexpr int kScanIntervalMs = 200;
    static constexpr int kUseTimeoutMs = 800;
    static constexpr int kConfirmTimeoutMs = 400;
    static constexpr int kMaxRetries = 2;

    
    State state_ = State::Idle;
    int64_t stateStartMs_ = 0;
    int64_t stateDelayMs_ = 0;  

    PotionChoice target_{};
    int originalSlot_ = -1;
    float originalPitch_ = 0.0f;
    bool pitchModified_ = false;
    float aimPitch_ = 90.0f;
    float flickFrom_ = 0.0f;
    int thrown_ = 0;
    int retries_ = 0;
    int misclickSlot_ = -1;

    float maxHealth_ = 20.0f;
    float lastHealth_ = 20.0f;
    int64_t lastMaxHealthMs_ = 0;
    int64_t lastPollMs_ = 0;
    int64_t lastScanMs_ = 0;
    int64_t lastUiScanMs_ = 0;  
    int64_t lastDamageMs_ = 0;
    int64_t lowSinceMs_ = 0;   
    int64_t lastThrowMs_ = 0;
    int burstFatigue_ = 0;     

    
    float inFlightHeal_ = 0.0f;
    int64_t inFlightUntilMs_ = 0;
    float healthAtThrow_ = 0.0f;

    
    std::atomic<bool> rotationRequested_{false};
    std::atomic<bool> rotationSent_{false};
    std::atomic<float> aimYaw_{0.0f};
    std::atomic<float> aimPitchPacket_{90.0f};
    std::atomic<bool> pendingUse_{false};
    std::atomic<bool> useDispatched_{false};
    std::atomic<bool> useSucceeded_{false};
    std::atomic<bool> silentAimOk_{true};

    
    
    
    mutable std::atomic<int> potsAvailable_{-1};

    mutable std::mt19937_64 rng_;

    
    int64_t jitter(int baseMs) const;
    int64_t jitter(int baseMs, int spreadPercent) const;
    bool roll(int percent) const;
    float fatigueScale() const;

    bool wantsPot(float predicted) const;
    bool pickPotion(PotionChoice& out) const;
    int countPotions() const;
    bool targetStillThere(bool requireFullStack) const;
    AutoPotAim effectiveAim() const;

    void beginBurst();
    void beginSwitch(int slot);
    void beginAim();
    void queueUse();
    void failStep();
    void beginRestore();
    void finishBurst();
    void resetSequence();
    void enterState(State next, int64_t delayMs);
};

extern AutoPot* autoPot;
