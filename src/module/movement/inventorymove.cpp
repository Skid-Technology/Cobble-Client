#include "inventorymove.h"

#include <algorithm>

#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

InventoryMove* inventoryMove = nullptr;

namespace {

bool clearEx(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}


bool physicallyHeld(int lwjglKey) {
    if (lwjglKey == 0)
        return false;
    if (lwjglKey < 0) {
        switch (lwjglKey + 100) {
            case 0:
                return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            case 1:
                return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            case 2:
                return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            default:
                return false;
        }
    }
    const UINT vk = MapVirtualKeyA(static_cast<UINT>(lwjglKey), MAPVK_VSC_TO_VK);
    return vk != 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

jclass globalClass(JNIEnv* env, const char* name) {
    jclass local = nullptr;
    if (!Java::AssignClass(name, local) || !local) {
        clearEx(env);
        return nullptr;
    }
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    clearEx(env);
    return global;
}

}  

InventoryMove::InventoryMove() : Module("InventoryMove") {
    keybind = 0;
    binds_[Forward].field = &JniCache::fid_keyBindForward;
    binds_[Back].field = &JniCache::fid_keyBindBack;
    binds_[Left].field = &JniCache::fid_keyBindLeft;
    binds_[Right].field = &JniCache::fid_keyBindRight;
    binds_[Jump].field = &JniCache::fid_keyBindJump;
    binds_[Sneak].field = &JniCache::fid_keyBindSneak;
    binds_[Sprint].field = &JniCache::fid_keyBindSprint;
}

void InventoryMove::onEnable() {
    active_ = false;
    lastKind_ = ScreenKind::None;
    for (Bind& b : binds_) {
        b.keyCode = 0;
        b.wanted = false;
        b.written = false;
        b.owned = false;
    }
}

void InventoryMove::onDisable() {
    JNIEnv* env = Java::Env();
    if (env)
        handBack(env, !Menu::isOpen);
    active_ = false;
    lastKind_ = ScreenKind::None;
}

void InventoryMove::resolveClasses(JNIEnv* env) {
    if (classesResolved_)
        return;
    classesResolved_ = true;
    clsGuiContainer_ = globalClass(env, "net.minecraft.client.gui.inventory.GuiContainer");
    clsGuiChat_ = globalClass(env, "net.minecraft.client.gui.GuiChat");
    clsGuiRepair_ = globalClass(env, "net.minecraft.client.gui.inventory.GuiRepair");
    clsGuiEditSign_ = globalClass(env, "net.minecraft.client.gui.GuiEditSign");
    clsGuiBook_ = globalClass(env, "net.minecraft.client.gui.GuiScreenBook");
}

InventoryMove::ScreenKind InventoryMove::classifyScreen(JNIEnv* env) {
    if (!mc.instance || !JniCache::fid_currentScreen)
        return ScreenKind::None;

    jobject screen = env->GetObjectField(mc.instance, JniCache::fid_currentScreen);
    if (clearEx(env) || !screen)
        return ScreenKind::None;

    resolveClasses(env);

    ScreenKind kind = ScreenKind::Other;
    
    
    const jclass textScreens[] = {clsGuiChat_, clsGuiRepair_, clsGuiEditSign_, clsGuiBook_};
    for (jclass cls : textScreens) {
        if (cls && env->IsInstanceOf(screen, cls) == JNI_TRUE) {
            kind = ScreenKind::TextEntry;
            break;
        }
    }
    if (kind == ScreenKind::Other && clsGuiContainer_ &&
        env->IsInstanceOf(screen, clsGuiContainer_) == JNI_TRUE)
        kind = ScreenKind::Container;

    env->DeleteLocalRef(screen);
    clearEx(env);
    return kind;
}

bool InventoryMove::screenAllowed(ScreenKind kind) const {
    switch (kind) {
        case ScreenKind::Container:
            return settings.inContainers;
        case ScreenKind::TextEntry:
            return settings.inTextEntry;
        case ScreenKind::Other:
            return settings.inOther;
        default:
            return false;
    }
}

bool InventoryMove::bindAllowed(int index) const {
    switch (index) {
        case Forward:
        case Back:
        case Left:
        case Right:
            return settings.movement;
        case Jump:
            return settings.jump;
        case Sneak:
            return settings.sneak;
        case Sprint:
            return settings.sprint;
        default:
            return false;
    }
}

void InventoryMove::cacheKeyCodes(JNIEnv* env, jobject gameSettings) {
    for (Bind& b : binds_) {
        b.keyCode = 0;
        if (!b.field || !*b.field || !JniCache::mid_getKeyCode)
            continue;
        jobject keyBind = env->GetObjectField(gameSettings, *b.field);
        if (clearEx(env) || !keyBind)
            continue;
        const jint code = env->CallIntMethod(keyBind, JniCache::mid_getKeyCode);
        if (!clearEx(env))
            b.keyCode = static_cast<int>(code);
        env->DeleteLocalRef(keyBind);
    }
}

void InventoryMove::flush(JNIEnv* env, jobject gameSettings) {
    if (!JniCache::fid_pressed)
        return;
    for (int i = 0; i < Count; ++i) {
        Bind& b = binds_[i];
        if (b.wanted == b.written)
            continue;
        if (!b.field || !*b.field)
            continue;
        jobject keyBind = env->GetObjectField(gameSettings, *b.field);
        if (clearEx(env) || !keyBind)
            continue;
        env->SetBooleanField(keyBind, JniCache::fid_pressed, b.wanted ? JNI_TRUE : JNI_FALSE);
        clearEx(env);
        
        
        if (!b.wanted && JniCache::fid_pressTime) {
            env->SetIntField(keyBind, JniCache::fid_pressTime, 0);
            clearEx(env);
        }
        env->DeleteLocalRef(keyBind);
        b.written = b.wanted;
        b.owned = b.wanted;
    }
}

void InventoryMove::handBack(JNIEnv* env, bool syncPhysical) {
    bool anyOwned = false;
    for (const Bind& b : binds_)
        anyOwned = anyOwned || b.owned;
    if (!anyOwned) {
        for (Bind& b : binds_) {
            b.written = false;
            b.wanted = false;
        }
        return;
    }

    jobject gameSettings = (mc.instance && JniCache::fid_gameSettings)
                               ? env->GetObjectField(mc.instance, JniCache::fid_gameSettings)
                               : nullptr;
    if (clearEx(env) || !gameSettings) {
        for (Bind& b : binds_) {
            b.owned = false;
            b.written = false;
            b.wanted = false;
        }
        return;
    }

    for (Bind& b : binds_) {
        
        b.wanted = syncPhysical && b.owned && physicallyHeld(b.keyCode);
        if (!b.owned)
            b.written = b.wanted;  
    }
    flush(env, gameSettings);

    for (Bind& b : binds_) {
        b.owned = false;
        b.written = false;
        b.wanted = false;
    }
    env->DeleteLocalRef(gameSettings);
    clearEx(env);
}

void InventoryMove::onUpdate() {
    if (!enabled || !mc.isValid())
        return;
    JNIEnv* env = Java::Env();
    if (!env)
        return;

    
    
    if (Menu::isOpen) {
        if (active_) {
            handBack(env, false);
            active_ = false;
            lastKind_ = ScreenKind::None;
        }
        return;
    }

    const ScreenKind kind = classifyScreen(env);
    const bool want = kind != ScreenKind::None && screenAllowed(kind) && player.isValid();

    if (!want) {
        if (active_) {
            
            
            handBack(env, kind == ScreenKind::None);
            active_ = false;
        }
        lastKind_ = kind;
        return;
    }

    jobject gameSettings = JniCache::fid_gameSettings
                               ? env->GetObjectField(mc.instance, JniCache::fid_gameSettings)
                               : nullptr;
    if (clearEx(env) || !gameSettings)
        return;

    if (!active_ || kind != lastKind_) {
        
        
        cacheKeyCodes(env, gameSettings);
        for (Bind& b : binds_) {
            b.written = false;
            b.wanted = false;
        }
        active_ = true;
        lastKind_ = kind;
    }

    for (int i = 0; i < Count; ++i)
        binds_[i].wanted = bindAllowed(i) && physicallyHeld(binds_[i].keyCode);

    flush(env, gameSettings);
    env->DeleteLocalRef(gameSettings);
    clearEx(env);
}

std::string InventoryMove::getDisplayInfo() {
    if (!active_)
        return "";
    switch (lastKind_) {
        case ScreenKind::Container:
            return "Container";
        case ScreenKind::TextEntry:
            return "Text";
        case ScreenKind::Other:
            return "Screen";
        default:
            return "";
    }
}

void InventoryMove::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled)
                    setEnabled(enabledValue);
            }
            ImGui::Spacing();
            ui::Checkbox("Movement", &settings.movement);
            ui::Checkbox("Jump", &settings.jump);
            ui::Checkbox("Sneak", &settings.sneak);
            ui::Checkbox("Sprint", &settings.sprint);
        },
        "Screens",
        [this]() {
            ui::Checkbox("Containers", &settings.inContainers);
            ui::Checkbox("Text Screens", &settings.inTextEntry);
            ui::Checkbox("Other Screens", &settings.inOther);
        });
}

void InventoryMove::saveSettings(json::Value& node) const {
    node["movement"] = settings.movement;
    node["jump"] = settings.jump;
    node["sneak"] = settings.sneak;
    node["sprint"] = settings.sprint;
    node["inContainers"] = settings.inContainers;
    node["inTextEntry"] = settings.inTextEntry;
    node["inOther"] = settings.inOther;
}

void InventoryMove::loadSettings(const json::Value& node) {
    settings.movement = node["movement"].as_bool(settings.movement);
    settings.jump = node["jump"].as_bool(settings.jump);
    settings.sneak = node["sneak"].as_bool(settings.sneak);
    settings.sprint = node["sprint"].as_bool(settings.sprint);
    settings.inContainers = node["inContainers"].as_bool(settings.inContainers);
    settings.inTextEntry = node["inTextEntry"].as_bool(settings.inTextEntry);
    settings.inOther = node["inOther"].as_bool(settings.inOther);
}
