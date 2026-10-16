#include "aimassist.h"
#include "antibot.h"
#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"
#include "../../util/inject_trace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

AimAssist* aimAssist = nullptr;




static constexpr unsigned long long kRotClaimTailMs = 250ull;

void AimAssist::markRotationWrite() {
    lastRotWriteMs_.store(GetTickCount64(), std::memory_order_release);
}

bool AimAssist::isRewritingRotation() const {
    if (!enabled)
        return false;
    const unsigned long long last = lastRotWriteMs_.load(std::memory_order_acquire);
    if (last == 0ull)
        return false;
    const unsigned long long now = GetTickCount64();
    return now >= last && (now - last) <= kRotClaimTailMs;
}





static jclass g_aimC03 = nullptr;
static jfieldID g_aim_fid_yaw = nullptr;
static jfieldID g_aim_fid_pitch = nullptr;
static jfieldID g_aim_fid_rotating = nullptr;
static bool g_aimResolvedC03 = false;

static void resolveAimC03(JNIEnv* env) {
    if (g_aimResolvedC03)
        return;
    g_aimResolvedC03 = true;

    jclass localClass = nullptr;
    if (Java::AssignClass("net.minecraft.network.play.client.C03PacketPlayer", localClass)) {
        g_aimC03 = (jclass)env->NewGlobalRef(localClass);

        g_aim_fid_yaw = env->GetFieldID(g_aimC03, "yaw", "F");
        if (!g_aim_fid_yaw)
            g_aim_fid_yaw = env->GetFieldID(g_aimC03, "field_149476_e", "F");

        g_aim_fid_pitch = env->GetFieldID(g_aimC03, "pitch", "F");
        if (!g_aim_fid_pitch)
            g_aim_fid_pitch = env->GetFieldID(g_aimC03, "field_149473_f", "F");

        g_aim_fid_rotating = env->GetFieldID(g_aimC03, "rotating", "Z");
        if (!g_aim_fid_rotating)
            g_aim_fid_rotating = env->GetFieldID(g_aimC03, "field_149481_i", "Z");

        if (env->ExceptionCheck())
            env->ExceptionClear();
    }
}


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


struct NeuromotorState {
    float driftYaw = 0.0f;  
    float driftPitch = 0.0f;
    float reactionJitter = 0.0f;   
    float overshootFactor = 1.0f;  
    int ticksUntilCorrection = 0;  
    Timer stateRefreshTimer;

    void update(std::mt19937& rng, float speedSetting = 50.0f) {
        auto randF = [&](float min, float max) {
            std::uniform_real_distribution<float> d(min, max);
            return d(rng);
        };
        auto randI = [&](int min, int max) {
            std::uniform_int_distribution<int> d(min, max);
            return d(rng);
        };

        
        
        
        
        float speedNorm = speedSetting / 100.0f;
        float bellCurve = std::exp(-std::pow((speedNorm - 0.5f) * 2.5f, 2.0f));
        float speedScale = 0.15f + bellCurve * 0.45f;  

        
        float yawDriftStep = randF(-0.012f, 0.012f) * speedScale;
        float pitchDriftStep = randF(-0.008f, 0.008f) * speedScale;

        driftYaw += yawDriftStep;
        driftPitch += pitchDriftStep;

        
        float decayRate = std::lerp(0.85f, 0.94f, speedScale);
        driftYaw *= decayRate;
        driftPitch *= decayRate;

        
        float maxDrift = 0.06f * speedScale;
        driftYaw = std::clamp(driftYaw, -maxDrift, maxDrift);
        driftPitch = std::clamp(driftPitch, -maxDrift * 0.8f, maxDrift * 0.8f);

        
        if (stateRefreshTimer.Elapsed<>() > randF(800.f, 2000.f)) {
            
            float overshootRange = std::lerp(0.02f, 0.08f, speedScale);
            overshootFactor = 1.0f + randF(-overshootRange, overshootRange);
            reactionJitter = randF(-15.f, 15.f) * speedScale;
            ticksUntilCorrection = randI(3, 12);
            stateRefreshTimer.Start();
        }
    }
};

static NeuromotorState g_neuroState;


static float calculateGCD(float sensitivity) {
    float f = sensitivity * 0.6f + 0.2f;
    return f * f * f * 1.2f;
}


static float applyGCDStrict(float delta, float gcd) {
    if (gcd < 0.0001f)
        return delta;
    return std::round(delta / gcd) * gcd;
}






static float limitPitchOnGrid(float pitch, float gcd) {
    constexpr float kPitchLimit = 89.9f;
    if (std::abs(pitch) <= kPitchLimit)
        return pitch;
    if (gcd < 0.0001f)
        return pitch > 0.0f ? kPitchLimit : -kPitchLimit;
    const float steps = std::ceil((std::abs(pitch) - kPitchLimit) / gcd);
    return pitch > 0.0f ? pitch - steps * gcd : pitch + steps * gcd;
}















static void varyStepCounts(float& yawDelta, float& pitchDelta, float gcd, std::mt19937& rng) {
    if (gcd < 0.0001f)
        return;

    const long yawSteps = std::lround(yawDelta / gcd);
    const long pitchSteps = std::lround(pitchDelta / gcd);
    if (yawSteps == 0 && pitchSteps == 0)
        return;

    std::uniform_int_distribution<int> roll(0, 99);
    std::uniform_int_distribution<int> pick(0, 1);

    
    
    
    const bool yawRoomy = std::labs(yawSteps) >= 2;
    const bool pitchRoomy = std::labs(pitchSteps) >= 2;
    if (!yawRoomy && !pitchRoomy)
        return;
    if (roll(rng) >= 14)
        return;

    const bool touchYaw = yawRoomy && (!pitchRoomy || pick(rng) == 0);
    float& target = touchYaw ? yawDelta : pitchDelta;
    const long steps = touchYaw ? yawSteps : pitchSteps;
    const long adjusted = steps + (pick(rng) == 0 ? 1 : -1);
    target = static_cast<float>(adjusted) * gcd;
}

static float g_cachedGCD = 0.0f;
static Timer g_gcdRefreshTimer;


AimBoost::AimBoost(float speed_mult_min, float speed_mult_max, float freq_min_ms, float freq_max_ms, bool continuous)
    : speed_mult_min(speed_mult_min),
      speed_mult_max(speed_mult_max),
      frequency_min_ms((uint32_t)freq_min_ms),
      frequency_max_ms((uint32_t)freq_max_ms),
      continuous(continuous) {
    speed_mult = (speed_mult_min + speed_mult_max) / 2;
    prev_speed_mult = speed_mult;
}

float AimBoost::Delay() {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    auto randFloat = [&](float min, float max) -> float {
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    };

    auto randInt = [&](int min, int max) -> int {
        std::uniform_int_distribution<int> dis(min, max);
        return dis(gen);
    };

    auto slerp = [](float start, float end, float t) -> float {
        t = std::clamp(t, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);
        return start + (end - start) * t;
    };

    if (timer.Elapsed<>() >= frequency_ms) {
        if (continuous) {
            prev_speed_mult = randFloat(speed_mult_min, speed_mult_max);
        } else {
            prev_speed_mult = speed_mult;
            speed_mult = randFloat(speed_mult_min, speed_mult_max);
        }

        frequency_ms = randInt(frequency_min_ms, frequency_max_ms);
        timer.Start();
    }

    return slerp(prev_speed_mult, continuous ? speed_mult : 1.f, timer.Elapsed<>() / frequency_ms);
}


AimAssist::AimAssist() : Module("AimAssist") {
    keybind = 0;
    std::random_device rd;
    rng.seed(rd());
    entityLockEnabled_.store(settings.entityLock, std::memory_order_relaxed);
}

