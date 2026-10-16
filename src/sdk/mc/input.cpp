


#include "../../util/logger.hpp"
#include "../core/minecraft.h"
#include "../jni/jni_cache.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>





static jobject getGameSettings(JNIEnv* env, jobject mcInstance) {
    if (!JniCache::fid_gameSettings)
        return nullptr;
    return env->GetObjectField(mcInstance, JniCache::fid_gameSettings);
}





static int getKeyBindCode(JNIEnv* env, jobject mcInstance, jfieldID keyBindField, int defaultCode) {
    if (!keyBindField || !JniCache::mid_getKeyCode)
        return defaultCode;

    jobject gameSettings = getGameSettings(env, mcInstance);
    if (!gameSettings)
        return defaultCode;

    int code = defaultCode;
    jobject keyBind = env->GetObjectField(gameSettings, keyBindField);
    if (keyBind) {
        code = env->CallIntMethod(keyBind, JniCache::mid_getKeyCode);
        env->DeleteLocalRef(keyBind);
    }

    env->DeleteLocalRef(gameSettings);
    return code;
}





void Minecraft::setKeyBindState(int keyCode, bool pressed) {
    if (!isValid())
        return;
    if (!JniCache::cls_KeyBinding || !JniCache::mid_setKeyBindState)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    if (pressed && isInGui() && keyCode != getSprintKey())
        return;

    env->CallStaticVoidMethod(JniCache::cls_KeyBinding, JniCache::mid_setKeyBindState, keyCode, pressed);
}





int Minecraft::getSneakKey() {
    if (!isValid())
        return 42;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindSneak, 42) : 42;
}





int Minecraft::getSprintKey() {
    if (!isValid())
        return 29;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindSprint, 29) : 29;
}

int Minecraft::getForwardKey() {
    if (!isValid())
        return 17;
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindForward, 17) : 17;
}

int Minecraft::getJumpKey() {
    if (!isValid())
        return 57;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindJump, 57) : 57;
}

int Minecraft::getBackKey() {
    if (!isValid())
        return 31;
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindBack, 31) : 31;
}

int Minecraft::getLeftKey() {
    if (!isValid())
        return 30;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindLeft, 30) : 30;
}

int Minecraft::getRightKey() {
    if (!isValid())
        return 32;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindRight, 32) : 32;
}





int Minecraft::getInventoryKey() {
    if (!isValid())
        return 18;  
    JNIEnv* env = Java::Env();
    return env ? getKeyBindCode(env, instance, JniCache::fid_keyBindInventory, 18) : 18;
}





bool Minecraft::isMoving() {
    auto physicalHeld = [](int lwjglKey) {
        const UINT vk = MapVirtualKeyA(static_cast<UINT>(lwjglKey), MAPVK_VSC_TO_VK);
        return vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
    };

    
    if ((GetAsyncKeyState('W') & 0x8000) != 0 ||
        (GetAsyncKeyState('A') & 0x8000) != 0 ||
        (GetAsyncKeyState('S') & 0x8000) != 0 ||
        (GetAsyncKeyState('D') & 0x8000) != 0) {
        return true;
    }

    if (!isValid())
        return false;

    
    if (physicalHeld(getForwardKey()) || physicalHeld(getBackKey()) ||
        physicalHeld(getLeftKey()) || physicalHeld(getRightKey())) {
        return true;
    }

    JNIEnv* env = Java::Env();
    if (!env || !JniCache::fid_pressed)
        return false;

    jobject gameSettings = getGameSettings(env, instance);
    if (!gameSettings)
        return false;

    const jfieldID moveFields[] = {JniCache::fid_keyBindForward, JniCache::fid_keyBindBack,
                                   JniCache::fid_keyBindLeft, JniCache::fid_keyBindRight};

    bool moving = false;
    for (jfieldID fid_kb : moveFields) {
        if (!fid_kb)
            continue;

        jobject keyBind = env->GetObjectField(gameSettings, fid_kb);
        if (!keyBind)
            continue;

        if (env->GetBooleanField(keyBind, JniCache::fid_pressed))
            moving = true;

        env->DeleteLocalRef(keyBind);

        if (moving)
            break;
    }

    env->DeleteLocalRef(gameSettings);
    return moving;
}





void Minecraft::releaseMovementKeys() {
    if (!isValid())
        return;
    if (!JniCache::cls_KeyBinding)
        return;

    
    if (keysReleased)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject gameSettings = getGameSettings(env, instance);
    if (!gameSettings)
        return;

    
    jfieldID keyBindFields[] = {JniCache::fid_keyBindForward, JniCache::fid_keyBindBack,   JniCache::fid_keyBindLeft,
                                JniCache::fid_keyBindRight,   JniCache::fid_keyBindJump,   JniCache::fid_keyBindSneak,
                                JniCache::fid_keyBindAttack,  JniCache::fid_keyBindUseItem};

    const char* keyBindNames[] = {"keyBindForward", "keyBindBack",  "keyBindLeft",   "keyBindRight",
                                  "keyBindJump",    "keyBindSneak", "keyBindAttack", "keyBindUseItem"};

    savedKeyStates.clear();

    for (int i = 0; i < 8; i++) {
        jfieldID fid_kb = keyBindFields[i];
        if (!fid_kb)
            continue;

        jobject keyBind = env->GetObjectField(gameSettings, fid_kb);
        if (!keyBind)
            continue;

        
        bool isPressed = false;
        if (JniCache::fid_pressed) {
            isPressed = env->GetBooleanField(keyBind, JniCache::fid_pressed);
            savedKeyStates[keyBindNames[i]] = isPressed;
        }

        
        if (JniCache::mid_getKeyCode && JniCache::mid_setKeyBindState) {
            int keyCode = env->CallIntMethod(keyBind, JniCache::mid_getKeyCode);
            env->CallStaticVoidMethod(JniCache::cls_KeyBinding, JniCache::mid_setKeyBindState, keyCode, JNI_FALSE);
        }

        
        if (JniCache::fid_pressed) {
            env->SetBooleanField(keyBind, JniCache::fid_pressed, JNI_FALSE);
        }

        env->DeleteLocalRef(keyBind);
    }

    keysReleased = true;
    env->DeleteLocalRef(gameSettings);
}





