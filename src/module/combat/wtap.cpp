#include "wtap.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>

WTap* wTap = nullptr;

namespace {
bool isPhysicalKeyDown(int lwjglKey) {
    const UINT virtualKey = MapVirtualKeyA(static_cast<UINT>(lwjglKey), MAPVK_VSC_TO_VK);
    return virtualKey != 0 && (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
}
}  

WTap::WTap() : Module("WTap"), rng_(std::random_device{}()) {}

std::uint64_t WTap::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

void WTap::onEnable() {
    state_ = State::Idle;
    wasAttacking_ = false;
}

void WTap::onDisable() {
    restoreInputs();
    state_ = State::Idle;
    wasAttacking_ = false;
}

bool WTap::isTapping() const {
    if (!enabled)
        return false;
    return state_ != State::Idle;
}

bool WTap::isHoldingWeapon() const {
    switch (player.getHeldItemId()) {
        case 267:
        case 268:
        case 272:
        case 276:
        case 283:
        case 258:
        case 271:
        case 275:
        case 279:
        case 286:
            return true;
        default:
            return false;
    }
}

bool WTap::rollChance() {
    std::uniform_int_distribution<int> distribution(1, 100);
    return distribution(rng_) <= std::clamp(settings.chance, 0, 100);
}

bool WTap::shouldTrigger() {
    if (settings.targetOnly && !mc.isTargetingEntity())
        return false;
    if (settings.weaponOnly && !isHoldingWeapon())
        return false;
    return rollChance();
}

void WTap::releaseInputs() {
    if (settings.mode == Mode::Forward || settings.mode == Mode::Hybrid)
        mc.setKeyBindState(mc.getForwardKey(), false);
    if (settings.mode == Mode::Sprint || settings.mode == Mode::Hybrid) {
        mc.setKeyBindState(mc.getSprintKey(), false);
        player.setSprinting(false);
    }
}

void WTap::restoreInputs() {
    if (!player.isValid())
        return;
    if (settings.mode == Mode::Forward || settings.mode == Mode::Hybrid)
        mc.setKeyBindState(mc.getForwardKey(), isPhysicalKeyDown(mc.getForwardKey()));
    if (settings.mode == Mode::Sprint || settings.mode == Mode::Hybrid)
        mc.setKeyBindState(mc.getSprintKey(), isPhysicalKeyDown(mc.getSprintKey()));
}

void WTap::onUpdate() {
    if (!enabled)
        return;

    if (Menu::isOpen || mc.isInGui() || !player.isValid()) {
        restoreInputs();
        state_ = State::Idle;
        wasAttacking_ = false;
        return;
    }

    const std::uint64_t now = nowMs();
    const bool attacking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (attacking && !wasAttacking_ && state_ == State::Idle && shouldTrigger()) {
        state_ = State::Waiting;
        stateAtMs_ = now;
    }
    wasAttacking_ = attacking;

    if (state_ == State::Waiting && now - stateAtMs_ >= static_cast<std::uint64_t>(std::max(settings.delayMs, 0))) {
        releaseInputs();
        state_ = State::Released;
        stateAtMs_ = now;
    } else if (state_ == State::Released &&
               now - stateAtMs_ >= static_cast<std::uint64_t>(std::max(settings.releaseMs, 10))) {
        restoreInputs();
        state_ = State::Idle;
    }
}

void WTap::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);
            int mode = static_cast<int>(settings.mode);
            const char* modes[] = {"Forward", "Sprint", "Hybrid"};
            if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                settings.mode = static_cast<Mode>(mode);
            ui::SliderInt("Chance", &settings.chance, 0, 100, "%d%%");
            ui::SliderInt("Delay", &settings.delayMs, 0, 120, "%d ms");
            ui::SliderInt("Release Time", &settings.releaseMs, 20, 180, "%d ms");
        },
        "Conditions",
        [this]() {
            ui::Checkbox("Looking At Player", &settings.targetOnly);
            ui::Checkbox("Weapon Only", &settings.weaponOnly);
        });
}

std::string WTap::getDisplayInfo() {
    static const char* names[] = {"Forward", "Sprint", "Hybrid"};
    return names[std::clamp(static_cast<int>(settings.mode), 0, 2)];
}



void WTap::saveSettings(json::Value& node) const {
    node["mode"] = static_cast<int>(settings.mode);
    node["chance"] = settings.chance;
    node["delayMs"] = settings.delayMs;
    node["releaseMs"] = settings.releaseMs;
    node["targetOnly"] = settings.targetOnly;
    node["weaponOnly"] = settings.weaponOnly;
}

void WTap::loadSettings(const json::Value& node) {
    settings.mode = static_cast<Mode>(
        (std::clamp)(node["mode"].as_int(static_cast<int>(settings.mode)), 0, 2));
    settings.chance = (std::clamp)(node["chance"].as_int(settings.chance), 0, 100);
    settings.delayMs = (std::clamp)(node["delayMs"].as_int(settings.delayMs), 0, 120);
    settings.releaseMs = (std::clamp)(node["releaseMs"].as_int(settings.releaseMs), 20, 180);
    settings.targetOnly = node["targetOnly"].as_bool(settings.targetOnly);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
}
