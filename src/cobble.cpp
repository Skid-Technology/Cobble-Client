#include "cobble.hpp"
#include "config/config.hpp"
#include "module/combat/aimassist.h"
#include "module/combat/aimbow.h"
#include "module/combat/antibot.h"
#include "module/combat/autoclicker.h"
#include "module/combat/autorod.h"
#include "module/combat/blockhit.h"
#include "module/combat/backtrack.h"
#include "module/combat/hitselect.h"
#include "module/combat/nohitdelay.h"
#include "module/combat/opportunity.h"
#include "module/combat/criticals.h"
#include "module/combat/piercing.h"
#include "module/combat/wtap.h"

#include "module/combat/reach.h"
#include "module/combat/velocity.h"
#include "module/module_manager.h"
#include "module/movement/inventorymove.h"
#include "module/movement/nojumpdelay.h"
#include "module/movement/no_slow_down.h"
#include "module/movement/blink.h"
#include "module/movement/fakelag.h"
#include "module/movement/timer.h"
#include "module/movement/timerrange.h"
#include "module/movement/sprint.h"
#include "module/movement/faststop.h"
#include "module/utility/armorswitch.h"
#include "module/utility/autorefill.h"
#include "module/utility/altmanager.h"
#include "module/utility/autotool.h"
#include "module/utility/breaker.h"
#include "module/utility/bridgeassist.h"
#include "module/utility/clutch.h"
#include "module/utility/fastplace.h"
#include "module/utility/fastbreak.h"
#include "module/utility/noitemrelease.h"
#include "module/utility/inventorymanager.h"
#include "module/utility/teams.h"
#include "module/utility/throwpots.h"
#include "module/utility/autopot.h"

#include "module/visual/blockesp.h"
#include "module/visual/chams.h"
#include "module/visual/animations.h"
#include "module/visual/esp.h"
#include "module/visual/item_esp.h"
#include "module/visual/nametags.h"
#include "module/visual/anti_debuff.h"
#include "module/visual/tracers.h"
#include "module/visual/trajectories.h"
#include "module/visual/dropped_info.h"
#include "module/visual/target_hud.h"
#include "module/visual/pings.h"

#include "hooks/winsock_hooks.h"
#include "jni/backtrack/backtrack_gate.hpp"
#include "jni/packet/bytecode_patcher.hpp"
#include "jni/packet/packet_interceptor.hpp"
#include "sdk/network/lunar_packet_listener.hpp"
#include "sdk/network/lunar_sender.hpp"
#include "sdk/network/network_probe.hpp"
#include "sdk/player/render_data.h"
#include "sdk/player/visual_cache.h"
#include "security/dll_integrity.hpp"
#include "security/honeypot.hpp"
#include "security/loader_validator.hpp"  
#include "security/loader_verify.hpp"
#include "security/log_uploader.hpp"
#include "security/session_validator.hpp"
#include "security/vmprotect.h"
#include "ui/core/overlay.h"
#include "ui/hud/notifications.h"
#include "util/diagnostics.hpp"
#include "util/inject_trace.hpp"
#include "util/state.hpp"
#include "util/state_check.hpp"
#include "util/team/friends_manager.h"
#include "jni/apollo/apollo_handshake.hpp"

#include <algorithm>


volatile char COBBLE_SIGNATURE[17] = {};
static const bool _sig_init = []() {
    auto s = OBFUSCATE_STRING("COBBLE_LOADED_V1");
    memcpy((void*)COBBLE_SIGNATURE, s.c_str(), 16);
    return true;
}();


std::string g_Honeypot_HWID;
std::string g_Honeypot_Username;

static void invalidateSignature() {
    for (int i = 0; i < 16; i++) {
        COBBLE_SIGNATURE[i] = '\0';
    }
}

