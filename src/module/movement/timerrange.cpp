#include "timerrange.h"

#include "timer.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/aes_string.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

TimerRange* timerRange = nullptr;

namespace {
bool isHoldingWeapon() {
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

bool isAttackPacket(JNIEnv* env, jobject packet) {
    if (!env || !packet)
        return false;
    jclass cls = env->GetObjectClass(packet);
    jclass classClass = cls ? env->GetObjectClass(cls) : nullptr;
    jmethodID getName = classClass
                            ? env->GetMethodID(classClass, OBFUSCATE_STRING("getName").c_str(),
                                               OBFUSCATE_STRING("()Ljava/lang/String;").c_str())
                            : nullptr;
    jstring name = getName ? static_cast<jstring>(env->CallObjectMethod(cls, getName)) : nullptr;
    bool result = false;
    if (name) {
        const char* chars = env->GetStringUTFChars(name, nullptr);
        if (chars) {
            result = std::strstr(chars, OBFUSCATE_STRING("C02PacketUseEntity").c_str()) != nullptr;
            env->ReleaseStringUTFChars(name, chars);
        }
        env->DeleteLocalRef(name);
    }
    if (classClass)
        env->DeleteLocalRef(classClass);
    if (cls)
        env->DeleteLocalRef(cls);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    return result;
}
}  

TimerRange::TimerRange() : Module("TimerRange"), rng_(std::random_device{}()) {}

std::uint64_t TimerRange::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

void TimerRange::onEnable() {
    if (timerModule && timerModule->enabled)
        timerModule->setEnabled(false);
    pendingAtMs_.store(0, std::memory_order_release);
    activeUntilMs_.store(0, std::memory_order_release);
    currentSpeed_.store(1.0f, std::memory_order_release);
    wasAttacking_ = false;
}

void TimerRange::onDisable() {
    pendingAtMs_.store(0, std::memory_order_release);
    activeUntilMs_.store(0, std::memory_order_release);
    currentSpeed_.store(1.0f, std::memory_order_release);
    wasAttacking_ = false;
    mc.setTimerSpeed(1.0f);
}

bool TimerRange::conditionsMet() const {
    if (!player.isValid())
        return false;
    if (settings.targetOnly && !mc.isTargetingEntity())
        return false;
    if (settings.weaponOnly && !isHoldingWeapon())
        return false;
    return true;
}

void TimerRange::selectSpeed() {
    std::lock_guard<std::mutex> lock(rngMutex_);
    const float minimum = std::clamp(std::min(settings.minSpeed, settings.maxSpeed), 0.05f, 3.0f);
    const float maximum = std::clamp(std::max(settings.minSpeed, settings.maxSpeed), minimum, 3.0f);
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    currentSpeed_.store(distribution(rng_), std::memory_order_release);
}

void TimerRange::activate(std::uint64_t atMs) {
    selectSpeed();
    pendingAtMs_.store(atMs, std::memory_order_release);
}

void TimerRange::onOutgoingPacket(JNIEnv* env, jobject packet, PacketPhase phase) {
    if (!enabled || !isAttackPacket(env, packet))
        return;

    const std::uint64_t now = nowMs();
    if (settings.mode == TriggerMode::AfterHit && phase == PacketPhase::POST)
        activate(now + static_cast<std::uint64_t>(std::clamp(settings.offsetMs, 0, 500)));
    else if (settings.mode == TriggerMode::PerHit && phase == PacketPhase::POST)
        activate(now);
}

void TimerRange::onUpdate() {
    if (!enabled)
        return;
    if (Menu::isOpen || mc.isInGui() || !player.isValid()) {
        mc.setTimerSpeed(1.0f);
        wasAttacking_ = false;
        return;
    }

    const std::uint64_t now = nowMs();
    const bool attacking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (settings.mode == TriggerMode::BeforeHit && attacking && !wasAttacking_ && conditionsMet())
        activate(now);
    wasAttacking_ = attacking;

    if (settings.mode == TriggerMode::Constant) {
        if (conditionsMet()) {
            if (currentSpeed_.load(std::memory_order_acquire) == 1.0f)
                selectSpeed();
            mc.setTimerSpeed(currentSpeed_.load(std::memory_order_acquire));
        } else {
            mc.setTimerSpeed(1.0f);
        }
        return;
    }

    const std::uint64_t pending = pendingAtMs_.load(std::memory_order_acquire);
    if (pending != 0 && now >= pending) {
        pendingAtMs_.store(0, std::memory_order_release);
        activeUntilMs_.store(now + static_cast<std::uint64_t>(std::clamp(settings.durationMs, 10, 1000)),
                             std::memory_order_release);
    }

    if (now < activeUntilMs_.load(std::memory_order_acquire))
        mc.setTimerSpeed(currentSpeed_.load(std::memory_order_acquire));
    else
        mc.setTimerSpeed(1.0f);
}

void TimerRange::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Trigger",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);
            const char* modes[] = {"Before Hit", "After Hit", "Per Hit", "Constant"};
            int mode = static_cast<int>(settings.mode);
            if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                settings.mode = static_cast<TriggerMode>(mode);
            if (settings.mode == TriggerMode::AfterHit)
                ui::SliderInt("Offset", &settings.offsetMs, 0, 500, "%d ms");
            if (settings.mode != TriggerMode::Constant)
                ui::SliderInt("Duration", &settings.durationMs, 10, 1000, "%d ms");
        },
        "Timer",
        [this]() {
            ui::SliderFloat("Minimum Speed", &settings.minSpeed, 0.05f, 3.0f, "%.2fx", true);
            ui::SliderFloat("Maximum Speed", &settings.maxSpeed, 0.05f, 3.0f, "%.2fx", true);
            ui::Checkbox("Looking At Player", &settings.targetOnly);
            ui::Checkbox("Weapon Only", &settings.weaponOnly);
            settings.minSpeed = std::clamp(settings.minSpeed, 0.05f, 3.0f);
            settings.maxSpeed = std::clamp(settings.maxSpeed, settings.minSpeed, 3.0f);
        });
}

