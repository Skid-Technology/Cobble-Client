#include "autoclicker.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/core/overlay.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"
#include "hitselect.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cmath>
#include <array>
#include <limits>
#include <vector>

AutoClicker* autoClicker = nullptr;


namespace ItemIds {

constexpr int WOOD_SWORD = 268;
constexpr int STONE_SWORD = 272;
constexpr int IRON_SWORD = 267;
constexpr int DIAMOND_SWORD = 276;
constexpr int GOLD_SWORD = 283;


constexpr int WOOD_AXE = 271;
constexpr int STONE_AXE = 275;
constexpr int IRON_AXE = 258;
constexpr int DIAMOND_AXE = 279;
constexpr int GOLD_AXE = 286;
}  

AutoClicker::AutoClicker() : Module("AutoClicker") {
    keybind = 0;  

    std::random_device rd;
    rng.seed(rd());
    reseedSecureRng();

    regenerateDelayRange();
}

void AutoClicker::onEnable() {
    auto now = std::chrono::steady_clock::now();

    
    stableTimelineStart = now;
    stableLastClick = now;  
    stableClickCount = 0;

    
    lastClick = now;
    lastClickWasDown = false;
    wasBreakingBlock = false;
    clicksSinceRangeChange = 0;
    sessionStart = now;
    fatigueMultiplier = 1.0f;
    fatigueTargetMultiplier = 1.0f;
    fatiguePhaseClicksRemaining = 0;
    recoveringFromFatigue = true;
    inBurst = false;
    burstClicksRemaining = 0;
    burstSpeedMultiplier = 1.0f;
    sigmaState = 0.12f + static_cast<float>(secureUnit()) * 0.20f;  
    sigmaClicksRemaining = 0;
    sigmaTarget = sigmaState;
    
    
    sigmaSessionOffset = (static_cast<float>(secureUnit()) - 0.5f) * 0.08f;  
    tempoStepScale = 0.08f + static_cast<float>(secureUnit()) * 0.16f;        
    tempoPull = 0.02f + static_cast<float>(secureUnit()) * 0.10f;             
    hesitationChance = 0.01f + static_cast<float>(secureUnit()) * 0.05f;      
    doubleTapChance = 0.005f + static_cast<float>(secureUnit()) * 0.035f;     
    shapePhaseClicksRemaining = 0;
    for (auto& row : pinkRows)
        row = static_cast<float>(secureUnit()) * 2.0f - 1.0f;
    pinkCounter = 0;
    pinkState = 0.0f;
    pinkAmplitude = 0.05f + static_cast<float>(secureUnit()) * 0.12f;  
    float initialMinCps = std::isfinite(settings.minCPS) ? settings.minCPS : 1.0f;
    float initialMaxCps = std::isfinite(settings.maxCPS) ? settings.maxCPS : 1.0f;
    initialMinCps = std::clamp(initialMinCps, 1.0f, 20.0f);
    initialMaxCps = std::clamp(initialMaxCps, 1.0f, 20.0f);
    if (initialMinCps > initialMaxCps)
        std::swap(initialMinCps, initialMaxCps);
    plusCpsState = (initialMinCps + initialMaxCps) * 0.5f;
    plusStateClicksRemaining = 0;
    lastGeneratedMinCps = initialMinCps;
    lastGeneratedMaxCps = initialMaxCps;
    clickDownFraction = 0.5f;
    
    currentClickIntervalMs = 1000.0f / plusCpsState;
    pendingGapMs = currentClickIntervalMs;
    lastObservedHotbarSlot = player.getCurrentSlot();
    manualUseYieldUntil = now;

    if (!settings.stableCPS) {
        regenerateDelayRange();
        calculateNextDelay();
    }
}

void AutoClicker::onDisable() {
    HWND hwnd = GetForegroundWindow();

    
    if (wasBreakingBlock && hwnd) {
        PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
        wasBreakingBlock = false;
    }

    
    if (lastClickWasDown && hwnd) {
        PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
        lastClickWasDown = false;
    }
}

void AutoClicker::regenerateDelayRange() {
    
    std::uniform_real_distribution<float> minDist(15.0f, 40.0f);
    std::uniform_real_distribution<float> maxDist(60.0f, 85.0f);
    std::uniform_int_distribution<int> nextChangeDist(30, 70);

    currentDelayMin = minDist(rng);
    currentDelayMax = maxDist(rng);
    clicksUntilRangeChange = nextChangeDist(rng);
    clicksSinceRangeChange = 0;
}

