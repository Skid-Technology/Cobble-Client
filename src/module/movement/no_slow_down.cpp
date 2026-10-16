#include "no_slow_down.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"
#include "../combat/blockhit.h"
#include "sprint.h"

#include <algorithm>
#include <mutex>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

NoSlowDown* noSlowDown = nullptr;

namespace {

bool clearJniException(JNIEnv* env) {
    if (!env || !env->ExceptionCheck())
        return false;
    env->ExceptionClear();
    return true;
}

jclass assignGlobalClass(JNIEnv* env, const char* name) {
    jclass local = nullptr;
    if (!env || !Java::AssignClass(name, local) || !local) {
        clearJniException(env);
        return nullptr;
    }

    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!global)
        clearJniException(env);
    return global;
}

bool isSwordId(int id) {
    return id == 267 || id == 268 || id == 272 || id == 276 || id == 283;
}

bool isBowId(int id) {
    return id == 261;
}

bool isConsumeId(int id) {
    switch (id) {
        case 260:  
        case 282:  
        case 297:  
        case 319:  
        case 320:  
        case 322:  
        case 335:  
        case 349:  
        case 350:  
        case 357:  
        case 360:  
        case 363:  
        case 364:  
        case 365:  
        case 366:  
        case 367:  
        case 373:  
        case 391:  
        case 392:  
        case 393:  
        case 394:  
        case 396:  
        case 400:  
        case 411:  
        case 412:  
        case 413:  
        case 423:  
        case 424:  
            return true;
        default:
            return false;
    }
}

std::pair<float, float> getMultipliers(NoSlowDown::HeldItemType type, const NoSlowDown& module) {
    switch (type) {
        case NoSlowDown::HeldItemType::Sword:
            return {module.blockForward, module.blockStrafe};
        case NoSlowDown::HeldItemType::Bow:
            return {module.bowForward, module.bowStrafe};
        case NoSlowDown::HeldItemType::Consume:
            return {module.consumeForward, module.consumeStrafe};
        default:
            return {0.2f, 0.2f};
    }
}

}  

NoSlowDown::NoSlowDown() : Module("NoSlowDown") {
    keybind = 0;
}

void NoSlowDown::onDisable() {
    
    JNIEnv* env = Java::Env();
    if (env && cachedPlayerClass) {
        env->DeleteGlobalRef(cachedPlayerClass);
        cachedPlayerClass = nullptr;
    }
    
    movementInputField     = nullptr;
    moveForwardField       = nullptr;
    moveStrafeField        = nullptr;
    entityMoveForwardField = nullptr;
    entityMoveStrafeField  = nullptr;
    itemInUseCountField    = nullptr;
}

bool NoSlowDown::initializeFields(JNIEnv* env, jobject playerInstance) {
    if (!env || !playerInstance)
        return false;

    jclass playerClass = env->GetObjectClass(playerInstance);
    if (!playerClass)
        return false;

    
    if (cachedPlayerClass && !env->IsSameObject(cachedPlayerClass, playerClass)) {
        env->DeleteGlobalRef(cachedPlayerClass);
        cachedPlayerClass = nullptr;
        movementInputField = nullptr;
        moveForwardField   = nullptr;
        moveStrafeField    = nullptr;
        entityMoveForwardField = nullptr;
        entityMoveStrafeField  = nullptr;
        itemInUseCountField    = nullptr;
    }

    if (movementInputField && moveForwardField && moveStrafeField) {
        env->DeleteLocalRef(playerClass);
        return true;
    }

    if (!cachedPlayerClass)
        cachedPlayerClass = static_cast<jclass>(env->NewGlobalRef(playerClass));

    const auto findField = [env](jclass cls, const char* name, const char* fallback,
                                 const char* signature) -> jfieldID {
        jfieldID field = env->GetFieldID(cls, name, signature);
        if (!field) {
            clearJniException(env);
            field = env->GetFieldID(cls, fallback, signature);
        }
        clearJniException(env);
        return field;
    };

    movementInputField =
        findField(playerClass, "movementInput", "field_71158_b", "Lnet/minecraft/util/MovementInput;");
    entityMoveStrafeField =
        findField(playerClass, "moveStrafing", "field_70702_br", "F");
    entityMoveForwardField =
        findField(playerClass, "moveForward", "field_70701_bs", "F");
    itemInUseCountField =
        findField(playerClass, "itemInUseCount", "field_71072_f", "I");

    if (!movementInputField) {
        env->DeleteLocalRef(playerClass);
        return false;
    }

    jobject movementInput = env->GetObjectField(playerInstance, movementInputField);
    if (!movementInput || clearJniException(env)) {
        env->DeleteLocalRef(playerClass);
        return false;
    }

    jclass movementInputClass = env->GetObjectClass(movementInput);
    if (movementInputClass) {
        moveStrafeField =
            findField(movementInputClass, "moveStrafe", "field_78902_a", "F");
        moveForwardField =
            findField(movementInputClass, "moveForward", "field_78900_b", "F");
        env->DeleteLocalRef(movementInputClass);
    }

    env->DeleteLocalRef(movementInput);
    env->DeleteLocalRef(playerClass);
    return moveForwardField && moveStrafeField;
}

