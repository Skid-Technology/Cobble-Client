#pragma once

#include "../module.h"

class FastStop : public Module {
public:
    FastStop();

    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        
        
        float stopStrength = 1.0f;
        bool onlyOnGround = true;  
    } settings;

private:
    bool wasMoving = false;
};

extern FastStop* fastStop;
