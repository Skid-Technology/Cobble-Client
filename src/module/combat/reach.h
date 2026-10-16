#pragma once

#include "../module.h"

class Reach : public Module {
public:
    Reach();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float distance = 3.6f;
        
        
        bool weaponsOnly = false;     
        bool sprintingOnly = false;   
        bool movingOnly = false;      
        
        
        int hitChance = 100;          
        int tickDelay = 0;            
    } settings;

private:
    int tickCounter = 0;
    unsigned int rngState = 0;
    
    float randomFloat(float min, float max);
    bool shouldApplyThisTick();
};

extern Reach* reach;
