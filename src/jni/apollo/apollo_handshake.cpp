#include "apollo_handshake.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../util/logger.hpp"
#include "../../util/aes_string.hpp"
#include "../java.h"
#include "apollo_packet_observer.hpp"

namespace ApolloHandshake {





static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_debugLogging{true};  
static std::atomic<uint64_t> g_worldSerial{0};
static std::atomic<uint64_t> g_lastHandshakeSerial{0};
static std::mutex g_mutex;
static std::atomic<int64_t> g_lastWorldChangeMs{0};  

static Stats g_stats;


static std::atomic<bool> g_pendingHandshake{false};
static std::atomic<int64_t> g_pendingHandshakeTimeMs{0};
static constexpr int64_t kHandshakeRetryDelayMs = 100;  
static constexpr int kMaxHandshakeRetries = 50;         
static std::atomic<int> g_handshakeRetryCount{0};


static jclass g_clsNetHandler = nullptr;
static jmethodID g_midAddToSendQueue = nullptr;
static jclass g_clsC17Packet = nullptr;
static jmethodID g_midC17Ctor = nullptr;
static jclass g_clsPacketBuffer = nullptr;
static jmethodID g_midPacketBufferCtor = nullptr;
static jclass g_clsUnpooled = nullptr;
static jmethodID g_midWrappedBuffer = nullptr;






static void writeVarint(std::vector<uint8_t>& buf, uint64_t value) {
    while (value > 0x7F) {
        buf.push_back((uint8_t)((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buf.push_back((uint8_t)value);
}


static void writeLengthDelimited(std::vector<uint8_t>& buf, int fieldNum, const uint8_t* data, size_t len) {
    
    writeVarint(buf, (fieldNum << 3) | 2);
    writeVarint(buf, len);
    buf.insert(buf.end(), data, data + len);
}


static void writeString(std::vector<uint8_t>& buf, int fieldNum, const std::string& str) {
    writeLengthDelimited(buf, fieldNum, (const uint8_t*)str.data(), str.size());
}


static void writeVarintField(std::vector<uint8_t>& buf, int fieldNum, uint64_t value) {
    
    writeVarint(buf, (fieldNum << 3) | 0);
    writeVarint(buf, value);
}


static void writeEmbeddedMessage(std::vector<uint8_t>& buf, int fieldNum, const std::vector<uint8_t>& msg) {
    writeLengthDelimited(buf, fieldNum, msg.data(), msg.size());
}







static std::vector<uint8_t> buildMinecraftVersion(const std::string& version) {
    std::vector<uint8_t> msg;
    writeString(msg, 1, version);  
    return msg;
}



static std::vector<uint8_t> buildLunarClientVersion() {
    std::vector<uint8_t> msg;
    writeString(msg, 1, "master");   
    writeString(msg, 2, "abc1234");  
    writeString(msg, 3, "3.2.0");    
    return msg;
}




static std::vector<uint8_t> buildModMessage(const std::string& id, const std::string& name, const std::string& version,
                                            int type) {
    std::vector<uint8_t> msg;
    writeString(msg, 1, id);
    writeString(msg, 2, name);
    writeString(msg, 3, version);
    writeVarintField(msg, 4, type);  
    return msg;
}









static std::vector<uint8_t> buildPlayerHandshakeMessage() {
    std::vector<uint8_t> msg;

    
    auto mcVersion = buildMinecraftVersion("V1_8");
    writeEmbeddedMessage(msg, 1, mcVersion);

    
    auto lcVersion = buildLunarClientVersion();
    writeEmbeddedMessage(msg, 2, lcVersion);

    
    auto mod1 = buildModMessage("freelook", "Freelook", "1.0.0", 3);  
    writeEmbeddedMessage(msg, 3, mod1);

    auto mod2 = buildModMessage("teamview", "Team View", "1.0.0", 3);
    writeEmbeddedMessage(msg, 3, mod2);

    auto mod3 = buildModMessage("nametag", "Nametag", "1.0.0", 3);
    writeEmbeddedMessage(msg, 3, mod3);

    
    writeVarintField(msg, 4, 1);

    return msg;
}



static std::vector<uint8_t> buildAnyWrapper(const std::string& typeUrl, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> msg;
    writeString(msg, 1, typeUrl);
    writeLengthDelimited(msg, 2, payload.data(), payload.size());
    return msg;
}










static std::vector<uint8_t> buildConfigurableSettings(bool enable) {
    std::vector<uint8_t> msg;
    writeVarintField(msg, 1, enable ? 1 : 0);  
    
    return msg;
}






static std::vector<uint8_t> buildOverrideConfigurableSettingsMessage() {
    std::vector<uint8_t> msg;

    
    

    auto settings = buildConfigurableSettings(true);

    
    {
        std::vector<uint8_t> entry;
        writeString(entry, 1, "nametag");
        writeEmbeddedMessage(entry, 2, settings);
        writeEmbeddedMessage(msg, 1, entry);
    }

    
    {
        std::vector<uint8_t> entry;
        writeString(entry, 1, "team");
        writeEmbeddedMessage(entry, 2, settings);
        writeEmbeddedMessage(msg, 1, entry);
    }

    
    {
        std::vector<uint8_t> entry;
        writeString(entry, 1, "team_view");
        writeEmbeddedMessage(entry, 2, settings);
        writeEmbeddedMessage(msg, 1, entry);
    }

    return msg;
}





static jobject getNetHandler(JNIEnv* env) {
    if (!env || !mc.isValid() || !JniCache::mid_getNetHandler)
        return nullptr;

    jobject netHandler = env->CallObjectMethod(mc.instance, JniCache::mid_getNetHandler);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (netHandler)
            env->DeleteLocalRef(netHandler);
        return nullptr;
    }
    return netHandler;
}

static bool initSendPacketMethod(JNIEnv* env, jobject netHandler) {
    if (g_midAddToSendQueue)
        return true;

    jclass cls = env->GetObjectClass(netHandler);
    if (!cls)
        return false;

    
    g_midAddToSendQueue = env->GetMethodID(cls, OBFUSCATE_STRING("addToSendQueue").c_str(), OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    if (!g_midAddToSendQueue) {
        env->ExceptionClear();
        g_midAddToSendQueue = env->GetMethodID(cls, OBFUSCATE_STRING("func_147297_a").c_str(), OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    }
    if (!g_midAddToSendQueue) {
        env->ExceptionClear();
        g_midAddToSendQueue = env->GetMethodID(cls, OBFUSCATE_STRING("sendPacket").c_str(), OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());
    }

    env->DeleteLocalRef(cls);
    return g_midAddToSendQueue != nullptr;
}

static bool initPacketClasses(JNIEnv* env) {
    if (g_clsC17Packet)
        return true;

    
    jclass c17Local = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/play/client/C17PacketCustomPayload").c_str());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] C17PacketCustomPayload class not found");
        return false;
    }
    if (!c17Local) {
        log_warn("[ApolloHandshake] C17PacketCustomPayload class is null");
        return false;
    }

    g_clsC17Packet = (jclass)env->NewGlobalRef(c17Local);
    env->DeleteLocalRef(c17Local);

    
    g_midC17Ctor =
        env->GetMethodID(g_clsC17Packet, OBFUSCATE_STRING("<init>").c_str(), OBFUSCATE_STRING("(Ljava/lang/String;Lnet/minecraft/network/PacketBuffer;)V").c_str());
    if (!g_midC17Ctor) {
        env->ExceptionClear();
        g_midC17Ctor = env->GetMethodID(g_clsC17Packet, OBFUSCATE_STRING("<init>").c_str(), OBFUSCATE_STRING("(Ljava/lang/String;Lio/netty/buffer/ByteBuf;)V").c_str());
    }
    if (!g_midC17Ctor) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] C17PacketCustomPayload constructor not found");
        return false;
    }

    
    jclass pbLocal = env->FindClass(OBFUSCATE_STRING("net/minecraft/network/PacketBuffer").c_str());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] PacketBuffer class not found");
        return false;
    }
    if (pbLocal) {
        g_clsPacketBuffer = (jclass)env->NewGlobalRef(pbLocal);
        env->DeleteLocalRef(pbLocal);

        
        g_midPacketBufferCtor = env->GetMethodID(g_clsPacketBuffer, OBFUSCATE_STRING("<init>").c_str(), OBFUSCATE_STRING("(Lio/netty/buffer/ByteBuf;)V").c_str());
        if (!g_midPacketBufferCtor) {
            env->ExceptionClear();
        }
    }

    
    jclass unpooledLocal = env->FindClass(OBFUSCATE_STRING("io/netty/buffer/Unpooled").c_str());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] Unpooled class not found");
        return false;
    }
    if (unpooledLocal) {
        g_clsUnpooled = (jclass)env->NewGlobalRef(unpooledLocal);
        env->DeleteLocalRef(unpooledLocal);

        
        g_midWrappedBuffer = env->GetStaticMethodID(g_clsUnpooled, OBFUSCATE_STRING("wrappedBuffer").c_str(), OBFUSCATE_STRING("([B)Lio/netty/buffer/ByteBuf;").c_str());
        if (!g_midWrappedBuffer) {
            env->ExceptionClear();
            log_warn("[ApolloHandshake] Unpooled.wrappedBuffer not found");
        }
    }

    
    return true;
}


