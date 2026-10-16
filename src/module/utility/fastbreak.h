#pragma once

#include "../module.h"

#include <cstdint>

class FastBreak : public Module {
public:
    FastBreak();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float delayMs = 0.0f;     
        bool blocksOnly = false;  
    } settings;

private:
    std::uint64_t lastDamageMs_ = 0;
};

extern FastBreak* fastBreak;
