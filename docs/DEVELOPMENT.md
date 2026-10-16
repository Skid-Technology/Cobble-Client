# Cobble – Development Guide

This guide explains how to work within the Cobble codebase: creating modules, using the UI system, and following project conventions.

Work on `testing`. `main` is production. Feature branches come off `testing` and merge back into `testing`. See [UPDATE_WORKFLOW.md](UPDATE_WORKFLOW.md#branches) for releases and hotfixes.

---

## Project Structure

```
src/
|-- cobble.cpp/.hpp     # Entry point, module lifecycle
|-- jni/                # JVM bridge
|   |-- apollo/         # Apollo packet observer
|   |-- noitemrelease/  # NoItemRelease sendPacket gate
|   `-- packet/         # PacketInterceptor + bytecode patcher
|-- sdk/                # Minecraft wrappers
|-- module/
|   |-- module.h        # Base class
|   |-- module_manager.h/.cpp
|   |-- combat/         # AutoClicker, AimAssist, Velocity, Reach
|   |-- visual/         # NameTags
|   `-- utility/        # NoJumpDelay
`-- ui/
    |-- core/           # Overlay, ImGui hooks
    |-- widgets/        # Consolidated widgets
    |-- menu/           # Tab navigation
    `-- hud/            # ArrayList, Notifications
```

---

## How to Create a Module

### Step 1: Create Files

```
src/module/<category>/
├── mymodule.h
└── mymodule.cpp
```

### Step 2: Header

```cpp
// src/module/combat/mymodule.h
#pragma once
#include "../module.h"

class MyModule : public Module
{
public:
    MyModule();
    
    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    
    struct Settings {
        float myValue = 5.0f;
        bool myToggle = true;
    } settings;
};

extern MyModule* myModule;
```

### Step 3: Implementation

```cpp
// src/module/combat/mymodule.cpp
#include "mymodule.h"
#include "../../sdk/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"

MyModule* myModule = nullptr;

MyModule::MyModule() : Module("MyModule")
{
    keybind = 'M';  // Default keybind (0 = none)
}

void MyModule::onEnable() { }
void MyModule::onDisable() { }

void MyModule::onUpdate()
{
    if (!enabled) return;
    if (Menu::isOpen) return;
    if (!player.isValid()) return;
    
    // Your logic here
}

void MyModule::onRenderMenu()
{
    ui::two_column(
        "Settings", [this]() {
            ui::CheckboxBind("Enabled", &enabled, &keybind);
            ImGui::Spacing();
            ui::SliderFloat("Value", &settings.myValue, 0.0f, 10.0f, "%.1f");
        },
        "Options", [this]() {
            ui::Checkbox("Toggle", &settings.myToggle);
        }
    );
}
```

### Step 4: Register in cobble.cpp

```cpp
// Add include
#include "module/combat/mymodule.h"

// In initialize()
myModule = new MyModule();
moduleManager->registerModule(myModule);

// In shutdown()
if (myModule) myModule->setEnabled(false);
// ... after Overlay::Shutdown()
if (myModule) { delete myModule; myModule = nullptr; }
```

### Step 5: Add to Menu

**widgets.cpp** – Add to tab subtabs:
```cpp
std::vector<ui::tab_t> ui::tabs = {
    { ICON_FA_CROSSHAIRS, "Combat", { "AutoClicker", ..., "MyModule" } },
    // ...
};
```

**menu.cpp** – Add to module lookup:
```cpp
#include "../../module/combat/mymodule.h"

static Module* getModuleForSubtab(int tabIdx, int subtabIdx)
{
    if (tabIdx == 0) {  // Combat
        switch (subtabIdx) {
            // ...
            case 5: return myModule;
        }
    }
}
```

### Step 6: Add to vcxproj

```xml
<ClCompile Include="src\module\combat\mymodule.cpp" />
```

### Step 7: Add to ConfigManager

**config.cpp** – Add serialization in `applyConfig()` and `buildConfigFromModules()`:

```cpp
// In applyConfig() - load settings from JSON:
else if (name == "MyModule" && myModule) {
    auto& s = myModule->settings;
    s.myValue = data["myValue"].as_float(s.myValue);
    s.myToggle = data["myToggle"].as_bool(s.myToggle);
}

// In buildConfigFromModules() - save settings to JSON:
else if (name == "MyModule" && myModule) {
    const auto& s = myModule->settings;
    config_["modules"][name]["myValue"] = s.myValue;
    config_["modules"][name]["myToggle"] = s.myToggle;
}
```

**Note:** The config is automatically saved when the menu closes. No extra code needed.

---

## UI Guidelines

### Layout Helpers

```cpp
// Two columns (standard layout)
ui::two_column(
    "Left Title", []() { /* widgets */ },
    "Right Title", []() { /* widgets */ }
);

// Single section
ui::section("Title", []() { /* widgets */ });
```

### Available Widgets

| Widget | Usage |
|--------|-------|
| `ui::Checkbox(label, &bool)` | Toggle on/off |
| `ui::CheckboxBind(label, &bool, &key)` | Checkbox + keybind |
| `ui::SliderFloat(label, &float, min, max, fmt)` | Float slider |
| `ui::SliderInt(label, &int, min, max, fmt)` | Int slider |
| `ui::Binder(label, &key)` | Keybind only |
| `ui::color(label, float[4], alpha)` | Color picker |
| `ui::Combo(label, &int, items[], count)` | Dropdown selector |

### Standard onRenderMenu() Pattern

```cpp
void MyModule::onRenderMenu()
{
    ui::two_column(
        "General", [this]() {
            ui::CheckboxBind("Enabled", &enabled, &keybind);
            ImGui::Spacing();
            // Core settings here
        },
        "Conditions", [this]() {
            // Toggles/options here
        }
    );
}
```

### Visual Order Convention

1. **Enabled checkbox** (always first)
2. **Core value sliders**
3. **Mode selector** (if applicable)
4. **Condition checkboxes**

---

## Anti-Patterns

### ❌ Never Do This

```cpp
// Static module (causes crash on inject)
AutoClicker autoClicker;  // WRONG

// Use heap allocation instead:
autoClicker = new AutoClicker();  // CORRECT
```

```cpp
// Hard-coded magic numbers
float delay = 0.0167f;  // WRONG

// Use named constants:
static constexpr float TICK_RATE = 1.0f / 60.0f;  // CORRECT
```

```cpp
// Duplicate widget implementations
// Each file has its own SliderFloat...  // WRONG

// All widgets live in widgets.cpp  // CORRECT
```

```cpp
// Module-specific code in menu.cpp
if (cur_tab == 0 && subtab == 0) {
    ImGui::Checkbox("AutoClicker", &autoClicker->enabled);  // WRONG
}

// Use module->onRenderMenu() instead  // CORRECT
```

```cpp
// Raw ImGui widgets in onRenderMenu() (animation desync)
void onRenderMenu() {
    ImGui::BeginCombo("Mode", ...);  // WRONG - won't fade with menu
}

// Use ui:: wrapped widgets instead:
void onRenderMenu() {
    ui::Combo("Mode", &mode, items, count);  // CORRECT
```

### ❌ JNI Safety

```cpp
// Never call JNI from render thread
void onRenderMenu() {
    player.getHealth();  // WRONG - may crash
}

// Cache values in onUpdate(), display in onRenderMenu()
void onUpdate() {
    cachedHealth = player.getHealth();  // CORRECT
}
```

---

## PacketInterceptor (Apollo + NoItemRelease)

PacketInterceptor is now a focused observer/gate:

- Incoming hooks (Apollo): `channelRead0`, `handleCustomPayload`, `handleTeams`,
  `LCNetHandlerClient.handleNametagsUpdate`, `handleOverrideNametags`
- Outgoing gate (NoItemRelease): `NetworkManager.sendPacket(...)` only for
  `C07PacketPlayerDigging.RELEASE_USE_ITEM`

There is no tick/drain path anymore; Apollo runs on incoming hooks only.

---

## Overlay Context Stability

The overlay is sensitive to context switches (Alt+Tab, resize, fullscreen).

- Reinit only on OpenGL context change, not on every HDC change.
- The overlay binds to the first valid window/context and ignores others.
- A cooldown/suspend window prevents reinit loops and white flashes.
- If you see `GL ERROR 1282` spam, confirm the target window/context is stable.

---

## Build Commands

## Build Commands

```powershell
# Full build (recommended) - Builds DLL + Loader + Signs Manifest
.\build.ps1 -Release

# Prepare for Release (Signing + ZIP)
.\build.ps1 -PrepareRelease

# Quick DLL rebuild (for module development)
msbuild Cobble.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## Testing Autoupdater

1. **Setup Server**: Ensure `cobble-server` is running locally or remotely.
2. **Build Latest**:
   ```powershell
   # In cobble.json: set "version": "1.0.10"
   .\build.ps1 -PrepareRelease
   # Upload out/releases/latest/* to /opt/cobble-server/releases/latest/
   ```
3. **Build Old**:
   ```powershell
   # In cobble.json: set "version": "1.0.0"
   .\build.ps1 -Release
   ```
4. **Run**: Launch `out/bin/Release/CobbleLoader.exe`. It should detect the update ("1.0.10"), download it, and restart itself using the integrated update mode.

### Debugging Update Mode

The loader has an integrated "Update Mode" triggered by CLI args. You generally don't run this manually, but it's good to know:
- **Flag**: `--update-pid <PID>`
- **Behavior**: Waits for PID to exit, then moves `--update-new` file to `--update-old` path.
- **Logs**: If build fails or loops, check `loader/main.cpp` Phase 2.5 logic.


---

## Debugging Tips

### Useful Log Points

```cpp
log_ok("Module enabled: %s", name);
log_err("JNI call failed!");
log_debug("Value: %.2f", someFloat);
```

### Testing Modules Safely

1. Start with module disabled by default
2. Use defensive checks in `onUpdate()`:
   ```cpp
   if (!enabled) return;
   if (Menu::isOpen) return;
   if (!player.isValid()) return;
   ```
3. Test keybind toggle before testing logic

---

## Checklist for New Modules

- [ ] Header with `onRenderMenu()` override
- [ ] Implementation with UI using `ui::two_column()`
- [ ] Registered in `cobble.cpp` (alloc, register, cleanup)
- [ ] Added to `widgets.cpp` tab definition
- [ ] Added to `menu.cpp` subtab lookup
- [ ] Added to `config.cpp` (applyConfig + buildConfigFromModules)
- [ ] Added to `Cobble.vcxproj`
- [ ] Tested build
- [ ] Tested in-game
