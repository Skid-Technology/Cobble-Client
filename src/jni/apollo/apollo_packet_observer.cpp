#include "apollo_packet_observer.hpp"
#include "apollo_handshake.hpp"

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../../sdk/network/lunar_packet_listener.hpp"
#include "../../sdk/player/effect_tracker.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../util/team/friends_manager.h"
#include "../../module/movement/blink.h"
#include "../../module/visual/nametags.h"


static std::atomic<int> g_typeLogBudget{0};

static std::atomic<int> g_forceNametagLogs{0};
static std::atomic<bool> g_debugEntityEffects{false};
static std::atomic<int> g_effectLogBudget{0};
static std::atomic<int> g_equipLogBudget{0};
static std::atomic<int> g_animLogBudget{0};


static std::mutex g_cleanNamesMutex;
static std::unordered_map<std::string, std::string> g_cleanNames;

namespace {


constexpr int kMaxVarintBytes = 10;       
constexpr int kMaxParseIterations = 500;  
constexpr int kMaxNestedDepth = 10;       


inline bool safeReadVarint(const uint8_t* data, int len, int& pos, int& outValue) {
    outValue = 0;
    int shift = 0;
    int bytesRead = 0;
    while (pos < len && bytesRead < kMaxVarintBytes) {
        uint8_t b = data[pos++];
        outValue |= (b & 0x7F) << shift;
        shift += 7;
        bytesRead++;
        if (!(b & 0x80))
            return true;
    }
    return false;  
}


inline bool safeSkipVarint(const uint8_t* data, int len, int& pos) {
    int bytesRead = 0;
    while (pos < len && bytesRead < kMaxVarintBytes) {
        if (!(data[pos++] & 0x80))
            return true;
        bytesRead++;
    }
    return false;  
}

inline bool shouldCheckWorldChange() {
    static std::atomic<int64_t> lastCheckMs{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t last = lastCheckMs.load();
    if (nowMs - last < 200) {
        return false;
    }
    lastCheckMs.store(nowMs);
    return true;
}

inline bool checkWorldChangeIfNeeded() {
    if (friendsManager && shouldCheckWorldChange()) {
        return friendsManager->checkWorldChange();
    }
    return false;
}

inline void logWorldChangeSkip(const char* msg) {
    static std::atomic<int64_t> lastLogMs{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t last = lastLogMs.load();
    if (nowMs - last < 500) {
        return;
    }
    lastLogMs.store(nowMs);
    log_debug("%s", msg);
}

inline bool isNameChar(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

inline std::string extractNameFromJsonLine(const std::string& line) {
    std::string best;
    std::string fallback;
    size_t i = 0;
    while (i < line.size()) {
        if (line[i] != '"') {
            ++i;
            continue;
        }
        size_t start = i + 1;
        size_t end = start;
        bool escaped = false;
        while (end < line.size()) {
            char ch = line[end];
            if (ch == '\\' && !escaped) {
                escaped = true;
                ++end;
                continue;
            }
            if (ch == '"' && !escaped) {
                break;
            }
            escaped = false;
            ++end;
        }
        if (end >= line.size()) {
            break;
        }
        std::string value = line.substr(start, end - start);
        size_t after = end + 1;
        while (after < line.size() && std::isspace(static_cast<unsigned char>(line[after])))
            ++after;
        if (after < line.size() && line[after] == ':') {
            i = end + 1;
            continue;  
        }

        if (!value.empty() && value[0] == '#' && value.size() >= 4) {
            i = end + 1;
            continue;  
        }

        std::string cleaned;
        cleaned.reserve(value.size());
        for (unsigned char c : value) {
            if (isNameChar(c))
                cleaned.push_back(static_cast<char>(c));
        }
        if (!cleaned.empty()) {
            if (cleaned.size() >= 2 && best.empty())
                best = cleaned;
            if (fallback.empty())
                fallback = cleaned;
        }
        i = end + 1;
    }
    return best.empty() ? fallback : best;
}

inline std::string extractNameFromJsonLines(const std::vector<std::string>& lines) {
    std::string fallback;
    for (const auto& line : lines) {
        std::string name = extractNameFromJsonLine(line);
        if (!name.empty()) {
            if (name.size() >= 2)
                return name;
            if (fallback.empty())
                fallback = name;
        }
    }
    return fallback;
}

inline bool isWorldChangePacket(JNIEnv* env, jobject packet) {
    static jclass cls_S01PacketJoinGame = nullptr;
    static jclass cls_S07PacketRespawn = nullptr;
    static bool triedInit = false;

    if (!triedInit) {
        triedInit = true;
        jclass local = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/play/server/S01PacketJoinGame").c_str());
        if (local) {
            cls_S01PacketJoinGame = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
        local = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/play/server/S07PacketRespawn").c_str());
        if (local) {
            cls_S07PacketRespawn = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    if (cls_S01PacketJoinGame && env->IsInstanceOf(packet, cls_S01PacketJoinGame)) {
        return true;
    }
    if (cls_S07PacketRespawn && env->IsInstanceOf(packet, cls_S07PacketRespawn)) {
        return true;
    }
    return false;
}

inline bool isServerPosLookPacket(JNIEnv* env, jobject packet) {
    static jclass cls_S08PacketPlayerPosLook = nullptr;
    static bool triedInit = false;

    if (!triedInit) {
        triedInit = true;
        jclass local = nullptr;
        if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.network.play.server.S08PacketPlayerPosLook"), local)) {
            cls_S08PacketPlayerPosLook = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    return cls_S08PacketPlayerPosLook && env->IsInstanceOf(packet, cls_S08PacketPlayerPosLook);
}

struct EffectAddFields {
    jclass cls = nullptr;
    jobject fInt0 = nullptr;   
    jobject fInt1 = nullptr;   
    jobject fByte0 = nullptr;  
    jobject fByte1 = nullptr;  
    bool ok = false;
};

struct EffectRemoveFields {
    jclass cls = nullptr;
    jobject fInt0 = nullptr;   
    jobject fByte0 = nullptr;  
    bool ok = false;
};

static EffectAddFields g_effAdd;
static EffectRemoveFields g_effRemove;


struct ReflectCache {
    jclass cls_Class = nullptr;
    jclass cls_Field = nullptr;
    jmethodID mid_getDeclaredFields = nullptr;  
    jmethodID mid_getName = nullptr;            
    jmethodID mid_fieldGetName = nullptr;       
    jmethodID mid_getType = nullptr;            
    jmethodID mid_setAccessible = nullptr;      
    jmethodID mid_getInt = nullptr;             
    jmethodID mid_getByte = nullptr;            
    jmethodID mid_getShort = nullptr;           
    jmethodID mid_getObj = nullptr;             
    bool ok = false;
};

static ReflectCache g_reflect;

static bool init_reflect(JNIEnv* env) {
    if (g_reflect.ok)
        return true;

    jclass localClass = env->FindClass(OBFUSCATE_STRING("java/lang/Class").c_str());
    jclass localField = env->FindClass(OBFUSCATE_STRING("java/lang/reflect/Field").c_str());
    if (!localClass || !localField) {
        env->ExceptionClear();
        return false;
    }

    g_reflect.cls_Class = (jclass)env->NewGlobalRef(localClass);
    g_reflect.cls_Field = (jclass)env->NewGlobalRef(localField);
    env->DeleteLocalRef(localClass);
    env->DeleteLocalRef(localField);

    if (!g_reflect.cls_Class || !g_reflect.cls_Field) {
        env->ExceptionClear();
        return false;
    }

    g_reflect.mid_getDeclaredFields = env->GetMethodID(g_reflect.cls_Class, OBFUSCATE_STRING("getDeclaredFields").c_str(), OBFUSCATE_STRING("()[Ljava/lang/reflect/Field;").c_str());
    g_reflect.mid_getName = env->GetMethodID(g_reflect.cls_Class, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
    g_reflect.mid_fieldGetName = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
    g_reflect.mid_getType = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("getType").c_str(), OBFUSCATE_STRING("()Ljava/lang/Class;").c_str());
    g_reflect.mid_setAccessible = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("setAccessible").c_str(), OBFUSCATE_STRING("(Z)V").c_str());
    g_reflect.mid_getInt = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("getInt").c_str(), OBFUSCATE_STRING("(Ljava/lang/Object;)I").c_str());
    g_reflect.mid_getByte = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("getByte").c_str(), OBFUSCATE_STRING("(Ljava/lang/Object;)B").c_str());
    g_reflect.mid_getShort = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("getShort").c_str(), OBFUSCATE_STRING("(Ljava/lang/Object;)S").c_str());
    g_reflect.mid_getObj = env->GetMethodID(g_reflect.cls_Field, OBFUSCATE_STRING("get").c_str(), OBFUSCATE_STRING("(Ljava/lang/Object;)Ljava/lang/Object;").c_str());

    env->ExceptionClear();

    g_reflect.ok = g_reflect.mid_getDeclaredFields && g_reflect.mid_getName && g_reflect.mid_getType &&
                   g_reflect.mid_fieldGetName && g_reflect.mid_setAccessible && g_reflect.mid_getInt && g_reflect.mid_getByte &&
                   g_reflect.mid_getShort && g_reflect.mid_getObj;
    return g_reflect.ok;
}

static std::string jstringToStd(JNIEnv* env, jstring s) {
    if (!s)
        return "";
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c)
        env->ReleaseStringUTFChars(s, c);
    return out;
}

static bool build_effect_add_cache(JNIEnv* env, jclass packetCls) {
    if (!init_reflect(env) || !packetCls)
        return false;

    jobjectArray fields = (jobjectArray)env->CallObjectMethod(packetCls, g_reflect.mid_getDeclaredFields);
    if (!fields || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    jobject intEntity = nullptr;
    jobject intDuration = nullptr;
    jobject byteEffect = nullptr;
    jobject byteAmp = nullptr;

    std::vector<jobject> ints;
    std::vector<jobject> bytes;

    int len = env->GetArrayLength(fields);
    ints.reserve(4);
    bytes.reserve(4);

    for (int i = 0; i < len; i++) {
        jobject f = env->GetObjectArrayElement(fields, i);
        if (!f)
            continue;

        env->CallVoidMethod(f, g_reflect.mid_setAccessible, JNI_TRUE);

        std::string fieldName;
        {
            jstring jf = (jstring)env->CallObjectMethod(f, g_reflect.mid_fieldGetName);
            fieldName = jstringToStd(env, jf);
            if (jf) env->DeleteLocalRef(jf);
            env->ExceptionClear();
        }

        jobject typeCls = env->CallObjectMethod(f, g_reflect.mid_getType);
        if (typeCls) {
            jstring jname = (jstring)env->CallObjectMethod(typeCls, g_reflect.mid_getName);
            std::string name = jstringToStd(env, jname);
            if (jname) env->DeleteLocalRef(jname);
            env->DeleteLocalRef(typeCls);

            if (name == "int") {
                if (!intEntity && (fieldName.find("entity") != std::string::npos || fieldName.find("Entity") != std::string::npos)) {
                    intEntity = env->NewGlobalRef(f);
                } else if (!intDuration && (fieldName.find("dur") != std::string::npos || fieldName.find("Dur") != std::string::npos)) {
                    intDuration = env->NewGlobalRef(f);
                } else {
                    ints.push_back(env->NewGlobalRef(f));
                }
            } else if (name == "byte") {
                if (!byteEffect && (fieldName.find("effect") != std::string::npos || fieldName.find("Effect") != std::string::npos)) {
                    byteEffect = env->NewGlobalRef(f);
                } else if (!byteAmp && (fieldName.find("amp") != std::string::npos || fieldName.find("Amp") != std::string::npos)) {
                    byteAmp = env->NewGlobalRef(f);
                } else {
                    bytes.push_back(env->NewGlobalRef(f));
                }
            }
        }

        env->DeleteLocalRef(f);
        env->ExceptionClear();
    }

    env->DeleteLocalRef(fields);

    if (!intEntity && ints.size() >= 1) {
        intEntity = ints[0];
        ints[0] = nullptr;
    }
    if (!intDuration && ints.size() >= 2) {
        
        for (auto& r : ints) {
            if (r) {
                intDuration = r;
                r = nullptr;
                break;
            }
        }
    }
    if (!byteEffect && bytes.size() >= 1) {
        byteEffect = bytes[0];
        bytes[0] = nullptr;
    }
    if (!byteAmp && bytes.size() >= 2) {
        for (auto& r : bytes) {
            if (r) {
                byteAmp = r;
                r = nullptr;
                break;
            }
        }
    }

    if (intEntity && intDuration && byteEffect && byteAmp) {
        g_effAdd.fInt0 = intEntity;
        g_effAdd.fInt1 = intDuration;
        g_effAdd.fByte0 = byteEffect;
        g_effAdd.fByte1 = byteAmp;
        g_effAdd.ok = true;

        
        for (auto& r : ints) {
            if (r) env->DeleteGlobalRef(r);
        }
        for (auto& r : bytes) {
            if (r) env->DeleteGlobalRef(r);
        }
        return true;
    }

    
    if (intEntity) env->DeleteGlobalRef(intEntity);
    if (intDuration) env->DeleteGlobalRef(intDuration);
    if (byteEffect) env->DeleteGlobalRef(byteEffect);
    if (byteAmp) env->DeleteGlobalRef(byteAmp);
    for (auto& r : ints) {
        if (r) env->DeleteGlobalRef(r);
    }
    for (auto& r : bytes) {
        if (r) env->DeleteGlobalRef(r);
    }
    for (auto* ref : {&g_effAdd.fInt0, &g_effAdd.fInt1, &g_effAdd.fByte0, &g_effAdd.fByte1}) {
        if (*ref) {
            env->DeleteGlobalRef(*ref);
            *ref = nullptr;
        }
    }
    g_effAdd.ok = false;
    return false;
}

static bool build_effect_remove_cache(JNIEnv* env, jclass packetCls) {
    if (!init_reflect(env) || !packetCls)
        return false;

    jobjectArray fields = (jobjectArray)env->CallObjectMethod(packetCls, g_reflect.mid_getDeclaredFields);
    if (!fields || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    jobject intEntity = nullptr;
    jobject byteEffect = nullptr;
    std::vector<jobject> ints;
    std::vector<jobject> bytes;

    int len = env->GetArrayLength(fields);
    for (int i = 0; i < len; i++) {
        jobject f = env->GetObjectArrayElement(fields, i);
        if (!f)
            continue;

        env->CallVoidMethod(f, g_reflect.mid_setAccessible, JNI_TRUE);

        std::string fieldName;
        {
            jstring jf = (jstring)env->CallObjectMethod(f, g_reflect.mid_fieldGetName);
            fieldName = jstringToStd(env, jf);
            if (jf) env->DeleteLocalRef(jf);
            env->ExceptionClear();
        }

        jobject typeCls = env->CallObjectMethod(f, g_reflect.mid_getType);
        if (typeCls) {
            jstring jname = (jstring)env->CallObjectMethod(typeCls, g_reflect.mid_getName);
            std::string name = jstringToStd(env, jname);
            if (jname) env->DeleteLocalRef(jname);
            env->DeleteLocalRef(typeCls);

            if (name == "int") {
                if (!intEntity && (fieldName.find("entity") != std::string::npos || fieldName.find("Entity") != std::string::npos)) {
                    intEntity = env->NewGlobalRef(f);
                } else {
                    ints.push_back(env->NewGlobalRef(f));
                }
            } else if (name == "byte") {
                if (!byteEffect && (fieldName.find("effect") != std::string::npos || fieldName.find("Effect") != std::string::npos)) {
                    byteEffect = env->NewGlobalRef(f);
                } else {
                    bytes.push_back(env->NewGlobalRef(f));
                }
            }
        }

        env->DeleteLocalRef(f);
        env->ExceptionClear();
    }

    env->DeleteLocalRef(fields);

    if (!intEntity && ints.size() >= 1) {
        intEntity = ints[0];
        ints[0] = nullptr;
    }
    if (!byteEffect && bytes.size() >= 1) {
        byteEffect = bytes[0];
        bytes[0] = nullptr;
    }

    if (intEntity && byteEffect) {
        g_effRemove.fInt0 = intEntity;
        g_effRemove.fByte0 = byteEffect;
        g_effRemove.ok = true;

        for (auto& r : ints) {
            if (r) env->DeleteGlobalRef(r);
        }
        for (auto& r : bytes) {
            if (r) env->DeleteGlobalRef(r);
        }
        return true;
    }

    for (auto& r : ints) {
        if (r) env->DeleteGlobalRef(r);
    }
    for (auto& r : bytes) {
        if (r) env->DeleteGlobalRef(r);
    }
    if (intEntity) env->DeleteGlobalRef(intEntity);
    if (byteEffect) env->DeleteGlobalRef(byteEffect);
    if (g_effRemove.fInt0) {
        env->DeleteGlobalRef(g_effRemove.fInt0);
        g_effRemove.fInt0 = nullptr;
    }
    if (g_effRemove.fByte0) {
        env->DeleteGlobalRef(g_effRemove.fByte0);
        g_effRemove.fByte0 = nullptr;
    }
    g_effRemove.ok = false;
    return false;
}

static bool try_handle_entity_effect_packets(JNIEnv* env, jobject packet) {
    static jclass clsAdd = nullptr;
    static jclass clsRemove = nullptr;
    static ULONGLONG lastTryAddMs = 0;
    static ULONGLONG lastTryRemoveMs = 0;

    
    if (!clsAdd) {
        ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastTryAddMs > 5000) {
            lastTryAddMs = nowMs;
            jclass local = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.network.play.server.S1DPacketEntityEffect"), local) && local) {
                clsAdd = (jclass)env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
            env->ExceptionClear();
        }
    }
    if (!clsRemove) {
        ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastTryRemoveMs > 5000) {
            lastTryRemoveMs = nowMs;
            jclass local = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.network.play.server.S1EPacketRemoveEntityEffect"), local) && local) {
                clsRemove = (jclass)env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
            env->ExceptionClear();
        }
    }

    if (clsAdd && env->IsInstanceOf(packet, clsAdd)) {
        if (!g_effAdd.ok) {
            build_effect_add_cache(env, clsAdd);
        }
        if (!g_effAdd.ok)
            return true;

        int entityId = env->CallIntMethod(g_effAdd.fInt0, g_reflect.mid_getInt, packet);
        int duration = env->CallIntMethod(g_effAdd.fInt1, g_reflect.mid_getInt, packet);
        int effectId = (int)env->CallByteMethod(g_effAdd.fByte0, g_reflect.mid_getByte, packet);
        int amp = (int)env->CallByteMethod(g_effAdd.fByte1, g_reflect.mid_getByte, packet);
        env->ExceptionClear();

        if (entityId > 0 && effectId > 0) {
            EffectTracker::OnEffectAdd(entityId, effectId, amp, duration);
            int budget = g_effectLogBudget.load();
            if (g_debugEntityEffects.load() || budget > 0) {
                if (budget > 0) g_effectLogBudget.store(budget - 1);
                log_debug("[ApolloPacketObserver][S1D] entityId=%d eff=%d amp=%d dur=%d", entityId, effectId, amp, duration);
            }
        }
        return true;
    }

    if (clsRemove && env->IsInstanceOf(packet, clsRemove)) {
        if (!g_effRemove.ok) {
            build_effect_remove_cache(env, clsRemove);
        }
        if (!g_effRemove.ok)
            return true;

        int entityId = env->CallIntMethod(g_effRemove.fInt0, g_reflect.mid_getInt, packet);
        int effectId = (int)env->CallByteMethod(g_effRemove.fByte0, g_reflect.mid_getByte, packet);
        env->ExceptionClear();

        if (entityId > 0 && effectId > 0) {
            EffectTracker::OnEffectRemove(entityId, effectId);
            int budget = g_effectLogBudget.load();
            if (g_debugEntityEffects.load() || budget > 0) {
                if (budget > 0) g_effectLogBudget.store(budget - 1);
                log_debug("[ApolloPacketObserver][S1E] entityId=%d eff=%d", entityId, effectId);
            }
        }
        return true;
    }

    return false;
}





struct EquipmentFields {
    jclass cls = nullptr;
    jobject fEntityId = nullptr;  
    jobject fSlot = nullptr;      
    bool slotIsShort = false;
    jobject fStack = nullptr;     
    bool ok = false;
};

struct AnimationFields {
    jclass cls = nullptr;
    jobject fEntityId = nullptr;  
    jobject fTypeInt = nullptr;   
    jobject fTypeByte = nullptr;  
    bool ok = false;
};

static EquipmentFields g_equip;
static AnimationFields g_anim;
static std::mutex g_heldMutex;
static std::unordered_map<int, jobject> g_heldByEntity;  

static void clear_held_items(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(g_heldMutex);
    for (auto& kv : g_heldByEntity) {
        if (kv.second) {
            env->DeleteGlobalRef(kv.second);
        }
    }
    g_heldByEntity.clear();
}

static void set_held_item(JNIEnv* env, int entityId, jobject stackLocal) {
    if (entityId <= 0)
        return;

    jobject newGlobal = nullptr;
    if (stackLocal) {
        newGlobal = env->NewGlobalRef(stackLocal);
    }

    std::lock_guard<std::mutex> lock(g_heldMutex);
    auto it = g_heldByEntity.find(entityId);
    if (it != g_heldByEntity.end()) {
        if (it->second) {
            env->DeleteGlobalRef(it->second);
        }
        if (newGlobal) {
            it->second = newGlobal;
        } else {
            g_heldByEntity.erase(it);
        }
    } else if (newGlobal) {
        g_heldByEntity.emplace(entityId, newGlobal);
    }
}

static jobject get_held_item_local(JNIEnv* env, int entityId) {
    std::lock_guard<std::mutex> lock(g_heldMutex);
    auto it = g_heldByEntity.find(entityId);
    if (it == g_heldByEntity.end() || !it->second)
        return nullptr;
    return env->NewLocalRef(it->second);
}

static bool build_equipment_cache(JNIEnv* env, jclass packetCls) {
    if (!init_reflect(env) || !packetCls)
        return false;

    jobjectArray fields = (jobjectArray)env->CallObjectMethod(packetCls, g_reflect.mid_getDeclaredFields);
    if (!fields || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    jobject intEntity = nullptr;
    jobject intSlot = nullptr;
    jobject shortSlot = nullptr;
    jobject stackField = nullptr;

    std::vector<jobject> ints;
    int len = env->GetArrayLength(fields);
    for (int i = 0; i < len; i++) {
        jobject f = env->GetObjectArrayElement(fields, i);
        if (!f)
            continue;

        env->CallVoidMethod(f, g_reflect.mid_setAccessible, JNI_TRUE);

        std::string fieldName;
        {
            jstring jf = (jstring)env->CallObjectMethod(f, g_reflect.mid_fieldGetName);
            fieldName = jstringToStd(env, jf);
            if (jf) env->DeleteLocalRef(jf);
            env->ExceptionClear();
        }

        jobject typeCls = env->CallObjectMethod(f, g_reflect.mid_getType);
        if (typeCls) {
            jstring jname = (jstring)env->CallObjectMethod(typeCls, g_reflect.mid_getName);
            std::string typeName = jstringToStd(env, jname);
            if (jname) env->DeleteLocalRef(jname);
            env->DeleteLocalRef(typeCls);

            if (typeName == "int") {
                if (!intEntity && (fieldName.find("entity") != std::string::npos || fieldName.find("Entity") != std::string::npos)) {
                    intEntity = env->NewGlobalRef(f);
                } else if (!intSlot && (fieldName.find("slot") != std::string::npos || fieldName.find("Slot") != std::string::npos ||
                                        fieldName.find("equip") != std::string::npos || fieldName.find("Equip") != std::string::npos)) {
                    intSlot = env->NewGlobalRef(f);
                } else {
                    ints.push_back(env->NewGlobalRef(f));
                }
            } else if (typeName == "short") {
                if (!shortSlot && (fieldName.find("slot") != std::string::npos || fieldName.find("Slot") != std::string::npos ||
                                   fieldName.find("equip") != std::string::npos || fieldName.find("Equip") != std::string::npos)) {
                    shortSlot = env->NewGlobalRef(f);
                }
            } else if (typeName.find("ItemStack") != std::string::npos) {
                if (!stackField) {
                    stackField = env->NewGlobalRef(f);
                }
            }
        }

        env->DeleteLocalRef(f);
        env->ExceptionClear();
    }

    env->DeleteLocalRef(fields);

    if (!intEntity && !ints.empty()) {
        intEntity = ints[0];
        ints[0] = nullptr;
    }
    if (!intSlot) {
        for (auto& r : ints) {
            if (r) {
                intSlot = r;
                r = nullptr;
                break;
            }
        }
    }

    for (auto& r : ints) {
        if (r) env->DeleteGlobalRef(r);
    }

    if (intEntity && (intSlot || shortSlot) && stackField) {
        g_equip.fEntityId = intEntity;
        if (intSlot) {
            g_equip.fSlot = intSlot;
            g_equip.slotIsShort = false;
        } else {
            g_equip.fSlot = shortSlot;
            g_equip.slotIsShort = true;
        }
        g_equip.fStack = stackField;
        g_equip.ok = true;
        return true;
    }

    if (intEntity) env->DeleteGlobalRef(intEntity);
    if (intSlot) env->DeleteGlobalRef(intSlot);
    if (shortSlot) env->DeleteGlobalRef(shortSlot);
    if (stackField) env->DeleteGlobalRef(stackField);
    g_equip.ok = false;
    return false;
}

static bool build_animation_cache(JNIEnv* env, jclass packetCls) {
    if (!init_reflect(env) || !packetCls)
        return false;

    jobjectArray fields = (jobjectArray)env->CallObjectMethod(packetCls, g_reflect.mid_getDeclaredFields);
    if (!fields || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    jobject intEntity = nullptr;
    jobject intType = nullptr;
    jobject byteType = nullptr;

    std::vector<jobject> ints;
    std::vector<jobject> bytes;

    int len = env->GetArrayLength(fields);
    for (int i = 0; i < len; i++) {
        jobject f = env->GetObjectArrayElement(fields, i);
        if (!f)
            continue;

        env->CallVoidMethod(f, g_reflect.mid_setAccessible, JNI_TRUE);

        std::string fieldName;
        {
            jstring jf = (jstring)env->CallObjectMethod(f, g_reflect.mid_fieldGetName);
            fieldName = jstringToStd(env, jf);
            if (jf) env->DeleteLocalRef(jf);
            env->ExceptionClear();
        }

        jobject typeCls = env->CallObjectMethod(f, g_reflect.mid_getType);
        if (typeCls) {
            jstring jname = (jstring)env->CallObjectMethod(typeCls, g_reflect.mid_getName);
            std::string typeName = jstringToStd(env, jname);
            if (jname) env->DeleteLocalRef(jname);
            env->DeleteLocalRef(typeCls);

            if (typeName == "int") {
                if (!intEntity && (fieldName.find("entity") != std::string::npos || fieldName.find("Entity") != std::string::npos)) {
                    intEntity = env->NewGlobalRef(f);
                } else if (!intType && (fieldName.find("type") != std::string::npos || fieldName.find("Type") != std::string::npos ||
                                        fieldName.find("anim") != std::string::npos || fieldName.find("Anim") != std::string::npos)) {
                    intType = env->NewGlobalRef(f);
                } else {
                    ints.push_back(env->NewGlobalRef(f));
                }
            } else if (typeName == "byte") {
                if (!byteType && (fieldName.find("type") != std::string::npos || fieldName.find("Type") != std::string::npos ||
                                  fieldName.find("anim") != std::string::npos || fieldName.find("Anim") != std::string::npos)) {
                    byteType = env->NewGlobalRef(f);
                } else {
                    bytes.push_back(env->NewGlobalRef(f));
                }
            }
        }

        env->DeleteLocalRef(f);
        env->ExceptionClear();
    }

    env->DeleteLocalRef(fields);

    if (!intEntity && !ints.empty()) {
        intEntity = ints[0];
        ints[0] = nullptr;
    }

    if (!intType) {
        for (auto& r : ints) {
            if (r) {
                intType = r;
                r = nullptr;
                break;
            }
        }
    }

    if (!byteType) {
        for (auto& r : bytes) {
            if (r) {
                byteType = r;
                r = nullptr;
                break;
            }
        }
    }

    for (auto& r : ints) {
        if (r) env->DeleteGlobalRef(r);
    }
    for (auto& r : bytes) {
        if (r) env->DeleteGlobalRef(r);
    }

    if (intEntity && (intType || byteType)) {
        g_anim.fEntityId = intEntity;
        g_anim.fTypeInt = intType;
        g_anim.fTypeByte = byteType;
        g_anim.ok = true;
        return true;
    }

    if (intEntity) env->DeleteGlobalRef(intEntity);
    if (intType) env->DeleteGlobalRef(intType);
    if (byteType) env->DeleteGlobalRef(byteType);
    g_anim.ok = false;
    return false;
}

static bool infer_potion_effects_from_stack(JNIEnv* env, int entityId, jobject stackLocal) {
    if (!env || entityId <= 0 || !stackLocal)
        return false;

    if (!JniCache::mid_getItem || !JniCache::mid_getIdFromItem || !JniCache::cls_Item) {
        return false;
    }

    jobject itemObj = env->CallObjectMethod(stackLocal, JniCache::mid_getItem);
    if (!itemObj || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (itemObj) env->DeleteLocalRef(itemObj);
        return false;
    }

    int itemId = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, itemObj);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(itemObj);
        return false;
    }

    
    if (itemId != 373) {
        env->DeleteLocalRef(itemObj);
        return false;
    }

    static jclass cls_ItemPotion = nullptr;
    static jmethodID mid_getEffects = nullptr;
    static ULONGLONG lastTryMs = 0;

    if (!cls_ItemPotion) {
        ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastTryMs > 5000) {
            lastTryMs = nowMs;
            jclass local = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.item.ItemPotion"), local) && local) {
                cls_ItemPotion = (jclass)env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
            env->ExceptionClear();
        }
    }

    if (!cls_ItemPotion) {
        env->DeleteLocalRef(itemObj);
        return false;
    }

    if (!mid_getEffects) {
        
        mid_getEffects = env->GetMethodID(cls_ItemPotion, OBFUSCATE_STRING("getEffects").c_str(), OBFUSCATE_STRING("(Lnet/minecraft/item/ItemStack;)Ljava/util/List;").c_str());
        if (!mid_getEffects) {
            env->ExceptionClear();
            mid_getEffects = env->GetMethodID(cls_ItemPotion, OBFUSCATE_STRING("func_77832_l").c_str(), OBFUSCATE_STRING("(Lnet/minecraft/item/ItemStack;)Ljava/util/List;").c_str());
        }
        env->ExceptionClear();
    }

    if (!mid_getEffects || !env->IsInstanceOf(itemObj, cls_ItemPotion)) {
        env->DeleteLocalRef(itemObj);
        return false;
    }

    jobject list = env->CallObjectMethod(itemObj, mid_getEffects, stackLocal);
    env->DeleteLocalRef(itemObj);

    if (!list || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (list) env->DeleteLocalRef(list);
        return false;
    }

    if (!JniCache::mid_listSize || !JniCache::mid_listGet || !JniCache::mid_getPotionID || !JniCache::mid_getDuration ||
        !JniCache::mid_getAmplifier) {
        env->DeleteLocalRef(list);
        return false;
    }

    int count = env->CallIntMethod(list, JniCache::mid_listSize);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(list);
        return false;
    }

    bool any = false;
    for (int i = 0; i < count; i++) {
        jobject eff = env->CallObjectMethod(list, JniCache::mid_listGet, i);
        if (!eff) {
            env->ExceptionClear();
            continue;
        }

        int effId = env->CallIntMethod(eff, JniCache::mid_getPotionID);
        int dur = env->CallIntMethod(eff, JniCache::mid_getDuration);
        int amp = env->CallIntMethod(eff, JniCache::mid_getAmplifier);
        env->ExceptionClear();

        if (effId > 0 && dur > 0) {
            EffectTracker::OnEffectAdd(entityId, effId, amp, dur);
            any = true;
        }

        env->DeleteLocalRef(eff);
    }

    env->DeleteLocalRef(list);
    return any;
}

static void try_handle_equipment_packet(JNIEnv* env, jobject packet) {
    static jclass clsEquip = nullptr;
    static ULONGLONG lastTryMs = 0;
    if (!clsEquip) {
        ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastTryMs > 5000) {
            lastTryMs = nowMs;
            jclass local = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.network.play.server.S04PacketEntityEquipment"), local) && local) {
                clsEquip = (jclass)env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
            env->ExceptionClear();
        }
    }

