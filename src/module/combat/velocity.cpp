#include "velocity.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <random>

Velocity* velocity = nullptr;

static std::mt19937 rng(std::random_device{}());

namespace {


bool isPhysicalKeyDown(int lwjglKey) {
    const UINT virtualKey = MapVirtualKeyA(static_cast<UINT>(lwjglKey), MAPVK_VSC_TO_VK);
    return virtualKey != 0 && (GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
}
}  

Velocity::Velocity() : Module("Velocity") {
    keybind = 0;
}

void Velocity::onEnable() {
    lastHurtTime = 0;
    queuedHit.active = false;
    pending.active = false;
    legitTicks = 0;
    inputsHeld = false;
}

void Velocity::onDisable() {
    queuedHit.active = false;
    pending.active = false;
    legitTicks = 0;
    releaseInputs();
}

bool Velocity::checkChance(float chance) {
    if (chance >= 100.0f)
        return true;
    if (chance <= 0.0f)
        return false;
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rng) <= static_cast<int>(std::round(chance));
}

bool Velocity::checkFOV() {
    
    if (settings.fov >= 360.0f)
        return true;

    
    
    double motionX = player.getMotionX();
    double motionZ = player.getMotionZ();

    if (motionX == 0 && motionZ == 0)
        return true;

    
    float kbAngle = (float)(atan2(-motionX, motionZ) * 180.0 / 3.14159265);
    float playerYaw = player.getYaw();

    
    while (kbAngle < 0)
        kbAngle += 360;
    while (playerYaw < 0)
        playerYaw += 360;

    
    float angleDiff = abs(kbAngle - playerYaw);
    if (angleDiff > 180)
        angleDiff = 360 - angleDiff;

    
    
    float halfFov = settings.fov / 2.0f;
    return angleDiff >= (180.0f - halfFov);
}

bool Velocity::isLookingAtPlayer() {
    return mc.isTargetingEntity();
}

bool Velocity::isHoldingWeapon() {
    int itemId = player.getHeldItemId();
    
    
    return (itemId == 268 || itemId == 272 || itemId == 267 || itemId == 276 || itemId == 283 || itemId == 271 ||
            itemId == 275 || itemId == 258 || itemId == 279 || itemId == 286);
}

bool Velocity::checkConditions() {
    
    if (settings.onlyLookingAtPlayer && !isLookingAtPlayer())
        return false;

    if (settings.onlyMousePressed) {
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
            return false;
    }

    if (settings.onlyMovingForward) {
        if (!(GetAsyncKeyState('W') & 0x8000))
            return false;
    }

    if (settings.onlyHoldingWeapon && !isHoldingWeapon())
        return false;

    if (settings.onlySprinting && !player.isSprinting())
        return false;

    return true;
}

void Velocity::queueHit(int hurtTime) {
    if (!checkFOV() || !checkConditions())
        return;

    if (!checkChance(settings.chance))
        return;

    std::uniform_real_distribution<float> variance(-settings.percentageVariance, settings.percentageVariance);
    queuedHit.active = true;
    queuedHit.mode = settings.mode;
    queuedHit.applyAtHurtTime = (std::max)(0, hurtTime - settings.reduceDelay);
    queuedHit.horizontal = (std::clamp)(settings.horizontal + variance(rng), 0.0f, 100.0f);
    queuedHit.vertical = (std::clamp)(settings.vertical + variance(rng), 0.0f, 100.0f);

    if (settings.mode == Mode::Ping) {
        
        
        std::uniform_int_distribution<int> jitter(-10, 10);
        int d = (std::max)(0, settings.pingDelay + jitter(rng));
        queuedHit.applyAtMs = GetTickCount() + static_cast<unsigned long>(d);
    } else {
        int microDelay = 0;
        if (settings.microDelayMs > 0) {
            std::uniform_int_distribution<int> delay(0, settings.microDelayMs);
            microDelay = delay(rng);
        }
        queuedHit.applyAtMs = GetTickCount() + static_cast<unsigned long>(microDelay);
    }
}