std::string TimerRange::getDisplayInfo() {
    static const char* modes[] = {"Before", "After", "Per Hit", "Constant"};
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s %.2f-%.2fx",
                  modes[std::clamp(static_cast<int>(settings.mode), 0, 3)], settings.minSpeed, settings.maxSpeed);
    return buffer;
}



void TimerRange::saveSettings(json::Value& node) const {
    node["mode"] = static_cast<int>(settings.mode);
    node["minSpeed"] = settings.minSpeed;
    node["maxSpeed"] = settings.maxSpeed;
    node["offsetMs"] = settings.offsetMs;
    node["durationMs"] = settings.durationMs;
    node["targetOnly"] = settings.targetOnly;
    node["weaponOnly"] = settings.weaponOnly;
}

void TimerRange::loadSettings(const json::Value& node) {
    const int mode = (std::clamp)(node["mode"].as_int(static_cast<int>(settings.mode)), 0, 3);
    settings.mode = static_cast<TriggerMode>(mode);
    settings.minSpeed = node["minSpeed"].as_float(settings.minSpeed);
    settings.maxSpeed = node["maxSpeed"].as_float(settings.maxSpeed);
    settings.offsetMs = (std::clamp)(node["offsetMs"].as_int(settings.offsetMs), 0, 500);
    
    const int legacyDuration = node["intervalTicks"].as_int(0) * 50;
    settings.durationMs = (std::clamp)(
        node["durationMs"].as_int(legacyDuration > 0 ? legacyDuration : settings.durationMs), 10, 1000);
    settings.minSpeed = (std::clamp)(settings.minSpeed, 0.05f, 3.0f);
    settings.maxSpeed = (std::clamp)(settings.maxSpeed, settings.minSpeed, 3.0f);
    settings.targetOnly = node["targetOnly"].as_bool(settings.targetOnly);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
}