void AutoClicker::onUpdate() {
    if (!enabled)
        return;

    
    if (Menu::isOpen)
        return;

    HWND hwnd = GetForegroundWindow();
    if (!hwnd)
        return;

    
    if (hwnd != Overlay::gameWindow)
        return;

    const auto inputNow = std::chrono::steady_clock::now();
    const int currentHotbarSlot = player.getCurrentSlot();
    const bool switchedHotbarSlot =
        lastObservedHotbarSlot >= 0 && currentHotbarSlot >= 0 && currentHotbarSlot != lastObservedHotbarSlot;
    lastObservedHotbarSlot = currentHotbarSlot;

    
    
    
    
    const bool manualRightClick = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (manualRightClick || switchedHotbarSlot) {
        if (lastClickWasDown) {
            PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
            lastClickWasDown = false;
        }

        lastClick = inputNow;
        nextEventTime = inputNow;
        stableTimelineStart = inputNow;
        stableLastClick = inputNow;
        stableClickCount = 0;
        manualUseYieldUntil = inputNow + std::chrono::milliseconds(manualRightClick ? 80 : 45);
        return;
    }

    if (inputNow < manualUseYieldUntil)
        return;

    
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        if (settings.stableCPS) {
            stableTimelineStart = inputNow;
            stableLastClick = inputNow;
            stableClickCount = 0;
        }
        return;
    }

    
    bool inInventory = mc.isInventoryOpen();  
    bool inAnyGui = mc.isInGui();             

    
    if (inAnyGui && !inInventory)
        return;

    
    if (inInventory && !settings.workInInventory)
        return;

    
    
    if (inInventory && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        return;  
    }

    
    if (settings.weaponOnly && !inInventory && !isHoldingWeapon()) {
        
        if (settings.stableCPS) {
            auto now = std::chrono::steady_clock::now();
            stableTimelineStart = now;
            stableLastClick = now;
            stableClickCount = 0;
        }
        return;
    }

    
    
    
    bool isBreaking = settings.breakBlocks && !inInventory && isTargetingBlock();

    if (isBreaking && !wasBreakingBlock) {
        
        if (lastClickWasDown) {
            PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
            lastClickWasDown = false;
        }
        
        PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
        wasBreakingBlock = true;
        return;
    }

    if (isBreaking && wasBreakingBlock) {
        
        return;
    }

    if (!isBreaking && wasBreakingBlock) {
        
        PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
        wasBreakingBlock = false;
        lastClickWasDown = false;
        
        auto now = std::chrono::steady_clock::now();
        lastClick = now;
        nextEventTime = now;
        stableTimelineStart = now;
        stableLastClick = now;
        stableClickCount = 0;
    }

    
    if (settings.attackingOnly && !inInventory) {
        if (!isTargetingPlayer()) {
            return;
        }
    }

    
    
    
    if (settings.stableCPS) {
        auto now = std::chrono::steady_clock::now();

        
        
        
        double targetCps = static_cast<double>(settings.stableCPSValue);
        double minIntervalMs = 1000.0 / targetCps;  

        
        double timeSinceLastClick = std::chrono::duration<double, std::milli>(now - stableLastClick).count();

        
        
        if (timeSinceLastClick < minIntervalMs)
            return;

        
        bool useRightClick = inInventory && (GetAsyncKeyState(VK_SHIFT) & 0x8000);

        
        if (useRightClick) {
            PostMessage(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, 0);
            PostMessage(hwnd, WM_RBUTTONUP, 0, 0);
        } else {
            
            if (hitSelect && hitSelect->enabled && !hitSelect->shouldAllowClick()) {
                
                stableLastClick = now;
                return;
            }
            PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
            PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
        }
        stableClickCount++;
        stableLastClick = now;
        return;
    }

    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClick);
    if (elapsed.count() < nextDelayMs)
        return;

    
    
    bool useRightClick = inInventory && (GetAsyncKeyState(VK_SHIFT) & 0x8000);

    HWND clickHwnd = GetForegroundWindow();
    if (clickHwnd) {
        if (useRightClick) {
            
            if (lastClickWasDown) {
                PostMessage(clickHwnd, WM_RBUTTONUP, 0, 0);
                lastClickWasDown = false;
            } else {
                PostMessage(clickHwnd, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                lastClickWasDown = true;
            }
        } else {
            
            if (hitSelect && hitSelect->enabled && !hitSelect->shouldAllowClick()) {
                
                lastClick = now;
                calculateNextDelay();
                return;
            }
            doClick();
        }
    }
    lastClick = now;
    lastAutoClick = now;

    
    clicksSinceRangeChange++;
    if (clicksSinceRangeChange >= clicksUntilRangeChange) {
        regenerateDelayRange();
    }

    calculateNextDelay();
}

