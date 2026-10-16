# Cobble – Architecture Overview

Cobble is an injectable ghost client for Lunar Client (Minecraft), composed of three independent but coordinated systems:

- **Loader** (C++) – Injection, security, licensing
- **Client DLL** (C++ / JNI) – Modules, SDK, UI
- **License Server** (Go) – HWID tracking, rate limiting

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              USER                                        │
└─────────────────────────────────────────────────────────────────────────┘
         │                              │
         ▼                              ▼
┌─────────────────┐            ┌─────────────┐
│  CobbleLoader   │            │  Minecraft  │
│ ┌─────────────┐ │            │ ┌─────────┐ │
│ │ Security:   │ │            │ │Cobble.dll │
│ │ - VMProtect │ │            │ │         │ │
│ │ - VM/AntiDbg│ │            │ │ ┌─────┐ │ │
│ │ - Integrity │ │            │ │ │ JNI │ │ │
│ └─────────────┘ │            │ │ └─────┘ │ │
│        │        │            │ │ ImGui   │ │
│        ▼        │            │ │ Overlay │ │
│ ┌─────────────┐ │◄──HTTPS───►│ └─────────┘ │
│ │ License     │ │            └─────────────┘
│ │ Check       │ │
│ └─────────────┘ │                    ┌─────────────────────┐
│        │        │                    │  License Server     │
│        ▼        │      HTTPS         │  (Go + SQLite/PG)   │
│ ┌─────────────┐ │────────────────────┤                     │
│ │ LoadLibrary │ │                    │  - /api/verify      │
│ │ Injection   │ │                    │  - /api/admin/*     │
│ └─────────────┘ │                    │  - Rate Limiting    │
└─────────────────┘                    └─────────────────────┘
```

---

## Component Responsibilities

### Loader (`loader/`)

| Component | Responsibility |
|-----------|----------------|
| `inject.cpp` | LoadLibrary injection into javaw.exe |
| `process.cpp` | Minecraft/Lunar process detection |
| `security.cpp` | VMProtect integration (anti-debug/VM definitions) |
| `license.cpp` | HTTPS license verification, injection secret retrieval |
| `hwid.cpp` | SHA256(CPU + Disk + MAC) generation |
| `main.cpp` | Creates `InjectionData` shared memory for DLL validation |

### Client DLL (`src/`)

| Component | Responsibility |
|-----------|----------------|
| `jni/` | JVM attach, JNIEnv management, class discovery |
| `sdk/` | Minecraft wrappers (player, world, inventory) |
| `module/` | Feature modules (combat, visual, utility) |
| `ui/` | ImGui overlay, menu, widgets, HUD |
| `config/` | Config persistence, server sync |
| `security/` | All security components (integrity, validation, crypto) |
| `util/` | Helpers organized by domain |

#### Directory Structure

```
src/
├── security/                 # Security & anti-tamper
│   ├── dll_integrity.hpp     # SHA256 .text hash (active)
│   ├── session_validator.hpp # Injection validation + heartbeat
│   ├── loader_verify.hpp     # PEB module list check
│   └── tweetnacl.h           # Ed25519 crypto
│
├── util/
│   ├── math/                 # Vector, geometry, worldToScreen
│   ├── network/              # HTTP client, JSON parser
│   ├── inventory/            # Cache & hooks
│   ├── team/                 # FriendsManager (Apollo + manual)
│   ├── logger.hpp            # Logging utility
│   └── hwid.hpp              # Hardware ID generation
└── ...
```

#### Security Components

| File | Responsibility |
|------|----------------|
| `security/dll_integrity.hpp` | SHA256 hash of .text section (anti-patch) |
| `security/session_validator.hpp` | Validates injection secret + heartbeat thread |
| `security/loader_verify.hpp` | Detects LoadLibrary injection (PEB check) |
| `security/tweetnacl.h` | Ed25519 signature verification |

> **Note**: Only `DllIntegrity::Check()` (SHA256) runs in the heartbeat thread
> (every 2-3 minutes). Legacy FNV-1a system removed.

#### Team Detection Systems

| File | Responsibility | Status |
|------|----------------|--------|
| `util/team/friends_manager.h/.cpp` | Manual friends + Apollo nametag tracking | **Primary** |

> **Note**: Apollo API provides reliable team detection via explicit server packets.
> VisualTeamResolver was removed (caused false positives from particles/shaders).

### Packet Interceptor (JVMTI Bytecode Path)

PacketInterceptor now only coordinates JVMTI bytecode hooks for Apollo and the NoItemRelease gate.

- Incoming observer hooks:
  - `NetworkManager.channelRead0`
  - `NetHandlerPlayClient.handleCustomPayload` (Apollo payload)
  - `NetHandlerPlayClient.handleTeams`
  - `LCNetHandlerClient.handleNametagsUpdate/handleOverrideNametags`
- Outgoing gate:
  - `NetworkManager.sendPacket(...)` for `C07PacketPlayerDigging.RELEASE_USE_ITEM`

Implementation is split into focused units:
- `src/jni/packet/bytecode_patcher.*` (JVMTI patcher + hooks)
- `src/jni/apollo/apollo_packet_observer.*` (Apollo parsing)
- `src/jni/noitemrelease/noitemrelease_gate.*` (NoItemRelease gate)
- `src/jni/packet/packet_interceptor.*` (orchestration)

### Overlay Context Stability

Overlay reinit is guarded to avoid rapid context churn.

- Reinit only triggers when the OpenGL context changes.
- The overlay binds to the first valid window/context and ignores other windows.
- A cooldown/suspend window prevents continuous reinit loops.
- `wglMakeCurrent` failure skips rendering for that frame.

### License Server (`cobble-server` repo, separate)

| Component | Responsibility |
|-----------|----------------|
| `handlers/auth.go` | JWT auth (register, login, refresh, logout, me) |
| `handlers/verify.go` | HWID verification, generates `injection_secret` |
| `handlers/loader.go` | Loader login, generates `injection_secret` |
| `handlers/injection.go` | `/api/dll/validate` - DLL validates its secret |
| `handlers/profile.go` | User profile, password change, HWID reset |
| `handlers/dashboard.go` | Announcements, changelogs, downloads |
| `handlers/admin.go` | User/license CRUD, announcements, logs |
| `middleware/` | CORS, JWT auth, API key auth, rate limiting |
| `database/db.go` | PostgreSQL + `injection_secrets` table |

#### Loader Authentication Flow (v3 - With Injection Secret)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LICENSE VERIFICATION                                  │
├─────────────────────────────────────────────────────────────────────────┤
│  1. POST /api/verify or /api/loader/login                               │
│  2. Server generates 32-byte injection_secret (stored in DB, 5min TTL) │
│  3. Server returns signed response:                                     │
│     { valid, nonce, expires, username, session_token, injection_secret }│
│  4. Loader verifies Ed25519 signature                                   │
│  5. Loader stores injection_secret for handshake                        │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                    INJECTION WITH HANDSHAKE                              │
├─────────────────────────────────────────────────────────────────────────┤
│  1. Loader creates shared memory: "Local\CobbleInjection"              │
│  2. Writes InjectionData struct:                                        │
│     { signature, secret, username, hwid, timestamp, expiresIn }         │
│  3. Loader injects DLL via SecureManualMap                              │
│  4. DLL reads shared memory in SessionValidator::ValidateInjection()    │
│  5. DLL verifies: signature == "COBBLE_INJ_V1", not expired            │
│  6. If invalid → DLL crashes silently (no hints to crackers)           │
│  7. If valid → DLL continues initialization                             │
└─────────────────────────────────────────────────────────────────────────┘
```

> **Security Note**: Extracted DLLs no longer work because they require a
> fresh `injection_secret` from the server, passed via loader shared memory.

### Config System (`src/config/`)

| Component | Responsibility |
|-----------|----------------|
| `config.hpp` | ConfigManager singleton, lifecycle |
| `config.cpp` | Module serialization/deserialization |
| `util/json.hpp` | JSON parser and serializer |
| `util/http.hpp` | WinHTTP client for server sync |
| `util/hwid.hpp` | Hardware ID generation |

---

## Config Persistence Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         STARTUP FLOW                                 │
├─────────────────────────────────────────────────────────────────────┤
│  1. Load %APPDATA%/Cobble/config.json (instant)                     │
│  2. Apply to modules immediately                                     │
│  3. Async fetch from server (non-blocking)                          │
│  4. Merge server config → apply → save local cache                  │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                        SETTINGS CHANGE FLOW                          │
├─────────────────────────────────────────────────────────────────────┤
│  1. User changes setting in menu                                     │
│  2. Menu close → saveLocal() (immediate)                            │
│  3. Debounce 400ms → pushServerConfig() (async)                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Config JSON Structure

```json
{
  "version": 1,
  "flags": {
    "experimentalUnlocked": false
  },
  "hud": {
    "arrayList": true,
    "notifications": true
  },
  "modules": {
    "AutoClicker": {
      "enabled": false,
      "keybind": 0,
      "minCps": 10.0,
      "maxCps": 12.0
    },
    "AimAssist": {
      "enabled": false,
      "keybind": 0,
      "speed": 50.0,
      "fov": 180
    }
  }
}
```

### Server Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/config` | GET | Fetch stored config by HWID |
| `/api/config/save` | POST | Save config (HWID + JSON) |

---

## Execution Flow

```
CobbleLoader.exe
    │
    ├─[0] UPDATE MODE CHECK
    │     └── If --update-pid args present → Act as updater
    │           ├── Wait for old process to exit
    │           ├── Backup old.exe → old.exe.old
    │           ├── Move new.exe → old.exe location
    │           ├── Launch updated loader (CREATE_NEW_CONSOLE)
    │           └── Exit (return 0)
    │
    ├─[1] Security Initialization
    │     ├── VMProtect protection check (if Pack build)
    │     ├── VMProtect debugger / image-integrity checks
    │     ├── Early Telemetry (report tools before blocking)
    │     └── Anti-Attach patches (conditionally applied)
    │
    ├─[2] UI Init + UPDATE CHECK
    │     ├── setupConsole() + printBanner()
    │     ├── Updater::Check() → fetch manifest from server
    │     │     ├── Verify Ed25519 signature
    │     │     └── Compare current vs remote version
    │     ├── If update available (optional):
    │     │     └── Prompt "Press Y to update..."
    │     ├── If update required (blocked/mandatory):
    │     │     └── Auto-download + apply
    │     ├── Download to %TEMP%\cb_xxx\
    │     ├── Verify SHA256 + size
    │     ├── Spawn new loader with --update-pid args
    │     └── Exit (new loader takes over)
    │
    ├─[3] License Verification
    │     ├── HWID::Generate()
    │     ├── License::Verify(api.cobble.vip) → injection_secret
    │     └── On transport error (E12031 CF reset, etc.): retry api2.cobble.vip
    │           └── Pin working host; pass apiHost to DLL via InjectionData
    │
    ├─[4] Process Detection
    │     └── findLunarClient() → javaw.exe with "Lunar"
    │
    ├─[5] Create Injection Handshake
    │     ├── Create shared memory "Local\CobbleInjection"
    │     └── Write InjectionData { signature, secret, hwid, timestamp }
    │
    └─[6] SecureManualMap Injection
          ├── Decrypt payload (AES-256-CBC)
          ├── Map DLL into target process
          └── SecureZeroMemory(decryptedDll)

Cobble.dll (DllMain)
    └── CreateThread → cobble::context::initialize()
                           │
                           ├── LoaderVerify::verifyOrDie()  // Anti-LoadLibrary
                           │
                           ├── SessionValidator::ValidateInjection()  // NEW
                           │     ├── Read shared memory "Local\CobbleInjection"
                           │     ├── Verify signature == "COBBLE_INJ_V1"
                           │     ├── Check timestamp not expired (5 minutes)
                           │     └── If invalid → silent crash (memset + ExitProcess)
                           │
                           ├── c_logger::initialize()
                           ├── Java::Init()
                           ├── SDK_Init()
                           │
                           ├── Dynamic Module Allocation
                           │     ├── moduleManager = new ModuleManager()
                           │     └── ... (all modules)
                           │
                           ├── moduleManager->registerModule(*)
                           ├── ConfigManager::get().init()
                           ├── Overlay::Init()
                           │
                           └── Main loop (1ms tick)
                                 ├── Integrity::Verify() (periodic)
                                 ├── player.refresh()
                                 └── moduleManager->update()

cobble::context::shutdown()
    ├── ConfigManager::get().shutdown()
    ├── Disable all modules
    ├── Overlay::Shutdown()
    ├── Java::Kill()
    └── delete all module pointers
```

---

## Module System Architecture

### Ownership Model

```
cobble::context owns:
├── moduleManager (unique ownership)
├── autoClicker (unique ownership)
├── aimAssist (unique ownership)
└── ... all modules

ModuleManager references:
└── modules[] (non-owning pointers)
```

**Rules:**
- All modules are heap-allocated at runtime (no static globals)
- `cobble::context` is the sole owner and deleter
- `ModuleManager` only holds references for iteration
- Modules are self-contained and own their settings

### Module Lifecycle

```
new Module() → registerModule() → onEnable/onDisable → onUpdate (tick) → delete
```

### Base Class

```cpp
struct Module {
    const char* name;
    bool enabled = false;
    int keybind = 0;

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onUpdate() {}
    virtual void onRender() {}
    virtual void onRenderMenu() {}  // Self-rendering UI
};
```

---

## UI Architecture

### Design Philosophy

The UI follows a **decentralized rendering model**:

- Each module renders its own settings via `onRenderMenu()`
- No central "Settings" page knows about individual modules
- Menu framework only orchestrates navigation
- Widgets are consolidated and reusable

### Directory Structure

```
src/ui/
├── core/overlay.cpp       # OpenGL hooks, ImGui init
├── widgets/
│   ├── widgets.hpp/.cpp   # Consolidated widgets
│   ├── theme.hpp/.cpp     # Color scheme
│   └── icons.hpp          # FontAwesome definitions
├── menu/menu.cpp          # Tab navigation, dynamic routing
└── hud/
    ├── arraylist.cpp      # Active modules display
    └── notifications.cpp  # Toast system
```

### Rendering Flow

```
Overlay::Render()
    └── Menu::Render()
            ├── Render tabs (left nav)
            ├── Get current module via getModuleForSubtab()
            └── module->onRenderMenu()  // Module renders itself
```

### UI Hard Rules

1. **Never call module-specific code from menu.cpp** – only `onRenderMenu()`
2. **Use `ui::two_column()` for standard layouts**
3. **All widgets live in `widgets.cpp`** – no duplicates
4. **Scaling is automatic** – use `ui::s(value)` for sizes

---

## Threading Model

| Context | Thread | Safety Notes |
|---------|--------|--------------|
| Loader main | Main thread | Security init, injection |
| DLL main loop | JNI thread | Attached to JVM, 1ms tick |
| DLL heartbeat | Background | License validation every 2-3 min |
| Overlay render | OpenGL thread | ImGui calls only here |
| VisualTeamResolver scan | JNI thread | Runs every 20 ticks in main loop |

**Critical:**
- Never call JNI from the OpenGL thread
- Never call ImGui from the JNI thread
- DLL heartbeat does NOT run integrity checks (causes timing issues)

---

## Apollo API Integration

Cobble intercepts Lunar Client's Apollo API packets to detect team colors:

```
Player joins server
    │
    ├── Server sends S3FPacketCustomPayload (channel: "lunar:apollo")
    │
    └── ApolloPacketObserver::NativeOnReceive()
            ├── Parse protobuf override_nametag_message
            ├── Extract nametag color from adventure_component
            └── FriendsManager::setApolloNametagColor(uuid, color)
```

**Detected Colors:**
- `GREEN` / `DARK_GREEN` → Teammate (ignored by AimAssist)
- `RED` → Enemy
- Other colors → Neutral

---

## Security Model

### Loader Security
| Layer | Implementation |
|-------|----------------|
| Anti-Debug | **VMProtect** runtime debugger checks |
| Anti-Tamper | **VMProtect** image CRC validation |
| Anti-VM | **VMProtect** virtual-machine checks |
| Code Prot | **VMProtect Ultra** mutation + virtualization markers in `Cobble.dll` and `CobbleLoader.exe` |
| Injection | SecureManualMap (no LoadLibrary, no persistent RWX) |
| Payload Encryption | AES-256-CBC with server-provided key |
| Handshake | Writes `injection_secret` + `sync_token` to shared memory |

### DLL Security

| Layer | Implementation |
|-------|----------------|
| Loader Verify | Checks if in PEB module list (fails if LoadLibrary used) |
| Session Validator | Reads shared memory, validates injection_secret |
| Expiry Check | Secret must be < 5 minutes old |
| Silent Fail | Corrupts JNI cache + lets next JNI call crash |

### Phase 1: JNI Poisoning & Dead Man Switches (`util/state.hpp`)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    JNI POISONING SYSTEM                                  │
├─────────────────────────────────────────────────────────────────────────┤
│  Location: util/state.hpp                                                │
│                                                                          │
│  TRIGGERS:                                                               │
│  1. Heartbeat fails 3x → g_stateCorrupted = true                        │
│  2. Dead man switch: 5.3 min without validation → isStateStale() = true │
│  3. JNI cache checksum mismatch → tamper detected                       │
│                                                                          │
│  ACTIONS:                                                                │
│  • attemptRecovery() → sets JniCache::cls_* to nullptr                  │
│  • attemptRecoveryAlt() → swaps method IDs (wrong calls)                │
│  • Result: Next JNI call crashes with null dereference                  │
│                                                                          │
│  WHY IT WORKS:                                                           │
│  • Crash looks like memory corruption, not security                      │
│  • No "banned" or "security" strings in crash                           │
│  • Reverser can't find obvious trigger point                            │
└─────────────────────────────────────────────────────────────────────────┘
```

**Module Gradual Degradation:**
- `Module::shouldUpdate()` returns `false` if `sdk::internal::isOperational()` fails
- Modules silently stop updating (no crash, just stops working)
- Reverser sees: "features stopped" not "security triggered"

### Phase 2: Server Challenge Validation

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYNC TOKEN FLOW                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  VERIFY (/api/verify):                                                   │
│  1. Server generates 32-byte sync_token (64 hex chars)                  │
│  2. Stores in active_sessions table                                     │
│  3. Returns sync_token in signed response                               │
│                                                                          │
│  INJECTION:                                                              │
│  4. Loader copies sync_token to InjectionData shared memory             │
│  5. DLL reads sync_token on startup                                     │
│                                                                          │
│  HEARTBEAT (/api/heartbeat):                                            │
│  6. DLL calculates: sync_hash = SHA256(sync_token + hwid + time_bucket) │
│  7. Server validates hash (±1 minute tolerance for latency)             │
│  8. Server rotates: new_sync_token returned on success                  │
│  9. DLL updates g_SyncToken for next heartbeat                          │
│                                                                          │
│  ON FAILURE:                                                             │
│  • Server returns { ok: false, reason: "sync_error" }                   │
│  • DLL sets g_stateCorrupted = true → JNI poison                        │
│                                                                          │
│  TIME BUCKET FORMAT:                                                     │
│  • bucket = unix_timestamp / 60 (1 minute intervals)                    │
│  • Server checks bucket-1, bucket, bucket+1 for tolerance              │
│  • prev_sync_token kept for race condition handling                     │
└─────────────────────────────────────────────────────────────────────────┘
```

**Why Fake Loaders Fail:**
- Without real server handshake → no valid sync_token
- Hardcoded sync_token → fails after first rotation
- Wrong HWID in hash → sync_error
- Time bucket wrong → sync_error

### Integrity Check System

| System | Algorithm | Location | Frequency |
|--------|-----------|----------|-----------|
| `DllIntegrity::Check()` | SHA256 | Heartbeat thread | Every 2-3 min |

### HWID Generation

```
HWID = SHA256(CPU_ID + DISK_SERIAL + MAC_ADDRESS)
```

### Injection Secret Flow (Phase 1)

```
Server generates secret → Loader receives → Shared memory → DLL validates
     │                                                           │
     └── Stored in DB (5 min TTL) ─────── Can validate via /api/dll/validate
```

### Sync Token Flow (Phase 2)

```
Server generates sync_token → Loader passes via shared memory → DLL
     │                                                            │
     │                                     DLL calculates sync_hash
     │                                                 │
     └── Stored in active_sessions ←── Server validates on heartbeat
                                                       │
                                        Server rotates → new_sync_token
```

### License Server Protection

- Rate limiting (60 req/min per IP)
- HWID abuse detection (3+ unique HWIDs = ban)
- X-Forwarded-For trust with validation
- Admin endpoints require API key
- Injection secrets expire after 5 minutes
- Sync tokens rotate on every heartbeat

---

## Design Principles

| Principle | Implementation |
|-----------|----------------|
| **Separation of concerns** | JNI / SDK / Modules / UI are isolated |
| **No static initialization** | All modules heap-allocated at runtime |
| **Fail-fast security** | Exit immediately on any detection |
| **UI is declarative** | Modules describe their UI, framework renders |
| **Modules are self-contained** | Own their settings, keybinds, and UI |
| **Single source of truth** | One widget implementation, one theme, one render path |

---

## Build Commands

```powershell
# Build everything (DLL + Loader with hash verification)
.\build.ps1 -Release

# Build only DLL
msbuild Cobble.vcxproj /p:Configuration=Release /p:Platform=x64

# Build only Loader
msbuild CobbleLoader.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Deploy Server

The license server lives in its own repo (`cobble-server`). Deploy via Docker
Compose; see [cobble-server DEPLOY.md](https://github.com/SyraTechnology/cobble-server/blob/main/DEPLOY.md).
