#include "bridgeassist.h"
#include <Windows.h>
#include <cmath>
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

BridgeAssist* bridgeAssist = nullptr;

void BridgeAssist::Sneak() {
    m_isEdge = true;
    if (!m_wasSneaking) {
        mc.setKeyBindState(mc.getSneakKey(), true);
        m_wasSneaking = true;
    }
}

void BridgeAssist::UnSneak() {
    m_isEdge = false;
    if (m_wasSneaking) {
        mc.setKeyBindState(mc.getSneakKey(), false);
        m_wasSneaking = false;
    }
}

void BridgeAssist::onUpdate() {
    bool isSneaking = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    if (!enabled) {
        if (m_wasSneaking)
            UnSneak();
        m_shiftInitiated = false;
        return;
    }

    if (Menu::isOpen || mc.isInGui()) {
        if (m_wasSneaking)
            UnSneak();
        m_shiftInitiated = false;
        return;
    }

    if (!player.isValid())
        return;

    
    int heldItemId = player.getHeldItemId();
    if (heldItemId <= 0 || heldItemId > 255) {
        if (m_wasSneaking)
            UnSneak();
        m_shiftInitiated = false;
        return;
    }

    
    if (settings.onlyWhenSneaking) {
        if (!m_shiftInitiated && isSneaking)
            m_shiftInitiated = true;
    }

    
    float pitch = player.getPitch();
    float pitchThreshold = settings.onlyLookingDown ? 60.0f : 45.0f;

    if (pitch < pitchThreshold) {
        if (m_wasSneaking)
            UnSneak();
        m_shiftInitiated = false;
        return;
    }

    
    if (settings.onlyWhenSneaking && !m_shiftInitiated)
        return;

    
    m_from = Minecraft::Vec3((float)player.getPosX(), (float)player.getPosY() + 1.0f, (float)player.getPosZ());

    Minecraft::Vec3 m_to = Minecraft::Vec3(m_from.x, m_from.y - 100.0f, m_from.z);
    Minecraft::Vec3 hitBlockPos = {};

    auto res = mc.rayTraceBlocks(m_from, m_to, hitBlockPos, false);

    
    float diffY = (float)player.getPosY() - hitBlockPos.y - 1.0f;

    
    static bool jumped = false;

    m_wasSneaking = m_isEdge;
    m_isEdge = false;

    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        jumped = true;

    if (jumped) {
        UnSneak();
        double motionY = player.getMotionY();
        if (motionY < -0.4 || std::abs(diffY) < 0.001f)
            jumped = false;
        return;
    }

    
    double motionY = player.getMotionY();
    if (std::abs(motionY) > 0.5) {
        UnSneak();
        return;
    }

    
    if (static_cast<int>(diffY) != 0 || res == Minecraft::RAYTRACE_BLOCKS_RESULT::NO_HIT) {
        Sneak();
    } else {
        UnSneak();
    }
}

void BridgeAssist::onRenderMenu() {
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
        "Mode",
        [this]() {
            ui::Checkbox("Only When Sneaking", &settings.onlyWhenSneaking);
            ui::Checkbox("Only When Looking Down", &settings.onlyLookingDown);
        });
}



void BridgeAssist::saveSettings(json::Value& node) const {
    node["onlyLookingDown"] = settings.onlyLookingDown;
    node["onlyWhenSneaking"] = settings.onlyWhenSneaking;
}

void BridgeAssist::loadSettings(const json::Value& node) {
    settings.onlyLookingDown = node["onlyLookingDown"].as_bool(settings.onlyLookingDown);
    settings.onlyWhenSneaking = node["onlyWhenSneaking"].as_bool(settings.onlyWhenSneaking);
}