void Velocity::applyQueuedHit(int hurtTime) {
    const PendingHit hit = queuedHit;
    queuedHit.active = false;

    switch (hit.mode) {
            case Mode::Ping:
            case Mode::Regular: {
                if (settings.tickSpread > 0) {
                    pending.active = true;
                    pending.totalTicks = settings.tickSpread + 1;
                    pending.ticksRemaining = settings.tickSpread;
                    double motionX = player.getMotionX();
                    double motionY = player.getMotionY();
                    double motionZ = player.getMotionZ();
                    pending.targetMotionX = motionX * (hit.horizontal / 100.0);
                    pending.targetMotionY = motionY * (hit.vertical / 100.0);
                    pending.targetMotionZ = motionZ * (hit.horizontal / 100.0);
                    pending.lastHurtTime = hurtTime;
                    double newX = motionX - ((motionX - pending.targetMotionX) / pending.totalTicks);
                    double newY = motionY - ((motionY - pending.targetMotionY) / pending.totalTicks);
                    double newZ = motionZ - ((motionZ - pending.targetMotionZ) / pending.totalTicks);
                    player.setMotionX(newX);
                    player.setMotionY(newY);
                    player.setMotionZ(newZ);
                } else {
                    pending.active = false;
                    player.setMotionX(player.getMotionX() * (hit.horizontal / 100.0));
                    player.setMotionY(player.getMotionY() * (hit.vertical / 100.0));
                    player.setMotionZ(player.getMotionZ() * (hit.horizontal / 100.0));
                }
                break;
            }

            case Mode::Ignore: {
                player.setMotionX(0);
                player.setMotionZ(0);
                switch (settings.verticalMode) {
                    case VerticalMode::Never:
                        break;
                    case VerticalMode::OnlyInAir:
                        if (!player.isOnGround())
                            player.setMotionY(0);
                        break;
                    case VerticalMode::Always:
                        player.setMotionY(0);
                        break;
                }
                break;
            }

            default:
                break;
    }
}

void Velocity::continuePendingReduction(int hurtTime) {
    if (!pending.active || pending.ticksRemaining <= 0 || pending.lastHurtTime == hurtTime)
        return;

    pending.lastHurtTime = hurtTime;
    if (pending.active && pending.ticksRemaining > 0) {
        double currentX = player.getMotionX();
        double currentY = player.getMotionY();
        double currentZ = player.getMotionZ();

        double xDiff = (currentX - pending.targetMotionX) / (pending.ticksRemaining + 1);
        double yDiff = (currentY - pending.targetMotionY) / (pending.ticksRemaining + 1);
        double zDiff = (currentZ - pending.targetMotionZ) / (pending.ticksRemaining + 1);

        player.setMotionX(currentX - xDiff);
        player.setMotionY(currentY - yDiff);
        player.setMotionZ(currentZ - zDiff);

        pending.ticksRemaining--;

        if (pending.ticksRemaining == 0) {
            pending.active = false;
        }
    }
}

void Velocity::releaseInputs() {
    if (!inputsHeld)
        return;
    
    
    if (player.isValid())
        mc.setKeyBindState(mc.getJumpKey(), isPhysicalKeyDown(mc.getJumpKey()));
    inputsHeld = false;
}

void Velocity::handleLegit(int hurtTime, bool justGotHit) {
    
    if (justGotHit && checkFOV() && checkConditions() && checkChance(settings.jumpChance))
        legitTicks = settings.legitWindow;

    if (legitTicks <= 0) {
        releaseInputs();
        return;
    }

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    bool wantJump = false;
    if (player.isOnGround()) {
        if (settings.mode == Mode::LegitAbuse)
            wantJump = true;  
        else
            wantJump = (legitTicks >= settings.legitWindow - 1);  
    }

    if (wantJump) {
        mc.setKeyBindState(mc.getJumpKey(), true);
        inputsHeld = true;
    } else {
        releaseInputs();
    }

    legitTicks--;
    if (legitTicks <= 0)
        releaseInputs();
}

