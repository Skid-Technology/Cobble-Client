#pragma once

#include <Windows.h>
#include <vector>
#include "module.h"


class ModuleManager {
public:
    void registerModule(Module* mod);
    void update();
    void onRender();  
    bool hasRenderWork() const;
    bool needsPlayerCache() const;
    void checkKeybinds();

    void onWorldChange();

    const std::vector<Module*>& getModules() const { return modules; }

private:
    std::vector<Module*> modules;

    
    bool keyStates[256] = {false};

    
    bool isMinecraftFocused();
};

extern ModuleManager* moduleManager;
