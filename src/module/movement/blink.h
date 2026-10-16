#pragma once
#include "../module.h"
#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include <vector>
#include <mutex>
#include <windows.h>

struct Vec3d {
    double x, y, z;
};

class Blink : public Module {
public:
    Blink();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    
    bool shouldBlockPacket();
    void queuePacket(JNIEnv* env, jobject packet);
    void flushQueuedPackets();
    void clearQueuedPackets();

    struct Settings {
        float timerLimit = 3.0f;                           
        bool movementOnly = true;                          
        bool showTimerHud = true;                          
        bool showBreadcrumbs = true;                       
        float blinkColor[4] = {1.0f, 0.55f, 0.0f, 0.78f};  
    } settings;

    
    bool isBlinking = false;
    ULONGLONG blinkStartTime = 0;

    
    std::vector<Vec3d> pathPositions;
    RenderPos cachedRenderPos{};
    bool hasRenderPos = false;
    std::mutex pathMutex;

    
    std::vector<jobject> queuedPackets;
    std::mutex queueMutex;
};

extern Blink* blink;
