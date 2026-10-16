#include "clutch.h"

#include "../../../infra/ext/imgui/imgui.h"
#include "../../module/combat/autorod.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/render_data.h"
#include "../../sdk/player/visual_cache.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/inject_trace.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"

#include <cstdio>


#ifdef COBBLE_TRACE
#undef COBBLE_TRACE
#define COBBLE_TRACE(step) COBBLE_TRACE_ONCE(step)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

Clutch* scaffold = nullptr;
Clutch* clutch = nullptr;

namespace {
constexpr double kPi = 3.14159265358979323846;



constexpr std::uint64_t kVanillaUseIntervalMs = 200;




constexpr float kAimToleranceDeg = 4.0f;



constexpr int kMaxAimTicks = 12;





constexpr int kMinLookaheadTicks = 4;







constexpr int kPreemptiveDepth = 5;

struct Direction {
    int x;
    int y;
    int z;
};

constexpr std::array<Direction, 6> kDirections = {
    Direction{1, 0, 0}, Direction{-1, 0, 0}, Direction{0, 1, 0},
    Direction{0, -1, 0}, Direction{0, 0, 1}, Direction{0, 0, -1}};



inline void stepPhysics(double& x, double& y, double& z, double& mx, double& my, double& mz) {
    x += mx;
    y += my;
    z += mz;
    mx *= 0.91;
    mz *= 0.91;
    my = (my - 0.08) * 0.98;
}

inline bool finite3(double a, double b, double c) {
    return !std::isnan(a) && !std::isnan(b) && !std::isnan(c) && !std::isinf(a) && !std::isinf(b) &&
           !std::isinf(c);
}

}  

Clutch::Clutch(PlaceMode mode)
    : Module(mode == PlaceMode::Clutch ? "Clutch" : "Scaffold"), placeMode_(mode) {
    renders = true;
    worldVisual = true;  
    usesPlayerCache = true;
    
    
    
    
    
    
    
    
    
    
    locked = true;
    if (isScaffold()) {
        
        
        settings.aimTicks = 2;
        settings.snapBack = false;
        settings.reactMinMs = 60;
        settings.reactMaxMs = 140;
    }
}

Clutch::~Clutch() = default;

std::uint64_t Clutch::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

float Clutch::wrapDegrees(float angle) {
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}



float Clutch::smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

int Clutch::rollReactionMs() {
    const int lo = std::clamp(settings.reactMinMs, 0, 1000);
    const int hi = std::clamp(settings.reactMaxMs, lo, 1000);
    if (hi <= lo)
        return lo;
    
    
    const int a = lo + static_cast<int>(rng_() % static_cast<unsigned>(hi - lo + 1));
    const int b = lo + static_cast<int>(rng_() % static_cast<unsigned>(hi - lo + 1));
    return (a + b) / 2;
}

bool Clutch::rollMiss() {
    const int chance = std::clamp(settings.missChance, 0, 90);
    return chance > 0 && static_cast<int>(rng_() % 100u) < chance;
}

void Clutch::onEnable() {
    phase_ = Phase::Idle;
    target_ = {};
    slot_ = -1;
    originalSlot_ = -1;
    reactDeadlineMs_ = 0;
    restoreSlotAtMs_ = 0;
    nextPlaceMs_ = 0;
    aimTick_ = 0;
    aimTotalTicks_ = 0;
    haveReturnView_ = false;
    placesThisRun_ = 0;
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
    publishPreview({});
}

void Clutch::onDisable() {
    
    
    
    status_.store(nullptr, std::memory_order_release);
    std::lock_guard<std::mutex> lock(pathMutex_);
    cachedPath_.clear();
    cachedPlacement_ = {};
}

void Clutch::onWorldChange() {
    
    
    target_ = {};
    phase_ = Phase::Idle;
    nextPlaceMs_ = 0;
    placesThisRun_ = 0;
    haveReturnView_ = false;
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
    publishPreview({});
}

void Clutch::restoreSlot() {
    if (settings.restoreSlot && originalSlot_ >= 0 && originalSlot_ < 9 && player.isValid())
        player.setCurrentSlot(originalSlot_);
    originalSlot_ = -1;
}



void Clutch::abort(JNIEnv* env) {
    (void)env;
    restoreSlot();
    phase_ = Phase::Idle;
    target_ = {};
    slot_ = -1;
    aimTick_ = 0;
    aimTotalTicks_ = 0;
    haveReturnView_ = false;
    placesThisRun_ = 0;
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
    publishPreview({});
}

