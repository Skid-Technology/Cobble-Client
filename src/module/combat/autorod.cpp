#include "autorod.h"

#include "antibot.h"
#include "autoclicker.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kDeg = 57.29577951308232;  



constexpr double kMaxCastAngle = 8.0;


constexpr int kHitDelayTicks = 2;

constexpr int kMinHoldTicks = 3;
constexpr int kMaxHoldTicks = 12;


constexpr double kMeleeAbortDistance = 3.2;

float wrapDeg(float degrees) {
    degrees = std::fmod(degrees + 180.0f, 360.0f);
    if (degrees < 0.0f)
        degrees += 360.0f;
    return degrees - 180.0f;
}




struct PlayerListRefs {
    std::vector<PlayerInfo>& list;
    ~PlayerListRefs() {
        JNIEnv* env = Java::Env();
        if (!env)
            return;
        for (auto& p : list) {
            if (p.instance) {
                env->DeleteGlobalRef(p.instance);
                p.instance = nullptr;
            }
        }
    }
};

}  

AutoRod* autoRod = nullptr;

AutoRod::AutoRod() : Module("AutoRod"), rng_(std::random_device{}()) {
    keybind = 0;
}

bool AutoRod::isWeapon(int itemId) {
    switch (itemId) {
        case 267: case 268: case 272: case 276: case 283:
        case 258: case 271: case 275: case 279: case 286:
            return true;
        default:
            return false;
    }
}

int AutoRod::findRodSlot() const {
    for (int slot = 0; slot < 9; ++slot) {
        if (player.getItemInSlot(slot) == 346)
            return slot;
    }
    return -1;
}










int AutoRod::hookFlightTicks(double distance) {
    const double reach = std::clamp(distance, 0.0, 16.0);
    const double remaining = 1.0 - reach / 18.75;
    int ticks = kMaxHoldTicks;
    if (remaining > 0.0) {
        const double solved = std::log(remaining) / std::log(0.92);
        if (std::isfinite(solved))
            ticks = (int)std::ceil(solved) + 2;
    }
    return std::clamp(ticks, kMinHoldTicks, kMaxHoldTicks);
}

bool AutoRod::measure(const PlayerInfo& target, double& distanceOut, double& angleOut) const {
    const double eyeY = player.getPosY() + (player.isSneaking() ? 1.54 : 1.62);
    const double dx = target.posX - player.getPosX();
    const double dz = target.posZ - player.getPosZ();

    
    
    double bodyHeight = 1.8;
    const double hitboxHeight = (double)target.hitboxMaxY - (double)target.hitboxMinY;
    if (hitboxHeight > 0.1 && hitboxHeight < 4.0)
        bodyHeight = hitboxHeight;
    const double dy = (target.posY + bodyHeight * 0.78) - eyeY;

    const double flat = std::sqrt(dx * dx + dz * dz);
    distanceOut = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distanceOut) || distanceOut < 1e-4)
        return false;

    const float yawTo = (float)(std::atan2(-dx, dz) * kDeg);
    const float pitchTo = (float)(-std::atan2(dy, std::max(flat, 1e-4)) * kDeg);
    const float yawDelta = std::fabs(wrapDeg(yawTo - player.getYaw()));
    const float pitchDelta = std::fabs(pitchTo - player.getPitch());
    angleOut = std::sqrt((double)yawDelta * yawDelta + (double)pitchDelta * pitchDelta);
    return std::isfinite(angleOut);
}

int AutoRod::pickTarget(double& distanceOut) const {
    distanceOut = 0.0;
    std::vector<PlayerInfo> players = mc.getPlayerList();
    PlayerListRefs release{players};
    if (players.empty())
        return 0;

    const double minDistance = std::clamp((double)settings.minDistance, 1.0, 20.0);
    const double maxDistance =
        std::max(minDistance + 0.5, std::clamp((double)settings.maxDistance, 1.0, 20.0));

    int bestId = 0;
    double bestAngle = 1e30;
    for (const auto& p : players) {
        if (!p.instance || p.isNPC || p.isInvisible || p.isOnLocalTeam)
            continue;
        if (p.health <= 0.0f)
            continue;
        if (AntiBot::Filtered(p))
            continue;
        if (friendsManager && !p.uuid.empty() &&
            (friendsManager->isFriendByUUID(p.uuid) || friendsManager->isApolloTeammate(p.uuid) ||
             friendsManager->isLunarTeammate(p.uuid)))
            continue;

        double distance = 0.0;
        double angle = 0.0;
        if (!measure(p, distance, angle))
            continue;
        
        
        
        if (distance < minDistance || distance > maxDistance)
            continue;
        if (angle > kMaxCastAngle || angle >= bestAngle)
            continue;
        
        
        if (!mc.canEntityBeSeen(p.instance))
            continue;

        bestAngle = angle;
        distanceOut = distance;
        bestId = p.entityId;
    }
    return bestId;
}

