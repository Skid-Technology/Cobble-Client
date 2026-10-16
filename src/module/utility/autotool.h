#pragma once
#include "../../sdk/core/minecraft.h"
#include "../module.h"

class AutoTool : public Module {
public:
    AutoTool() : Module("Auto Tool") {}

    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool switchBack = true;   
        bool autoSword = true;    
    } settings;

private:
    int m_previousSlot = -1;  
    bool m_switched = false;  

    
    enum class ToolType { NONE, PICKAXE, AXE, SHOVEL, SWORD, SHEARS };

    
    ToolType getBestToolForBlock(int blockId);

    
    int findToolInHotbar(ToolType type);

    
    bool isPickaxe(int itemId);
    bool isAxe(int itemId);
    bool isShovel(int itemId);
    bool isSword(int itemId);
    bool isShears(int itemId);
};

extern AutoTool* autoTool;
