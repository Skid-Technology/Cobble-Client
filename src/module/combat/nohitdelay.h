#pragma once

#include "../module.h"

class NoHitDelay : public Module {
public:
    NoHitDelay();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool weaponOnly = false;
    } settings;
};

extern NoHitDelay* noHitDelay;
