#include "inventorymanager.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../ui/hud/notifications.h"
#include "../../util/textures/item_textures.h"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

InventoryManager* inventoryManager = nullptr;

static void sendKey(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}



static void sendLwjglKey(int lwjglKeyCode) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = (WORD)lwjglKeyCode;
    inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = (WORD)lwjglKeyCode;
    inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

InventoryManager::InventoryManager() : Module("InvManager") {
    keybind = 0;
}

bool InventoryManager::canToggle() {
    return state == State::Idle;
}

bool InventoryManager::hasSavedLayout() const {
    return presets[settings.activePreset].saved;
}

int InventoryManager::inventoryToContainerSlot(int invSlot) {
    
    
    if (invSlot >= 0 && invSlot <= 8) {
        return 36 + invSlot;  
    } else if (invSlot >= 9 && invSlot <= 35) {
        return invSlot;  
    }
    return -1;
}

void InventoryManager::saveHotbar() {
    if (!mc.isValid() || !player.isValid()) {
        NotificationManager::addSystem("Cannot save, not in game");
        return;
    }
    
    auto slots = mc.getInventorySlots();
    
    if (slots.empty()) {
        NotificationManager::addSystem("Cannot save, inventory empty");
        return;
    }

    Preset& preset = presets[settings.activePreset];
    
    for (int i = 0; i < 9; i++) {
        preset.slots[i].itemId = 0;
        preset.slots[i].damage = 0;
    }

    for (const auto& slot : slots) {
        if (slot.slotId >= 0 && slot.slotId <= 8) {
            preset.slots[slot.slotId].itemId = slot.itemId;
            preset.slots[slot.slotId].damage = slot.damage;
        }
    }

    preset.saved = true;
    char msg[32];
    snprintf(msg, sizeof(msg), "Preset %d saved", settings.activePreset + 1);
    NotificationManager::addSystem(msg);
}

void InventoryManager::onEnable() {
    if (!mc.isValid() || !player.isValid()) {
        enabled = false;
        return;
    }
    
    
    if (state != State::Idle) {
        enabled = false;
        return;
    }
    
    
    if (mc.isInGui()) {
        enabled = false;
        return;
    }

    if (!presets[settings.activePreset].saved) {
        saveHotbar();
        enabled = false;
        return;
    }

    scanForSwaps();

    if (pendingSwaps.empty()) {
        NotificationManager::addSystem(lastScanMissingItems ? "Preset items missing in inventory" : "Hotbar already matches");
        enabled = false;
        return;
    }

    
    sendLwjglKey(mc.getInventoryKey());
    state = State::OpeningInventory;
    stateStartTime = std::chrono::steady_clock::now();
}

void InventoryManager::onDisable() {
    state = State::Idle;
    pendingSwaps.clear();
}

void InventoryManager::finishRestore() {
    if (mc.isInventoryOpen()) {
        sendKey(VK_ESCAPE);
    }
    state = State::Idle;
    pendingSwaps.clear();
    enabled = false;
}

void InventoryManager::onUpdate() {
    if (!player.isValid())
        return;

    if (state == State::Idle)
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stateStartTime).count();

    switch (state) {
        case State::Idle:
            break;

        case State::OpeningInventory: {
            if (mc.isPlayerInventoryOpen()) {
                state = State::Swapping;
                stateStartTime = now;
                lastSwapTime = now - std::chrono::milliseconds(effectiveDelay());
            } else if (elapsed > TIMEOUT_MS) {
                finishRestore();
            }
            break;
        }

        case State::Swapping: {
            if (!mc.isPlayerInventoryOpen()) {
                finishRestore();
                return;
            }

            if (pendingSwaps.empty()) {
                state = State::Closing;
                stateStartTime = now;
                return;
            }

            auto swapElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSwapTime).count();
            int delay = effectiveDelay();
            if (swapElapsed < delay)
                return;

            SwapTask task = pendingSwaps.front();
            pendingSwaps.erase(pendingSwaps.begin());

            int containerSlot = inventoryToContainerSlot(task.sourceSlot);
            
            
            if (containerSlot < 0 || task.targetHotbar < 0 || task.targetHotbar > 8) {
                lastSwapTime = now;
                break;
            }
            
            mc.windowClickSwap(containerSlot, task.targetHotbar);

            lastSwapTime = now;
            if (settings.retryOnFail) {
                lastSwap = task;
                state = State::Verifying;
                stateStartTime = now;
            }
            break;
        }

        case State::Verifying: {
            if (!mc.isPlayerInventoryOpen()) {
                finishRestore();
                return;
            }

            auto verifyElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stateStartTime).count();
            int verifyDelay = settings.retryOnFail ? (std::max)(VERIFY_DELAY_MS, effectiveDelay()) : VERIFY_DELAY_MS;
            if (verifyElapsed < verifyDelay)
                break;

            auto slots = mc.getInventorySlots();
            int currentId = 0;
            int currentDamage = 0;
            bool found = false;
            for (const auto& s : slots) {
                if (s.slotId == lastSwap.targetHotbar) {
                    currentId = s.itemId;
                    currentDamage = s.damage;
                    found = true;
                    break;
                }
            }

            bool ok = found && currentId == lastSwap.expectedId &&
                      (!lastSwap.compareDamage || currentDamage == lastSwap.expectedDamage);

            if (ok) {
                state = State::Swapping;
                stateStartTime = now;
            } else {
                if (lastSwap.retryCount < MAX_RETRIES) {
                    lastSwap.retryCount++;
                    pendingSwaps.insert(pendingSwaps.begin(), lastSwap);
                    state = State::Swapping;
                    stateStartTime = now;
                    lastSwapTime = now - std::chrono::milliseconds(effectiveDelay());
                } else {
                    state = State::Swapping;
                    stateStartTime = now;
                }
            }
            break;
        }

        case State::Closing: {
            if (elapsed >= effectiveDelay()) {
                finishRestore();
            }
            break;
        }
    }
}

