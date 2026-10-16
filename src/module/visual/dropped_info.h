#pragma once

#include "../module.h"
#include <mutex>

class DroppedInfo : public Module {
public:
    DroppedInfo();

    void onEnable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float x = 20.0f;
        float y = 200.0f;
        float scale = 1.0f;
        bool background = true;
    } settings;

private:
    std::mutex mutex;
    int potionCount = 0;

    
    bool dragging = false;
    float dragOffsetX = 0, dragOffsetY = 0;
};

extern DroppedInfo* droppedInfo;