void Clutch::publishPreview(const Placement& p) {
    std::lock_guard<std::mutex> lock(pathMutex_);
    cachedPlacement_ = p;
}



bool Clutch::isPlaceableBlock(int id) {
    if (id <= 0 || id > 255)
        return false;
    static constexpr int kRejected[] = {
        6,   8,   9,   10,  11,  26,  27,  28,  30,  31,  32,  37,  38,  39,  40,
        50,  51,  55,  59,  63,  64,  65,  66,  68,  69,  70,  71,  72,  75,  76,
        77,  78,  83,  90,  93,  94,  96,  104, 105, 106, 111, 115, 119, 127, 131,
        132, 140, 141, 142, 143, 147, 148, 149, 150, 157, 171, 175};
    for (int rejected : kRejected) {
        if (rejected == id)
            return false;
        if (rejected > id)
            break;  
    }
    return true;
}

int Clutch::findBlockSlot() const {
    
    
    const int held = player.getCurrentSlot();
    if (held >= 0 && held < 9 && isPlaceableBlock(player.getItemInSlot(held)))
        return held;
    for (int slot = 0; slot < 9; ++slot) {
        if (isPlaceableBlock(player.getItemInSlot(slot)))
            return slot;
    }
    return -1;
}






bool Clutch::isDangerous(double predictedX, double predictedY, double predictedZ) const {
    if (!finite3(predictedX, predictedY, predictedZ))
        return false;

    int depth = std::clamp(settings.safeDepth, 1, 24);
    if (!settings.allowForward && player.isOnGround()) {
        
        
        
        
        
        
        
        
        depth = (std::max)(depth, kPreemptiveDepth);
    }

    const int footY = static_cast<int>(std::floor(player.getPosY())) - 1;
    if (footY < 0)
        return true;  

    const int lowest = std::max(0, footY - depth);

    
    
    static constexpr double kOffsets[] = {-0.3, 0.0, 0.3};
    for (double offsetX : kOffsets) {
        for (double offsetZ : kOffsets) {
            const int x = static_cast<int>(std::floor(predictedX + offsetX));
            const int z = static_cast<int>(std::floor(predictedZ + offsetZ));
            for (int y = footY; y >= lowest; --y) {
                if (mc.getBlockId(x, y, z) != 0)
                    return false;  
            }
        }
    }
    return true;
}

bool Clutch::isPlacementUsable(const Placement& p) const {
    if (!p.valid)
        return false;
    if (mc.getBlockId(p.targetX, p.targetY, p.targetZ) != 0)
        return false;  
    if (mc.getBlockId(p.supportX, p.supportY, p.supportZ) == 0)
        return false;  

    
    
    const double px = player.getPosX();
    const double py = player.getPosY();
    const double pz = player.getPosZ();
    if (p.targetX + 1 > px - 0.3 && p.targetX < px + 0.3 && p.targetZ + 1 > pz - 0.3 &&
        p.targetZ < pz + 0.3 && p.targetY + 1 > py && p.targetY < py + 1.8) {
        return false;
    }

    const double dx = p.hitX - px;
    const double dy = p.hitY - (py + 1.62);
    const double dz = p.hitZ - pz;
    const double reach = static_cast<double>(std::clamp(settings.reach, 3.0f, 4.5f));
    return dx * dx + dy * dy + dz * dz <= reach * reach;
}

