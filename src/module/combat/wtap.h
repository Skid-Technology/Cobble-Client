#pragma once

#include "../module.h"

#include <cstdint>
#include <random>

class WTap : public Module {
public:
    enum class Mode {
        Forward = 0,
        Sprint = 1,
        Hybrid = 2
    };

    struct Settings {
        Mode mode = Mode::Hybrid;
        int chance = 85;
        int delayMs = 15;
        int releaseMs = 55;
        bool targetOnly = true;
        bool weaponOnly = false;
    } settings;

    WTap();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    bool isTapping() const;

private:
    enum class State {
        Idle,
        Waiting,
        Released
    };

    static std::uint64_t nowMs();
    bool shouldTrigger();
    bool isHoldingWeapon() const;
    bool rollChance();
    void releaseInputs();
    void restoreInputs();

    std::mt19937 rng_;
    State state_ = State::Idle;
    std::uint64_t stateAtMs_ = 0;
    bool wasAttacking_ = false;
};

extern WTap* wTap;
