#include "lunar_sender.hpp"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../jni/java.h"
#include <mutex>

namespace LunarSender {

    static jclass cls_LunarClientAPI = nullptr;
    static jobject obj_LunarClientAPI = nullptr;
    static jmethodID mid_sendPacket = nullptr;
    static jmethodID mid_getInstance = nullptr;

    static jclass cls_WaypointAdd = nullptr;
    static jmethodID mid_ctor_WaypointAdd = nullptr;

    static jclass cls_WaypointRemove = nullptr;
    static jmethodID mid_ctor_WaypointRemove = nullptr;
    
    
    
    
    
    bool Init() {
        if (!Java::Initialized) return false;
        JNIEnv* env = Java::Env();
        if (!env) return false;

        
        jclass localAPI = nullptr;
        if (Java::AssignClass(OBFUSCATE_STRING("com.lunarclient.bukkitapi.LunarClientAPI"), localAPI)) {
            cls_LunarClientAPI = (jclass)env->NewGlobalRef(localAPI);
            env->DeleteLocalRef(localAPI);

            mid_getInstance = env->GetStaticMethodID(cls_LunarClientAPI, OBFUSCATE_STRING("getInstance").c_str(), OBFUSCATE_STRING("()Lcom/lunarclient/bukkitapi/LunarClientAPI;").c_str());
            if (mid_getInstance) {
                 obj_LunarClientAPI = env->NewGlobalRef(env->CallStaticObjectMethod(cls_LunarClientAPI, mid_getInstance));
            }
            
            
            
            
            
            
            
            mid_sendPacket = env->GetMethodID(cls_LunarClientAPI, OBFUSCATE_STRING("sendPacket").c_str(), OBFUSCATE_STRING("(Lcom/lunarclient/bukkitapi/nethandler/LCPacket;)V").c_str());
            if (!mid_sendPacket) {
                env->ExceptionClear();
                
                mid_sendPacket = env->GetMethodID(cls_LunarClientAPI, OBFUSCATE_STRING("sendPacket").c_str(), OBFUSCATE_STRING("(Lcom/lunarclient/bukkitapi/nethandler/shared/LCPacket;)V").c_str());
            }
        }
        
        
        jclass localAdd = nullptr;
        if (Java::AssignClass(OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.client.LCPacketWaypointAdd"), localAdd)) {
            cls_WaypointAdd = (jclass)env->NewGlobalRef(localAdd);
            env->DeleteLocalRef(localAdd);
            
            
            
            
            
            
            
            
            
            mid_ctor_WaypointAdd = env->GetMethodID(cls_WaypointAdd, OBFUSCATE_STRING("<init>").c_str(), OBFUSCATE_STRING("(Ljava/lang/String;Ljava/lang/String;IIIIZZ)V").c_str());
        }

        
        jclass localRemove = nullptr;
        if (Java::AssignClass(OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.client.LCPacketWaypointRemove"), localRemove)) {
             cls_WaypointRemove = (jclass)env->NewGlobalRef(localRemove);
             env->DeleteLocalRef(localRemove);

             
             mid_ctor_WaypointRemove = env->GetMethodID(cls_WaypointRemove, OBFUSCATE_STRING("<init>").c_str(), OBFUSCATE_STRING("(Ljava/lang/String;Ljava/lang/String;)V").c_str());
        }
        
        return (obj_LunarClientAPI && mid_sendPacket && cls_WaypointAdd && cls_WaypointRemove);
    }

    void SendWaypoint(const std::string& name, int x, int y, int z, int color, bool forced) {
        JNIEnv* env = Java::Env();
        if (!env || !obj_LunarClientAPI || !mid_sendPacket || !cls_WaypointAdd || !mid_ctor_WaypointAdd) return;

        jstring jName = env->NewStringUTF(name.c_str());
        jstring jWorld = env->NewStringUTF(""); 
        
        
        
        jobject packet = env->NewObject(cls_WaypointAdd, mid_ctor_WaypointAdd, jName, jWorld, color, x, y, z, forced, true);
        
        if (packet) {
            env->CallVoidMethod(obj_LunarClientAPI, mid_sendPacket, packet);
            env->DeleteLocalRef(packet);
        }
        
        env->DeleteLocalRef(jName);
        env->DeleteLocalRef(jWorld);
    }

    void RemoveWaypoint(const std::string& name) {
        JNIEnv* env = Java::Env();
        if (!env || !obj_LunarClientAPI || !mid_sendPacket || !cls_WaypointRemove || !mid_ctor_WaypointRemove) return;
        
        jstring jName = env->NewStringUTF(name.c_str());
        jstring jWorld = env->NewStringUTF(""); 
        
        jobject packet = env->NewObject(cls_WaypointRemove, mid_ctor_WaypointRemove, jName, jWorld);
        
        if (packet) {
            env->CallVoidMethod(obj_LunarClientAPI, mid_sendPacket, packet);
            env->DeleteLocalRef(packet);
        }
        
        env->DeleteLocalRef(jName);
        env->DeleteLocalRef(jWorld);
    }
}
