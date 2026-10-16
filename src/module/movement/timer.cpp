#include "timer.h"
#include "timerrange.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

TimerMod* timerModule = nullptr;

TimerMod::TimerMod() : Module("Timer") {
    keybind = 0;
}

void TimerMod::onEnable() {
    if (timerRange && timerRange->enabled)
        timerRange->setEnabled(false);
}

void TimerMod::onDisable() {
    
    mc.setTimerSpeed(1.0f);
}


static bool isHoldingWeapon() {
    int itemId = player.getHeldItemId();
    
    
    switch (itemId) {
        case 267:
        case 268:
        case 272:
        case 276:
        case 283:  
        case 258:
        case 271:
        case 275:
        case 279:
        case 286:  
            return true;
        default:
            return false;
    }
}

void TimerMod::onUpdate() {
    
    if (!enabled) {
        
        
        mc.setTimerSpeed(1.0f);
        return;
    }

    if (Menu::isOpen) {
        mc.setTimerSpeed(1.0f);
        return;
    }

    if (!player.isValid()) {
        mc.setTimerSpeed(1.0f);
        return;
    }

    
    bool shouldApply = true;

    
    if (settings.mousePressed) {
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            shouldApply = false;
        }
    }

    
    if (settings.weaponOnly) {
        if (!isHoldingWeapon()) {
            shouldApply = false;
        }
    }

    
    if (shouldApply) {
        mc.setTimerSpeed(static_cast<float>(settings.speed));
    } else {
        mc.setTimerSpeed(1.0f);
    }
}

void TimerMod::onRenderMenu() {
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

            
            double oldSpeed = settings.speed;

            
            float speedFloat = (float)settings.speed;

            
            
            double scaled = settings.speed * 1000.0;
            bool needs3Dec = std::abs(scaled - std::round(scaled / 10.0) * 10.0) > 0.5;
            const char* fmt = needs3Dec ? "%.3fx" : "%.2fx";

            
            bool changed = ui::SliderFloat("Speed", &speedFloat, 1.0f, 3.0f, fmt, true);

            if (changed) {
                
                settings.speed = (double)speedFloat;
            }

            
            if (settings.speed < 1.0)
                settings.speed = 1.0;
            if (settings.speed > 3.0)
                settings.speed = 3.0;
        },
        "Conditions",
        [this]() {
            ui::Checkbox("Mouse Pressed", &settings.mousePressed);
            ui::Checkbox("Weapon Only", &settings.weaponOnly);
        });
}

std::string TimerMod::getDisplayInfo() {
    char buf[32];
    
    double scaled = settings.speed * 1000.0;
    bool needs3Dec = std::abs(scaled - std::round(scaled / 10.0) * 10.0) > 0.5;
    snprintf(buf, sizeof(buf), needs3Dec ? "%.3fx" : "%.2fx", settings.speed);
    return buf;
}



void TimerMod::saveSettings(json::Value& node) const {
    node["speed"] = (float)settings.speed;
    node["mousePressed"] = settings.mousePressed;
    node["weaponOnly"] = settings.weaponOnly;
}

void TimerMod::loadSettings(const json::Value& node) {
    settings.speed = (double)node["speed"].as_float((float)settings.speed);
    settings.mousePressed = node["mousePressed"].as_bool(settings.mousePressed);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
}
