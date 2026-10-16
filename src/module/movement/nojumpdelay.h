#pragma once

#include "../module.h"

class NoJumpDelay : public Module {
public:
    NoJumpDelay();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
};

extern NoJumpDelay* noJumpDelay;
