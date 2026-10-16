#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include "../module.h"

class AutoClicker : public Module {
public:
    enum class RandomizationMode { RANDOM, SECURE_RANDOM, HUMAN };

    AutoClicker();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float minCPS = 14.0f;
        float maxCPS = 16.0f;
        bool weaponOnly = false;
        bool breakBlocks = false;  
        bool workInInventory = true;
        RandomizationMode randomizationMode = RandomizationMode::HUMAN;
        bool randomization = true;  

        
        bool stableCPS = false;
        float stableCPSValue = 17.0f;  

        
        bool advancedMode = true;
        float dropChance = 5.0f;    
        float spikeChance = 10.0f;  
        float spikeMultiplier = 1.3f;
        float kurtosis = 2.0f;  

        
        bool attackingOnly = false;  
    } settings;

private:
    std::chrono::steady_clock::time_point lastClick;
    std::chrono::steady_clock::time_point nextEventTime;
    std::chrono::steady_clock::time_point stableTimelineStart;
    std::chrono::steady_clock::time_point stableLastClick;  
    int stableClickCount = 0;
    int nextDelayMs = 100;
    std::mt19937 rng;
    std::mt19937_64 secureRng;
    uint64_t secureDrawCount = 0;
    float plusCpsState = 15.0f;
    int plusStateClicksRemaining = 0;
    float lastGeneratedMinCps = -1.0f;
    float lastGeneratedMaxCps = -1.0f;
    float clickDownFraction = 0.5f;
    float currentClickIntervalMs = 66.0f;
    float pendingGapMs = 66.0f;  

    
    
    
    int lastObservedHotbarSlot = -1;
    std::chrono::steady_clock::time_point manualUseYieldUntil{};

    
    int clicksSinceRangeChange = 0;
    int clicksUntilRangeChange = 50;
    float currentDelayMin = 25.0f;
    float currentDelayMax = 70.0f;

    
    bool lastClickWasDown = false;

    
    bool wasBreakingBlock = false;

    
    std::chrono::steady_clock::time_point sessionStart;
    float fatigueMultiplier = 1.0f;
    float fatigueTargetMultiplier = 1.0f;
    int fatiguePhaseClicksRemaining = 0;
    bool recoveringFromFatigue = true;

    
    bool inBurst = false;
    int burstClicksRemaining = 0;
    float burstSpeedMultiplier = 1.0f;

    
    
    float sigmaState = 0.22f;
    int sigmaClicksRemaining = 0;
    float sigmaTarget = 0.22f;
    float sigmaSessionOffset = 0.0f;  

    
    
    float tempoStepScale = 0.14f;   
    float tempoPull = 0.06f;        
    float hesitationChance = 0.03f;
    float doubleTapChance = 0.02f;
    int shapePhaseClicksRemaining = 0;

    
    
    
    std::array<float, 16> pinkRows{};
    int pinkCounter = 0;
    float pinkState = 0.0f;
    float pinkAmplitude = 0.10f;  

public:
    
    std::chrono::steady_clock::time_point lastAutoClick;

    void calculateNextDelay();
    void regenerateDelayRange();
    void doClick();

    
    void updateFatigue();
    void updateBurstMode();
    void updateSigmaState();
    void updateShapeParams();
    float nextPink();
    void reseedSecureRng();
    double secureUnit();
    float generateTargetCps(float minCps, float maxCps);
    float generateHumanIntervalMs(float minCps, float maxCps);

    
    bool isHoldingWeapon();
    bool isTargetingBlock();
    bool isTargetingPlayer();
    int getHeldItemId();

private:
    float delayAccumulator = 0.0f;
};

extern AutoClicker* autoClicker;
