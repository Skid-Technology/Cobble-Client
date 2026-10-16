#include "autorefill.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../movement/sprint.h"
#include "../../ui/core/overlay.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#include <algorithm>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

AutoRefill* autoRefill = nullptr;

namespace {

constexpr float kPi = 3.14159265f;



void sendLwjglKeyState(int keyCode, bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = static_cast<WORD>(keyCode);
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &input, sizeof(INPUT));
}

void sendLwjglKey(int keyCode) {
    sendLwjglKeyState(keyCode, true);
    sendLwjglKeyState(keyCode, false);
}

bool playerInventorySlotScreenPosition(int inventorySlot, POINT& position) {
    if (!mc.isPlayerInventoryOpen() || !Overlay::gameWindow || GetForegroundWindow() != Overlay::gameWindow)
        return false;

    JNIEnv* env = Java::Env();
    if (!env || !JniCache::fid_currentScreen)
        return false;

    jobject screen = env->GetObjectField(mc.instance, JniCache::fid_currentScreen);
    if (!screen)
        return false;

    jclass guiScreen = nullptr;
    if (!Java::AssignClass("net.minecraft.client.gui.GuiScreen", guiScreen)) {
        env->DeleteLocalRef(screen);
        return false;
    }

    jfieldID width = env->GetFieldID(guiScreen, "width", "I");
    if (!width) {
        env->ExceptionClear();
        width = env->GetFieldID(guiScreen, "field_146294_l", "I");
    }
    jfieldID height = env->GetFieldID(guiScreen, "height", "I");
    if (!height) {
        env->ExceptionClear();
        height = env->GetFieldID(guiScreen, "field_146295_m", "I");
    }

    const int guiWidth = width ? env->GetIntField(screen, width) : 0;
    const int guiHeight = height ? env->GetIntField(screen, height) : 0;
    env->DeleteLocalRef(guiScreen);
    env->DeleteLocalRef(screen);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (guiWidth <= 0 || guiHeight <= 0 || inventorySlot < 0 || inventorySlot > 35)
        return false;

    RECT client{};
    if (!GetClientRect(Overlay::gameWindow, &client))
        return false;
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth <= 0 || clientHeight <= 0)
        return false;

    const int guiLeft = (guiWidth - 176) / 2;
    const int guiTop = (guiHeight - 166) / 2;
    const int column = inventorySlot % 9;
    const int row = inventorySlot >= 9 ? (inventorySlot - 9) / 9 : 3;
    const int slotX = guiLeft + 16 + column * 18;
    const int slotY = guiTop + (inventorySlot >= 9 ? 92 + row * 18 : 150);

    position.x = slotX * clientWidth / guiWidth;
    position.y = slotY * clientHeight / guiHeight;
    return ClientToScreen(Overlay::gameWindow, &position) != FALSE;
}

}  

AutoRefill::AutoRefill() : Module("AutoRefill") {
    keybind = 0;  
    rng_.seed(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
              (reinterpret_cast<uint64_t>(this) * 0x9E3779B97F4A7C15ull));
}

void AutoRefill::applyPreset(AutoRefillPreset preset) {
    settings.preset = static_cast<int>(preset);
    switch (preset) {
        case AutoRefillPreset::Instant:
            
            settings.speed = 0;
            settings.reactionMs = 0;
            settings.jitterPercent = 0;
            settings.hesitateChance = 0;
            settings.fatigue = false;
            settings.humanPath = false;
            settings.legitMoveTime = 60;
            break;
        case AutoRefillPreset::Fast:
            settings.speed = 45;
            settings.reactionMs = 70;
            settings.jitterPercent = 20;
            settings.hesitateChance = 0;
            settings.fatigue = false;
            settings.humanPath = true;
            settings.legitMoveTime = 90;
            break;
        case AutoRefillPreset::Human:
            settings.speed = 80;
            settings.reactionMs = 190;
            settings.jitterPercent = 35;
            settings.hesitateChance = 12;
            settings.fatigue = true;
            settings.humanPath = true;
            settings.legitMoveTime = 130;
            break;
        case AutoRefillPreset::Careful:
            settings.speed = 150;
            settings.reactionMs = 300;
            settings.jitterPercent = 55;
            settings.hesitateChance = 25;
            settings.fatigue = true;
            settings.humanPath = true;
            settings.legitMoveTime = 210;
            break;
        case AutoRefillPreset::Custom:
        default:
            break;
    }
}