void AimAssist::releaseCachedTarget(JNIEnv* env) {
    std::lock_guard<std::recursive_mutex> lock(targetMutex_);
    if (!cachedTarget || !env)
        return;

    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->DeleteGlobalRef(cachedTarget);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    cachedTarget = nullptr;
}

void AimAssist::resetTargetState(JNIEnv* env) {
    std::lock_guard<std::recursive_mutex> lock(targetMutex_);
    releaseCachedTarget(env);
    hasLockedTarget = false;
    targetHasHitbox = false;
    prevFrameHadTarget = false;
    targetChangedThisFrame = false;
    reactionPrimed = false;
    controllerTimer.Start();
    yawdiffSpeed = 0.0f;
    pitchdiffSpeed = 0.0f;
    simulatedJitterYaw = simulatedJitterPitch = 0.0f;
    simulatedJitterTargetYaw = simulatedJitterTargetPitch = 0.0f;
    appliedJitterYaw_ = appliedJitterPitch_ = 0.0f;
    yawRotationResidual = pitchRotationResidual = 0.0;
    cursorAccumY = 0.0;
    
    predVelX = predVelY = predVelZ = 0.0;
    predTimer.Start();
}

void AimAssist::setEntityLock(bool enabled) {
    settings.entityLock = enabled;
    entityLockEnabled_.store(enabled, std::memory_order_release);
    targetResetRequested_.store(true, std::memory_order_release);
}

void AimAssist::onWorldChange() {
    resetTargetState(Java::Env());
    targetResetRequested_.store(true, std::memory_order_release);
}

void AimAssist::onEnable() {
    log_ok("AimAssist enabled");

    std::lock_guard<std::recursive_mutex> lock(targetMutex_);
    JNIEnv* env = Java::Env();
    releaseCachedTarget(env);
    entityLockEnabled_.store(settings.entityLock, std::memory_order_release);
    targetResetRequested_.store(false, std::memory_order_release);
    hasLockedTarget = false;
    pitchdiffSpeed = 0.f;
    yawdiffSpeed = 0.f;
    cursorAccumY = 0.0;
    yawRotationResidual = pitchRotationResidual = 0.0;
    targetYawSpeed = 0.f;
    targetPitchSpeed = 0.f;
    reactionPrimed = false;
    controllerTimer.Start();
    targetHasHitbox = false;
    targetHitboxMinX = targetHitboxMinY = targetHitboxMinZ = 0.0f;
    targetHitboxMaxX = targetHitboxMaxY = targetHitboxMaxZ = 0.0f;
    prevFrameHadTarget = false;
    targetRampTimer.Start();
    clickRampTimer.Start();
    hitboxRefreshTimer.Start();
    lastMouseDown = false;
    clickStartedThisFrame = false;
    targetSpeedVariance = 1.0f;
    aimHumanizer = 1.0f;
    simulatedJitterYaw = simulatedJitterPitch = 0.0f;
    simulatedJitterTargetYaw = simulatedJitterTargetPitch = 0.0f;
    appliedJitterYaw_ = appliedJitterPitch_ = 0.0f;
    simulatedJitterTimer.Start();
}

void AimAssist::onDisable() {
    log_ok("AimAssist disabled");
    std::lock_guard<std::recursive_mutex> lock(targetMutex_);
    hasLockedTarget = false;

    releaseCachedTarget(Java::Env());
    targetResetRequested_.store(false, std::memory_order_release);
    cursorAccumY = 0.0;
    yawRotationResidual = pitchRotationResidual = 0.0;
    pitchdiffSpeed = yawdiffSpeed = 0.f;
    targetYawSpeed = 0.f;
    targetPitchSpeed = 0.f;
    reactionPrimed = false;
    targetHasHitbox = false;
    targetHitboxMinX = targetHitboxMinY = targetHitboxMinZ = 0.0f;
    targetHitboxMaxX = targetHitboxMaxY = targetHitboxMaxZ = 0.0f;
    reactionTimeTimer.Start();
    lastMouseDown = false;
    clickStartedThisFrame = false;
    simulatedJitterYaw = simulatedJitterPitch = 0.0f;
    simulatedJitterTargetYaw = simulatedJitterTargetPitch = 0.0f;
    appliedJitterYaw_ = appliedJitterPitch_ = 0.0f;
}

float AimAssist::wrapTo180(float value) {
    float res = std::fmod(value + 180.f, 360.f);
    if (res < 0)
        res += 360.f;
    return res - 180.f;
}

std::pair<float, float> AimAssist::getAngles(double x1, double y1, double z1, double x2, double y2, double z2) {
    float d_x = (float)(x2 - x1);
    float d_y = (float)(y2 - y1);
    float d_z = (float)(z2 - z1);

    constexpr float PI = 3.14159265358979323846f;
    float hypothenuse = std::sqrt(d_x * d_x + d_z * d_z);
    float yaw = std::atan2(d_z, d_x) * 180.f / PI - 90.f;
    float pitch = -std::atan2(d_y, hypothenuse) * 180.f / PI;

    return std::make_pair(yaw, pitch);
}

float AimAssist::randFloat(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(rng);
}

int AimAssist::randInt(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(rng);
}

float AimAssist::slerp(float start, float end, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    return start + (end - start) * t;
}

