#include "altmanager_gate.hpp"

#include "../../module/utility/altmanager.h"
#include "../../sdk/core/minecraft.h"
#include "../../util/logger.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

namespace AltManagerGate {
namespace {

constexpr const char* kLoginStartClass =
    "net.minecraft.network.login.client.C00PacketLoginStart";
constexpr const char* kGameProfileClass = "com.mojang.authlib.GameProfile";
constexpr const char* kGameProfileSignature = "Lcom/mojang/authlib/GameProfile;";

std::mutex g_cacheMutex;
std::atomic<bool> g_cacheReady{false};
jclass g_loginStartClass = nullptr;
jclass g_gameProfileClass = nullptr;
jfieldID g_profileField = nullptr;
jmethodID g_profileConstructor = nullptr;
jmethodID g_getProfileId = nullptr;

void clearPendingException(JNIEnv* env) {
    if (env && env->ExceptionCheck())
        env->ExceptionClear();
}

jfieldID findProfileField(JNIEnv* env, jclass loginStartClass) {
    jfieldID field =
        env->GetFieldID(loginStartClass, "profile", kGameProfileSignature);
    if (field)
        return field;

    clearPendingException(env);
    field = env->GetFieldID(loginStartClass, "field_149305_a",
                            kGameProfileSignature);
    clearPendingException(env);
    return field;
}

void clearCache(JNIEnv* env) {
    g_cacheReady.store(false, std::memory_order_release);
    g_profileField = nullptr;
    g_profileConstructor = nullptr;
    g_getProfileId = nullptr;

    if (g_gameProfileClass) {
        env->DeleteGlobalRef(g_gameProfileClass);
        g_gameProfileClass = nullptr;
    }
    if (g_loginStartClass) {
        env->DeleteGlobalRef(g_loginStartClass);
        g_loginStartClass = nullptr;
    }
}

bool ensureCache(JNIEnv* env) {
    if (g_cacheReady.load(std::memory_order_acquire))
        return true;

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    if (g_cacheReady.load(std::memory_order_relaxed))
        return true;

    jclass loginStartLocal = nullptr;
    if (!Java::AssignClass(kLoginStartClass, loginStartLocal) ||
        !loginStartLocal) {
        clearPendingException(env);
        return false;
    }
    g_loginStartClass =
        static_cast<jclass>(env->NewGlobalRef(loginStartLocal));
    env->DeleteLocalRef(loginStartLocal);

    jclass gameProfileLocal = nullptr;
    if (!Java::AssignClass(kGameProfileClass, gameProfileLocal) ||
        !gameProfileLocal) {
        clearPendingException(env);
        clearCache(env);
        return false;
    }
    g_gameProfileClass =
        static_cast<jclass>(env->NewGlobalRef(gameProfileLocal));
    env->DeleteLocalRef(gameProfileLocal);

    if (!g_loginStartClass || !g_gameProfileClass) {
        clearCache(env);
        return false;
    }

    g_profileField = findProfileField(env, g_loginStartClass);
    g_profileConstructor = env->GetMethodID(
        g_gameProfileClass, "<init>",
        "(Ljava/util/UUID;Ljava/lang/String;)V");
    if (!g_profileConstructor)
        clearPendingException(env);

    g_getProfileId = env->GetMethodID(
        g_gameProfileClass, "getId", "()Ljava/util/UUID;");
    if (!g_getProfileId)
        clearPendingException(env);

    if (!g_profileField || !g_profileConstructor) {
        log_warn("[AltManager] Login packet mappings could not be resolved");
        clearCache(env);
        return false;
    }

    g_cacheReady.store(true, std::memory_order_release);
    return true;
}










constexpr const char* kSessionSignature = "Lnet/minecraft/util/Session;";

std::mutex g_sessionMutex;
jmethodID g_getSessionMid = nullptr;
jfieldID g_sessionField = nullptr;
bool g_sessionWarned = false;

bool ensureSessionAccess(JNIEnv* env) {
    if (g_getSessionMid || g_sessionField)
        return true;
    if (!mc.cls || !mc.instance)
        return false;

    g_getSessionMid = env->GetMethodID(mc.cls, "getSession", kSessionSignature);
    if (!g_getSessionMid) {
        clearPendingException(env);
        g_getSessionMid =
            env->GetMethodID(mc.cls, "func_110432_I", kSessionSignature);
    }
    if (!g_getSessionMid) {
        clearPendingException(env);
        g_sessionField = env->GetFieldID(mc.cls, "session", kSessionSignature);
    }
    if (!g_getSessionMid && !g_sessionField) {
        clearPendingException(env);
        g_sessionField =
            env->GetFieldID(mc.cls, "field_71449_j", kSessionSignature);
    }
    clearPendingException(env);

    if (!g_getSessionMid && !g_sessionField) {
        if (!g_sessionWarned) {
            g_sessionWarned = true;
            log_warn("[AltManager] Session mappings could not be resolved");
        }
        return false;
    }
    return true;
}


jobject acquireSession(JNIEnv* env) {
    if (!ensureSessionAccess(env))
        return nullptr;

    jobject session = g_getSessionMid
                          ? env->CallObjectMethod(mc.instance, g_getSessionMid)
                          : env->GetObjectField(mc.instance, g_sessionField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return session;
}



template <typename Fn>
bool forEachSessionString(JNIEnv* env, jobject session, Fn&& fn) {
    jclass sessionCls = env->GetObjectClass(session);
    jclass classCls = env->FindClass("java/lang/Class");
    jclass fieldCls = env->FindClass("java/lang/reflect/Field");
    clearPendingException(env);

    
    
    auto release = [&]() {
        if (fieldCls)
            env->DeleteLocalRef(fieldCls);
        if (classCls)
            env->DeleteLocalRef(classCls);
        if (sessionCls)
            env->DeleteLocalRef(sessionCls);
    };

    if (!sessionCls || !classCls || !fieldCls) {
        release();
        return false;
    }

    jmethodID midDeclaredFields = env->GetMethodID(
        classCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    jmethodID midClassName =
        env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
    jmethodID midFieldType =
        env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
    jmethodID midSetAccessible =
        env->GetMethodID(fieldCls, "setAccessible", "(Z)V");
    clearPendingException(env);
    if (!midDeclaredFields || !midClassName || !midFieldType ||
        !midSetAccessible) {
        release();
        return false;
    }

    jobjectArray fields = static_cast<jobjectArray>(
        env->CallObjectMethod(sessionCls, midDeclaredFields));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        release();
        return false;
    }
    if (!fields) {
        release();
        return false;
    }

    int stringIndex = 0;
    const jsize count = env->GetArrayLength(fields);
    for (jsize i = 0; i < count; ++i) {
        jobject field = env->GetObjectArrayElement(fields, i);
        if (!field) {
            clearPendingException(env);
            continue;
        }

        bool isString = false;
        jobject type = env->CallObjectMethod(field, midFieldType);
        if (type && !env->ExceptionCheck()) {
            jstring typeName =
                static_cast<jstring>(env->CallObjectMethod(type, midClassName));
            if (typeName && !env->ExceptionCheck()) {
                const char* raw = env->GetStringUTFChars(typeName, nullptr);
                isString = raw && strcmp(raw, "java.lang.String") == 0;
                if (raw)
                    env->ReleaseStringUTFChars(typeName, raw);
            }
            clearPendingException(env);
            if (typeName)
                env->DeleteLocalRef(typeName);
        }
        clearPendingException(env);
        if (type)
            env->DeleteLocalRef(type);

        if (isString) {
            env->CallVoidMethod(field, midSetAccessible, JNI_TRUE);
            clearPendingException(env);
            fn(stringIndex++, field);
        }

        env->DeleteLocalRef(field);
    }

    env->DeleteLocalRef(fields);
    release();
    return stringIndex > 0;
}

}  

void MaybeSpoofLoginPacket(JNIEnv* env, jobject packet) {
    if (!env || !packet || !altManager || !altManager->isActive())
        return;

    if (!ensureCache(env))
        return;

    const bool isLoginStart =
        env->IsInstanceOf(packet, g_loginStartClass) == JNI_TRUE;
    clearPendingException(env);
    if (!isLoginStart)
        return;

    jobject oldProfile = env->GetObjectField(packet, g_profileField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        oldProfile = nullptr;
    }

    jobject uuid = nullptr;
    if (oldProfile && g_getProfileId) {
        uuid = env->CallObjectMethod(oldProfile, g_getProfileId);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uuid = nullptr;
        }
    }

