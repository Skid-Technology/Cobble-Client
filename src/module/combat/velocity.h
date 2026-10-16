#pragma once

#include "../module.h"

class Velocity : public Module {
public:
    Velocity();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    
    
    
    
    
    
    
    
    
    
    enum class Mode {
        Regular = 0,
        Legit = 1,
        LegitAbuse = 2,
        Ignore = 3,
        Ping = 4  
                  
    };

    
    enum class VerticalMode {
        Never = 0,      
        OnlyInAir = 1,  
        Always = 2      
    };

    struct Settings {
        
        Mode mode = Mode::Regular;
        float horizontal = 100.0f;  
        float vertical = 100.0f;    
        float chance = 100.0f;      
        float fov = 360.0f;         
        int reduceDelay = 0;        

        
        float jumpChance = 100.0f;  
        bool forwardTap = true;     
        int legitWindow = 10;       

        
        int pingDelay = 100;        

        
        VerticalMode verticalMode = VerticalMode::Never;

        
        float percentageVariance = 3.0f;  
        int tickSpread = 1;               
        int microDelayMs = 15;            

        
        bool onlyLookingAtPlayer = false;
        bool onlyMousePressed = false;
        bool onlyMovingForward = false;
        bool onlyHoldingWeapon = false;
        bool onlySprinting = false;  
    } settings;

private:
    int lastHurtTime = 0;

    struct PendingHit {
        bool active = false;
        Mode mode = Mode::Regular;
        int applyAtHurtTime = 0;
        unsigned long applyAtMs = 0;
        float horizontal = 100.0f;
        float vertical = 100.0f;
    } queuedHit;

    
    struct PendingReduction {
        bool active = false;
        double targetMotionX = 0.0;
        double targetMotionY = 0.0;
        double targetMotionZ = 0.0;
        int ticksRemaining = 0;
        int totalTicks = 0;
        int lastHurtTime = -1;
    } pending;

    
    int legitTicks = 0;       
    bool inputsHeld = false;  

    
    bool checkChance(float chance);
    bool checkFOV();
    bool checkConditions();
    bool isLookingAtPlayer();
    bool isHoldingWeapon();
    float getAttackerAngle();
    void queueHit(int hurtTime);
    void applyQueuedHit(int hurtTime);
    void continuePendingReduction(int hurtTime);
    void handleLegit(int hurtTime, bool justGotHit);
    void releaseInputs();
};

extern Velocity* velocity;
