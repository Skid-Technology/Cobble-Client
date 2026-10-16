#include "noitemrelease_gate.hpp"

#include <cstring>
#include "../../module/utility/noitemrelease.h"
#include "../../util/aes_string.hpp"

namespace {

static bool shouldSendPacket(JNIEnv* env, jobject packet) {
    if (!packet)
        return true;

    if (!::noItemRelease || !::noItemRelease->enabled || !::noItemRelease->wasUsingItem) {
        return true;
    }

    
    
    jclass cls = env->GetObjectClass(packet);
    if (!cls) {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return true;
    }

    jclass clsCls = env->GetObjectClass(cls);
    if (clsCls) {
        jmethodID mid = env->GetMethodID(clsCls, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
        if (mid) {
            jstring jname = (jstring)env->CallObjectMethod(cls, mid);
            if (jname) {
                const char* cstr = env->GetStringUTFChars(jname, nullptr);
                if (cstr) {
                    if (strstr(cstr, OBFUSCATE_STRING("C07PacketPlayerDigging").c_str()) != nullptr) {
                        bool shouldBlock = false;

                        
                        jfieldID statusField = env->GetFieldID(
                            cls, OBFUSCATE_STRING("status").c_str(), OBFUSCATE_STRING("Lnet/minecraft/network/play/client/C07PacketPlayerDigging$Action;").c_str());
                        if (env->ExceptionCheck())
                            env->ExceptionClear();

                        if (statusField) {
                            jobject statusObj = env->GetObjectField(packet, statusField);
                            if (statusObj) {
                                jclass statusCls = env->GetObjectClass(statusObj);
                                if (statusCls) {
                                    jmethodID nameMethod = env->GetMethodID(statusCls, OBFUSCATE_STRING("name").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
                                    if (nameMethod) {
                                        jstring enumName = (jstring)env->CallObjectMethod(statusObj, nameMethod);
                                        if (enumName) {
                                            const char* statusStr = env->GetStringUTFChars(enumName, nullptr);
                                            if (statusStr) {
                                                shouldBlock = (strcmp(statusStr, OBFUSCATE_STRING("RELEASE_USE_ITEM").c_str()) == 0);
                                                env->ReleaseStringUTFChars(enumName, statusStr);
                                            }
                                            env->DeleteLocalRef(enumName);
                                        }
                                    }
                                    env->DeleteLocalRef(statusCls);
                                }
                                env->DeleteLocalRef(statusObj);
                            }
                        } else {
                            
                            jfieldID intStatusField = env->GetFieldID(cls, OBFUSCATE_STRING("field_149508_e").c_str(), OBFUSCATE_STRING("I").c_str());
                            if (env->ExceptionCheck())
                                env->ExceptionClear();

                            if (!intStatusField) {
                                intStatusField = env->GetFieldID(cls, OBFUSCATE_STRING("status").c_str(), OBFUSCATE_STRING("I").c_str());
                                if (env->ExceptionCheck())
                                    env->ExceptionClear();
                            }

                            if (intStatusField) {
                                jint statusValue = env->GetIntField(packet, intStatusField);
                                shouldBlock = (statusValue == 5);
                            }
                        }

                        env->ReleaseStringUTFChars(jname, cstr);
                        env->DeleteLocalRef(jname);
                        env->DeleteLocalRef(clsCls);
                        env->DeleteLocalRef(cls);

                        if (shouldBlock) {
                            return false;
                        }
                        goto skip_cleanup;
                    }
                    env->ReleaseStringUTFChars(jname, cstr);
                }
                env->DeleteLocalRef(jname);
            }
        }
        env->DeleteLocalRef(clsCls);
    }
skip_cleanup:
    env->DeleteLocalRef(cls);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return true;
}

}  

namespace NoItemReleaseGate {

jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass, jobject packet) {
    return shouldSendPacket(env, packet) ? JNI_TRUE : JNI_FALSE;
}

}  