double AutoRod::distanceToId(int entityId) const {
    if (entityId == 0)
        return -1.0;
    std::vector<PlayerInfo> players = mc.getPlayerList();
    PlayerListRefs release{players};
    for (const auto& p : players) {
        if (p.entityId != entityId)
            continue;
        if (p.health <= 0.0f)
            return -1.0;
        const double dx = p.posX - player.getPosX();
        const double dy = p.posY - player.getPosY();
        const double dz = p.posZ - player.getPosZ();
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        return std::isfinite(distance) ? distance : -1.0;
    }
    return -1.0;
}






int AutoRod::jitterTicks(int baseTicks) {
    const int base = std::max(1, baseTicks);
    const int spread = std::clamp(settings.jitterPercent, 0, 90);
    if (spread == 0)
        return base;
    
    
    const int amp = std::max(1, (base * spread) / 100);
    std::uniform_int_distribution<int> dist(-amp, amp);
    return std::max(1, base + dist(rng_));
}

int AutoRod::rollCooldownMs() {
    const int base = std::clamp(settings.cooldownMs, 400, 6000);
    const int spread = std::clamp(settings.jitterPercent, 0, 90);
    if (spread == 0)
        return base;
    const int amp = std::max(1, (base * spread) / 100);
    std::uniform_int_distribution<int> dist(-amp, amp);
    return std::max(300, base + dist(rng_));
}





void AutoRod::abortSequence(bool restoreSlot) {
    if (restoreSlot && rodSlot_ >= 0 && weaponSlot_ >= 0 && player.getCurrentSlot() == rodSlot_)
        player.setCurrentSlot(weaponSlot_);
    if (phase_ != Phase::Idle) {
        lastSequence_ = std::chrono::steady_clock::now();
        cooldownMsThisRound_ = 0;  
    }
    phase_ = Phase::Idle;
    waitTicks_ = 0;
    rodSlot_ = weaponSlot_ = -1;
    targetId_ = 0;
}

void AutoRod::onEnable() {
    phase_ = Phase::Idle;
    waitTicks_ = 0;
    rodSlot_ = weaponSlot_ = -1;
    targetId_ = 0;
    prevAttackDown_ = false;
    requestPending_.store(false, std::memory_order_relaxed);
    observedAutoClick_ = autoClicker ? autoClicker->lastAutoClick : std::chrono::steady_clock::time_point{};
    cooldownMsThisRound_ = 0;
    lastSequence_ = std::chrono::steady_clock::now() - std::chrono::hours(1);
}

void AutoRod::onDisable() {
    
    
    
    abortSequence(true);
    requestPending_.store(false, std::memory_order_relaxed);
}




void AutoRod::onUpdate() {
    if (!enabled || Menu::isOpen) {
        prevAttackDown_ = false;
        requestPending_.store(false, std::memory_order_relaxed);
        return;
    }

    const bool attackDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool attackPressed = attackDown && !prevAttackDown_;
    prevAttackDown_ = attackDown;

    bool autoClick = false;
    if (autoClicker && autoClicker->lastAutoClick != observedAutoClick_) {
        observedAutoClick_ = autoClicker->lastAutoClick;
        autoClick = true;
    }

    if (attackPressed || autoClick || !settings.onlyWhileAttacking)
        requestPending_.store(true, std::memory_order_release);
}

