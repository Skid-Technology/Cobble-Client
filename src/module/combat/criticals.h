#pragma once

#include "../module.h"

class Criticals : public Module {
public:
    Criticals();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    bool isRecentlyCritical(unsigned long windowMs = 400) const;

    
    
    
    enum class Mode : int {
        MiniJump = 0,
        Jump = 1,
    };

    
    
    
    
    enum class Trigger : int {
        Chance = 0,
        Hits = 1,
    };

    struct Settings {
        Mode mode = Mode::MiniJump;      
        Trigger trigger = Trigger::Chance;  
        int hitInterval = 2;             
        int maxDelayMs = 100;            
        int chance = 80;                 
        bool onlyWhileAttacking = true;  
        bool lookingAtPlayer = true;     
        bool onlyWithSword = false;      
    } settings;

private:
    unsigned long lastCriticalTime = 0;
    bool wasAttacking = false;

    
    
    
    
    
    int hitCount = 0;
    unsigned long lastHitTime = 0;

    void doJumpCritical(double height);
    void startCritical();

    bool isHoldingSword();
    bool isLookingAtPlayer();
    
    bool isValidAttack();
    
    void handleComboHit();
    
    bool canDoCritical();
    bool checkChance();
};

extern Criticals* criticals;
