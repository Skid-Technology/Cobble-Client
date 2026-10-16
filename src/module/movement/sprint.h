#pragma once

#include "../module.h"

class Sprint : public Module {
public:
    Sprint();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;

private:
    int sprintKeyCode = 0;  
};

extern Sprint* sprint;