    if (!clsEquip || !env->IsInstanceOf(packet, clsEquip))
        return;

    if (!g_equip.ok) {
        build_equipment_cache(env, clsEquip);
    }
    if (!g_equip.ok)
        return;

    int entityId = env->CallIntMethod(g_equip.fEntityId, g_reflect.mid_getInt, packet);
    int slot = 0;
    if (g_equip.slotIsShort) {
        slot = (int)env->CallShortMethod(g_equip.fSlot, g_reflect.mid_getShort, packet);
    } else {
        slot = env->CallIntMethod(g_equip.fSlot, g_reflect.mid_getInt, packet);
    }
    jobject stackObj = env->CallObjectMethod(g_equip.fStack, g_reflect.mid_getObj, packet);
    env->ExceptionClear();

    
    if (entityId > 0 && slot == 0) {
        
        jobject prevHeld = get_held_item_local(env, entityId);

        int prevId = 0, prevDmg = 0;
        int newId = 0, newDmg = 0;

        auto readStack = [&](jobject st, int& outId, int& outDmg) {
            outId = 0;
            outDmg = 0;
            if (!st)
                return;

            jobject itemObj = nullptr;
            if (JniCache::mid_getItem && JniCache::mid_getIdFromItem && JniCache::cls_Item) {
                itemObj = env->CallObjectMethod(st, JniCache::mid_getItem);
                if (itemObj && !env->ExceptionCheck()) {
                    outId = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, itemObj);
                }
                env->ExceptionClear();
            }
            if (JniCache::mid_getItemDamage) {
                outDmg = env->CallIntMethod(st, JniCache::mid_getItemDamage);
                env->ExceptionClear();
            }
            if (itemObj)
                env->DeleteLocalRef(itemObj);
        };