int64_t AutoRefill::jitter(int baseMs) const {
    return jitter(baseMs, settings.jitterPercent);
}

int64_t AutoRefill::jitter(int baseMs, int spreadPercent) const {
    if (baseMs <= 0)
        return 0;
    const int spread = (std::clamp)(spreadPercent, 0, 100);
    if (spread == 0)
        return baseMs;
    
    
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const float noise = (dist(rng_) + dist(rng_)) * 0.5f;
    const float scaled =
        static_cast<float>(baseMs) * (1.0f + noise * (static_cast<float>(spread) / 100.0f));
    return static_cast<int64_t>(std::lround((std::max)(0.0f, scaled)));
}

bool AutoRefill::roll(int percent) const {
    if (percent <= 0)
        return false;
    if (percent >= 100)
        return true;
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rng_) <= percent;
}

float AutoRefill::fatigueScale() const {
    if (!settings.fatigue || movesDone_ <= 0)
        return 1.0f;
    
    
    return 1.0f + 0.06f * static_cast<float>((std::min)(movesDone_, 6));
}

int64_t AutoRefill::nextGap() const {
    int64_t gap =
        static_cast<int64_t>(std::lround(static_cast<float>(jitter(settings.speed)) * fatigueScale()));
    
    
    
    if (roll(settings.hesitateChance))
        gap += jitter(settings.speed + 110, 45);
    return gap;
}

void AutoRefill::enterState(State next, int64_t delayMs) {
    state = next;
    stateStartTime = std::chrono::steady_clock::now();
    stateDelay_ = delayMs > 0 ? delayMs : 0;
}

bool AutoRefill::takeNextTask(RefillTask& out) {
    if (pendingRefills.empty())
        return false;

    size_t pick = 0;
    if (settings.nearestFirst && lastSourceSlot_ >= 9) {
        
        
        const int lastRow = (lastSourceSlot_ - 9) / 9;
        const int lastCol = lastSourceSlot_ % 9;
        int best = 1 << 30;
        for (size_t i = 0; i < pendingRefills.size(); ++i) {
            const int slot = pendingRefills[i].sourceSlot;
            if (slot < 9)
                continue;
            const int dr = ((slot - 9) / 9) - lastRow;
            const int dc = (slot % 9) - lastCol;
            const int cost = dr * dr + dc * dc;
            if (cost < best) {
                best = cost;
                pick = i;
            }
        }
    }

    out = pendingRefills[pick];
    pendingRefills.erase(pendingRefills.begin() + static_cast<std::ptrdiff_t>(pick));
    lastSourceSlot_ = out.sourceSlot;
    return true;
}

bool AutoRefill::canToggle() {
    
    return !settings.onInventoryOpen;
}

bool AutoRefill::isPacketRefilling() const {
    return settings.mode == 0 && state != State::Idle && state != State::Reacting &&
           state != State::RestoreSprintDown && state != State::RestoreSprintUp;
}

bool AutoRefill::startLegitMove(int slotId) {
    if (!playerInventorySlotScreenPosition(slotId, mouseTarget_))
        return false;
    if (!GetCursorPos(&mouseStart_))
        return false;

    const int base = (std::max)(20, settings.legitMoveTime);
    if (!settings.humanPath) {
        pathDuration_ = base;
        pathSettle_ = 0;
        bowFrac_ = 0.0f;
        overshootFrac_ = 0.0f;
        tremorAmp_ = 0.0f;
        return true;
    }

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    
    std::uniform_int_distribution<int> scatter(-4, 4);
    mouseTarget_.x += scatter(rng_);
    mouseTarget_.y += scatter(rng_);

    const float dx = static_cast<float>(mouseTarget_.x - mouseStart_.x);
    const float dy = static_cast<float>(mouseTarget_.y - mouseStart_.y);
    const float dist = std::sqrt(dx * dx + dy * dy);

    
    
    
    
    const float scale = 0.5f + 0.5f * std::log2(1.0f + dist / 220.0f);
    pathDuration_ = jitter(static_cast<int>(std::lround(static_cast<float>(base) * scale)), 18);
    if (pathDuration_ < 24)
        pathDuration_ = 24;
    
    pathSettle_ = jitter(28, 60);

    
    
    bowFrac_ = (0.035f + unit(rng_) * 0.075f) * (unit(rng_) < 0.5f ? -1.0f : 1.0f);
    
    overshootFrac_ = roll(34) ? (0.02f + unit(rng_) * 0.045f) : 0.0f;
    tremorAmp_ = 0.6f + unit(rng_) * 0.9f;
    tremorPhase_ = unit(rng_) * (2.0f * kPi);
    tremorFreq_ = 5.0f + unit(rng_) * 9.0f;
    return true;
}