void AimAssist::onUpdate() {
    std::lock_guard<std::recursive_mutex> lock(targetMutex_);
    clickStartedThisFrame = false;
    
    
    hasServerRotation_.store(false, std::memory_order_relaxed);
    JNIEnv* env = Java::Env();
    if (!env)
        return;

    if (targetResetRequested_.exchange(false, std::memory_order_acq_rel))
        resetTargetState(env);

    const bool entityLock = entityLockEnabled_.load(std::memory_order_acquire);
    if (!player.isValid()) {
        markAimIdle();
        return;
    }
    if (Menu::isOpen) {
        markAimIdle();
        return;
    }
    if (mc.isInGui()) {
        markAimIdle();
        return;
    }

    
    if (settings.armorFilterKeybind != 0) {
        bool keyDown = (GetAsyncKeyState(settings.armorFilterKeybind) & 0x8000) != 0;
        if (keyDown && !armorFilterKeyPrev_) {
            settings.armorFilterEnabled = !settings.armorFilterEnabled;
            NotificationManager::add("Armor Filter", settings.armorFilterEnabled);
        }
        armorFilterKeyPrev_ = keyDown;
    }

    bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    if (!settings.alwaysAim && !mouseDown) {
        
        
        if (!entityLock)
            resetTargetState(env);
        lastMouseDown = false;
        markAimIdle();
        return;
    }

    if (mouseDown && !lastMouseDown) {
        clickStartedThisFrame = true;
        clickRampTimer.Start();
        targetRampTimer.Start();
        yawdiffSpeed = 0.f;
        pitchdiffSpeed = 0.f;
        reactionPrimed = false;
    }

    if (settings.breakBlocks && mouseDown) {
        if (mc.isTargetingBlock()) {
            markAimIdle();
            return;
        }
    }

    
    if (settings.weaponOnly && !isHoldingWeapon()) {
        markAimIdle();
        return;
    }

    float localYaw = player.getYaw();
    float localPitch = player.getPitch();
    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();
    
    
    double eyeY = localY + 1.62;
    targetChangedThisFrame = false;

    double targetX = 0.0;
    double targetY = 0.0;
    double targetZ = 0.0;

    hasLockedTarget = findTarget(targetX, targetY, targetZ);

    if (!hasLockedTarget) {
        prevFrameHadTarget = false;
        
        
        
        
        yawdiffSpeed = pitchdiffSpeed = 0.0f;
        targetYawSpeed = targetPitchSpeed = 0.0f;
        yawRotationResidual = pitchRotationResidual = 0.0;
        cursorAccumY = 0.0;
        reactionPrimed = false;
        
        
        predVelX = predVelY = predVelZ = 0.0;
        predTimer.Start();
        controllerTimer.Start();
        lastMouseDown = mouseDown;
        return;
    }

    lockedTargetX = targetX;
    lockedTargetY = targetY;
    lockedTargetZ = targetZ;

    
    
    
    
    
    
    
    
    {
        
        
        if (targetChangedThisFrame) {
            predVelX = predVelY = predVelZ = 0.0;
            predPrevX = targetX;
            predPrevY = targetY;
            predPrevZ = targetZ;
            predTimer.Start();
        }

        const double jdx = targetX - predPrevX;
        const double jdy = targetY - predPrevY;
        const double jdz = targetZ - predPrevZ;
        const double moved2 = jdx * jdx + jdy * jdy + jdz * jdz;
        const float dtp = predTimer.Elapsed<>() / 1000.0f;

        if (moved2 > 1e-8 && dtp >= 0.008f) {
            if (dtp > 0.5f || moved2 > 9.0) {
                
                predVelX = predVelY = predVelZ = 0.0;
            } else {
                const double sm = 0.35;
                predVelX += ((jdx / dtp) - predVelX) * sm;
                predVelY += ((jdy / dtp) - predVelY) * sm;
                predVelZ += ((jdz / dtp) - predVelZ) * sm;
            }
            predPrevX = targetX;
            predPrevY = targetY;
            predPrevZ = targetZ;
            predTimer.Start();
        } else if (dtp > 0.3f) {
            
            predVelX = predVelY = predVelZ = 0.0;
            predPrevX = targetX;
            predPrevY = targetY;
            predPrevZ = targetZ;
            predTimer.Start();
        }

        
        
        predVelX = std::clamp(predVelX, -12.0, 12.0);
        predVelY = std::clamp(predVelY, -12.0, 12.0);
        predVelZ = std::clamp(predVelZ, -12.0, 12.0);
    }

    
    
    const bool adaptiveLock = (settings.aimMode == AimMode::ADAPTIVE) && entityLock;

    
    
    
    
    
    
    
    double aimX, aimY, aimZ;
    if (!getAimPoint(targetX, targetY, targetZ, aimX, aimY, aimZ)) {
        markAimIdle();
        return;
    }

    
    
    
    
    
    
    
    
    
    
    
    if (settings.prediction > 0.0f && resolvedBox_.valid) {
        const double leadSec = (settings.prediction / 100.0) * 0.16;
        
        
        
        constexpr double kFaceInset = 0.09;
        
        
        const auto lead = [&](double vel, double point, double lo, double hi) {
            const double back = std::max(0.0, (point - lo) - kFaceInset);
            const double fwd = std::max(0.0, (hi - point) - kFaceInset);
            return std::clamp(vel * leadSec, -back, fwd);
        };
        aimX += lead(predVelX, aimX, resolvedBox_.minX, resolvedBox_.maxX);
        aimZ += lead(predVelZ, aimZ, resolvedBox_.minZ, resolvedBox_.maxZ);
        if (!settings.horizontalOnly && !adaptiveLock)
            aimY += lead(predVelY, aimY, resolvedBox_.minY, resolvedBox_.maxY);
    }

    
    
    
    
    
    
    {
        float yawBudget = 0.0f, pitchBudget = 0.0f;
        if (resolvedBox_.valid) {
            const double dx = aimX - localX;
            const double dz = aimZ - localZ;
            const double flat = std::sqrt(dx * dx + dz * dz);
            const double dy = aimY - eyeY;
            const double dist = std::sqrt(flat * flat + dy * dy);
            const double halfW =
                std::min(resolvedBox_.maxX - resolvedBox_.minX, resolvedBox_.maxZ - resolvedBox_.minZ) * 0.5;
            const double halfH = (resolvedBox_.maxY - resolvedBox_.minY) * 0.5;
            if (flat > 0.05 && halfW > 0.0)
                yawBudget = (float)(std::atan2(halfW - 0.09, flat) * 57.29577951308232);
            if (dist > 0.05 && halfH > 0.0)
                pitchBudget = (float)(std::atan2(halfH - 0.09, dist) * 57.29577951308232);
            
            
            yawBudget = std::clamp(yawBudget * 0.5f, 0.0f, 3.0f);
            pitchBudget = std::clamp(pitchBudget * 0.5f, 0.0f, 3.0f);
        }
        packetYawBudget_.store(yawBudget, std::memory_order_release);
        packetPitchBudget_.store(pitchBudget, std::memory_order_release);
    }

    auto [yawTarget, pitchTarget] = getAngles(localX, eyeY, localZ, aimX, aimY, aimZ);

    float yawDiff = wrapTo180(-(localYaw - yawTarget));
    float pitchDiff = wrapTo180(-(localPitch - pitchTarget));

    
    
    
    if (settings.stopOnHitbox && targetHasHitbox && !adaptiveLock) {
        float hitboxWidth = targetHitboxMaxX - targetHitboxMinX;
        float hitboxHeight = targetHitboxMaxY - targetHitboxMinY;

        if (hitboxWidth > 0.01f && hitboxHeight > 0.01f) {
            
            float yawToMinX = getAngles(localX, eyeY, localZ, targetHitboxMinX, aimY, targetHitboxMinZ).first;
            float yawToMaxX = getAngles(localX, eyeY, localZ, targetHitboxMaxX, aimY, targetHitboxMaxZ).first;
            float pitchToMinY = getAngles(localX, eyeY, localZ, aimX, targetHitboxMinY, aimZ).second;
            float pitchToMaxY = getAngles(localX, eyeY, localZ, aimX, targetHitboxMaxY, aimZ).second;

            float yawDiffToMinX = wrapTo180(-(localYaw - yawToMinX));
            float yawDiffToMaxX = wrapTo180(-(localYaw - yawToMaxX));
            float pitchDiffToMinY = wrapTo180(-(localPitch - pitchToMinY));
            float pitchDiffToMaxY = wrapTo180(-(localPitch - pitchToMaxY));

            
            bool insideYaw = (yawDiffToMinX * yawDiffToMaxX) <= 0.0f;
            bool insidePitch = (pitchDiffToMinY * pitchDiffToMaxY) <= 0.0f;

            bool insideHitbox = settings.horizontalOnly ? insideYaw : (insideYaw && insidePitch);

            if (insideHitbox) {
                prevFrameHadTarget = hasLockedTarget;
                lastMouseDown = mouseDown;
                markAimIdle();
                return;
            }
        }
    }

    
    if (settings.targetMode == TargetMode::SWITCH && settings.switchSortBy == SortBy::HURT_TIME &&
        settings.hurtTimeLock && targetChangedThisFrame) {
        
        
        
        
        
        const float gcdSnap = g_cachedGCD > 0.0001f ? g_cachedGCD
                                                    : calculateGCD(mc.getMouseSensitivity());
        g_cachedGCD = gcdSnap;
        const float yawNow = player.getYaw();
        const float snappedYaw =
            yawNow + applyGCDStrict(wrapTo180(yawTarget - yawNow), gcdSnap);
        player.setYaw(snappedYaw);
        player.setPrevYaw(snappedYaw);
        if (!settings.horizontalOnly) {
            const float pitchNow = player.getPitch();
            const float stepped = pitchNow + applyGCDStrict(pitchTarget - pitchNow, gcdSnap);
            const float snappedPitch = limitPitchOnGrid(stepped, gcdSnap);
            player.setPitch(snappedPitch);
            player.setPrevPitch(snappedPitch);
        }
        markRotationWrite();
        prevFrameHadTarget = hasLockedTarget;
        lastMouseDown = mouseDown;
        
        
        appliedJitterYaw_ = appliedJitterPitch_ = 0.0f;
        markAimIdle();
        return;
    }

    cachedMouseSensitivity = mc.getMouseSensitivity();

    
    
    applyAimController(yawDiff, pitchDiff);
    prevFrameHadTarget = hasLockedTarget;
    lastMouseDown = mouseDown;
}