        readStack(prevHeld, prevId, prevDmg);
        readStack(stackObj, newId, newDmg);

        int budget = g_equipLogBudget.load();
        if ((g_debugEntityEffects.load() || budget > 0) && (prevId == 373 || newId == 373 || newId == 374)) {
            if (budget > 0) g_equipLogBudget.store(budget - 1);
            log_debug("[ApolloPacketObserver][Equip] entityId=%d slot=%d prev=%d:%d new=%d:%d",
                      entityId, slot, prevId, prevDmg, newId, newDmg);
        }

        
        if (prevHeld && prevId == 373 && newId != 373) {
            bool ok = infer_potion_effects_from_stack(env, entityId, prevHeld);
            budget = g_equipLogBudget.load();
            if (g_debugEntityEffects.load() || budget > 0) {
                if (budget > 0) g_equipLogBudget.store(budget - 1);
                log_debug("[ApolloPacketObserver][PotionInferEquip] entityId=%d ok=%d prevPotionDmg=%d newId=%d",
                          entityId, ok ? 1 : 0, prevDmg, newId);
            }
        }

        if (prevHeld)
            env->DeleteLocalRef(prevHeld);

        set_held_item(env, entityId, stackObj);
    }

    if (stackObj) {
        env->DeleteLocalRef(stackObj);
    }
}