void InventoryManager::scanForSwaps() {
    pendingSwaps.clear();
    lastScanMissingItems = false;

    auto slots = mc.getInventorySlots();
    if (slots.empty())
        return;

    struct CurrentSlot {
        int itemId = 0;
        int damage = 0;
    };
    CurrentSlot current[36] = {};

    for (const auto& slot : slots) {
        if (slot.slotId >= 0 && slot.slotId < 36) {
            current[slot.slotId].itemId = slot.itemId;
            current[slot.slotId].damage = slot.damage;
        }
    }

    const Preset& preset = presets[settings.activePreset];

    auto slotMatches = [&](int hotbarIdx) {
        int wantedId = preset.slots[hotbarIdx].itemId;
        int wantedDamage = preset.slots[hotbarIdx].damage;
        bool compareDamage = (wantedId == 373);

        if (wantedId == 0)
            return current[hotbarIdx].itemId == 0;

        if (current[hotbarIdx].itemId != wantedId)
            return false;

        if (compareDamage && current[hotbarIdx].damage != wantedDamage)
            return false;

        return true;
    };

    bool correct[9] = {};
    for (int i = 0; i < 9; i++) {
        correct[i] = slotMatches(i);
    }

    
    for (int targetSlot = 0; targetSlot < 9; targetSlot++) {
        if (correct[targetSlot])
            continue;

        int wantedId = preset.slots[targetSlot].itemId;
        int wantedDamage = preset.slots[targetSlot].damage;
        bool compareDamage = (wantedId == 373);

        auto matchesWanted = [&](const CurrentSlot& s) {
            if (s.itemId != wantedId)
                return false;
            if (compareDamage && s.damage != wantedDamage)
                return false;
            return true;
        };

        
        if (wantedId == 0) {
            if (current[targetSlot].itemId != 0)
                lastScanMissingItems = true;
            continue;
        }

        int sourceSlot = -1;

        
        for (int i = 9; i < 36; i++) {
            if (matchesWanted(current[i])) {
                sourceSlot = i;
                break;
            }
        }

        
        if (sourceSlot == -1) {
            for (int i = 0; i < 9; i++) {
                if (i == targetSlot)
                    continue;
                if (correct[i])  
                    continue;
                if (matchesWanted(current[i])) {
                    sourceSlot = i;
                    break;
                }
            }
        }

        if (sourceSlot == -1) {
            
            lastScanMissingItems = true;
            continue;
        }

        SwapTask task{};
        task.sourceSlot = sourceSlot;
        task.targetHotbar = targetSlot;
        task.expectedId = wantedId;
        task.expectedDamage = wantedDamage;
        task.compareDamage = compareDamage;
        task.retryCount = 0;
        pendingSwaps.push_back(task);

        
        std::swap(current[sourceSlot], current[targetSlot]);

        if (sourceSlot < 9)
            correct[sourceSlot] = slotMatches(sourceSlot);
        correct[targetSlot] = slotMatches(targetSlot);
    }
}


