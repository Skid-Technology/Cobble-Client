#include "sprint.h"
#include "../utility/autorefill.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cmath>

Sprint* sprint = nullptr;

Sprint::Sprint() : Module("Sprint") {
    keybind = 0;
}

void Sprint::onEnable() {
    
    if (mc.isValid()) {
        sprintKeyCode = mc.getSprintKey();
    }
}

void Sprint::onDisable() {
    
    if (mc.isValid() && sprintKeyCode != 0) {
        mc.setKeyBindState(sprintKeyCode, false);
    }
}

void Sprint::onUpdate() {
    if (!enabled)
        return;
    if (Menu::isOpen)
        return;
    if (mc.isInGui())
        return;
    if (!player.isValid())
        return;

    if (sprintKeyCode == 0) {
        sprintKeyCode = mc.getSprintKey();
    }

    
    
    if (autoRefill && autoRefill->isPacketRefilling()) {
        mc.setKeyBindState(sprintKeyCode, false);
        return;
    }

    
    mc.setKeyBindState(sprintKeyCode, true);
}

void Sprint::onRenderMenu() {
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
        [this]() { ImGui::TextWrapped("Maintains continuous sprint during movement."); });
}
