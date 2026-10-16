#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "menu.h"
#include "../../cobble.hpp"
#include "../../config/config.hpp"
#include "../../spectate/spectate.hpp"
#include "../../module/combat/aimassist.h"
#include "../../module/combat/aimbow.h"
#include "../../module/combat/antibot.h"
#include "../../module/combat/autoclicker.h"
#include "../../module/combat/autorod.h"
#include "../../module/combat/blockhit.h"
#include "../../module/combat/backtrack.h"
#include "../../module/combat/hitselect.h"
#include "../../module/combat/nohitdelay.h"
#include "../../module/combat/criticals.h"
#include "../../module/combat/opportunity.h"
#include "../../module/combat/piercing.h"
#include "../../module/combat/wtap.h"

#include "../../module/combat/reach.h"
#include "../../module/combat/velocity.h"
#include "../../module/module_manager.h"
#include "../../module/movement/inventorymove.h"
#include "../../module/movement/nojumpdelay.h"
#include "../../module/movement/no_slow_down.h"
#include "../../module/movement/blink.h"
#include "../../module/movement/fakelag.h"
#include "../../module/movement/timer.h"
#include "../../module/movement/timerrange.h"
#include "../../module/movement/sprint.h"
#include "../../module/movement/faststop.h"
#include "../../module/utility/armorswitch.h"
#include "../../module/utility/autorefill.h"
#include "../../module/utility/altmanager.h"
#include "../../module/utility/autotool.h"
#include "../../module/utility/breaker.h"
#include "../../module/utility/bridgeassist.h"
#include "../../module/utility/clutch.h"
#include "../../module/utility/fastplace.h"
#include "../../module/utility/fastbreak.h"
#include "../../module/utility/noitemrelease.h"
#include "../../module/utility/inventorymanager.h"
#include "../../module/utility/teams.h"
#include "../../module/utility/throwpots.h"
#include "../../module/utility/autopot.h"
#include "../../module/visual/blockesp.h"
#include "../../module/visual/animations.h"
#include "../../module/visual/chams.h"
#include "../../module/visual/esp.h"
#include "../../module/visual/item_esp.h"
#include "../../module/visual/nametags.h"
#include "../../module/visual/anti_debuff.h"
#include "../../module/visual/tracers.h"
#include "../../module/visual/trajectories.h"
#include "../../module/visual/dropped_info.h"
#include "../../sdk/core/minecraft.h"
#include "../../security/session_validator.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/http.hpp"
#include "../../util/team/friends_manager.h"
#include "../core/hud_fonts.h"
#include "../hud/arraylist.h"
#include "../hud/watermark.h"
#include "../../module/visual/target_hud.h"
#include "../../module/visual/pings.h"
#include "../hud/notifications.h"
#include "../widgets/icons.hpp"
#include "../widgets/theme.hpp"
#include "../widgets/widgets.hpp"
#include "menu_nav.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <gl/GL.h>

#include "../../sdk/player/render_data.h"



#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif


static bool g_configsNeedsRefresh = false;



Module* getModuleForSubtab(int tabIdx, int subtabIdx) {
    
    if (tabIdx == 0) {
        switch (subtabIdx) {
            case 0:
                return autoClicker;
            case 1:
                return aimAssist;
            case 2:
                return velocity;
            case 3:
                return reach;
            case 4:
                return hitSelect;
            case 5:
                return noHitDelay;
            case 6:
                return criticals;
            case 7:
                return wTap;
            case 8:
                return piercing;
            case 9:
                return aimBow;
            case 10:
                return blockHit;
            case 11:
                return autoRod;
            case 12:
                return antiBot;
            case 13:
                return opportunity;
        }
    }
    
    else if (tabIdx == 1) {
        switch (subtabIdx) {
            case 0:
                return nameTags;
            case 1:
                return esp;
            case 2:
                return itemEsp;
            case 3:
                return tracers;
            case 4:
                return blockESP;
            case 5:
                return trajectories;
            case 6:
                return antiDebuff;
            case 7:
                return nullptr;  
            case 8:
                return droppedInfo;  
            case 9:
                return nullptr;  
            case 10:
                return nullptr;  
            case 11:
                return targetHud;
            case 12:
                return nullptr;  
            case 13:
                return pingsModule;
            case 14:
                return chams;
            case 15:
                return animations;
        }
    }
    
    else if (tabIdx == 2) {
        switch (subtabIdx) {
            case 0:
                return timerModule;
            case 1:
                return timerRange;
            case 2:
                return noSlowDown;
            case 3:
                return noJumpDelay;
            case 4:
                return sprint;
            case 5:
                return inventoryMove;
            case 6:
                return fastStop;
        }
    }
    
    else if (tabIdx == 3) {
        switch (subtabIdx) {
            case 0:
                return armorSwitch;
            case 1:
                return autoRefill;
            case 2:
                return throwPots;
            case 3:
                return autoPot;
            case 4:
                return fastPlace;
            case 5:
                return noItemRelease;
            case 6:
                return bridgeAssist;
            case 7:
                return scaffold;
            case 8:
                return clutch;
            case 9:
                return breaker;
            case 10:
                return autoTool;
            case 11:
                return inventoryManager;
            case 12:
                return fastBreak;
            case 13:
                return altManager;
            case 14:
                return teamsModule;
        }
    }
    
    else if (tabIdx == 4) {
        switch (subtabIdx) {
            case 0:
                return backtrack;
            case 1:
                return blink;
            case 2:
                return fakeLag;
        }
    }
    

    return nullptr;
}


static void renderSettingsPage() {
    float halfWidth =
        (ImGui::GetWindowWidth() - GImGui->Style.WindowPadding.x * 2) / 2 - GImGui->Style.ItemSpacing.x / 2;
    float bottomRowHeight = ui::s(100);
    float topRowHeight = ImGui::GetContentRegionAvail().y - bottomRowHeight - ui::s(10);

    
    ImGui::BeginGroup();
    {
        ui::begin_child("General", {halfWidth, topRowHeight});
        {
            ui::Binder("Menu Key", &Menu::menuKey);

            ImGui::Spacing();

            ui::color("Accent Color", g_SchemeColor);
            ui::Checkbox("Accent Rainbow", &ui::theme::accentRainbow);
            if (ui::theme::accentRainbow) {
                const char* rbTypes[] = {"Flow", "Solid", "Fast", "Wide"};
                ui::Combo("Rainbow", &ui::theme::rainbowType, rbTypes, 4);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            
            bool syncEnabled = ConfigManager::get().isSyncEnabled();
            if (ui::Checkbox("Cloud Sync", &syncEnabled)) {
                ConfigManager::get().setSyncEnabled(syncEnabled);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Sync config across devices");
            ImGui::PopStyleColor();
        }
        ui::end_child();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    
    ImGui::BeginGroup();
    {
        ui::begin_child("Friends", {halfWidth, topRowHeight});
        {
            if (friendsManager) {
                
                static char friendInput[64] = "";
                static std::string lastAddStatus = "";
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ui::s(60));
                bool enterPressed = ImGui::InputText("##addfriend", friendInput, sizeof(friendInput),
                                                     ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();
                if ((ImGui::Button("Add", ImVec2(ui::s(50), 0)) || enterPressed) && friendInput[0] != '\0') {
                    std::string inputName = friendInput;
                    bool added = false;

                    
                    
                    auto mojangProfile = http::lookupMojangUUID(inputName);
                    if (mojangProfile.found) {
                        friendsManager->addFriendByUUID(mojangProfile.uuid, mojangProfile.name);
                        lastAddStatus = "Added '" + mojangProfile.name + "' (Mojang)";
                        log_debug("[Friends] Mojang API: %s -> %s", inputName.c_str(), mojangProfile.uuid.c_str());
                        added = true;
                    }

                    
                    if (!added) {
                        PlayerInfo found = mc.findPlayerByName(inputName);
                        if (!found.uuid.empty()) {
                            friendsManager->addFriendByUUID(found.uuid, inputName);
                            lastAddStatus = "Added '" + inputName + "' (World)";
                            log_debug("[Friends] World scan: %s -> %s", inputName.c_str(), found.uuid.c_str());
                            added = true;
                        }
                    }

                    
                    if (!added) {
                        friendsManager->addFriend(inputName);
                        lastAddStatus = "'" + inputName + "' not found (name only)";
                        log_debug("[Friends] Fallback to name: %s", inputName.c_str());
                    }

                    friendInput[0] = '\0';
                }

                
                auto friends = friendsManager->getFriends();
                if (!friends.empty()) {
                    ImGui::Separator();
                    std::string uuidToRemove;
                    std::string nameToRemove;
                    for (const auto& f : friends) {
                        ImGui::Text("%s", f.displayName.c_str());
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ui::s(20));
                        ImGui::PushID((f.uuid.empty() ? f.displayName : f.uuid).c_str());
                        if (ImGui::SmallButton("X")) {
                            if (f.uuid.empty())
                                nameToRemove = f.displayName;
                            else
                                uuidToRemove = f.uuid;
                        }
                        ImGui::PopID();
                    }
                    if (!uuidToRemove.empty())
                        friendsManager->removeFriendByUUID(uuidToRemove);
                    if (!nameToRemove.empty())
                        friendsManager->removeFriend(nameToRemove);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                    ImGui::TextWrapped("No friends added");
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                
                
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("Teams moved to Utility > Teams");
                ImGui::PopStyleColor();

                ImGui::Spacing();

                
                ui::CheckboxBind("Quick Add", &friendsManager->quickAddEnabled, &friendsManager->quickAddKey);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("Press key while looking at player");
                ImGui::PopStyleColor();

                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::PopStyleColor();
            }
        }
        ui::end_child();
    }
    ImGui::EndGroup();

    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - bottomRowHeight);

    ImGui::BeginGroup();
    {
        ui::begin_child("Actions", {ImGui::GetWindowWidth() - GImGui->Style.WindowPadding.x * 2, bottomRowHeight});
        {
            float buttonWidth = (ImGui::GetContentRegionAvail().x - ui::s(10)) / 2;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.08f, 0.1f, 1.0f));

            if (ImGui::Button("PANIC", ImVec2(buttonWidth, ui::s(32)))) {
                if (moduleManager) {
                    for (auto mod : moduleManager->getModules()) {
                        if (mod) {
                            mod->setEnabled(false);
                        }
                    }
                }
                
                
                ConfigManager::get().onSettingChanged();
            }

            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.08f, 0.1f, 1.0f));

            if (ImGui::Button("EJECT CLIENT", ImVec2(buttonWidth, ui::s(32)))) {
                cobble::context::is_running = false;
                Menu::isOpen = false;
            }

            ImGui::PopStyleColor(3);
        }
        ui::end_child();
    }
    ImGui::EndGroup();
}




