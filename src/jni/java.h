#pragma once

#include <string>

#include "../../infra/ext/jni/jni.h"
#include "../../infra/ext/jni/jvmti.h"
#include "../sdk/version.h"







struct Java {
    static void Init();
    static void Kill();

    
    
    static JNIEnv* Env();

    static bool AssignClass(std::string name, jclass& out);
    static std::string NotchDescriptor(const std::string& descriptor);
    static void GetMinecraftVersion();

    
    static inline JavaVM* vm = nullptr;
    static inline jvmtiEnv* tiEnv = nullptr;
    static inline bool Initialized = false;
    static inline jobject ClassLoader = nullptr;
    
    
    static inline jmethodID MidLoadClass = nullptr;
    static inline jmethodID MidFindClass = nullptr;
    
    
    static inline bool RuntimeObfuscated = false;
    static inline MinecraftVersion Version = MinecraftVersion::UNDEFINED;
};
