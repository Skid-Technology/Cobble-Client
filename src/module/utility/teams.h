#pragma once

#include <string>
#include "../module.h"





















class Teams : public Module {
public:
    Teams();

    void onEnable() override;
    void onDisable() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;
    std::string getDisplayInfo() override;

private:
    
    
    
    
    void pushEnabled() const;
};

extern Teams* teamsModule;
