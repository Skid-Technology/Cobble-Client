#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "dropped_info.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../../infra/ext/imgui/imgui.h"
#include "../../ui/hud/hud_drag.hpp"
#include "../../ui/menu/menu.h"
#include "../../util/network/json.hpp"

#include <vector>

DroppedInfo* droppedInfo = nullptr;

DroppedInfo::DroppedInfo() : Module("DroppedInfo") {
    keybind = 0;
    enabled = false;
    renders = true;  
}

void DroppedInfo::onEnable() {
    std::lock_guard<std::mutex> lock(mutex);
    potionCount = 0;
}

void DroppedInfo::onUpdate() {
    if (!enabled)
        return;
    if (!Java::Env() || !Java::Initialized)
        return;

    
    std::vector<ItemEntityInfo> items = mc.getItemEntityList();

    int total = 0;
    for (const auto& item : items) {
        
        if (item.itemId != 373)
            continue;

        
        
        
        int dmg = item.damage;
        if (dmg == 16421 || dmg == 16453) {
            total += item.stackSize;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        potionCount = total;
    }
}

void DroppedInfo::onRender() {
    bool open = Menu::isOpen;
    if (!enabled && !open)
        return;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!draw)
        return;

    ImFont* f = font(2); 
    if (!f) return;

    int count = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        count = potionCount;
    }

    
    if (!enabled && open) {
        count = 0;
    } else if (!enabled) {
        return;
    }

    float s = settings.scale;
    float fontSize = 20.0f * s;
    float padX = 10.0f * s;
    float padY = 6.0f * s;
    float rounding = 6.0f * s;

    ImU32 accent = IM_COL32((int)(g_SchemeColor[0] * 255.0f), 
                            (int)(g_SchemeColor[1] * 255.0f), 
                            (int)(g_SchemeColor[2] * 255.0f), 255);
    ImU32 gray = IM_COL32(150, 155, 165, 255);
    ImU32 shadow = IM_COL32(0, 0, 0, 200);

    const char* labelStr = "Dropped Pots: ";
    char numStr[16];
    snprintf(numStr, sizeof(numStr), "%d", count);

    ImVec2 labelSz = f->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, labelStr);
    ImVec2 numSz = f->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, numStr);

    float boxW = padX + labelSz.x + numSz.x + padX;
    float boxH = padY + labelSz.y + padY;

    
    hud::dragBox(settings.x, settings.y, boxW, boxH, 2.0f, dragging, dragOffsetX, dragOffsetY);

    float x1 = settings.x;
    float y1 = settings.y;
    float x2 = x1 + boxW;
    float y2 = y1 + boxH;

    if (settings.background) {
        draw->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(19, 19, 19, 210), rounding);
    }

    float tx = x1 + padX;
    float ty = y1 + padY;
    float sd = 1.0f * s;

    
    draw->AddText(f, fontSize, ImVec2(tx + sd, ty + sd), shadow, labelStr);
    draw->AddText(f, fontSize, ImVec2(tx + labelSz.x + sd, ty + sd), shadow, numStr);

    
    draw->AddText(f, fontSize, ImVec2(tx, ty), gray, labelStr);
    draw->AddText(f, fontSize, ImVec2(tx + labelSz.x, ty), accent, numStr);
}

void DroppedInfo::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Settings",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();
            ui::SliderFloat("Scale", &settings.scale, 0.5f, 3.0f, "%.1f");
            ui::SliderFloat("X", &settings.x, 0.0f, 1000.0f, "%.0f");
            ui::SliderFloat("Y", &settings.y, 0.0f, 1000.0f, "%.0f");
            ImGui::Spacing();
            const bool resetPosition = ImGui::Button("Reset Position");
            ui::DescribeLastItem("Reset Position", "Restores the overlay position and scale to their defaults.");
            if (resetPosition) {
                settings.x = 10.0f;
                settings.y = 10.0f;
                settings.scale = 1.0f;
            }
        },
        "Display",
        [this]() { ui::Checkbox("Background", &settings.background); });
}



void DroppedInfo::saveSettings(json::Value& node) const {
    node["x"] = settings.x;
    node["y"] = settings.y;
    node["scale"] = settings.scale;
    node["background"] = settings.background;
}

void DroppedInfo::loadSettings(const json::Value& node) {
    settings.x = node["x"].as_float(settings.x);
    settings.y = node["y"].as_float(settings.y);
    settings.scale = node["scale"].as_float(settings.scale);
    settings.background = node["background"].as_bool(settings.background);
}