static jobject createCustomPayloadPacket(JNIEnv* env, const char* channel, const std::vector<uint8_t>& data) {
    if (!initPacketClasses(env)) {
        return nullptr;
    }

    
    jbyteArray byteArray = env->NewByteArray((jsize)data.size());
    if (!byteArray) {
        log_warn("[ApolloHandshake] Failed to create byte array");
        return nullptr;
    }
    env->SetByteArrayRegion(byteArray, 0, (jsize)data.size(), (const jbyte*)data.data());

    
    jobject byteBuf = nullptr;
    if (g_clsUnpooled && g_midWrappedBuffer) {
        byteBuf = env->CallStaticObjectMethod(g_clsUnpooled, g_midWrappedBuffer, byteArray);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            byteBuf = nullptr;
        }
    }

    if (!byteBuf) {
        log_warn("[ApolloHandshake] Failed to create ByteBuf");
        env->DeleteLocalRef(byteArray);
        return nullptr;
    }

    
    jobject packetBuffer = nullptr;
    if (g_clsPacketBuffer && g_midPacketBufferCtor) {
        packetBuffer = env->NewObject(g_clsPacketBuffer, g_midPacketBufferCtor, byteBuf);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            packetBuffer = nullptr;
        }
    }

    
    jobject bufferArg = packetBuffer ? packetBuffer : byteBuf;

    
    jstring channelStr = env->NewStringUTF(channel);
    if (!channelStr) {
        log_warn("[ApolloHandshake] Failed to create channel string");
        if (packetBuffer)
            env->DeleteLocalRef(packetBuffer);
        env->DeleteLocalRef(byteBuf);
        env->DeleteLocalRef(byteArray);
        return nullptr;
    }

    
    jobject packet = env->NewObject(g_clsC17Packet, g_midC17Ctor, channelStr, bufferArg);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] Failed to create C17PacketCustomPayload");
        packet = nullptr;
    }

    
    env->DeleteLocalRef(channelStr);
    if (packetBuffer && packetBuffer != bufferArg)
        env->DeleteLocalRef(packetBuffer);
    env->DeleteLocalRef(byteBuf);
    env->DeleteLocalRef(byteArray);

    return packet;
}

