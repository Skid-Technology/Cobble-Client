#include "antibot.h"

#include <chrono>
#include <cstdio>

#include "../../sdk/core/minecraft.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

AntiBot* antiBot = nullptr;

namespace {

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}


bool legalUsername(const std::string& s) {
    if (s.size() < 3 || s.size() > 16)
        return false;
    for (unsigned char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok)
            return false;
    }
    return true;
}




bool npcShapedUuid(const std::string& uuid) {
    if (uuid.size() != 36)
        return false;
    if (uuid[14] == '2')
        return true;
    
    for (char c : uuid) {
        if (c != '0' && c != '-')
            return false;
    }
    return true;
}

}  

AntiBot::AntiBot() : Module("AntiBot") {
    
    
    
    enabled = true;
}

bool AntiBot::Filtered(const PlayerInfo& p) {
    AntiBot* self = antiBot;
    if (!self || !self->enabled)
        return false;
    return self->evaluate(p);
}

bool AntiBot::evaluate(const PlayerInfo& p) const {
    
    
    if (settings.nameTag && p.isNPC) {
        hitsNameTag_.fetch_add(1, std::memory_order_relaxed);
        lastHitMs_.store(nowMs(), std::memory_order_relaxed);
        return true;
    }
    if (settings.noHealth && p.maxHealth <= 0.0f) {
        hitsNoHealth_.fetch_add(1, std::memory_order_relaxed);
        lastHitMs_.store(nowMs(), std::memory_order_relaxed);
        return true;
    }
    if (settings.invisible && p.isInvisible) {
        hitsInvisible_.fetch_add(1, std::memory_order_relaxed);
        lastHitMs_.store(nowMs(), std::memory_order_relaxed);
        return true;
    }
    if (settings.npcUuid && npcShapedUuid(p.uuid)) {
        hitsNpcUuid_.fetch_add(1, std::memory_order_relaxed);
        lastHitMs_.store(nowMs(), std::memory_order_relaxed);
        return true;
    }
    
    
    if (settings.fakeProfile && !p.profileName.empty() && !legalUsername(p.profileName)) {
        hitsFakeProfile_.fetch_add(1, std::memory_order_relaxed);
        lastHitMs_.store(nowMs(), std::memory_order_relaxed);
        return true;
    }
    return false;
}

AntiBot::Stats AntiBot::stats() const {
    Stats s;
    s.nameTag = hitsNameTag_.load(std::memory_order_relaxed);
    s.npcUuid = hitsNpcUuid_.load(std::memory_order_relaxed);
    s.fakeProfile = hitsFakeProfile_.load(std::memory_order_relaxed);
    s.noHealth = hitsNoHealth_.load(std::memory_order_relaxed);
    s.invisible = hitsInvisible_.load(std::memory_order_relaxed);
    return s;
}

void AntiBot::resetStats() {
    hitsNameTag_.store(0, std::memory_order_relaxed);
    hitsNpcUuid_.store(0, std::memory_order_relaxed);
    hitsFakeProfile_.store(0, std::memory_order_relaxed);
    hitsNoHealth_.store(0, std::memory_order_relaxed);
    hitsInvisible_.store(0, std::memory_order_relaxed);
    lastHitMs_.store(0, std::memory_order_relaxed);
}

void AntiBot::onWorldChange() {
    
    
    resetStats();
}

std::string AntiBot::getDisplayInfo() {
    const bool anyCheck = settings.nameTag || settings.npcUuid || settings.fakeProfile || settings.noHealth ||
                          settings.invisible;
    if (!anyCheck)
        return "Off";
    const std::uint64_t last = lastHitMs_.load(std::memory_order_relaxed);
    if (last != 0 && nowMs() - last < 1500)
        return "Filtering";
    return "";
}

void AntiBot::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Checks",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled)
                    setEnabled(enabledValue);
            }
            ImGui::Spacing();
            ui::Checkbox("Name Tag", &settings.nameTag);
            ui::Checkbox("NPC UUID", &settings.npcUuid);
            ui::Checkbox("Fake Profile", &settings.fakeProfile);
            ui::Checkbox("No Health", &settings.noHealth);
            ui::Checkbox("Invisible", &settings.invisible);
        },
        "Detections",
        [this]() {
            const Stats s = stats();
            const std::uint32_t total = s.nameTag + s.npcUuid + s.fakeProfile + s.noHealth + s.invisible;

            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Every combat module asks AntiBot before it picks a target. Turn this off and "
                               "they will aim at NPCs.");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            char line[96];
            std::snprintf(line, sizeof(line), "Rejected: %u", total);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetAccentRGB()), "%s", line);

            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            std::snprintf(line, sizeof(line), "Name Tag  %u", s.nameTag);
            ImGui::TextUnformatted(line);
            std::snprintf(line, sizeof(line), "NPC UUID  %u", s.npcUuid);
            ImGui::TextUnformatted(line);
            std::snprintf(line, sizeof(line), "Fake Profile  %u", s.fakeProfile);
            ImGui::TextUnformatted(line);
            std::snprintf(line, sizeof(line), "No Health  %u", s.noHealth);
            ImGui::TextUnformatted(line);
            std::snprintf(line, sizeof(line), "Invisible  %u", s.invisible);
            ImGui::TextUnformatted(line);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            if (ImGui::Button("Reset Counters", ImVec2(-1, ui::s(24.0f))))
                resetStats();
            ui::DescribeLastItem("Reset Counters");
        });
}

void AntiBot::saveSettings(json::Value& node) const {
    node["nameTag"] = settings.nameTag;
    node["npcUuid"] = settings.npcUuid;
    node["fakeProfile"] = settings.fakeProfile;
    node["noHealth"] = settings.noHealth;
    node["invisible"] = settings.invisible;
}

void AntiBot::loadSettings(const json::Value& node) {
    settings.nameTag = node["nameTag"].as_bool(settings.nameTag);
    settings.npcUuid = node["npcUuid"].as_bool(settings.npcUuid);
    settings.fakeProfile = node["fakeProfile"].as_bool(settings.fakeProfile);
    settings.noHealth = node["noHealth"].as_bool(settings.noHealth);
    settings.invisible = node["invisible"].as_bool(settings.invisible);
}