static void try_handle_animation_packet(JNIEnv* env, jobject packet) {
    static jclass clsAnim = nullptr;
    static ULONGLONG lastTryMs = 0;
    if (!clsAnim) {
        ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastTryMs > 5000) {
            lastTryMs = nowMs;
            jclass local = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.network.play.server.S0BPacketAnimation"), local) && local) {
                clsAnim = (jclass)env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
            env->ExceptionClear();
        }
    }

    if (!clsAnim || !env->IsInstanceOf(packet, clsAnim))
        return;

    if (!g_anim.ok) {
        build_animation_cache(env, clsAnim);
    }
    if (!g_anim.ok)
        return;

    int entityId = env->CallIntMethod(g_anim.fEntityId, g_reflect.mid_getInt, packet);
    int animType = 0;
    if (g_anim.fTypeInt) {
        animType = env->CallIntMethod(g_anim.fTypeInt, g_reflect.mid_getInt, packet);
    } else if (g_anim.fTypeByte) {
        animType = (int)env->CallByteMethod(g_anim.fTypeByte, g_reflect.mid_getByte, packet);
    }
    env->ExceptionClear();

    if (entityId <= 0)
        return;

    jobject held = get_held_item_local(env, entityId);
    if (!held)
        return;

    
    int budget = g_animLogBudget.load();
    if (g_debugEntityEffects.load() || budget > 0) {
        if (budget > 0) g_animLogBudget.store(budget - 1);
        int heldId = 0;
        int heldDmg = 0;
        jobject itemObj = nullptr;
        if (JniCache::mid_getItem && JniCache::mid_getIdFromItem && JniCache::cls_Item) {
            itemObj = env->CallObjectMethod(held, JniCache::mid_getItem);
            if (itemObj && !env->ExceptionCheck()) {
                heldId = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, itemObj);
            }
            env->ExceptionClear();
        }
        if (JniCache::mid_getItemDamage) {
            heldDmg = env->CallIntMethod(held, JniCache::mid_getItemDamage);
            env->ExceptionClear();
        }
        if (itemObj) env->DeleteLocalRef(itemObj);

        if (heldId == 373) {
            log_debug("[ApolloPacketObserver][Anim] entityId=%d type=%d heldId=%d dmg=%d", entityId, animType, heldId, heldDmg);
        }
    }

    
    
    if (animType == 3) {
        bool ok = infer_potion_effects_from_stack(env, entityId, held);
        budget = g_animLogBudget.load();
        if ((g_debugEntityEffects.load() || budget > 0) && ok) {
            if (budget > 0) g_animLogBudget.store(budget - 1);
            log_debug("[ApolloPacketObserver][PotionInfer] entityId=%d inferred effects from held potion", entityId);
        }
    }

    env->DeleteLocalRef(held);
}

}  

