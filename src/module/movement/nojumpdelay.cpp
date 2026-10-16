#include "nojumpdelay.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

NoJumpDelay* noJumpDelay = nullptr;

NoJumpDelay::NoJumpDelay() : Module("NoJumpDelay") {
    keybind = 0;
}

void NoJumpDelay::onEnable() {}

void NoJumpDelay::onDisable() {}

void NoJumpDelay::onUpdate() {
    if (!enabled)
        return;
    if (Menu::isOpen)
        return;
    if (mc.isInGui())
        return;
    if (!player.isValid())
        return;

    
    
    if (player.isOnGround() && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
        player.setJumpTicks(0);
    }
}

void NoJumpDelay::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
        },
        "Info",
        [this]() { ImGui::TextWrapped("Removes jump delay."); });
}
