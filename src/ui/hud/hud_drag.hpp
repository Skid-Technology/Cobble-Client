#pragma once














#include "../../../infra/ext/imgui/imgui.h"
#include "../menu/menu.h"

namespace hud {







inline void dragBox(float& x, float& y, float w, float h, float pad, bool& dragging, float& offX,
                     float& offY) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!draw)
        return;

    const ImVec2 m = ImGui::GetMousePos();
    auto outline = [&]() {
        draw->AddRect(ImVec2(x - pad, y - pad), ImVec2(x + w + pad, y + h + pad),
                      IM_COL32(255, 255, 255, 255));
    };

    if (dragging) {
        outline();
        if (ImGui::IsMouseDown(0)) {
            x = m.x - offX;
            y = m.y - offY;
        } else {
            dragging = false;
            Menu::hidden = false;  
        }
        return;
    }

    
    if (!Menu::isOpen)
        return;

    const bool hover = m.x >= x - pad && m.x <= x + w + pad && m.y >= y - pad && m.y <= y + h + pad;
    if (!hover)
        return;

    outline();
    if (ImGui::IsMouseClicked(0)) {
        dragging = true;
        offX = m.x - x;
        offY = m.y - y;
        Menu::hidden = true;  
    }
}

}  
