#pragma once

#include <atomic>
#include <chrono>
#include <random>
#include <vector>

#include "../module.h"


enum class ThrowPotsType {
    Both = 0,
    Pots = 1,
    Soup = 2
};



enum class ThrowPotsMode {
    Dynamic = 0,
    Single = 1
};

enum class ThrowPotsAim {
    Current = 0,
    Down = 1,
    SilentDown = 2
};






class ThrowPots : public Module {
public:
    ThrowPots();

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    
    
    void onOutgoingPacket(JNIEnv* env, jobject packet);
    bool needsOutgoingPacket() const;
    bool isRewritingRotation() const;

    struct Settings {
        ThrowPotsType type = ThrowPotsType::Both;
        ThrowPotsMode mode = ThrowPotsMode::Dynamic;
        int delayMinMs = 80;
        int delayMaxMs = 115;
        bool scroll = false;
        int scrollDelayMs = 100;
        bool randomOrder = false;
        bool throwBowls = true;
        bool allowHealthI = true;
        ThrowPotsAim aim = ThrowPotsAim::Current;
        int switchDelay = 50;
        bool switchBack = true;
        bool retryOnFail = true;
        int maxThrows = 9;
    } settings;

private:
    enum class State {
        Idle,
        Scroll,
        SwitchDelay,
        AwaitRotation,
        AwaitUseFlush,
        Verify,
        AwaitDropFlush,
        ItemDelay,
        RestoreScroll,
        RestoreDelay
    };

    struct PlannedItem {
        int slot = -1;
        int itemId = 0;
        int damage = 0;
        int stackSize = 0;
        int healAmount = 0;
        bool isSoup = false;
    };

    static constexpr int kPotionItemId = 373;
    static constexpr int kSoupItemId = 282;
    static constexpr int kBowlItemId = 281;
    static constexpr int kHotbarSize = 9;
    static constexpr int kSoupHealAmount = 8;
    static constexpr int kMaxRetries = 2;
    static constexpr int kPacketTimeoutMs = 1000;
    
    static constexpr int kUseSettleMs = 51;

    State state_ = State::Idle;
    std::chrono::steady_clock::time_point stateStart_{};
    std::vector<PlannedItem> plan_;
    size_t planIndex_ = 0;
    PlannedItem target_{};
    int originalSlot_ = -1;
    int scrollTarget_ = -1;
    int itemDelayMs_ = 0;
    float originalPitch_ = 0.0f;
    bool pitchModified_ = false;
    int successfulUses_ = 0;
    int retryCount_ = 0;
    std::mt19937 rng_;

    std::atomic<bool> rotationRequested_{false};
    std::atomic<bool> rotationSent_{false};
    std::atomic<float> aimYaw_{0.0f};
    std::atomic<bool> pendingUse_{false};
    std::atomic<bool> useDispatched_{false};
    std::atomic<int> pendingDropSlot_{-1};
    std::atomic<bool> dropDispatched_{false};

    bool buildPlan();
    bool selectNextItem();
    bool targetItemPresent(bool requireOriginalStackSize) const;
    void beginSlotChange(int slot, State afterArrival);
    void stepScroll();
    void requestDownRotation();
    void queueUse();
    void queueBowlDrop();
    void advanceAfterItem();
    void retryOrRestore();
    void beginRestore();
    void finish(bool restorePlayerState);
    void resetTransientState();
    int rollItemDelay();
};

extern ThrowPots* throwPots;