namespace ApolloPacketObserver {

void SetDebugEntityEffects(bool enabled) {
    g_debugEntityEffects.store(enabled);
    EffectTracker::SetDebug(enabled);
    if (enabled) {
        
        g_effectLogBudget.store(20);
        g_equipLogBudget.store(20);
        g_animLogBudget.store(20);
    }
}











static std::mutex g_onReceiveMutex;

void JNICALL ApolloPacketObserver::NativeOnReceive(JNIEnv* env, jclass, jobject packet) {
    if (!packet)
        return;

    
    
    
    
    
    
    
    

    std::lock_guard<std::mutex> receiveLock(g_onReceiveMutex);

    

    
    
    
    
    
    
    
    
    if (nameTags && nameTags->enabled && nameTags->settings.showEffects) {
        
        try_handle_entity_effect_packets(env, packet);

        
        
        try_handle_equipment_packet(env, packet);
        try_handle_animation_packet(env, packet);
    }

    if (friendsManager && isWorldChangePacket(env, packet)) {
        friendsManager->notifyWorldChangePacket();
        g_typeLogBudget.store(5);
        EffectTracker::Clear();
        clear_held_items(env);

        
        
        ApolloHandshake::OnWorldChange(env);

        return;
    }

    if (::blink && ::blink->enabled && isServerPosLookPacket(env, packet)) {
        static std::atomic<int64_t> lastLogMs{0};
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        int64_t last = lastLogMs.load();
        if (nowMs - last > 500) {
            lastLogMs.store(nowMs);
            log_warn("[Blink] Server correction (S08PacketPlayerPosLook) received while blinking");
        }

        ::blink->setEnabled(false);
    }

    bool blockActive = false;

    
    static jclass cls_S3FPacketCustomPayload = nullptr;
    static jmethodID mid_getChannelName = nullptr;
    static jmethodID mid_getBufferData = nullptr;
    static jmethodID mid_getChannelName_legacy = nullptr;
    static jmethodID mid_getPayloadBytes_legacy = nullptr;
    static jclass cls_PacketBuffer = nullptr;
    static jmethodID mid_readableBytes = nullptr;
    static jmethodID mid_readBytes = nullptr;
    static jmethodID mid_array = nullptr;
    static jmethodID mid_readerIndex = nullptr;
    static bool useLegacyCustomPayload = false;
    static bool triedInit = false;

    if (!triedInit) {
        triedInit = true;
        jclass local = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/play/server/S3FPacketCustomPayload").c_str());
        if (local) {
            cls_S3FPacketCustomPayload = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
            mid_getChannelName = env->GetMethodID(cls_S3FPacketCustomPayload, OBFUSCATE_STRING("getChannelName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
            mid_getBufferData =
                env->GetMethodID(cls_S3FPacketCustomPayload, OBFUSCATE_STRING("getBufferData").c_str(), OBFUSCATE_STRING("()Lnet/minecraft/network/PacketBuffer;").c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                mid_getChannelName = nullptr;
                mid_getBufferData = nullptr;
            }
        }
        
        if (cls_S3FPacketCustomPayload && (!mid_getChannelName || !mid_getBufferData)) {
            mid_getChannelName_legacy =
                env->GetMethodID(cls_S3FPacketCustomPayload, OBFUSCATE_STRING("func_149169_c").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
            mid_getPayloadBytes_legacy = env->GetMethodID(cls_S3FPacketCustomPayload, OBFUSCATE_STRING("func_149168_d").c_str(), OBFUSCATE_STRING("()[B").c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                mid_getChannelName_legacy = nullptr;
                mid_getPayloadBytes_legacy = nullptr;
            } else if (mid_getChannelName_legacy && mid_getPayloadBytes_legacy) {
                useLegacyCustomPayload = true;
            }
        }
        
        jclass pbLocal = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/PacketBuffer").c_str());
        if (pbLocal) {
            cls_PacketBuffer = (jclass)env->NewGlobalRef(pbLocal);
            env->DeleteLocalRef(pbLocal);
            mid_readableBytes = env->GetMethodID(cls_PacketBuffer, OBFUSCATE_STRING("readableBytes").c_str(), OBFUSCATE_STRING("()I").c_str());
            mid_readBytes = env->GetMethodID(cls_PacketBuffer, OBFUSCATE_STRING("readBytes").c_str(), OBFUSCATE_STRING("(I)Lio/netty/buffer/ByteBuf;").c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            mid_array = env->GetMethodID(cls_PacketBuffer, OBFUSCATE_STRING("array").c_str(), OBFUSCATE_STRING("()[B").c_str());
            mid_readerIndex = env->GetMethodID(cls_PacketBuffer, OBFUSCATE_STRING("readerIndex").c_str(), OBFUSCATE_STRING("()I").c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                mid_array = nullptr;
                mid_readerIndex = nullptr;
            }
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    
    if (cls_S3FPacketCustomPayload && env->IsInstanceOf(packet, cls_S3FPacketCustomPayload)) {
        jmethodID channelMethod = mid_getChannelName ? mid_getChannelName : mid_getChannelName_legacy;
        if (channelMethod) {
            jstring channelStr = (jstring)env->CallObjectMethod(packet, channelMethod);
            if (channelStr) {
                const char* channel = env->GetStringUTFChars(channelStr, nullptr);
                if (channel) {
                    
                    static std::unordered_set<std::string> loggedChannels;
                    if (loggedChannels.find(channel) == loggedChannels.end()) {
                        log_debug("[PacketInterceptor] CustomPayload channel: %s", channel);
                        loggedChannels.insert(channel);
                    }

                    
                    if (strcmp(channel, OBFUSCATE_STRING("lunar:apollo").c_str()) == 0 || strcmp(channel, OBFUSCATE_STRING("apollo:json").c_str()) == 0) {
                        jbyteArray byteArray = nullptr;
                        jbyte* bytes = nullptr;
                        const uint8_t* data = nullptr;
                        int len = 0;
                        jobject packetBuffer = nullptr;

                        
                        if (mid_getBufferData && !useLegacyCustomPayload) {
                            packetBuffer = env->CallObjectMethod(packet, mid_getBufferData);
                            if (packetBuffer && !env->ExceptionCheck() && mid_readableBytes && mid_array &&
                                mid_readerIndex) {
                                jint readableBytes = env->CallIntMethod(packetBuffer, mid_readableBytes);
                                if (readableBytes > 0 && readableBytes < 65536) {
                                    jint readerIdx = env->CallIntMethod(packetBuffer, mid_readerIndex);
                                    if (!env->ExceptionCheck()) {
                                        byteArray = (jbyteArray)env->CallObjectMethod(packetBuffer, mid_array);
                                        if (byteArray && !env->ExceptionCheck()) {
                                            jint arrayLen = env->GetArrayLength(byteArray);
                                            if (readerIdx >= 0 && readerIdx + readableBytes <= arrayLen) {
                                                bytes = env->GetByteArrayElements(byteArray, nullptr);
                                                if (bytes) {
                                                    data = (const uint8_t*)(bytes + readerIdx);
                                                    len = readableBytes;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        
                        if (!data && mid_getPayloadBytes_legacy) {
                            byteArray = (jbyteArray)env->CallObjectMethod(packet, mid_getPayloadBytes_legacy);
                            if (byteArray && !env->ExceptionCheck()) {
                                jint arrayLen = env->GetArrayLength(byteArray);
                                if (arrayLen > 0 && arrayLen < 65536) {
                                    bytes = env->GetByteArrayElements(byteArray, nullptr);
                                    if (bytes) {
                                        data = (const uint8_t*)bytes;
                                        len = arrayLen;
                                    }
                                }
                            }
                        }

                        if (data && len > 0) {
                            
                            
                            
                            std::string typeUrl;
                            int pos = 0;
                            int iterations = 0;
                            while (pos < len && iterations++ < kMaxParseIterations) {
                                uint8_t tag = data[pos++];
                                int fieldNum = tag >> 3;
                                int wireType = tag & 0x07;

                                if (wireType == 2) {  
                                    
                                    int strLen = 0;
                                    if (!safeReadVarint(data, len, pos, strLen) || strLen < 0 || pos + strLen > len) {
                                        break;  
                                    }
                                    if (fieldNum == 1 && strLen > 0) {
                                        
                                        typeUrl = std::string((const char*)(data + pos), strLen);
                                    }
                                    pos += strLen;
                                } else if (wireType == 0) {  
                                    if (!safeSkipVarint(data, len, pos)) {
                                        break;  
                                    }
                                } else {
                                    break;  
                                }
                            }

                            
                            if (!typeUrl.empty()) {
                                
                                ApolloHandshake::OnApolloResponse();

                                
                                static const std::unordered_set<std::string> kKnownTypes = {
                                    OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.configurable.v1.OverrideConfigurableSettingsMessage"),
                                    OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.player.v1.UpdatePlayerWorldMessage"),
                                    OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.team.v1.UpdateTeamMembersMessage"),
                                    OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.nametag.v1.OverrideNametagMessage"),
                                    OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.common.v1.PingResponse"),
                                };
                                static std::unordered_set<std::string> s_loggedTypes;
                                if (s_loggedTypes.size() < 30 && kKnownTypes.find(typeUrl) == kKnownTypes.end() &&
                                    s_loggedTypes.insert(typeUrl).second) {
                                    log_debug("[Apollo] New/unknown Apollo type_url: %s", typeUrl.c_str());
                                }

                                int budget = g_typeLogBudget.load();
                                if (budget > 0) {
                                    if (g_typeLogBudget.compare_exchange_strong(budget, budget - 1)) {
                                        log_debug("[Apollo] Apollo payload type: %s (block=%d)", typeUrl.c_str(),
                                                  blockActive ? 1 : 0);
                                    }
                                }

                                static std::atomic<int> teamCount{0};

                                
                                if (typeUrl.find("UpdateTeamMembers") != std::string::npos) {
                                    
                                    const uint8_t* valueData = nullptr;
                                    int valueLen = 0;
                                    int parsePos = 0;
                                    int parseIter = 0;
                                    while (parsePos < len && parseIter++ < kMaxParseIterations) {
                                        uint8_t tag = data[parsePos++];
                                        int fieldNum = tag >> 3;
                                        int wireType = tag & 0x07;
                                        if (wireType == 2) {
                                            int fLen = 0;
                                            if (!safeReadVarint(data, len, parsePos, fLen) || fLen < 0 ||
                                                parsePos + fLen > len) {
                                                break;
                                            }
                                            if (fieldNum == 2 && fLen > 0) {
                                                valueData = data + parsePos;
                                                valueLen = fLen;
                                            }
                                            parsePos += fLen;
                                        } else if (wireType == 0) {
                                            if (!safeSkipVarint(data, len, parsePos))
                                                break;
                                        } else
                                            break;
                                    }

                                    
                                    if (valueData && valueLen > 0) {
                                        std::vector<std::pair<uint64_t, uint64_t>> teammateUuids;

                                        
                                        static std::atomic<int> parseDebugCount{0};
                                        bool shouldDebug = parseDebugCount.load() < 3;
                                        if (shouldDebug) {
                                            log_debug("[Apollo] Team value len: %d bytes", valueLen);
                                        }

                                        int memberCount = 0;
                                        int vPos = 0;
                                        int vIter = 0;
                                        while (vPos < valueLen && vIter++ < kMaxParseIterations) {
                                            uint8_t vTag = valueData[vPos++];
                                            int vFieldNum = vTag >> 3;
                                            int vWireType = vTag & 0x07;

                                            if (vWireType == 2) {  
                                                int vLen = 0;
                                                if (!safeReadVarint(valueData, valueLen, vPos, vLen) || vLen < 0 ||
                                                    vPos + vLen > valueLen) {
                                                    break;
                                                }
                                                if (vFieldNum == 1 && vLen > 0) {
                                                    memberCount++;
                                                    
                                                    const uint8_t* memberData = valueData + vPos;
                                                    int mPos = 0;
                                                    uint64_t uuidHigh = 0, uuidLow = 0;

                                                    
                                                    if (memberCount == 1 && shouldDebug) {
                                                        log_debug(
                                                            "[Apollo]   Member1 len=%d, first bytes: %02x %02x %02x "
                                                            "%02x %02x",
                                                            vLen, vLen > 0 ? memberData[0] : 0,
                                                            vLen > 1 ? memberData[1] : 0, vLen > 2 ? memberData[2] : 0,
                                                            vLen > 3 ? memberData[3] : 0, vLen > 4 ? memberData[4] : 0);
                                                    }
                                                    int mIter = 0;
                                                    while (mPos < vLen && mIter++ < kMaxParseIterations) {
                                                        uint8_t mTag = memberData[mPos++];
                                                        int mFieldNum = mTag >> 3;
                                                        int mWireType = mTag & 0x07;
                                                        if (mWireType == 2) {
                                                            int mLen = 0;
                                                            if (!safeReadVarint(memberData, vLen, mPos, mLen) ||
                                                                mLen < 0 || mPos + mLen > vLen) {
                                                                break;
                                                            }
                                                            if (mFieldNum == 1 && mLen > 0) {
                                                                
                                                                
                                                                const uint8_t* uuidData = memberData + mPos;
                                                                int uPos = 0;
                                                                int uIter = 0;
                                                                while (uPos < mLen && uIter++ < kMaxParseIterations) {
                                                                    uint8_t uTag = uuidData[uPos++];
                                                                    int uFieldNum = uTag >> 3;
                                                                    int uWireType = uTag & 0x07;

                                                                    if (uWireType == 1 && uPos + 8 <= mLen) {
                                                                        
                                                                        uint64_t val = 0;
                                                                        for (int i = 0; i < 8; i++) {
                                                                            val |= (uint64_t)uuidData[uPos + i]
                                                                                   << (i * 8);
                                                                        }
                                                                        uPos += 8;
                                                                        if (uFieldNum == 1)
                                                                            uuidHigh = val;
                                                                        else if (uFieldNum == 2)
                                                                            uuidLow = val;
                                                                    } else if (uWireType == 0) {
                                                                        
                                                                        if (!safeSkipVarint(uuidData, mLen, uPos))
                                                                            break;
                                                                    } else
                                                                        break;
                                                                }
                                                                if (uuidHigh != 0 || uuidLow != 0) {
                                                                    teammateUuids.push_back({uuidHigh, uuidLow});
                                                                }
                                                            }
                                                            mPos += mLen;
                                                        } else if (mWireType == 0) {
                                                            while (mPos < vLen && (memberData[mPos++] & 0x80))
                                                                ;
                                                        } else
                                                            break;
                                                    }
                                                }
                                                vPos += vLen;
                                            } else if (vWireType == 0) {
                                                while (vPos < valueLen && (valueData[vPos++] & 0x80))
                                                    ;
                                            } else
                                                break;
                                        }

                                        if (shouldDebug) {
                                            log_debug("[Apollo] Team: found %d members, extracted %zu UUIDs",
                                                      memberCount, teammateUuids.size());
                                            parseDebugCount++;
                                        }

                                        
                                        if (!teammateUuids.empty() && teamCount.load() < 5) {
                                            log_ok("[Apollo] Team update: %d teammates", (int)teammateUuids.size());
                                            for (size_t i = 0; i < teammateUuids.size(); i++) {
                                                char uuidStr[64];
                                                uint64_t hi = teammateUuids[i].first;
                                                uint64_t lo = teammateUuids[i].second;
                                                snprintf(uuidStr, sizeof(uuidStr), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(hi >> 32), (uint16_t)(hi >> 16), (uint16_t)hi,
                                                         (uint16_t)(lo >> 48),
                                                         (unsigned long long)(lo & 0xFFFFFFFFFFFFULL));
                                                log_debug("  Teammate[%d]: %s", (int)i, uuidStr);
                                            }
                                            teamCount++;
                                        }

                                        
                                        
                                        if (friendsManager) {
                                            
                                            std::set<std::string> newTeammates;
                                            for (size_t i = 0; i < teammateUuids.size(); i++) {
                                                char uuidStr[64];
                                                uint64_t hi = teammateUuids[i].first;
                                                uint64_t lo = teammateUuids[i].second;
                                                snprintf(uuidStr, sizeof(uuidStr), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(hi >> 32), (uint16_t)(hi >> 16), (uint16_t)hi,
                                                         (uint16_t)(lo >> 48),
                                                         (unsigned long long)(lo & 0xFFFFFFFFFFFFULL));
                                                newTeammates.insert(uuidStr);
                                            }

                                            friendsManager->syncLunarTeammates(newTeammates);

                                            
                                            static size_t lastTeammateCount = 0;
                                            size_t currentCount = newTeammates.size();
                                            if (currentCount != lastTeammateCount) {
                                                log_ok("[Apollo] Team synced: %d teammate(s)", (int)currentCount);
                                                lastTeammateCount = currentCount;
                                            }
                                        }
                                    }
                                } else if (!blockActive && typeUrl.find("OverrideNametag") != std::string::npos) {
                                    
                                    const uint8_t* valueData = nullptr;
                                    int valueLen = 0;
                                    int parsePos = 0;
                                    int parseIter = 0;
                                    while (parsePos < len && parseIter++ < kMaxParseIterations) {
                                        uint8_t tag = data[parsePos++];
                                        int fieldNum = tag >> 3;
                                        int wireType = tag & 0x07;
                                        if (wireType == 2) {
                                            int fLen = 0;
                                            if (!safeReadVarint(data, len, parsePos, fLen) || fLen < 0 ||
                                                parsePos + fLen > len) {
                                                break;
                                            }
                                            if (fieldNum == 2 && fLen > 0) {
                                                valueData = data + parsePos;
                                                valueLen = fLen;
                                            }
                                            parsePos += fLen;
                                        } else if (wireType == 0) {
                                            if (!safeSkipVarint(data, len, parsePos))
                                                break;
                                        } else
                                            break;
                                    }

                                    
                                    
                                    
                                    if (valueData && valueLen > 0) {
                                        uint64_t uuidHigh = 0, uuidLow = 0;
                                        std::vector<std::string> jsonLines;

                                        int vPos = 0;
                                        int fieldCount = 0;
                                        int vIter = 0;
                                        while (vPos < valueLen && vIter++ < kMaxParseIterations) {
                                            uint8_t vTag = valueData[vPos++];
                                            int vFieldNum = vTag >> 3;
                                            int vWireType = vTag & 0x07;

                                            if (vWireType == 2) {
                                                int vLen = 0;
                                                if (!safeReadVarint(valueData, valueLen, vPos, vLen) || vLen < 0 ||
                                                    vPos + vLen > valueLen) {
                                                    break;
                                                }
                                                if (vFieldNum == 1 && vLen > 0) {
                                                    const uint8_t* uuidData = valueData + vPos;
                                                    int uPos = 0;
                                                    int uIter = 0;
                                                    while (uPos < vLen && uIter++ < kMaxParseIterations) {
                                                        uint8_t uTag = uuidData[uPos++];
                                                        int uFieldNum = uTag >> 3;
                                                        int uWireType = uTag & 0x07;

                                                        if (uWireType == 1 && uPos + 8 <= vLen) {
                                                            
                                                            uint64_t val = 0;
                                                            for (int i = 0; i < 8; i++) {
                                                                val |= (uint64_t)uuidData[uPos + i] << (i * 8);
                                                            }
                                                            uPos += 8;
                                                            if (uFieldNum == 1)
                                                                uuidHigh = val;
                                                            else if (uFieldNum == 2)
                                                                uuidLow = val;
                                                        } else if (uWireType == 0) {
                                                            if (!safeSkipVarint(uuidData, vLen, uPos))
                                                                break;
                                                        } else
                                                            break;
                                                    }
                                                } else if (vFieldNum == 3 && vLen > 0) {
                                                    jsonLines.push_back(
                                                        std::string((const char*)(valueData + vPos), vLen));
                                                }
                                                vPos += vLen;
                                            } else if (vWireType == 0) {
                                                if (!safeSkipVarint(valueData, valueLen, vPos))
                                                    break;
                                            } else
                                                break;
                                            fieldCount++;
                                        }

                                        
                                        static std::atomic<int> nametagDebug{0};
                                        if (nametagDebug.load() < 3) {
                                            log_debug("[Apollo] Nametag parsed: uuidHigh=%llx uuidLow=%llx lines=%zu",
                                                      (unsigned long long)uuidHigh, (unsigned long long)uuidLow,
                                                      jsonLines.size());
                                            nametagDebug++;
                                        }

                                        
                                        static std::atomic<int> jsonPreviewCount{0};
                                        static std::string lastJsonPreview;
                                        if (!jsonLines.empty() && jsonPreviewCount.load() < 10) {
                                            std::string preview = jsonLines[0];
                                            if (preview.length() > 80)
                                                preview = preview.substr(0, 80) + "...";
                                            if (preview != lastJsonPreview) {
                                                
                                                lastJsonPreview = preview;
                                                jsonPreviewCount++;
                                            }
                                        }

                                        
                                        
                                        std::string detectedColor = "unknown";
                                        bool foundGreen = false;  
                                        
                                        
                                        
                                        bool greenFromExtra = false;

                                        
                                        bool hasExtra = false;
                                        for (const auto& line : jsonLines) {
                                            if (line.find("\"extra\"") != std::string::npos) {
                                                hasExtra = true;
                                                break;
                                            }
                                        }

                                        
                                        
                                        
                                        if (!jsonLines.empty()) {
                                            const std::string colorKey = "\"color\":\"#";
                                            const std::string extraKey = "\"extra\"";

                                            static const std::unordered_map<std::string, std::string> kHexToColor = {
                                                
                                                {"000000", "BLACK"},
                                                {"0000AA", "DARK_BLUE"},
                                                {"00AA00", "DARK_GREEN"},
                                                {"00AAAA", "DARK_AQUA"},
                                                {"AA0000", "DARK_RED"},
                                                {"AA00AA", "DARK_PURPLE"},
                                                {"FFAA00", "GOLD"},
                                                {"AAAAAA", "GRAY"},
                                                {"555555", "DARK_GRAY"},
                                                {"5555FF", "BLUE"},
                                                {"55FF55", "GREEN"},
                                                {"55FFFF", "AQUA"},
                                                {"FF5555", "RED"},
                                                {"FF55FF", "LIGHT_PURPLE"},
                                                {"FFFF55", "YELLOW"},
                                                {"FFFFFF", "WHITE"},
                                                
                                                {"55FF5F", "GREEN"},  
                                                {"FF5551", "RED"},    
                                                {"AA0003", "DARK_RED"},
                                                {"FF55FA", "LIGHT_PURPLE"},
                                                {"AA00AC", "DARK_PURPLE"},
                                                {"00AAAC", "DARK_AQUA"},
                                                {"5555FE", "BLUE"},
                                                {"FFFFFA", "WHITE"},
                                                {"AAAAAC", "GRAY"},
                                                {"55FFFA", "AQUA"},
                                                {"FFAA02", "GOLD"},
                                                {"FFFF5D", "YELLOW"},  
                                            };

                                            
                                            auto extractColorAt = [&](const std::string& line,
                                                                      size_t pos) -> std::string {
                                                if (pos + colorKey.size() + 6 <= line.size()) {
                                                    std::string hex = line.substr(pos + colorKey.size(), 6);
                                                    for (auto& ch : hex)
                                                        ch = (char)std::toupper((unsigned char)ch);
                                                    auto it = kHexToColor.find(hex);
                                                    if (it != kHexToColor.end()) {
                                                        return it->second;
                                                    }
                                                }
                                                return "";
                                            };

                                            
                                            
                                            std::string level0Color;
                                            std::string level1Color;

                                            auto scanColors = [&](const std::string& line) {
                                                int depth = 0;
                                                bool inString = false;
                                                bool escape = false;

                                                for (size_t i = 0; i + colorKey.size() + 6 <= line.size(); ++i) {
                                                    
                                                    if (line.compare(i, colorKey.size(), colorKey) == 0) {
                                                        std::string c = extractColorAt(line, i);
                                                        if (depth == 1 && level0Color.empty()) {
                                                            level0Color = c;
                                                        } else if (depth > 1 && depth <= 3 && level1Color.empty()) {
                                                            level1Color = c;
                                                        }
                                                    }

                                                    char ch = line[i];
                                                    if (inString) {
                                                        if (escape) {
                                                            escape = false;
                                                        } else if (ch == '\\') {
                                                            escape = true;
                                                        } else if (ch == '"') {
                                                            inString = false;
                                                        }
                                                        continue;
                                                    }

                                                    if (ch == '"') {
                                                        inString = true;
                                                        continue;
                                                    }
                                                    if (ch == '{' || ch == '[') {
                                                        depth++;
                                                    } else if ((ch == '}' || ch == ']') && depth > 0) {
                                                        depth--;
                                                    }
                                                }
                                            };

                                            for (const auto& line : jsonLines) {
                                                scanColors(line);
                                                if (!level0Color.empty() && !level1Color.empty())
                                                    break;
                                            }

                                            
                                            
                                            if (level1Color == "GREEN" || level1Color == "DARK_GREEN") {
                                                detectedColor = level1Color;
                                                foundGreen = true;
                                                greenFromExtra = true;
                                            } else if (level0Color == "GREEN" || level0Color == "DARK_GREEN") {
                                                detectedColor = level0Color;
                                                foundGreen = true;
                                            } else if (!level1Color.empty()) {
                                                
                                                detectedColor = level1Color;
                                            } else if (!level0Color.empty()) {
                                                detectedColor = level0Color;
                                            }
                                        }

                                        
                                        if (detectedColor == "unknown") {
                                            for (const auto& line : jsonLines) {
                                                size_t pos = line.find(
                                                    "\xc2"
                                                    "\xa7");
                                                while (pos != std::string::npos && pos + 2 < line.size()) {
                                                    char code = (char)std::tolower((unsigned char)line[pos + 2]);
                                                    switch (code) {
                                                        case '0':
                                                            detectedColor = "BLACK";
                                                            break;
                                                        case '1':
                                                            detectedColor = "DARK_BLUE";
                                                            break;
                                                        case '2':
                                                            detectedColor = "DARK_GREEN";
                                                            break;
                                                        case '3':
                                                            detectedColor = "DARK_AQUA";
                                                            break;
                                                        case '4':
                                                            detectedColor = "DARK_RED";
                                                            break;
                                                        case '5':
                                                            detectedColor = "DARK_PURPLE";
                                                            break;
                                                        case '6':
                                                            detectedColor = "GOLD";
                                                            break;
                                                        case '7':
                                                            detectedColor = "GRAY";
                                                            break;
                                                        case '8':
                                                            detectedColor = "DARK_GRAY";
                                                            break;
                                                        case '9':
                                                            detectedColor = "BLUE";
                                                            break;
                                                        case 'a':
                                                            detectedColor = "GREEN";
                                                            break;
                                                        case 'b':
                                                            detectedColor = "AQUA";
                                                            break;
                                                        case 'c':
                                                            detectedColor = "RED";
                                                            break;
                                                        case 'd':
                                                            detectedColor = "LIGHT_PURPLE";
                                                            break;
                                                        case 'e':
                                                            detectedColor = "YELLOW";
                                                            break;
                                                        case 'f':
                                                            detectedColor = "WHITE";
                                                            break;
                                                        default:
                                                            break;
                                                    }
                                                    if (detectedColor != "unknown")
                                                        break;
                                                    pos = line.find(
                                                        "\xc2"
                                                        "\xa7",
                                                        pos + 3);
                                                }
                                                if (detectedColor != "unknown")
                                                    break;
                                            }
                                        }  

                                        
                                        if (detectedColor == "unknown" && !jsonLines.empty()) {
                                            static std::unordered_set<std::string> unknownLogged;
                                            std::string preview = jsonLines[0];
                                            if (preview.length() > 80)
                                                preview = preview.substr(0, 80) + "...";
                                            if (unknownLogged.size() < 20 &&
                                                unknownLogged.find(preview) == unknownLogged.end()) {
                                                char uuidStr[64];
                                                snprintf(uuidStr, sizeof(uuidStr), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(uuidHigh >> 32), (uint16_t)(uuidHigh >> 16),
                                                         (uint16_t)uuidHigh, (uint16_t)(uuidLow >> 48),
                                                         (unsigned long long)(uuidLow & 0xFFFFFFFFFFFFULL));
                                                log_debug("[Apollo] Nametag unknown color: UUID=%s JSON[0]=%s", uuidStr,
                                                          preview.c_str());
                                                unknownLogged.insert(preview);
                                            }
                                        }

                                        
                                        if ((uuidHigh != 0 || uuidLow != 0) && !jsonLines.empty()) {
                                            
                                            std::string playerName = extractNameFromJsonLines(jsonLines);
                                            if (playerName.empty())
                                                playerName = "?";

                                            
                                            if (playerName != "?" && (uuidHigh != 0 || uuidLow != 0)) {
                                                char regUuid[64];
                                                snprintf(regUuid, sizeof(regUuid), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(uuidHigh >> 32), (uint16_t)(uuidHigh >> 16),
                                                         (uint16_t)uuidHigh, (uint16_t)(uuidLow >> 48),
                                                         (unsigned long long)(uuidLow & 0xFFFFFFFFFFFFULL));
                                                RegisterCleanName(regUuid, playerName);
                                            }

                                            static std::unordered_map<std::string, std::string> lastNametagColor;
                                            std::string uuidKey =
                                                std::to_string(uuidHigh) + ":" + std::to_string(uuidLow);
                                            auto itColor = lastNametagColor.find(uuidKey);
                                            bool isColorChange = (itColor != lastNametagColor.end()) &&
                                                                 (itColor->second != detectedColor);
                                            bool shouldLog = (itColor == lastNametagColor.end()) ||
                                                             (itColor->second != detectedColor);
                                            bool forceAllColors = false;
                                            int forceBudget = g_forceNametagLogs.load();
                                            if (forceBudget > 0) {
                                                
                                                
                                                static std::unordered_set<std::string> forceLoggedUuids;
                                                static int64_t forceSessionStart = 0;
                                                auto now = std::chrono::steady_clock::now().time_since_epoch();
                                                int64_t nowMs =
                                                    std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

                                                
                                                if (nowMs - forceSessionStart > 5000) {
                                                    forceLoggedUuids.clear();
                                                    forceSessionStart = nowMs;
                                                }

                                                
                                                if (forceLoggedUuids.find(uuidKey) == forceLoggedUuids.end()) {
                                                    if (g_forceNametagLogs.compare_exchange_strong(forceBudget,
                                                                                                   forceBudget - 1)) {
                                                        forceLoggedUuids.insert(uuidKey);
                                                        shouldLog = true;
                                                        forceAllColors = true;
                                                    }
                                                }
                                            }

                                            
                                            if (isColorChange && (detectedColor == "GREEN" ||
                                                                  detectedColor == "DARK_GREEN" || hasExtra)) {
                                                char uuidStr[64];
                                                snprintf(uuidStr, sizeof(uuidStr), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(uuidHigh >> 32), (uint16_t)(uuidHigh >> 16),
                                                         (uint16_t)uuidHigh, (uint16_t)(uuidLow >> 48),
                                                         (unsigned long long)(uuidLow & 0xFFFFFFFFFFFFULL));
                                                log_warn("[Apollo] COLOR CHANGE for %s: %s -> %s", uuidStr,
                                                         itColor->second.c_str(), detectedColor.c_str());
                                                
                                                for (size_t jIdx = 0; jIdx < jsonLines.size() && jIdx < 3; jIdx++) {
                                                    std::string jsonPreview = jsonLines[jIdx];
                                                    if (jsonPreview.length() > 120)
                                                        jsonPreview = jsonPreview.substr(0, 120) + "...";
                                                    log_debug("[Apollo]   JSON[%zu]: %s", jIdx, jsonPreview.c_str());
                                                }
                                            }

                                            if (shouldLog) {
                                                lastNametagColor[uuidKey] = detectedColor;
                                                bool isTeammateSignal =
                                                    (detectedColor == "GREEN" || detectedColor == "DARK_GREEN");
                                                if (forceAllColors || isTeammateSignal || detectedColor == "unknown") {
                                                    log_ok("[Apollo] Nametag: \"%s\" -> %s", playerName.c_str(),
                                                           detectedColor.c_str());
                                                }
                                            }

                                            
                                            if (friendsManager && detectedColor != "unknown") {
                                                char uuidStr[64];
                                                snprintf(uuidStr, sizeof(uuidStr), "%08x-%04x-%04x-%04x-%012llx",
                                                         (uint32_t)(uuidHigh >> 32), (uint16_t)(uuidHigh >> 16),
                                                         (uint16_t)uuidHigh, (uint16_t)(uuidLow >> 48),
                                                         (unsigned long long)(uuidLow & 0xFFFFFFFFFFFFULL));
                                                bool hasTeammateExtra = greenFromExtra;
                                                friendsManager->setApolloNametagColor(uuidStr, detectedColor,
                                                                                      hasTeammateExtra);

                                                
                                                if (foundGreen) {
                                                    static std::unordered_map<std::string, int64_t> lastSignalLogMs;
                                                    auto now = std::chrono::steady_clock::now().time_since_epoch();
                                                    int64_t nowMs =
                                                        std::chrono::duration_cast<std::chrono::milliseconds>(now)
                                                            .count();
                                                    int64_t& last = lastSignalLogMs[uuidStr];
                                                    if (nowMs - last > 2000) {
                                                        log_debug(
                                                            "[Apollo] Teammate signal: %s name=%s color=%s extra=%d "
                                                            "green=%d lines=%zu",
                                                            uuidStr, playerName.c_str(), detectedColor.c_str(),
                                                            hasExtra ? 1 : 0, foundGreen ? 1 : 0, jsonLines.size());
                                                        for (size_t jIdx = 0; jIdx < jsonLines.size() && jIdx < 3;
                                                             jIdx++) {
                                                            std::string jsonPreview = jsonLines[jIdx];
                                                            if (jsonPreview.length() > 200)
                                                                jsonPreview = jsonPreview.substr(0, 200) + "...";
                                                            log_debug("[Apollo]   RAW[%zu]: %s", jIdx,
                                                                      jsonPreview.c_str());
                                                        }
                                                        last = nowMs;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (byteArray && bytes) {
                            env->ReleaseByteArrayElements(byteArray, bytes, JNI_ABORT);
                        }
                        if (packetBuffer) {
                            env->DeleteLocalRef(packetBuffer);
                        }
                        if (env->ExceptionCheck())
                            env->ExceptionClear();
                    }
                    env->ReleaseStringUTFChars(channelStr, channel);
                }
                env->DeleteLocalRef(channelStr);
            }
        }
    }
    
    static jclass cls_S3EPacketTeams = nullptr;
    static jmethodID mid_getAction = nullptr;
    static jmethodID mid_getName = nullptr;
    static jmethodID mid_getPlayers = nullptr;
    static bool triedTeamsInit = false;

    if (!triedTeamsInit) {
        triedTeamsInit = true;
        jclass local = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/play/server/S3EPacketTeams").c_str());
        if (local) {
            cls_S3EPacketTeams = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
            mid_getAction = env->GetMethodID(cls_S3EPacketTeams, OBFUSCATE_STRING("getAction").c_str(), OBFUSCATE_STRING("()I").c_str());
            mid_getName = env->GetMethodID(cls_S3EPacketTeams, OBFUSCATE_STRING("getName").c_str(), OBFUSCATE_STRING("()Ljava/lang/String;").c_str());
            mid_getPlayers = env->GetMethodID(cls_S3EPacketTeams, OBFUSCATE_STRING("getPlayers").c_str(), OBFUSCATE_STRING("()Ljava/util/Collection;").c_str());
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    if (cls_S3EPacketTeams && env->IsInstanceOf(packet, cls_S3EPacketTeams)) {
        static std::atomic<int> teamPacketCount{0};

        
        jint action = mid_getAction ? env->CallIntMethod(packet, mid_getAction) : -1;

        
        jstring nameStr = mid_getName ? (jstring)env->CallObjectMethod(packet, mid_getName) : nullptr;
        const char* teamName = nameStr ? env->GetStringUTFChars(nameStr, nullptr) : nullptr;

        
        jobject playersCollection = mid_getPlayers ? env->CallObjectMethod(packet, mid_getPlayers) : nullptr;
        int playersCount = 0;
        std::vector<std::string> teamPlayers;
        if (playersCollection) {
            jclass cls_Collection = env->FindClass(OBFUSCATE_STRING("java/util/Collection").c_str());
            if (env->ExceptionCheck())
                env->ExceptionClear();
            
            
            jmethodID mid_size = cls_Collection ? env->GetMethodID(cls_Collection, OBFUSCATE_STRING("size").c_str(), OBFUSCATE_STRING("()I").c_str()) : nullptr;
            jmethodID mid_iterator = cls_Collection ? env->GetMethodID(cls_Collection, OBFUSCATE_STRING("iterator").c_str(),
                                                      OBFUSCATE_STRING("()Ljava/util/Iterator;").c_str()) : nullptr;
            if (mid_size) {
                playersCount = env->CallIntMethod(playersCollection, mid_size);
            }
            if (mid_iterator) {
                jobject iterator = env->CallObjectMethod(playersCollection, mid_iterator);
                if (iterator) {
                    jclass cls_Iterator = env->FindClass(OBFUSCATE_STRING("java/util/Iterator").c_str());
                    if (env->ExceptionCheck())
                        env->ExceptionClear();
                    jmethodID mid_hasNext = cls_Iterator ? env->GetMethodID(cls_Iterator, OBFUSCATE_STRING("hasNext").c_str(),
                                                            OBFUSCATE_STRING("()Z").c_str()) : nullptr;
                    jmethodID mid_next = cls_Iterator ? env->GetMethodID(cls_Iterator, OBFUSCATE_STRING("next").c_str(),
                                                         OBFUSCATE_STRING("()Ljava/lang/Object;").c_str()) : nullptr;
                    while (mid_hasNext && mid_next && env->CallBooleanMethod(iterator, mid_hasNext)) {
                        jobject value = env->CallObjectMethod(iterator, mid_next);
                        if (value) {
                            teamPlayers.push_back(jstringToString((jstring)value));
                            env->DeleteLocalRef(value);
                        }
                    }
                    if (cls_Iterator)
                        env->DeleteLocalRef(cls_Iterator);
                    env->DeleteLocalRef(iterator);
                }
            }
            if (cls_Collection)
                env->DeleteLocalRef(cls_Collection);
            env->DeleteLocalRef(playersCollection);
        }

        if (friendsManager && teamName)
            friendsManager->updateScoreboardTeam(teamName, action, teamPlayers);

        
        
        bool isRelevant = (action == 0 || action == 3 || action == 4 || playersCount > 0);

        if (isRelevant && teamPacketCount.load() < 20) {
            const char* actionStr = "";
            switch (action) {
                case 0:
                    actionStr = "CREATE";
                    break;
                case 1:
                    actionStr = "REMOVE";
                    break;
                case 2:
                    actionStr = "UPDATE";
                    break;
                case 3:
                    actionStr = "ADD_PLAYERS";
                    break;
                case 4:
                    actionStr = "REMOVE_PLAYERS";
                    break;
                default:
                    actionStr = "UNKNOWN";
                    break;
            }

            
            

            teamPacketCount++;
        }

        if (teamName)
            env->ReleaseStringUTFChars(nameStr, teamName);
        if (nameStr)
            env->DeleteLocalRef(nameStr);
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    
    if (!blockActive && LunarPacketListener::IsReady()) {
        LunarPacketListener::processIncomingPacket(env, packet);
    }
}

void ApolloPacketObserver::BoostLogBudget(int count) {
    g_typeLogBudget.store(count);
}

void ApolloPacketObserver::ForceLogNametags(int count) {
    g_forceNametagLogs.store(count);
}

std::string ApolloPacketObserver::GetCleanNameByUUID(const std::string& uuid) {
    if (uuid.empty())
        return "";
    std::lock_guard<std::mutex> lock(g_cleanNamesMutex);
    auto it = g_cleanNames.find(uuid);
    if (it != g_cleanNames.end())
        return it->second;
    return "";
}

void ApolloPacketObserver::RegisterCleanName(const std::string& uuid, const std::string& name) {
    if (uuid.empty() || name.empty())
        return;
    std::lock_guard<std::mutex> lock(g_cleanNamesMutex);
    g_cleanNames[uuid] = name;
}

void ApolloPacketObserver::ClearCleanNames() {
    std::lock_guard<std::mutex> lock(g_cleanNamesMutex);
    g_cleanNames.clear();
}

}  
