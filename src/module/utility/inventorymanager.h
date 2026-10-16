#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include "../module.h"



class InventoryManager : public Module {
public:
    InventoryManager();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    bool canToggle() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    void saveHotbar();

    
    bool hasSavedLayout() const;

    struct SavedSlot {
        int itemId = 0;
        int damage = 0;  
    };

    
    static constexpr int MAX_PRESETS = 4;
    struct Preset {
        SavedSlot slots[9] = {};
        bool saved = false;
        std::string name;  
    };

    struct Settings {
        int swapDelay = 50;  
        bool retryOnFail = false;
        int activePreset = 0;  
    } settings;

    
    Preset presets[MAX_PRESETS];

    
    Preset& getActivePreset() { return presets[settings.activePreset]; }
    const Preset& getActivePreset() const { return presets[settings.activePreset]; }

private:
    enum class State { Idle, OpeningInventory, Swapping, Verifying, Closing };

    State state = State::Idle;
    std::chrono::steady_clock::time_point stateStartTime;
    std::chrono::steady_clock::time_point lastSwapTime;

    struct SwapTask {
        int sourceSlot;    
        int targetHotbar;  
        int expectedId;    
        int expectedDamage; 
        bool compareDamage;
        int retryCount;
    };
    std::vector<SwapTask> pendingSwaps;
    SwapTask lastSwap{};   

    
    bool lastScanMissingItems = false;

    static constexpr int TIMEOUT_MS = 2000;
    static constexpr int VERIFY_DELAY_MS = 40;
    static constexpr int MAX_RETRIES = 5;
    static constexpr int RETRY_MIN_DELAY = 25;

    void scanForSwaps();
    void finishRestore();

    
    int inventoryToContainerSlot(int invSlot);

    int effectiveDelay() const {
        return settings.retryOnFail ? (std::max)(settings.swapDelay, RETRY_MIN_DELAY) : settings.swapDelay;
    }
};

extern InventoryManager* inventoryManager;
