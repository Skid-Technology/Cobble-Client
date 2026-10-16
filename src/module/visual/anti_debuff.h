#pragma once

#include "../module.h"
#include "../../jni/java.h"

class AntiDebuff : public Module {
public:
    AntiDebuff();

    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        bool blindness = true;
        bool nausea = true;
    } settings;

private:
    bool initializeJni();
    void releaseJni();
    void removePotionEffect(jobject activePotions, int potionId);

    jclass integerClass = nullptr;
    jmethodID integerValueOf = nullptr;
    jmethodID mapRemove = nullptr;
};

extern AntiDebuff* antiDebuff;