bool NoSlowDown::isKeyPressed(JNIEnv* env, jobject gameSettings, jfieldID keyBindField) {
    if (!env || !gameSettings || !keyBindField)
        return false;

    jobject keyBind = env->GetObjectField(gameSettings, keyBindField);
    if (!keyBind || clearJniException(env))
        return false;

    bool pressed = false;
    if (JniCache::fid_pressed) {
        pressed = env->GetBooleanField(keyBind, JniCache::fid_pressed) == JNI_TRUE;
        clearJniException(env);
    }

    if (!pressed && JniCache::mid_getKeyCode) {
        const int keyCode = env->CallIntMethod(keyBind, JniCache::mid_getKeyCode);
        if (!clearJniException(env)) {
            if (keyCode < 0) {
                const int button = keyCode + 100;
                const int virtualKey = button == 0 ? VK_LBUTTON :
                                       button == 1 ? VK_RBUTTON :
                                       button == 2 ? VK_MBUTTON : 0;
                pressed = virtualKey != 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
            } else {
                const UINT virtualKey =
                    MapVirtualKeyA(static_cast<UINT>(keyCode), MAPVK_VSC_TO_VK);
                pressed = virtualKey != 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
            }
        }
    }

    env->DeleteLocalRef(keyBind);
    return pressed;
}

bool NoSlowDown::isPlayerUsingItem(JNIEnv* env, jobject playerInstance) {
    if (!env || !playerInstance)
        return false;

    
    
    bool usingItem = false;
    if (player.mid_isUsingItem) {
        usingItem = env->CallBooleanMethod(playerInstance, player.mid_isUsingItem) == JNI_TRUE;
        if (clearJniException(env))
            usingItem = false;
    }

    if (!usingItem && itemInUseCountField) {
        usingItem = env->GetIntField(playerInstance, itemInUseCountField) > 0;
        if (clearJniException(env))
            usingItem = false;
    }
    return usingItem;
}

NoSlowDown::HeldItemType NoSlowDown::getHeldItemType() {
    const int id = player.getHeldItemId();
    if (isSwordId(id))
        return HeldItemType::Sword;
    if (isBowId(id))
        return HeldItemType::Bow;
    if (isConsumeId(id))
        return HeldItemType::Consume;
    return HeldItemType::None;
}

void NoSlowDown::applyMovement(JNIEnv* env, jobject playerInstance) {
    if (!enabled || !env || !playerInstance || !mc.instance || mc.isInGui())
        return;
    if (!initializeFields(env, playerInstance))
        return;

    const HeldItemType type = getHeldItemType();
    if (type == HeldItemType::None)
        return;

    
    if (mode == NoSlowMode::Consumable && type != HeldItemType::Consume)
        return;
    if (mode == NoSlowMode::Sword && type != HeldItemType::Sword)
        return;
    if (mode == NoSlowMode::Bow && type != HeldItemType::Bow)
        return;

    jobject gameSettings = JniCache::fid_gameSettings
        ? env->GetObjectField(mc.instance, JniCache::fid_gameSettings)
        : nullptr;
    if (!gameSettings || clearJniException(env))
        return;

    const bool usePressed =
        isKeyPressed(env, gameSettings, JniCache::fid_keyBindUseItem);
    
    
    
    
    
    
    
    
    
    
    if (!usePressed && blockHit && blockHit->isServerBlocking()) {
        env->DeleteLocalRef(gameSettings);
        return;
    }
    if (!usePressed && !isPlayerUsingItem(env, playerInstance)) {
        env->DeleteLocalRef(gameSettings);
        return;
    }

    float forward = 0.0f;
    float strafe = 0.0f;
    if (isKeyPressed(env, gameSettings, JniCache::fid_keyBindForward))
        forward += 1.0f;
    if (isKeyPressed(env, gameSettings, JniCache::fid_keyBindBack))
        forward -= 1.0f;
    if (isKeyPressed(env, gameSettings, JniCache::fid_keyBindLeft))
        strafe += 1.0f;
    if (isKeyPressed(env, gameSettings, JniCache::fid_keyBindRight))
        strafe -= 1.0f;

    const bool sprintPressed =
        isKeyPressed(env, gameSettings, JniCache::fid_keyBindSprint) || (sprint && sprint->enabled);

    const bool sneaking =
        player.isSneaking() || isKeyPressed(env, gameSettings, JniCache::fid_keyBindSneak);
    env->DeleteLocalRef(gameSettings);
    if (clearJniException(env))
        return;

    if (sprintPressed && forward > 0.0f && !sneaking) {
        player.setSprinting(true);
    }

    if (sneaking) {
        forward *= 0.3f;
        strafe *= 0.3f;
    }

    const auto [forwardMultiplier, strafeMultiplier] = getMultipliers(type, *this);
    forward *= (std::clamp)(forwardMultiplier, 0.2f, 1.0f);
    strafe *= (std::clamp)(strafeMultiplier, 0.2f, 1.0f);

    jobject movementInput = env->GetObjectField(playerInstance, movementInputField);
    if (!movementInput || clearJniException(env))
        return;

    env->SetFloatField(movementInput, moveForwardField, forward);
    env->SetFloatField(movementInput, moveStrafeField, strafe);
    if (entityMoveForwardField)
        env->SetFloatField(playerInstance, entityMoveForwardField, forward);
    if (entityMoveStrafeField)
        env->SetFloatField(playerInstance, entityMoveStrafeField, strafe);

    env->DeleteLocalRef(movementInput);
    clearJniException(env);
}