static bool sendPacketToServer(JNIEnv* env, jobject packet) {
    if (!packet)
        return false;

    jobject netHandler = getNetHandler(env);
    if (!netHandler) {
        log_warn("[ApolloHandshake] Failed to get NetHandler");
        env->DeleteLocalRef(packet);
        return false;
    }

    if (!initSendPacketMethod(env, netHandler)) {
        log_warn("[ApolloHandshake] Failed to get sendPacket method");
        env->DeleteLocalRef(netHandler);
        env->DeleteLocalRef(packet);
        return false;
    }

    env->CallVoidMethod(netHandler, g_midAddToSendQueue, packet);
    bool success = !env->ExceptionCheck();
    if (!success) {
        env->ExceptionClear();
        log_warn("[ApolloHandshake] Exception while sending packet");
    }

    env->DeleteLocalRef(netHandler);
    env->DeleteLocalRef(packet);
    return success;
}





bool Init() {
    if (g_initialized.load())
        return true;

    g_initialized.store(true);
    log_ok("[ApolloHandshake] Initialized");
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);

    
    

    g_initialized.store(false);
    log_debug("[ApolloHandshake] Shutdown");
}

bool IsReady() {
    
    return mc.isValid();
}

uint64_t GetWorldSerial() {
    return g_worldSerial.load();
}

void SetDebugLogging(bool enabled) {
    g_debugLogging.store(enabled);
}

bool IsDebugLogging() {
    return g_debugLogging.load();
}

Stats GetStats() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_stats;
}

void OnApolloResponse() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_stats.responsesReceived++;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    g_stats.lastResponseMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool SendHandshake(JNIEnv* env) {
    if (!env || !IsReady()) {
        log_warn("[ApolloHandshake] Not ready to send handshake");
        return false;
    }

    bool debug = g_debugLogging.load();

    
    auto handshakePayload = buildPlayerHandshakeMessage();
     




    
    std::string typeUrl = OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.player.v1.PlayerHandshakeMessage");
    auto anyWrapper = buildAnyWrapper(typeUrl, handshakePayload);
     




    
    jobject packet = createCustomPayloadPacket(env, OBFUSCATE_STRING("lunar:apollo").c_str(), anyWrapper);
    if (!packet) {
        log_warn("[ApolloHandshake] Failed to create handshake packet");
        return false;
    }

    if (!sendPacketToServer(env, packet)) {
        log_warn("[ApolloHandshake] Failed to send handshake packet");
        return false;
    }

    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stats.handshakesSent++;
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        g_stats.lastHandshakeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    log_ok("[ApolloHandshake] Handshake sent on lunar:apollo channel (%zu bytes)", anyWrapper.size());
    return true;
}