bool AutoRefill::updateLegitMove(long long elapsed) {
    const float duration = pathDuration_ > 0 ? static_cast<float>(pathDuration_) : 1.0f;
    float t = static_cast<float>(elapsed) / duration;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    const float dx = static_cast<float>(mouseTarget_.x - mouseStart_.x);
    const float dy = static_cast<float>(mouseTarget_.y - mouseStart_.y);
    const float dist = std::sqrt(dx * dx + dy * dy);

    float along = t;
    float lateral = 0.0f;
    if (settings.humanPath) {
        
        along = t * t * t * (10.0f - 15.0f * t + 6.0f * t * t);
        if (overshootFrac_ > 0.0f) {
            
            
            
            const float o = (t - 0.55f) / 0.45f;
            if (o > 0.0f)
                along += overshootFrac_ * std::sin(kPi * (o > 1.0f ? 1.0f : o));
        }
        lateral = bowFrac_ * dist * std::sin(kPi * t) +
                  tremorAmp_ * std::sin(tremorPhase_ + tremorFreq_ * t);
    }

    float perpX = 0.0f;
    float perpY = 0.0f;
    if (dist > 0.001f) {
        perpX = -dy / dist;
        perpY = dx / dist;
    }

    const int x = mouseStart_.x + static_cast<int>(std::lround(dx * along + perpX * lateral));
    const int y = mouseStart_.y + static_cast<int>(std::lround(dy * along + perpY * lateral));
    SetCursorPos(x, y);
    return elapsed >= pathDuration_ + pathSettle_;
}

void AutoRefill::onEnable() {
    
    if (mc.isInGui()) {
        enabled = false;
        return;
    }

    scanForRefills();

    if (pendingRefills.empty()) {
        enabled = false;
        return;
    }

    movesDone_ = 0;
    lastSourceSlot_ = -1;
    decoyStage_ = 0;
    hasActiveTask_ = false;
    manualTrigger_ = true;
    nextGapMs_ = 0;
    
    
    
    
    enterState(State::Reacting, jitter(settings.reactionMs));
    lastMoveTime = stateStartTime;
}

void AutoRefill::onDisable() {
    if (sprintKeyHeld_) {
        sendLwjglKeyState(mc.getSprintKey(), false);
        sprintKeyHeld_ = false;
    }
    if (movementPaused_) {
        mc.restoreMovementKeys();
        movementPaused_ = false;
    }
    state = State::Idle;
    pendingRefills.clear();
    hasActiveTask_ = false;
    decoyStage_ = 0;
    movesDone_ = 0;
    lastSourceSlot_ = -1;
    nextGapMs_ = 0;
    pendingClickSlot_.store(-1, std::memory_order_relaxed);
    pendingClickHotbar_.store(-1, std::memory_order_relaxed);
}

void AutoRefill::finishRefill() {
    if (movementPaused_) {
        mc.restoreMovementKeys();
        movementPaused_ = false;
    }
    state = State::Idle;
    pendingRefills.clear();
    hasActiveTask_ = false;
    decoyStage_ = 0;
    movesDone_ = 0;
    lastSourceSlot_ = -1;
    nextGapMs_ = 0;
    pendingClickSlot_.store(-1, std::memory_order_relaxed);
    pendingClickHotbar_.store(-1, std::memory_order_relaxed);

    
    if (!settings.onInventoryOpen) {
        enabled = false;
    }
}