Clutch::Placement Clutch::findPlacement(double predictedX, double predictedY,
                                        double predictedZ) const {
    Placement best;
    best.score = std::numeric_limits<double>::max();

    const double eyeX = player.getPosX();
    const double eyeY = player.getPosY() + 1.62;
    const double eyeZ = player.getPosZ();
    const float curYaw = player.getYaw();
    const float curPitch = player.getPitch();

    const int centerX = static_cast<int>(std::floor(predictedX));
    const int centerY = static_cast<int>(std::floor(predictedY)) - 1;
    const int centerZ = static_cast<int>(std::floor(predictedZ));
    const int radius = std::clamp(settings.searchRadius, 1, 6);
    const double maxReach = static_cast<double>(std::clamp(settings.reach, 3.0f, 4.5f));
    const double maxReachSq = maxReach * maxReach;

    for (int ring = 0; ring <= radius; ++ring) {
        for (int x = centerX - ring; x <= centerX + ring; ++x) {
            for (int z = centerZ - ring; z <= centerZ + ring; ++z) {
                if (std::max(std::abs(x - centerX), std::abs(z - centerZ)) != ring)
                    continue;
                for (int y = centerY - 1; y <= centerY + 1; ++y) {
                    if (y < 0 || mc.getBlockId(x, y, z) != 0)
                        continue;
                    for (const Direction& dir : kDirections) {
                        const int supportX = x - dir.x;
                        const int supportY = y - dir.y;
                        const int supportZ = z - dir.z;
                        if (supportY < 0 || mc.getBlockId(supportX, supportY, supportZ) == 0)
                            continue;

                        const double hitX = supportX + 0.5 + dir.x * 0.49;
                        const double hitY = supportY + 0.5 + dir.y * 0.49;
                        const double hitZ = supportZ + 0.5 + dir.z * 0.49;
                        const double dx = hitX - eyeX;
                        const double dy = hitY - eyeY;
                        const double dz = hitZ - eyeZ;
                        const double reachSq = dx * dx + dy * dy + dz * dz;
                        if (reachSq > maxReachSq)
                            continue;

                        const double landingDx = (x + 0.5) - predictedX;
                        const double landingDz = (z + 0.5) - predictedZ;

                        
                        
                        
                        double cameraPenalty = 0.0;
                        if (settings.cameraWeight) {
                            const double horiz = std::sqrt(dx * dx + dz * dz);
                            const float wantYaw =
                                static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
                            const float wantPitch = std::clamp(
                                static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f,
                                90.0f);
                            const float yawDiff = std::abs(wrapDegrees(wantYaw - curYaw));
                            const float pitchDiff = std::abs(wrapDegrees(wantPitch - curPitch));
                            cameraPenalty =
                                (yawDiff * yawDiff + pitchDiff * pitchDiff) * 0.0006;
                        }

                        const double score = landingDx * landingDx + landingDz * landingDz +
                                             reachSq * 0.04 + std::abs(y - centerY) * 0.8 +
                                             cameraPenalty;
                        if (score < best.score) {
                            best = {x,    y,    z,     supportX, supportY, supportZ,
                                    hitX, hitY, hitZ,  score,    true};
                        }
                    }
                }
            }
        }
        if (best.valid)
            break;  
    }
    return best.valid ? best : Placement{};
}


bool Clutch::wantsPlacement(Placement& out) const {
    out = {};

    const double x = player.getPosX();
    const double y = player.getPosY();
    const double z = player.getPosZ();
    if (!finite3(x, y, z))
        return false;

    if (isScaffold()) {
        
        
        const double ahead = std::clamp(settings.scaffoldLead, 0.4f, 1.6f);
        const double yawRad = static_cast<double>(player.getYaw()) * kPi / 180.0;
        const double targetX = x - std::sin(yawRad) * ahead;
        const double targetZ = z + std::cos(yawRad) * ahead;
        const int footY = static_cast<int>(std::floor(y)) - 1;
        if (footY < 0)
            return false;
        if (mc.getBlockId(static_cast<int>(std::floor(targetX)), footY,
                          static_cast<int>(std::floor(targetZ))) != 0) {
            return false;  
        }
        out = findPlacement(targetX, y, targetZ);
        return out.valid;
    }

    double px = x, py = y, pz = z;
    double mx = player.getMotionX(), my = player.getMotionY(), mz = player.getMotionZ();
    if (!finite3(mx, my, mz))
        return false;

    const int ticks = std::clamp((settings.predictionMs + 49) / 50, kMinLookaheadTicks, 16);
    for (int tick = 0; tick < ticks; ++tick)
        stepPhysics(px, py, pz, mx, my, mz);

    
    
    
    
    if (!isDangerous(px, py, pz) && !isDangerous(x, y, z))
        return false;

    out = findPlacement(px, py, pz);
    if (!out.valid)
        out = findPlacement(x, y, z);  
    return out.valid;
}





void Clutch::applyRotation(float yaw, float pitch) {
    const float curYaw = player.getYaw();
    const float curPitch = player.getPitch();
    const float sensitivity = mc.getMouseSensitivity();
    const float gf = sensitivity * 0.6f + 0.2f;
    const float gcd = gf * gf * gf * 1.2f;

    float outYaw = yaw;
    float outPitch = std::clamp(pitch, -90.0f, 90.0f);
    if (gcd > 0.0001f) {
        outYaw = curYaw + std::round(wrapDegrees(yaw - curYaw) / gcd) * gcd;
        outPitch = std::clamp(curPitch + std::round((outPitch - curPitch) / gcd) * gcd, -90.0f,
                              90.0f);
    }
    player.setYaw(outYaw);
    player.setPitch(outPitch);
}


