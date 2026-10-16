#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>
#include <Windows.h>
#include "../../sdk/core/minecraft.h"
#include "../module.h"


namespace RefillItems {
constexpr int MUSHROOM_SOUP = 282;
constexpr int BOWL = 281;  
constexpr int POTION = 373;
}  



enum class AutoRefillPreset { Instant = 0, Fast = 1, Human = 2, Careful = 3, Custom = 4 };



class AutoRefill : public Module {
public:
    AutoRefill();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;

    
    
    
    
    void onOutgoingPacket(JNIEnv* env, jobject packet);
    bool isPacketRefilling() const;
    std::string getDisplayInfo() override;
    bool canToggle() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        
        bool refillSoup = true;
        bool refillPotions = true;

        
        int speed = 80;

        
        int mode = 0;
        int legitMoveTime = 130;

        
        bool onInventoryOpen = false;

        
        int preset = static_cast<int>(AutoRefillPreset::Human);
        int reactionMs = 190;      
        int jitterPercent = 35;    
        bool fatigue = true;       
        int hesitateChance = 12;   
        bool humanPath = true;     
        bool nearestFirst = true;  
    } settings;

    void applyPreset(AutoRefillPreset preset);

private:
    enum class State {
        Idle,
        Reacting,  
        PacketOpening,
        Refilling,  
        Closing,    
        LegitOpening,
        LegitMoveSource,
        LegitAwaitClick,
        RestoreSprintDown,
        RestoreSprintUp
    };
    State state = State::Idle;
    std::chrono::steady_clock::time_point stateStartTime;
    std::chrono::steady_clock::time_point lastMoveTime;
    int64_t stateDelay_ = 0;  
    int64_t nextGapMs_ = 0;   

    struct RefillTask {
        int hotbarSlot;
        int sourceSlot;
    };
    std::vector<RefillTask> pendingRefills;
    RefillTask activeTask_{};
    bool hasActiveTask_ = false;
    bool manualTrigger_ = false;  
    int movesDone_ = 0;
    int lastSourceSlot_ = -1;

    POINT mouseStart_{};
    POINT mouseTarget_{};
    
    int64_t pathDuration_ = 130;
    int64_t pathSettle_ = 0;       
    float bowFrac_ = 0.0f;         
    float overshootFrac_ = 0.0f;   
    float tremorAmp_ = 0.0f;
    float tremorPhase_ = 0.0f;
    float tremorFreq_ = 8.0f;
    int decoyStage_ = 0;  

    bool movementPaused_ = false;
    bool sprintKeyHeld_ = false;

    
    bool wasInventoryOpen = false;

    
    
    
    
    
    
    std::atomic<int> pendingClickSlot_{-1};
    std::atomic<int> pendingClickHotbar_{-1};
    jclass c03Class_ = nullptr;  
    bool c03Resolved_ = false;

    mutable std::mt19937_64 rng_;

    int64_t jitter(int baseMs) const;
    int64_t jitter(int baseMs, int spreadPercent) const;
    bool roll(int percent) const;
    float fatigueScale() const;
    int64_t nextGap() const;
    void enterState(State next, int64_t delayMs);
    bool takeNextTask(RefillTask& out);

    void scanForRefills();
    void finishRefill();
    bool startLegitMove(int slotId);
    bool updateLegitMove(long long elapsed);
};

extern AutoRefill* autoRefill;