void AutoClicker::calculateNextDelay() {
    float minCps = settings.minCPS;
    float maxCps = settings.maxCPS;

    
    
    
    if (!std::isfinite(minCps))
        minCps = 1.0f;
    if (!std::isfinite(maxCps))
        maxCps = 1.0f;
    minCps = std::clamp(minCps, 1.0f, 20.0f);
    maxCps = std::clamp(maxCps, 1.0f, 20.0f);

    if (minCps > maxCps)
        std::swap(minCps, maxCps);

    
    if (settings.stableCPS)
        return;

    
    
    
    
    
    
    
    
    
    
    if (lastClickWasDown) {
        
        if (settings.randomization) {
            updateFatigue();
            updateBurstMode();
            currentClickIntervalMs = generateHumanIntervalMs(minCps, maxCps);
        } else {
            std::uniform_real_distribution<float> d(minCps, maxCps);
            currentClickIntervalMs = 1000.0f / std::max(d(rng), 0.1f);
        }
        
        
        
        float holdMs = currentClickIntervalMs * (0.18f + static_cast<float>(secureUnit()) * 0.22f);
        holdMs *= 1.0f + (std::max(fatigueMultiplier, 1.0f) - 1.0f) * 0.6f;
        if (secureUnit() < 0.015) {
            
            holdMs += 30.0f + static_cast<float>(secureUnit()) * 50.0f;
        }
        holdMs = std::clamp(holdMs, 18.0f, std::min(140.0f, currentClickIntervalMs * 0.45f));
        pendingGapMs = std::max(1.0f, currentClickIntervalMs - holdMs);
        nextDelayMs = std::clamp(static_cast<int>(std::lround(holdMs)), 4, 500);
    } else {
        
        
        nextDelayMs = std::clamp(static_cast<int>(std::lround(pendingGapMs)), 4, 500);
    }
}

void AutoClicker::reseedSecureRng() {
    std::random_device rd;
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::array<uint32_t, 10> seedData{};
    for (size_t i = 0; i < seedData.size(); ++i) {
        const uint64_t mixedTime = static_cast<uint64_t>(now) >> ((i % 4) * 16);
        seedData[i] = rd() ^ static_cast<uint32_t>(mixedTime) ^
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this) >> (i % 8));
    }
    std::seed_seq seed(seedData.begin(), seedData.end());
    secureRng.seed(seed);
    secureDrawCount = 0;
}

double AutoClicker::secureUnit() {
    if (++secureDrawCount >= 256)
        reseedSecureRng();
    return std::generate_canonical<double, std::numeric_limits<double>::digits>(secureRng);
}

float AutoClicker::generateTargetCps(float minCps, float maxCps) {
    minCps = std::clamp(minCps, 1.0f, 25.0f);
    maxCps = std::clamp(maxCps, minCps, 25.0f);
    const float range = maxCps - minCps;
    if (range <= 0.001f)
        return minCps;

    if (settings.randomizationMode == RandomizationMode::RANDOM) {
        std::uniform_real_distribution<float> distribution(minCps, maxCps);
        return distribution(rng);
    }

    
    
    
    const float u = static_cast<float>((secureUnit() + secureUnit()) * 0.5);
    return minCps + u * range;
}