static const char* getItemShortName(int itemId) {
    switch (itemId) {
        
        case 268: return "WSw";
        case 272: return "StSw";
        case 267: return "ISw";
        case 276: return "DSw";
        case 283: return "GSw";
        
        
        case 271: return "WAx";
        case 275: return "StAx";
        case 258: return "IAx";
        case 279: return "DAx";
        case 286: return "GAx";
        
        
        case 270: return "WPi";
        case 274: return "StPi";
        case 257: return "IPi";
        case 278: return "DPi";
        case 285: return "GPi";
        
        
        case 269: return "WSh";
        case 273: return "StSh";
        case 256: return "ISh";
        case 277: return "DSh";
        case 284: return "GSh";
        
        
        case 310: return "DHel";
        case 311: return "DChst";
        case 312: return "DLeg";
        case 313: return "DBoot";
        
        
        case 306: return "IHel";
        case 307: return "IChst";
        case 308: return "ILeg";
        case 309: return "IBoot";
        
        
        case 314: return "GHel";
        case 315: return "GChst";
        case 316: return "GLeg";
        case 317: return "GBoot";
        
        
        case 298: return "LHel";
        case 299: return "LChst";
        case 300: return "LLeg";
        case 301: return "LBoot";
        
        
        case 302: return "CHel";
        case 303: return "CChst";
        case 304: return "CLeg";
        case 305: return "CBoot";
        
        
        case 373: return "Pot";
        case 438: return "SPot";  
        
        
        case 260: return "Apl";
        case 322: return "Gap";
        case 297: return "Brd";
        case 320: return "Pork";
        case 364: return "Stk";
        case 366: return "Chk";
        case 360: return "Mel";
        case 357: return "Cke";
        case 391: return "Car";
        case 396: return "GCar";
        case 392: return "Pto";
        case 393: return "BPto";
        case 400: return "Pie";
        
        
        case 261: return "Bow";
        case 262: return "Arr";
        
        
        case 368: return "Prl";
        case 381: return "Eye";
        
        
        case 325: return "Bkt";
        case 326: return "Wtr";
        case 327: return "Lva";
        case 335: return "Mlk";
        
        
        case 346: return "Rod";
        
        
        case 344: return "Egg";
        case 332: return "Snw";
        case 385: return "Fire";
        
        
        case 264: return "Dia";
        case 265: return "IIng";
        case 266: return "GIng";
        case 388: return "Emr";
        case 280: return "Stk";
        case 287: return "Str";
        case 263: return "Coal";
        case 331: return "Red";
        case 352: return "Bone";
        case 377: return "BRod";
        case 288: return "Fth";
        case 334: return "Lea";
        
        
        case 259: return "F&S";
        case 345: return "Comp";
        case 359: return "Shr";
        case 384: return "XP";
        
        
        case 1: return "Stn";
        case 2: return "Grs";
        case 3: return "Dir";
        case 4: return "Cob";
        case 5: return "Plk";
        case 7: return "Bed";
        case 12: return "Snd";
        case 13: return "Grv";
        case 17: return "Log";
        case 18: return "Lvs";
        case 20: return "Gls";
        case 24: return "Sstn";
        case 45: return "Brk";
        case 48: return "Mss";
        case 49: return "Obs";
        case 87: return "Nrk";
        case 98: return "StBr";
        case 112: return "NBrk";
        case 121: return "End";
        case 155: return "Qrz";
        case 170: return "Hay";
        
        
        case 54: return "Chst";
        case 61: return "Frnc";
        case 58: return "Crft";
        case 47: return "Book";
        case 116: return "Ench";
        case 130: return "EChst";
        case 145: return "Anvl";
        case 379: return "Brew";
        
        default:
            if (itemId > 0) return "?";
            return "";
    }
}