void AutoRefill::onUpdate() {
    if (!player.isValid())
        return;

    
    if (settings.onInventoryOpen && state == State::Idle) {
        bool inventoryOpen = mc.isInventoryOpen();

        if (inventoryOpen && !wasInventoryOpen) {
            scanForRefills();
            if (!pendingRefills.empty()) {
                movesDone_ = 0;
                lastSourceSlot_ = -1;
                decoyStage_ = 0;
                hasActiveTask_ = false;
                manualTrigger_ = false;
                nextGapMs_ = 0;
                
                
                
                enterState(State::Reacting, jitter(settings.reactionMs));
                lastMoveTime = stateStartTime;
            }
        }

        wasInventoryOpen = inventoryOpen;
    }

    if (state == State::Idle)
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stateStartTime).count();

    switch (state) {
        case State::Idle:
            break;

        case State::Reacting: {
            if (elapsed < stateDelay_)
                return;

            if (!manualTrigger_) {
                
                
                enterState(settings.mode == 1 ? State::LegitMoveSource : State::Refilling, 0);
                lastMoveTime = stateStartTime;
                nextGapMs_ = 0;
                return;
            }

            
            
            if (mc.isInGui()) {
                finishRefill();
                return;
            }

            if (settings.mode == 0) {
                
                
                if (sprint && !sprint->enabled)
                    sprint->setEnabled(true);
                mc.releaseMovementKeys();
                movementPaused_ = true;
                mc.setKeyBindState(mc.getSprintKey(), false);
                mc.sendInventoryOpenPacket();
                enterState(State::PacketOpening, jitter(80, 30));
            } else {
                sendLwjglKey(mc.getInventoryKey());
                enterState(State::LegitOpening, 0);
            }
            return;
        }

        case State::PacketOpening: {
            
            
            if (elapsed < stateDelay_)
                return;
            if (movementPaused_) {
                mc.restoreMovementKeys();
                movementPaused_ = false;
            }
            enterState(State::Refilling, 0);
            lastMoveTime = stateStartTime;
            nextGapMs_ = 0;
            break;
        }

        case State::Refilling: {
            
            
            if (pendingClickSlot_.load(std::memory_order_acquire) >= 0)
                return;

            const auto sinceMove =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMoveTime).count();
            if (sinceMove < nextGapMs_)
                return;

            RefillTask task{};
            if (!takeNextTask(task)) {
                enterState(State::Closing, jitter(settings.speed + 60, 40));
                return;
            }

            
            
            pendingClickHotbar_.store(task.hotbarSlot, std::memory_order_relaxed);
            pendingClickSlot_.store(task.sourceSlot, std::memory_order_release);
            lastMoveTime = now;
            ++movesDone_;
            nextGapMs_ = nextGap();
            break;
        }

        case State::LegitOpening: {
            if (mc.isPlayerInventoryOpen()) {
                enterState(settings.mode == 1 ? State::LegitMoveSource : State::Refilling, 0);
                lastMoveTime = stateStartTime;
                
                
                nextGapMs_ = jitter(settings.speed, 40);
                break;
            }
            if (elapsed > 1000)
                finishRefill();
            break;
        }

        case State::LegitMoveSource: {
            if (!mc.isPlayerInventoryOpen()) {
                finishRefill();
                return;
            }

            if (!hasActiveTask_) {
                const auto wait =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMoveTime).count();
                if (wait < nextGapMs_)
                    return;
                if (!takeNextTask(activeTask_)) {
                    enterState(State::Closing, jitter(settings.speed + 60, 40));
                    return;
                }
                hasActiveTask_ = true;

                
                
                
                decoyStage_ = 0;
                int firstTarget = activeTask_.sourceSlot;
                if (settings.humanPath && roll(settings.hesitateChance)) {
                    const int step = roll(50) ? 1 : 9;  
                    const int drift = activeTask_.sourceSlot + (roll(50) ? step : -step);
                    if (drift >= 9 && drift <= 35) {
                        decoyStage_ = 1;
                        firstTarget = drift;
                    }
                }

                if (!startLegitMove(firstTarget)) {
                    finishRefill();
                    return;
                }
                stateStartTime = now;
                return;
            }

            if (!updateLegitMove(elapsed))
                return;

            if (decoyStage_ == 1) {
                
                decoyStage_ = 0;
                if (!startLegitMove(activeTask_.sourceSlot)) {
                    finishRefill();
                    return;
                }
                stateStartTime = now;
                return;
            }

            
            
            
            pendingClickHotbar_.store(activeTask_.hotbarSlot, std::memory_order_relaxed);
            pendingClickSlot_.store(activeTask_.sourceSlot, std::memory_order_release);
            ++movesDone_;
            enterState(State::LegitAwaitClick, 0);
            break;
        }

        case State::LegitAwaitClick: {
            if (!mc.isPlayerInventoryOpen() || !hasActiveTask_) {
                finishRefill();
                return;
            }
            
            if (pendingClickSlot_.load(std::memory_order_acquire) >= 0)
                return;

            hasActiveTask_ = false;
            lastMoveTime = now;
            nextGapMs_ = nextGap();
            enterState(State::LegitMoveSource, 0);
            break;
        }

        case State::RestoreSprintDown: {
            
            
            if (elapsed < stateDelay_)
                return;
            const int sprintKey = mc.getSprintKey();
            mc.setKeyBindState(sprintKey, true);
            sendLwjglKeyState(sprintKey, true);
            sprintKeyHeld_ = true;
            
            
            enterState(State::RestoreSprintUp, 90 + jitter(30, 60));
            break;
        }

        case State::RestoreSprintUp: {
            if (elapsed < stateDelay_)
                return;
            const int sprintKey = mc.getSprintKey();
            mc.setKeyBindState(sprintKey, false);
            sendLwjglKeyState(sprintKey, false);
            sprintKeyHeld_ = false;
            finishRefill();
            break;
        }

        case State::Closing: {
            if (elapsed < stateDelay_)
                return;

            
            
            
            
            
            
            

            
            
            if (!settings.onInventoryOpen) {
                if (settings.mode == 0)
                    mc.sendInventoryClosePacket();
                else
                    sendLwjglKey(mc.getInventoryKey());
            }
            if (settings.mode == 0 && !settings.onInventoryOpen) {
                enterState(State::RestoreSprintDown, jitter(55, 35));
                break;
            }
            finishRefill();
            break;
        }
    }
}