bool AimAssist::findTarget(double& targetX, double& targetY, double& targetZ) {
    
    
    
    static Timer scanTimer;
    JNIEnv* env = Java::Env();
    if (!env)
        return false;
    const bool entityLock = entityLockEnabled_.load(std::memory_order_acquire);

    
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
        if (cachedTarget) {
            env->DeleteGlobalRef(cachedTarget);
            cachedTarget = nullptr;
        }
        targetHasHitbox = false;
        log_warn("[AimAssist] Cleared JNI exception - cache reset");
    }

    bool wantHitboxRefresh = entityLock && cachedTarget && hitboxRefreshTimer.Elapsed<>() >= hitboxRefreshMs;
    bool forceScan = false;

    
    if (cachedTarget && scanTimer.Elapsed<>() < 50 && !wantHitboxRefresh)  
    {
        
        if (Java::Env()->IsSameObject(cachedTarget, NULL)) {
            Java::Env()->DeleteGlobalRef(cachedTarget);  
            cachedTarget = nullptr;                      
            targetHasHitbox = false;
            prevFrameHadTarget = false;
            forceScan = true;
        } else {
            if (!JniCache::fid_posX || !JniCache::fid_posY || !JniCache::fid_posZ) {
                Java::Env()->ExceptionClear();
                Java::Env()->DeleteGlobalRef(cachedTarget);
                cachedTarget = nullptr;
                targetHasHitbox = false;
                forceScan = true;
            } else {
                
                double cX = Java::Env()->GetDoubleField(cachedTarget, JniCache::fid_posX);
                double cY = Java::Env()->GetDoubleField(cachedTarget, JniCache::fid_posY);
                double cZ = Java::Env()->GetDoubleField(cachedTarget, JniCache::fid_posZ);

                if (Java::Env()->ExceptionCheck()) {
                    Java::Env()->ExceptionClear();
                    Java::Env()->DeleteGlobalRef(cachedTarget);
                    cachedTarget = nullptr;
                    targetHasHitbox = false;
                    prevFrameHadTarget = false;
                    forceScan = true;
                } else {
                    
                    
                    
                    
                    if (JniCache::fid_boundingBox && JniCache::cls_AxisAlignedBB) {
                        jobject aabb = Java::Env()->GetObjectField(cachedTarget, JniCache::fid_boundingBox);
                        if (aabb) {
                            if (JniCache::fid_aabb_minX) targetHitboxMinX = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_minX);
                            if (JniCache::fid_aabb_minY) targetHitboxMinY = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_minY);
                            if (JniCache::fid_aabb_minZ) targetHitboxMinZ = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_minZ);
                            if (JniCache::fid_aabb_maxX) targetHitboxMaxX = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_maxX);
                            if (JniCache::fid_aabb_maxY) targetHitboxMaxY = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_maxY);
                            if (JniCache::fid_aabb_maxZ) targetHitboxMaxZ = (float)Java::Env()->GetDoubleField(aabb, JniCache::fid_aabb_maxZ);
                            Java::Env()->DeleteLocalRef(aabb);
                            const float w = targetHitboxMaxX - targetHitboxMinX;
                            const float h = targetHitboxMaxY - targetHitboxMinY;
                            targetHasHitbox = w > 0.01f && h > 0.01f;
                        }
                        if (Java::Env()->ExceptionCheck())
                            Java::Env()->ExceptionClear();
                    }
                    
                    double localX = player.getPosX();
                    double localY = player.getPosY();
                    double localZ = player.getPosZ();
                    double dx = cX - localX;
                    double dy = cY - localY;
                    double dz = cZ - localZ;
                    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    
                    
                    
                    const bool adaptiveHardLock = (settings.aimMode == AimMode::ADAPTIVE) && entityLock;
                    double maxDist = entityLock ? (settings.distance + 1.5f) : settings.distance;
                    if (adaptiveHardLock)
                        maxDist = std::max<double>(maxDist, settings.distance + 3.0);

                    
                    float localYaw = player.getYaw();
                    
                    
                    float yawToTarget = getAngles(localX, localY + 1.62, localZ,
                                                  cX, cY + 0.9, cZ).first;
                    float yawDiff = std::abs(wrapTo180(-(localYaw - yawToTarget)));
                    float limit = entityLock ? std::max(90.0f, settings.fov * 0.5f) : (settings.fov * 0.5f);
                    if (adaptiveHardLock)
                        limit = std::max(limit, 180.0f);
                    
                    
                    
                    if (dist < 3.0f)
                        limit = std::max(limit, 150.0f);

                    if (dist > maxDist || dist < 0.08f || yawDiff > limit) {
                        log_debug("[AimAssist] target dropped: dist=%.1f yaw=%.1f", dist, yawDiff);
                        Java::Env()->DeleteGlobalRef(cachedTarget);
                        cachedTarget = nullptr;
                        targetHasHitbox = false;
                        prevFrameHadTarget = false;
                        forceScan = true;  
                    } else {
                        targetX = cX;
                        targetY = cY;
                        targetZ = cZ;
                        return true;
                    }
                }
            }
        }
    }

    
    if (!forceScan && scanTimer.Elapsed<>() < 50 && !wantHitboxRefresh) {
        return false;  
    }

    
    scanTimer.Start();

    auto players = mc.getPlayerList();

    
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (friendsManager) {
        friendsManager->resolvePending(players);
    }

    if (players.empty()) {
        if (cachedTarget) {
            Java::Env()->DeleteGlobalRef(cachedTarget);
            cachedTarget = nullptr;
        }
        targetHasHitbox = false;
        return false;
    }

    float localYaw = player.getYaw();
    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();

    float lowestHealth = FLT_MAX;
    float minimalAngleDiff = (float)settings.fov / 2.f;
    float closestDistance = FLT_MAX;
    float bestSwitchScore = FLT_MAX;  
    bool found = false;

    jobject bestPlayerObjLocal = nullptr;
    PlayerInfo closestPlayer;
    bool refreshedHitbox = false;
    bool lockedEntityFound = false;
    auto releasePlayerRef = [env](PlayerInfo& candidate) {
        if (candidate.instance && env) {
            env->DeleteGlobalRef(candidate.instance);
            candidate.instance = nullptr;
        }
    };

    for (auto& p : players) {
        bool sameAsLocked = false;
        if (cachedTarget && env) {
            if (env->IsSameObject(cachedTarget, NULL)) {
                Java::Env()->DeleteGlobalRef(cachedTarget);
                cachedTarget = nullptr;
            } else {
                sameAsLocked = env->IsSameObject(cachedTarget, p.instance);
            }
        }

        
        if (sameAsLocked && entityLock) {
            targetHitboxMinX = p.hitboxMinX;
            targetHitboxMinY = p.hitboxMinY;
            targetHitboxMinZ = p.hitboxMinZ;
            targetHitboxMaxX = p.hitboxMaxX;
            targetHitboxMaxY = p.hitboxMaxY;
            targetHitboxMaxZ = p.hitboxMaxZ;
            float hitboxWidth = targetHitboxMaxX - targetHitboxMinX;
            float hitboxHeight = targetHitboxMaxY - targetHitboxMinY;
            targetHasHitbox = (hitboxWidth > 0.01f) && (hitboxHeight > 0.01f);
            refreshedHitbox = true;
        }

        double dx = p.posX - localX;
        double dy = p.posY - localY;
        double dz = p.posZ - localZ;
        float dist = (float)std::sqrt(dx * dx + dy * dy + dz * dz);

        float maxDist = (entityLock && sameAsLocked) ? (settings.distance + 1.5f) : settings.distance;
        if (dist > maxDist || dist < 0.08f) {
            releasePlayerRef(p);
            continue;
        }
        if (p.isInvisible && !settings.invisibles) {
            releasePlayerRef(p);
            continue;
        }
        if (AntiBot::Filtered(p)) {
            releasePlayerRef(p);
            continue;
        }
        if (p.health <= 0.0f) {
            releasePlayerRef(p);
            continue;
        }

        if (friendsManager) {
            if (p.isOnLocalTeam && friendsManager->honorsVanillaTeams()) {
                releasePlayerRef(p);
                continue;
            }

            bool isApolloTeam = !p.uuid.empty() && friendsManager->isLunarTeammate(p.uuid);
            if (isApolloTeam) {
                releasePlayerRef(p);
                continue;
            }

            bool isApolloNametag = !p.uuid.empty() && friendsManager->isApolloTeammate(p.uuid);
            if (isApolloNametag) {
                releasePlayerRef(p);
                continue;
            }

            
            bool isFriendByUUID = !p.uuid.empty() && friendsManager->isFriendByUUID(p.uuid);
            bool isFriendByName = friendsManager->isFriend(FriendsManager::stripColorCodes(p.name));
            if (isFriendByUUID || isFriendByName) {
                releasePlayerRef(p);
                continue;
            }

            
            
            static int localTeamColor = -1;
            static Timer colorRefreshTimer;
            if (colorRefreshTimer.Elapsed<>() > 1000) {  
                localTeamColor = mc.getLocalPlayerTeamColor();
                colorRefreshTimer.Start();
            }

            ChatColor playerColor = static_cast<ChatColor>(p.teamColor);
            ChatColor localColor = static_cast<ChatColor>(localTeamColor);
            static std::string localPlayerName;
            static Timer nameRefreshTimer;
            if (localPlayerName.empty() || nameRefreshTimer.Elapsed<>() > 1000) {
                localPlayerName = mc.getLocalPlayerName();
                nameRefreshTimer.Start();
            }
            bool isScoreboardTeammate =
                friendsManager->isScoreboardTeammate(localPlayerName,
                                                     p.profileName.empty() ? p.name : p.profileName) ||
                friendsManager->isTeammate(p.uuid, playerColor, localColor);
            if (isScoreboardTeammate) {
                releasePlayerRef(p);
                continue;
            }
        }

        
        
        
        if (settings.armorFilterEnabled && settings.armorTierMask != 0) {
            auto tierBit = [](int id) -> int {
                if (id >= 298 && id <= 301) return AimAssist::ARMOR_LEATHER;
                if (id >= 302 && id <= 305) return AimAssist::ARMOR_CHAIN;
                if (id >= 306 && id <= 309) return AimAssist::ARMOR_IRON;
                if (id >= 310 && id <= 313) return AimAssist::ARMOR_DIAMOND;
                if (id >= 314 && id <= 317) return AimAssist::ARMOR_GOLD;
                return 0;
            };
            bool match = false;
            bool hasAnyArmor = false;
            for (const auto& piece : p.armor) {
                if (piece.id > 0) {
                    hasAnyArmor = true;
                    int b = tierBit(piece.id);
                    if (b && (settings.armorTierMask & b)) { match = true; break; }
                }
            }
            if (!match && (settings.armorTierMask & AimAssist::ARMOR_NO_ARMOR) && !hasAnyArmor)
                match = true;
            if (!match) { releasePlayerRef(p); continue; }
        }
        

        float yawToTarget = getAngles(localX, localY + 1.62, localZ,
                                      p.posX, p.posY + 0.9, p.posZ).first;
        float yawDiff = std::abs(wrapTo180(-(localYaw - yawToTarget)));

        float maxYawDiff = (entityLock && sameAsLocked) ? std::max(90.0f, settings.fov * 0.5f) : (settings.fov * 0.5f);
        
        if (dist < 3.0f)
            maxYawDiff = std::max(maxYawDiff, 150.0f);
        if (yawDiff > maxYawDiff) {
            releasePlayerRef(p);
            continue;
        }

        
        if (entityLock && sameAsLocked) {
            if (bestPlayerObjLocal && env)
                env->DeleteLocalRef(bestPlayerObjLocal);
            bestPlayerObjLocal = env && p.instance ? env->NewLocalRef(p.instance) : nullptr;
            closestPlayer = p;
            found = true;
            lockedEntityFound = true;
        } else if (lockedEntityFound) {
            
        } else if (settings.targetMode == TargetMode::DISTANCE) {
            if (dist < closestDistance) {
                closestDistance = dist;
                closestPlayer = p;
                found = true;
                if (env) {
                    if (bestPlayerObjLocal)
                        env->DeleteLocalRef(bestPlayerObjLocal);
                    bestPlayerObjLocal = p.instance ? env->NewLocalRef(p.instance) : nullptr;
                }
            }
        } else if (settings.targetMode == TargetMode::HEALTH) {
            if (p.health < lowestHealth) {
                lowestHealth = p.health;
                closestPlayer = p;
                found = true;
                if (env) {
                    if (bestPlayerObjLocal)
                        env->DeleteLocalRef(bestPlayerObjLocal);
                    bestPlayerObjLocal = p.instance ? env->NewLocalRef(p.instance) : nullptr;
                }
            }
        } else if (settings.targetMode == TargetMode::SWITCH) {
            
            
            float score = FLT_MAX;
            float tieBreaker = dist * 0.001f;  

            switch (settings.switchSortBy) {
                case SortBy::DISTANCE:
                    score = dist;
                    break;
                case SortBy::HEALTH:
                    score = p.health + tieBreaker;
                    break;
                case SortBy::FOV:
                    score = yawDiff + tieBreaker;
                    break;
                case SortBy::HURT_TIME:
                    score = (float)p.hurtTime + tieBreaker;
                    break;
            }

            
            
            bool isCurrentTarget = cachedTarget && env && !env->IsSameObject(cachedTarget, NULL) &&
                                   env->IsSameObject(cachedTarget, p.instance);
            if (isCurrentTarget) {
                score -= 0.5f;  
            }

            if (score < bestSwitchScore) {
                bestSwitchScore = score;
                closestPlayer = p;
                found = true;
                if (env) {
                    if (bestPlayerObjLocal)
                        env->DeleteLocalRef(bestPlayerObjLocal);
                    bestPlayerObjLocal = p.instance ? env->NewLocalRef(p.instance) : nullptr;
                }
            }
        } else  
        {
            if (yawDiff < minimalAngleDiff) {
                minimalAngleDiff = yawDiff;
                closestPlayer = p;
                found = true;
                if (env) {
                    if (bestPlayerObjLocal)
                        env->DeleteLocalRef(bestPlayerObjLocal);
                    bestPlayerObjLocal = p.instance ? env->NewLocalRef(p.instance) : nullptr;
                }
            }
        }

        releasePlayerRef(p);
    }

    if (found) {
        targetX = closestPlayer.posX;
        targetY = closestPlayer.posY;
        targetZ = closestPlayer.posZ;
        targetHitboxMinX = closestPlayer.hitboxMinX;
        targetHitboxMinY = closestPlayer.hitboxMinY;
        targetHitboxMinZ = closestPlayer.hitboxMinZ;
        targetHitboxMaxX = closestPlayer.hitboxMaxX;
        targetHitboxMaxY = closestPlayer.hitboxMaxY;
        targetHitboxMaxZ = closestPlayer.hitboxMaxZ;

        float hitboxWidth = targetHitboxMaxX - targetHitboxMinX;
        float hitboxHeight = targetHitboxMaxY - targetHitboxMinY;
        targetHasHitbox = (hitboxWidth > 0.01f) && (hitboxHeight > 0.01f);
    } else {
        targetHasHitbox = false;
    }

    if (found && bestPlayerObjLocal) {
        if (cachedTarget) {
            
            if (!Java::Env()->IsSameObject(cachedTarget, bestPlayerObjLocal)) {
                Java::Env()->DeleteGlobalRef(cachedTarget);
                cachedTarget = Java::Env()->NewGlobalRef(bestPlayerObjLocal);
                targetChangedThisFrame = true;
            }
        } else {
            cachedTarget = Java::Env()->NewGlobalRef(bestPlayerObjLocal);
            targetChangedThisFrame = true;
        }
    } else {
        if (cachedTarget) {
            Java::Env()->DeleteGlobalRef(cachedTarget);
            cachedTarget = nullptr;
        }
    }

    if (bestPlayerObjLocal && env) {
        env->DeleteLocalRef(bestPlayerObjLocal);
    }

    if (refreshedHitbox) {
        hitboxRefreshTimer.Start();
    }

    return found;
}