float AutoClicker::generateHumanIntervalMs(float minCps, float maxCps) {
    if (!std::isfinite(minCps))
        minCps = 1.0f;
    if (!std::isfinite(maxCps))
        maxCps = 1.0f;
    minCps = std::clamp(minCps, 1.0f, 20.0f);
    maxCps = std::clamp(maxCps, 1.0f, 20.0f);
    if (minCps > maxCps)
        std::swap(minCps, maxCps);

    
    
    
    const double minIntervalMs = 1000.0 / static_cast<double>(maxCps);
    const double maxIntervalMs = 1000.0 / static_cast<double>(minCps);
    const double minLogInterval = std::log(minIntervalMs);
    const double maxLogInterval = std::log(maxIntervalMs);
    const double logRange = maxLogInterval - minLogInterval;
    if (logRange <= 1e-6)
        return static_cast<float>(minIntervalMs);

    
    
    
    if (std::abs(lastGeneratedMinCps - minCps) > 1e-3f ||
        std::abs(lastGeneratedMaxCps - maxCps) > 1e-3f) {
        lastGeneratedMinCps = minCps;
        lastGeneratedMaxCps = maxCps;
        plusCpsState = (minCps + maxCps) * 0.5f;
        plusStateClicksRemaining = 0;
    }

    
    
    
    updateShapeParams();
    if (plusStateClicksRemaining <= 0) {
        const double u = (secureUnit() + secureUnit() + secureUnit()) / 3.0;  
        const float target = minCps + static_cast<float>(u) * (maxCps - minCps);
        plusCpsState = std::clamp(0.5f * plusCpsState + 0.5f * target, minCps, maxCps);
        plusStateClicksRemaining = 4 + static_cast<int>(secureUnit() * 9.0);  
    }
    --plusStateClicksRemaining;
    const float center = (minCps + maxCps) * 0.5f;
    const float step = static_cast<float>(secureUnit() - secureUnit()) * (maxCps - minCps) * tempoStepScale;
    plusCpsState = std::clamp(plusCpsState + step + (center - plusCpsState) * tempoPull, minCps, maxCps);

    
    
    
    const double rangeInfluence = std::clamp(logRange / 1.5, 0.0, 1.0);
    const double baseInterval = 1000.0 / std::max(static_cast<double>(plusCpsState), 1.0);
    const double rawTempoMultiplier =
        std::max(static_cast<double>(fatigueMultiplier) * static_cast<double>(burstSpeedMultiplier), 0.1);
    double logInterval = std::log(baseInterval) + std::log(rawTempoMultiplier) * rangeInfluence;

    
    
    
    updateSigmaState();
    const double u1 = std::max(1e-9, secureUnit());
    const double u2 = secureUnit();
    const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    
    
    const double sigmaStateNormalized = std::clamp(
        (static_cast<double>(sigmaState + sigmaSessionOffset) - 0.08) / 0.36, 0.0, 1.0);
    
    
    const double sigmaScale = 0.20 + sigmaStateNormalized * 0.20;
    const double sigma = std::min(logRange * sigmaScale, 0.42);
    logInterval += sigma * z - 0.5 * sigma * sigma;

    
    
    
    const double pinkScale = std::clamp(logRange / 0.5, 0.0, 1.0);
    logInterval += static_cast<double>(nextPink()) * pinkAmplitude * pinkScale;

    
    
    const double q = secureUnit();
    if (q < hesitationChance) {
        
        const double amount = 0.25 + secureUnit() * 0.50;
        logInterval += (maxLogInterval - logInterval) * amount;
    } else if (q < hesitationChance + doubleTapChance) {
        
        const double amount = 0.10 + secureUnit() * 0.20;
        logInterval = minLogInterval + (logInterval - minLogInterval) * amount;
    }

    
    
    logInterval = std::clamp(logInterval, minLogInterval, maxLogInterval);
    return static_cast<float>(std::exp(logInterval));
}

void AutoClicker::doClick() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd)
        return;

    
    if (lastClickWasDown) {
        PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
        lastClickWasDown = false;
    } else {
        PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
        lastClickWasDown = true;
    }
}

bool AutoClicker::isHoldingWeapon() {
    int id = getHeldItemId();
    if (id == -1)
        return false;

    
    if (id == ItemIds::WOOD_SWORD || id == ItemIds::STONE_SWORD || id == ItemIds::IRON_SWORD ||
        id == ItemIds::DIAMOND_SWORD || id == ItemIds::GOLD_SWORD)
        return true;

    
    if (id == ItemIds::WOOD_AXE || id == ItemIds::STONE_AXE || id == ItemIds::IRON_AXE || id == ItemIds::DIAMOND_AXE ||
        id == ItemIds::GOLD_AXE)
        return true;

    return false;
}

bool AutoClicker::isTargetingBlock() {
    return mc.isTargetingBlock();
}