void AutoRefill::onOutgoingPacket(JNIEnv* env, jobject packet) {
    if (pendingClickSlot_.load(std::memory_order_acquire) < 0 || !env || !packet)
        return;

    if (!c03Resolved_) {
        c03Resolved_ = true;
        jclass local = nullptr;
        if (Java::AssignClass("net.minecraft.network.play.client.C03PacketPlayer", local)) {
            c03Class_ = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }
    if (!c03Class_)
        return;

    if (env->ExceptionCheck())
        env->ExceptionClear();
    
    
    
    if (!env->IsInstanceOf(packet, c03Class_))
        return;

    const int slot = pendingClickSlot_.load(std::memory_order_acquire);
    const int hotbar = pendingClickHotbar_.load(std::memory_order_relaxed);
    pendingClickHotbar_.store(-1, std::memory_order_relaxed);
    pendingClickSlot_.store(-1, std::memory_order_release);
    
    
    
    
    if (hotbar >= 0 && hotbar <= 8)
        mc.windowClickSwap(slot, hotbar);
    else
        mc.windowClick(slot, 0);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

void AutoRefill::scanForRefills() {
    pendingRefills.clear();

    auto slots = mc.getInventorySlots();
    if (slots.empty())
        return;

    
    
    std::vector<int> soupSources;
    std::vector<int> potionSources;
    for (const auto& slot : slots) {
        if (slot.slotId < 9 || slot.slotId > 35)
            continue;
        if (settings.refillSoup && slot.itemId == RefillItems::MUSHROOM_SOUP) {
            soupSources.push_back(slot.slotId);
        } else if (settings.refillPotions && slot.itemId == RefillItems::POTION && slot.damage == 16421) {
            
            potionSources.push_back(slot.slotId);
        }
    }

    if (soupSources.empty() && potionSources.empty())
        return;

    
    int hotbar[9] = {};
    for (const auto& slot : slots) {
        if (slot.slotId >= 0 && slot.slotId <= 8)
            hotbar[slot.slotId] = slot.itemId;
    }

    size_t soupIdx = 0;
    size_t potionIdx = 0;
    for (int hotbarSlot = 0; hotbarSlot <= 8; hotbarSlot++) {
        const int existing = hotbar[hotbarSlot];
        int source = -1;

        
        
        if (settings.refillSoup && (existing == 0 || existing == RefillItems::BOWL) &&
            soupIdx < soupSources.size()) {
            source = soupSources[soupIdx++];
        } else if (settings.refillPotions && existing == 0 && potionIdx < potionSources.size()) {
            source = potionSources[potionIdx++];
        }

        if (source != -1) {
            RefillTask task;
            task.hotbarSlot = hotbarSlot;
            task.sourceSlot = source;
            pendingRefills.push_back(task);
        }
    }
}

void AutoRefill::onRenderMenu() {
    ui::two_column(
        "General",
        [this]() {
            ui::Binder("Keybind", &keybind);
            ImGui::Spacing();
            static const char* modes[] = {"Packet", "Legit"};
            ui::Combo("Mode", &settings.mode, modes, IM_ARRAYSIZE(modes));
            ImGui::Spacing();
            ui::Checkbox("On Inventory Open", &settings.onInventoryOpen);
        },
        "Filters",
        [this]() {
            ui::Checkbox("Refill Soup", &settings.refillSoup);
            ui::Checkbox("Refill Potions", &settings.refillPotions);
        });

    ui::two_column(
        "Timing",
        [this]() {
            static const char* presets[] = {"Instant", "Fast", "Human", "Careful", "Custom"};
            int preset = (std::clamp)(settings.preset, 0, 4);
            if (ui::Combo("Preset", &preset, presets, IM_ARRAYSIZE(presets)))
                applyPreset(static_cast<AutoRefillPreset>(preset));

            
            
            bool touched = false;
            touched |= ui::SliderInt("Gap (ms)", &settings.speed, 0, 300);
            touched |= ui::SliderInt("Reaction (ms)", &settings.reactionMs, 0, 600);
            touched |= ui::SliderInt("Jitter (%)", &settings.jitterPercent, 0, 100);
            touched |= ui::Checkbox("Fatigue", &settings.fatigue);
            if (touched)
                settings.preset = static_cast<int>(AutoRefillPreset::Custom);
        },
        "Behaviour",
        [this]() {
            bool touched = false;
            touched |= ui::SliderInt("Hesitate (%)", &settings.hesitateChance, 0, 60);
            ui::Checkbox("Nearest Source First", &settings.nearestFirst);
            ImGui::Spacing();
            touched |= ui::Checkbox("Human Mouse Path", &settings.humanPath);
            if (settings.mode == 1)
                touched |= ui::SliderInt("Mouse Move (ms)", &settings.legitMoveTime, 40, 500);
            if (touched)
                settings.preset = static_cast<int>(AutoRefillPreset::Custom);
        });

    ImGui::Spacing();

    
    const int moved = movesDone_;
    switch (state) {
        case State::Idle:
            if (settings.onInventoryOpen)
                ImGui::TextDisabled("Passive - refills when you open your inventory");
            else
                ImGui::TextDisabled("Idle - press the keybind to refill once");
            break;
        case State::Reacting:
            ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "Reacting...");
            break;
        default:
            ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "Refilling - %d item%s moved", moved,
                               moved == 1 ? "" : "s");
            break;
    }
    if (settings.mode == 1 && !settings.humanPath)
        ImGui::TextDisabled("Straight-line cursor: fast, and obvious to anyone watching");
}

