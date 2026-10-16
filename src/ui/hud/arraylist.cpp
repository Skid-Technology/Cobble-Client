#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "arraylist.h"
#include "../../../infra/ext/imgui/imgui.h"
#include "../../module/module_manager.h"
#include "../../module/utility/autorefill.h"
#include "../../module/utility/armorswitch.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../core/hud_fonts.h"
#include "../core/overlay.h"
#include "../menu/menu.h"
#include "../widgets/widgets.hpp"
#include "hud_color.hpp"
#include "hud_drag.hpp"
#include "hud_place.hpp"
#include "watermark.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>







namespace {

struct Row {
    std::string name;
    std::string info;
    float prog = 0.0f;   
    bool alive = false;  
    
    float nameW = 0.0f;
    float infoW = 0.0f;
    float itemW = 0.0f;
};

std::vector<Row> g_rows;

void markLive(const std::string& name, const std::string& info) {
    for (auto& r : g_rows) {
        if (r.name == name) {
            r.alive = true;
            r.info = info;
            return;
        }
    }
    Row row;
    row.name = name;
    row.info = info;
    row.alive = true;
    g_rows.push_back(std::move(row));
}

inline float easeOutCubic(float t) {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

}  

void ArrayListRenderer::render() {
    
    
    auto cancelMove = []() {
        if (dragging) {
            dragging = false;
            Menu::hidden = false;
        }
    };

    if (!enabled) {
        g_rows.clear();
        cancelMove();
        return;
    }
    
    
    if (!player.isValid()) {
        g_rows.clear();
        cancelMove();
        return;  
    }

    
    for (auto& r : g_rows)
        r.alive = false;

    for (auto* mod : moduleManager->getModules()) {
        if (mod && mod->enabled)
            markLive(mod->name, showInfo ? mod->getDisplayInfo() : std::string());
    }

    
    
    if (autoRefill && !autoRefill->enabled && autoRefill->settings.onInventoryOpen)
        markLive(autoRefill->name, showInfo ? autoRefill->getDisplayInfo() : std::string());
    if (armorSwitch && !armorSwitch->enabled && armorSwitch->settings.onInventoryOpen)
        markLive(armorSwitch->name, showInfo ? armorSwitch->getDisplayInfo() : std::string());

    
    float dtA = ImGui::GetIO().DeltaTime;
    if (dtA <= 0.0f || dtA > 0.25f)
        dtA = 1.0f / 60.0f;  
    const float step = dtA * 7.0f;  
    for (auto& r : g_rows)
        r.prog = (std::clamp)(r.prog + (r.alive ? step : -step), 0.0f, 1.0f);
    g_rows.erase(std::remove_if(g_rows.begin(), g_rows.end(),
                                [](const Row& r) { return !r.alive && r.prog <= 0.0f; }),
                 g_rows.end());

    if (g_rows.empty()) {
        cancelMove();
        return;
    }

    
    
    ImFont* arrayFont = hud_fonts::GetById(fontStyle);
    if (!arrayFont)
        arrayFont = font(5);
    if (!arrayFont)
        arrayFont = font(2);
    if (!arrayFont || arrayFont->FontSize <= 0.0f) {
        cancelMove();
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!draw) {
        cancelMove();
        return;
    }

    
    const float fontSize = 24.0f * scale;
    const float infoFS = fontSize;  
    
    
    const float rowH = std::floor(fontSize * 1.18f);
    const float padIn = 7.0f * scale;   
    const float barWidth = 3.0f * scale;
    const float barGap = 6.0f * scale;  
    
    
    const float infoGap = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, " ").x;
    const float rounding = 4.0f * scale;

    
    
    
    const float infoDrop = 0.0f;

