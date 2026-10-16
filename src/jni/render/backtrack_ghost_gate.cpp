#include "backtrack_ghost_gate.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "../../module/combat/backtrack.h"
#include "../../sdk/core/minecraft.h"
#include "../../util/logger.hpp"
#include "../java.h"
#include "../../util/inject_trace.hpp"











#include "../../sdk/player/render_data.h"
#include <gl/GL.h>

namespace {

static const uint8_t kBacktrackGhostHookBytecode[] = {
    
    0xCA,
    0xFE,
    0xBA,
    0xBE,
    0x00,
    0x00,
    0x00,
    0x34,
    
    0x00,
    0x0C,
    
    0x01,
    0x00,
    0x23,
    'c',
    'o',
    'm',
    '/',
    'c',
    'o',
    'b',
    'b',
    'l',
    'e',
    '/',
    'a',
    'g',
    'e',
    'n',
    't',
    '/',
    'B',
    'a',
    'c',
    'k',
    't',
    'r',
    'a',
    'c',
    'k',
    'G',
    'h',
    'o',
    's',
    't',
    'H',
    'o',
    'o',
    'k',
    
    0x07,
    0x00,
    0x01,
    
    0x01,
    0x00,
    0x10,
    'j',
    'a',
    'v',
    'a',
    '/',
    'l',
    'a',
    'n',
    'g',
    '/',
    'O',
    'b',
    'j',
    'e',
    'c',
    't',
    
    0x07,
    0x00,
    0x03,
    
    0x01,
    0x00,
    0x06,
    '<',
    'i',
    'n',
    'i',
    't',
    '>',
    
    0x01,
    0x00,
    0x03,
    '(',
    ')',
    'V',
    
    0x01,
    0x00,
    0x04,
    'C',
    'o',
    'd',
    'e',
    
    0x01,
    0x00,
    0x0D,
    'o',
    'n',
    'R',
    'e',
    'n',
    'd',
    'e',
    'r',
    'W',
    'o',
    'r',
    'l',
    'd',
    
    0x01,
    0x00,
    0x15,
    '(',
    'L',
    'j',
    'a',
    'v',
    'a',
    '/',
    'l',
    'a',
    'n',
    'g',
    '/',
    'O',
    'b',
    'j',
    'e',
    'c',
    't',
    ';',
    ')',
    'V',
    
    0x0C,
    0x00,
    0x05,
    0x00,
    0x06,
    
    0x0A,
    0x00,
    0x04,
    0x00,
    0x0A,

    
    0x00,
    0x01,
    
    0x00,
    0x02,
    
    0x00,
    0x04,
    
    0x00,
    0x00,
    
    0x00,
    0x00,
    
    0x00,
    0x02,

    
    0x00,
    0x01,  
    0x00,
    0x05,  
    0x00,
    0x06,  
    0x00,
    0x01,  
    0x00,
    0x07,  
    0x00,
    0x00,
    0x00,
    0x11,  
    0x00,
    0x01,
    0x00,
    0x01,  
    0x00,
    0x00,
    0x00,
    0x05,  
    0x2A,  
    0xB7,
    0x00,
    0x0B,  
    0xB1,  
    0x00,
    0x00,  
    0x00,
    0x00,  

    
    
    0x01,
    0x09,
    0x00,
    0x08,  
    0x00,
    0x09,  
    0x00,
    0x00,  

    
    0x00,
    0x00,
};

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_shuttingDown{false};
static std::atomic<std::uint32_t> g_callbacksInFlight{0};
static jclass g_cls_BacktrackGhostHook = nullptr;

struct CallbackScope {
    CallbackScope() { g_callbacksInFlight.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel); }
};

}  

#include "../../module/visual/pings.h"
#include "../../module/visual/chams.h"

namespace BacktrackGhostGate {

void JNICALL NativeOnRenderWorld(JNIEnv* env, jclass, jobject viewEntity) {
    CallbackScope callback;
    (void)viewEntity;
    if (g_shuttingDown.load(std::memory_order_acquire) || !env)
        return;

    GLfloat proj[16];
    GLfloat mv[16];
    glGetFloatv(GL_PROJECTION_MATRIX, proj);
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);

    
    RenderData::projection.m00 = proj[0];
    RenderData::projection.m01 = proj[1];
    RenderData::projection.m02 = proj[2];
    RenderData::projection.m03 = proj[3];
    RenderData::projection.m10 = proj[4];
    RenderData::projection.m11 = proj[5];
    RenderData::projection.m12 = proj[6];
    RenderData::projection.m13 = proj[7];
    RenderData::projection.m20 = proj[8];
    RenderData::projection.m21 = proj[9];
    RenderData::projection.m22 = proj[10];
    RenderData::projection.m23 = proj[11];
    RenderData::projection.m30 = proj[12];
    RenderData::projection.m31 = proj[13];
    RenderData::projection.m32 = proj[14];
    RenderData::projection.m33 = proj[15];