void AimAssist::updateSimulatedJitter() {
    float intensity = std::clamp(settings.jitterPercent, 0.0f, 100.0f) / 100.0f;
    if (intensity <= 0.0001f) {
        simulatedJitterYaw = simulatedJitterPitch = 0.0f;
        simulatedJitterTargetYaw = simulatedJitterTargetPitch = 0.0f;
        return;
    }

    if (simulatedJitterTimer.Elapsed<>() >= static_cast<float>(simulatedJitterIntervalMs)) {
        float amplitude = std::lerp(0.015f, 0.42f, intensity);
        float impulse = randFloat(0.0f, 1.0f) < (0.04f + intensity * 0.14f) ? randFloat(1.15f, 1.8f) : 1.0f;
        simulatedJitterTargetYaw = randFloat(-amplitude, amplitude) * impulse;
        simulatedJitterTargetPitch = randFloat(-amplitude * 0.65f, amplitude * 0.65f) * impulse;

        int intervalMin = static_cast<int>(std::lerp(70.0f, 11.0f, intensity));
        int intervalMax = static_cast<int>(std::lerp(140.0f, 38.0f, intensity));
        simulatedJitterIntervalMs = randInt(intervalMin, intervalMax);
        simulatedJitterTimer.Start();
    }

    float response = std::lerp(0.12f, 0.72f, intensity);
    simulatedJitterYaw = std::lerp(simulatedJitterYaw, simulatedJitterTargetYaw, response);
    simulatedJitterPitch = std::lerp(simulatedJitterPitch, simulatedJitterTargetPitch, response);
}

