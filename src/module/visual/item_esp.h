#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "../../jni/java.h"
#include "../module.h"
#include "../../sdk/core/minecraft.h"


struct ImDrawList;

class ItemESP : public Module {
public:
    ItemESP();

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
        bool showQuantity = true;
        bool showName = true;       
        float nameScale = 1.5f;     

        float boxColor[4] = {0.0f, 1.0f, 1.0f, 1.0f};      
        float filledColor[4] = {0.0f, 1.0f, 1.0f, 0.15f};  
        float outlineColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};  
        float nameColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};     
        
        float range = 128.0f;
    } settings;

    bool enabledItemIds[512];

private:
    
    std::mutex cacheMutex;
    std::vector<ItemEntityInfo> cachedItems;

    
    double cachedRenderX = 0, cachedRenderY = 0, cachedRenderZ = 0;
    float cachedPartialTicks = 0.0f;

    
    double cachedLocalX = 0, cachedLocalY = 0, cachedLocalZ = 0;
    double cachedLocalLastX = 0, cachedLocalLastY = 0, cachedLocalLastZ = 0;
    
    
    long long lastUpdateTick = 0;
};

extern ItemESP* itemEsp;
