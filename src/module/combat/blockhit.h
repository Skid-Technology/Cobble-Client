#pragma once

#include <jni.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

#include "../module.h"





















































































































































































































































class BlockHit : public Module {
public:
    BlockHit();

    void onEnable() override;
    void onDisable() override;
    void onWorldChange() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    
    

    
    
    void onClientTick(JNIEnv* env);
    
    
    bool needsClientTick() const;

    
    bool isServerBlocking() const { return serverBlocking_.load(std::memory_order_acquire); }

private:
    
    std::uint32_t tick_ = 0;
    std::uint32_t blockedAtTick_ = 0;
    std::uint32_t releasedAtTick_ = 0;
    std::uint32_t lastHitTakenTick_ = 0;
    bool haveTakenHit_ = false;
    int prevHurtTime_ = 0;
    
    std::uint32_t beatsHeld_ = 0;
    
    
    bool holePending_ = false;
    
    
    bool immuneHole_ = false;
    
    
    
    bool wasSprinting_ = false;

    
    std::uint32_t beatsPerHole_ = 4;    
    std::uint32_t minHoldTicks_ = 3;    
    std::uint32_t maxHoldTicks_ = 26;   
    std::uint32_t holeTicks_ = 6;       
    std::uint32_t immuneMargin_ = 3;    
    std::mt19937 rng_;

    
    bool apiTried_ = false;
    bool apiUsable_ = false;  
    jmethodID midOnStoppedUsing_ = nullptr;  
    
    
    jfieldID fMovementInput_ = nullptr;      
    jfieldID fInputStrafe_ = nullptr;        
    jfieldID fInputForward_ = nullptr;       
    jfieldID fSprintToggle_ = nullptr;       
    
    
    jfieldID fRidingEntity_ = nullptr;       

    
    bool prevAttackDown_ = false;
    std::chrono::steady_clock::time_point observedAutoClick_{};

    
    
    std::atomic<std::uint32_t> attackSignals_{0};
    
    std::atomic<bool> serverBlocking_{false};
    
    std::atomic<const char*> gateReason_{nullptr};

    static std::uint64_t nowMs();
    static bool isSwordId(int id);
    bool isHoldingSword() const;
    
    
    
    bool fightLive() const;

    
    void rollCycle();

    
    
    bool resolveApi(JNIEnv* env, jobject playerInstance);
    bool clientUsingItem(JNIEnv* env, jobject playerInstance) const;
    bool isRiding(JNIEnv* env, jobject playerInstance) const;
    
    
    
    
    
    void applyVanillaSlowdown(JNIEnv* env, jobject playerInstance);
    
    
    
    
    
    
    
    bool undoVanillaSlowdown(JNIEnv* env, jobject playerInstance, bool scaled);
    
    bool sendBlock(JNIEnv* env, jobject playerInstance);
    
    
    
    void sendRelease(JNIEnv* env, jobject playerInstance, bool immune = false);

    
    void standDown(JNIEnv* env, jobject playerInstance);
    
    
    
    
    
    
    
    void abandonBlock();
};

extern BlockHit* blockHit;
