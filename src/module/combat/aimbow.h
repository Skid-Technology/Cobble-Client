#pragma once

#include <atomic>
#include <string>
#include "../module.h"

















class AimBow : public Module {
public:
    AimBow();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;

    
    
    
    
    
    
    
    bool isRewritingRotation() const;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    enum class AimMode {
        Automatic,
        ClosestToCrosshair
    };

    
    
    enum ArmorTierBit {
        ARMOR_NO_ARMOR = 1 << 0,  
        ARMOR_LEATHER  = 1 << 1,
        ARMOR_CHAIN    = 1 << 2,
        ARMOR_IRON     = 1 << 3,
        ARMOR_GOLD     = 1 << 4,
        ARMOR_DIAMOND  = 1 << 5,
    };

    struct Settings {
        AimMode mode = AimMode::Automatic;
        float range = 60.0f;           
        int fov = 120;                 
        float speed = 25.0f;           
        bool predict = true;           
        float predictStrength = 1.0f;  
        float verticalLead = 0.5f;     
                                       
        float aimHeight = 0.62f;       
        bool liveCharge = true;        
        float minSolveCharge = 0.5f;   
        bool targetInvisible = false;
        bool targetTeammates = false;
        bool onlyWhileDrawing = true;  
        bool onlyVisible = true;
        int armorTierMask = 0;         
        bool armorFilterEnabled = true;  
        int armorFilterKeybind = 0;      
    } settings;

private:
    unsigned long chargeStartMs = 0;
    bool wasDrawing = false;
    bool hasTarget = false;
    bool armorFilterKeyPrev_ = false;  

    
    
    
    
    
    
    
    
    double smoothVx_ = 0.0;
    double smoothVy_ = 0.0;
    double smoothVz_ = 0.0;
    std::string velUuid_;   
    double sampleX_ = 0.0;  
    double sampleY_ = 0.0;
    double sampleZ_ = 0.0;
    bool hasSample_ = false;

    
    float lastCharge_ = 0.0f;   
    float lastDist_ = 0.0f;     
    bool lastInReach_ = false;  

    float manualYaw = 0.0f;
    float manualPitch = 0.0f;
    float lastOverrideYaw = 0.0f;
    float lastOverridePitch = 0.0f;
    bool hasOverride = false;

    
    std::atomic<unsigned long long> lastRotWriteMs_{0};

    static float wrapDeg(float a);
    void resetVelocity();
};

extern AimBow* aimBow;
