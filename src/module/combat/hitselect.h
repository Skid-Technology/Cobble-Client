#pragma once

#include <chrono>
#include <random>
#include "../module.h"

class HitSelect : public Module {
public:
    HitSelect();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        
        int minComboHits = 0;

        
        int comboDetectionTime = 100;

        
        bool waitForFirstHit = true;

        
        int waitDuration = 500;

        
        bool weaponOnly = true;
    } settings;

    
    
    bool shouldAllowClick();

private:
    std::chrono::steady_clock::time_point lastHitTime;
    std::chrono::steady_clock::time_point comboStartTime;
    std::chrono::steady_clock::time_point lastAllowedClick;

    int comboHitCount = 0;
    bool inCombo = false;
    bool firstHitLanded = false;

    
    int lastTargetHurtTime = 0;
    bool hasTarget = false;

    
    bool isHoldingWeapon();
    int getHeldItemId();
    void detectHit();
};

extern HitSelect* hitSelect;