void Velocity::onUpdate() {
    if (!enabled || Menu::isOpen || mc.isInGui() || !player.isValid()) {
        releaseInputs();
        return;
    }

    const int currentHurtTime = player.getHurtTime();
    const bool justGotHit = currentHurtTime > 0 && currentHurtTime > lastHurtTime;

    if (settings.mode == Mode::Legit || settings.mode == Mode::LegitAbuse) {
        handleLegit(currentHurtTime, justGotHit);
    } else {
        if (justGotHit)
            queueHit(currentHurtTime);

        const bool tickDelayElapsed = queuedHit.active && currentHurtTime <= queuedHit.applyAtHurtTime;
        const bool microDelayElapsed = queuedHit.active &&
                                       static_cast<long>(GetTickCount() - queuedHit.applyAtMs) >= 0;
        if (tickDelayElapsed && microDelayElapsed)
            applyQueuedHit(currentHurtTime);

        continuePendingReduction(currentHurtTime);
    }

    lastHurtTime = currentHurtTime;
}

float Velocity::getAttackerAngle() {
    
    
    return 0.0f;
}

void Velocity::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();

            
            const char* modes[] = {"Regular", "Legit", "LegitAbuse", "Ignore", "Ping"};
            int modeIdx = static_cast<int>(settings.mode);
            if (ui::Combo("Mode", &modeIdx, modes, IM_ARRAYSIZE(modes)))
                settings.mode = static_cast<Mode>(modeIdx);

            ImGui::Spacing();

            
            if (settings.mode == Mode::Regular) {
                ui::SliderFloat("Horizontal", &settings.horizontal, 0.0f, 100.0f, "%.0f%%");
                ui::SliderFloat("Vertical", &settings.vertical, 0.0f, 100.0f, "%.0f%%");
                ui::SliderFloat("Chance", &settings.chance, 0.0f, 100.0f, "%.0f%%");
            } else if (settings.mode == Mode::Legit) {
                ui::SliderFloat("Chance", &settings.jumpChance, 0.0f, 100.0f, "%.0f%%");
                ui::SliderInt("Window", &settings.legitWindow, 4, 20, "%d ticks");
                ui::TickTooltip();
            } else if (settings.mode == Mode::LegitAbuse) {
                ui::SliderFloat("Chance", &settings.jumpChance, 0.0f, 100.0f, "%.0f%%");
                ui::SliderInt("Window", &settings.legitWindow, 4, 20, "%d ticks");
                ui::TickTooltip();
            } else if (settings.mode == Mode::Ignore) {
                ui::SliderFloat("Chance", &settings.chance, 0.0f, 100.0f, "%.0f%%");
                const char* vertModes[] = {"Never", "Only Air", "Always"};
                int vertIdx = static_cast<int>(settings.verticalMode);
                if (ui::Combo("Vertical", &vertIdx, vertModes, IM_ARRAYSIZE(vertModes)))
                    settings.verticalMode = static_cast<VerticalMode>(vertIdx);
            } else if (settings.mode == Mode::Ping) {
                ui::SliderInt("Ping", &settings.pingDelay, 0, 400, "%d ms");
                ui::SliderFloat("Horizontal", &settings.horizontal, 0.0f, 100.0f, "%.0f%%");
                ui::SliderFloat("Vertical", &settings.vertical, 0.0f, 100.0f, "%.0f%%");
                ui::SliderFloat("Chance", &settings.chance, 0.0f, 100.0f, "%.0f%%");
            }

            ui::SliderFloat("FOV", &settings.fov, 0.0f, 360.0f, "%.0f");

            int delay = settings.reduceDelay;
            if (ui::SliderInt("Delay", &delay, 0, 5, "%d ticks"))
                settings.reduceDelay = delay;
            ui::TickTooltip();
        },
        "Settings",
        [this]() {
            
            if (settings.mode == Mode::Regular) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                ImGui::Text("Anti-Detection");
                ImGui::PopStyleColor();
                ImGui::Spacing();

                ui::SliderFloat("Variance", &settings.percentageVariance, 0.0f, 10.0f, "%.1f%%");

                int spread = settings.tickSpread;
                if (ui::SliderInt("Tick Spread", &spread, 0, 3, "%d"))
                    settings.tickSpread = spread;
                ui::TickTooltip();

                int microDelay = settings.microDelayMs;
                if (ui::SliderInt("Micro Delay", &microDelay, 0, 50, "%dms"))
                    settings.microDelayMs = microDelay;

                ImGui::Spacing();
            }

            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::Text("Conditions");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ui::Checkbox("Looking at Player", &settings.onlyLookingAtPlayer);
            ui::Checkbox("Mouse Pressed", &settings.onlyMousePressed);
            ui::Checkbox("Moving Forward", &settings.onlyMovingForward);
            ui::Checkbox("Holding Weapon", &settings.onlyHoldingWeapon);
            if (settings.mode == Mode::Legit || settings.mode == Mode::LegitAbuse) {
                ui::Checkbox("Only Sprinting", &settings.onlySprinting);
            }
        });
}

