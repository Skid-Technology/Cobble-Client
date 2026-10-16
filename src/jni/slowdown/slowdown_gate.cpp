#include "slowdown_gate.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "../../module/movement/no_slow_down.h"
#include "../../module/combat/backtrack.h"
#include "../../module/combat/autorod.h"
#include "../../module/combat/blockhit.h"
#include "../../module/combat/opportunity.h"
#include "../../module/utility/clutch.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/player/player.h"
#include "../../util/logger.hpp"
#include "../java.h"
#include "../../util/inject_trace.hpp"














namespace {



















static const uint8_t kSlowDownHookBytecode[] = {
    
    0xCA, 0xFE, 0xBA, 0xBE,
    0x00, 0x00, 0x00, 0x34,
    
    0x00, 0x0C,
    
    0x01, 0x00, 0x1D,
    'c','o','m','/','c','o','b','b','l','e','/','a','g','e','n','t','/','S','l','o','w','D','o','w','n','H','o','o','k',
    
    0x07, 0x00, 0x01,
    
    0x01, 0x00, 0x10,
    'j','a','v','a','/','l','a','n','g','/','O','b','j','e','c','t',
    
    0x07, 0x00, 0x03,
    
    0x01, 0x00, 0x06,
    '<','i','n','i','t','>',
    
    0x01, 0x00, 0x03,
    '(',')',  'V',
    
    0x01, 0x00, 0x04,
    'C','o','d','e',
    
    0x01, 0x00, 0x0A,
    'o','n','S','l','o','w','D','o','w','n',
    
    0x01, 0x00, 0x15,
    '(','L','j','a','v','a','/','l','a','n','g','/','O','b','j','e','c','t',';',')',  'V',
    
    0x0C, 0x00, 0x05, 0x00, 0x06,
    
    0x0A, 0x00, 0x04, 0x00, 0x0A,

    
    0x00, 0x01,
    
    0x00, 0x02,
    
    0x00, 0x04,
    
    0x00, 0x00,
    
    0x00, 0x00,
    
    0x00, 0x02,

    
    
    0x00, 0x01,
    
    0x00, 0x05,
    
    0x00, 0x06,
    
    0x00, 0x01,
    
    0x00, 0x07,
    
    0x00, 0x00, 0x00, 0x11,
    
    0x00, 0x01, 0x00, 0x01,
    
    0x00, 0x00, 0x00, 0x05,
    
    0x2A,        
    0xB7, 0x00, 0x0B, 
    0xB1,        
    
    0x00, 0x00,
    
    0x00, 0x00,

    
    
    0x01, 0x09,
    
    0x00, 0x08,
    
    0x00, 0x09,
    
    0x00, 0x00,

    
    0x00, 0x00,
};

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_shuttingDown{false};
static std::atomic<std::uint32_t> g_callbacksInFlight{0};
static jclass g_cls_SlowDownHook = nullptr;

struct CallbackScope {
    CallbackScope() { g_callbacksInFlight.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel); }
};

}  

namespace SlowDownGate {

void JNICALL NativeOnSlowDown(JNIEnv* env, jclass, jobject playerInstance) {
    CallbackScope callback;
    if (g_shuttingDown.load(std::memory_order_acquire) || !env || !playerInstance)
        return;

    const bool wantSlow = noSlowDown && noSlowDown->enabled;
    const bool wantBt = backtrack && backtrack->needsClientTick();
    const bool wantRod = autoRod && autoRod->needsClientTick();
    const bool wantBlock = blockHit && blockHit->needsClientTick();
    const bool wantClutch = clutch && clutch->needsClientTick();
    const bool wantScaffold = scaffold && scaffold->needsClientTick();
    const bool wantOpportunity = opportunity && opportunity->needsClientTick();
    if (!wantSlow && !wantBt && !wantRod && !wantBlock && !wantClutch &&
        !wantScaffold && !wantOpportunity)
        return;

    
    
    jobject localPlayer = nullptr;
    if (mc.instance && JniCache::fid_thePlayer) {
        localPlayer = env->GetObjectField(mc.instance, JniCache::fid_thePlayer);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            localPlayer = nullptr;
        }
    }
    const bool isLocalPlayer = localPlayer &&
                               env->IsSameObject(playerInstance, localPlayer) == JNI_TRUE;
    if (localPlayer)
        env->DeleteLocalRef(localPlayer);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (!isLocalPlayer)
        return;

    if (wantBt)
        backtrack->onClientTick(env);

    if (!wantSlow && !wantRod && !wantBlock && !wantClutch && !wantScaffold &&
        !wantOpportunity)
        return;

    std::lock_guard<std::recursive_mutex> lock(player.instanceMutex);
    if (!player.instance ||
        env->IsSameObject(playerInstance, player.instance) != JNI_TRUE) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return;
    }
    if (wantSlow)
        noSlowDown->applyMovement(env, playerInstance);
    
    
    
    if (wantRod)
        autoRod->onClientTick(env);
    
    
    
    
    if (wantClutch)
        clutch->onClientTick(env);
    if (wantScaffold)
        scaffold->onClientTick(env);
    
    
    
    if (wantOpportunity)
        opportunity->onClientTick(env);
    
    
    
    if (wantBlock)
        blockHit->onClientTick(env);
}