void Minecraft::restoreMovementKeys() {
    if (!isValid())
        return;
    if (!JniCache::cls_KeyBinding || !JniCache::mid_setKeyBindState || !JniCache::mid_getKeyCode)
        return;

    
    if (!keysReleased)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    jobject gameSettings = getGameSettings(env, instance);
    if (!gameSettings)
        return;

    
    
    
    
    
    
    
    
    if (isInGui()) {
        keysReleased = false;
        savedKeyStates.clear();
        env->DeleteLocalRef(gameSettings);
        return;
    }

    jfieldID keyBindFields[] = {JniCache::fid_keyBindForward, JniCache::fid_keyBindBack,   JniCache::fid_keyBindLeft,
                                JniCache::fid_keyBindRight,   JniCache::fid_keyBindJump,   JniCache::fid_keyBindSneak,
                                JniCache::fid_keyBindAttack,  JniCache::fid_keyBindUseItem};

    const char* keyBindNames[] = {"keyBindForward", "keyBindBack",  "keyBindLeft",   "keyBindRight",
                                  "keyBindJump",    "keyBindSneak", "keyBindAttack", "keyBindUseItem"};

    for (int i = 0; i < 8; i++) {
        jfieldID fid_kb = keyBindFields[i];
        if (!fid_kb)
            continue;

        jobject keyBind = env->GetObjectField(gameSettings, fid_kb);
        if (!keyBind)
            continue;

        int keyCode = env->CallIntMethod(keyBind, JniCache::mid_getKeyCode);

        
        bool wasPressed = savedKeyStates[keyBindNames[i]];

        
        bool isPhysicallyHeld = false;

        if (keyCode < 0) {
            
            int btn = keyCode + 100;
            if (btn == 0)
                isPhysicallyHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            else if (btn == 1)
                isPhysicallyHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            else if (btn == 2)
                isPhysicallyHeld = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        } else {
            
            UINT vk = MapVirtualKeyA(keyCode, MAPVK_VSC_TO_VK);
            if (vk != 0) {
                isPhysicallyHeld = (GetAsyncKeyState(vk) & 0x8000) != 0;
            }
        }

        
        if (isPhysicallyHeld) {
            env->CallStaticVoidMethod(JniCache::cls_KeyBinding, JniCache::mid_setKeyBindState, keyCode, JNI_TRUE);

            if (JniCache::fid_pressed) {
                env->SetBooleanField(keyBind, JniCache::fid_pressed, JNI_TRUE);
            }
        }

        env->DeleteLocalRef(keyBind);
    }

    keysReleased = false;
    savedKeyStates.clear();
    env->DeleteLocalRef(gameSettings);
}





void Minecraft::ungrabMouse() {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    
    if (JniCache::fid_mouseHelper && JniCache::mid_ungrabMouseCursor) {
        jobject mouseHelper = env->GetObjectField(instance, JniCache::fid_mouseHelper);
        if (mouseHelper) {
            env->CallVoidMethod(mouseHelper, JniCache::mid_ungrabMouseCursor);
            env->DeleteLocalRef(mouseHelper);
        }
    }
    env->ExceptionClear();

    
    if (JniCache::fid_inGameHasFocus) {
        env->SetBooleanField(instance, JniCache::fid_inGameHasFocus, JNI_FALSE);
    }
    env->ExceptionClear();

    
    if (JniCache::cls_Mouse && JniCache::mid_setGrabbed) {
        env->CallStaticVoidMethod(JniCache::cls_Mouse, JniCache::mid_setGrabbed, JNI_FALSE);
    }
    env->ExceptionClear();
}





void Minecraft::grabMouse() {
    if (!isValid())
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    
    if (JniCache::fid_inGameHasFocus) {
        env->SetBooleanField(instance, JniCache::fid_inGameHasFocus, JNI_TRUE);
    }
    env->ExceptionClear();

    
    if (JniCache::fid_mouseHelper && JniCache::mid_grabMouseCursor) {
        jobject mouseHelper = env->GetObjectField(instance, JniCache::fid_mouseHelper);
        if (mouseHelper) {
            env->CallVoidMethod(mouseHelper, JniCache::mid_grabMouseCursor);
            env->DeleteLocalRef(mouseHelper);
        }
    }
    env->ExceptionClear();

    
    if (JniCache::cls_Mouse && JniCache::mid_setGrabbed) {
        env->CallStaticVoidMethod(JniCache::cls_Mouse, JniCache::mid_setGrabbed, JNI_TRUE);
    }
    env->ExceptionClear();
}
