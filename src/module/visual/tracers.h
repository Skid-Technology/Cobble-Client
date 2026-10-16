#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../jni/java.h"
#include "../module.h"


struct ImDrawList;


struct TracerPlayer {
    int entityId = 0;
    std::string name;
    double posX = 0, posY = 0, posZ = 0;
    double lastPosX = 0, lastPosY = 0, lastPosZ = 0;
    float distance = 0;
    float health = 20.0f;
    bool valid = false;
};

class Tracers : public Module {
public:
    Tracers();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool colorByDistance = true;
        bool colorByHealth = false;
        bool colorByTeam = false;
        float tracerColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};  
        float thickness = 2.0f;
        float range = 128.0f;

        
        int lineFrom = 0;  

        
        
        
        
        float opacity = 0.6f;

        
        
        
        
        float startGap = 55.0f;

        bool outline = true;  
        bool endDot = true;   
    } settings;

private:
    struct SmoothedPlayer {
        double x = 0, y = 0, z = 0;
        long long lastSeen = 0;
        bool initialized = false;
    };

    
    std::mutex cacheMutex;
    std::vector<TracerPlayer> cachedPlayers;
    std::unordered_map<int, SmoothedPlayer> smoothedPlayers;
    long long lastRenderTime = 0;

    
    double cachedRenderX = 0, cachedRenderY = 0, cachedRenderZ = 0;
    float cachedPartialTicks = 0.0f;

    
    double cachedLocalX = 0, cachedLocalY = 0, cachedLocalZ = 0;
    double cachedLocalLastX = 0, cachedLocalLastY = 0, cachedLocalLastZ = 0;

    
    jobject lastWorldRef = nullptr;
    void checkWorldChange();

    
    long long lastUpdateTick = 0;
};

extern Tracers* tracers;
