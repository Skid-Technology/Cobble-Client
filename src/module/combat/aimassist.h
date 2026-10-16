#pragma once

#include <jni.h>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>
#include "../module.h"


class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;

public:
    Timer() { Start(); }
    void Start() { m_start = std::chrono::high_resolution_clock::now(); }

    template <typename DurationT = std::chrono::milliseconds>
    float Elapsed() const {
        return (float)std::chrono::duration_cast<DurationT>(std::chrono::high_resolution_clock::now() - m_start)
            .count();
    }
};




class AimBoost {
public:
    AimBoost(float speed_mult_min, float speed_mult_max, float freq_min_ms, float freq_max_ms, bool continuous);
    AimBoost() = default;

    float speed_mult_min = 0.5f;
    float speed_mult_max = 1.8f;
    uint32_t frequency_min_ms = 400;
    uint32_t frequency_max_ms = 700;
    uint32_t frequency_ms = 500;
    bool continuous = true;

    float Delay();

private:
    float prev_speed_mult = 1.f;
    float speed_mult = 1.f;
    Timer timer;
};

class AimAssist : public Module {
public:
    AimAssist();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onWorldChange() override;
    void onRenderMenu() override;

    
    
    
    
    void onOutgoingPacket(JNIEnv* env, jobject packet);

    
    
    
    
    
    
    
    
    
    
    
    
    bool isRewritingRotation() const;

    std::string getDisplayInfo() override;
    void setEntityLock(bool enabled);
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    enum class TargetMode { HEALTH, DISTANCE, FOV, SWITCH };
    enum class SortBy { DISTANCE, HEALTH, FOV, HURT_TIME };

    
    
    
    
    
    
    
    enum class AimMode { SMOOTH = 0, ADAPTIVE = 1, BLATANT = 2 };

    
    enum class ArmorFilter {
        ANY       = 0,  
        NONE      = 1,  
        LEATHER   = 2,
        CHAINMAIL = 3,
        IRON      = 4,
        GOLD      = 5,
        DIAMOND   = 6,
    };

    
    
    
    struct Bone {
        const char* name;
        float xRatio;
        float yRatio;
        float zRatio;
    };
    static constexpr Bone kBones[] = {
        {"Center", 0.5f, 0.5f, 0.5f},
    };
    static constexpr int kBoneCount = static_cast<int>(sizeof(kBones) / sizeof(kBones[0]));

    
    
    
    enum ArmorTierBit {
        ARMOR_NO_ARMOR = 1 << 0,  
        ARMOR_LEATHER  = 1 << 1,
        ARMOR_CHAIN    = 1 << 2,
        ARMOR_IRON     = 1 << 3,
        ARMOR_GOLD     = 1 << 4,
        ARMOR_DIAMOND  = 1 << 5,
    };

    struct Settings {
        bool horizontalOnly = false;  
        bool invisibles = false;
        bool alwaysAim = false;
        bool breakBlocks = false;
        bool aimAtClosestPoint = false;
        bool entityLock = false;  
        bool weaponOnly = false;
        bool stopOnHitbox = false;  
        
        

        AimMode aimMode = AimMode::SMOOTH;
        TargetMode targetMode = TargetMode::DISTANCE;
        SortBy switchSortBy = SortBy::DISTANCE;  
        bool hurtTimeLock = false;               
        int fov = 180;
        float distance = 5.0f;
        float speed = 50.0f;
        float jitterPercent = 0.0f;
        float prediction = 0.0f;      
        bool packetRotation = false;  
        int reactionTime = 50;  

        int aimBone = 0;  

        ArmorFilter armorFilter = ArmorFilter::ANY;  
        int armorTierMask = 0;                       
        bool armorFilterEnabled = true;              
        int armorFilterKeybind = 0;                  
    } settings;

private:
    void releaseCachedTarget(JNIEnv* env);
    void resetTargetState(JNIEnv* env);
    void updateSimulatedJitter();
    
    
    
    
    void markAimIdle() { controllerTimer.Start(); }

    
    