bool Init(JNIEnv* env) {
    if (g_initialized.load())
        return true;
    if (!env)
        return false;

    g_shuttingDown.store(false, std::memory_order_release);

    
    jclass hook = nullptr;
    if (!Java::AssignClass("com.cobble.agent.SlowDownHook", hook)) {
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    COBBLE_TRACE(hook ? "SlowDownGate::Init: AssignClass found existing class" : "SlowDownGate::Init: AssignClass did not find class");

    if (!hook) {
        if (Java::ClassLoader) {
            COBBLE_TRACE("SlowDownGate::Init: defining class using Java::ClassLoader");
            hook = env->DefineClass("com/cobble/agent/SlowDownHook",
                                    Java::ClassLoader,
                                    reinterpret_cast<const jbyte*>(kSlowDownHookBytecode),
                                    static_cast<jint>(sizeof(kSlowDownHookBytecode)));
            if (env->ExceptionCheck()) {
                COBBLE_TRACE("SlowDownGate::Init: DefineClass threw exception");
                jthrowable ex = env->ExceptionOccurred();
                if (ex) {
                    env->ExceptionClear();
                    jclass exClass = env->GetObjectClass(ex);
                    jclass classClass = env->FindClass("java/lang/Class");
                    jmethodID mid_getName = classClass ? env->GetMethodID(classClass, "getName", "()Ljava/lang/String;") : nullptr;
                    jstring nameStr = (exClass && mid_getName) ? (jstring)env->CallObjectMethod(exClass, mid_getName) : nullptr;
                    
                    jclass throwableClass = env->FindClass("java/lang/Throwable");
                    jmethodID mid_getMessage = throwableClass ? env->GetMethodID(throwableClass, "getMessage", "()Ljava/lang/String;") : nullptr;
                    jstring msgStr = (ex && mid_getMessage) ? (jstring)env->CallObjectMethod(ex, mid_getMessage) : nullptr;

                    const char* nameChars = nameStr ? env->GetStringUTFChars(nameStr, nullptr) : "UnknownException";
                    const char* msgChars = msgStr ? env->GetStringUTFChars(msgStr, nullptr) : "No message";
                    char traceBuf[1024];
                    wsprintfA(traceBuf, "SlowDownGate::Init: Exception: %s - Message: %s", nameChars, msgChars);
                    COBBLE_TRACE(traceBuf);

                    if (nameStr) env->ReleaseStringUTFChars(nameStr, nameChars);
                    if (msgStr) env->ReleaseStringUTFChars(msgStr, msgChars);
                }
            }
        } else {
            COBBLE_TRACE("SlowDownGate::Init: Java::ClassLoader is NULL");
        }
    }

    if (!hook) {
        COBBLE_TRACE("SlowDownGate::Init: Hook class is NULL (DefineClass failed)");
        log_warn("[SlowDownGate] Failed to define SlowDownHook class");
        return false;
    }

    JNINativeMethod methods[1];
    const char* mName = "onSlowDown";
    const char* mDesc = "(Ljava/lang/Object;)V";
    methods[0].name      = const_cast<char*>(mName);
    methods[0].signature = const_cast<char*>(mDesc);
    methods[0].fnPtr     = reinterpret_cast<void*>(&NativeOnSlowDown);

    if (env->RegisterNatives(hook, methods, 1) != 0) {
        COBBLE_TRACE("SlowDownGate::Init: RegisterNatives failed");
        if (env->ExceptionCheck()) env->ExceptionClear();
        log_err("[SlowDownGate] RegisterNatives failed");
        env->DeleteLocalRef(hook);
        return false;
    }

    g_cls_SlowDownHook = static_cast<jclass>(env->NewGlobalRef(hook));
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (!g_cls_SlowDownHook) {
        env->UnregisterNatives(hook);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        env->DeleteLocalRef(hook);
        return false;
    }
    env->DeleteLocalRef(hook);
    g_initialized.store(true, std::memory_order_release);
    COBBLE_TRACE("SlowDownGate::Init: Success!");
    log_ok("[SlowDownGate] SlowDownHook native bridge ready");
    return true;
}

bool Shutdown(JNIEnv* env, bool safeToUnregister) {
    g_shuttingDown.store(true, std::memory_order_release);

    
    
    
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (g_callbacksInFlight.load(std::memory_order_acquire) != 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            log_err("[SlowDownGate] Timed out waiting for native callbacks");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!env) {
        log_err("[SlowDownGate] Cannot shut down without JNIEnv");
        return false;
    }

    if (g_cls_SlowDownHook) {
        if (!safeToUnregister) {
            log_warn("[SlowDownGate] Native unregister was not declared safe");
            return false;
        }

        const jint unregisterResult = env->UnregisterNatives(g_cls_SlowDownHook);
        const bool unregisterException = env->ExceptionCheck() == JNI_TRUE;
        if (unregisterException)
            env->ExceptionClear();
        if (unregisterResult != JNI_OK || unregisterException) {
            log_err("[SlowDownGate] Failed to unregister SlowDownHook natives (result=%d, exception=%d)",
                    unregisterResult, unregisterException ? 1 : 0);
            return false;
        }
        log_ok("[SlowDownGate] Unregistered SlowDownHook natives");

        env->DeleteGlobalRef(g_cls_SlowDownHook);
        g_cls_SlowDownHook = nullptr;
    }

    g_initialized.store(false, std::memory_order_release);
    g_shuttingDown.store(false, std::memory_order_release);
    return true;
}

}  
