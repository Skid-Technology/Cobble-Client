#pragma once

#include "../module.h"

class TimerMod : public Module {
public:
    TimerMod();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        double speed = 1.0;         
        bool mousePressed = false;  
        bool weaponOnly = false;    
    } settings;
};

extern TimerMod* timerModule;
