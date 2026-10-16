#pragma once

#include "../module.h"

enum class ItemReleaseMode {
    CONSUMABLE = 0,  
    SWORD = 1,       
    ALL = 2          
};

class NoItemRelease : public Module {
public:
    NoItemRelease();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    struct {
        ItemReleaseMode mode = ItemReleaseMode::ALL;
    } settings;

    
    bool wasUsingItem = false;

private:
    bool shouldBlockRelease();
    bool isConsumable(int itemId);
    bool isSword(int itemId);
    bool isBowOrRod(int itemId);
};

extern NoItemRelease* noItemRelease;
