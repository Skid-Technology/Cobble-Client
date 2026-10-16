#include "packet_interceptor.hpp"

#include <atomic>
#include <cstring>
#include "../altmanager/altmanager_gate.hpp"
#include "../apollo/apollo_packet_observer.hpp"
#include "bytecode_patcher.hpp"
#include "outgoing_gate.hpp"
#include "../slowdown/slowdown_gate.hpp"
#include "../render/backtrack_ghost_gate.hpp"
#include "../backtrack/backtrack_gate.hpp"
#include "../../util/logger.hpp"

#include "../../util/inject_trace.hpp"

namespace PacketInterceptor {

static std::atomic<bool> initialized{false};

bool Init() {
    COBBLE_TRACE("PacketInterceptor::Init: enter");
    log_debug("[PacketInterceptor] Initializing...");

    if (!Java::Initialized) {
        COBBLE_TRACE("PacketInterceptor::Init: Java not initialized");
        return false;
    }

    JNIEnv* env = Java::Env();
    if (!env) {
        COBBLE_TRACE("PacketInterceptor::Init: Env is null");
        return false;
    }

    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    bool haveCaps = false;

    if (Java::tiEnv) {
        if (Java::tiEnv->GetCapabilities(&caps) == JVMTI_ERROR_NONE) {
            haveCaps = true;
        }
    }

    COBBLE_TRACE(haveCaps ? "PacketInterceptor::Init: Got JVMTI caps" : "PacketInterceptor::Init: GetCapabilities failed");

    BytecodePatcher::HookTargets targets;
    targets.outgoing = true;       
    targets.incoming = true;       
    targets.customPayload = true;  
    targets.teams = true;          
    targets.lunarNametags = true;  
    BytecodePatcher::SetTargets(targets);

    bool interceptReady = false;
    if (Java::tiEnv && haveCaps) {
        COBBLE_TRACE(caps.can_generate_all_class_hook_events ? "PacketInterceptor::Init: can_generate_all_class_hook_events=1" : "PacketInterceptor::Init: can_generate_all_class_hook_events=0");
        COBBLE_TRACE(caps.can_retransform_classes ? "PacketInterceptor::Init: can_retransform_classes=1" : "PacketInterceptor::Init: can_retransform_classes=0");

        if (caps.can_generate_all_class_hook_events && caps.can_retransform_classes) {
            bool regHooks = BytecodePatcher::RegisterHooks(env, &OutgoingPacketGate::NativeShouldSend,
                                                          &ApolloPacketObserver::NativeOnReceive);
            COBBLE_TRACE(regHooks ? "PacketInterceptor::Init: RegisterHooks ok" : "PacketInterceptor::Init: RegisterHooks failed");

            
            bool btInit = BacktrackGate::Init(env);
            COBBLE_TRACE(btInit ? "PacketInterceptor::Init: BacktrackGate::Init ok"
                                : "PacketInterceptor::Init: BacktrackGate::Init failed");

            bool sdInit = SlowDownGate::Init(env);
            COBBLE_TRACE(sdInit ? "PacketInterceptor::Init: SlowDownGate::Init ok" : "PacketInterceptor::Init: SlowDownGate::Init failed");

            
            
            
            
            bool bgInit = BacktrackGhostGate::Init(env);
            COBBLE_TRACE(bgInit ? "PacketInterceptor::Init: BacktrackGhostGate::Init ok"
                                : "PacketInterceptor::Init: BacktrackGhostGate::Init failed");

            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            targets.slowdown = sdInit;
            
            
            targets.renderGhost = bgInit;

            
            
            targets.incomingBacktrack = regHooks && btInit;
            targets.backtrack = false;
            BytecodePatcher::SetTargets(targets);

            bool instHook = BytecodePatcher::InstallBytecodeHook(env);
            COBBLE_TRACE(instHook ? "PacketInterceptor::Init: InstallBytecodeHook ok" : "PacketInterceptor::Init: InstallBytecodeHook failed");
            const bool backtrackOperational =
                btInit && instHook && BytecodePatcher::IsIncomingBacktrackReady();
            BacktrackGate::SetOperational(backtrackOperational);
            if (!backtrackOperational) {
                log_err("[Backtrack] Disabled: native bridge or required bytecode hooks are incomplete");
            }

            
            
            
            (void)sdInit;
            if (regHooks && instHook) {
                interceptReady = true;
            }
        }
    } else {
        COBBLE_TRACE("PacketInterceptor::Init: tiEnv is null or haveCaps is false");
    }

    if (!interceptReady) {
        COBBLE_TRACE("PacketInterceptor::Init: interceptReady = false");
        
    } else {
        COBBLE_TRACE("PacketInterceptor::Init: interceptReady = true");
    }

    initialized = true;
    return true;
}

bool Shutdown() {
    JNIEnv* env = Java::Env();
    if (!env) {
        initialized = false;
        return false;
    }

    BacktrackGate::SetOperational(false);
    const bool safeToUnregister = BytecodePatcher::Shutdown(env);
    if (!safeToUnregister) {
        
        
        log_err("[PacketInterceptor] Bytecode restoration failed; keeping native bridges loaded");
        return false;
    }

    if (!SlowDownGate::Shutdown(env, true)) {
        log_err("[PacketInterceptor] SlowDownGate teardown failed; keeping DLL loaded");
        return false;
    }
    if (!BacktrackGate::Shutdown(env, true)) {
        log_err("[PacketInterceptor] BacktrackGate teardown failed; keeping DLL loaded");
        return false;
    }
    if (!BacktrackGhostGate::Shutdown(env, true)) {
        log_err("[PacketInterceptor] BacktrackGhostGate teardown failed; keeping DLL loaded");
        return false;
    }

    AltManagerGate::Shutdown(env);
    initialized = false;
    return true;
}

bool IsReady() {
    return initialized.load();
}

}  