void InventoryManager::onRenderMenu() {
    ui::section("Settings", [this]() {
        ui::Binder("Keybind", &keybind);
        
        ImGui::Spacing();
        
        int delay = settings.swapDelay;
        if (ui::SliderInt("Click Delay (ms)", &delay, 0, 200, "%d"))
            settings.swapDelay = delay;

        bool retry = settings.retryOnFail;
        if (ui::Checkbox("Retry on Fail", &retry))
            settings.retryOnFail = retry;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Use if swaps fail on some servers");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        
        ImGui::Text("PRESET");
        ImGui::Spacing();
        
        
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ui::s(6)) / 4.0f;
        bool presetHovered = false;
        for (int p = 0; p < MAX_PRESETS; p++) {
            if (p > 0) ImGui::SameLine();
            
            bool isActive = (settings.activePreset == p);
            bool hasSaved = presets[p].saved;
            
            
            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
            } else if (hasSaved) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            }
            
            char label[8];
            snprintf(label, sizeof(label), "%d", p + 1);
            if (ImGui::Button(label, ImVec2(buttonWidth, 0))) {
                settings.activePreset = p;
            }
            presetHovered = presetHovered || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
            
            ImGui::PopStyleColor(2);
        }
        ui::HoverDescription("Preset", presetHovered,
                             "Selects which saved hotbar layout is active for inventory organization.");
        
        ImGui::Spacing();
        
        
        Preset& currentPreset = presets[settings.activePreset];
        
        
        const bool saveHotbarPressed =
            ImGui::Button(currentPreset.saved ? "Overwrite" : "Save Hotbar", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ui::DescribeLastItem("Save Hotbar",
                             currentPreset.saved ? "Replaces this preset with the current hotbar layout."
                                                 : "Stores the current hotbar layout in this preset.");
        if (saveHotbarPressed) {
            saveHotbar();
        }
        
        ImGui::Spacing();
        
        
        float slotSize = ui::s(28);
        float spacing = ui::s(2);
        float totalWidth = slotSize * 9 + spacing * 8;
        float startX = (ImGui::GetContentRegionAvail().x - totalWidth) / 2;
        
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        cursor.x += startX;
        
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        for (int i = 0; i < 9; i++) {
            ImVec2 slotMin = ImVec2(cursor.x + i * (slotSize + spacing), cursor.y);
            ImVec2 slotMax = ImVec2(slotMin.x + slotSize, slotMin.y + slotSize);
            
            
            if (currentPreset.saved && currentPreset.slots[i].itemId != 0) {
                
                draw->AddRectFilled(slotMin, slotMax, IM_COL32(40, 40, 40, 220), 4.0f);
                draw->AddRect(slotMin, slotMax, IM_COL32(80, 80, 80, 255), 4.0f);
                
                
                unsigned int texId = ItemTextureManager::get().getTexture(currentPreset.slots[i].itemId, currentPreset.slots[i].damage);
                
                if (texId != 0) {
                    
                    float padding = ui::s(2);
                    ImVec2 imgMin = ImVec2(slotMin.x + padding, slotMin.y + padding);
                    ImVec2 imgMax = ImVec2(slotMax.x - padding, slotMax.y - padding);
                    draw->AddImage((ImTextureID)(intptr_t)texId, imgMin, imgMax);
                } else {
                    
                    const char* name = getItemShortName(currentPreset.slots[i].itemId);
                    ImVec2 textSize = ImGui::CalcTextSize(name);
                    ImVec2 textPos = ImVec2(
                        slotMin.x + (slotSize - textSize.x) / 2,
                        slotMin.y + (slotSize - textSize.y) / 2
                    );
                    draw->AddText(textPos, IM_COL32(200, 255, 200, 255), name);
                }
            } else {
                
                draw->AddRectFilled(slotMin, slotMax, IM_COL32(30, 30, 30, 200), 4.0f);
                draw->AddRect(slotMin, slotMax, IM_COL32(60, 60, 60, 255), 4.0f);
            }
            
            
            char numBuf[2];
            snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
            ImVec2 numSize = ImGui::CalcTextSize(numBuf);
            ImVec2 numPos = ImVec2(slotMin.x + (slotSize - numSize.x) / 2, slotMax.y + 2);
            draw->AddText(numPos, IM_COL32(100, 100, 100, 255), numBuf);
        }
        
        ImGui::Dummy(ImVec2(totalWidth, slotSize + ui::s(14)));
        
        if (!currentPreset.saved) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Press keybind to save current hotbar");
            ImGui::PopStyleColor();
        }
    });
}



void InventoryManager::saveSettings(json::Value& node) const {
    node["swapDelay"] = settings.swapDelay;
    node["retryOnFail"] = settings.retryOnFail;
    node["activePreset"] = settings.activePreset;
    for (int p = 0; p < MAX_PRESETS; p++) {
        std::string presetKey = "p" + std::to_string(p);
        node["presets"][presetKey]["saved"] = presets[p].saved;
        for (int i = 0; i < 9; i++) {
            std::string slotKey = std::to_string(i);
            node["presets"][presetKey][slotKey]["id"] = presets[p].slots[i].itemId;
            node["presets"][presetKey][slotKey]["dmg"] = presets[p].slots[i].damage;
        }
    }
}

void InventoryManager::loadSettings(const json::Value& node) {
    settings.swapDelay = node["swapDelay"].as_int(settings.swapDelay);
    settings.retryOnFail = node["retryOnFail"].as_bool(settings.retryOnFail);
    settings.activePreset = node["activePreset"].as_int(0);
    if (!node.has("presets"))
        return;
    const json::Value& presetsNode = node["presets"];
    for (int p = 0; p < MAX_PRESETS; p++) {
        std::string presetKey = "p" + std::to_string(p);
        if (!presetsNode.has(presetKey))
            continue;
        const json::Value& presetNode = presetsNode[presetKey];
        presets[p].saved = presetNode["saved"].as_bool(false);
        for (int i = 0; i < 9; i++) {
            std::string slotKey = std::to_string(i);
            if (!presetNode.has(slotKey))
                continue;
            presets[p].slots[i].itemId = presetNode[slotKey]["id"].as_int(0);
            presets[p].slots[i].damage = presetNode[slotKey]["dmg"].as_int(0);
        }
    }
}