    jstring spoofedName = env->NewStringUTF(altManager->settings.spoofedName);
    jobject newProfile = spoofedName
                             ? env->NewObject(g_gameProfileClass,
                                              g_profileConstructor, uuid,
                                              spoofedName)
                             : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        newProfile = nullptr;
    }

    if (newProfile) {
        env->SetObjectField(packet, g_profileField, newProfile);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            log_warn("[AltManager] Failed to replace login GameProfile");
        } else {
            log_debug("[AltManager] Login username changed to '%s'",
                      altManager->settings.spoofedName);
        }
    }

    if (newProfile)
        env->DeleteLocalRef(newProfile);
    if (spoofedName)
        env->DeleteLocalRef(spoofedName);
    if (uuid)
        env->DeleteLocalRef(uuid);
    if (oldProfile)
        env->DeleteLocalRef(oldProfile);
}

bool SetSession(JNIEnv* env, const char* username, const char* uuid,
                const char* token, const char* sessionType,
                bool overwriteEmpty) {
    if (!env)
        return false;

    std::lock_guard<std::mutex> lock(g_sessionMutex);

    jobject session = acquireSession(env);
    if (!session)
        return false;

    jclass fieldCls = env->FindClass("java/lang/reflect/Field");
    jmethodID midSet =
        fieldCls ? env->GetMethodID(fieldCls, "set",
                                    "(Ljava/lang/Object;Ljava/lang/Object;)V")
                 : nullptr;
    clearPendingException(env);
    if (!midSet) {
        if (fieldCls)
            env->DeleteLocalRef(fieldCls);
        env->DeleteLocalRef(session);
        return false;
    }

    const char* values[4] = {username, uuid, token, sessionType};
    bool updated = false;

    forEachSessionString(env, session, [&](int index, jobject field) {
        if (index > 3)
            return;
        const char* value = values[index];
        if (!value)
            return;
        if (!overwriteEmpty && value[0] == '\0')
            return;

        jstring jvalue = env->NewStringUTF(value);
        if (!jvalue) {
            clearPendingException(env);
            return;
        }
        env->CallVoidMethod(field, midSet, session, jvalue);
        if (!env->ExceptionCheck() && index == 0)
            updated = true;
        clearPendingException(env);
        env->DeleteLocalRef(jvalue);
    });

    env->DeleteLocalRef(fieldCls);
    env->DeleteLocalRef(session);

    if (updated)
        log_debug("[AltManager] Session username set to '%s'",
                  username ? username : "");
    return updated;
}

