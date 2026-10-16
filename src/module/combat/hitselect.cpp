#include "hitselect.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

HitSelect* hitSelect = nullptr;


namespace HitSelectItems {
constexpr int WOOD_SWORD = 268;
constexpr int STONE_SWORD = 272;
constexpr int IRON_SWORD = 267;
constexpr int DIAMOND_SWORD = 276;
constexpr int GOLD_SWORD = 283;
constexpr int WOOD_AXE = 271;
constexpr int STONE_AXE = 275;
constexpr int IRON_AXE = 258;
constexpr int DIAMOND_AXE = 279;
constexpr int GOLD_AXE = 286;
}  

HitSelect::HitSelect() : Module("HitSelect") {
    keybind = 0;
}

void HitSelect::onEnable() {
    auto now = std::chrono::steady_clock::now();
    lastHitTime = now;
    comboStartTime = now;
    lastAllowedClick = now;
    comboHitCount = 0;
    inCombo = false;
    firstHitLanded = false;
    lastTargetHurtTime = 0;
    hasTarget = false;
}

void HitSelect::onDisable() {
    inCombo = false;
    comboHitCount = 0;
}

void HitSelect::detectHit() {
    
    Minecraft mc;
    bool targeting = mc.isTargetingEntity();

    if (!targeting) {
        hasTarget = false;
        return;
    }

    hasTarget = true;
    int currentHurtTime = mc.getTargetedEntityHurtTime();

    
    if (currentHurtTime > lastTargetHurtTime && currentHurtTime >= 9) {
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastHit = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHitTime).count();

        
        if (timeSinceLastHit <= settings.comboDetectionTime) {
            comboHitCount++;
            if (!inCombo && comboHitCount >= settings.minComboHits) {
                inCombo = true;
                comboStartTime = now;
            }
        } else {
            
            comboHitCount = 1;
            inCombo = false;
            firstHitLanded = false;
        }

        lastHitTime = now;
        firstHitLanded = true;
    }

    lastTargetHurtTime = currentHurtTime;
}

void HitSelect::onUpdate() {
    if (!enabled)
        return;
    detectHit();
}

bool HitSelect::shouldAllowClick() {
    if (!enabled)
        return true;

    
    if (settings.weaponOnly && !isHoldingWeapon()) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();

    
    if (settings.waitForFirstHit && !firstHitLanded) {
        return true;
    }

    
    Minecraft mc;
    if (!mc.isTargetingEntity()) {
        return true;  
    }

    int targetHurtTime = mc.getTargetedEntityHurtTime();

    
    if (targetHurtTime <= 2) {
        lastAllowedClick = now;
        return true;
    }

    
    if (inCombo) {
        auto timeSinceLastAllowed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAllowedClick).count();

        
        if (timeSinceLastAllowed < settings.waitDuration) {
            
            return false;
        }
    }

    lastAllowedClick = now;
    return true;
}

bool HitSelect::isHoldingWeapon() {
    int id = getHeldItemId();
    if (id == -1)
        return false;

    return (id == HitSelectItems::WOOD_SWORD || id == HitSelectItems::STONE_SWORD || id == HitSelectItems::IRON_SWORD ||
            id == HitSelectItems::DIAMOND_SWORD || id == HitSelectItems::GOLD_SWORD || id == HitSelectItems::WOOD_AXE ||
            id == HitSelectItems::STONE_AXE || id == HitSelectItems::IRON_AXE || id == HitSelectItems::DIAMOND_AXE ||
            id == HitSelectItems::GOLD_AXE);
}

int HitSelect::getHeldItemId() {
    return player.getHeldItemId();
}

void HitSelect::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Combo",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }

            ImGui::Spacing();

            ui::SliderInt("Min Combo Hits", &settings.minComboHits, 0, 5, "%d");
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Hits needed before filtering");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ui::SliderInt("Combo Detection", &settings.comboDetectionTime, 50, 500, "%d ms");
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Time between hits for combo");
            ImGui::PopStyleColor();
        },
        "Timing",
        [this]() {
            ui::Checkbox("Wait For First Hit", &settings.waitForFirstHit);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Don't filter until first hit");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ui::SliderInt("Wait Duration", &settings.waitDuration, 100, 1000, "%d ms");
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Time between hits in combo");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ui::Checkbox("Weapon Only", &settings.weaponOnly);
        });
}

std::string HitSelect::getDisplayInfo() {
    if (inCombo) {
        return "Combo x" + std::to_string(comboHitCount);
    }
    return "";
}



void HitSelect::saveSettings(json::Value& node) const {
    node["minComboHits"] = settings.minComboHits;
    node["comboDetectionTime"] = settings.comboDetectionTime;
    node["waitForFirstHit"] = settings.waitForFirstHit;
    node["waitDuration"] = settings.waitDuration;
    node["weaponOnly"] = settings.weaponOnly;
}

void HitSelect::loadSettings(const json::Value& node) {
    settings.minComboHits = node["minComboHits"].as_int(settings.minComboHits);
    settings.comboDetectionTime = node["comboDetectionTime"].as_int(settings.comboDetectionTime);
    settings.waitForFirstHit = node["waitForFirstHit"].as_bool(settings.waitForFirstHit);
    settings.waitDuration = node["waitDuration"].as_int(settings.waitDuration);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
}
