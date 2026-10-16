#pragma once























#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/player/render_data.h"

namespace hud {






inline bool screenSize(float& w, float& h) {
    const ImGuiIO& io = ImGui::GetIO();
    w = io.DisplaySize.x;
    h = io.DisplaySize.y;
    if (w < 16.0f || h < 16.0f) {
        w = static_cast<float>(RenderData::screenWidth);
        h = static_cast<float>(RenderData::screenHeight);
    }
    return w >= 16.0f && h >= 16.0f;
}




inline void placeToPixels(bool right, bool bottom, float relX, float relY, float w, float h, float sw,
                          float sh, float& x, float& y) {
    x = relX * sw - (right ? w : 0.0f);
    y = relY * sh - (bottom ? h : 0.0f);
}



inline void placeToFractions(bool right, bool bottom, float x, float y, float w, float h, float sw,
                             float sh, float& relX, float& relY) {
    if (sw > 0.0f)
        relX = (x + (right ? w : 0.0f)) / sw;
    if (sh > 0.0f)
        relY = (y + (bottom ? h : 0.0f)) / sh;
}

}  