    for (auto& r : g_rows) {
        r.nameW = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, r.name.c_str()).x;
        r.infoW = r.info.empty() ? 0.0f : arrayFont->CalcTextSizeA(infoFS, FLT_MAX, 0.0f, r.info.c_str()).x;
        const float textW = r.nameW + (r.info.empty() ? 0.0f : infoGap + r.infoW);
        r.itemW = padIn + textW + (colorBar ? barGap + barWidth : padIn);
    }

    
    if (sortByLength)
        std::stable_sort(g_rows.begin(), g_rows.end(),
                         [](const Row& a, const Row& b) { return a.itemW > b.itemW; });

    
    
    
    
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    if (!hud::screenSize(screenWidth, screenHeight)) {
        cancelMove();
        return;
    }

    
    
    
    bool anchorLeft = (anchor == 1 || anchor == 3);    
    bool anchorBottom = (anchor == 2 || anchor == 3);  

    
    
    float blockWidth = 0.0f;
    float blockHeight = 0.0f;
    for (const auto& r : g_rows) {
        const float e = easeOutCubic(r.prog);
        blockWidth = (std::max)(blockWidth, r.itemW * e);
        blockHeight += rowH * e;
    }

    
    
    
    
    
    
    
    
    const float edgeMargin = std::floor(4.0f * scale);

    float blockX = 0.0f;
    float blockY = 0.0f;

    if (userPlaced) {
        hud::placeToPixels(!anchorLeft, anchorBottom, relX, relY, blockWidth, blockHeight, screenWidth,
                           screenHeight, blockX, blockY);
    } else {
        blockX = anchorLeft ? edgeMargin : (screenWidth - blockWidth - edgeMargin);
        blockY = anchorBottom ? (screenHeight - blockHeight - yOffset) : yOffset;

        
        
        
        
        if (!anchorBottom && Watermark::enabled && Watermark::lastW > 0.0f) {
            const float wx1 = Watermark::lastX;
            const float wx2 = wx1 + Watermark::lastW;
            const float wy2 = Watermark::lastY + Watermark::lastH;
            const bool xOverlap = (blockX < wx2) && (blockX + blockWidth > wx1);
            if (xOverlap && blockY < wy2)
                blockY = wy2 + edgeMargin;
        }
    }

    
    
    hud::dragBox(blockX, blockY, blockWidth, blockHeight, 2.0f, dragging, dragOffsetX, dragOffsetY);

    
    
    if (dragging)
        userPlaced = true;

    
    blockX = (std::clamp)(blockX, 0.0f, (std::max)(0.0f, screenWidth - blockWidth));
    blockY = (std::clamp)(blockY, 0.0f, (std::max)(0.0f, screenHeight - blockHeight));

    
    
    if (userPlaced)
        hud::placeToFractions(!anchorLeft, anchorBottom, blockX, blockY, blockWidth, blockHeight, screenWidth,
                              screenHeight, relX, relY);

    float y = blockY;

    const ImU32 infoColor = IM_COL32(158, 163, 173, 255);

    long long rainbowTimeMs = 0;
    if (rainbow) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        rainbowTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    const ImU32 flatColor =
        useAccent ? GetAccentRGB()
                  : IM_COL32((int)(color[0] * 255.0f), (int)(color[1] * 255.0f), (int)(color[2] * 255.0f), 255);

    const int rowCount = static_cast<int>(g_rows.size());
    for (int idx = 0; idx < rowCount; ++idx) {
        const Row& r = g_rows[static_cast<size_t>(idx)];
        const float e = easeOutCubic(r.prog);
        const float rh = rowH * e;

        const ImU32 moduleColor =
            rainbow ? hud::GetRainbowColorEx(idx, rainbowSaturation, rainbowTimeMs, rainbowType) : flatColor;

        
        float rectX1, rectX2, textX, barX1, barX2;
        if (anchorLeft) {
            rectX1 = blockX;
            rectX2 = blockX + r.itemW;
            textX = rectX1 + (colorBar ? barWidth + barGap : padIn);
            barX1 = blockX;
            barX2 = blockX + barWidth;
        } else {
            const float rightEdge = blockX + blockWidth;
            rectX1 = rightEdge - r.itemW;
            rectX2 = rightEdge;
            textX = rectX1 + padIn;
            barX1 = rightEdge - barWidth;
            barX2 = rightEdge;
        }

        
        float slide = (1.0f - e) * 14.0f * scale;
        if (anchorLeft)
            slide = -slide;
        rectX1 += slide;
        rectX2 += slide;
        textX += slide;
        barX1 += slide;
        barX2 += slide;

        
        
        
        
        
        const bool isFirst = (idx == 0);
        const bool isLast = (idx == rowCount - 1);
        ImDrawFlags cornerFlags;
        if (rowCount == 1)
            cornerFlags = ImDrawFlags_RoundCornersAll;
        else if (isFirst)
            cornerFlags = ImDrawFlags_RoundCornersTop;
        else if (isLast)
            cornerFlags = ImDrawFlags_RoundCornersBottom;
        else
            cornerFlags = ImDrawFlags_RoundCornersNone;

        
        if (showBackground) {
            draw->AddRectFilled(ImVec2(rectX1, y), ImVec2(rectX2, y + rh),
                                hud::ColScaleAlpha(IM_COL32(19, 19, 19, 150), e), rounding, cornerFlags);
        }

        
        const float nameY = y + (rh - fontSize) * 0.5f;
        const float infoY = nameY + infoDrop;
        const float infoX = textX + r.nameW + infoGap;

        
        
        
        if (!showBackground) {
            const ImU32 sh = hud::ColScaleAlpha(IM_COL32(0, 0, 0, 140), e);
            const float sd = (std::max)(1.0f, std::floor(scale));
            const float tap[4][2] = {{-sd, 0.0f}, {sd, 0.0f}, {0.0f, -sd}, {0.0f, sd}};
            for (const auto& t : tap) {
                draw->AddText(arrayFont, fontSize, ImVec2(textX + t[0], nameY + t[1]), sh, r.name.c_str());
                if (!r.info.empty())
                    draw->AddText(arrayFont, infoFS, ImVec2(infoX + t[0], infoY + t[1]), sh, r.info.c_str());
            }
        }

        draw->AddText(arrayFont, fontSize, ImVec2(textX, nameY), hud::ColScaleAlpha(moduleColor, e), r.name.c_str());
        if (!r.info.empty()) {
            draw->AddText(arrayFont, infoFS, ImVec2(infoX, infoY), hud::ColScaleAlpha(infoColor, e), r.info.c_str());
        }

        
        
        
        
        if (colorBar) {
            const ImU32 mc = hud::ColScaleAlpha(moduleColor, e);
            ImDrawFlags barFlags;
            if (rowCount == 1)
                barFlags = anchorLeft ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
            else if (isFirst)
                barFlags = anchorLeft ? ImDrawFlags_RoundCornersTopLeft : ImDrawFlags_RoundCornersTopRight;
            else if (isLast)
                barFlags = anchorLeft ? ImDrawFlags_RoundCornersBottomLeft : ImDrawFlags_RoundCornersBottomRight;
            else
                barFlags = ImDrawFlags_RoundCornersNone;
            const float barRound = (barFlags == ImDrawFlags_RoundCornersNone) ? 0.0f : rounding;
            draw->AddRectFilled(ImVec2(barX1, y), ImVec2(barX2, y + rh), mc, barRound, barFlags);
        }

        y += rh;
    }
}

bool ArrayListRenderer::hasActive() {
    if (!enabled)
        return false;
    if (Menu::isOpen)
        return false;
    if (!player.isValid())
        return false;
    return true;
}
