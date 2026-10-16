#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../jni/java.h"
#include "../module.h"


struct ImDrawList;


struct EspPlayer {
    int entityId = 0;
    std::string name;
    std::string profileName;
    std::string uuid;                                 
    int teamColor = -1;                               
    bool isOnLocalTeam = false;
    double posX = 0, posY = 0, posZ = 0;              
    double lastPosX = 0, lastPosY = 0, lastPosZ = 0;  
    float distance = 0;
    float health = 20.0f;
    float maxHealth = 20.0f;
    bool valid = false;
};

class ESP : public Module {
public:
    ESP();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool box = true;
        bool box3D = false;
        bool filled = true;
        bool outline = true;
        bool healthBar = true;
        bool teamColors = true;  

        float boxColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};      
        float filledColor[4] = {1.0f, 0.0f, 0.0f, 0.15f};  
        float outlineColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};  
        float friendColor[4] = {0.0f, 1.0f, 0.0f, 1.0f};   
        float enemyColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};    

        float range = 128.0f;
    } settings;

private:
    struct SmoothedPlayer {
        double x = 0, y = 0, z = 0;
        float health = 20.0f;
        long long lastSeen = 0;
        bool initialized = false;
    };

    
    std::mutex cacheMutex;
    std::vector<EspPlayer> cachedPlayers;
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

extern ESP* esp;
