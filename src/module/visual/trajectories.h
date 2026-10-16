#pragma once

#include <mutex>
#include <vector>
#include "../../jni/java.h"
#include "../module.h"


struct ImDrawList;


struct TrajectoryPoint {
    float relX = 0, relY = 0, relZ = 0;  
    bool valid = false;
    bool hitBlock = false;
    bool hitPlayer = false;
};

class Trajectories : public Module {
public:
    Trajectories();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float lineColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};  
        float thickness = 2.0f;

        
        bool showBow = true;
        bool showEnderPearl = true;
        bool showEgg = true;
        bool showSnowball = true;
        bool showPotionHealing = true;
        bool showPotionPoison = true;
        bool showPotionSlowness = true;
        bool showExpBottle = true;
    } settings;

private:
    
    std::mutex cacheMutex;
    std::vector<TrajectoryPoint> cachedTrajectory;
    std::vector<TrajectoryPoint> renderedTrajectory;
    long long lastRenderTime = 0;

    
    double cachedRenderX = 0, cachedRenderY = 0, cachedRenderZ = 0;
    float cachedPartialTicks = 0.0f;

    
    double cachedLocalX = 0, cachedLocalY = 0, cachedLocalZ = 0;
    float cachedYaw = 0, cachedPitch = 0;

    
    double prevLocalX = 0, prevLocalY = 0, prevLocalZ = 0;
    bool cachedSneaking = false;
    bool firstFrameAfterEnable = true;

    
    jobject lastWorldRef = nullptr;
    void checkWorldChange();

    
    long long lastUpdateTick = 0;

    
    int cachedHeldItemId = 0;

    
    long long bowChargeStartTime = 0;  
    bool wasBowCharging = false;       
    float cachedBowCharge = 0.0f;      

    
    struct ProjectileType {
        float gravity;             
        float initialSpeed;        
        float drag;                
        float upwardAngle = 0.0f;  
    };

    ProjectileType getProjectileType(int itemId, float bowCharge = 1.0f);
    bool isThrowableItem(int itemId);
    void simulateTrajectory();
    float getBowChargeLevel();  
};

extern Trajectories* trajectories;