void AutoRod::onClientTick(JNIEnv* env) {
    (void)env;
    if (!enabled || !player.isValid() || mc.isInGui()) {
        abortSequence(true);
        requestPending_.store(false, std::memory_order_relaxed);
        return;
    }

    
    
    
    switch (phase_) {
        case Phase::RodSelected: {
            if (waitTicks_ > 0) {
                --waitTicks_;
                return;
            }
            
            
            if (player.getCurrentSlot() != rodSlot_ || mc.getRightClickDelay() > 0) {
                abortSequence(true);
                return;
            }
            
            
            
            const double distance = distanceToId(targetId_);
            if (distance < 0.0 || distance <= kMeleeAbortDistance) {
                
                
                abortSequence(true);
                return;
            }
            
            
            
            
            mc.rightClickMouse();
            phase_ = Phase::HookFlying;
            waitTicks_ = hookFlightTicks(distance) - 1;
            return;
        }

        case Phase::HookFlying: {
            
            
            
            
            
            if (player.getCurrentSlot() != rodSlot_) {
                
                
                abortSequence(false);
                return;
            }
            const double distance = distanceToId(targetId_);
            const bool targetGone = distance < 0.0;
            const bool inMelee = distance >= 0.0 && distance <= kMeleeAbortDistance;
            if (waitTicks_ > 0 && !targetGone && !inMelee) {
                --waitTicks_;
                return;
            }
            if (weaponSlot_ >= 0)
                player.setCurrentSlot(weaponSlot_);
            phase_ = Phase::WeaponSelected;
            waitTicks_ = jitterTicks(kHitDelayTicks) - 1;
            return;
        }

        case Phase::WeaponSelected:
            if (waitTicks_ > 0) {
                --waitTicks_;
                return;
            }
            if (settings.autoHit && weaponSlot_ >= 0 && player.getCurrentSlot() == weaponSlot_ &&
                mc.isTargetingEntity()) {
                mc.clickMouse();
            }
            abortSequence(false);
            return;

        case Phase::Idle:
            break;
    }

    
    if (!requestPending_.exchange(false, std::memory_order_acq_rel))
        return;

    if (cooldownMsThisRound_ == 0)
        cooldownMsThisRound_ = rollCooldownMs();
    if (std::chrono::steady_clock::now() - lastSequence_ < std::chrono::milliseconds(cooldownMsThisRound_))
        return;

    
    if (mc.getRightClickDelay() > 0)
        return;

    const int currentSlot = player.getCurrentSlot();
    if (settings.requireWeapon && !isWeapon(player.getHeldItemId()))
        return;
    const int rodSlot = findRodSlot();
    if (rodSlot < 0 || rodSlot == currentSlot)
        return;

    
    
    double distance = 0.0;
    const int target = pickTarget(distance);
    if (target == 0)
        return;

    weaponSlot_ = currentSlot;
    rodSlot_ = rodSlot;
    targetId_ = target;
    player.setCurrentSlot(rodSlot_);
    phase_ = Phase::RodSelected;
    
    
    
    waitTicks_ = 0;
}

std::string AutoRod::getDisplayInfo() {
    switch (phase_) {
        case Phase::RodSelected:
            return "Cast";
        case Phase::HookFlying:
            return "Hook";
        case Phase::WeaponSelected:
            return "Hit";
        case Phase::Idle:
            break;
    }
    return "Ready";
}

void AutoRod::onRenderMenu() {
    ui::two_column(
        "General",
        [this]() {
            ui::Checkbox("Only While Attacking", &settings.onlyWhileAttacking);
            ui::Checkbox("Require Weapon", &settings.requireWeapon);
            ui::Checkbox("Auto Hit", &settings.autoHit);
        },
        "Range",
        [this]() {
            ui::SliderFloat("Min Distance", &settings.minDistance, 3.0f, 6.0f, "%.1f");
            ui::SliderFloat("Max Distance", &settings.maxDistance, 5.0f, 12.0f, "%.1f");
            ui::SliderInt("Cooldown (ms)", &settings.cooldownMs, 400, 6000, "%d");
            ui::SliderInt("Timing Jitter (%)", &settings.jitterPercent, 0, 90, "%d");
        });
}

void AutoRod::saveSettings(json::Value& node) const {
    node["onlyWhileAttacking"] = settings.onlyWhileAttacking;
    node["requireWeapon"] = settings.requireWeapon;
    node["autoHit"] = settings.autoHit;
    node["minDistance"] = settings.minDistance;
    node["maxDistance"] = settings.maxDistance;
    node["cooldownMs"] = settings.cooldownMs;
    node["jitterPercent"] = settings.jitterPercent;
}

void AutoRod::loadSettings(const json::Value& node) {
    settings.onlyWhileAttacking = node["onlyWhileAttacking"].as_bool(settings.onlyWhileAttacking);
    settings.requireWeapon = node["requireWeapon"].as_bool(settings.requireWeapon);
    settings.autoHit = node["autoHit"].as_bool(settings.autoHit);
    settings.minDistance = std::clamp(node["minDistance"].as_float(settings.minDistance), 3.0f, 6.0f);
    settings.maxDistance = std::clamp(node["maxDistance"].as_float(settings.maxDistance), 5.0f, 12.0f);
    settings.cooldownMs = node["cooldownMs"].as_int(settings.cooldownMs);
    settings.jitterPercent = node["jitterPercent"].as_int(settings.jitterPercent);
}
