#include "blockhit.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/aes_string.hpp"
#include "aimassist.h"
#include "autoclicker.h"
#include "autorod.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

BlockHit* blockHit = nullptr;

namespace {





constexpr std::uint32_t kPressureTicks = 40;

bool clearEx(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

bool gameFocused() {
    const HWND fg = GetForegroundWindow();
    if (!fg)
        return false;
    
    
    
    
    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    return fgPid != 0 && fgPid == GetCurrentProcessId();
}

}  

std::uint64_t BlockHit::nowMs() {
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

BlockHit::BlockHit() : Module("BlockHit"), rng_((unsigned)nowMs()) {
    keybind = 0;
    rollCycle();
}

void BlockHit::onEnable() {
    tick_ = 0;
    blockedAtTick_ = 0;
    releasedAtTick_ = 0;
    lastHitTakenTick_ = 0;
    haveTakenHit_ = false;
    prevHurtTime_ = 0;
    beatsHeld_ = 0;
    holePending_ = false;
    immuneHole_ = false;
    wasSprinting_ = false;
    prevAttackDown_ = false;
    observedAutoClick_ =
        autoClicker ? autoClicker->lastAutoClick : std::chrono::steady_clock::time_point{};
    attackSignals_.store(0, std::memory_order_release);
    gateReason_.store(nullptr, std::memory_order_release);
    rollCycle();
    
    
    
}





void BlockHit::onDisable() {
    attackSignals_.store(0, std::memory_order_release);
    gateReason_.store(nullptr, std::memory_order_release);
    prevAttackDown_ = false;
}





void BlockHit::onWorldChange() {
    serverBlocking_.store(false, std::memory_order_release);
    gateReason_.store(nullptr, std::memory_order_release);
    blockedAtTick_ = 0;
    releasedAtTick_ = 0;
    lastHitTakenTick_ = 0;
    haveTakenHit_ = false;
    prevHurtTime_ = 0;
    beatsHeld_ = 0;
    holePending_ = false;
    immuneHole_ = false;
    wasSprinting_ = false;
    apiTried_ = false;
    apiUsable_ = false;
    midOnStoppedUsing_ = nullptr;
    fMovementInput_ = nullptr;
    fInputStrafe_ = nullptr;
    fInputForward_ = nullptr;
    fSprintToggle_ = nullptr;
    fRidingEntity_ = nullptr;
    rollCycle();
}





void BlockHit::onUpdate() {
    if (!enabled)
        return;

    
    
    const bool attackDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && gameFocused();
    if (attackDown && !prevAttackDown_)
        attackSignals_.fetch_add(1, std::memory_order_acq_rel);
    prevAttackDown_ = attackDown;

    if (autoClicker && autoClicker->lastAutoClick != observedAutoClick_) {
        observedAutoClick_ = autoClicker->lastAutoClick;
        attackSignals_.fetch_add(1, std::memory_order_acq_rel);
    }
}





void BlockHit::rollCycle() {
    std::uniform_int_distribution<int> pick(0, 99);

    
    
    
    
    const int beats = pick(rng_);
    beatsPerHole_ = (beats < 22) ? 3u : (beats < 58 ? 4u : (beats < 84 ? 5u : 6u));

    
    
    
    minHoldTicks_ = 2u + (std::uint32_t)(pick(rng_) % 3);

    
    
    
    
    maxHoldTicks_ = 20u + (std::uint32_t)(pick(rng_) % 15);

    
    
    
    
    const int hole = pick(rng_);
    if (hole < 86)
        holeTicks_ = 4u + (std::uint32_t)(pick(rng_) % 5);
    else
        holeTicks_ = 9u + (std::uint32_t)(pick(rng_) % 6);

    
    
    
    
    immuneMargin_ = 2u + (std::uint32_t)(pick(rng_) % 4);
}





bool BlockHit::isSwordId(int itemId) {
    
    
    return itemId == 268 || itemId == 272 || itemId == 267 || itemId == 276 || itemId == 283;
}

bool BlockHit::isHoldingSword() const {
    return isSwordId(player.getHeldItemId());
}

bool BlockHit::fightLive() const {
    
    if (haveTakenHit_ && (tick_ - lastHitTakenTick_) <= kPressureTicks)
        return true;
    
    
    if (aimAssist && aimAssist->isRewritingRotation())
        return true;
    
    
    return mc.isTargetingEntity();
}







bool BlockHit::resolveApi(JNIEnv* env, jobject playerInstance) {
    if (apiTried_)
        return apiUsable_;
    if (!env || !playerInstance || !mc.instance || !JniCache::fid_playerController)
        return false;  

    jclass playerCls = env->GetObjectClass(playerInstance);
    if (clearEx(env) || !playerCls)
        return false;

    
    
    
    
    const auto field = [&](jclass cls, const std::string& name, const std::string& srg,
                           const std::string& sig) -> jfieldID {
        jfieldID id = env->GetFieldID(cls, name.c_str(), sig.c_str());
        if (clearEx(env) || !id) {
            id = env->GetFieldID(cls, srg.c_str(), sig.c_str());
            clearEx(env);
        }
        return id;
    };

    fMovementInput_ = field(playerCls, OBFUSCATE_STRING("movementInput"),
                            OBFUSCATE_STRING("field_71158_b"),
                            OBFUSCATE_STRING("Lnet/minecraft/util/MovementInput;"));
    fSprintToggle_ = field(playerCls, OBFUSCATE_STRING("sprintToggleTimer"),
                           OBFUSCATE_STRING("field_71503_h"), OBFUSCATE_STRING("I"));
    
    fRidingEntity_ = field(playerCls, OBFUSCATE_STRING("ridingEntity"),
                           OBFUSCATE_STRING("field_70154_o"),
                           OBFUSCATE_STRING("Lnet/minecraft/entity/Entity;"));

    if (fMovementInput_) {
        jobject input = env->GetObjectField(playerInstance, fMovementInput_);
        if (!clearEx(env) && input) {
            jclass inputCls = env->GetObjectClass(input);
            if (!clearEx(env) && inputCls) {
                fInputStrafe_ = field(inputCls, OBFUSCATE_STRING("moveStrafe"),
                                      OBFUSCATE_STRING("field_78902_a"), OBFUSCATE_STRING("F"));
                fInputForward_ = field(inputCls, OBFUSCATE_STRING("moveForward"),
                                       OBFUSCATE_STRING("field_78900_b"), OBFUSCATE_STRING("F"));
                env->DeleteLocalRef(inputCls);
            }
        }
        if (input)
            env->DeleteLocalRef(input);
    }
    env->DeleteLocalRef(playerCls);

    jobject controller = env->GetObjectField(mc.instance, JniCache::fid_playerController);
    if (clearEx(env) || !controller)
        return false;  
    jclass controllerCls = env->GetObjectClass(controller);
    if (!clearEx(env) && controllerCls) {
        const std::string playerSig =
            OBFUSCATE_STRING("(Lnet/minecraft/entity/player/EntityPlayer;)V");
        midOnStoppedUsing_ = env->GetMethodID(
            controllerCls, OBFUSCATE_STRING("onStoppedUsingItem").c_str(), playerSig.c_str());
        if (clearEx(env) || !midOnStoppedUsing_) {
            midOnStoppedUsing_ = env->GetMethodID(
                controllerCls, OBFUSCATE_STRING("func_78766_b").c_str(), playerSig.c_str());
            clearEx(env);
        }
        env->DeleteLocalRef(controllerCls);
    }
    env->DeleteLocalRef(controller);

    
    
    
    
    
    
    apiUsable_ = midOnStoppedUsing_ != nullptr && fMovementInput_ != nullptr &&
                 fInputStrafe_ != nullptr && fInputForward_ != nullptr;
    apiTried_ = true;
    return apiUsable_;
}

bool BlockHit::clientUsingItem(JNIEnv* env, jobject playerInstance) const {
    if (!env || !playerInstance || !player.mid_isUsingItem)
        return false;
    const jboolean value = env->CallBooleanMethod(playerInstance, player.mid_isUsingItem);
    if (clearEx(env))
        return false;
    return value == JNI_TRUE;
}






























void BlockHit::applyVanillaSlowdown(JNIEnv* env, jobject playerInstance) {
    if (!env || !playerInstance || !fMovementInput_ || !fInputStrafe_ || !fInputForward_)
        return;
    jobject input = env->GetObjectField(playerInstance, fMovementInput_);
    if (clearEx(env) || !input)
        return;

    const float strafe = env->GetFloatField(input, fInputStrafe_) * 0.2f;
    const float forward = env->GetFloatField(input, fInputForward_) * 0.2f;
    if (!clearEx(env)) {
        env->SetFloatField(input, fInputStrafe_, strafe);
        env->SetFloatField(input, fInputForward_, forward);
        clearEx(env);
    }
    env->DeleteLocalRef(input);

    
    
    
    if (fSprintToggle_) {
        env->SetIntField(playerInstance, fSprintToggle_, 0);
        clearEx(env);
    }

    
    
    
    
    if (forward < 0.8f && player.isSprinting())
        player.setSprinting(false);
}



















bool BlockHit::undoVanillaSlowdown(JNIEnv* env, jobject playerInstance, bool scaled) {
    if (!env || !playerInstance || !fMovementInput_ || !fInputStrafe_ || !fInputForward_)
        return false;
    jobject input = env->GetObjectField(playerInstance, fMovementInput_);
    if (clearEx(env) || !input)
        return false;

    const float factor = scaled ? 5.0f : 1.0f;
    const float strafe = env->GetFloatField(input, fInputStrafe_) * factor;
    const float forward = env->GetFloatField(input, fInputForward_) * factor;
    bool held = false;
    if (!clearEx(env)) {
        if (scaled) {
            env->SetFloatField(input, fInputStrafe_, strafe);
            env->SetFloatField(input, fInputForward_, forward);
        }
        held = !clearEx(env) && forward >= 0.8f;
    }
    env->DeleteLocalRef(input);
    return held;
}





bool BlockHit::isRiding(JNIEnv* env, jobject playerInstance) const {
    if (!env || !playerInstance || !fRidingEntity_)
        return false;
    jobject mount = env->GetObjectField(playerInstance, fRidingEntity_);
    if (clearEx(env))
        return false;
    if (!mount)
        return false;
    env->DeleteLocalRef(mount);
    return true;
}

bool BlockHit::needsClientTick() const {
    
    
    return enabled || serverBlocking_.load(std::memory_order_acquire);
}

bool BlockHit::sendBlock(JNIEnv* env, jobject playerInstance) {
    if (!env || !playerInstance)
        return false;
    
    
    
    wasSprinting_ = player.isSprinting();
    
    
    
    const bool reported = mc.sendUseHeldItem();
    
    
    
    
    
    const bool took = reported || clientUsingItem(env, playerInstance);
    if (!took) {
        wasSprinting_ = false;
        return false;  
    }
    
    
    
    
    serverBlocking_.store(true, std::memory_order_release);
    blockedAtTick_ = tick_;
    
    
    beatsHeld_ = 0;
    holePending_ = false;
    immuneHole_ = false;
    
    
    applyVanillaSlowdown(env, playerInstance);
    return true;
}

void BlockHit::sendRelease(JNIEnv* env, jobject playerInstance, bool immune) {
    
    
    
    serverBlocking_.store(false, std::memory_order_release);
    releasedAtTick_ = tick_;
    beatsHeld_ = 0;
    holePending_ = true;
    
    
    immuneHole_ = immune;
    
    
    rollCycle();

    if (!env || !playerInstance || !midOnStoppedUsing_ || !mc.instance ||
        !JniCache::fid_playerController) {
        wasSprinting_ = false;
        return;
    }
    jobject controller = env->GetObjectField(mc.instance, JniCache::fid_playerController);
    if (clearEx(env) || !controller) {
        wasSprinting_ = false;
        return;
    }
    
    
    
    
    
    const bool vanillaScaled = clientUsingItem(env, playerInstance) && !isRiding(env, playerInstance);
    
    
    
    
    
    env->CallVoidMethod(controller, midOnStoppedUsing_, playerInstance);
    clearEx(env);
    env->DeleteLocalRef(controller);

    
    
    const bool forwardHeld = undoVanillaSlowdown(env, playerInstance, vanillaScaled);
    
    
    
    
    
    if (wasSprinting_ && forwardHeld && !player.isSprinting())
        player.setSprinting(true);
    wasSprinting_ = false;
}

void BlockHit::standDown(JNIEnv* env, jobject playerInstance) {
    if (serverBlocking_.load(std::memory_order_acquire))
        sendRelease(env, playerInstance);
    beatsHeld_ = 0;
    
    
    holePending_ = false;
}

















void BlockHit::abandonBlock() {
    serverBlocking_.store(false, std::memory_order_release);
    beatsHeld_ = 0;
    holePending_ = false;
}






void BlockHit::onClientTick(JNIEnv* env) {
    if (!env)
        return;
    jobject self = player.instance;

    ++tick_;

    
    
    std::uint32_t signals = attackSignals_.exchange(0, std::memory_order_acq_rel);
    
    
    bool beat = signals > 0;

    
    
    
    
    const int hurt = player.getHurtTime();
    const bool hitTaken = hurt > prevHurtTime_;
    if (hitTaken) {
        lastHitTakenTick_ = tick_;
        haveTakenHit_ = true;
        beat = true;
    }
    prevHurtTime_ = hurt;

    
    
    
    if (!player.isValid() || !self) {
        serverBlocking_.store(false, std::memory_order_release);
        beatsHeld_ = 0;
        holePending_ = false;
        gateReason_.store("No Player", std::memory_order_release);
        return;
    }

    
    
    resolveApi(env, self);

    
    
    const bool rmbDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool clientUsing = clientUsingItem(env, self);
    
    
    
    
    const bool foreignUse = clientUsing && !serverBlocking_.load(std::memory_order_acquire);

    const char* gate = nullptr;
    
    
    bool handOver = false;
    if (!enabled)
        gate = "Off";
    else if (Menu::isOpen || mc.isInGui())
        gate = "GUI";
    else if (!gameFocused())
        gate = "Unfocused";
    
    
    
    
    
    
    else if (rmbDown || foreignUse) {
        gate = "Manual";
        
        
        
        handOver = foreignUse;
    }
    
    
    else if (autoRod && autoRod->isSequencing())
        gate = "AutoRod";
    
    
    
    
    else if (isRiding(env, self))
        gate = "Riding";
    
    
    
    else if (!isHoldingSword())
        gate = "No Sword";
    
    else if (!apiUsable_)
        gate = "No API";

    gateReason_.store(gate, std::memory_order_release);
    if (gate) {
        if (handOver)
            abandonBlock();
        else
            standDown(env, self);
        return;
    }

    

    
    
    
    const bool live = fightLive();

    if (serverBlocking_.load(std::memory_order_acquire)) {
        if (beat)
            ++beatsHeld_;
        
        if (!live) {
            standDown(env, self);
            return;
        }
        const std::uint32_t held = tick_ - blockedAtTick_;
        
        
        if (held < minHoldTicks_)
            return;
        
        
        
        
        
        
        if (hitTaken || beatsHeld_ >= beatsPerHole_ || held >= maxHoldTicks_)
            sendRelease(env, self, hurt > 0);
        return;
    }

    
    if (!live) {
        
        
        holePending_ = false;
        return;
    }
    if (holePending_) {
        if (immuneHole_) {
            
            
            
            
            if (hurt > static_cast<int>(immuneMargin_))
                return;
        } else if (tick_ - releasedAtTick_ < holeTicks_) {
            
            
            
            return;
        }
    } else if (!beat) {
        
        
        return;
    } else if (hurt > static_cast<int>(immuneMargin_)) {
        
        
        
        return;
    }
    sendBlock(env, self);
}

std::string BlockHit::getDisplayInfo() {
    
    
    if (const char* gate = gateReason_.load(std::memory_order_acquire))
        return gate;
    
    return serverBlocking_.load(std::memory_order_acquire) ? "Block" : "Open";
}

void BlockHit::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled)
                    setEnabled(enabledValue);
            }
        },
        "Pattern",
        []() {
            
            
            
            ImGui::TextDisabled("A real block, at a real block's");
            ImGui::TextDisabled("cost: a fifth of your movement,");
            ImGui::TextDisabled("no sprint. So it only goes up");
            ImGui::TextDisabled("when a hit can land on you, and");
            ImGui::TextDisabled("never in the ten immune ticks");
            ImGui::TextDisabled("after one does. Nothing to tune.");
        });
}