std::string Velocity::getDisplayInfo() {
    switch (settings.mode) {
        case Mode::Regular:
            return "Regular";
        case Mode::Legit:
            return "Legit";
        case Mode::LegitAbuse:
            return "LegitAbuse";
        case Mode::Ignore:
            return "Ignore";
        case Mode::Ping:
            return "Ping";
        default:
            return "";
    }
}



void Velocity::saveSettings(json::Value& node) const {
    node["mode"] = static_cast<int>(settings.mode);
    node["horizontal"] = settings.horizontal;
    node["vertical"] = settings.vertical;
    node["chance"] = settings.chance;
    node["fov"] = settings.fov;
    node["reduceDelay"] = settings.reduceDelay;
    node["jumpChance"] = settings.jumpChance;
    node["forwardTap"] = settings.forwardTap;
    node["legitWindow"] = settings.legitWindow;
    node["pingDelay"] = settings.pingDelay;
    node["verticalMode"] = static_cast<int>(settings.verticalMode);
    node["onlyLookingAtPlayer"] = settings.onlyLookingAtPlayer;
    node["onlyMousePressed"] = settings.onlyMousePressed;
    node["onlyMovingForward"] = settings.onlyMovingForward;
    node["onlyHoldingWeapon"] = settings.onlyHoldingWeapon;
    node["onlySprinting"] = settings.onlySprinting;
    node["percentageVariance"] = settings.percentageVariance;
    node["tickSpread"] = settings.tickSpread;
    node["microDelayMs"] = settings.microDelayMs;
}

void Velocity::loadSettings(const json::Value& node) {
    settings.mode = static_cast<Mode>(node["mode"].as_int(static_cast<int>(settings.mode)));
    settings.horizontal = node["horizontal"].as_float(settings.horizontal);
    settings.vertical = node["vertical"].as_float(settings.vertical);
    settings.chance = node["chance"].as_float(settings.chance);
    settings.fov = node["fov"].as_float(settings.fov);
    settings.reduceDelay = node["reduceDelay"].as_int(settings.reduceDelay);
    settings.jumpChance = node["jumpChance"].as_float(settings.jumpChance);
    settings.forwardTap = node["forwardTap"].as_bool(settings.forwardTap);
    settings.legitWindow = node["legitWindow"].as_int(settings.legitWindow);
    settings.pingDelay = node["pingDelay"].as_int(settings.pingDelay);
    settings.verticalMode =
        static_cast<VerticalMode>(node["verticalMode"].as_int(static_cast<int>(settings.verticalMode)));
    settings.onlyLookingAtPlayer = node["onlyLookingAtPlayer"].as_bool(settings.onlyLookingAtPlayer);
    settings.onlyMousePressed = node["onlyMousePressed"].as_bool(settings.onlyMousePressed);
    settings.onlyMovingForward = node["onlyMovingForward"].as_bool(settings.onlyMovingForward);
    settings.onlyHoldingWeapon = node["onlyHoldingWeapon"].as_bool(settings.onlyHoldingWeapon);
    settings.onlySprinting = node["onlySprinting"].as_bool(settings.onlySprinting);
    settings.percentageVariance = node["percentageVariance"].as_float(settings.percentageVariance);
    settings.tickSpread = node["tickSpread"].as_int(settings.tickSpread);
    settings.microDelayMs = node["microDelayMs"].as_int(settings.microDelayMs);
}
