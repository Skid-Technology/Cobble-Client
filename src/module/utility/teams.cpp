#include "teams.h"

#include <algorithm>
#include <string>

#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"

Teams* teamsModule = nullptr;

namespace {




struct ChatColorEntry {
    const char* name;
    ImVec4 rgb;
};

constexpr ChatColorEntry kChatColors[16] = {
    {"Black", {0.00f, 0.00f, 0.00f, 1.0f}},        {"Dark Blue", {0.00f, 0.00f, 0.67f, 1.0f}},
    {"Dark Green", {0.00f, 0.67f, 0.00f, 1.0f}},   {"Dark Aqua", {0.00f, 0.67f, 0.67f, 1.0f}},
    {"Dark Red", {0.67f, 0.00f, 0.00f, 1.0f}},     {"Dark Purple", {0.67f, 0.00f, 0.67f, 1.0f}},
    {"Gold", {1.00f, 0.67f, 0.00f, 1.0f}},         {"Gray", {0.67f, 0.67f, 0.67f, 1.0f}},
    {"Dark Gray", {0.33f, 0.33f, 0.33f, 1.0f}},    {"Blue", {0.33f, 0.33f, 1.00f, 1.0f}},
    {"Green", {0.33f, 1.00f, 0.33f, 1.0f}},        {"Aqua", {0.33f, 1.00f, 1.00f, 1.0f}},
    {"Red", {1.00f, 0.33f, 0.33f, 1.0f}},          {"Light Purple", {1.00f, 0.33f, 1.00f, 1.0f}},
    {"Yellow", {1.00f, 1.00f, 0.33f, 1.0f}},       {"White", {1.00f, 1.00f, 1.00f, 1.0f}},
};

const char* const kChatColorNames[16] = {
    kChatColors[0].name,  kChatColors[1].name,  kChatColors[2].name,  kChatColors[3].name,
    kChatColors[4].name,  kChatColors[5].name,  kChatColors[6].name,  kChatColors[7].name,
    kChatColors[8].name,  kChatColors[9].name,  kChatColors[10].name, kChatColors[11].name,
    kChatColors[12].name, kChatColors[13].name, kChatColors[14].name, kChatColors[15].name,
};

int colorIndex(ChatColor color) {
    const int raw = static_cast<int>(color);
    return (raw >= 0 && raw <= 15) ? raw : static_cast<int>(ChatColor::GREEN);
}

}  

Teams::Teams() : Module("Teams") {
    
    
    
    enabled = true;
}

void Teams::pushEnabled() const {
    if (friendsManager)
        friendsManager->teams.enabled = enabled;
}

void Teams::onEnable() {
    pushEnabled();
}

void Teams::onDisable() {
    pushEnabled();
    
    
    
    if (friendsManager)
        friendsManager->clearDetected();
}

std::string Teams::getDisplayInfo() {
    if (!friendsManager)
        return "";
    const FriendsManager::TeamStats stats = friendsManager->getTeamStats();
    if (stats.lunar != 0)
        return "Lunar " + std::to_string(stats.lunar);
    if (stats.apolloNametags != 0)
        return "Tags " + std::to_string(stats.apolloNametags);
    return "";
}