void Clutch::aimAt(const Placement& p, bool freshOpportunity) {
    const double dx = p.hitX - player.getPosX();
    const double dy = p.hitY - (player.getPosY() + 1.62);
    const double dz = p.hitZ - player.getPosZ();
    const double horiz = std::sqrt(dx * dx + dz * dz);

    aimToYaw_ = static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
    aimToPitch_ =
        std::clamp(static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f, 90.0f);
    aimFromYaw_ = player.getYaw();
    aimFromPitch_ = player.getPitch();

    if (freshOpportunity && settings.snapBack && !haveReturnView_) {
        returnYaw_ = aimFromYaw_;
        returnPitch_ = aimFromPitch_;
        haveReturnView_ = true;
    }

    
    
    const float travel = std::max(std::abs(wrapDegrees(aimToYaw_ - aimFromYaw_)),
                                  std::abs(aimToPitch_ - aimFromPitch_));
    int budget = std::clamp(settings.aimTicks, 1, 6);
    if (travel < 8.0f)
        budget = 1;
    else if (travel > 60.0f)
        budget += 1;
    aimTotalTicks_ = std::clamp(budget, 1, kMaxAimTicks);
    aimTick_ = 0;
}



void Clutch::beginSettle() {
    phase_ = Phase::Settle;
    restoreSlotAtMs_ = nowMs() + 90 + static_cast<std::uint64_t>(rng_() % 150u);

    if (settings.snapBack && haveReturnView_) {
        aimFromYaw_ = player.getYaw();
        aimFromPitch_ = player.getPitch();
        aimToYaw_ = returnYaw_;
        aimToPitch_ = returnPitch_;
        aimTick_ = 0;
        aimTotalTicks_ = std::clamp(settings.aimTicks, 1, 6);
    } else {
        aimTick_ = 0;
        aimTotalTicks_ = 0;
    }
}