void cobble::context::initialize(HMODULE hModule) {
    COBBLE_DLL_ULTRA_START("Cobble.Context.Initialize");
    COBBLE_TRACE("ctx::initialize:enter");
    
    LoaderVerify::verifyOrDie(hModule);

    
    if (!SessionValidator::ValidateInjection()) {
        DWORD oldProtect;
        if (VirtualProtect(hModule, 0x1000, PAGE_READWRITE, &oldProtect)) {
            memset(hModule, 0xCC, 0x1000);
        }
        ExitProcess(0);
        return;
    }

    
    extern std::string g_Honeypot_HWID;
    extern std::string g_Honeypot_Username;
    g_Honeypot_HWID = SessionValidator::g_HWID;
    g_Honeypot_Username = SessionValidator::g_Username;

    if (!LoaderValidator::RunStartupValidation()) {
        sdk::internal::markCorrupted();
    }

    DllIntegrity::Initialize(hModule);
    SessionValidator::StartHeartbeatThread(hModule);
    COBBLE_DLL_ULTRA_END();

    
    COBBLE_TRACE("before c_logger::initialize");
    c_logger::initialize();
    COBBLE_TRACE("after c_logger::initialize");

    
    COBBLE_TRACE("before Diagnostics::installCrashHandler");
    Diagnostics::installCrashHandler();
    COBBLE_TRACE("after Diagnostics::installCrashHandler");

    log_raw("========================================");
    log_ok("Cobble injected! Build: %s", __DATE__);
    log_debug("Module Handle: 0x%p", hModule);

    (void)COBBLE_SIGNATURE[0];
    moduleHandle = hModule;

    log_raw("Initializing Java...");
    COBBLE_TRACE("before Java::Init");
    Java::Init();
    COBBLE_TRACE("after Java::Init");
    if (!Java::Initialized) {
        COBBLE_TRACE("Java::Initialized=false -> exit");
        log_err("Java initialization failed!");
        FreeLibraryAndExitThread(moduleHandle, 0);
        return;
    }
    log_ok("Java initialized");

    log_raw("Initializing SDK...");
    COBBLE_TRACE("before SDK_Init");
    SDK_Init();
    COBBLE_TRACE("after SDK_Init");
    sdk::internal::onCacheReady();  
    sdk::internal::wireGateD();      
    COBBLE_TRACE("after onCacheReady");
    log_ok("SDK initialized");

    
    log_raw("Running NetworkManager probe...");
    COBBLE_TRACE("before NetworkProbe::run");
    NetworkProbe::run();
    COBBLE_TRACE("after NetworkProbe::run");

    log_raw("Allocating modules...");
    
    COBBLE_TRACE("alloc:ModuleManager");
    moduleManager = new ModuleManager();
    COBBLE_TRACE("alloc:AutoClicker");
    autoClicker = new AutoClicker();
    autoRod = new AutoRod();
    blockHit = new BlockHit();
    COBBLE_TRACE("alloc:AimAssist");
    aimAssist = new AimAssist();
    COBBLE_TRACE("alloc:AimBow");
    aimBow = new AimBow();
    
    COBBLE_TRACE("alloc:AntiBot");
    antiBot = new AntiBot();
    COBBLE_TRACE("alloc:ArmorSwitch");
    armorSwitch = new ArmorSwitch();
    COBBLE_TRACE("alloc:AutoRefill");
    autoRefill = new AutoRefill();
    COBBLE_TRACE("alloc:ThrowPots");
    throwPots = new ThrowPots();
    COBBLE_TRACE("alloc:AutoPot");
    autoPot = new AutoPot();
    COBBLE_TRACE("alloc:Velocity");
    velocity = new Velocity();
    COBBLE_TRACE("alloc:Reach");
    reach = new Reach();
    COBBLE_TRACE("alloc:Piercing");
    piercing = new Piercing();

    COBBLE_TRACE("alloc:Backtrack");
    backtrack = new Backtrack();
    COBBLE_TRACE("alloc:HitSelect");
    hitSelect = new HitSelect();
    COBBLE_TRACE("alloc:NoHitDelay");
    noHitDelay = new NoHitDelay();
    COBBLE_TRACE("alloc:Criticals");
    criticals = new Criticals();
    COBBLE_TRACE("alloc:Opportunity");
    opportunity = new Opportunity();
    COBBLE_TRACE("alloc:WTap");
    wTap = new WTap();
    COBBLE_TRACE("alloc:NoSlowDown");
    noSlowDown = new NoSlowDown();
    COBBLE_TRACE("alloc:NoJumpDelay");
    noJumpDelay = new NoJumpDelay();
    COBBLE_TRACE("alloc:NoItemRelease");
    noItemRelease = new NoItemRelease();
    COBBLE_TRACE("alloc:BridgeAssist");
    bridgeAssist = new BridgeAssist();
    COBBLE_TRACE("alloc:Scaffold");
    scaffold = new Clutch(PlaceMode::Scaffold);
    COBBLE_TRACE("alloc:Clutch");
    clutch = new Clutch(PlaceMode::Clutch);
    COBBLE_TRACE("alloc:Breaker");
    breaker = new Breaker();
    COBBLE_TRACE("alloc:Blink");
    blink = new Blink();
    COBBLE_TRACE("alloc:FakeLag");
    fakeLag = new FakeLag();
    COBBLE_TRACE("alloc:FastPlace");
    fastPlace = new FastPlace();
    COBBLE_TRACE("alloc:FastBreak");
    fastBreak = new FastBreak();
    COBBLE_TRACE("alloc:TimerMod");
    timerModule = new TimerMod();
    COBBLE_TRACE("alloc:TimerRange");
    timerRange = new TimerRange();
    COBBLE_TRACE("alloc:Sprint");
    sprint = new Sprint();
    COBBLE_TRACE("alloc:InventoryMove");
    inventoryMove = new InventoryMove();
    COBBLE_TRACE("alloc:AutoTool");
    autoTool = new AutoTool();
    COBBLE_TRACE("alloc:InventoryManager");
    inventoryManager = new InventoryManager();
    COBBLE_TRACE("alloc:AltManager");
    altManager = new AltManager();
    COBBLE_TRACE("alloc:FastStop");
    fastStop = new FastStop();
    COBBLE_TRACE("alloc:NameTags");
    nameTags = new NameTags();
    COBBLE_TRACE("alloc:AntiDebuff");
    antiDebuff = new AntiDebuff();
    COBBLE_TRACE("alloc:ESP");
    esp = new ESP();
    COBBLE_TRACE("alloc:Chams");
    chams = new Chams();
    COBBLE_TRACE("alloc:Animations");
    animations = new Animations();
    COBBLE_TRACE("alloc:ItemESP");
    itemEsp = new ItemESP();
    COBBLE_TRACE("alloc:Tracers");
    tracers = new Tracers();
    COBBLE_TRACE("alloc:BlockESP");
    blockESP = new BlockESP();
    COBBLE_TRACE("alloc:Trajectories");
    trajectories = new Trajectories();
    COBBLE_TRACE("alloc:DroppedInfo");
    droppedInfo = new DroppedInfo();
    COBBLE_TRACE("alloc:TargetHUD");
    targetHud = new TargetHUD();
    COBBLE_TRACE("alloc:Pings");
    pingsModule = new Pings();
    COBBLE_TRACE("alloc:FriendsManager");
    friendsManager = new FriendsManager();
    
    
    COBBLE_TRACE("alloc:Teams");
    teamsModule = new Teams();
    log_ok("Modules allocated");
    COBBLE_TRACE("modules allocated");

    log_raw("Registering modules...");
    moduleManager->registerModule(autoClicker);
    moduleManager->registerModule(autoRod);
    moduleManager->registerModule(blockHit);
    moduleManager->registerModule(aimAssist);
    moduleManager->registerModule(aimBow);
    moduleManager->registerModule(antiBot);
    moduleManager->registerModule(armorSwitch);
    moduleManager->registerModule(autoRefill);
    moduleManager->registerModule(throwPots);
    moduleManager->registerModule(autoPot);
    moduleManager->registerModule(velocity);
    moduleManager->registerModule(reach);
    moduleManager->registerModule(piercing);

    moduleManager->registerModule(backtrack);
    moduleManager->registerModule(hitSelect);
    moduleManager->registerModule(noHitDelay);
    moduleManager->registerModule(criticals);
    moduleManager->registerModule(opportunity);
    moduleManager->registerModule(wTap);
    moduleManager->registerModule(noSlowDown);
    moduleManager->registerModule(noJumpDelay);
    moduleManager->registerModule(noItemRelease);
    moduleManager->registerModule(bridgeAssist);
    moduleManager->registerModule(scaffold);
    moduleManager->registerModule(clutch);
    moduleManager->registerModule(breaker);
    moduleManager->registerModule(blink);
    moduleManager->registerModule(fakeLag);
    moduleManager->registerModule(fastPlace);
    moduleManager->registerModule(fastBreak);
    moduleManager->registerModule(timerModule);
    moduleManager->registerModule(timerRange);
    moduleManager->registerModule(sprint);
    moduleManager->registerModule(inventoryMove);
    moduleManager->registerModule(autoTool);
    moduleManager->registerModule(inventoryManager);
    moduleManager->registerModule(teamsModule);
    moduleManager->registerModule(altManager);
    moduleManager->registerModule(fastStop);
    moduleManager->registerModule(nameTags);
    moduleManager->registerModule(antiDebuff);
    moduleManager->registerModule(esp);
    moduleManager->registerModule(chams);
    moduleManager->registerModule(animations);
    moduleManager->registerModule(itemEsp);
    moduleManager->registerModule(tracers);
    moduleManager->registerModule(blockESP);
    moduleManager->registerModule(trajectories);
    moduleManager->registerModule(droppedInfo);
    moduleManager->registerModule(targetHud);
    moduleManager->registerModule(pingsModule);
    log_ok("Modules registered");
    COBBLE_TRACE("modules registered");

    
    log_raw("Initializing Config...");
    COBBLE_TRACE("before ConfigManager::init");
    ConfigManager::get().init();
    COBBLE_TRACE("after ConfigManager::init");

    
    
    
    log_uploader::startPeriodic();

    log_raw("Initializing WSA hooks...");
    COBBLE_TRACE("before WinsockHooks::Init");
    if (WinsockHooks::Init()) {
        log_ok("WSA hooks initialized");
    } else {
        log_warn("WSA hooks failed - using PacketInterceptor only");
    }
    COBBLE_TRACE("after WinsockHooks::Init");

    
    log_raw("Initializing PacketInterceptor...");
    COBBLE_TRACE("before PacketInterceptor::Init");
    if (PacketInterceptor::Init()) {
        log_ok("PacketInterceptor initialized");
    } else {
        log_warn("PacketInterceptor failed");
    }
    COBBLE_TRACE("after PacketInterceptor::Init");

    
    log_raw("Initializing LunarPacketListener...");
    COBBLE_TRACE("before LunarPacketListener::Init");
    LunarPacketListener::Init();
    COBBLE_TRACE("after LunarPacketListener::Init");
    LunarSender::Init();
    COBBLE_TRACE("after LunarSender::Init");

    
    DWORD pid = GetCurrentProcessId();
    std::wstring mutexName = OBFUSCATE_WSTRING(L"Global\\Cobble_Instance_") + std::to_wstring(pid);
    hInstanceMutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        log_warn("Cobble instance mutex already exists! (Double Injection?)");
    } else {
        log_ok("Created instance mutex: %ls", mutexName.c_str());
    }

    const auto startTime = std::chrono::steady_clock::now();
    auto lastSessionRefresh = startTime;
    auto lastPlayerRefresh = startTime;
    auto lastModuleUpdate = startTime;
    auto lastRenderUpdate = startTime;
    auto lastVisualCacheUpdate = startTime;

    constexpr auto OVERLAY_INIT_DELAY = std::chrono::milliseconds(1000);
    constexpr auto NOTIFICATION_DELAY = std::chrono::milliseconds(1100);
    constexpr auto SESSION_REFRESH_INTERVAL = std::chrono::milliseconds(1000);
    constexpr auto PLAYER_REFRESH_INTERVAL = std::chrono::milliseconds(2);
    constexpr auto MODULE_UPDATE_INTERVAL = std::chrono::milliseconds(2);
    constexpr auto RENDERDATA_UPDATE_INTERVAL = std::chrono::milliseconds(4);
    constexpr auto VISUAL_CACHE_INTERVAL = std::chrono::milliseconds(4);
    constexpr auto MIN_SLEEP = std::chrono::milliseconds(1);
    bool overlayInitialized = false;
    bool notificationShown = false;

    is_running = true;
    
    Diagnostics::tick();
    Diagnostics::startWatchdog();
    log_ok("Main loop starting...");
    COBBLE_TRACE("main loop:enter");

    while (is_running) {
        
        Diagnostics::tick();

        auto now = std::chrono::steady_clock::now();

        if (!overlayInitialized && now - startTime >= OVERLAY_INIT_DELAY) {
            DIAG_SCOPE("Overlay::Init");
            COBBLE_TRACE("before Overlay::Init");
            Overlay::Init();
            COBBLE_TRACE("after Overlay::Init");
            overlayInitialized = true;
        }

        if (overlayInitialized && !notificationShown && now - startTime >= NOTIFICATION_DELAY) {
            NotificationManager::addSystem("Cobble loaded");
            notificationShown = true;
            COBBLE_TRACE("notification posted");
        }

        
        if (now - lastSessionRefresh >= SESSION_REFRESH_INTERVAL) {
            DIAG_SCOPE("SessionValidator::refresh");
            SessionValidator::refreshState();

            
            sdk::internal::syncTick();

            
            if (friendsManager) {
                friendsManager->checkWorldChange();
            }

            
            ApolloHandshake::Tick();

            
            
            
            
            if (SessionValidator::isStateCorrupted() || SessionValidator::isStateStale()) {
                sdk::internal::attemptRecovery();
            }

            internal::checkDevOverride();
            lastSessionRefresh = now;
        }

        if (now - lastPlayerRefresh >= PLAYER_REFRESH_INTERVAL) {
            DIAG_SCOPE("player.refresh");
            player.refresh();

            
            if (SDK_PollWorldChange() && moduleManager) {
                moduleManager->onWorldChange();
            }

            lastPlayerRefresh = now;
        }

        if (moduleManager && moduleManager->needsPlayerCache() &&
            now - lastVisualCacheUpdate >= VISUAL_CACHE_INTERVAL) {
            DIAG_SCOPE("VisualCache::Update");
            VisualCache::Update();
            lastVisualCacheUpdate = now;
        }

        if (now - lastModuleUpdate >= MODULE_UPDATE_INTERVAL) {
            DIAG_SCOPE("moduleManager.update");
            moduleManager->update();
            lastModuleUpdate = now;
        }

        
        if (now - lastRenderUpdate >= RENDERDATA_UPDATE_INTERVAL) {
            DIAG_SCOPE("RenderData::Update");
            RenderData::Update();
            
            RenderData::gameFps = mc.getGameFps();
            
            
            
            RenderData::guiCoversWorld = mc.isGuiCoveringWorld();
            lastRenderUpdate = now;
        }

        auto nextWake =
            std::min({lastPlayerRefresh + PLAYER_REFRESH_INTERVAL, lastModuleUpdate + MODULE_UPDATE_INTERVAL,
                      lastRenderUpdate + RENDERDATA_UPDATE_INTERVAL, lastVisualCacheUpdate + VISUAL_CACHE_INTERVAL,
                      lastSessionRefresh + SESSION_REFRESH_INTERVAL});
        auto sleepFor = nextWake - now;
        if (sleepFor > MIN_SLEEP) {
            std::this_thread::sleep_for(sleepFor);
        } else {
            std::this_thread::sleep_for(MIN_SLEEP);
        }
    }

    if (!shutdown()) {
        log_err("Shutdown could not safely detach native hooks; DLL will remain loaded");
        return;
    }
    FreeLibraryAndExitThread(moduleHandle, 0);
}

