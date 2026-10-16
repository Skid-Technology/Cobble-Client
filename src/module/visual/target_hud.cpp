#include "target_hud.h"

#include <cmath>
#include <cstdio>

#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/health_predictor.hpp"

TargetHUD* targetHud = nullptr;

TargetHUD::TargetHUD() : Module("TargetHUD") {
    renders = true;
    usesPlayerCache = true;
    keybind = 0;
}

static ImU32 targetHpColor(float pct) {
    pct = pct < 0 ? 0 : (pct > 1 ? 1 : pct);
    int r, g;
    if (pct > 0.5f) {
        r = (int)(255 * (1.0f - (pct - 0.5f) * 2.0f));
        g = 220;
    } else {
        r = 235;
        g = (int)(220 * (pct * 2.0f));
    }
    return IM_COL32(r, g, 60, 255);
}

void TargetHUD::onRender() {
    if (!enabled || !player.isValid())
        return;

    JNIEnv* env = Java::Env();
    auto players = mc.getPlayerList();

    const float d2r = 3.14159265f / 180.0f;
    const float yaw = player.getYaw() * d2r;
    const float pitch = player.getPitch() * d2r;
    const double ex = player.getPosX();
    const double ey = player.getPosY() + 1.62;
    const double ez = player.getPosZ();
    const double lx = -std::sin(yaw) * std::cos(pitch);
    const double ly = -std::sin(pitch);
    const double lz = std::cos(yaw) * std::cos(pitch);

    int bestIdx = -1;
    double bestAngle = 1e9, bestDist = 0.0;
    for (size_t i = 0; i < players.size(); ++i) {
        const auto& p = players[i];
        const double dx = p.posX - ex, dy = (p.posY + 0.9) - ey, dz = p.posZ - ez;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 0.1 || dist > 6.0)
            continue;
        const double dot = (dx * lx + dy * ly + dz * lz) / dist;
        const double ang = std::acos(dot < -1 ? -1 : (dot > 1 ? 1 : dot));
        if (ang < bestAngle && ang < 0.70) {
            bestAngle = ang;
            bestIdx = (int)i;
            bestDist = dist;
        }
    }

    PlayerInfo target;
    bool have = false;
    if (bestIdx >= 0) {
        target = players[bestIdx];
        have = true;
    }

    
    if (env) {
        for (auto& p : players)
            if (p.instance) {
                env->DeleteGlobalRef(p.instance);
                p.instance = nullptr;
            }
    }
    if (!have)
        return;

    const int weaponId = player.getHeldItemId();
    const float maxHp = target.maxHealth > 0.5f ? target.maxHealth : 20.0f;
    const float hp = std::min(HealthPredictor::get().query(target, weaponId, 0), maxHp);
    const float pct = maxHp > 0 ? hp / maxHp : 0.0f;

    const float w = 220.0f, h = 74.0f;
    const float x = RenderData::screenWidth * 0.5f - w * 0.5f;
    const float y = RenderData::screenHeight * 0.18f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(16, 17, 22, 225), 9.0f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, 22), 9.0f, 0, 1.2f);
    dl->AddRectFilled(ImVec2(x, y + 8), ImVec2(x + 3.0f, y + h - 8), targetHpColor(pct), 2.0f);

    std::string name = target.name.empty() ? std::string("Player") : target.name;
    if (name.size() > 18)
        name = name.substr(0, 18);
    dl->AddText(font, 18.0f, ImVec2(x + 14, y + 10), IM_COL32(240, 240, 245, 255), name.c_str());
    char distBuf[24];
    snprintf(distBuf, sizeof(distBuf), "%.1fm", bestDist);
    ImVec2 dsz = font->CalcTextSizeA(14.0f, FLT_MAX, 0, distBuf);
    dl->AddText(font, 14.0f, ImVec2(x + w - dsz.x - 12, y + 12), IM_COL32(160, 165, 175, 255), distBuf);

    const float bx = x + 14, by = y + 38, bw = w - 28, bh = 12.0f;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 42, 50, 255), 6.0f);
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * pct, by + bh), targetHpColor(pct), 6.0f);

    char hpBuf[32];
    snprintf(hpBuf, sizeof(hpBuf), "%.1f / %.0f", hp, maxHp);
    ImVec2 hsz = font->CalcTextSizeA(12.0f, FLT_MAX, 0, hpBuf);
    dl->AddText(font, 12.0f, ImVec2(bx + bw * 0.5f - hsz.x * 0.5f, by), IM_COL32(255, 255, 255, 235), hpBuf);
}

void TargetHUD::onRenderMenu() {
    ui::two_column(
        "General",
        [this]() {
            ui::Binder("Keybind", &keybind);
            ImGui::Spacing();
            ImGui::TextDisabled("Predicted-HP panel for your");
            ImGui::TextDisabled("current target.");
        },
        "Info",
        [this]() {
            ImGui::TextDisabled("Target = the player nearest");
            ImGui::TextDisabled("your crosshair, within reach.");
        });
}