void Teams::onRenderMenu() {
    if (!friendsManager) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Unavailable: friends manager missing");
        return;
    }

    FriendsManager::TeamSettings& policy = friendsManager->teams;
    bool enabledValue = enabled;

    ui::two_column(
        "Sources",
        [this, &policy, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled) {
                setEnabled(enabledValue);
            }
            ImGui::Spacing();
            ui::Checkbox("Lunar API", &policy.lunarApiEnabled);
            ui::Checkbox("Apollo Nametags", &policy.apolloNametags);
            ui::Checkbox("Scoreboard", &policy.scoreboard);
            ui::Checkbox("Own Nametag Color", &policy.matchOwnNametag);
            ui::Checkbox("Manual Color", &policy.manualColorEnabled);
            if (policy.manualColorEnabled) {
                int index = colorIndex(policy.manualColor);
                if (ui::Combo("Team Color", &index, kChatColorNames, 16))
                    policy.manualColor = static_cast<ChatColor>(index);
                
                
                ImGui::TextColored(kChatColors[colorIndex(policy.manualColor)].rgb, "%s",
                                   kChatColors[colorIndex(policy.manualColor)].name);
            }
        },
        "Confirmation",
        [&policy]() {
            ui::SliderInt("Confirm Hits", &policy.confirmHits, 1, 8);
            ui::SliderInt("Confirm Window (ms)", &policy.confirmWindowMs, 250, 5000);
            ui::SliderInt("Grace Time (ms)", &policy.graceMs, 0, 20000);
            ui::Checkbox("Require Team Tag", &policy.requireTeamTag);
            ui::Checkbox("Lunar Overrides Tags", &policy.lunarExclusive);
        });

    ui::full_width("Scope", [&policy]() {
        ui::Checkbox("Vanilla Teams", &policy.vanillaTeamFlag);
        if (friendsManager) {
            ui::CheckboxBind("Quick Add", &friendsManager->quickAddEnabled, &friendsManager->quickAddKey);
        }
        ImGui::Spacing();
        if (ImGui::Button("Clear Detected", ImVec2(ImGui::CalcItemWidth(), ui::s(24)))) {
            friendsManager->clearDetected();
        }
        ui::DescribeLastItem("Clear Detected");
    });

    ImGui::Spacing();

    const FriendsManager::TeamStats stats = friendsManager->getTeamStats();
    if (!enabled) {
        ImGui::TextDisabled("Off - every player counts as an enemy");
    } else if (stats.lunar != 0) {
        ImGui::TextColored(ImVec4(0.42f, 0.92f, 1.0f, 1.0f), "Lunar API: %d teammate(s)", (int)stats.lunar);
    } else if (stats.apolloNametags != 0) {
        ImGui::TextColored(ImVec4(0.42f, 0.92f, 1.0f, 1.0f), "Nametags: %d teammate(s) confirmed",
                           (int)stats.apolloNametags);
    } else if (stats.scoreboardTeams != 0) {
        ImGui::TextDisabled("Scoreboard: %d team(s) tracked", (int)stats.scoreboardTeams);
    } else {
        ImGui::TextDisabled("No teammates detected yet");
    }
    ImGui::TextDisabled("%d manual friend(s) - add them in Settings", (int)stats.friends);
}

void Teams::saveSettings(json::Value& node) const {
    if (!friendsManager)
        return;
    const FriendsManager::TeamSettings& policy = friendsManager->teams;
    node["lunarApi"] = policy.lunarApiEnabled;
    node["apolloNametags"] = policy.apolloNametags;
    node["scoreboard"] = policy.scoreboard;
    node["matchOwnNametag"] = policy.matchOwnNametag;
    node["manualColorEnabled"] = policy.manualColorEnabled;
    node["manualColor"] = static_cast<int>(policy.manualColor);
    node["confirmHits"] = policy.confirmHits;
    node["graceMs"] = policy.graceMs;
    node["confirmWindowMs"] = policy.confirmWindowMs;
    node["lunarExclusive"] = policy.lunarExclusive;
    node["requireTeamTag"] = policy.requireTeamTag;
    node["vanillaTeamFlag"] = policy.vanillaTeamFlag;
}

void Teams::loadSettings(const json::Value& node) {
    if (!friendsManager)
        return;
    FriendsManager::TeamSettings& policy = friendsManager->teams;
    policy.lunarApiEnabled = node["lunarApi"].as_bool(policy.lunarApiEnabled);
    policy.apolloNametags = node["apolloNametags"].as_bool(policy.apolloNametags);
    policy.scoreboard = node["scoreboard"].as_bool(policy.scoreboard);
    policy.matchOwnNametag = node["matchOwnNametag"].as_bool(policy.matchOwnNametag);
    policy.manualColorEnabled = node["manualColorEnabled"].as_bool(policy.manualColorEnabled);
    policy.manualColor =
        static_cast<ChatColor>(std::clamp(node["manualColor"].as_int(colorIndex(policy.manualColor)), 0, 15));
    policy.confirmHits = std::clamp(node["confirmHits"].as_int(policy.confirmHits), 1, 8);
    policy.graceMs = std::clamp(node["graceMs"].as_int(policy.graceMs), 0, 20000);
    policy.confirmWindowMs = std::clamp(node["confirmWindowMs"].as_int(policy.confirmWindowMs), 250, 5000);
    policy.lunarExclusive = node["lunarExclusive"].as_bool(policy.lunarExclusive);
    policy.requireTeamTag = node["requireTeamTag"].as_bool(policy.requireTeamTag);
    policy.vanillaTeamFlag = node["vanillaTeamFlag"].as_bool(policy.vanillaTeamFlag);

    
    
    pushEnabled();
}