    RenderData::modelView.m00 = mv[0];
    RenderData::modelView.m01 = mv[1];
    RenderData::modelView.m02 = mv[2];
    RenderData::modelView.m03 = mv[3];
    RenderData::modelView.m10 = mv[4];
    RenderData::modelView.m11 = mv[5];
    RenderData::modelView.m12 = mv[6];
    RenderData::modelView.m13 = mv[7];
    RenderData::modelView.m20 = mv[8];
    RenderData::modelView.m21 = mv[9];
    RenderData::modelView.m22 = mv[10];
    RenderData::modelView.m23 = mv[11];
    RenderData::modelView.m30 = mv[12];
    RenderData::modelView.m31 = mv[13];
    RenderData::modelView.m32 = mv[14];
    RenderData::modelView.m33 = mv[15];

    bool wasUpdated = RenderData::updated;
    RenderData::updated = RenderData::IsMatrixValid(RenderData::projection) && RenderData::IsMatrixValid(RenderData::modelView);

    if (RenderData::updated && !wasUpdated) {
        log_ok("RenderData: Successfully intercepted matrices via GL callback! Proj m00=%d, MV m00=%d",
               (int)(proj[0] * 1000.0f), (int)(mv[0] * 1000.0f));
    }

    
    
    
    if (pingsModule && pingsModule->enabled) {
        pingsModule->onRender3D();
    }

    
    
    if (chams && chams->enabled) {
        chams->onRender3D();
    }
}

bool Init(JNIEnv* env) {
    if (g_initialized.load())
        return true;
    if (!env)
        return false;

    g_shuttingDown.store(false, std::memory_order_release);

    jclass hook = nullptr;
    if (!Java::AssignClass("com.cobble.agent.BacktrackGhostHook", hook)) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    if (!hook) {
        if (Java::ClassLoader) {
            hook = env->DefineClass("com/cobble/agent/BacktrackGhostHook", Java::ClassLoader,
                                    reinterpret_cast<const jbyte*>(kBacktrackGhostHookBytecode),
                                    static_cast<jint>(sizeof(kBacktrackGhostHookBytecode)));
            if (env->ExceptionCheck()) {
                COBBLE_TRACE("BacktrackGhostGate::Init: DefineClass threw");
                env->ExceptionClear();
            }
        } else {
            COBBLE_TRACE("BacktrackGhostGate::Init: Java::ClassLoader is NULL");
        }
    }

    if (!hook) {
        log_warn("[BacktrackGhostGate] Failed to define BacktrackGhostHook class");
        return false;
    }

    JNINativeMethod methods[1];
    const char* mName = "onRenderWorld";
    const char* mDesc = "(Ljava/lang/Object;)V";
    methods[0].name = const_cast<char*>(mName);
    methods[0].signature = const_cast<char*>(mDesc);
    methods[0].fnPtr = reinterpret_cast<void*>(&NativeOnRenderWorld);

    if (env->RegisterNatives(hook, methods, 1) != 0) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        log_err("[BacktrackGhostGate] RegisterNatives failed");
        env->DeleteLocalRef(hook);
        return false;
    }

    g_cls_BacktrackGhostHook = (jclass)env->NewGlobalRef(hook);
    env->DeleteLocalRef(hook);
    g_initialized = true;
    log_ok("[BacktrackGhostGate] BacktrackGhostHook native bridge ready");
    return true;
}

bool Shutdown(JNIEnv* env, bool safeToUnregister) {
    g_shuttingDown.store(true, std::memory_order_release);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (g_callbacksInFlight.load(std::memory_order_acquire) != 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            log_err("[BacktrackGhostGate] Timed out waiting for native callbacks");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!env) {
        log_err("[BacktrackGhostGate] Cannot shut down without JNIEnv");
        return false;
    }

    if (g_cls_BacktrackGhostHook) {
        if (!safeToUnregister) {
            log_warn("[BacktrackGhostGate] Native unregister was not declared safe");
            return false;
        }

        const jint unregisterResult = env->UnregisterNatives(g_cls_BacktrackGhostHook);
        const bool unregisterException = env->ExceptionCheck() == JNI_TRUE;
        if (unregisterException)
            env->ExceptionClear();
        if (unregisterResult != JNI_OK || unregisterException) {
            log_err("[BacktrackGhostGate] Failed to unregister BacktrackGhostHook natives (result=%d, exception=%d)",
                    unregisterResult, unregisterException ? 1 : 0);
            return false;
        }
        log_ok("[BacktrackGhostGate] Unregistered BacktrackGhostHook natives");

        env->DeleteGlobalRef(g_cls_BacktrackGhostHook);
        g_cls_BacktrackGhostHook = nullptr;
    }

    g_initialized.store(false, std::memory_order_release);
    g_shuttingDown.store(false, std::memory_order_release);
    return true;
}

bool IsReady() {
    return g_initialized.load();
}

}  