void AimAssist::applyAimController(float yawDiff, float pitchDiff) {
    float dt = controllerTimer.Elapsed<std::chrono::duration<float, std::milli>>() / 1000.0f;
    controllerTimer.Start();
    
    
    
    
    constexpr float kLoopPeriodSec = 0.002f;  
    if (!(dt > 0.0001f) || dt > 0.02f)
        dt = kLoopPeriodSec;

    const float speed = std::clamp(settings.speed, 1.0f, 120.0f);
    float rate = 3.0f + (speed / 120.0f) * 13.0f;
    if (settings.aimMode == AimMode::ADAPTIVE) {
        const float error = std::sqrt(yawDiff * yawDiff + pitchDiff * pitchDiff);
        rate *= std::clamp(0.75f + error / 90.0f, 0.75f, 1.55f);
    } else if (settings.aimMode == AimMode::BLATANT) {
        rate = 18.0f + (speed / 120.0f) * 10.0f;
    }

    
    
    const float alpha = 1.0f - std::exp(-rate * dt);
    float yawStep = yawDiff * alpha;
    float pitchStep = settings.horizontalOnly ? 0.0f : pitchDiff * alpha;
    if (std::abs(yawDiff) < 0.03f)
        yawStep = yawDiff;
    if (std::abs(pitchDiff) < 0.03f)
        pitchStep = pitchDiff;

    updateSimulatedJitter();
    if (settings.packetRotation) {
        const float gcd = g_cachedGCD > 0.0001f ? g_cachedGCD : calculateGCD(mc.getMouseSensitivity());
        
        
        
        const auto steps = [gcd](float degrees) { return std::round(degrees / gcd); };
        
        
        
        
        
        const float yawCap = std::floor(packetYawBudget_.load(std::memory_order_acquire) / gcd);
        const float pitchCap = std::floor(packetPitchBudget_.load(std::memory_order_acquire) / gcd);
        const float yawSteps = std::clamp(steps(simulatedJitterYaw), -yawCap, yawCap);
        const float pitchSteps =
            settings.horizontalOnly ? 0.0f : std::clamp(steps(simulatedJitterPitch), -pitchCap, pitchCap);
        serverYawOffset_.store(yawSteps * gcd, std::memory_order_relaxed);
        serverPitchOffset_.store(pitchSteps * gcd, std::memory_order_relaxed);
        hasServerRotation_.store(true, std::memory_order_release);
    }

    
    
    
    
    
    
    const float desiredJitterYaw = settings.packetRotation ? 0.0f : simulatedJitterYaw;
    const float desiredJitterPitch =
        (settings.packetRotation || settings.horizontalOnly) ? 0.0f : simulatedJitterPitch;
    yawStep += desiredJitterYaw - appliedJitterYaw_;
    pitchStep += desiredJitterPitch - appliedJitterPitch_;
    appliedJitterYaw_ = desiredJitterYaw;
    appliedJitterPitch_ = desiredJitterPitch;

    
    
    
    
    
    
    
    
    
    
    
    
    const float gcdNow = g_cachedGCD > 0.0001f ? g_cachedGCD : calculateGCD(cachedMouseSensitivity);
    g_cachedGCD = gcdNow;
    yawRotationResidual += yawStep;
    pitchRotationResidual += pitchStep;
    float yawApply = applyGCDStrict(static_cast<float>(yawRotationResidual), gcdNow);
    float pitchApply = applyGCDStrict(static_cast<float>(pitchRotationResidual), gcdNow);
    varyStepCounts(yawApply, pitchApply, gcdNow, rng);
    yawRotationResidual -= yawApply;
    pitchRotationResidual -= pitchApply;

    if (std::abs(yawApply) > 0.0001f) {
        const float currentYaw = player.getYaw();
        const float nextYaw = currentYaw + yawApply;
        player.setYaw(nextYaw);
        player.setPrevYaw(std::lerp(currentYaw, nextYaw, 0.82f));
        markRotationWrite();
    }
    if (!settings.horizontalOnly && std::abs(pitchApply) > 0.0001f) {
        const float currentPitch = player.getPitch();
        
        const float nextPitch = limitPitchOnGrid(currentPitch + pitchApply, gcdNow);
        pitchApply = nextPitch - currentPitch;
        if (std::abs(pitchApply) > 0.0001f) {
            player.setPitch(nextPitch);
            player.setPrevPitch(std::lerp(currentPitch, nextPitch, 0.82f));
            markRotationWrite();
        }
    }
}