    void markRotationWrite();
    std::atomic<unsigned long long> lastRotWriteMs_{0};

    
    bool isHoldingWeapon();

    
    float wrapTo180(float value);
    std::pair<float, float> getAngles(double x1, double y1, double z1, double x2, double y2, double z2);
    float randFloat(float min, float max);
    int randInt(int min, int max);
    float slerp(float start, float end, float t);

    
    bool findTarget(double& targetX, double& targetY, double& targetZ);

    
    
    
    void applyAimController(float yawDiff, float pitchDiff);

    
    bool getAimPoint(double targetX, double targetY, double targetZ, double& aimX, double& aimY, double& aimZ);

    
    
    
    
    struct AimBox {
        double minX = 0.0, minY = 0.0, minZ = 0.0;
        double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
        bool valid = false;
    } resolvedBox_;

    
    AimBoost yawBoost{0.9f, 1.1f, 450, 600, true};
    AimBoost yawBoost2{1.0f, 1.3f, 800, 1000, false};
    AimBoost pitchBoost{0.8f, 1.2f, 450, 600, true};
    AimBoost pitchBoost2{0.7f, 1.3f, 600, 900, false};

    
    
    
    double yawRotationResidual = 0.0;
    double pitchRotationResidual = 0.0;
    double cursorAccumY = 0.0;
    float cachedMouseSensitivity = 0.5f;

    float pitchdiffSpeed = 0.f;
    float yawdiffSpeed = 0.f;
    float targetYawSpeed = 0.f;
    float targetPitchSpeed = 0.f;
    bool reactionPrimed = false;
    bool targetHasHitbox = false;
    float targetHitboxMinX = 0.0f;
    float targetHitboxMinY = 0.0f;
    float targetHitboxMinZ = 0.0f;
    float targetHitboxMaxX = 0.0f;
    float targetHitboxMaxY = 0.0f;
    float targetHitboxMaxZ = 0.0f;
    bool armorFilterKeyPrev_ = false;  
    bool prevFrameHadTarget = false;
    bool targetChangedThisFrame = false;
    bool clickStartedThisFrame = false;
    bool lastMouseDown = false;
    Timer targetRampTimer;
    float targetRampDurationMs = 200.0f;
    Timer clickRampTimer;
    float clickRampDurationMs = 140.0f;
    Timer hitboxRefreshTimer;
    float hitboxRefreshMs = 140.0f;
    float targetSpeedVariance = 1.0f;
    float aimHumanizer = 1.0f;
    Timer simulatedJitterTimer;
    float simulatedJitterYaw = 0.0f;
    float simulatedJitterPitch = 0.0f;
    float simulatedJitterTargetYaw = 0.0f;
    float simulatedJitterTargetPitch = 0.0f;
    int simulatedJitterIntervalMs = 30;
    
    
    
    
    
    
    float appliedJitterYaw_ = 0.0f;
    float appliedJitterPitch_ = 0.0f;

    Timer reactionTimeTimer;
    Timer controllerTimer;

    
    jobject cachedTarget = nullptr;
    std::atomic<bool> entityLockEnabled_{false};
    std::atomic<bool> targetResetRequested_{false};

    
    bool hasLockedTarget = false;
    double lockedTargetX = 0.0;
    double lockedTargetY = 0.0;
    double lockedTargetZ = 0.0;

    
    double predPrevX = 0.0, predPrevY = 0.0, predPrevZ = 0.0;
    double predVelX = 0.0, predVelY = 0.0, predVelZ = 0.0;
    Timer predTimer;

    std::mt19937 rng;
    mutable std::recursive_mutex targetMutex_;

    
    
    std::atomic<bool> hasServerRotation_{false};
    std::atomic<float> serverYawOffset_{0.0f};
    std::atomic<float> serverPitchOffset_{0.0f};

    
    
    
    
    
    std::atomic<float> packetYawBudget_{0.0f};
    std::atomic<float> packetPitchBudget_{0.0f};
};

extern AimAssist* aimAssist;