#if COBBLE_SPECTATE_ENABLED
static void renderSpectatePage() {
    auto& sm = SpectateManager::get();

    static double specLastPoll = 0.0;
    constexpr double kSpecPoll = 5.0;
    static std::string specSelected;         
    static bool showOtherSpectators = true;  

    double nowT = ImGui::GetTime();
    if (nowT - specLastPoll >= kSpecPoll) {
        sm.refreshListAsync();
        specLastPoll = nowT;
    }

    
    {
        bool allow = sm.allowSpectate();
        ui::Checkbox("Allow others to spectate me", &allow);
        sm.setAllowSpectate(allow);

        bool cfg = sm.allowConfigView();
        ui::Checkbox("Allow spectators to view my config", &cfg);
        sm.setAllowConfigView(cfg);

        if (sm.allowSpectate()) {
            ImGui::SameLine();
            ImGui::Text("     Spectators watching you: %d", sm.spectatorCount());
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float colW =
        (ImGui::GetWindowWidth() - GImGui->Style.WindowPadding.x * 2) / 2 - GImGui->Style.ItemSpacing.x / 2;
    const float colH = ImGui::GetContentRegionAvail().y;

    
    if (sm.isSpectating()) {
        spectate::SpectateSnapshot snap;
        bool have = sm.getLatestSnapshot(snap);

        ImGui::BeginGroup();
        ui::begin_child("SpectateView", {colW, colH});
        {
            ImGui::Text("Spectating: %s", sm.currentTarget().c_str());
            ImGui::SameLine();
            {
                
                bool live = (sm.framesReceived() % 2) == 0;
                ImGui::TextColored(sm.framesReceived() > 0 ? (live ? ImVec4(0.4f, 0.9f, 0.4f, 1) : ImVec4(0.3f, 0.8f, 0.3f, 1))
                                                           : ImVec4(0.9f, 0.5f, 0.3f, 1),
                                   "  [frames: %u]", sm.framesReceived());
            }
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            if (have) {
                ImGui::Text("Health: %.1f", snap.health);
                ImGui::Text("Pos: %.0f, %.0f, %.0f", snap.x, snap.y, snap.z);
                if (!snap.activity.empty())
                    ImGui::Text("Activity: %s", snap.activity.c_str());
                ImGui::Text("Entities: %d   Blocks: %d", (int)snap.entities.size(), (int)snap.blocks.size());
            } else {
                ImGui::TextWrapped("Waiting for frames...");
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("Stop Spectating", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                sm.stopSpectating();
        }
        ui::end_child();
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ui::begin_child("SpectateRadar", {colW, colH});
        {
            static int specCamMode = 0;  
            static float freeYaw = 0.0f, freePitch = 0.0f;
            if (ImGui::SmallButton(specCamMode == 1 ? "View: 1st" : "View: 3rd"))
                specCamMode ^= 1;
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                freeYaw = 0.0f;
                freePitch = 0.0f;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("drag = freelook");
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float side = avail.x < avail.y ? avail.x : avail.y;
            if (side < ui::s(80))
                side = ui::s(80);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 vc = ImVec2(p0.x + side * 0.5f, p0.y + side * 0.5f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + side, p0.y + side), IM_COL32(24, 26, 32, 255), 4.0f);
            dl->AddRect(p0, ImVec2(p0.x + side, p0.y + side), ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
            dl->PushClipRect(p0, ImVec2(p0.x + side, p0.y + side), true);

            if (have) {
                
                
                
                
                if (ImGui::IsMouseHoveringRect(p0, ImVec2(p0.x + side, p0.y + side)) && ImGui::IsMouseDragging(0)) {
                    ImVec2 dd = ImGui::GetMouseDragDelta(0);
                    freeYaw += dd.x * 0.4f;
                    freePitch = std::clamp(freePitch + dd.y * 0.4f, -80.0f, 80.0f);
                    ImGui::ResetMouseDragDelta(0);
                }
                const float d2r = 3.14159265f / 180.0f;
                const float cyaw = (snap.yaw + freeYaw) * d2r;
                const float cpitch = std::clamp(snap.pitch + freePitch, -89.0f, 89.0f) * d2r;
                const float fx = -std::sin(cyaw) * std::cos(cpitch);
                const float fy = -std::sin(cpitch);
                const float fz = std::cos(cyaw) * std::cos(cpitch);
                float rx = -fz, rz = fx;  
                float rl = std::sqrt(rx * rx + rz * rz);
                if (rl < 1e-4f)
                    rl = 1.0f;
                rx /= rl;
                rz /= rl;
                const float ux = -rz * fy, uy = rz * fx - rx * fz, uz = rx * fy;  
                const float camDist = specCamMode == 1 ? 0.2f : 4.5f, focal = side * 0.85f;
                const double ehx = snap.x, ehy = snap.y + 1.62, ehz = snap.z;  
                const double cpx = ehx - fx * camDist, cpy = ehy - fy * camDist, cpz = ehz - fz * camDist;

                auto project = [&](double wx, double wy, double wz, float& ox, float& oy, float& depth) -> bool {
                    double relx = wx - cpx, rely = wy - cpy, relz = wz - cpz;
                    double cz = relx * fx + rely * fy + relz * fz;
                    if (cz < 0.1)
                        return false;
                    double cx = relx * rx + relz * rz;              
                    double cy = relx * ux + rely * uy + relz * uz;  
                    ox = vc.x + (float)(cx / cz) * focal;
                    oy = vc.y - (float)(cy / cz) * focal;
                    depth = (float)cz;
                    return true;
                };

                struct BV { float x, y, depth; ImU32 col; };
                std::vector<BV> vis;
                vis.reserve(4096);
                for (const auto& b : snap.blocks) {
                    double wx = (double)snap.originX + b.dx, wy = (double)snap.originY + b.dy,
                           wz = (double)snap.originZ + b.dz;
                    float ox, oy, od;
                    if (!project(wx, wy, wz, ox, oy, od))
                        continue;
                    if (ox < p0.x - 30 || ox > p0.x + side + 30 || oy < p0.y - 30 || oy > p0.y + side + 30)
                        continue;
                    ImU32 col = IM_COL32(60 + (b.id * 37) % 170, 60 + (b.id * 61) % 170, 60 + (b.id * 97) % 170, 255);
                    vis.push_back({ox, oy, od, col});
                    if (vis.size() >= 5000)
                        break;
                }
                std::sort(vis.begin(), vis.end(), [](const BV& a, const BV& b) { return a.depth > b.depth; });
                for (const auto& v : vis) {
                    float s = focal / v.depth * 0.55f;
                    if (s < 1.0f) s = 1.0f;
                    if (s > 42.0f) s = 42.0f;
                    dl->AddRectFilled(ImVec2(v.x - s * 0.5f, v.y - s * 0.5f), ImVec2(v.x + s * 0.5f, v.y + s * 0.5f),
                                      v.col);
                }

                
                float fxs, fys, fdd, hxs, hys, hdd;
                if (project(snap.x, snap.y, snap.z, fxs, fys, fdd) &&
                    project(snap.x, snap.y + 1.8, snap.z, hxs, hys, hdd)) {
                    float lw = focal / fdd * 0.30f;
                    dl->AddLine(ImVec2(fxs, fys), ImVec2(hxs, hys), IM_COL32(90, 200, 255, 255), lw < 2 ? 2 : lw);
                    dl->AddCircleFilled(ImVec2(hxs, hys), lw < 3 ? 3 : lw * 0.6f, IM_COL32(120, 210, 255, 255));
                    std::string tn = sm.currentTarget();
                    if (!tn.empty()) {
                        ImVec2 tsz = ImGui::GetFont()->CalcTextSizeA(13.0f, FLT_MAX, 0, tn.c_str());
                        dl->AddText(ImGui::GetFont(), 13.0f, ImVec2(hxs - tsz.x * 0.5f, hys - 18),
                                    IM_COL32(120, 210, 255, 255), tn.c_str());
                    }
                }

                
                for (const auto& e : snap.entities) {
                    float ox, oy, od;
                    if (!project(e.x, e.y, e.z, ox, oy, od))
                        continue;
                    float s = focal / od * 0.4f;
                    if (s < 2.0f) s = 2.0f;
                    if (s > 22.0f) s = 22.0f;
                    ImU32 col = e.type == 0 ? IM_COL32(255, 90, 90, 255) : IM_COL32(255, 210, 90, 255);
                    dl->AddCircleFilled(ImVec2(ox, oy), s, col);
                    if (e.type == 0 && !e.name.empty()) {
                        ImVec2 tsz = ImGui::GetFont()->CalcTextSizeA(12.0f, FLT_MAX, 0, e.name.c_str());
                        dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(ox - tsz.x * 0.5f, oy - s - 14),
                                    IM_COL32(255, 255, 255, 235), e.name.c_str());
                    }
                }
            }
            dl->PopClipRect();
            ImGui::Dummy(ImVec2(side, side));
        }
        ui::end_child();
        ImGui::EndGroup();
        return;
    }

    
    auto browse = sm.getBrowseState();
    bool busy = (browse.status == SpectateManager::BrowseState::Loading);

    std::vector<const SpectateManager::BrowseEntry*> visible;
    for (const auto& e : browse.entries) {
        if (!showOtherSpectators && e.isSpectator)
            continue;
        visible.push_back(&e);
    }
    const SpectateManager::BrowseEntry* sel = nullptr;
    for (auto* e : visible) {
        if (e->username == specSelected) {
            sel = e;
            break;
        }
    }
    if (!sel)
        specSelected.clear();

    ImGui::BeginGroup();
    ui::begin_child("SpectateList", {colW, colH});
    {
        ui::Checkbox("Show other spectators", &showOtherSpectators);
        ImGui::Spacing();
        if (visible.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", browse.entries.empty() ? "No players available to spectate." : "");
            ImGui::PopStyleColor();
        }
        const float cardH = ui::s(46);
        const float pad = ui::s(6);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (auto* e : visible) {
            ImGui::PushID(e->username.c_str());
            ImVec2 cp = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##srow", e->username == specSelected, 0, ImVec2(0, cardH)))
                specSelected = e->username;
            dl->AddText(ImVec2(cp.x + pad, cp.y + pad), ImGui::GetColorU32(ImGuiCol_Text), e->username.c_str());
            std::string sub = (e->activity.empty() ? std::string("In game") : e->activity) + "   \xC2\xB7   " +
                              std::to_string(e->spectatorCount) + " watching";
            dl->AddText(ImVec2(cp.x + pad, cp.y + pad + ui::s(20)), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                        sub.c_str());
            ImGui::PopID();
        }
    }
    ui::end_child();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    ui::begin_child("SpectateDetail", {colW, colH});
    {
        if (sel) {
            ImGui::Text("%s", sel->username.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", sel->activity.empty() ? "In game" : sel->activity.c_str());
            ImGui::Text("%d watching", sel->spectatorCount);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("Spectate", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !busy)
                sm.startSpectatingAsync(sel->username);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Select a player to spectate.");
            ImGui::PopStyleColor();
        }
        if (!browse.message.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", browse.message.c_str());
            ImGui::PopStyleColor();
        }
    }
    ui::end_child();
    ImGui::EndGroup();
}
#endif 


static void renderConfigsPage() {
    static bool didInit = false;
    static std::vector<std::string> configs;
    static int selected = -1;
    static char nameBuf[64] = "";
    static bool confirmReset = false;
    static double lastPollTime = 0.0;
    constexpr double kPollIntervalSec = 6.0;

    auto refresh = [&]() {
        std::string selectedName;
        if (selected >= 0 && selected < (int)configs.size())
            selectedName = configs[selected];

        configs = ConfigManager::get().listUserConfigs();

        selected = -1;
        if (!selectedName.empty()) {
            for (int i = 0; i < (int)configs.size(); i++) {
                if (configs[i] == selectedName) {
                    selected = i;
                    break;
                }
            }
        }
    };

    if (!didInit) {
        refresh();
        didInit = true;
    }

    
    if (g_configsNeedsRefresh) {
        refresh();
        g_configsNeedsRefresh = false;
        lastPollTime = ImGui::GetTime();
    } else {
        double now = ImGui::GetTime();
        if (now - lastPollTime >= kPollIntervalSec) {
            refresh();
            lastPollTime = now;
        }
    }

    ImGui::PushFont(font(2));
    ImGui::Text("Configuration library");
    ImGui::PopFont();
    ImGui::TextDisabled("Save a setup before trying another one, or restore the client defaults in one step.");
    ImGui::Spacing();

    
    
    
    static int configView = 0;  
    static int prevConfigView = -1;
    {
        float halfW = (ImGui::GetContentRegionAvail().x - GImGui->Style.ItemSpacing.x) / 2;
        auto viewBtn = [&](const char* label, int idx) {
            bool active = (configView == idx);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, GetSchemeColor());
            if (ImGui::Button(label, ImVec2(halfW, 0)))
                configView = idx;
            if (active)
                ImGui::PopStyleColor();
        };
        viewBtn("My configs", 0);
        ImGui::SameLine();
        viewBtn("Community", 1);
    }
    ImGui::Spacing();

    
    
    
    
    const float colHalfWidth =
        (ImGui::GetWindowWidth() - GImGui->Style.WindowPadding.x * 2) / 2 - GImGui->Style.ItemSpacing.x / 2;
    const float colHeight = ImGui::GetContentRegionAvail().y;

    if (configView == 0) {
        ImGui::BeginGroup();
        {
            ui::begin_child("Saved profiles", {colHalfWidth, colHeight});
            {
                ImGui::TextDisabled("Choose a profile to load, overwrite, or delete.");
                ImGui::Spacing();
                if (configs.empty()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("No saved profiles yet.");
                    ImGui::TextDisabled("Give your current setup a name and save it here.");
                } else {
                    for (int i = 0; i < (int)configs.size(); i++) {
                        const bool isSel = (i == selected);
                        if (isSel)
                            ImGui::PushStyleColor(ImGuiCol_Header, GetSchemeColor(0.28f));
                        ImGui::PushID(i);
                        ImGui::PushFont(font(isSel ? 1 : 0));
                        if (ImGui::Selectable(configs[i].c_str(), isSel, 0, ImVec2(-FLT_MIN, ui::s(34)))) {
                            selected = i;
                            strncpy_s(nameBuf, sizeof(nameBuf), configs[i].c_str(), _TRUNCATE);
                            confirmReset = false;
                        }
                        ImGui::PopFont();
                        ImGui::PopID();
                        if (isSel)
                            ImGui::PopStyleColor();
                    }
                }
            }
            ui::end_child();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
            ui::begin_child("Profile workspace", {colHalfWidth, colHeight});
            {
                ImGui::Text("Profile name");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText("##configname", nameBuf, sizeof(nameBuf));
                ImGui::TextDisabled("Save creates a new profile or updates the selected one.");

                ImGui::Spacing();

                std::string err;
                std::string n =
                    nameBuf[0] != '\0'
                        ? std::string(nameBuf)
                        : (selected >= 0 && selected < (int)configs.size() ? configs[selected] : std::string());

                ImGui::PushStyleColor(ImGuiCol_Button, GetSchemeColor(0.88f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetSchemeColor());
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, GetSchemeColor(0.72f));
                if (ImGui::Button("Save profile", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    if (n.empty()) {
                        NotificationManager::addSystem("Error: enter a name");
                    } else if (ConfigManager::get().saveUserConfig(n, &err)) {
                        NotificationManager::addSystem("Saved config: " + n);
                        refresh();
                    } else {
                        NotificationManager::addSystem("Error: " + (err.empty() ? std::string("save failed") : err));
                    }
                }
                ImGui::PopStyleColor(3);

                if (ImGui::Button("Load selected", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    if (n.empty()) {
                        NotificationManager::addSystem("Error: select a config");
                    } else if (ConfigManager::get().loadUserConfig(n, &err)) {
                        NotificationManager::addSystem("Loaded config: " + n);
                        confirmReset = false;
                    } else {
                        NotificationManager::addSystem("Error: " + (err.empty() ? std::string("load failed") : err));
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.18f, 0.18f, 1.0f));
                if (ImGui::Button("Delete selected", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    if (n.empty()) {
                        NotificationManager::addSystem("Error: select a config");
                    } else if (ConfigManager::get().deleteUserConfig(n, &err)) {
                        NotificationManager::addSystem("Deleted config: " + n);
                        if (selected >= 0 && selected < (int)configs.size() && configs[selected] == n)
                            selected = -1;
                        if (std::string(nameBuf) == n)
                            nameBuf[0] = '\0';
                        refresh();
                    } else {
                        NotificationManager::addSystem("Error: " +
                                                       (err.empty() ? std::string("delete failed") : err));
                    }
                }
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::PushFont(font(1));
                ImGui::Text("Factory reset");
                ImGui::PopFont();
                ImGui::TextDisabled("Resets the active setup. Saved profiles stay available.");

                if (!confirmReset) {
                    if (ImGui::Button("Restore defaults", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        confirmReset = true;
                } else {
                    ImGui::TextDisabled("This replaces the active setup immediately.");
                    const float buttonWidth = (ImGui::GetContentRegionAvail().x - GImGui->Style.ItemSpacing.x) * 0.5f;
                    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                        confirmReset = false;
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.19f, 0.19f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.24f, 0.24f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.48f, 0.13f, 0.13f, 1.0f));
                    if (ImGui::Button("Reset now", ImVec2(buttonWidth, 0))) {
                        if (ConfigManager::get().resetToDefaults(&err)) {
                            NotificationManager::addSystem("Restored factory defaults");
                            nameBuf[0] = '\0';
                            selected = -1;
                        } else {
                            NotificationManager::addSystem("Error: " +
                                                           (err.empty() ? std::string("reset failed") : err));
                        }
                        confirmReset = false;
                    }
                    ImGui::PopStyleColor(3);
                }
            }
            ui::end_child();
        }
        ImGui::EndGroup();
    }

    
    static char pubNameBuf[64] = "";
    static char pubDescBuf[128] = "";
    static std::string pubSelectedId;  
    static bool pubAnonymous = false;  
    static int pubFilter = 0;          
    static double pubLastPoll = 0.0;
    constexpr double kPubPollIntervalSec = 15.0;

    
    if (configView == 1 && prevConfigView != 1) {
        ConfigManager::get().refreshPublicConfigsAsync();
        pubLastPoll = ImGui::GetTime();
    }
    prevConfigView = configView;

    if (configView != 1)
        return;

    if (ImGui::GetTime() - pubLastPoll >= kPubPollIntervalSec) {
        ConfigManager::get().refreshPublicConfigsAsync();
        pubLastPoll = ImGui::GetTime();
    }

    auto browse = ConfigManager::get().getBrowseState();
    bool busy = (browse.status == ConfigManager::BrowseState::Loading);

    
    {
        float halfW = (ImGui::GetContentRegionAvail().x - GImGui->Style.ItemSpacing.x) / 2;
        auto filterBtn = [&](const char* label, int idx) {
            bool active = (pubFilter == idx);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, GetSchemeColor());
            if (ImGui::Button(label, ImVec2(halfW, 0)))
                pubFilter = idx;
            if (active)
                ImGui::PopStyleColor();
        };
        filterBtn("Browse", 0);
        ImGui::SameLine();
        filterBtn("Mine", 1);
    }
    ImGui::Spacing();

    
    std::vector<const ConfigManager::PublicConfigEntry*> visible;
    for (const auto& e : browse.entries) {
        if (pubFilter == 1 && !e.owned)
            continue;
        visible.push_back(&e);
    }

    
    const ConfigManager::PublicConfigEntry* sel = nullptr;
    for (auto* e : visible) {
        if (e->id == pubSelectedId) {
            sel = e;
            break;
        }
    }
    if (!sel)
        pubSelectedId.clear();

    const float pubColWidth =
        (ImGui::GetWindowWidth() - GImGui->Style.WindowPadding.x * 2) / 2 - GImGui->Style.ItemSpacing.x / 2;
    const float pubColHeight = ImGui::GetContentRegionAvail().y;

    
    ImGui::BeginGroup();
    {
        ui::begin_child("Browse Public", {pubColWidth, pubColHeight});
        {
            if (visible.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("%s", pubFilter == 1
                                             ? "You haven't published any configs."
                                             : (browse.entries.empty() ? "No configs found." : ""));
                ImGui::PopStyleColor();
            }

            const float cardH = ui::s(62);
            const float pad = ui::s(6);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (auto* e : visible) {
                ImGui::PushID(e->id.c_str());
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                bool selected = (e->id == pubSelectedId);
                if (ImGui::Selectable("##card", selected, 0, ImVec2(0, cardH)))
                    pubSelectedId = e->id;

                ImU32 nameCol = ImGui::GetColorU32(ImGuiCol_Text);
                ImU32 subCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
                dl->AddText(ImVec2(p0.x + pad, p0.y + pad), nameCol, e->name.c_str());
                std::string meta = "by " + e->owner + "  \xC2\xB7  " + std::to_string(e->downloads) + " downloads";
                dl->AddText(ImVec2(p0.x + pad, p0.y + pad + ui::s(20)), subCol, meta.c_str());
                if (!e->description.empty()) {
                    std::string snip = e->description.substr(0, 48);
                    if (e->description.size() > 48)
                        snip += "...";
                    dl->AddText(ImVec2(p0.x + pad, p0.y + pad + ui::s(38)), subCol, snip.c_str());
                }
                ImGui::PopID();
            }
        }
        ui::end_child();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    
    ImGui::BeginGroup();
    {
        ui::begin_child("Detail", {pubColWidth, pubColHeight});
        {
            if (sel) {
                ImGui::TextWrapped("%s", sel->name.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                std::string meta = "by " + sel->owner + "  \xC2\xB7  " + std::to_string(sel->downloads) + " downloads";
                ImGui::TextWrapped("%s", meta.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
                if (!sel->description.empty())
                    ImGui::TextWrapped("%s", sel->description.c_str());
                ImGui::Spacing();

                if (ImGui::Button("Apply", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !busy)
                    ConfigManager::get().downloadAndApplyPublicConfigAsync(sel->id);
                if (sel->owned) {
                    if (ImGui::Button("Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !busy) {
                        ConfigManager::get().deletePublicConfigAsync(sel->id);
                        pubSelectedId.clear();
                    }
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("Select a config to view details.");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Publish current settings");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##pubname", "Name", pubNameBuf, sizeof(pubNameBuf));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##pubdesc", "Description", pubDescBuf, sizeof(pubDescBuf));

            
            std::string uname = SessionValidator::GetUsername();
            if (uname.empty())
                uname = "My account";
            {
                float halfW = (ImGui::GetContentRegionAvail().x - GImGui->Style.ItemSpacing.x) / 2;
                auto authorBtn = [&](const char* label, bool anon) {
                    bool active = (pubAnonymous == anon);
                    if (active)
                        ImGui::PushStyleColor(ImGuiCol_Button, GetSchemeColor());
                    if (ImGui::Button(label, ImVec2(halfW, 0)))
                        pubAnonymous = anon;
                    if (active)
                        ImGui::PopStyleColor();
                };
                authorBtn("Anonymous", true);
                ImGui::SameLine();
                authorBtn(uname.c_str(), false);
            }

            if (ImGui::Button("Publish", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !busy) {
                if (pubNameBuf[0] == '\0')
                    NotificationManager::addSystem("Error: enter a name");
                else
                    ConfigManager::get().publishPublicConfigAsync(pubNameBuf, pubDescBuf, pubAnonymous);
            }

            if (!browse.message.empty()) {
                ImGui::Spacing();
                bool isErr = (browse.status == ConfigManager::BrowseState::Error);
                ImGui::PushStyleColor(ImGuiCol_Text, isErr ? IM_COL32(255, 90, 90, 255)
                                                           : ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("%s", browse.message.c_str());
                ImGui::PopStyleColor();
            }
        }
        ui::end_child();
    }
    ImGui::EndGroup();
}









static void GetRoundedRectOutline(ImDrawList* draw, const ImVec2& a, const ImVec2& b, float rounding,
                                  ImVector<ImVec2>& out) {
    draw->PathClear();
    draw->PathRect(a, b, rounding);
    out.resize(draw->_Path.Size);
    for (int i = 0; i < draw->_Path.Size; ++i)
        out[i] = draw->_Path[i];
    draw->PathClear();
}



static ImVec2 SampleClosedPolyline(const ImVector<ImVec2>& pts, const ImVector<float>& cum, float dist) {
    const int n = pts.Size;
    const float total = cum[n];
    dist -= total * floorf(dist / total);  
    int i = 0;
    while (i < n - 1 && cum[i + 1] < dist)
        ++i;
    const float segLen = cum[i + 1] - cum[i];
    const float t = segLen > 0.f ? (dist - cum[i]) / segLen : 0.f;
    const ImVec2 p0 = pts[i];
    const ImVec2 p1 = pts[(i + 1) % n];
    return ImVec2(p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t);
}









static void AddOutlineSweep(ImDrawList* draw, const ImVec2& a, const ImVec2& b, float rounding,
                            float headU, float intensity) {
    if (!draw || intensity <= 0.01f)
        return;

    ImVector<ImVec2> pts;
    GetRoundedRectOutline(draw, a, b, rounding, pts);
    const int n = pts.Size;
    if (n < 3)
        return;

    ImVector<float> cum;
    cum.resize(n + 1);
    cum[0] = 0.f;
    for (int i = 0; i < n; ++i) {
        const ImVec2 p0 = pts[i];
        const ImVec2 p1 = pts[(i + 1) % n];
        const float dx = p1.x - p0.x, dy = p1.y - p0.y;
        cum[i + 1] = cum[i] + sqrtf(dx * dx + dy * dy);
    }
    const float total = cum[n];
    if (total <= 0.f)
        return;

    
    
    
    
    
    
    
    
    
    
    
    const int M = 192;                
    const float headSigma = 0.022f;   
    const float tailTau = 0.090f;     

    ImVector<ImVec2> sp;
    ImVector<float> sb;
    sp.resize(M + 1);
    sb.resize(M + 1);
    for (int i = 0; i <= M; ++i) {
        const float u = (float)i / M;
        sp[i] = SampleClosedPolyline(pts, cum, u * total);
        
        
        float d = headU - u;
        d -= floorf(d + 0.5f);
        if (d >= 0.f)
            sb[i] = expf(-d / tailTau);  
        else
            sb[i] = expf(-(d * d) / (2.f * headSigma * headSigma));  
    }

    
    
    
    
    
    
    
    const ImVec2 center((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    ImVector<ImVec2> nrm;
    nrm.resize(M);
    for (int i = 0; i < M; ++i) {
        const ImVec2 pa = sp[(i - 1 + M) % M];
        const ImVec2 pb = sp[(i + 1) % M];
        float tx = pb.x - pa.x, ty = pb.y - pa.y;
        const float len = sqrtf(tx * tx + ty * ty);
        if (len > 1e-6f) {
            tx /= len;
            ty /= len;
        }
        ImVec2 nn(-ty, tx);
        
        if (nn.x * (sp[i].x - center.x) + nn.y * (sp[i].y - center.y) < 0.f)
            nn = ImVec2(-nn.x, -nn.y);
        nrm[i] = nn;
    }

    
    
    
    const bool aaFill = (draw->Flags & ImDrawListFlags_AntiAliasedFill) != 0;
    draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;

    
    
    
    
    
    
    
    
    
    
    const float innerCap = rounding * 0.85f;  
    
    
    const float vGlow = 0.60f;
    const int PASSES = 8;
    for (int k = 0; k < PASSES; ++k) {
        const float t = (float)k / (float)(PASSES - 1);     
        
        
        
        
        const float outerBase = ImLerp(0.55f, 0.85f, t);
        const float outerGain = ImLerp(1.05f, 2.30f, t);    
        const float innerBase = ImLerp(0.40f, 0.60f, t);
        const float innerGain = ImLerp(1.05f, 2.10f, t);    
        const float pAlpha = ImLerp(0.20f, 0.010f, t * t);  
        for (int i = 0; i < M; ++i) {
            const int j = (i + 1) % M;
            const float bmid = (sb[i] + sb[j]) * 0.5f;
            const float alpha = intensity * bmid * pAlpha;
            if (alpha <= 0.0020f)
                continue;  
            const float outI = ui::s(outerBase + outerGain * sb[i]);
            const float outJ = ui::s(outerBase + outerGain * sb[j]);
            const float inI = ImMin(ui::s(innerBase + innerGain * sb[i]), innerCap);
            const float inJ = ImMin(ui::s(innerBase + innerGain * sb[j]), innerCap);
            const ImVec2 quad[4] = {
                ImVec2(sp[i].x - nrm[i].x * inI, sp[i].y - nrm[i].y * inI * vGlow),    
                ImVec2(sp[j].x - nrm[j].x * inJ, sp[j].y - nrm[j].y * inJ * vGlow),    
                ImVec2(sp[j].x + nrm[j].x * outJ, sp[j].y + nrm[j].y * outJ * vGlow),  
                ImVec2(sp[i].x + nrm[i].x * outI, sp[i].y + nrm[i].y * outI * vGlow),  
            };
            const int a8 = (int)(ImClamp(alpha, 0.f, 1.f) * 255.f);
            const ImU32 base = GetAccentRGB();
            const ImU32 glowCol = (base & ~IM_COL32_A_MASK) | ((ImU32)a8 << IM_COL32_A_SHIFT);
            draw->AddConvexPolyFilled(quad, 4, glowCol);
        }
    }

    if (aaFill)
        draw->Flags |= ImDrawListFlags_AntiAliasedFill;
    
}










static void renderModuleList(int tab) {
    if (tab < 0 || tab >= (int)ui::tabs.size())
        return;

    
    
    ui::SetDescriptionContext("__modules__");

    auto& t = ui::tabs[tab];

    
    static char searchBuf[64] = "";
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(ui::theme::rounding::Frame));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ui::s(8), ui::s(6)));

        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##modulesearch", "Search settings...", searchBuf, sizeof(searchBuf));

        ImGui::PopStyleVar(2);
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    
    struct SearchResult {
        int tabIdx;
        int subtabIdx;
        const char* name;
    };

    std::vector<SearchResult> filteredResults;
    if (searchBuf[0] == '\0') {
        
        const int n = (int)t.m_subtabs.size();
        for (int i = 0; i < n; ++i) {
            filteredResults.push_back({tab, i, t.m_subtabs[i]});
        }
    } else {
        
        
        std::string searchStr = searchBuf;
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

        for (int tIdx = 0; tIdx < (int)ui::tabs.size(); ++tIdx) {
            auto& otherTab = ui::tabs[tIdx];
            const int n = (int)otherTab.m_subtabs.size();
            for (int i = 0; i < n; ++i) {
                const char* name = otherTab.m_subtabs[i];
                std::string nameStr = name;
                std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
                if (nameStr.find(searchStr) != std::string::npos) {
                    filteredResults.push_back({tIdx, i, name});
                }
            }
        }
    }

    const int filteredCount = (int)filteredResults.size();

    const float cardGapY = ui::s(6);   
    const float cardH = ui::s(56);     
    const float cardRound = ui::s(ui::theme::rounding::ModuleCard);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float cardW = avail;  

    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, cardGapY));

    
    auto drawCard = [&](int idx) {
        const auto& result = filteredResults[idx];
        const char* name = result.name;
        int originalTab = result.tabIdx;
        int originalSubtab = result.subtabIdx;

        
        
        Module* m = getModuleForSubtab(originalTab, originalSubtab);
        bool isLocked = m && m->showsDeveloperOnly();
        
        
        bool isArrayList = !m && strcmp(name, "ArrayList") == 0;
        bool isNotifications = !m && strcmp(name, "Notifications") == 0;
        bool isWatermark = !m && strcmp(name, "Watermark") == 0;
        bool isClickGui = !m && strcmp(name, "ClickGUI") == 0;  
        bool isEnabled = m ? m->enabled
                           : (isArrayList ? ArrayListRenderer::enabled
                                          : (isNotifications ? NotificationManager::enabled
                                                             : (isWatermark ? Watermark::enabled
                                                                            : (isClickGui ? (ui::gui_mode == 1)
                                                                                          : false))));

        ImGui::PushID(idx);

        
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb{pos, pos + ImVec2(cardW, cardH)};
        ImGui::ItemSize(bb);

        
        const float padR = ui::s(14);
        const float gearZoneW = ui::s(30);

        ImRect gearRect{ImVec2(bb.Max.x - padR - gearZoneW, bb.Min.y), ImVec2(bb.Max.x - padR, bb.Max.y)};

        
        
        auto openConfig = [&]() {
            
            if (ui::cur_tab != originalTab) {
                ui::cur_tab = originalTab;
            }
            auto& activeTab = ui::tabs[originalTab];
            activeTab.next_subtab = originalSubtab;
            activeTab.next_view = ui::CategoryView::Config;
            ui::content_anim_dst = 0.f;
        };

        
        
        
        
        
        auto toggleModule = [&]() {
            if (m) {
                
                m->toggle();
            } else if (isArrayList) {
                ArrayListRenderer::enabled = !ArrayListRenderer::enabled;
            } else if (isNotifications) {
                NotificationManager::enabled = !NotificationManager::enabled;
            } else if (isWatermark) {
                Watermark::enabled = !Watermark::enabled;
            } else if (isClickGui) {
                ui::gui_mode = (ui::gui_mode == 1) ? 0 : 1;  
            }
            ConfigManager::get().onSettingChanged();
        };

        
        ImGuiID gearId = window->GetID((std::string(name) + "##gear").c_str());
        ImGui::ItemAdd(gearRect, gearId);
        bool gHovered, gHeld;
        bool gPressed = ImGui::ButtonBehavior(gearRect, gearId, &gHovered, &gHeld);
        if (gPressed) {
            openConfig();
        }

        
        ImRect nameRect{bb.Min, ImVec2(gearRect.Min.x, bb.Max.y)};
        ImGuiID nameId = window->GetID(name);
        ImGui::ItemAdd(nameRect, nameId);
        bool nHovered, nHeld;
        bool nPressed = ImGui::ButtonBehavior(nameRect, nameId, &nHovered, &nHeld);
        if (nPressed) {
            toggleModule();
        }

        
        if ((nHovered || gHovered) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            openConfig();
        }

        struct card_state {
            float hover;
            float gearHover;
            float enabled;
            float spin;  
        };
        auto& obj = ui::anim_obj(name, 2, card_state{0.f, 0.f, 0.f, 0.f});
        obj.hover = ui::animate(obj.hover, 0.f, 1.f, nHovered || gHovered);
        obj.gearHover = ui::animate(obj.gearHover, 0.f, 1.f, gHovered);
        obj.enabled = ui::animate(obj.enabled, 0.f, 1.f, isEnabled);

        
        
        
        const float dt = ImGui::GetIO().DeltaTime;
        const float baseSpeed = 0.4f;  
        obj.spin += dt * baseSpeed;
        if (obj.spin > 1.0e6f)  
            obj.spin -= 1.0e6f;

        ImDrawList* draw = window->DrawList;

        
        
        
        const float plateA = 0.30f + obj.hover * 0.16f + obj.enabled * 0.10f;
        draw->AddRectFilled(bb.Min, bb.Max, ui::GetColor(ImGuiCol_FrameBg, plateA), cardRound);

        
        
        if (obj.hover > 0.01f)
            draw->AddRect(bb.Min, bb.Max, GetSchemeColor(0.30f * obj.hover), cardRound, 0, ui::s(1.0f));

        
        
        
        
        
        const float sweepT = obj.enabled;
        if (sweepT > 0.01f) {
            const float headU = obj.spin - floorf(obj.spin);  
            AddOutlineSweep(draw, bb.Min, bb.Max, cardRound, headU, sweepT);
        }

        
        const char* nameEnd = ImGui::FindRenderedTextEnd(name);
        const char* subtitle = ui::GetModuleDescription(name);
        ImFont* nameFont = font(2);  
        ImFont* subFont = font(0);   
        const float nameFS = nameFont->FontSize * 0.96f;
        const float subFS = subFont->FontSize * 0.9f;
        ImVec2 nameSize = nameFont->CalcTextSizeA(nameFS, FLT_MAX, 0.f, name, nameEnd);
        const float textX = nameRect.Min.x + ui::s(14);
        const float lineGap = ui::s(2);
        const float blockH = nameSize.y + lineGap + subFS;
        const float nameY = bb.GetCenter().y - blockH * 0.5f;

        ImU32 nameCol;
        if (isLocked) {
            nameCol = IM_COL32(100, 100, 100, 150);
        } else {
            float accentT = ImMax(obj.enabled, obj.hover);
            nameCol = ui::col_anim(ui::GetColor(ImGuiCol_Text, 0.9f), GetSchemeColor(), accentT);
        }
        draw->AddText(nameFont, nameFS, ImVec2(textX, nameY), nameCol, name, nameEnd);

        ImU32 subCol = isLocked ? IM_COL32(90, 90, 90, 130) : ui::GetColor(ImGuiCol_TextDisabled, 0.75f);
        draw->AddText(subFont, subFS, ImVec2(textX, nameY + nameSize.y + lineGap), subCol, subtitle);

        
        const char* gear = ICON_FA_COG;
        const char* gearEnd = ImGui::FindRenderedTextEnd(gear);
        ImVec2 gearSize = ImGui::CalcTextSize(gear, gearEnd);
        ImVec2 gearPos(gearRect.GetCenter().x - gearSize.x / 2.f, gearRect.GetCenter().y - gearSize.y / 2.f);
        ImU32 gearCol = ui::GetColor(ImGuiCol_TextDisabled, 0.45f + obj.gearHover * 0.55f);
        draw->AddText(gearPos, gearCol, gear, gearEnd);

        ui::HoverDescription(name, nHovered || gHovered,
                             isLocked ? "Developer only (for now.)" : subtitle, isLocked);

        ImGui::PopID();
    };

    
    ImGui::BeginGroup();
    for (int i = 0; i < filteredCount; ++i)
        drawCard(i);
    ImGui::EndGroup();

    ImGui::PopStyleVar();
}















static void hudFontCombo(const char* label, int* fontId) {
    const int count = hud_fonts::Count();
    if (count <= 0 || !hud_fonts::Names())
        return;
    int row = hud_fonts::ComboIndexOf(*fontId);
    if (ui::Combo(label, &row, hud_fonts::Names(), count))
        *fontId = hud_fonts::IdAt(row);
}

static void renderModuleConfig(int tab) {
    if (tab < 0 || tab >= (int)ui::tabs.size())
        return;

    auto& t = ui::tabs[tab];

    
    
    const float bodyTopY = ImGui::GetCursorPosY();
    float bodyMargin = 0.f;  

    
    
    
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const char* backLabel = "Back";
        ImGuiID id = window->GetID("##module_config_back");

        const char* chevron = ICON_FA_CHEVRON_LEFT;
        ImVec2 chevSize = ImGui::CalcTextSize(chevron, 0, 1);
        ImVec2 labelSize = ImGui::CalcTextSize(backLabel, 0, 1);
        float iconGap = ui::s(6);
        float padX = ui::s(8);
        float rowHeight = ui::s(26);
        float controlW = padX + chevSize.x + iconGap + labelSize.x + padX;

        
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 origin = window->DC.CursorPos;
        ImVec2 bbMin{origin.x + availWidth - controlW, origin.y};
        ImRect bb{bbMin, bbMin + ImVec2(controlW, rowHeight)};
        ImGui::ItemAdd(bb, id);
        ImGui::ItemSize(ImVec2(availWidth, rowHeight));

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        
        if (pressed) {
            t.next_view = ui::CategoryView::List;
            ui::content_anim_dst = 0.f;
        }

        struct back_state {
            float hover;
        };
        auto& obj = ui::anim_obj("##module_config_back", 1, back_state{0.f});
        obj.hover = ui::animate(obj.hover, 0.f, 1.f, hovered);

        if (obj.hover > 0.01f) {
            window->DrawList->AddRectFilled(bb.Min, bb.Max,
                                            ui::GetColor(ImGuiCol_FrameBgHovered, obj.hover * 0.35f),
                                            ui::s(4));
        }

        ImU32 col = ui::col_anim(ui::GetColor(ImGuiCol_Text, 0.85f), GetSchemeColor(), obj.hover);

        float chevX = bb.Min.x + padX;
        window->DrawList->AddText(ImVec2(chevX, bb.GetCenter().y - chevSize.y / 2), col, chevron,
                                  ImGui::FindRenderedTextEnd(chevron));
        window->DrawList->AddText(ImVec2(chevX + chevSize.x + iconGap, bb.GetCenter().y - labelSize.y / 2), col,
                                  backLabel, ImGui::FindRenderedTextEnd(backLabel));
    }

    
    
    
    ImGui::SetCursorPosY(bodyTopY);
    {
        float availWidth = ImGui::GetContentRegionAvail().x;
        bodyMargin = ui::s(6.f);
        ImGui::Indent(bodyMargin);
        ImGui::PushItemWidth(availWidth - bodyMargin * 2.f);
    }

    
    Module* mod = getModuleForSubtab(ui::cur_tab, t.cur_subtab);
    if (mod) {
        ui::SetDescriptionContext(mod->name,  true);
        
        if (!mod->isAccessible()) {
            ImGui::Spacing();
            ImGui::Spacing();

            
            float width = ImGui::GetContentRegionAvail().x;

            
            ImVec2 iconSize = ImGui::CalcTextSize(ICON_FA_LOCK);
            ImGui::SetCursorPosX((width - iconSize.x) / 2);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text(ICON_FA_LOCK);
            ImGui::PopStyleColor();

            
            const char* text = "This feature is locked.";
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX((width - textSize.x) / 2);
            ImGui::Text(text);

            ImGui::Spacing();

            const char* subText = "Enter unlock code:";
            ImVec2 subSize = ImGui::CalcTextSize(subText);
            ImGui::SetCursorPosX((width - subSize.x) / 2);
            ImGui::Text(subText);

            
            static char buf[64] = "";
            static bool incorrect = false;

            ImGui::SetCursorPosX((width - ui::s(200)) / 2);
            ImGui::SetNextItemWidth(ui::s(200));

            

            bool enterPressed = ImGui::InputText("##pass", buf, sizeof(buf),
                                                 ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

            if (incorrect) {
                const char* err = "Incorrect code!";
                ImVec2 errSize = ImGui::CalcTextSize(err);
                ImGui::SetCursorPosX((width - errSize.x) / 2);
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, err);
            }

            ImGui::Spacing();

            ImGui::SetCursorPosX((width - ui::s(100)) / 2);
            if (ImGui::Button("Unlock", {ui::s(100), ui::s(30)}) || enterPressed) {
                if (Module::checkUnlockCode(buf)) {
                    Module::experimentalUnlocked = true;
                    incorrect = false;
                    memset(buf, 0, sizeof(buf));
                } else {
                    incorrect = true;
                    memset(buf, 0, sizeof(buf));
                }
            }

        } else {
            
            mod->onRenderMenu();
        }
    }
    
    else if (ui::cur_tab == 1 && t.cur_subtab == 7) {
        
        ui::SetDescriptionContext("ArrayList",  true);
        ui::two_column(
            "Settings",
            []() {
                ui::Checkbox("Enabled", &ArrayListRenderer::enabled);
                ui::Checkbox("Rainbow", &ArrayListRenderer::rainbow);
                ui::Checkbox("Color Bar", &ArrayListRenderer::colorBar);
                ui::Checkbox("Background", &ArrayListRenderer::showBackground);
            },
            "Options",
            []() {
                ui::Checkbox("Sort by Length", &ArrayListRenderer::sortByLength);
                ui::Checkbox("Show Info", &ArrayListRenderer::showInfo);
                hudFontCombo("Font", &ArrayListRenderer::fontStyle);
                ui::SliderFloat("Size", &ArrayListRenderer::scale, 0.5f, 2.0f, "%.1fx");

                
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("Drag the ArrayList directly on screen to move it.");
                ImGui::PopStyleColor();

                
                
                
                
                
                const char* anchorItems[] = {"Top Right", "Top Left", "Bottom Right", "Bottom Left"};
                if (ui::Combo("Corner", &ArrayListRenderer::anchor, anchorItems, 4))
                    ArrayListRenderer::userPlaced = false;
                if (ImGui::Button("Reset Position", ImVec2(-1, ui::s(24))))
                    ArrayListRenderer::userPlaced = false;

                if (ArrayListRenderer::rainbow) {
                    const char* rbTypes[] = {"Flow", "Solid", "Fast", "Wide"};
                    ui::Combo("Rainbow Type", &ArrayListRenderer::rainbowType, rbTypes, 4);
                    bool highSat = ArrayListRenderer::rainbowSaturation > 0.6f;
                    if (ui::Checkbox("High Saturation", &highSat)) {
                        ArrayListRenderer::rainbowSaturation = highSat ? 0.8f : 0.4f;
                    }
                }
                if (!ArrayListRenderer::rainbow) {
                    ui::Checkbox("Follow Accent", &ArrayListRenderer::useAccent);
                    if (!ArrayListRenderer::useAccent)
                        ui::color("Color", ArrayListRenderer::color,  false);
                }
            });
    } else if (ui::cur_tab == 1 && t.cur_subtab == 9) {
        
        ui::SetDescriptionContext("Notifications",  true);
        ui::section("Settings", []() { ui::Checkbox("Enabled", &NotificationManager::enabled); });
    } else if (ui::cur_tab == 1 && t.cur_subtab == 10) {
        
        
        
        
        ui::SetDescriptionContext("Watermark",  true);

        
        
        
        if (Watermark::pollPicker())
            ConfigManager::get().onSettingChanged();

        
        
        static int wmSelected = 0;
        static int wmPreset = 0;
        static int wmAddKind = Watermark::KindText;

        ui::two_column(
            "Items",
            []() {
                ui::Checkbox("Enabled", &Watermark::enabled);

                
                
                if (ui::Combo("Preset", &wmPreset, Watermark::presetNames(), Watermark::presetCount())) {
                    Watermark::applyPreset(wmPreset);
                    wmSelected = 0;
                }

                if (wmSelected >= Watermark::itemCount)
                    wmSelected = Watermark::itemCount - 1;
                if (wmSelected < 0)
                    wmSelected = 0;

                
                
                const float colW = ImGui::CalcItemWidth();
                for (int i = 0; i < Watermark::itemCount; ++i) {
                    const Watermark::Item& it = Watermark::items[i];
                    const char* body = (it.kind == Watermark::KindText && it.text[0] != '\0')
                                           ? it.text
                                           : Watermark::kindName(it.kind);
                    char label[128];
                    ImFormatString(label, sizeof(label), "%d. %s%s##wmItem%d", i + 1, it.enabled ? "" : "[off] ",
                                   body, i);
                    const bool isSel = (i == wmSelected);
                    if (isSel)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                    if (ImGui::Button(label, ImVec2(colW, ui::s(22))))
                        wmSelected = i;
                    if (isSel)
                        ImGui::PopStyleColor();
                }
                if (Watermark::itemCount == 0)
                    ImGui::TextDisabled("No items -- add one below.");
                ImGui::Spacing();

                
                
                
                const char* kinds[Watermark::KindCount];
                for (int k = 0; k < Watermark::KindCount; ++k)
                    kinds[k] = Watermark::kindName(k);
                ui::Combo("New Item", &wmAddKind, kinds, Watermark::KindCount);

                
                
                
                const float halfW = (colW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                const ImVec2 halfBtn(halfW, ui::s(24));
                if (ImGui::Button("Add", halfBtn) && Watermark::addItem(wmAddKind))
                    wmSelected = Watermark::itemCount - 1;
                ImGui::SameLine();
                if (ImGui::Button("Remove", halfBtn))
                    Watermark::removeItem(wmSelected);
                if (ImGui::Button("Move Up", halfBtn)) {
                    Watermark::moveItem(wmSelected, -1);
                    if (wmSelected > 0)
                        --wmSelected;
                }
                ImGui::SameLine();
                if (ImGui::Button("Move Down", halfBtn)) {
                    Watermark::moveItem(wmSelected, 1);
                    if (wmSelected < Watermark::itemCount - 1)
                        ++wmSelected;
                }
                if (ImGui::Button("Reset Items", ImVec2(colW, ui::s(24)))) {
                    Watermark::resetItems();
                    wmSelected = 0;
                }
            },
            "Selected Item",
            []() {
                if (wmSelected < 0 || wmSelected >= Watermark::itemCount) {
                    ImGui::TextDisabled("Add an item to edit it.");
                    return;
                }
                Watermark::Item& it = Watermark::items[wmSelected];

                const char* kinds[Watermark::KindCount];
                for (int k = 0; k < Watermark::KindCount; ++k)
                    kinds[k] = Watermark::kindName(k);

                ui::Checkbox("Item Enabled", &it.enabled);
                ui::Combo("Kind", &it.kind, kinds, Watermark::KindCount);

                const bool isDynamic = (it.kind == Watermark::KindFps || it.kind == Watermark::KindClock ||
                                        it.kind == Watermark::KindUptime);
                if (it.kind == Watermark::KindText) {
                    ImGui::InputText("Text", it.text, sizeof(it.text));
                } else if (isDynamic) {
                    
                    ImGui::InputTextWithHint("Label", "optional, e.g. FPS", it.text, sizeof(it.text));
                    ui::Checkbox("Label First", &it.labelBefore);
                } else if (it.kind == Watermark::KindSpacer) {
                    ui::SliderFloat("Length", &it.size, 0.0f, 64.0f, "%.0f px");
                } else if (it.kind == Watermark::KindImage) {
                    ImGui::TextDisabled("Configured under Image.");
                }

                
                
                if (it.kind != Watermark::KindSpacer && it.kind != Watermark::KindImage) {
                    const char* colorModes[] = {"Primary", "Secondary", "Accent", "Rainbow", "Custom"};
                    ui::Combo("Color", &it.colorMode, colorModes, Watermark::ColorModeCount);
                    if (it.colorMode == Watermark::ColorCustom)
                        ui::color("Custom Color", it.color, true);
                }
                if (it.kind == Watermark::KindText || isDynamic) {
                    const char* weights[] = {"Primary", "Secondary"};
                    ui::Combo("Weight", &it.weight, weights, 2);
                }

                
                
                bool customGap = (it.gap >= 0.0f);
                if (ui::Checkbox("Custom Gap", &customGap))
                    it.gap = customGap ? Watermark::itemGap : -1.0f;
                if (customGap)
                    ui::SliderFloat("Gap After", &it.gap, 0.0f, 48.0f, "%.0f px");
            });
        ui::two_column(
            "Text & Colors",
            []() {
                
                
                hudFontCombo("Primary Font", &Watermark::fontPrimary);
                hudFontCombo("Secondary Font", &Watermark::fontSecondary);
                
                
                
                
                ui::SliderFloat("Font Size", &Watermark::fontSize, 8.0f, 40.0f, "%.0f px", true);
                ui::SliderFloat("Secondary Ratio", &Watermark::secondaryRatio, 0.5f, 1.2f, "%.2fx");
                ui::SliderFloat("Scale", &Watermark::scale, 0.4f, 2.5f, "%.2fx", true);
                ui::SliderFloat("Opacity", &Watermark::opacity, 0.05f, 1.0f, "%.2f");

                ImGui::Spacing();
                ui::color("Primary Color", Watermark::primaryColor, true);
                ui::color("Secondary Color", Watermark::secondaryColor, true);
                const char* rbTypes[] = {"Flow", "Solid", "Fast", "Wide"};
                ui::Combo("Rainbow Type", &Watermark::rainbowType, rbTypes, 4);
                ui::SliderFloat("Rainbow Saturation", &Watermark::rainbowSaturation, 0.0f, 1.0f, "%.2f");

                ImGui::Spacing();
                const char* axes[] = {"Horizontal", "Vertical"};
                ui::Combo("Layout", &Watermark::layout, axes, 2);
                const char* aligns[] = {"Start", "Center", "End"};
                ui::Combo("Align", &Watermark::align, aligns, 3);
                ui::SliderFloat("Item Gap", &Watermark::itemGap, 0.0f, 48.0f, "%.0f px");
                ui::SliderFloat("Label Gap", &Watermark::labelGap, 0.0f, 20.0f, "%.0f px");
            },
            "Card",
            []() {
                ui::Checkbox("Background", &Watermark::showBackground);
                if (Watermark::showBackground) {
                    ui::color("Background Color", Watermark::backgroundColor, true);
                    ui::Checkbox("Gradient", &Watermark::backgroundGradient);
                    if (Watermark::backgroundGradient) {
                        ui::color("Gradient Color", Watermark::backgroundColor2, true);
                        ui::Checkbox("Vertical Gradient", &Watermark::gradientVertical);
                    }
                }

                ui::Checkbox("Border", &Watermark::showBorder);
                if (Watermark::showBorder) {
                    ui::color("Border Color", Watermark::borderColor, true);
                    ui::SliderFloat("Border Width", &Watermark::borderWidth, 0.5f, 4.0f, "%.1f px");
                }

                
                
                ui::SliderFloat("Corner Radius", &Watermark::rounding, 0.0f, 20.0f, "%.0f px");
                ui::SliderFloat("Padding X", &Watermark::paddingX, 0.0f, 40.0f, "%.0f px");
                ui::SliderFloat("Padding Y", &Watermark::paddingY, 0.0f, 30.0f, "%.0f px");

                ImGui::Spacing();
                ui::Checkbox("Glow", &Watermark::showGlow);
                if (Watermark::showGlow) {
                    ui::Checkbox("Glow Uses Accent", &Watermark::glowUseAccent);
                    if (!Watermark::glowUseAccent)
                        ui::color("Glow Color", Watermark::glowColor, true);
                    ui::SliderFloat("Glow Size", &Watermark::glowSize, 0.0f, 24.0f, "%.0f px");
                }
            });
        ui::two_column(
            "Accent Bar",
            []() {
                ui::Checkbox("Accent Bar", &Watermark::showAccentBar);
                if (Watermark::showAccentBar) {
                    const char* sides[] = {"Left", "Right", "Top", "Bottom"};
                    ui::Combo("Bar Side", &Watermark::barSide, sides, 4);
                    const char* colorModes[] = {"Primary", "Secondary", "Accent", "Rainbow", "Custom"};
                    ui::Combo("Bar Color", &Watermark::barColorMode, colorModes, Watermark::ColorModeCount);
                    if (Watermark::barColorMode == Watermark::ColorCustom)
                        ui::color("Bar Custom Color", Watermark::barColor, true);
                    ui::SliderFloat("Bar Thickness", &Watermark::barWidth, 1.0f, 12.0f, "%.0f px");
                    
                    
                    ui::SliderFloat("Bar Gap", &Watermark::barGap, 0.0f, 24.0f, "%.0f px");
                    ui::SliderFloat("Bar Inset", &Watermark::barInset, 0.0f, 0.45f, "%.2f");
                    ui::SliderFloat("Bar Rounding", &Watermark::barRounding, 0.0f, 8.0f, "%.0f px");
                }
            },
            "Effects",
            []() {
                
                
                const char* fx[] = {"None", "Shadow", "Outline"};
                ui::Combo("Text Effect", &Watermark::textEffect, fx, 3);
                if (Watermark::textEffect != Watermark::EffectNone) {
                    ui::color("Effect Color", Watermark::effectColor, true);
                    ui::SliderFloat("Effect Size", &Watermark::effectSize, 0.5f, 4.0f, "%.1f px");
                }

                
                
                
                if (Watermark::hasKind(Watermark::KindSeparator)) {
                    ImGui::Spacing();
                    ui::color("Separator Color", Watermark::separatorColor, true);
                    ui::SliderFloat("Separator Thickness", &Watermark::separatorThickness, 0.5f, 4.0f, "%.1f px");
                    ui::SliderFloat("Separator Length", &Watermark::separatorLength, 0.05f, 1.0f, "%.2f");
                }
                if (Watermark::hasKind(Watermark::KindClock)) {
                    ImGui::Spacing();
                    ui::Checkbox("24-Hour Clock", &Watermark::clock24h);
                    ui::Checkbox("Clock Seconds", &Watermark::clockSeconds);
                }
                if (Watermark::hasKind(Watermark::KindUptime)) {
                    ImGui::Spacing();
                    ui::Checkbox("Uptime Hours", &Watermark::uptimeHours);
                }
            });
        ui::two_column(
            "Image",
            []() {
                
                
                
                const float colW = ImGui::CalcItemWidth();
                const ImVec2 fullBtn(colW, ui::s(24));
                const float halfW = (colW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                const ImVec2 halfBtn(halfW, ui::s(24));

                const bool busy = Watermark::pickerBusy();
                ImGui::BeginDisabled(busy);
                if (ImGui::Button(busy ? "Choosing..." : "Browse...", fullBtn))
                    Watermark::browseForImage();
                ImGui::EndDisabled();
                if (ImGui::Button("Open Folder", halfBtn))
                    Watermark::revealImageFolder();
                ImGui::SameLine();
                if (ImGui::Button("Clear", halfBtn)) {
                    Watermark::imagePath[0] = '\0';
                    Watermark::requestImageReload();
                }
                ImGui::InputTextWithHint("Path", "pick a file, or paste a path", Watermark::imagePath,
                                         sizeof(Watermark::imagePath));
                if (ImGui::Button("Reload", fullBtn))
                    Watermark::requestImageReload();
                ImGui::TextDisabled("%s", Watermark::imageStatus());

                ImGui::Spacing();
                ui::SliderFloat("Image Size", &Watermark::imageScale, 0.1f, 4.0f, "%.2fx", true);
                ui::SliderFloat("Image Opacity", &Watermark::imageOpacity, 0.0f, 1.0f, "%.2f");
                ui::SliderFloat("Image Rounding", &Watermark::imageRounding, 0.0f, 24.0f, "%.0f px");
                ui::color("Image Tint", Watermark::imageTint, true);
                if (Watermark::imageAnimated())
                    ui::SliderFloat("GIF Speed", &Watermark::imageAnimationSpeed, 0.1f, 4.0f, "%.2fx");
                if (!Watermark::hasKind(Watermark::KindImage))
                    ImGui::TextDisabled("Add an Image item to draw it.");
            },
            "Position",
            []() {
                
                
                const char* corners[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"};
                if (ui::Combo("Corner", &Watermark::anchor, corners, 4))
                    Watermark::userPlaced = false;
                ui::SliderFloat("Margin", &Watermark::margin, 0.0f, 60.0f, "%.0f px");
                if (ImGui::Button("Reset Position", ImVec2(ImGui::CalcItemWidth(), ui::s(24))))
                    Watermark::resetPosition();

                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("Drag the Watermark directly on screen to move it.");
                ImGui::PopStyleColor();
            });
    } else if (ui::cur_tab == 1 && t.cur_subtab == 12) {
        
        
        ui::SetDescriptionContext("ClickGUI",  true);
        ui::section("Appearance", []() {
            const char* modes[] = {"Old Renewed", "New"};
            ui::Combo("Mode", &ui::gui_mode, modes, 2);
        });
    }
    
    
    else {
        const char* msg = "Couldn't load this page.";
        float width = ImGui::GetContentRegionAvail().x;
        ImVec2 msgSize = ImGui::CalcTextSize(msg);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SetCursorPosX((width - msgSize.x) / 2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        ImGui::Text(msg);
        ImGui::PopStyleColor();
    }

    
    ImGui::PopItemWidth();
    ImGui::Unindent(bodyMargin);
}


static void renderPage() {
    if (ui::cur_tab >= ui::tabs.size())
        return;
    auto& tab = ui::tabs[ui::cur_tab];

    
    const int settingsIdx = (int)ui::tabs.size() - 1;
    const int configsIdx = settingsIdx - 1;
#if COBBLE_SPECTATE_ENABLED
    const int spectateIdx = settingsIdx - 2;
#endif

    
    static int s_prevTab = -1;
    if (s_prevTab != ui::cur_tab && ui::cur_tab == configsIdx) {
        g_configsNeedsRefresh = true;
    }
    s_prevTab = ui::cur_tab;

    if (ui::cur_tab == settingsIdx) {
        ui::SetDescriptionContext("Settings");
        renderSettingsPage();
        return;
    }
#if COBBLE_SPECTATE_ENABLED
    if (ui::cur_tab == spectateIdx) {
        ui::SetDescriptionContext("Spectate");
        renderSpectatePage();
        return;
    }
#endif
    if (ui::cur_tab == configsIdx) {
        ui::SetDescriptionContext("Configs");
        renderConfigsPage();
        return;
    }

    
    
    
    
    
    
    
    if (tab.cur_subtab >= tab.m_subtabs.size())
        return;

    if (ui::tabs[ui::cur_tab].cur_view == ui::CategoryView::List)
        renderModuleList(ui::cur_tab);
    else
        renderModuleConfig(ui::cur_tab);
}

void Menu::ApplyStyle() {
    ui::apply_style();
}









static constexpr float kOpenDuration = 0.27f;
static float g_openT = 0.0f;

static float easeOutExpo(float t) {
    t = ImClamp(t, 0.0f, 1.0f);
    return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -9.5f * t);
}


static float easeOutBack(float t) {
    const float c = 1.52f;
    const float u = ImClamp(t, 0.0f, 1.0f) - 1.0f;
    return 1.0f + (c + 1.0f) * u * u * u + c * u * u;
}


static float staggered(float t, float delay) {
    return easeOutExpo(ImClamp((t - delay) / ImMax(0.0001f, 1.0f - delay), 0.0f, 1.0f));
}





static void springTo(float& value, float& velocity, float target, float dt, float stiffness = 300.0f,
                     float damping = 27.0f) {
    const float step = ImMin(dt, 1.0f / 60.0f);
    velocity += ((target - value) * stiffness - velocity * damping) * step;
    value += velocity * step;
}







static void renderClickGuiBody(float openAnim, bool nu, float pop, float contentT) {
    ImGuiIO& io = ImGui::GetIO();

    
    
    
    
    
    const ImU32 panelBg = nu ? IM_COL32(12, 13, 15, 214) : IM_COL32(13, 14, 17, 234);
    const ImU32 recessBg = nu ? IM_COL32(7, 8, 9, 222) : IM_COL32(9, 10, 12, 240);

    
    ui::size = ui::s(ui::baseSize);

    
    
    
    
    const float slide = (1.0f - pop) * ui::s(26);

    
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f + slide), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ui::size);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, openAnim);

    ImGui::Begin("Cobble", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    {
        ImDrawList* windowDraw = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowMax = windowPos + ImGui::GetWindowSize();
        const float rounding = GImGui->Style.WindowRounding;

        
        ImVec2 leftPanelMin{windowPos.x, windowPos.y};
        ImVec2 leftPanelMax{windowPos.x + ui::s(176), windowMax.y};
        windowDraw->AddRectFilled(leftPanelMin, leftPanelMax, panelBg, rounding);
        windowDraw->AddRect(leftPanelMin, leftPanelMax, ui::GetColor(ImGuiCol_Border, 1.0f), rounding, 0, ui::s(1.0f));
        
        
        auto traceLine = [&](float x0, float x1, float y, float alpha) {
            const float mid = (x0 + x1) * 0.5f;
            const float half = (x1 - x0) * 0.5f * ImClamp(pop, 0.0f, 1.0f);
            if (half <= 0.5f)
                return;
            windowDraw->AddLine(ImVec2(mid - half, y), ImVec2(mid + half, y), GetSchemeColor(alpha), ui::s(1.0f));
        };
        traceLine(leftPanelMin.x + rounding, leftPanelMax.x - rounding, leftPanelMin.y + ui::s(1), 0.28f);

        ImVec2 rightPanelMin{windowPos.x + ui::s(185), windowPos.y};
        ImVec2 rightPanelMax{windowMax.x, windowMax.y};
        windowDraw->AddRectFilled(rightPanelMin, rightPanelMax, panelBg, rounding);
        windowDraw->AddRect(rightPanelMin, rightPanelMax, ui::GetColor(ImGuiCol_Border, 1.0f), rounding, 0,
                            ui::s(1.0f));
        traceLine(rightPanelMin.x + rounding, rightPanelMax.x - rounding, rightPanelMin.y + ui::s(1), 0.28f);

        
        
        
        const ImU32 lip = IM_COL32(255, 255, 255, (int)(16.0f * openAnim));
        windowDraw->AddLine(ImVec2(leftPanelMin.x + rounding, leftPanelMin.y + ui::s(2.5f)),
                            ImVec2(leftPanelMax.x - rounding, leftPanelMin.y + ui::s(2.5f)), lip, ui::s(1.0f));
        windowDraw->AddLine(ImVec2(rightPanelMin.x + rounding, rightPanelMin.y + ui::s(2.5f)),
                            ImVec2(rightPanelMax.x - rounding, rightPanelMin.y + ui::s(2.5f)), lip, ui::s(1.0f));

        ui::nav_sel_valid = false;

        
        ImGui::BeginChild("nav", {ui::s(176), ImGui::GetWindowHeight()}, 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            
            ImGui::BeginChild("brand", {ImGui::GetWindowWidth(), ui::s(42)}, 0, ImGuiWindowFlags_NoBackground);
            {
                ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(),
                                                          {ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                                                           ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - 2},
                                                          ui::GetColor(ImGuiCol_ChildBg), GImGui->Style.WindowRounding,
                                                          ImDrawFlags_RoundCornersTop);
                
                
                {
                    const float ulY = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();
                    const float ulMid = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() * 0.5f;
                    const float ulHalf = ImGui::GetWindowWidth() * 0.5f * ImClamp(pop, 0.0f, 1.0f);
                    ImGui::GetWindowDrawList()->AddRectFilled({ulMid - ulHalf, ulY - 2}, {ulMid + ulHalf, ulY},
                                                              GetSchemeColor());
                }

                const char* brand = "Cobble";
                ImVec2 brandSize = font(3)->CalcTextSizeA(font(3)->FontSize, FLT_MAX, 0.f, brand);
                ImGui::GetWindowDrawList()->AddText(
                    font(3), font(3)->FontSize,
                    {ImGui::GetWindowPos().x + ImGui::GetWindowWidth() / 2 - brandSize.x / 2,
                     ImGui::GetWindowPos().y + ImGui::GetWindowHeight() / 2 - brandSize.y / 2 - 2},
                    GetSchemeColor(), brand);
            }
            ImGui::EndChild();

            
            ImGui::SetCursorPosY(ui::s(50));
            ImGui::BeginChild("tabs", {ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ui::s(50)}, 0,
                              ImGuiWindowFlags_NoBackground);
            {
                
                
                ImDrawList* navDraw = ImGui::GetWindowDrawList();
                ImVec2 panelMin = ImGui::GetWindowPos();
                ImVec2 panelMax = panelMin + ImGui::GetWindowSize();
                navDraw->AddRectFilled(panelMin, panelMax, recessBg, GImGui->Style.WindowRounding,
                                       ImDrawFlags_RoundCornersBottom);
                
                
                float shadowH = ui::s(20);
                navDraw->AddRectFilledMultiColor(panelMin, ImVec2(panelMax.x, panelMin.y + shadowH),
                                                 ImGui::GetColorU32(ImVec4(0, 0, 0, 0.45f)),
                                                 ImGui::GetColorU32(ImVec4(0, 0, 0, 0.45f)),
                                                 ImGui::GetColorU32(ImVec4(0, 0, 0, 0.0f)),
                                                 ImGui::GetColorU32(ImVec4(0, 0, 0, 0.0f)));

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, 1});

                
                
                const int tabCount = (int)ui::tabs.size();
                const int settingsIdx = tabCount - 1;
                const float rowH = ui::s(38);   
                const float topPad = ui::s(8);  
                const float botPad = ui::s(8);  
                const float startY = ImGui::GetCursorPosY();
                const float availH = ImGui::GetContentRegionAvail().y;

                ImGui::Dummy({0, topPad});
                for (int i = 0; i < settingsIdx; ++i) {
                    ui::tab(i);
                }

                
                
                const float settingsY = startY + availH - rowH - botPad;
                if (settingsY > ImGui::GetCursorPosY()) {
                    ImGui::SetCursorPosY(settingsY);
                }
                ui::tab(settingsIdx);

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        
        
        
        
        
        
        if (ui::nav_sel_valid) {
            static float railY = 0.0f, railH = 0.0f, railYVel = 0.0f, railHVel = 0.0f;
            static bool railInit = false;

            const float inset = ui::s(9);
            const float targetY = ui::nav_sel_min.y + inset;
            const float targetH = ImMax(ui::s(6), (ui::nav_sel_max.y - ui::nav_sel_min.y) - inset * 2.0f);

            if (!railInit || openAnim < 0.999f) {
                
                
                
                
                railY = targetY;
                railH = targetH;
                railYVel = railHVel = 0.0f;
                railInit = true;
            } else {
                springTo(railY, railYVel, targetY, io.DeltaTime);
                springTo(railH, railHVel, targetH, io.DeltaTime, 340.0f, 30.0f);
            }

            
            
            
            const float smear = ImClamp(fabsf(railYVel) * 0.030f, 0.0f, ui::s(14));
            float y0 = railY - (railYVel < 0.0f ? smear : 0.0f);
            float y1 = railY + railH + (railYVel > 0.0f ? smear : 0.0f);

            const float railX = leftPanelMin.x + ui::s(4);
            const float railW = ui::s(3);
            windowDraw->AddRectFilled({railX, y0}, {railX + railW, y1}, GetSchemeColor(0.95f), railW * 0.5f);

            
            
            const float glowW = ui::s(46);
            const ImU32 glowIn = GetSchemeColor(0.17f);
            const ImU32 glowOut = GetSchemeColor(0.0f);
            windowDraw->AddRectFilledMultiColor({railX + railW, y0}, {railX + railW + glowW, y1}, glowIn, glowOut,
                                                glowOut, glowIn);
        }

        
        
        {
            float seamX = windowPos.x + ui::s(185);
            float seamTop = windowPos.y + ui::s(8);
            float seamBot = windowMax.y - ui::s(8);
            windowDraw->AddRectFilledMultiColor(
                {seamX, seamTop}, {seamX + ui::s(12), seamBot},
                ImGui::GetColorU32(ImVec4(0, 0, 0, 0.32f)), ImGui::GetColorU32(ImVec4(0, 0, 0, 0.0f)),
                ImGui::GetColorU32(ImVec4(0, 0, 0, 0.0f)), ImGui::GetColorU32(ImVec4(0, 0, 0, 0.32f)));
        }

        
        ImGui::SetCursorPos({ui::s(185), 0});
        ImGui::BeginChild("main", ImGui::GetContentRegionAvail(), 0, ImGuiWindowFlags_NoBackground);
        {
            
            ImGui::BeginChild("main header", {ImGui::GetWindowWidth(), ui::s(42)}, 0, ImGuiWindowFlags_NoBackground);
            {
                ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(),
                                                          {ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                                                           ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - 2},
                                                          ui::GetColor(ImGuiCol_ChildBg), GImGui->Style.WindowRounding,
                                                          ImDrawFlags_RoundCornersTop);
                
                
                {
                    const float ulY = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();
                    const float ulX = ImGui::GetWindowPos().x;
                    const float ulW = ImGui::GetWindowWidth() * ImClamp(pop, 0.0f, 1.0f);
                    ImGui::GetWindowDrawList()->AddRectFilled({ulX, ulY - 2}, {ulX + ulW, ulY}, GetSchemeColor());
                }

                
                const char* name = ui::tabs[ui::cur_tab].m_name;
                ImFont* titleFont = font(3);
                ImFont* subFont = font(2);

                float centerY = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() * 0.5f;
                float titleHeight = titleFont->FontSize;
                float baselineY = centerY - titleHeight * 0.5f;

                ImGui::GetWindowDrawList()->AddText(titleFont, titleFont->FontSize,
                                                    {ImGui::GetWindowPos().x + ui::s(20), baselineY},
                                                    ImGui::GetColorU32(ImGuiCol_TextDisabled, ui::anim), name);

                
                
                
                
                const int settingsIdx = (int)ui::tabs.size() - 1;
                const int configsIdx = settingsIdx - 1;
                const bool isSpecial = (ui::cur_tab == settingsIdx || ui::cur_tab == configsIdx);
                const ui::tab_t& curTab = ui::tabs[ui::cur_tab];
                if (!isSpecial && curTab.cur_view == ui::CategoryView::Config &&
                    curTab.cur_subtab < (int)curTab.m_subtabs.size()) {
                    float nameWidth = titleFont->CalcTextSizeA(titleFont->FontSize, FLT_MAX, 0.f, name).x;

                    ImGui::RenderArrow(ImGui::GetWindowDrawList(),
                                       {ImGui::GetWindowPos().x + ui::s(27) + nameWidth, centerY - ui::s(5)},
                                       ImGui::GetColorU32(ImGuiCol_Text, ui::anim * ui::content_anim), ImGuiDir_Right,
                                       ui::s(12));

                    const char* subtabName = curTab.m_subtabs[curTab.cur_subtab];

                    ImGui::GetWindowDrawList()->AddText(subFont, subFont->FontSize,
                                                        {ImGui::GetWindowPos().x + ui::s(40) + nameWidth, baselineY},
                                                        GetSchemeColor(ui::anim * ui::content_anim), subtabName);
                }
            }
            ImGui::EndChild();

            
            ImGui::GetWindowDrawList()->AddRectFilled({ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + ui::s(42)},
                                                      ImGui::GetWindowPos() + ImGui::GetWindowSize(), panelBg,
                                                      GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersBottom);

            
            
            
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ui::s(10), ui::s(13)});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {ui::s(15), ui::s(15)});

            
            
            
            
            float contentAlpha = contentT * ui::anim * ui::content_anim;
            ui::alpha_mod = contentAlpha;
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, contentAlpha);

            ImGui::SetCursorPosY(ui::s(42));
            ImGui::BeginChild("content", ImGui::GetContentRegionAvail(), 0,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding);
            { renderPage(); }
            ImGui::EndChild();
            ImGui::PopStyleVar(3);

            
            ui::alpha_mod = openAnim;
        }
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}







static void drawBlurBehind(ImDrawList* dl, const ImVec2& wpos, const ImVec2& wsize, float round) {
    if (wsize.x < 4.0f || wsize.y < 4.0f)
        return;

    const int sw = RenderData::screenWidth;
    const int sh = RenderData::screenHeight;
    if (sw < 4 || sh < 4)
        return;

    
    int rx = static_cast<int>(wpos.x);
    int ry = static_cast<int>(wpos.y);
    int rw = static_cast<int>(wsize.x);
    int rh = static_cast<int>(wsize.y);
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > sw) rw = sw - rx;
    if (ry + rh > sh) rh = sh - ry;
    if (rw < 4 || rh < 4)
        return;

    static GLuint blurTex = 0;
    if (blurTex == 0) {
        glGenTextures(1, &blurTex);
        if (blurTex == 0)
            return;
    }

    
    
    const int fbY = sh - (ry + rh);
    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glBindTexture(GL_TEXTURE_2D, blurTex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rx, fbY, rw, rh, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));
    while (glGetError() != GL_NO_ERROR) {
    }  

    const ImTextureID tid = (ImTextureID)(intptr_t)blurTex;
    const ImVec2 pmin(static_cast<float>(rx), static_cast<float>(ry));
    const ImVec2 pmax(static_cast<float>(rx + rw), static_cast<float>(ry + rh));

    
    const float bpx = ui::s(2.0f);
    const float du = bpx / static_cast<float>(rw);
    const float dv = bpx / static_cast<float>(rh);
    static const float offs[13][2] = {{0, 0},  {1, 0},   {-1, 0}, {0, 1},  {0, -1}, {1, 1},  {1, -1},
                                      {-1, 1}, {-1, -1}, {2, 0},  {-2, 0}, {0, 2},  {0, -2}};
    for (int i = 0; i < 13; ++i) {
        const float ox = offs[i][0] * du;
        const float oy = offs[i][1] * dv;
        
        const ImVec2 uv0(0.0f + ox, 1.0f - oy);
        const ImVec2 uv1(1.0f + ox, 0.0f - oy);
        const int a = static_cast<int>(255.0f / static_cast<float>(i + 1));
        dl->AddImageRounded(tid, pmin, pmax, uv0, uv1, IM_COL32(255, 255, 255, a), round);
    }
}

static void renderClickGuiNew(float openAnim, float pop, float contentT) {
    ImGuiIO& io = ImGui::GetIO();
    ui::size = ui::s(ui::baseSize);

    
    
    const float slide = (1.0f - pop) * ui::s(22);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f + slide), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ui::size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, openAnim);
    ImGui::Begin("Cobble", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        const ImVec2 wpos = ImGui::GetWindowPos();
        const ImVec2 wsize = ImGui::GetWindowSize();
        const ImVec2 wmax = wpos + wsize;
        const float round = ui::s(14);
        const float topH = ui::s(52);
        const float pad = ui::s(22);
        const float cy = wpos.y + topH * 0.5f;

        
        
        
        drawBlurBehind(dl, wpos, wsize, round);
        dl->AddRectFilled(wpos, wmax, IM_COL32(0, 0, 0, 175), round);
        dl->AddRect(wpos, wmax, IM_COL32(255, 255, 255, 13), round, 0, ui::s(1.0f));

        
        
        ImFont* brandFont = font(3);
        const char* brand = "Cobble";
        ImVec2 bsz = brandFont->CalcTextSizeA(brandFont->FontSize, FLT_MAX, 0.f, brand);
        dl->AddRectFilled(ImVec2(wpos.x + pad, cy - bsz.y * 0.40f),
                          ImVec2(wpos.x + pad + ui::s(3), cy + bsz.y * 0.40f), GetSchemeColor(), ui::s(1.5f));
        const float brandX = wpos.x + pad + ui::s(13);
        const ImVec2 brandPos(brandX, cy - bsz.y * 0.5f);
        dl->AddText(brandFont, brandFont->FontSize, brandPos, IM_COL32(244, 245, 249, 255), brand);

        struct new_cat_state {
            float hover;
            float sel;
        };

        ImFont* catFont = font(2);
        const float catFS = catFont->FontSize * 0.9f;
        
        
        float x = brandX + bsz.x + ui::s(14);
        const int tabCount = (int)ui::tabs.size();
        static float indX = 0.0f, indW = 0.0f, indInit = 0.0f;
        float selX = indX, selW = indW;

        for (int i = 0; i < tabCount; ++i) {
            const char* nm = ui::tabs[i].m_name;
            const char* ic = ui::tabs[i].m_icon;
            ImVec2 nsz = catFont->CalcTextSizeA(catFS, FLT_MAX, 0.f, nm);
            ImVec2 isz = font(0)->CalcTextSizeA(catFS, FLT_MAX, 0.f, ic);
            const float inner = isz.x + ui::s(7) + nsz.x;
            const float bw = inner + ui::s(20);

            ImRect bb(ImVec2(x, wpos.y + ui::s(7)), ImVec2(x + bw, wpos.y + topH - ui::s(7)));
            std::string key = std::string("ncat_") + nm;
            ImGuiID id = win->GetID(key.c_str());
            ImGui::ItemAdd(bb, id);
            bool hov = false, held = false;
            bool pressed = ImGui::ButtonBehavior(bb, id, &hov, &held);
            bool selected = ui::next_tab == i;
            if (pressed) {
                if (!selected) {
                    ui::next_tab = i;
                    ui::anim_dst = 0.f;
                } else if (i == ui::cur_tab && ui::tabs[i].cur_view == ui::CategoryView::Config) {
                    ui::tabs[i].next_view = ui::CategoryView::List;
                    ui::content_anim_dst = 0.f;
                }
            }

            auto& st = ui::anim_obj(key.c_str(), 0, new_cat_state{0.f, 0.f});
            st.hover = ui::animate(st.hover, 0.f, 1.f, hov);
            st.sel = ui::animate(st.sel, 0.f, 1.f, selected);

            
            
            
            
            
            
            
            
            
            
            if (ImMax(st.hover, st.sel) > 0.01f) {
                const float inset = ui::s(3);
                dl->AddRectFilled(ImVec2(bb.Min.x + inset, bb.Min.y), ImVec2(bb.Max.x - inset, bb.Max.y),
                                  GetSchemeColor(0.13f * ImMax(st.sel, st.hover * 0.6f)), ui::s(8));
            }

            ImU32 col = ui::col_anim(
                ui::col_anim(ui::GetColor(ImGuiCol_TextDisabled), ui::GetColor(ImGuiCol_Text, 0.85f), st.hover),
                IM_COL32(244, 245, 249, 255), st.sel);

            const float bx = x + (bw - inner) * 0.5f;
            dl->AddText(font(0), catFS, ImVec2(bx, cy - isz.y * 0.5f), col, ic);
            dl->AddText(catFont, catFS, ImVec2(bx + isz.x + ui::s(7), cy - nsz.y * 0.5f), col, nm);

            if (selected) {
                selX = bx;
                selW = inner;
            }
            x += bw;
        }

        static float indXVel = 0.0f, indWVel = 0.0f;
        if (indInit < 0.5f) {
            indX = selX;
            indW = selW;
            indXVel = indWVel = 0.0f;
            indInit = 1.0f;
        } else {
            springTo(indX, indXVel, selX, io.DeltaTime);
            springTo(indW, indWVel, selW, io.DeltaTime, 340.0f, 30.0f);
        }

        
        
        
        const float lead = ImClamp(fabsf(indXVel) * 0.045f, 0.0f, ui::s(26));
        const float ux0 = indX - (indXVel < 0.0f ? lead : 0.0f);
        const float ux1 = indX + indW + (indXVel > 0.0f ? lead : 0.0f);
        const float uy = wpos.y + topH;

        
        dl->AddRectFilled(ImVec2(ux0 - ui::s(3), uy - ui::s(6)), ImVec2(ux1 + ui::s(3), uy - ui::s(1)),
                          GetSchemeColor(0.16f), ui::s(3));
        dl->AddRectFilled(ImVec2(ux0, uy - ui::s(5)), ImVec2(ux1, uy - ui::s(2)), GetSchemeColor(), ui::s(1.5f));

        
        
        
        
        
        
        
        
        
        
        {
            const float washH = ui::s(60);
            const float fade = ui::s(32);
            const float wx0 = ux0 - fade, wx1 = ux1 + fade;
            const ImU32 nil = GetSchemeColor(0.0f);
            auto envelope = [&](float px) {
                if (px <= ux0)
                    return ImSaturate((px - wx0) / fade);
                if (px >= ux1)
                    return ImSaturate((wx1 - px) / fade);
                return 1.0f;
            };
            const int slices = ImClamp((int)((wx1 - wx0) / ui::s(6)), 8, 96);
            const float step = (wx1 - wx0) / (float)slices;
            float px = wx0;
            for (int i = 0; i < slices; ++i) {
                const float nx = px + step;
                dl->AddRectFilledMultiColor(ImVec2(px, uy), ImVec2(nx, uy + washH),
                                            GetSchemeColor(0.085f * envelope(px)),
                                            GetSchemeColor(0.085f * envelope(nx)), nil, nil);
                px = nx;
            }
        }

        
        
        {
            const float sepY = wpos.y + topH;
            const float midX = wpos.x + wsize.x * 0.5f;
            const ImU32 sepC = IM_COL32(255, 255, 255, 24);
            const ImU32 sep0 = IM_COL32(255, 255, 255, 0);
            dl->AddRectFilledMultiColor(ImVec2(wpos.x + round, sepY - ui::s(0.5f)), ImVec2(midX, sepY + ui::s(0.5f)),
                                        sep0, sepC, sepC, sep0);
            dl->AddRectFilledMultiColor(ImVec2(midX, sepY - ui::s(0.5f)), ImVec2(wmax.x - round, sepY + ui::s(0.5f)),
                                        sepC, sep0, sep0, sepC);
        }

        ImGui::SetCursorPos(ImVec2(0, topH));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ui::s(10), ui::s(13)});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {ui::s(20), ui::s(16)});
        
        
        float contentAlpha = contentT * ui::anim * ui::content_anim;
        ui::alpha_mod = contentAlpha;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, contentAlpha);
        ImGui::BeginChild("newcontent", ImVec2(wsize.x, wsize.y - topH), 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding);
        {
            renderPage();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ui::alpha_mod = openAnim;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Menu::Render() {
    
    ui::ResetBinderState();

    
    if (hidden && isOpen) {
        COBBLE_TRACE_ONCE("Menu::Render skip: hidden");
        return;
    }

    
    
    
    
    if (!isOpen) {
        openAnim = 0.f;
        g_openT = 0.f;
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    if (dt > (1.0f / 60.0f)) dt = 1.0f / 60.0f;
    g_openT = ImMin(1.0f, g_openT + dt / kOpenDuration);
    openAnim = easeOutExpo(g_openT);   
    const float pop = easeOutBack(g_openT);      
    const float contentT = staggered(g_openT, 0.30f);  

    
    ApplyStyle();
    ui::updateScale();

    
    
    
    
    
    
    
    
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        bg->AddRectFilled(ImVec2(0, 0), ds, IM_COL32(0, 0, 0, (int)(128 * openAnim)));
    }

    if (ui::gui_mode == 1)
        renderClickGuiNew(openAnim, pop, contentT);
    else
        renderClickGuiBody(openAnim, false, pop, contentT);

    ui::handle_anims();
}

void Menu::Hotkey(const char* label, Module* module) {
    ImGui::SameLine();
    ImGui::PushID(label);

    std::string text = (moduleToBind == module) ? "..." : "[?]";

    if (ImGui::SmallButton(text.c_str())) {
        moduleToBind = module;
    }

    ImGui::PopID();
}
