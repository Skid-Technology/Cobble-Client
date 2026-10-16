#pragma once

#include "../module.h"
#include "../../jni/java.h"

#include <cstdint>

class NoSlowDown : public Module {
public:
    enum class NoSlowMode { Consumable, Sword, Bow, All };
    enum class HeldItemType { None, Sword, Bow, Consume };

    NoSlowMode mode = NoSlowMode::All;

    float blockForward = 1.0f;
    float blockStrafe = 1.0f;
    float consumeForward = 1.0f;
    float consumeStrafe = 1.0f;
    float bowForward = 1.0f;
    float bowStrafe = 1.0f;

    
    
    jclass cachedPlayerClass = nullptr;

    NoSlowDown();

    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void applyMovement(JNIEnv* env, jobject playerInstance);
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

private:
    bool initializeFields(JNIEnv* env, jobject playerInstance);
    bool isKeyPressed(JNIEnv* env, jobject gameSettings, jfieldID keyBindField);
    bool isPlayerUsingItem(JNIEnv* env, jobject playerInstance);

    HeldItemType getHeldItemType();

    jfieldID movementInputField = nullptr;
    jfieldID moveForwardField = nullptr;
    jfieldID moveStrafeField = nullptr;
    jfieldID entityMoveForwardField = nullptr;
    jfieldID entityMoveStrafeField = nullptr;
    jfieldID itemInUseCountField = nullptr;
};

extern NoSlowDown* noSlowDown;