bool SendPingRequest(JNIEnv* env) {
    if (!env || !IsReady()) {
        return false;
    }

    bool debug = g_debugLogging.load();

    
    
    std::vector<uint8_t> pingPayload;  

    
    std::string typeUrl = OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.common.v1.PingRequest");
    auto anyWrapper = buildAnyWrapper(typeUrl, pingPayload);

     



    
    jobject packet = createCustomPayloadPacket(env, OBFUSCATE_STRING("lunar:apollo").c_str(), anyWrapper);
    if (!packet) {
        return false;
    }

    if (!sendPacketToServer(env, packet)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stats.pingsSent++;
    }

     



    return true;
}

void OnWorldChange(JNIEnv* env) {
    
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t lastMs = g_lastWorldChangeMs.load();
    if (nowMs - lastMs < 800) {
        
        return;
    }
    g_lastWorldChangeMs.store(nowMs);

    uint64_t newSerial = g_worldSerial.fetch_add(1) + 1;
    uint64_t lastHandshake = g_lastHandshakeSerial.load();

    if (newSerial == lastHandshake) {
        
        
        return;
    }

    

    
    if (mc.isValid() && SendHandshake(env)) {
        g_lastHandshakeSerial.store(newSerial);
        SendPingRequest(env);
        return;
    }

    
    g_pendingHandshakeTimeMs.store(nowMs);
    g_handshakeRetryCount.store(0);
    g_pendingHandshake.store(true);
    
}

void Tick() {
    
    if (!g_pendingHandshake.load()) {
        return;
    }

    
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t lastAttempt = g_pendingHandshakeTimeMs.load();

    if (nowMs - lastAttempt < kHandshakeRetryDelayMs) {
        return;  
    }

    
    int retries = g_handshakeRetryCount.fetch_add(1);
    if (retries >= kMaxHandshakeRetries) {
        
        g_pendingHandshake.store(false);
        return;
    }

    
    if (!mc.isValid()) {
        g_pendingHandshakeTimeMs.store(nowMs);
        if (retries % 10 == 0) {  
            log_debug("[ApolloHandshake] Still waiting for mc... (retry %d/%d)", retries, kMaxHandshakeRetries);
        }
        return;
    }

    
    JNIEnv* env = nullptr;
    if (Java::vm) {
        Java::vm->GetEnv((void**)&env, JNI_VERSION_1_8);
        if (!env) {
            Java::vm->AttachCurrentThread((void**)&env, nullptr);
        }
    }

    if (!env) {
        g_pendingHandshakeTimeMs.store(nowMs);
        return;
    }

    
    uint64_t currentSerial = g_worldSerial.load();
    if (SendHandshake(env)) {
        g_lastHandshakeSerial.store(currentSerial);
        SendPingRequest(env);
        g_pendingHandshake.store(false);
        
    } else {
        g_pendingHandshakeTimeMs.store(nowMs);
    }
}

bool HasPendingHandshake() {
    return g_pendingHandshake.load();
}

void RequestNametagRefresh() {
    
    
    g_lastHandshakeSerial.store(0);

    
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    g_pendingHandshakeTimeMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    g_handshakeRetryCount.store(0);
    g_pendingHandshake.store(true);

    
}

bool SendConfigurableSettings(JNIEnv* env) {
    if (!env || !IsReady()) {
        
        return false;
    }

    bool debug = g_debugLogging.load();

    
    auto configPayload = buildOverrideConfigurableSettingsMessage();
    if (debug) {
        
    }

    
    std::string typeUrl = OBFUSCATE_STRING("type.googleapis.com/lunarclient.apollo.configurable.v1.OverrideConfigurableSettingsMessage");
    auto anyWrapper = buildAnyWrapper(typeUrl, configPayload);
    if (debug) {
        
    }

    
    jobject packet = createCustomPayloadPacket(env, OBFUSCATE_STRING("lunar:apollo").c_str(), anyWrapper);
    if (!packet) {
        
        return false;
    }

    if (!sendPacketToServer(env, packet)) {
        
        return false;
    }

    
    return true;
}

bool RequestNametagData() {
    if (!IsReady()) {
        
        return false;
    }

    
    JNIEnv* env = nullptr;
    if (Java::vm) {
        Java::vm->GetEnv((void**)&env, JNI_VERSION_1_8);
        if (!env) {
            Java::vm->AttachCurrentThread((void**)&env, nullptr);
        }
    }

    if (!env) {
        
        return false;
    }

    

    
    ApolloPacketObserver::BoostLogBudget(100);
    ApolloPacketObserver::ForceLogNametags(100);

    
    bool handshakeOk = SendHandshake(env);

    
    bool configOk = SendConfigurableSettings(env);

    
    bool pingOk = SendPingRequest(env);

    if (handshakeOk || configOk) {
        
        
        return true;
    }

    return false;
}

}  