void Clutch::onClientTick(JNIEnv* env) {
    if (!env)
        return;

    
    
    
    if (!enabled || !player.isValid() || Menu::isOpen || mc.isInGui() ||
        (autoRod && autoRod->isSequencing())) {
        if (phase_ != Phase::Idle || originalSlot_ >= 0)
            abort(env);
        return;
    }

    const std::uint64_t now = nowMs();
    const float noiseAmp = std::clamp(settings.aimNoise, 0.0f, 3.0f);
    
    
    const auto noise = [this, noiseAmp]() -> float {
        if (noiseAmp <= 0.0f)
            return 0.0f;
        return (static_cast<int>(rng_() % 2001u) - 1000) / 1000.0f * noiseAmp;
    };

    
    if (phase_ == Phase::Settle) {
        if (aimTick_ < aimTotalTicks_) {
            ++aimTick_;
            const float t = smoothstep(static_cast<float>(aimTick_) /
                                       static_cast<float>(aimTotalTicks_));
            applyRotation(aimFromYaw_ + wrapDegrees(aimToYaw_ - aimFromYaw_) * t + noise(),
                          aimFromPitch_ + (aimToPitch_ - aimFromPitch_) * t + noise());
        }
        if (aimTick_ >= aimTotalTicks_ && now >= restoreSlotAtMs_) {
            restoreSlot();
            haveReturnView_ = false;
            placesThisRun_ = 0;
            target_ = {};
            slot_ = -1;
            phase_ = Phase::Idle;
            owningRotation_.store(false, std::memory_order_release);
            status_.store(nullptr, std::memory_order_release);
            publishPreview({});
        } else {
            status_.store("Settle", std::memory_order_release);
        }
        return;
    }

    Placement wanted;
    const bool want = wantsPlacement(wanted);

    if (!want) {
        
        
        if (phase_ == Phase::Idle) {
            status_.store(nullptr, std::memory_order_release);
            publishPreview({});
        } else {
            beginSettle();
        }
        return;
    }

    const int slot = findBlockSlot();
    if (slot < 0) {
        status_.store("No Blocks", std::memory_order_release);
        if (phase_ != Phase::Idle)
            beginSettle();
        else
            publishPreview({});
        return;
    }

    publishPreview(wanted);

    
    
    
    
    
    
    
    for (int hop = 0; hop < 3; ++hop) {
        switch (phase_) {
            case Phase::Idle: {
                if (now < nextPlaceMs_) {
                    status_.store("Idle", std::memory_order_release);
                    return;
                }
                if (rollMiss()) {
                    
                    
                    nextPlaceMs_ = now + 200u + (rng_() % 200u);
                    status_.store("Idle", std::memory_order_release);
                    return;
                }
                target_ = wanted;
                slot_ = slot;
                reactDeadlineMs_ = now + static_cast<std::uint64_t>(rollReactionMs());
                phase_ = Phase::React;
                owningRotation_.store(true, std::memory_order_release);
                status_.store("React", std::memory_order_release);
                break;  
            }

            case Phase::React: {
                if (now < reactDeadlineMs_) {
                    status_.store("React", std::memory_order_release);
                    return;
                }
                
                
                
                if (originalSlot_ < 0)
                    originalSlot_ = player.getCurrentSlot();
                slot_ = slot;
                if (player.getCurrentSlot() != slot_)
                    player.setCurrentSlot(slot_);

                if (!isPlacementUsable(target_))
                    target_ = wanted;
                aimAt(target_, true);
                phase_ = Phase::Aim;
                status_.store("Aim", std::memory_order_release);
                break;  
            }

            case Phase::Aim: {
                if (!isPlacementUsable(target_)) {
                    if (!isPlacementUsable(wanted)) {
                        beginSettle();
                        return;
                    }
                    target_ = wanted;
                    aimAt(target_, false);
                }

                ++aimTick_;
                const float t =
                    smoothstep(static_cast<float>(aimTick_) / static_cast<float>(aimTotalTicks_));
                
                
                float scale = 1.0f;
                if (settings.overshoot && aimTotalTicks_ >= 3 && aimTick_ == aimTotalTicks_ - 1)
                    scale = 1.05f + (rng_() % 70u) / 1000.0f;

                applyRotation(
                    aimFromYaw_ + wrapDegrees(aimToYaw_ - aimFromYaw_) * t * scale + noise(),
                    aimFromPitch_ + (aimToPitch_ - aimFromPitch_) * t * scale + noise());

                if (aimTick_ >= aimTotalTicks_) {
                    phase_ = Phase::Place;
                    aimTick_ = 0;  
                    status_.store("Place", std::memory_order_release);
                } else {
                    status_.store("Aim", std::memory_order_release);
                }
                return;
            }

            case Phase::Place: {
                if (!isPlacementUsable(target_)) {
                    if (!isPlacementUsable(wanted)) {
                        beginSettle();
                        return;
                    }
                    target_ = wanted;
                    aimAt(target_, false);
                    phase_ = Phase::Aim;
                    status_.store("Aim", std::memory_order_release);
                    return;
                }

                ++aimTick_;  
                if (aimTick_ > 24) {
                    
                    
                    beginSettle();
                    return;
                }

                
                
                
                
                
                
                
                if (now >= nextPlaceMs_ && mc.getRightClickDelay() == 0) {
                    mc.updateMouseOver();
                    if (mc.isTargetingBlock()) {
                        const Minecraft::Vec3 hit = mc.getTargetedBlockPos();
                        if (static_cast<int>(std::floor(hit.x)) == target_.supportX &&
                            static_cast<int>(std::floor(hit.y)) == target_.supportY &&
                            static_cast<int>(std::floor(hit.z)) == target_.supportZ) {
                            
                            
                            
                            mc.rightClickMouse();
                            ++placesThisRun_;
                            nextPlaceMs_ = now + kVanillaUseIntervalMs + (rng_() % 90u);

                            
                            
                            
                            target_ = {};
                            reactDeadlineMs_ = 0;
                            phase_ = Phase::React;
                            status_.store("React", std::memory_order_release);
                            return;
                        }
                    }
                }

                
                
                
                
                
                
                const double dx = target_.hitX - player.getPosX();
                const double dy = target_.hitY - (player.getPosY() + 1.62);
                const double dz = target_.hitZ - player.getPosZ();
                const double horiz = std::sqrt(dx * dx + dz * dz);
                const float holdYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
                const float holdPitch = std::clamp(
                    static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f, 90.0f);
                
                
                applyRotation(holdYaw + noise() * 0.25f, holdPitch + noise() * 0.25f);
                status_.store("Place", std::memory_order_release);
                return;
            }

            case Phase::Settle:
            default:
                return;
        }
    }
}