void AimAssist::onOutgoingPacket(JNIEnv* env, jobject packet) {
    if (!enabled || !settings.packetRotation || !env)
        return;
    if (!hasServerRotation_.load(std::memory_order_acquire))
        return;

    resolveAimC03(env);
    if (!g_aimC03 || !env->IsInstanceOf(packet, g_aimC03))
        return;

    const float offY = serverYawOffset_.load(std::memory_order_relaxed);
    const float offP = serverPitchOffset_.load(std::memory_order_relaxed);

    
    float finalYaw = 0.0f, finalPitch = 0.0f;
    bool haveYaw = g_aim_fid_yaw != nullptr;
    bool havePitch = g_aim_fid_pitch != nullptr && !settings.horizontalOnly;
    if (haveYaw)
        finalYaw = env->GetFloatField(packet, g_aim_fid_yaw) + offY;
    if (havePitch) {
        const float raw = env->GetFloatField(packet, g_aim_fid_pitch) + offP;
        finalPitch = limitPitchOnGrid(
            raw, g_cachedGCD > 0.0001f ? g_cachedGCD : calculateGCD(cachedMouseSensitivity));
    }

    
    
    
    
    
    
    
    
    
    
    static float s_lastSentYaw = 0.0f;
    static float s_lastSentPitch = 0.0f;
    static bool s_haveLastSent = false;
    static float s_ditherSign = 1.0f;

    if (s_haveLastSent && haveYaw) {
        bool yawDup = std::abs(finalYaw - s_lastSentYaw) < 1e-4f;
        bool pitchDup = havePitch ? (std::abs(finalPitch - s_lastSentPitch) < 1e-4f) : true;
        if (yawDup && pitchDup) {
            float gcd = g_cachedGCD > 0.0001f ? g_cachedGCD : calculateGCD(cachedMouseSensitivity);
            if (gcd < 0.0001f)
                gcd = 0.15f;
            
            
            
            
            
            
            
            
            
            
            const float budget = packetYawBudget_.load(std::memory_order_acquire);
            float sign = s_ditherSign;
            if (budget > 0.0f && std::abs(offY + sign * gcd) > budget &&
                std::abs(offY - sign * gcd) <= budget)
                sign = -sign;
            finalYaw += sign * gcd;
            s_ditherSign = -sign;
        }
    }

    if (haveYaw)
        env->SetFloatField(packet, g_aim_fid_yaw, finalYaw);
    if (havePitch)
        env->SetFloatField(packet, g_aim_fid_pitch, finalPitch);
    
    if (g_aim_fid_rotating)
        env->SetBooleanField(packet, g_aim_fid_rotating, JNI_TRUE);

    s_lastSentYaw = finalYaw;
    if (havePitch)
        s_lastSentPitch = finalPitch;
    s_haveLastSent = true;

    if (env->ExceptionCheck())
        env->ExceptionClear();
}

bool AimAssist::isHoldingWeapon() {
    int id = player.getHeldItemId();
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

bool AimAssist::getAimPoint(double targetX, double targetY, double targetZ, double& aimX, double& aimY, double& aimZ) {
    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();
    float localYaw = player.getYaw();

    
    if (targetHasHitbox) {
        double minX = targetHitboxMinX;
        double maxX = targetHitboxMaxX;
        double minY = targetHitboxMinY;
        double maxY = targetHitboxMaxY;
        double minZ = targetHitboxMinZ;
        double maxZ = targetHitboxMaxZ;
        double height = maxY - minY;

        
        
        
        
        const double boxCenterX = (minX + maxX) * 0.5;
        const double boxCenterZ = (minZ + maxZ) * 0.5;
        if (std::abs(boxCenterX - targetX) > 1.5 || std::abs(boxCenterZ - targetZ) > 1.5) {
            const double halfW = std::clamp((maxX - minX) * 0.5, 0.25, 0.45);
            const double halfD = std::clamp((maxZ - minZ) * 0.5, 0.25, 0.45);
            minX = targetX - halfW;
            maxX = targetX + halfW;
            minZ = targetZ - halfD;
            maxZ = targetZ + halfD;
        }
        if (std::abs(minY - targetY) > 1.5 || maxY < targetY - 0.25) {
            minY = targetY;
            maxY = targetY + std::clamp(height, 1.5, 2.0);
            height = maxY - minY;
        }

        
        
        
        resolvedBox_ = {minX, minY, minZ, maxX, maxY, maxZ, true};

        if (settings.aimAtClosestPoint) {
            aimX = std::clamp(localX, minX, maxX);
            
            
            
            aimY = minY + height * 0.5;
            aimZ = std::clamp(localZ, minZ, maxZ);
            
            
            
            
            double hdx = aimX - localX, hdz = aimZ - localZ;
            if (hdx * hdx + hdz * hdz < 0.02) {
                aimX = (minX + maxX) * 0.5;
                aimZ = (minZ + maxZ) * 0.5;
            }
            return true;
        }

        
        int boneIdx = settings.aimBone;
        if (boneIdx < 0 || boneIdx >= kBoneCount)
            boneIdx = 0;
        const Bone& bone = kBones[boneIdx];
        double width = maxX - minX;
        double depth = maxZ - minZ;
        aimX = minX + width * bone.xRatio;
        aimZ = minZ + depth * bone.zRatio;
        aimY = height > 0.01 ? minY + height * bone.yRatio : targetY;
        (void)localYaw;  
        return true;
    }

    constexpr double HALF_WIDTH = 0.3;

    
    resolvedBox_ = {targetX - HALF_WIDTH, targetY,      targetZ - HALF_WIDTH,
                    targetX + HALF_WIDTH, targetY + 1.8, targetZ + HALF_WIDTH,
                    true};

    if (settings.aimAtClosestPoint) {
        double minX = targetX - HALF_WIDTH;
        double maxX = targetX + HALF_WIDTH;
        
        
        double minY = targetY;
        double maxY = targetY + 1.8;
        double minZ = targetZ - HALF_WIDTH;
        double maxZ = targetZ + HALF_WIDTH;

        aimX = std::clamp(localX, minX, maxX);
        aimY = std::clamp(localY + (player.isSneaking() ? 1.54 : 1.62), minY + 0.3, maxY - 0.2);
        aimZ = std::clamp(localZ, minZ, maxZ);
        return true;
    } else {
        
        
        
        aimX = targetX;
        aimY = targetY + 0.9;
        aimZ = targetZ;
        return true;
    }
}

void AimAssist::onRenderMenu() {
    static const char* targetModes[] = {"Lowest Health", "Closest to Player", "Closest to Crosshair", "Switch"};
    static const char* sortByModes[] = {"Distance", "Health", "FOV", "Hurt Time"};
    int currentMode = static_cast<int>(settings.targetMode);
    int currentSortBy = static_cast<int>(settings.switchSortBy);

    bool enabledValue = enabled;
    ui::two_column(
        "Settings",
        [this, &currentMode, &currentSortBy, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();
            static const char* aimModes[] = {"Smooth", "Adaptive", "Blatant"};
            int am = static_cast<int>(settings.aimMode);
            if (ui::Combo("Mode", &am, aimModes, IM_ARRAYSIZE(aimModes)))
                settings.aimMode = static_cast<AimMode>(am);

            if (settings.aimMode == AimMode::BLATANT) {
                ImGui::TextDisabled("Fast lock with bounded smoothing");
            } else {
                if (settings.aimMode == AimMode::ADAPTIVE)
                    ImGui::TextDisabled("Speed auto-tunes to error, distance & motion");
                else
                    ui::SliderFloat("Speed", &settings.speed, 1.0f, 120.0f, "%.0f", true);
                ui::SliderInt("FOV", &settings.fov, 10, 360, "%d");
                ui::SliderFloat("Distance", &settings.distance, 3.0f, 8.0f, "%.1f");
                ui::SliderFloat("Prediction", &settings.prediction, 0.0f, 100.0f, "%.0f%%");
                ui::SliderInt("Reaction Time", &settings.reactionTime, 0, 200, "%d ms");
                ui::SliderFloat("Jitter", &settings.jitterPercent, 0.0f, 100.0f, "%.0f%%");
                ui::Checkbox("Smooth Camera", &settings.packetRotation);
                if (settings.packetRotation)
                    ImGui::TextDisabled("Jitter only on sent packet (1st person smooth)");
            }
        },
        "Options",
        [this, &currentMode, &currentSortBy]() {
            ui::Checkbox("Horizontal Only", &settings.horizontalOnly);
            ui::Checkbox("Always Aim", &settings.alwaysAim);
            ui::Checkbox("Break Blocks", &settings.breakBlocks);
            ui::Checkbox("Weapon Only", &settings.weaponOnly);
            ui::Checkbox("Aim Closest Point", &settings.aimAtClosestPoint);
            if (!settings.aimAtClosestPoint) {
                const char* boneNames[kBoneCount];
                for (int i = 0; i < kBoneCount; ++i)
                    boneNames[i] = kBones[i].name;
                int boneIdx = std::clamp(settings.aimBone, 0, kBoneCount - 1);
                if (ui::Combo("Bone", &boneIdx, boneNames, kBoneCount))
                    settings.aimBone = boneIdx;
            }
            ui::Checkbox("Target Invisibles", &settings.invisibles);
            ui::Checkbox("Stop On Hitbox", &settings.stopOnHitbox);
            bool entityLock = entityLockEnabled_.load(std::memory_order_acquire);
            if (ui::Checkbox("Entity Lock", &entityLock))
                setEntityLock(entityLock);
            ImGui::Spacing();
            if (ui::Combo("Target Mode", &currentMode, targetModes, IM_ARRAYSIZE(targetModes))) {
                settings.targetMode = static_cast<TargetMode>(currentMode);
            }
            
            if (settings.targetMode == TargetMode::SWITCH) {
                if (ui::Combo("Sort By", &currentSortBy, sortByModes, IM_ARRAYSIZE(sortByModes))) {
                    settings.switchSortBy = static_cast<SortBy>(currentSortBy);
                }
                
                if (settings.switchSortBy == SortBy::HURT_TIME) {
                    ui::Checkbox("Lock", &settings.hurtTimeLock);
                    if (settings.hurtTimeLock) {
                        ImGui::TextDisabled("Instant aim on target switch");
                    } else {
                        ImGui::TextDisabled("Targets least recently hit player");
                    }
                }
            }
            ImGui::Spacing();
            
            
            ImGui::Text("Armor Filter (target tiers):");
            auto tierCheckbox = [this](const char* label, int bit) {
                bool on = (settings.armorTierMask & bit) != 0;
                if (ui::Checkbox(label, &on)) {
                    if (on) settings.armorTierMask |= bit;
                    else settings.armorTierMask &= ~bit;
                }
            };
            tierCheckbox("No Armor", ARMOR_NO_ARMOR);
            tierCheckbox("Leather", ARMOR_LEATHER);
            tierCheckbox("Chainmail", ARMOR_CHAIN);
            tierCheckbox("Iron", ARMOR_IRON);
            tierCheckbox("Gold", ARMOR_GOLD);
            tierCheckbox("Diamond", ARMOR_DIAMOND);
            if (settings.armorTierMask == 0) {
                ImGui::TextDisabled("No tiers checked - targets everyone");
            } else {
                ImGui::TextDisabled("Targets players matching any checked tier");

                
                ui::Binder("Armor Filter Key", &settings.armorFilterKeybind);
                if (settings.armorFilterKeybind != 0)
                    ImGui::TextDisabled("Filter is currently %s",
                                        settings.armorFilterEnabled ? "ON" : "OFF");
            }
        });
}

std::string AimAssist::getDisplayInfo() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", settings.speed);
    return buf;
}