bool cobble::context::shutdown() {
    SessionValidator::StopHeartbeatThread();

    
    
    
    log_uploader::stop();
    log_uploader::uploadNowSync();

    invalidateSignature();

    
    
    

    
    ConfigManager::get().shutdown();

    
    
    if (moduleManager) {
        for (Module* module : moduleManager->getModules()) {
            if (!module || !module->enabled)
                continue;
            module->enabled = false;
            module->onDisable();
            log_debug("[MOD] %s: DISABLED (shutdown)", module->name);
        }
    }

    
    if (noSlowDown) {
        JNIEnv* noSlowEnv = Java::Env();
        if (noSlowEnv && noSlowDown->cachedPlayerClass) {
            noSlowEnv->DeleteGlobalRef(noSlowDown->cachedPlayerClass);
            noSlowDown->cachedPlayerClass = nullptr;
        }
    }

    
    
    
    
    
    
    BytecodePatcher::ApplyHideVanillaNames(Java::Env());

    if (backtrack) {
        
        const ULONGLONG drainDeadline = GetTickCount64() + 750;
        while (BacktrackGate::HasPending() && GetTickCount64() < drainDeadline)
            Sleep(5);

        if (BacktrackGate::HasPending()) {
            JNIEnv* drainEnv = Java::Env();
            if (!drainEnv || BacktrackGate::HasPendingForCurrentConnection(drainEnv)) {
                log_err("[Backtrack] Live-session packets remain after client-thread drain; keeping hooks and DLL loaded");
                return false;
            }

            
            
            BacktrackGate::Discard(drainEnv);
            if (BacktrackGate::HasPending()) {
                log_err("[Backtrack] Stale packet discard did not quiesce the gate; keeping DLL loaded");
                return false;
            }
            log_warn("[Backtrack] Discarded packets belonging to a closed connection during shutdown");
        }
    }

    if (!PacketInterceptor::Shutdown()) {
        log_err("Packet interceptor teardown was incomplete; aborting DLL unload");
        return false;
    }

    LunarPacketListener::Shutdown();

    if (hInstanceMutex) {
        ReleaseMutex(hInstanceMutex);
        CloseHandle(hInstanceMutex);
        hInstanceMutex = nullptr;
        log_raw("Released instance mutex");
    }

    WinsockHooks::Shutdown();

    Overlay::Shutdown();

    
    JNIEnv* env = Java::Env();
    if (env) {
        player.cleanup();
        JniCache::Cleanup();

        if (player.instance) {
            env->DeleteGlobalRef(player.instance);
            player.instance = nullptr;
        }
        if (mc.instance) {
            env->DeleteGlobalRef(mc.instance);
            mc.instance = nullptr;
        }
        if (mc.cls) {
            env->DeleteGlobalRef(mc.cls);
            mc.cls = nullptr;
        }
        mc.cleanup();
    }

    
    if (autoClicker) {
        delete autoClicker;
        autoClicker = nullptr;
    }
    if (autoRod) {
        delete autoRod;
        autoRod = nullptr;
    }
    if (blockHit) {
        delete blockHit;
        blockHit = nullptr;
    }
    if (aimAssist) {
        delete aimAssist;
        aimAssist = nullptr;
    }
    if (aimBow) {
        delete aimBow;
        aimBow = nullptr;
    }
    if (antiBot) {
        delete antiBot;
        antiBot = nullptr;
    }
    if (armorSwitch) {
        delete armorSwitch;
        armorSwitch = nullptr;
    }
    if (autoRefill) {
        delete autoRefill;
        autoRefill = nullptr;
    }
    if (throwPots) {
        delete throwPots;
        throwPots = nullptr;
    }
    if (autoPot) {
        delete autoPot;
        autoPot = nullptr;
    }
    if (velocity) {
        delete velocity;
        velocity = nullptr;
    }
    if (reach) {
        delete reach;
        reach = nullptr;
    }
    if (piercing) {
        delete piercing;
        piercing = nullptr;
    }

    if (backtrack) {
        delete backtrack;
        backtrack = nullptr;
    }
    if (hitSelect) {
        delete hitSelect;
        hitSelect = nullptr;
    }
    if (noHitDelay) {
        delete noHitDelay;
        noHitDelay = nullptr;
    }
    if (criticals) {
        delete criticals;
        criticals = nullptr;
    }
    if (opportunity) {
        delete opportunity;
        opportunity = nullptr;
    }
    if (wTap) {
        delete wTap;
        wTap = nullptr;
    }
    if (noSlowDown) {
        delete noSlowDown;
        noSlowDown = nullptr;
    }
    if (noJumpDelay) {
        delete noJumpDelay;
        noJumpDelay = nullptr;
    }
    if (noItemRelease) {
        delete noItemRelease;
        noItemRelease = nullptr;
    }
    if (bridgeAssist) {
        delete bridgeAssist;
        bridgeAssist = nullptr;
    }
    if (scaffold) {
        delete scaffold;
        scaffold = nullptr;
    }
    if (clutch) {
        delete clutch;
        clutch = nullptr;
    }
    if (breaker) {
        delete breaker;
        breaker = nullptr;
    }
    if (blink) {
        delete blink;
        blink = nullptr;
    }
    if (fakeLag) {
        delete fakeLag;
        fakeLag = nullptr;
    }
    if (fastPlace) {
        delete fastPlace;
        fastPlace = nullptr;
    }
    if (fastBreak) {
        delete fastBreak;
        fastBreak = nullptr;
    }
    if (timerModule) {
        delete timerModule;
        timerModule = nullptr;
    }
    if (timerRange) {
        delete timerRange;
        timerRange = nullptr;
    }
    if (sprint) {
        delete sprint;
        sprint = nullptr;
    }
    if (inventoryMove) {
        delete inventoryMove;
        inventoryMove = nullptr;
    }
    if (autoTool) {
        delete autoTool;
        autoTool = nullptr;
    }
    if (inventoryManager) {
        delete inventoryManager;
        inventoryManager = nullptr;
    }
    if (teamsModule) {
        delete teamsModule;
        teamsModule = nullptr;
    }
    if (altManager) {
        delete altManager;
        altManager = nullptr;
    }
    if (fastStop) {
        delete fastStop;
        fastStop = nullptr;
    }
    if (nameTags) {
        delete nameTags;
        nameTags = nullptr;
    }
    if (antiDebuff) {
        delete antiDebuff;
        antiDebuff = nullptr;
    }
    if (esp) {
        delete esp;
        esp = nullptr;
    }
    if (chams) {
        delete chams;
        chams = nullptr;
    }
    if (animations) {
        
        
        
        Animations* dead = animations;
        animations = nullptr;
        delete dead;
    }
    if (itemEsp) {
        delete itemEsp;
        itemEsp = nullptr;
    }
    if (tracers) {
        delete tracers;
        tracers = nullptr;
    }
    if (blockESP) {
        delete blockESP;
        blockESP = nullptr;
    }
    if (trajectories) {
        delete trajectories;
        trajectories = nullptr;
    }
    if (droppedInfo) {
        delete droppedInfo;
        droppedInfo = nullptr;
    }
    if (targetHud) {
        delete targetHud;
        targetHud = nullptr;
    }
    if (pingsModule) {
        delete pingsModule;
        pingsModule = nullptr;
    }
    if (friendsManager) {
        delete friendsManager;
        friendsManager = nullptr;
    }

    if (moduleManager) {
        delete moduleManager;
        moduleManager = nullptr;
    }

    Java::Kill();

    c_logger::shutdown();
    return true;
}
