#include "module_manager.h"
#include "../jni/packet/bytecode_patcher.hpp"
#include "../jni/java.h"
#include "../sdk/core/minecraft.h"
#include "../sdk/player/render_data.h"
#include "../ui/menu/menu.h"
#include "../util/diagnostics.hpp"
#include "../util/inject_trace.hpp"
#include "../util/state_check.hpp"
#include "../security/vmprotect.h"
#include "../spectate/spectate.hpp"
#include "utility/autorefill.h"

#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Psapi.h>
#include <Windows.h>

#ifdef COBBLE_DEV_BUILD
#include <atomic>
#include <chrono>

namespace ModuleDiag {


struct ModuleHealth {
    const char* name = nullptr;
    uint32_t consecutiveSkips = 0;   
    uint32_t exceptionCount = 0;     
    bool wasOperational = true;      
    bool reportedDegraded = false;   
    bool reportedException = false;  
};

inline ModuleHealth g_health[64];  
inline int g_healthCount = 0;
inline std::atomic<uint64_t> g_loopIterations{0};
inline std::atomic<uint64_t> g_lastPulseLog{0};


constexpr uint64_t PULSE_INTERVAL_MS = 60000;

inline ModuleHealth* getHealth(const char* name) {
    for (int i = 0; i < g_healthCount; i++) {
        if (g_health[i].name == name) return &g_health[i];
    }
    if (g_healthCount < 64) {
        g_health[g_healthCount].name = name;
        return &g_health[g_healthCount++];
    }
    return nullptr;
}

inline void logPulse() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint64_t last = g_lastPulseLog.load();

    if (last == 0) {
        g_lastPulseLog.store(nowMs);
        return;
    }

    if (nowMs - last >= PULSE_INTERVAL_MS) {
        g_lastPulseLog.store(nowMs);
        uint64_t iters = g_loopIterations.load();

        
        int active = 0, degraded = 0, errored = 0;
        for (int i = 0; i < g_healthCount; i++) {
            if (!g_health[i].wasOperational) degraded++;
            if (g_health[i].exceptionCount > 0) errored++;
            
        }

        char buf[256];
        wsprintfA(buf, "[PULSE] loops=%I64u degraded=%d errored=%d",
                  (unsigned long long)iters, degraded, errored);
        cobble_trace::write_step(buf);
    }
}

}  
#endif  

ModuleManager* moduleManager = nullptr;


bool ModuleManager::isMinecraftFocused() {
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);

    
    DWORD ourPid = GetCurrentProcessId();

    return foregroundPid == ourPid;
}

void ModuleManager::registerModule(Module* mod) {
    modules.push_back(mod);
}

void ModuleManager::update() {
    
    if (!isMinecraftFocused())
        return;

    
    
    
    
    if (!sdk::internal::gateAB())
        return;

    COBBLE_DLL_ULTRA_START("Cobble.ModuleManager.Update");
    checkKeybinds();

#ifdef COBBLE_DEV_BUILD
    ModuleDiag::g_loopIterations++;
    ModuleDiag::logPulse();
#endif

    for (auto* mod : modules) {
        if (mod->enabled && mod->shouldUpdate()) {
            
            Diagnostics::g_currentOperation.store(mod->name);

#ifdef COBBLE_DEV_BUILD
            
            auto* health = ModuleDiag::getHealth(mod->name);
            if (health && !health->wasOperational) {
                health->wasOperational = true;
                health->consecutiveSkips = 0;
                health->reportedDegraded = false;
                log_ok("[MOD] %s: recovered (operational again)", mod->name);
            }

            
            __try {
                mod->onUpdate();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                if (health) {
                    health->exceptionCount++;
                    if (!health->reportedException) {
                        health->reportedException = true;
                        log_err("[MOD] %s: SEH exception in onUpdate() (code=0x%08X)",
                                mod->name, GetExceptionCode());
                    }
                }
                
                mod->enabled = false;
                log_err("[MOD] %s: disabled after exception to protect main loop", mod->name);
            }
#else
            mod->onUpdate();
#endif
        }
#ifdef COBBLE_DEV_BUILD
        else if (mod->enabled && !mod->shouldUpdate()) {
            
            auto* health = ModuleDiag::getHealth(mod->name);
            if (health) {
                health->consecutiveSkips++;
                if (health->wasOperational) {
                    
                    health->wasOperational = false;
                    log_warn("[MOD] %s: shouldUpdate()=false (state degraded)", mod->name);
                } else if (!health->reportedDegraded && health->consecutiveSkips >= 500) {
                    
                    health->reportedDegraded = true;
                    log_err("[MOD] %s: persistently degraded for %u ticks",
                            mod->name, health->consecutiveSkips);
                }
            }
        }
#endif
    }
    Diagnostics::g_currentOperation.store("moduleManager.update.done");

    
    
    
    BytecodePatcher::ApplyHideVanillaNames(Java::Env());

    
    if (autoRefill && !autoRefill->enabled && autoRefill->settings.onInventoryOpen) {
        if (autoRefill->shouldUpdate())
            autoRefill->onUpdate();
    }

    
    
    
    SpectateManager::get().tick();

    COBBLE_DLL_ULTRA_END();
}

void ModuleManager::onWorldChange() {
#ifdef COBBLE_DEV_BUILD
    COBBLE_TRACE("ModuleManager::onWorldChange");
#endif
    for (auto* mod : modules) {
        mod->onWorldChange();
    }
#ifdef COBBLE_DEV_BUILD
    COBBLE_TRACE("ModuleManager::onWorldChange:done");
#endif
}

void ModuleManager::onRender() {
    
    
    
    if (!sdk::internal::gateD())
        return;
    COBBLE_DLL_ULTRA_START("Cobble.ModuleManager.Render");
    
    
    
    
    
    
    const bool hideWorldVisuals = RenderData::guiCoversWorld;
    for (auto* mod : modules) {
        if (!mod->enabled)
            continue;
        if (hideWorldVisuals && mod->worldVisual)
            continue;
        mod->onRender();
    }
    COBBLE_DLL_ULTRA_END();
}

bool ModuleManager::hasRenderWork() const {
    for (const auto* mod : modules) {
        if (mod->enabled && mod->renders)
            return true;
    }
    return false;
}

bool ModuleManager::needsPlayerCache() const {
    for (const auto* mod : modules) {
        if (mod->enabled && mod->usesPlayerCache)
            return true;
    }
    return false;
}

void ModuleManager::checkKeybinds() {
    if (mc.isInGui())
        return;
    if (Menu::isOpen)
        return;

    
    std::unordered_map<int, bool> currentStates;
    for (auto* mod : modules) {
        if (mod->keybind != 0 && currentStates.find(mod->keybind) == currentStates.end()) {
            currentStates[mod->keybind] = (GetAsyncKeyState(mod->keybind) & 0x8000) != 0;
        }
    }

    
    for (auto* mod : modules) {
        if (mod->keybind == 0)
            continue;

        bool pressed = currentStates[mod->keybind];
        bool wasPressed = keyStates[mod->keybind];

        if (pressed && !wasPressed) {
            mod->toggle();
        }
    }

    
    for (auto& [key, pressed] : currentStates) {
        keyStates[key] = pressed;
    }
}
