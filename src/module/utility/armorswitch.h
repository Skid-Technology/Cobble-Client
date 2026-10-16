#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <Windows.h>
#include "../module.h"
#include "../../sdk/core/minecraft.h"


namespace ArmorItems {

constexpr int LEATHER_HELMET = 298;
constexpr int CHAINMAIL_HELMET = 302;
constexpr int IRON_HELMET = 306;
constexpr int DIAMOND_HELMET = 310;
constexpr int GOLD_HELMET = 314;


constexpr int LEATHER_CHESTPLATE = 299;
constexpr int CHAINMAIL_CHESTPLATE = 303;
constexpr int IRON_CHESTPLATE = 307;
constexpr int DIAMOND_CHESTPLATE = 311;
constexpr int GOLD_CHESTPLATE = 315;


constexpr int LEATHER_LEGGINGS = 300;
constexpr int CHAINMAIL_LEGGINGS = 304;
constexpr int IRON_LEGGINGS = 308;
constexpr int DIAMOND_LEGGINGS = 312;
constexpr int GOLD_LEGGINGS = 316;


constexpr int LEATHER_BOOTS = 301;
constexpr int CHAINMAIL_BOOTS = 305;
constexpr int IRON_BOOTS = 309;
constexpr int DIAMOND_BOOTS = 313;
constexpr int GOLD_BOOTS = 317;
}  


enum class ArmorMaterial { Diamond = 0, Gold = 1, Iron = 2, Leather = 3, Chain = 4 };




class ArmorSwitch : public Module {
public:
    ArmorSwitch();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    bool canToggle() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        
        int set1Material = 0;  
        int set2Material = 2;  

        
        int switchDelay = 50;
        
        int retryMinDelay = 70;

        
        bool preferProtection = true;

        
        bool onInventoryOpen = false;

        
        bool retryOnFail = false;

        bool silentMode = false;

        
        bool visualMode = false;
        int visualMoveTime = 120;

        
        bool autoBalance = false;
        int maxDurabilityDiff = 10;  
        int checkInterval = 1000;    
    } settings;

    std::chrono::steady_clock::time_point lastBalanceCheck;
    void checkAutoBalance();

private:
    enum class State { Idle, OpeningInventory, Switching, VisualMove, Verifying, Closing };

    State state = State::Idle;
    std::chrono::steady_clock::time_point stateStartTime;
    std::chrono::steady_clock::time_point lastClickTime;

    
    int targetSet = 0;

    
    struct SwapTask {
        int containerSlot;   
        int hotbarIndex;     
        int expectedItemId;  
        
        
        
        
        
        int armorSlotType;
        int retryCount;  
    };
    std::vector<SwapTask> pendingSwaps;
    SwapTask lastSwap;  
    POINT mouseStart_{};
    POINT mouseTarget_{};
    int activeVisualMoveTime_ = 120;
    uint32_t visualSeed_ = 0xA341316Cu;

    static constexpr int DELAY_AFTER_OPEN = 100;
    static constexpr int DELAY_BEFORE_CLOSE = 80;
    static constexpr int MAX_RETRIES = 5;
    static constexpr int VERIFY_DELAY_MS = 40;

    int effectiveDelay() const {
        return settings.retryOnFail ? (std::max)(settings.switchDelay, settings.retryMinDelay) : settings.switchDelay;
    }

    
    int getArmorId(ArmorMaterial material, int slotType);

    
    
    
    int findArmorInInventory(const std::vector<SlotInfo>& slots, int slotType, ArmorMaterial material, int* outDamage,
                             int* outProtLevel = nullptr);

    
    int detectCurrentSet();

    void scanForSwaps();
    void finishSwap(bool fromOnInventoryOpen = false);
    bool startVisualMove(int containerSlot);
    bool updateVisualMove(long long elapsed);
    void performSwap();

    
    int inventoryToContainerSlot(int invSlot);

    
    bool wasInventoryOpen = false;
};

extern ArmorSwitch* armorSwitch;
