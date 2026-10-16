#pragma once

#include "../module.h"




class TargetHUD : public Module {
public:
    TargetHUD();

    void onRender() override;
    void onRenderMenu() override;
};

extern TargetHUD* targetHud;
