#pragma once

#include <string>
#include "../ui/hud/notifications.h"
#include "../util/aes_string.hpp"
#include "../util/logger.hpp"
#include "../util/state.hpp"

namespace json {
class Value;
}

struct Module {
    const char* name;
    bool enabled = false;
    int keybind = 0;               
    bool locked = false;           
    bool renders = false;          
    bool usesPlayerCache = false;  
    
    
    
    
    
    
    
    
    
    
    bool worldVisual = false;

    
    static inline bool experimentalUnlocked = false;

    
    
    static std::string unlockCodeMaterialized() { return OBFUSCATE_STRING("barcelon12"); }
    static bool checkUnlockCode(const char* candidate) {
        if (!candidate)
            return false;
        std::string expected = unlockCodeMaterialized();
        bool ok = (expected.size() == strlen(candidate)) && (memcmp(expected.data(), candidate, expected.size()) == 0);
        SecureZeroMemory(&expected[0], expected.size());
        return ok;
    }

    Module(const char* name) : name(name) {}
    virtual ~Module() = default;

    
    bool isAccessible() const { return !locked || experimentalUnlocked; }
    virtual bool showsDeveloperOnly() const { return locked && !experimentalUnlocked; }

    
    virtual void onEnable() {}

    
    virtual void onDisable() {}

    
    virtual void onWorldChange() {}

    
    virtual void onUpdate() {}

    
    virtual void onRender() {}

    
    
    virtual void onRenderMenu() {}

    
    
    
    
    virtual void saveSettings(json::Value& node) const { (void)node; }

    
    
    
    virtual void loadSettings(const json::Value& node) { (void)node; }

    
    
    virtual std::string getDisplayInfo() { return ""; }

    
    virtual bool canToggle() { return true; }

    
    
    bool shouldUpdate() const { return sdk::internal::isOperational(); }

    void toggle() {
        
        if (locked && !experimentalUnlocked)
            return;

        
        if (!canToggle())
            return;

        enabled = !enabled;
        if (enabled)
            onEnable();
        else
            onDisable();

        
        log_debug("[MOD] %s: %s", name, enabled ? "ENABLED" : "DISABLED");

        
        NotificationManager::add(name, enabled);
    }

    void setEnabled(bool value) {
        if (enabled == value)
            return;
        toggle();
    }
};
