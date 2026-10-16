#include "anti_debuff.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

AntiDebuff* antiDebuff = nullptr;

namespace {
constexpr int NAUSEA_POTION_ID = 9;
constexpr int BLINDNESS_POTION_ID = 15;
}

AntiDebuff::AntiDebuff() : Module("AntiDebuff") {
    keybind = 0;
}

bool AntiDebuff::initializeJni() {
    if (integerClass && integerValueOf && mapRemove)
        return true;

    JNIEnv* env = Java::Env();
    if (!env || !JniCache::cls_Map)
        return false;

    jclass localIntegerClass = env->FindClass("java/lang/Integer");
    if (!localIntegerClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    integerClass = static_cast<jclass>(env->NewGlobalRef(localIntegerClass));
    env->DeleteLocalRef(localIntegerClass);
    if (!integerClass)
        return false;

    integerValueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    mapRemove = env->GetMethodID(JniCache::cls_Map, "remove", "(Ljava/lang/Object;)Ljava/lang/Object;");

    if (env->ExceptionCheck() || !integerValueOf || !mapRemove) {
        env->ExceptionClear();
        releaseJni();
        return false;
    }

    return true;
}

void AntiDebuff::releaseJni() {
    JNIEnv* env = Java::Env();
    if (env && integerClass)
        env->DeleteGlobalRef(integerClass);

    integerClass = nullptr;
    integerValueOf = nullptr;
    mapRemove = nullptr;
}

void AntiDebuff::removePotionEffect(jobject activePotions, int potionId) {
    JNIEnv* env = Java::Env();
    if (!env || !activePotions)
        return;

    jobject key = env->CallStaticObjectMethod(integerClass, integerValueOf, potionId);
    if (env->ExceptionCheck() || !key) {
        env->ExceptionClear();
        return;
    }

    jobject removedEffect = env->CallObjectMethod(activePotions, mapRemove, key);
    if (env->ExceptionCheck())
        env->ExceptionClear();

    if (removedEffect)
        env->DeleteLocalRef(removedEffect);
    env->DeleteLocalRef(key);
}

void AntiDebuff::onDisable() {
    releaseJni();
}

void AntiDebuff::onUpdate() {
    if (!enabled || !Java::Initialized || !player.isValid() || !JniCache::fid_activePotionsMap)
        return;
    if (!initializeJni())
        return;

    JNIEnv* env = Java::Env();
    jobject activePotions = env->GetObjectField(player.instance, JniCache::fid_activePotionsMap);
    if (env->ExceptionCheck() || !activePotions) {
        env->ExceptionClear();
        return;
    }

    if (settings.blindness)
        removePotionEffect(activePotions, BLINDNESS_POTION_ID);
    if (settings.nausea)
        removePotionEffect(activePotions, NAUSEA_POTION_ID);

    env->DeleteLocalRef(activePotions);
}

void AntiDebuff::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);
        },
        "Effects",
        [this]() {
            ui::Checkbox("Blindness", &settings.blindness);
            ui::Checkbox("Nausea", &settings.nausea);
            ImGui::Spacing();
            ImGui::TextWrapped("Removes visual potion debuffs locally. The server-side potion state is unchanged.");
        });
}



void AntiDebuff::saveSettings(json::Value& node) const {
    node["blindness"] = settings.blindness;
    node["nausea"] = settings.nausea;
}

void AntiDebuff::loadSettings(const json::Value& node) {
    settings.blindness = node["blindness"].as_bool(settings.blindness);
    settings.nausea = node["nausea"].as_bool(settings.nausea);
}