bool AutoClicker::isTargetingPlayer() {
    
    

    JNIEnv* env = Java::Env();
    if (!env)
        return false;

    
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (isTargetingBlock())
        return false;

    
    auto players = mc.getPlayerList();
    if (players.empty())
        return false;

    float playerX = static_cast<float>(player.getPosX());
    float playerZ = static_cast<float>(player.getPosZ());

    bool found = false;
    for (auto& p : players) {
        
        float dx = static_cast<float>(p.posX) - playerX;
        float dz = static_cast<float>(p.posZ) - playerZ;
        float distSq = dx * dx + dz * dz;

        
        if (distSq < 20.25f) {  
            found = true;
        }

        
        if (p.instance) {
            env->DeleteGlobalRef(p.instance);
            p.instance = nullptr;
        }
    }

    
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return found;
}

int AutoClicker::getHeldItemId() {
    return player.getHeldItemId();
}

void AutoClicker::updateFatigue() {
    const auto sessionDuration =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - sessionStart).count();
    const float sessionLoad = std::min(static_cast<float>(sessionDuration) / 60.0f * 0.004f, 0.10f);

    if (fatiguePhaseClicksRemaining <= 0) {
        if (recoveringFromFatigue) {
            
            
            recoveringFromFatigue = false;
            std::uniform_real_distribution<float> exhaustionDist(0.08f, 0.18f);
            std::uniform_int_distribution<int> lengthDist(90, 220);
            fatigueTargetMultiplier = 1.0f + sessionLoad + exhaustionDist(rng);
            fatiguePhaseClicksRemaining = lengthDist(rng);
        } else {
            
            recoveringFromFatigue = true;
            std::uniform_real_distribution<float> recoveryDist(0.25f, 0.55f);
            std::uniform_int_distribution<int> lengthDist(40, 100);
            fatigueTargetMultiplier = 1.0f + sessionLoad * recoveryDist(rng);
            fatiguePhaseClicksRemaining = lengthDist(rng);
        }
    }

    --fatiguePhaseClicksRemaining;
    const float response = recoveringFromFatigue ? 0.045f : 0.018f;
    fatigueMultiplier += (fatigueTargetMultiplier - fatigueMultiplier) * response;
    fatigueMultiplier = std::clamp(fatigueMultiplier, 1.0f, 1.30f);
}

void AutoClicker::updateBurstMode() {
    
    

    if (burstClicksRemaining > 0) {
        burstClicksRemaining--;
        if (burstClicksRemaining == 0) {
            inBurst = false;
            burstSpeedMultiplier = 1.0f;
        }
        return;
    }

    
    std::uniform_int_distribution<int> burstChanceDist(0, 100);
    if (burstChanceDist(rng) > 8)
        return;  

    inBurst = true;

    
    std::uniform_int_distribution<int> lengthDist(3, 12);
    burstClicksRemaining = lengthDist(rng);

    
    std::uniform_int_distribution<int> typeDist(0, 100);
    if (typeDist(rng) < 70) {
        
        std::uniform_real_distribution<float> fastDist(0.85f, 0.95f);
        burstSpeedMultiplier = fastDist(rng);
    } else {
        
        std::uniform_real_distribution<float> slowDist(1.05f, 1.20f);
        burstSpeedMultiplier = slowDist(rng);
    }
}

void AutoClicker::updateSigmaState() {
    
    
    

    if (sigmaClicksRemaining <= 0) {
        std::uniform_real_distribution<float> targetDist(0.08f, 0.44f);
        std::uniform_int_distribution<int> lengthDist(40, 180);
        sigmaTarget = targetDist(rng);
        sigmaClicksRemaining = lengthDist(rng);
    }

    --sigmaClicksRemaining;
    sigmaState += (sigmaTarget - sigmaState) * 0.025f;
    sigmaState = std::clamp(sigmaState, 0.08f, 0.44f);
}

float AutoClicker::nextPink() {
    
    
    
    const int prev = pinkCounter;
    ++pinkCounter;
    const int changed = prev ^ pinkCounter;
    for (int bit = 0; bit < 16; ++bit) {
        if (changed & (1 << bit))
            pinkRows[bit] = static_cast<float>(secureUnit()) * 2.0f - 1.0f;
    }
    float sum = 0.0f;
    for (const float row : pinkRows)
        sum += row;
    const float target = sum / 16.0f;
    pinkState += (target - pinkState) * 0.4f;
    return pinkState;
}