void AimAssist::saveSettings(json::Value& node) const {
    node["horizontalOnly"] = settings.horizontalOnly;
    node["speed"] = settings.speed;
    node["fov"] = settings.fov;
    node["distance"] = settings.distance;
    node["reactionTime"] = settings.reactionTime;
    node["aimMode"] = static_cast<int>(settings.aimMode);
    node["targetMode"] = static_cast<int>(settings.targetMode);
    node["switchSortBy"] = static_cast<int>(settings.switchSortBy);
    node["hurtTimeLock"] = settings.hurtTimeLock;
    node["breakBlocks"] = settings.breakBlocks;
    node["weaponOnly"] = settings.weaponOnly;
    node["aimAtClosestPoint"] = settings.aimAtClosestPoint;
    node["aimBone"] = settings.aimBone;
    node["alwaysAim"] = settings.alwaysAim;
    node["invisibles"] = settings.invisibles;
    node["entityLock"] = settings.entityLock;
    node["stopOnHitbox"] = settings.stopOnHitbox;
    node["jitterPercent"] = settings.jitterPercent;
    node["prediction"] = settings.prediction;
    node["packetRotation"] = settings.packetRotation;
    node["armorFilter"] = static_cast<int>(settings.armorFilter);
    node["armorTierMask"] = settings.armorTierMask;
    node["armorFilterEnabled"] = settings.armorFilterEnabled;
    node["armorFilterKeybind"] = settings.armorFilterKeybind;
}

void AimAssist::loadSettings(const json::Value& node) {
    settings.horizontalOnly = node["horizontalOnly"].as_bool(settings.horizontalOnly);
    settings.speed = std::clamp(node["speed"].as_float(settings.speed), 1.0f, 120.0f);
    settings.fov = node["fov"].as_int(settings.fov);
    settings.distance = node["distance"].as_float(settings.distance);
    settings.reactionTime = node["reactionTime"].as_int(settings.reactionTime);
    settings.aimMode = static_cast<AimMode>(node["aimMode"].as_int(static_cast<int>(settings.aimMode)));
    settings.targetMode =
        static_cast<TargetMode>(node["targetMode"].as_int(static_cast<int>(settings.targetMode)));
    settings.switchSortBy =
        static_cast<SortBy>(node["switchSortBy"].as_int(static_cast<int>(settings.switchSortBy)));
    settings.hurtTimeLock = node["hurtTimeLock"].as_bool(settings.hurtTimeLock);
    settings.breakBlocks = node["breakBlocks"].as_bool(settings.breakBlocks);
    settings.weaponOnly = node["weaponOnly"].as_bool(settings.weaponOnly);
    settings.aimAtClosestPoint = node["aimAtClosestPoint"].as_bool(settings.aimAtClosestPoint);
    settings.aimBone = std::clamp(node["aimBone"].as_int(settings.aimBone), 0, kBoneCount - 1);
    settings.alwaysAim = node["alwaysAim"].as_bool(settings.alwaysAim);
    settings.invisibles = node["invisibles"].as_bool(settings.invisibles);
    
    
    
    bool entityLock = node.has("entityLock") ? node["entityLock"].as_bool(settings.entityLock)
                                             : node["lockAim"].as_bool(settings.entityLock);
    setEntityLock(entityLock);
    settings.stopOnHitbox = node["stopOnHitbox"].as_bool(settings.stopOnHitbox);
    settings.jitterPercent =
        (std::clamp)(node["jitterPercent"].as_float(settings.jitterPercent), 0.0f, 100.0f);
    settings.prediction = (std::clamp)(node["prediction"].as_float(settings.prediction), 0.0f, 100.0f);
    settings.packetRotation = node["packetRotation"].as_bool(settings.packetRotation);
    
    
    settings.armorFilter =
        static_cast<ArmorFilter>(node["armorFilter"].as_int(static_cast<int>(settings.armorFilter)));
    if (node.has("armorTierMask")) {
        settings.armorTierMask = node["armorTierMask"].as_int(settings.armorTierMask);
    } else {
        
        switch (settings.armorFilter) {
            case ArmorFilter::NONE:      settings.armorTierMask = ARMOR_NO_ARMOR; break;
            case ArmorFilter::LEATHER:   settings.armorTierMask = ARMOR_LEATHER;  break;
            case ArmorFilter::CHAINMAIL: settings.armorTierMask = ARMOR_CHAIN;    break;
            case ArmorFilter::IRON:      settings.armorTierMask = ARMOR_IRON;     break;
            case ArmorFilter::GOLD:      settings.armorTierMask = ARMOR_GOLD;     break;
            case ArmorFilter::DIAMOND:   settings.armorTierMask = ARMOR_DIAMOND;  break;
            default:                     settings.armorTierMask = 0;              break;
        }
    }
    settings.armorFilterEnabled = node["armorFilterEnabled"].as_bool(settings.armorFilterEnabled);
    settings.armorFilterKeybind = node["armorFilterKeybind"].as_int(settings.armorFilterKeybind);
}
