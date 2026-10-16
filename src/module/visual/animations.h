#pragma once

#include "../module.h"

#include <atomic>
#include <cstdint>








































class Animations : public Module {
public:
    Animations();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        int animation = 0;
        float scale = 1.0f;       
        float posX = 0.0f;        
        float posY = 0.0f;
        float posZ = 0.0f;
        float speed = 1.0f;       
    } settings;

    
    static int animationCount();
    static const char* animationName(int index);

    
    
    static bool animationMoves(int index);

    
    
    float swayDegPerSec() const { return sway_.load(std::memory_order_relaxed); }
    float swingSeconds() const { return swingPulse_.load(std::memory_order_relaxed); }

private:
    
    
    static void ensureGlHooks();

    
    
    
    std::atomic<float> sway_{0.0f};
    std::atomic<float> swingPulse_{9.0f};

    
    float lastYaw_ = 0.0f;
    bool haveYaw_ = false;
    std::uint64_t lastSampleMs_ = 0;
    bool prevClick_ = false;
    std::uint64_t swingAtMs_ = 0;
};

extern Animations* animations;