void Clutch::onUpdate() {
    lastUpdateTick_ = GetTickCount64();
    if (!enabled)
        return;

    if (Menu::isOpen || mc.isInGui() || !player.isValid()) {
        std::lock_guard<std::mutex> lock(pathMutex_);
        cachedPath_.clear();
        cachedPlacement_ = {};
        return;
    }

    const auto snap = VisualCache::GetSnapshot();
    if (snap.valid) {
        std::lock_guard<std::mutex> lock(pathMutex_);
        cachedLocalX_ = snap.localX;
        cachedLocalY_ = snap.localY;
        cachedLocalZ_ = snap.localZ;
        cachedLocalLastX_ = snap.localLastX;
        cachedLocalLastY_ = snap.localLastY;
        cachedLocalLastZ_ = snap.localLastZ;
        cachedRenderX_ = snap.renderPos.x;
        cachedRenderY_ = snap.renderPos.y;
        cachedRenderZ_ = snap.renderPos.z;
        cachedPartialTicks_ = snap.partialTicks;
        cachedPlayerValid_ = true;
        cachedMotionX_ = player.getMotionX();
        cachedMotionY_ = player.getMotionY();
        cachedMotionZ_ = player.getMotionZ();
    }

    if (!settings.showPath) {
        std::lock_guard<std::mutex> lock(pathMutex_);
        cachedPath_.clear();
        return;
    }

    
    const std::uint64_t now = nowMs();
    if (now < nextPathMs_)
        return;
    nextPathMs_ = now + 25u;

    const double baseX = player.getPosX();
    const double baseY = player.getPosY();
    const double baseZ = player.getPosZ();
    double mx = player.getMotionX();
    double my = player.getMotionY();
    double mz = player.getMotionZ();
    if (!finite3(baseX, baseY, baseZ) || !finite3(mx, my, mz))
        return;

    std::vector<PathPoint> path;
    path.reserve(41 * 4);
    path.push_back(PathPoint{});

    double relX = 0.0, relY = 0.0, relZ = 0.0;
    for (int step = 0; step < 40; ++step) {
        constexpr int kSubSteps = 4;
        for (int sub = 0; sub < kSubSteps; ++sub) {
            relX += mx / kSubSteps;
            relY += my / kSubSteps;
            relZ += mz / kSubSteps;
            path.push_back(PathPoint{relX, relY, relZ, false});
        }
        mx *= 0.91;
        mz *= 0.91;
        my = (my - 0.08) * 0.98;

        const int checkY = static_cast<int>(std::floor(baseY + relY));
        if (checkY < 0)
            break;
        if (mc.getBlockId(static_cast<int>(std::floor(baseX + relX)), checkY,
                          static_cast<int>(std::floor(baseZ + relZ))) != 0) {
            path.back().hitBlock = true;
            break;
        }
    }

    std::lock_guard<std::mutex> lock(pathMutex_);
    cachedPath_ = std::move(path);
}