bool GetSession(JNIEnv* env, std::string& username, std::string& uuid,
                std::string& token, std::string& sessionType) {
    if (!env)
        return false;

    std::lock_guard<std::mutex> lock(g_sessionMutex);

    jobject session = acquireSession(env);
    if (!session)
        return false;

    jclass fieldCls = env->FindClass("java/lang/reflect/Field");
    jmethodID midGet =
        fieldCls ? env->GetMethodID(fieldCls, "get",
                                    "(Ljava/lang/Object;)Ljava/lang/Object;")
                 : nullptr;
    clearPendingException(env);
    if (!midGet) {
        if (fieldCls)
            env->DeleteLocalRef(fieldCls);
        env->DeleteLocalRef(session);
        return false;
    }

    std::string* out[4] = {&username, &uuid, &token, &sessionType};
    bool ok = false;

    forEachSessionString(env, session, [&](int index, jobject field) {
        if (index > 3)
            return;

        jstring jvalue =
            static_cast<jstring>(env->CallObjectMethod(field, midGet, session));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }
        if (!jvalue)
            return;

        const char* raw = env->GetStringUTFChars(jvalue, nullptr);
        if (raw) {
            out[index]->assign(raw);
            env->ReleaseStringUTFChars(jvalue, raw);
            if (index == 0)
                ok = true;
        }
        env->DeleteLocalRef(jvalue);
    });

    env->DeleteLocalRef(fieldCls);
    env->DeleteLocalRef(session);
    return ok;
}

void Shutdown(JNIEnv* env) {
    if (!env)
        return;

    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        g_getSessionMid = nullptr;
        g_sessionField = nullptr;
        g_sessionWarned = false;
    }

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    clearCache(env);
}

}  