std::string AutoRefill::getDisplayInfo() {
    if (settings.mode == 1)
        return "Legit";
    return settings.onInventoryOpen ? "Inv Open" : "Manual";
}

void AutoRefill::saveSettings(json::Value& node) const {
    node["refillSoup"] = settings.refillSoup;
    node["refillPotions"] = settings.refillPotions;
    node["speed"] = settings.speed;
    node["mode"] = settings.mode;
    node["legitMoveTime"] = settings.legitMoveTime;
    node["onInventoryOpen"] = settings.onInventoryOpen;
    node["preset"] = settings.preset;
    node["reactionMs"] = settings.reactionMs;
    node["jitterPercent"] = settings.jitterPercent;
    node["fatigue"] = settings.fatigue;
    node["hesitateChance"] = settings.hesitateChance;
    node["humanPath"] = settings.humanPath;
    node["nearestFirst"] = settings.nearestFirst;
}

void AutoRefill::loadSettings(const json::Value& node) {
    settings.refillSoup = node["refillSoup"].as_bool(settings.refillSoup);
    settings.refillPotions = node["refillPotions"].as_bool(settings.refillPotions);
    settings.speed = (std::clamp)(node["speed"].as_int(settings.speed), 0, 300);
    settings.mode = (std::clamp)(node["mode"].as_int(settings.mode), 0, 1);
    settings.legitMoveTime = (std::max)(40, node["legitMoveTime"].as_int(settings.legitMoveTime));
    settings.onInventoryOpen = node["onInventoryOpen"].as_bool(settings.onInventoryOpen);
    
    
    
    
    settings.preset = (std::clamp)(node["preset"].as_int(settings.preset), 0, 4);
    settings.reactionMs = (std::clamp)(node["reactionMs"].as_int(settings.reactionMs), 0, 600);
    settings.jitterPercent = (std::clamp)(node["jitterPercent"].as_int(settings.jitterPercent), 0, 100);
    settings.fatigue = node["fatigue"].as_bool(settings.fatigue);
    settings.hesitateChance = (std::clamp)(node["hesitateChance"].as_int(settings.hesitateChance), 0, 60);
    settings.humanPath = node["humanPath"].as_bool(settings.humanPath);
    settings.nearestFirst = node["nearestFirst"].as_bool(settings.nearestFirst);
}