void NoSlowDown::onUpdate() {
    JNIEnv* env = Java::Env();
    if (!env || !mc.instance || Menu::isOpen || mc.isInGui())
        return;

    
    
    
    std::lock_guard<std::recursive_mutex> lock(player.instanceMutex);
    if (!player.isValid())
        return;

    applyMovement(env, player.instance);
}

static const char* kModeNames[] = {"Consumable", "Sword", "Bow", "All"};

void NoSlowDown::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);

            ImGui::Spacing();

            int modeIdx = static_cast<int>(mode);
            if (ui::Combo("Mode", &modeIdx, kModeNames, 4))
                mode = static_cast<NoSlowMode>(modeIdx);
        },
        "Multipliers",
        [this]() {
            ImGui::Text("-- Sword (blocking) --");
            ui::SliderFloat("Fwd##blk", &blockForward, 0.2f, 1.0f, "%.2f");
            ui::SliderFloat("Strafe##blk", &blockStrafe, 0.2f, 1.0f, "%.2f");

            ImGui::Spacing();
            ImGui::Text("-- Consume (food/pot) --");
            ui::SliderFloat("Fwd##con", &consumeForward, 0.2f, 1.0f, "%.2f");
            ui::SliderFloat("Strafe##con", &consumeStrafe, 0.2f, 1.0f, "%.2f");

            ImGui::Spacing();
            ImGui::Text("-- Bow --");
            ui::SliderFloat("Fwd##bow", &bowForward, 0.2f, 1.0f, "%.2f");
            ui::SliderFloat("Strafe##bow", &bowStrafe, 0.2f, 1.0f, "%.2f");
        });
}

void NoSlowDown::saveSettings(json::Value& node) const {
    node["mode"] = static_cast<int>(mode);
    node["blockForward"] = blockForward;
    node["blockStrafe"] = blockStrafe;
    node["consumeForward"] = consumeForward;
    node["consumeStrafe"] = consumeStrafe;
    node["bowForward"] = bowForward;
    node["bowStrafe"] = bowStrafe;
}

void NoSlowDown::loadSettings(const json::Value& node) {
    int savedMode = node.has("mode") ? node["mode"].as_int(static_cast<int>(mode)) : node["packetMode"].as_int(static_cast<int>(mode));
    mode = static_cast<NoSlowMode>((std::clamp)(savedMode, 0, 3));
    blockForward = (std::clamp)(node["blockForward"].as_float(blockForward), 0.2f, 1.0f);
    blockStrafe = (std::clamp)(node["blockStrafe"].as_float(blockStrafe), 0.2f, 1.0f);
    consumeForward = (std::clamp)(node["consumeForward"].as_float(consumeForward), 0.2f, 1.0f);
    consumeStrafe = (std::clamp)(node["consumeStrafe"].as_float(consumeStrafe), 0.2f, 1.0f);
    bowForward = (std::clamp)(node["bowForward"].as_float(bowForward), 0.2f, 1.0f);
    bowStrafe = (std::clamp)(node["bowStrafe"].as_float(bowStrafe), 0.2f, 1.0f);
}
