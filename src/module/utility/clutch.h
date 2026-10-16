#pragma once

#include <jni.h>

#include "../module.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>
#include <vector>






































enum class PlaceMode { Scaffold = 0, Clutch = 1 };

class Clutch : public Module {
public:
    struct Settings {
        float scaffoldLead = 0.9f;  

        int predictionMs = 400;     
        int searchRadius = 3;       
        int safeDepth = 4;          
        bool allowForward = false;  

        float reach = 4.2f;

        
        int reactMinMs = 110;    
        int reactMaxMs = 240;
        int aimTicks = 3;        
        float aimNoise = 0.6f;   
        bool overshoot = true;   
        bool snapBack = true;    
        bool restoreSlot = true;
        int missChance = 0;      

        bool cameraWeight = true;  

        
        bool showBps = true;
        bool showPath = true;
        bool showPlacement = true;
    } settings;

    explicit Clutch(PlaceMode mode);
    ~Clutch() override;

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    
    
    
    void onClientTick(JNIEnv* env);
    
    bool needsClientTick() const {
        return enabled || owningRotation_.load(std::memory_order_acquire);
    }

    
    
    bool isRewritingRotation() const { return owningRotation_.load(std::memory_order_acquire); }

private:
    struct Placement {
        int targetX = 0;
        int targetY = 0;
        int targetZ = 0;
        int supportX = 0;
        int supportY = 0;
        int supportZ = 0;
        double hitX = 0.0;
        double hitY = 0.0;
        double hitZ = 0.0;
        double score = 0.0;
        bool valid = false;
    };

    struct PathPoint {
        double relX = 0.0;
        double relY = 0.0;
        double relZ = 0.0;
        bool hitBlock = false;
    };

    
    
    
    
    
    enum class Phase { Idle, React, Aim, Place, Settle };

    static std::uint64_t nowMs();
    static bool isPlaceableBlock(int id);
    static float wrapDegrees(float angle);
    static float smoothstep(float t);

    int findBlockSlot() const;
    bool isDangerous(double predictedX, double predictedY, double predictedZ) const;
    bool isPlacementUsable(const Placement& placement) const;
    Placement findPlacement(double predictedX, double predictedY, double predictedZ) const;
    
    bool wantsPlacement(Placement& out) const;

    void aimAt(const Placement& p, bool freshOpportunity);
    void beginSettle();
    void abort(JNIEnv* env);
    void restoreSlot();
    void publishPreview(const Placement& p);

    
    
    void applyRotation(float yaw, float pitch);

    int rollReactionMs();
    bool rollMiss();

    const PlaceMode placeMode_;
    bool isClutch() const { return placeMode_ == PlaceMode::Clutch; }
    bool isScaffold() const { return placeMode_ == PlaceMode::Scaffold; }

    
    Phase phase_ = Phase::Idle;
    Placement target_;
    int slot_ = -1;
    int originalSlot_ = -1;
    std::uint64_t reactDeadlineMs_ = 0;
    std::uint64_t restoreSlotAtMs_ = 0;
    std::uint64_t nextPlaceMs_ = 0;
    int aimTick_ = 0;
    int aimTotalTicks_ = 0;
    float aimFromYaw_ = 0.0f;
    float aimFromPitch_ = 0.0f;
    float aimToYaw_ = 0.0f;
    float aimToPitch_ = 0.0f;
    float returnYaw_ = 0.0f;
    float returnPitch_ = 0.0f;
    bool haveReturnView_ = false;
    int placesThisRun_ = 0;
    std::mt19937 rng_{std::random_device{}()};

    
    
    std::atomic<bool> owningRotation_{false};
    std::atomic<const char*> status_{nullptr};

    
    mutable std::mutex pathMutex_;
    std::vector<PathPoint> cachedPath_;
    Placement cachedPlacement_;
    std::uint64_t nextPathMs_ = 0;

    double cachedLocalX_ = 0.0;
    double cachedLocalY_ = 0.0;
    double cachedLocalZ_ = 0.0;
    double cachedLocalLastX_ = 0.0;
    double cachedLocalLastY_ = 0.0;
    double cachedLocalLastZ_ = 0.0;
    double cachedRenderX_ = 0.0;
    double cachedRenderY_ = 0.0;
    double cachedRenderZ_ = 0.0;
    double cachedMotionX_ = 0.0;
    double cachedMotionY_ = 0.0;
    double cachedMotionZ_ = 0.0;
    bool cachedPlayerValid_ = false;
    float cachedPartialTicks_ = 0.0f;
    long long lastUpdateTick_ = 0;
};


extern Clutch* scaffold;
extern Clutch* clutch;
