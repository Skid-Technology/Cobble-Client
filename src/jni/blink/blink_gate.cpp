#include "blink_gate.hpp"

#include <cstring>
#include "../../module/movement/blink.h"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"

namespace {


static bool isMovementPacket(JNIEnv* env, jobject packet) {
    if (!packet)
        return false;

    jclass cls = env->GetObjectClass(packet);
    if (!cls) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    jclass clsCls = env->GetObjectClass(cls);
    if (!clsCls) {
        env->DeleteLocalRef(cls);
        return false;
    }

    jmethodID mid = env->GetMethodID(clsCls, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
    if (!mid) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    jstring jname = (jstring)env->CallObjectMethod(cls, mid);
    if (!jname) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    const char* cstr = env->GetStringUTFChars(jname, nullptr);
    bool isMovement = false;

    if (cstr) {
        
        
        
        
        
        isMovement = (strstr(cstr, OBFUSCATE_STRING("C03PacketPlayer").c_str()) != nullptr);
        env->ReleaseStringUTFChars(jname, cstr);
    }

    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(clsCls);
    env->DeleteLocalRef(cls);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return isMovement;
}

static bool isUseEntityPacket(JNIEnv* env, jobject packet) {
    if (!packet)
        return false;

    jclass cls = env->GetObjectClass(packet);
    if (!cls) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    jclass clsCls = env->GetObjectClass(cls);
    if (!clsCls) {
        env->DeleteLocalRef(cls);
        return false;
    }

    jmethodID mid = env->GetMethodID(clsCls, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
    if (!mid) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    jstring jname = (jstring)env->CallObjectMethod(cls, mid);
    if (!jname) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    const char* cstr = env->GetStringUTFChars(jname, nullptr);
    bool isUseEntity = false;

    if (cstr) {
        
        isUseEntity = (strstr(cstr, OBFUSCATE_STRING("C02PacketUseEntity").c_str()) != nullptr);
        env->ReleaseStringUTFChars(jname, cstr);
    }

    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(clsCls);
    env->DeleteLocalRef(cls);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return isUseEntity;
}

static bool isBowUsePacket(JNIEnv* env, jobject packet) {
    if (!packet)
        return false;

    jclass cls = env->GetObjectClass(packet);
    if (!cls) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    jclass clsCls = env->GetObjectClass(cls);
    if (!clsCls) {
        env->DeleteLocalRef(cls);
        return false;
    }

    jmethodID mid = env->GetMethodID(clsCls, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
    if (!mid) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    jstring jname = (jstring)env->CallObjectMethod(cls, mid);
    if (!jname) {
        env->DeleteLocalRef(clsCls);
        env->DeleteLocalRef(cls);
        return false;
    }

    const char* cstr = env->GetStringUTFChars(jname, nullptr);
    bool isBow = false;

    if (cstr) {
        
        isBow = (strstr(cstr, OBFUSCATE_STRING("C08PacketPlayerBlockPlacement").c_str()) != nullptr) ||
                (strstr(cstr, OBFUSCATE_STRING("C07PacketPlayerDigging").c_str()) != nullptr);
        env->ReleaseStringUTFChars(jname, cstr);
    }

    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(clsCls);
    env->DeleteLocalRef(cls);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return isBow;
}

}  

namespace BlinkGate {

bool ShouldSendPacket(JNIEnv* env, jobject packet) {
    
    if (!::blink || !::blink->enabled) {
        return true;
    }

    
    if (!::blink->shouldBlockPacket()) {
        return true;  
    }

    
    if (isUseEntityPacket(env, packet)) {
        log_warn("[Blink] Blocked UseEntity while blinking");
        return false;
    }

    
    if (isBowUsePacket(env, packet)) {
        log_warn("[Blink] Blocked bow use while blinking");
        return false;
    }

    
    if (::blink->settings.movementOnly) {
        if (!isMovementPacket(env, packet)) {
            return true;  
        }
    }

    
    ::blink->queuePacket(env, packet);
    log_debug("[Blink] Blocked outgoing packet");
    return false;
}

jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass, jobject packet) {
    return ShouldSendPacket(env, packet) ? JNI_TRUE : JNI_FALSE;
}

}  