void AutoClicker::updateShapeParams() {
    
    
    
    
    

    if (shapePhaseClicksRemaining > 0) {
        --shapePhaseClicksRemaining;
        return;
    }

    std::uniform_real_distribution<float> stepDist(0.08f, 0.24f);   
    std::uniform_real_distribution<float> pullDist(0.02f, 0.12f);   
    std::uniform_real_distribution<float> hesDist(0.01f, 0.06f);    
    std::uniform_real_distribution<float> tapDist(0.005f, 0.04f);   
    std::uniform_int_distribution<int> lengthDist(60, 200);

    tempoStepScale = stepDist(rng);
    tempoPull = pullDist(rng);
    hesitationChance = hesDist(rng);
    doubleTapChance = tapDist(rng);
    shapePhaseClicksRemaining = lengthDist(rng);
}

void AutoClicker::onRenderMenu() {
    static const char* randomizationModes[] = {"Random", "SecureRandom", "Human"};
    bool enabledValue = enabled;
    int randomizationMode = static_cast<int>(settings.randomizationMode);
    ui::two_column(
        "General",
        [this, &enabledValue, &randomizationMode]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();

            if (settings.stableCPS) {
                
                int cpsInt = static_cast<int>(settings.stableCPSValue);
                if (ui::SliderInt("CPS", &cpsInt, 1, 25, "%d")) {
                    settings.stableCPSValue = static_cast<float>(cpsInt);
                    
                    stableTimelineStart = std::chrono::steady_clock::now();
                    stableClickCount = 0;
                }
            } else {
                
                float minLimit = 1.0f;
                float maxLimit = 20.0f;
                ui::SliderFloat("Min CPS", &settings.minCPS, 1.0f, 20.0f, &minLimit, &settings.maxCPS, "%.1f");
                ui::SliderFloat("Max CPS", &settings.maxCPS, 1.0f, 20.0f, &settings.minCPS, &maxLimit, "%.1f");
                (void)randomizationMode;
                (void)randomizationModes;
                ui::Checkbox("Randomization", &settings.randomization);
            }
        },
        "Conditions",
        [this]() {
            ui::Checkbox("Stable CPS", &settings.stableCPS);
            ui::Checkbox("Weapon Only", &settings.weaponOnly);
            ui::Checkbox("Break Blocks", &settings.breakBlocks);
            ui::Checkbox("Inventory", &settings.workInInventory);
        });
}

std::string AutoClicker::getDisplayInfo() {
    if (settings.stableCPS)
        return "Stable";
    return "";
}



void AutoClicker::saveSettings(json::Value& node) const {
    node["minCps"] = settings.minCPS;
    node["maxCps"] = settings.maxCPS;
    node["weaponOnly"] = settings.weaponOnly;
    node["breakBlocks"] = settings.breakBlocks;
    node["workInInventory"] = settings.workInInventory;
    node["randomizationMode"] = static_cast<int>(settings.randomizationMode);
    node["randomization"] = settings.randomization;
    node["stableCps"] = settings.stableCPS;
    node["stableCpsValue"] = settings.stableCPSValue;
    node["advancedMode"] = settings.advancedMode;
    node["dropChance"] = settings.dropChance;
    node["spikeChance"] = settings.spikeChance;
    node["spikeMultiplier"] = settings.spikeMultiplier;
    node["kurtosis"] = settings.kurtosis;
    node["attackingOnly"] = settings.attackingOnly;
}

void AutoClicker::loadSettings(const json::Value& node) {
    settings.minCPS = node["minCps"].as_float(settings.minCPS);
    settings.maxCPS = node["maxCps"].as_float(settings.maxCPS);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
    settings.breakBlocks = node["breakBlocks"].as_bool(settings.breakBlocks);
    settings.workInInventory = node["workInInventory"].as_bool(settings.workInInventory);
    settings.randomizationMode = static_cast<RandomizationMode>(
        (std::clamp)(node["randomizationMode"].as_int(static_cast<int>(settings.randomizationMode)), 0, 2));
    settings.randomization = node["randomization"].as_bool(settings.randomization);
    settings.stableCPS = node["stableCps"].as_bool(settings.stableCPS);
    settings.stableCPSValue = node["stableCpsValue"].as_float(settings.stableCPSValue);
    settings.advancedMode = node["advancedMode"].as_bool(settings.advancedMode);
    settings.dropChance = node["dropChance"].as_float(settings.dropChance);
    settings.spikeChance = node["spikeChance"].as_float(settings.spikeChance);
    settings.spikeMultiplier = node["spikeMultiplier"].as_float(settings.spikeMultiplier);
    settings.kurtosis = node["kurtosis"].as_float(settings.kurtosis);
    settings.attackingOnly = node["attackingOnly"].as_bool(settings.attackingOnly);
}
