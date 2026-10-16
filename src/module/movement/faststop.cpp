#include "faststop.h"
#include "../combat/criticals.h"
#include "../combat/wtap.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#include <cmath>

FastStop* fastStop = nullptr;

FastStop::FastStop() : Module("FastStop") {
    keybind = 0;
}

void FastStop::onUpdate() {
    if (!enabled) {
        wasMoving = false;
        return;
    }
    if (Menu::isOpen || mc.isInGui() || !player.isValid()) {
        wasMoving = false;
        return;
    }

    
    if (!player.isOnGround()) {
        return;
    }

    
    if (criticals && criticals->isRecentlyCritical(400)) {
        wasMoving = false;
        return;
    }

    
    if (wTap && wTap->isTapping()) {
        wasMoving = false;
        return;
    }

    const bool moving = mc.isMoving();

    if (moving) {
        wasMoving = true;
        return;
    }

    
    if (!wasMoving)
        return;

    if (settings.onlyOnGround && !player.isOnGround())
        return;

    const double motionX = player.getMotionX();
    const double motionZ = player.getMotionZ();

    if (motionX == 0 && motionZ == 0) {
        wasMoving = false;
        return;
    }

    if (settings.stopStrength >= 0.99f) {
        player.setMotionX(0);
        player.setMotionZ(0);
        wasMoving = false;
        return;
    }

    const double factor = 1.0 - settings.stopStrength;
    double newX = motionX * factor;
    double newZ = motionZ * factor;
    if (std::abs(newX) < 0.001)
        newX = 0;
    if (std::abs(newZ) < 0.001)
        newZ = 0;
    player.setMotionX(newX);
    player.setMotionZ(newZ);

    if (newX == 0 && newZ == 0) {
        wasMoving = false;
    }
}

void FastStop::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();

            float strengthPct = settings.stopStrength * 100.0f;
            if (ui::SliderFloat("Stop Strength", &strengthPct, 10.0f, 100.0f, "%.0f%%")) {
                settings.stopStrength = strengthPct / 100.0f;
            }
            if (settings.stopStrength < 0.1f)
                settings.stopStrength = 0.1f;
            if (settings.stopStrength > 1.0f)
                settings.stopStrength = 1.0f;

            ui::Checkbox("Only On Ground", &settings.onlyOnGround);
        },
        "Info",
        [this]() {
            ImGui::TextWrapped(
                "Kills leftover momentum the moment you release the movement keys. "
                "100%% stops instantly; lower values bleed the slide off over a few ticks.");
        });
}

std::string FastStop::getDisplayInfo() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%%", settings.stopStrength * 100.0f);
    return buf;
}

void FastStop::saveSettings(json::Value& node) const {
    node["stopStrength"] = settings.stopStrength;
    node["onlyOnGround"] = settings.onlyOnGround;
}

void FastStop::loadSettings(const json::Value& node) {
    settings.stopStrength = node["stopStrength"].as_float(settings.stopStrength);
    if (settings.stopStrength < 0.1f)
        settings.stopStrength = 0.1f;
    if (settings.stopStrength > 1.0f)
        settings.stopStrength = 1.0f;
    settings.onlyOnGround = node["onlyOnGround"].as_bool(settings.onlyOnGround);
}
