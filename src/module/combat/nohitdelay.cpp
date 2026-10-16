#include "nohitdelay.h"
#include "../../sdk/core/minecraft.h"
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

NoHitDelay* noHitDelay = nullptr;

NoHitDelay::NoHitDelay() : Module("NoHitDelay") {
    keybind = 0;
}

void NoHitDelay::onEnable() {
    
}

void NoHitDelay::onDisable() {
    
}

void NoHitDelay::onUpdate() {
    if (!enabled)
        return;
    if (Menu::isOpen)
        return;
    if (mc.isInGui())
        return;
    if (!player.isValid())
        return;

    
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        return;

    
    if (settings.weaponOnly) {
        int itemId = player.getHeldItemId();
        
        
        bool isWeapon = (itemId == 268 || itemId == 272 || itemId == 267 || itemId == 276 || itemId == 283 ||
                         itemId == 271 || itemId == 275 || itemId == 258 || itemId == 279 || itemId == 286);
        if (!isWeapon)
            return;
    }

    
    
    mc.setLeftClickCounter(0);
}

void NoHitDelay::onRenderMenu() {
    bool enabledValue = enabled;
    ui::section("Settings", [this, &enabledValue]() {
        if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
            if (enabledValue != enabled) {
                setEnabled(enabledValue);
            }
        }
        ImGui::Spacing();
        ui::Checkbox("Weapon Only", &settings.weaponOnly);
    });
}

void NoHitDelay::saveSettings(json::Value& node) const {
    node["weaponOnly"] = settings.weaponOnly;
}

void NoHitDelay::loadSettings(const json::Value& node) {
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
}
