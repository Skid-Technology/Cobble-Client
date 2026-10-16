#include "lunar_packet_listener.hpp"
#include "../../jni/java.h"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"

#include <atomic>
#include <mutex>

namespace LunarPacketListener {


static std::atomic<bool> initialized{false};
static std::atomic<bool> debugLogging{true};


static jclass cls_LCPacketNametagsUpdate = nullptr;
static jclass cls_LCPacketNametagsOverride = nullptr;
static jclass cls_Map = nullptr;
static jclass cls_Set = nullptr;
static jclass cls_Iterator = nullptr;
static jclass cls_MapEntry = nullptr;
static jclass cls_UUID = nullptr;
static jclass cls_List = nullptr;


static jmethodID mid_Map_entrySet = nullptr;
static jmethodID mid_Set_iterator = nullptr;
static jmethodID mid_Iterator_hasNext = nullptr;
static jmethodID mid_Iterator_next = nullptr;
static jmethodID mid_MapEntry_getKey = nullptr;
static jmethodID mid_MapEntry_getValue = nullptr;
static jmethodID mid_UUID_toString = nullptr;
static jmethodID mid_List_size = nullptr;
static jmethodID mid_List_get = nullptr;


static jfieldID fid_NametagsUpdate_playersMap = nullptr;


static jfieldID fid_NametagsOverride_player = nullptr;
static jfieldID fid_NametagsOverride_nametag = nullptr;


static NametagUpdateCallback updateCallback = nullptr;
static NametagOverrideCallback overrideCallback = nullptr;
static std::mutex callbackMutex;


static jclass cls_Channel = nullptr;
static jclass cls_ChannelPipeline = nullptr;
static jclass cls_ChannelHandler = nullptr;
static jclass cls_ChannelHandlerContext = nullptr;


static jclass cls_NetworkManager = nullptr;
static jfieldID fid_NetworkManager_channel = nullptr;


static jmethodID mid_Channel_pipeline = nullptr;
static jmethodID mid_ChannelPipeline_names = nullptr;
static jmethodID mid_ChannelPipeline_get = nullptr;
static jmethodID mid_ChannelPipeline_addBefore = nullptr;
static jmethodID mid_ChannelPipeline_addLast = nullptr;


static jclass cls_LunarClientApi = nullptr;
static jclass cls_NametagsManager = nullptr;
static jobject obj_NametagsManager = nullptr;            
static jfieldID fid_NametagsManager_nametags = nullptr;  


static std::atomic<int> lastSeenNametagCount{0};

static bool cacheClasses(JNIEnv* env) {
    
    jclass localClass = nullptr;

    
    if (Java::AssignClass(OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.client.LCPacketNametagsUpdate"), localClass)) {
        cls_LCPacketNametagsUpdate = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);
        log_ok("[LunarPacketListener] Found LCPacketNametagsUpdate class");
    } else {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        log_warn("[LunarPacketListener] LCPacketNametagsUpdate class not found");
    }

    
    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.client.LCPacketNametagsOverride"), localClass)) {
        cls_LCPacketNametagsOverride = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);
        log_ok("[LunarPacketListener] Found LCPacketNametagsOverride class");
    } else {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        log_warn("[LunarPacketListener] LCPacketNametagsOverride class not found");
    }

    
    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.Map"), localClass)) {
        cls_Map = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_Map_entrySet = env->GetMethodID(cls_Map, OBFUSCATE_STRING("entrySet").c_str(), OBFUSCATE_STRING("()Ljava/util/Set;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.Set"), localClass)) {
        cls_Set = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_Set_iterator = env->GetMethodID(cls_Set, OBFUSCATE_STRING("iterator").c_str(), OBFUSCATE_STRING("()Ljava/util/Iterator;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.Iterator"), localClass)) {
        cls_Iterator = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_Iterator_hasNext = env->GetMethodID(cls_Iterator, OBFUSCATE_STRING("hasNext").c_str(), OBFUSCATE_STRING("()Z").c_str());
        mid_Iterator_next = env->GetMethodID(cls_Iterator, OBFUSCATE_STRING("next").c_str(), OBFUSCATE_STRING("()Ljava/lang/Object;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.Map$Entry"), localClass)) {
        cls_MapEntry = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_MapEntry_getKey = env->GetMethodID(cls_MapEntry, OBFUSCATE_STRING("getKey").c_str(), OBFUSCATE_STRING("()Ljava/lang/Object;").c_str());
        mid_MapEntry_getValue = env->GetMethodID(cls_MapEntry, OBFUSCATE_STRING("getValue").c_str(), OBFUSCATE_STRING("()Ljava/lang/Object;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.UUID"), localClass)) {
        cls_UUID = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_UUID_toString = env->GetMethodID(cls_UUID, OBFUSCATE_STRING("toString").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    localClass = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("java.util.List"), localClass)) {
        cls_List = (jclass)env->NewGlobalRef(localClass);
        env->DeleteLocalRef(localClass);

        mid_List_size = env->GetMethodID(cls_List, OBFUSCATE_STRING("size").c_str(), OBFUSCATE_STRING("()I").c_str());
        mid_List_get = env->GetMethodID(cls_List, OBFUSCATE_STRING("get").c_str(), OBFUSCATE_STRING("(I)Ljava/lang/Object;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    
    
    const std::string lunarHandlerNames[] = {OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.LCNetHandler"),
                                              OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.client.LCNetHandlerClient"),
                                              OBFUSCATE_STRING("com.lunarclient.bukkitapi.LunarClientAPI"),
                                              OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.LCPacketHandler"),
                                              OBFUSCATE_STRING("com.lunarclient.bukkitapi.nethandler.shared.LCPacketHandler")};

    for (const auto& handlerName : lunarHandlerNames) {
        localClass = nullptr;
        if (Java::AssignClass(handlerName, localClass)) {
            log_ok("[LunarPacketListener] Found Lunar handler: %s", handlerName.c_str());

            
            if (Java::tiEnv) {
                jint methodCount = 0;
                jmethodID* methods = nullptr;
                if (Java::tiEnv->GetClassMethods(localClass, &methodCount, &methods) == JVMTI_ERROR_NONE) {
                    
                    
                    for (int j = 0; j < methodCount && j < 20; j++) {
                        char* name = nullptr;
                        char* sig = nullptr;
                        Java::tiEnv->GetMethodName(methods[j], &name, &sig, nullptr);
                        if (name && sig) {
                            
                        }
                        if (name)
                            Java::tiEnv->Deallocate((unsigned char*)name);
                        if (sig)
                            Java::tiEnv->Deallocate((unsigned char*)sig);
                    }
                    Java::tiEnv->Deallocate((unsigned char*)methods);
                }
            }
            env->DeleteLocalRef(localClass);
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    return (cls_LCPacketNametagsUpdate != nullptr || cls_LCPacketNametagsOverride != nullptr);
}

static bool cacheFields(JNIEnv* env) {
    if (!cls_LCPacketNametagsUpdate && !cls_LCPacketNametagsOverride)
        return false;

    
    if (cls_LCPacketNametagsUpdate) {
        
        fid_NametagsUpdate_playersMap = env->GetFieldID(cls_LCPacketNametagsUpdate, OBFUSCATE_STRING("playersMap").c_str(), OBFUSCATE_STRING("Ljava/util/Map;").c_str());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            
            fid_NametagsUpdate_playersMap = env->GetFieldID(cls_LCPacketNametagsUpdate, OBFUSCATE_STRING("players").c_str(), OBFUSCATE_STRING("Ljava/util/Map;").c_str());
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }

        if (fid_NametagsUpdate_playersMap) {
            log_ok("[LunarPacketListener] Cached LCPacketNametagsUpdate.playersMap field");
        } else {
            
            if (Java::tiEnv) {
                jint fieldCount = 0;
                jfieldID* fields = nullptr;
                if (Java::tiEnv->GetClassFields(cls_LCPacketNametagsUpdate, &fieldCount, &fields) == JVMTI_ERROR_NONE) {
                    log_debug("[LunarPacketListener] LCPacketNametagsUpdate has %d fields:", fieldCount);
                    for (int i = 0; i < fieldCount; i++) {
                        char* name = nullptr;
                        char* sig = nullptr;
                        Java::tiEnv->GetFieldName(cls_LCPacketNametagsUpdate, fields[i], &name, &sig, nullptr);
                        if (name && sig) {
                            log_debug("  Field: %s %s", name, sig);
                        }
                        if (name)
                            Java::tiEnv->Deallocate((unsigned char*)name);
                        if (sig)
                            Java::tiEnv->Deallocate((unsigned char*)sig);
                    }
                    Java::tiEnv->Deallocate((unsigned char*)fields);
                }
            }
        }
    }

    
    if (cls_LCPacketNametagsOverride) {
        fid_NametagsOverride_player = env->GetFieldID(cls_LCPacketNametagsOverride, OBFUSCATE_STRING("player").c_str(), OBFUSCATE_STRING("Ljava/util/UUID;").c_str());
        if (env->ExceptionCheck())
            env->ExceptionClear();

        fid_NametagsOverride_nametag = env->GetFieldID(cls_LCPacketNametagsOverride, OBFUSCATE_STRING("tags").c_str(), OBFUSCATE_STRING("Ljava/util/List;").c_str());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            
            fid_NametagsOverride_nametag = env->GetFieldID(cls_LCPacketNametagsOverride, OBFUSCATE_STRING("nametag").c_str(), OBFUSCATE_STRING("Ljava/util/List;").c_str());
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }

        if (fid_NametagsOverride_player && fid_NametagsOverride_nametag) {
            log_ok("[LunarPacketListener] Cached LCPacketNametagsOverride fields");
        } else {
            
            if (Java::tiEnv) {
                jint fieldCount = 0;
                jfieldID* fields = nullptr;
                if (Java::tiEnv->GetClassFields(cls_LCPacketNametagsOverride, &fieldCount, &fields) ==
                    JVMTI_ERROR_NONE) {
                    log_debug("[LunarPacketListener] LCPacketNametagsOverride has %d fields:", fieldCount);
                    for (int i = 0; i < fieldCount; i++) {
                        char* name = nullptr;
                        char* sig = nullptr;
                        Java::tiEnv->GetFieldName(cls_LCPacketNametagsOverride, fields[i], &name, &sig, nullptr);
                        if (name && sig) {
                            log_debug("  Field: %s %s", name, sig);
                        }
                        if (name)
                            Java::tiEnv->Deallocate((unsigned char*)name);
                        if (sig)
                            Java::tiEnv->Deallocate((unsigned char*)sig);
                    }
                    Java::tiEnv->Deallocate((unsigned char*)fields);
                }
            }
        }
    }

    return true;
}

bool Init() {
    log_debug("[LunarPacketListener] Initializing...");

    if (!Java::Initialized) {
        log_err("[LunarPacketListener] Java not initialized");
        return false;
    }

    JNIEnv* env = Java::Env();
    if (!env) {
        log_err("[LunarPacketListener] No JNIEnv");
        return false;
    }

    if (!cacheClasses(env)) {
        log_warn("[LunarPacketListener] No Lunar packet classes found - listener disabled");
        return false;
    }

    cacheFields(env);

    initialized = true;
    log_ok("[LunarPacketListener] Initialized successfully");
    return true;
}

void Shutdown() {
    JNIEnv* env = Java::Env();
    if (env) {
        if (cls_LCPacketNametagsUpdate)
            env->DeleteGlobalRef(cls_LCPacketNametagsUpdate);
        if (cls_LCPacketNametagsOverride)
            env->DeleteGlobalRef(cls_LCPacketNametagsOverride);
        if (cls_Map)
            env->DeleteGlobalRef(cls_Map);
        if (cls_Set)
            env->DeleteGlobalRef(cls_Set);
        if (cls_Iterator)
            env->DeleteGlobalRef(cls_Iterator);
        if (cls_MapEntry)
            env->DeleteGlobalRef(cls_MapEntry);
        if (cls_UUID)
            env->DeleteGlobalRef(cls_UUID);
        if (cls_List)
            env->DeleteGlobalRef(cls_List);
    }

    cls_LCPacketNametagsUpdate = nullptr;
    cls_LCPacketNametagsOverride = nullptr;
    cls_Map = nullptr;
    cls_Set = nullptr;
    cls_Iterator = nullptr;
    cls_MapEntry = nullptr;
    cls_UUID = nullptr;
    cls_List = nullptr;

    initialized = false;
    log_debug("[LunarPacketListener] Shutdown complete");
}

bool IsReady() {
    return initialized.load();
}

void setDebugLogging(bool enabled) {
    debugLogging.store(enabled);
}

bool isDebugLogging() {
    return debugLogging.load();
}

void onNametagUpdate(NametagUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    updateCallback = callback;
}

void onNametagOverride(NametagOverrideCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    overrideCallback = callback;
}

void Tick() {
    
    
}


bool processIncomingPacket(JNIEnv* env, jobject packet) {
    if (!initialized.load() || !packet)
        return false;

    
    bool isNametagUpdate = cls_LCPacketNametagsUpdate && env->IsInstanceOf(packet, cls_LCPacketNametagsUpdate);
    bool isNametagOverride = cls_LCPacketNametagsOverride && env->IsInstanceOf(packet, cls_LCPacketNametagsOverride);

    if (!isNametagUpdate && !isNametagOverride)
        return false;

    if (debugLogging.load()) {
        if (isNametagUpdate) {
            log_ok("[LunarPacketListener] RECEIVED: LCPacketNametagsUpdate");

            
            if (fid_NametagsUpdate_playersMap && mid_Map_entrySet && mid_Set_iterator && mid_Iterator_hasNext &&
                mid_Iterator_next && mid_MapEntry_getKey && mid_MapEntry_getValue) {
                jobject playersMap = env->GetObjectField(packet, fid_NametagsUpdate_playersMap);
                if (playersMap) {
                    jobject entrySet = env->CallObjectMethod(playersMap, mid_Map_entrySet);
                    if (entrySet && !env->ExceptionCheck()) {
                        jobject iterator = env->CallObjectMethod(entrySet, mid_Set_iterator);
                        if (iterator && !env->ExceptionCheck()) {
                            while (env->CallBooleanMethod(iterator, mid_Iterator_hasNext) && !env->ExceptionCheck()) {
                                jobject entry = env->CallObjectMethod(iterator, mid_Iterator_next);
                                if (entry && !env->ExceptionCheck()) {
                                    jobject key = env->CallObjectMethod(entry, mid_MapEntry_getKey);
                                    jobject value = env->CallObjectMethod(entry, mid_MapEntry_getValue);

                                    if (key && value && !env->ExceptionCheck()) {
                                        
                                        const char* playerName = env->GetStringUTFChars((jstring)key, nullptr);
                                        jstring uuidStr = (jstring)env->CallObjectMethod(value, mid_UUID_toString);
                                        const char* uuid = uuidStr ? env->GetStringUTFChars(uuidStr, nullptr) : "?";

                                        log_debug("  Player: %s -> UUID: %s", playerName ? playerName : "?", uuid);

                                        if (playerName)
                                            env->ReleaseStringUTFChars((jstring)key, playerName);
                                        if (uuidStr && uuid)
                                            env->ReleaseStringUTFChars(uuidStr, uuid);
                                        if (uuidStr)
                                            env->DeleteLocalRef(uuidStr);
                                    }
                                    if (key)
                                        env->DeleteLocalRef(key);
                                    if (value)
                                        env->DeleteLocalRef(value);
                                    env->DeleteLocalRef(entry);
                                }
                            }
                            env->DeleteLocalRef(iterator);
                        }
                        env->DeleteLocalRef(entrySet);
                    }
                    env->DeleteLocalRef(playersMap);
                }
            }
        }

        if (isNametagOverride) {
            log_ok("[LunarPacketListener] RECEIVED: LCPacketNametagsOverride");

            
            if (fid_NametagsOverride_player && fid_NametagsOverride_nametag) {
                jobject playerUuid = env->GetObjectField(packet, fid_NametagsOverride_player);
                jobject tagsList = env->GetObjectField(packet, fid_NametagsOverride_nametag);

                if (playerUuid && mid_UUID_toString) {
                    jstring uuidStr = (jstring)env->CallObjectMethod(playerUuid, mid_UUID_toString);
                    if (uuidStr && !env->ExceptionCheck()) {
                        const char* uuid = env->GetStringUTFChars(uuidStr, nullptr);
                        log_debug("  Player UUID: %s", uuid ? uuid : "?");
                        if (uuid)
                            env->ReleaseStringUTFChars(uuidStr, uuid);
                        env->DeleteLocalRef(uuidStr);
                    }
                }

                if (tagsList && mid_List_size && mid_List_get) {
                    jint size = env->CallIntMethod(tagsList, mid_List_size);
                    log_debug("  Tags count: %d", size);

                    for (jint i = 0; i < size && !env->ExceptionCheck(); i++) {
                        jobject tag = env->CallObjectMethod(tagsList, mid_List_get, i);
                        if (tag && !env->ExceptionCheck()) {
                            
                            const char* tagStr = env->GetStringUTFChars((jstring)tag, nullptr);
                            log_debug("    Tag[%d]: %s", i, tagStr ? tagStr : "?");
                            if (tagStr)
                                env->ReleaseStringUTFChars((jstring)tag, tagStr);
                            env->DeleteLocalRef(tag);
                        }
                    }
                }

                if (playerUuid)
                    env->DeleteLocalRef(playerUuid);
                if (tagsList)
                    env->DeleteLocalRef(tagsList);
            }
        }
    }

    if (env->ExceptionCheck())
        env->ExceptionClear();
    return true;
}


jclass getNametagsUpdateClass() {
    return cls_LCPacketNametagsUpdate;
}
jclass getNametagsOverrideClass() {
    return cls_LCPacketNametagsOverride;
}

}  