void Clutch::onRender() {
    if (!enabled || !RenderData::updated || Menu::isOpen)
        return;
    if (GetTickCount64() - lastUpdateTick_ > 300)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<PathPoint> pathPoints;
    double interpX, interpY, interpZ;
    double motionX, motionY, motionZ;
    double renderX, renderY, renderZ;
    bool playerValid;
    Placement placement;
    {
        std::lock_guard<std::mutex> lock(pathMutex_);
        pathPoints = cachedPath_;
        interpX = cachedLocalLastX_ + (cachedLocalX_ - cachedLocalLastX_) * cachedPartialTicks_;
        interpY = cachedLocalLastY_ + (cachedLocalY_ - cachedLocalLastY_) * cachedPartialTicks_;
        interpZ = cachedLocalLastZ_ + (cachedLocalZ_ - cachedLocalLastZ_) * cachedPartialTicks_;
        renderX = cachedRenderX_;
        renderY = cachedRenderY_;
        renderZ = cachedRenderZ_;
        motionX = cachedMotionX_;
        motionY = cachedMotionY_;
        motionZ = cachedMotionZ_;
        playerValid = cachedPlayerValid_;
        placement = cachedPlacement_;
    }

    if (settings.showBps && playerValid) {
        const double horizontalSpeed = std::sqrt(motionX * motionX + motionZ * motionZ) * 20.0;
        const double verticalSpeed = motionY * 20.0;

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Speed: %.2f BPS  (Y: %.2f BPS)", horizontalSpeed,
                 verticalSpeed);

        const ImVec2 textSize = ImGui::CalcTextSize(buffer);
        const float x = (static_cast<float>(RenderData::screenWidth) - textSize.x) / 2.0f;
        const float y = static_cast<float>(RenderData::screenHeight) / 2.0f + 60.0f;

        draw->AddRectFilled({x - 12.0f, y - 6.0f}, {x + textSize.x + 12.0f, y + textSize.y + 6.0f},
                            IM_COL32(20, 20, 20, 160), 6.0f);
        draw->AddRect({x - 12.0f, y - 6.0f}, {x + textSize.x + 12.0f, y + textSize.y + 6.0f},
                      IM_COL32(255, 255, 255, 30), 6.0f, ImDrawFlags_None, 1.0f);

        ImU32 textColor = IM_COL32(255, 255, 255, 240);
        if (verticalSpeed < -5.0)
            textColor = IM_COL32(240, 70, 70, 240);
        else if (horizontalSpeed > 8.0)
            textColor = IM_COL32(70, 240, 200, 240);
        draw->AddText({x, y}, textColor, buffer);
    }

    if (settings.showPath && pathPoints.size() >= 2) {
        std::vector<ImVec2> screenPoints;
        screenPoints.reserve(pathPoints.size());
        for (const auto& point : pathPoints) {
            const Vector3 cameraRelPos{static_cast<float>(interpX + point.relX - renderX),
                                       static_cast<float>(interpY + point.relY - renderY),
                                       static_cast<float>(interpZ + point.relZ - renderZ)};
            Vector2 screenPos;
            if (CWorldToScreen::WorldToScreen(cameraRelPos, RenderData::modelView,
                                              RenderData::projection, RenderData::screenWidth,
                                              RenderData::screenHeight, screenPos)) {
                screenPoints.push_back({screenPos.x, screenPos.y});
            }
        }
        if (screenPoints.size() >= 2) {
            draw->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()),
                              IM_COL32(0, 0, 0, 180), ImDrawFlags_None, 3.5f);
            draw->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()),
                              IM_COL32(0, 191, 255, 220), ImDrawFlags_None, 1.5f);
        }
    }

    if (!settings.showPlacement || !placement.valid)
        return;

    const float minX = static_cast<float>(placement.targetX - renderX);
    const float minY = static_cast<float>(placement.targetY - renderY);
    const float minZ = static_cast<float>(placement.targetZ - renderZ);
    const Vector3 corners[8] = {{minX, minY, minZ},               {minX + 1.0f, minY, minZ},
                                {minX + 1.0f, minY, minZ + 1.0f}, {minX, minY, minZ + 1.0f},
                                {minX, minY + 1.0f, minZ},        {minX + 1.0f, minY + 1.0f, minZ},
                                {minX + 1.0f, minY + 1.0f, minZ + 1.0f},
                                {minX, minY + 1.0f, minZ + 1.0f}};

    Vector2 screenCorners[8];
    bool visible[8];
    int visibleCount = 0;
    for (int i = 0; i < 8; ++i) {
        visible[i] = CWorldToScreen::WorldToScreen(corners[i], RenderData::modelView,
                                                   RenderData::projection,
                                                   RenderData::screenWidth,
                                                   RenderData::screenHeight, screenCorners[i]);
        if (visible[i])
            ++visibleCount;
    }
    if (visibleCount < 3)
        return;

    const ImU32 faceColor = IM_COL32(0, 191, 255, 35);
    const ImU32 outlineColor = IM_COL32(0, 191, 255, 220);

    static constexpr int kFaces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {2, 3, 7, 6},
                                         {0, 1, 5, 4}, {0, 3, 7, 4}, {1, 2, 6, 5}};
    for (const auto& face : kFaces) {
        if (!visible[face[0]] || !visible[face[1]] || !visible[face[2]] || !visible[face[3]])
            continue;
        const ImVec2 pts[4] = {{screenCorners[face[0]].x, screenCorners[face[0]].y},
                               {screenCorners[face[1]].x, screenCorners[face[1]].y},
                               {screenCorners[face[2]].x, screenCorners[face[2]].y},
                               {screenCorners[face[3]].x, screenCorners[face[3]].y}};
        draw->AddConvexPolyFilled(pts, 4, faceColor);
    }

    static constexpr int kEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                          {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : kEdges) {
        if (visible[edge[0]] && visible[edge[1]]) {
            draw->AddLine({screenCorners[edge[0]].x, screenCorners[edge[0]].y},
                          {screenCorners[edge[1]].x, screenCorners[edge[1]].y}, outlineColor,
                          1.5f);
        }
    }
}

std::string Clutch::getDisplayInfo() {
    if (const char* status = status_.load(std::memory_order_acquire))
        return status;
    return "Armed";
}

