#pragma once

#include "../module.h"

#include <chrono>
#include <random>

class FastPlace : public Module {
public:
    FastPlace();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool blocksOnly = true;    
        int minDelayMs = 50;       
        int maxDelayMs = 110;      
        bool randomization = true; 
    } settings;

private:
    std::chrono::steady_clock::time_point lastPlace{};
    float nextDelayMs = 0.0f;
    std::mt19937 rng;

    double unit();        
    void rollNextDelay(); 
};

extern FastPlace* fastPlace;