void Clutch::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        isClutch() ? "Clutch" : "Scaffold",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled)
                    setEnabled(enabledValue);
            }
            ImGui::Spacing();

            if (isScaffold()) {
                ui::SliderFloat("Bridge Reach", &settings.scaffoldLead, 0.4f, 1.6f, "%.1f blocks");
            } else {
                ui::SliderInt("Prediction Time", &settings.predictionMs, 100, 800, "%d ms");
                ui::SliderInt("Fall Depth", &settings.safeDepth, 1, 24, "%d blocks");
                ui::Checkbox("Allow Forward", &settings.allowForward);
            }
            ui::SliderInt("Search Radius", &settings.searchRadius, 1, 6, "%d blocks");
            ui::SliderFloat("Reach", &settings.reach, 3.0f, 4.5f, "%.1f blocks");
            ui::Checkbox("Camera-Weighted Search", &settings.cameraWeight);
            ui::Checkbox("Switch Back", &settings.restoreSlot);
        },
        "Humanization & HUD",
        [this]() {
            ui::SliderInt("React Min", &settings.reactMinMs, 0, 500, "%d ms");
            ui::SliderInt("React Max", &settings.reactMaxMs, 0, 500, "%d ms");
            if (settings.reactMaxMs < settings.reactMinMs)
                settings.reactMaxMs = settings.reactMinMs;
            ui::SliderInt("Flick Ticks", &settings.aimTicks, 1, 6, "%d ticks");
            ui::SliderFloat("Aim Noise", &settings.aimNoise, 0.0f, 3.0f, "%.2f deg");
            ui::Checkbox("Overshoot", &settings.overshoot);
            ui::Checkbox("Snap Back", &settings.snapBack);
            ui::SliderInt("Miss Chance", &settings.missChance, 0, 90, "%d%%");
            ImGui::Spacing();
            ui::Checkbox("Show Speed HUD", &settings.showBps);
            ui::Checkbox("Show Visual Path", &settings.showPath);
            ui::Checkbox("Show Block Preview", &settings.showPlacement);
        });
}

void Clutch::saveSettings(json::Value& node) const {
    node["scaffoldLead"] = settings.scaffoldLead;
    node["predictionMs"] = settings.predictionMs;
    node["searchRadius"] = settings.searchRadius;
    node["safeDepth"] = settings.safeDepth;
    node["allowForward"] = settings.allowForward;
    node["reach"] = settings.reach;
    node["reactMinMs"] = settings.reactMinMs;
    node["reactMaxMs"] = settings.reactMaxMs;
    node["aimTicks"] = settings.aimTicks;
    node["aimNoise"] = settings.aimNoise;
    node["overshoot"] = settings.overshoot;
    node["snapBack"] = settings.snapBack;
    node["restoreSlot"] = settings.restoreSlot;
    node["missChance"] = settings.missChance;
    node["cameraWeight"] = settings.cameraWeight;
    node["showBps"] = settings.showBps;
    node["showPath"] = settings.showPath;
    node["showPlacement"] = settings.showPlacement;
}

void Clutch::loadSettings(const json::Value& node) {
    settings.scaffoldLead =
        (std::clamp)(node["scaffoldLead"].as_float(settings.scaffoldLead), 0.4f, 1.6f);
    settings.predictionMs =
        (std::clamp)(node["predictionMs"].as_int(settings.predictionMs), 100, 800);
    settings.searchRadius = (std::clamp)(node["searchRadius"].as_int(settings.searchRadius), 1, 6);
    settings.safeDepth = (std::clamp)(node["safeDepth"].as_int(settings.safeDepth), 1, 24);
    settings.allowForward = node["allowForward"].as_bool(settings.allowForward);
    settings.reach = (std::clamp)(node["reach"].as_float(settings.reach), 3.0f, 4.5f);
    settings.reactMinMs = (std::clamp)(node["reactMinMs"].as_int(settings.reactMinMs), 0, 500);
    settings.reactMaxMs = (std::clamp)(node["reactMaxMs"].as_int(settings.reactMaxMs),
                                       settings.reactMinMs, 500);
    settings.aimTicks = (std::clamp)(node["aimTicks"].as_int(settings.aimTicks), 1, 6);
    settings.aimNoise = (std::clamp)(node["aimNoise"].as_float(settings.aimNoise), 0.0f, 3.0f);
    settings.overshoot = node["overshoot"].as_bool(settings.overshoot);
    settings.snapBack = node["snapBack"].as_bool(settings.snapBack);
    settings.restoreSlot = node["restoreSlot"].as_bool(settings.restoreSlot);
    settings.missChance = (std::clamp)(node["missChance"].as_int(settings.missChance), 0, 90);
    settings.cameraWeight = node["cameraWeight"].as_bool(settings.cameraWeight);
    settings.showBps = node["showBps"].as_bool(settings.showBps);
    settings.showPath = node["showPath"].as_bool(settings.showPath);
    settings.showPlacement = node["showPlacement"].as_bool(settings.showPlacement);
}
