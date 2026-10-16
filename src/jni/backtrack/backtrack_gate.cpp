#include "backtrack_gate.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "../../module/combat/backtrack.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/jni/jni_cache.h"
#include "../../sdk/jni/member_resolver.h"
#include "../../util/logger.hpp"
#include "../java.h"

namespace {

static const std::uint8_t kIncomingHookBytecode[] = {
    0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x00, 0x00, 0x34, 0x00, 0x0C,
    0x01, 0x00, 0x1D,
    'c','o','m','/','c','o','b','b','l','e','/','a','g','e','n','t','/',
    'I','n','c','o','m','i','n','g','H','o','o','k',
    0x07, 0x00, 0x01,
    0x01, 0x00, 0x10,
    'j','a','v','a','/','l','a','n','g','/','O','b','j','e','c','t',
    0x07, 0x00, 0x03,
    0x01, 0x00, 0x06, '<','i','n','i','t','>',
    0x01, 0x00, 0x03, '(',')','V',
    0x01, 0x00, 0x04, 'C','o','d','e',
    0x01, 0x00, 0x0A, 'c','a','n','P','r','o','c','e','s','s',
    0x01, 0x00, 0x27,
    '(','L','j','a','v','a','/','l','a','n','g','/','O','b','j','e','c','t',';',
        'L','j','a','v','a','/','l','a','n','g','/','O','b','j','e','c','t',';',')','Z',
    0x0C, 0x00, 0x05, 0x00, 0x06,
    0x0A, 0x00, 0x04, 0x00, 0x0A,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02,
    0x00, 0x01, 0x00, 0x05, 0x00, 0x06, 0x00, 0x01,
    0x00, 0x07, 0x00, 0x00, 0x00, 0x11,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
    0x2A, 0xB7, 0x00, 0x0B, 0xB1,
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x09, 0x00, 0x08, 0x00, 0x09, 0x00, 0x00,
    0x00, 0x00,
};

constexpr std::uint64_t kRetentionMs = 180;
constexpr std::size_t kSoftQueueLimit = 256;
constexpr std::size_t kHardQueueLimit = 2048;
constexpr int kMaxReplayPerTick = 128;
constexpr std::uint64_t kMaxRetentionMs = 1000;




constexpr std::size_t kStreamQueueCeiling = 1024;




std::atomic<std::uint64_t> g_retentionMs{kRetentionMs};
std::atomic<std::size_t> g_softQueueLimit{kSoftQueueLimit};
std::atomic<bool> g_wholeStream{true};

std::atomic<bool> g_initialized{false};
std::atomic<bool> g_operational{false};
std::atomic<bool> g_shuttingDown{false};
std::atomic<bool> g_discarding{false};
std::atomic<bool> g_emergencyDrain{false};
std::atomic<bool> g_callbackObserved{false};
std::atomic<bool> g_firstPacketReported{false};
std::atomic<bool> g_missingSourceReported{false};
std::atomic<std::uint32_t> g_callbacksInFlight{0};

jclass g_incomingHook = nullptr;
jclass g_networkManager = nullptr;
jfieldID g_packetListener = nullptr;

jclass g_s14 = nullptr;
jclass g_s18 = nullptr;
jfieldID g_s14EntityId = nullptr;
jfieldID g_s14X = nullptr;
jfieldID g_s14Y = nullptr;
jfieldID g_s14Z = nullptr;
jfieldID g_s18EntityId = nullptr;
jfieldID g_s18X = nullptr;
jfieldID g_s18Y = nullptr;
jfieldID g_s18Z = nullptr;
jmethodID g_s14Process = nullptr;
jmethodID g_s18Process = nullptr;

enum class MovementKind : std::uint8_t { Relative, Teleport };

struct Movement {
    MovementKind kind = MovementKind::Relative;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

struct HeldPacket {
    jobject packet = nullptr;
    int entityId = -1;
    std::uint64_t engagementEpoch = 0;
    std::uint64_t connectionEpoch = 0;
    std::uint64_t heldAtMs = 0;
    std::uint64_t releaseAtMs = 0;
    Movement movement;
};

struct ReplayMarker {
    bool active = false;
    int entityId = -1;
    std::uint64_t engagementEpoch = 0;
    std::uint64_t connectionEpoch = 0;
    Movement movement;
};

struct DetachedState {
    std::deque<HeldPacket> packets;
    jobject handler = nullptr;
};

std::deque<HeldPacket> g_queue;















constexpr double kJitterFraction = 0.12;  
constexpr double kWalkTimeConstMs = 250.0;
constexpr double kReaimIntervalMs = 400.0;

struct LatencyModel {
    double currentMs = 0.0;
    double targetMs = 0.0;
    std::uint64_t lastAdmitMs = 0;
    std::uint64_t lastDeadlineMs = 0;
    std::uint32_t rngState = 0x9E3779B9u;
    
    
    
    
    std::uint32_t admitsSinceService = 0;
    double arrivalsPerService = 1.0;
};

LatencyModel g_latency;

std::uint32_t nextRandom(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}



double randUnit(std::uint32_t& state) {
    return static_cast<double>(nextRandom(state) >> 8) * (1.0 / 16777216.0);
}


std::uint64_t nextReleaseDeadlineLocked(std::uint64_t admitMs) {
    const double base = static_cast<double>(g_retentionMs.load(std::memory_order_acquire));
    if (base <= 0.0) {
        g_latency.lastAdmitMs = admitMs;
        g_latency.lastDeadlineMs = admitMs;
        return admitMs;
    }

    const double band = base * kJitterFraction;
    if (g_latency.lastAdmitMs == 0) {
        g_latency.currentMs = base;
        g_latency.targetMs = base;
    } else {
        const double dt = static_cast<double>(admitMs - g_latency.lastAdmitMs);
        
        
        if (randUnit(g_latency.rngState) < dt / kReaimIntervalMs)
            g_latency.targetMs = base + (randUnit(g_latency.rngState) * 2.0 - 1.0) * band;
        
        const double alpha = 1.0 - std::exp(-dt / kWalkTimeConstMs);
        g_latency.currentMs += (g_latency.targetMs - g_latency.currentMs) * alpha;
    }

    
    
    const double noise = (randUnit(g_latency.rngState) * 2.0 - 1.0) * band * 0.15;
    double delay = g_latency.currentMs + noise;
    delay = std::clamp(delay, base * (1.0 - kJitterFraction), base * (1.0 + kJitterFraction));

    std::uint64_t deadline = admitMs + static_cast<std::uint64_t>(delay + 0.5);
    if (g_latency.lastAdmitMs != 0 && deadline < g_latency.lastDeadlineMs)
        deadline = g_latency.lastDeadlineMs;  
    g_latency.lastAdmitMs = admitMs;
    g_latency.lastDeadlineMs = deadline;
    return deadline;
}

void resetLatencyModelLocked() {
    g_latency.currentMs = 0.0;
    g_latency.targetMs = 0.0;
    g_latency.lastAdmitMs = 0;
    g_latency.lastDeadlineMs = 0;
    g_latency.admitsSinceService = 0;
    g_latency.arrivalsPerService = 1.0;
}





int releaseBudgetLocked() {
    const double arrivals = static_cast<double>(g_latency.admitsSinceService);
    
    g_latency.arrivalsPerService += (arrivals - g_latency.arrivalsPerService) * 0.25;
    g_latency.admitsSinceService = 0;
    const double allowance = std::ceil(g_latency.arrivalsPerService * 2.0);
    if (allowance <= 2.0)
        return 2;
    if (allowance >= static_cast<double>(kMaxReplayPerTick))
        return kMaxReplayPerTick;
    return static_cast<int>(allowance);
}

ReplayMarker g_replay;
jobject g_connectionHandler = nullptr;
std::uint64_t g_connectionEpoch = 0;
BacktrackGate::TargetSnapshot g_snapshot;
std::uint64_t g_snapshotSequence = 0;

struct ProcessedBase {
    bool valid = false;
    int entityId = -1;
    std::uint64_t engagementEpoch = 0;
    std::uint64_t connectionEpoch = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

ProcessedBase g_processedBase;
std::mutex g_stateMutex;
std::mutex g_replayMutex;

struct CallbackScope {
    CallbackScope() {
        g_callbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    }
    ~CallbackScope() {
        g_callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void clearException(JNIEnv* env) {
    if (env && env->ExceptionCheck())
        env->ExceptionClear();
}

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

std::uint64_t nextSnapshotSequenceLocked() {
    ++g_snapshotSequence;
    if (g_snapshotSequence == 0)
        ++g_snapshotSequence;
    return g_snapshotSequence;
}

std::size_t pendingCountLocked() {
    return g_queue.size() + (g_replay.active ? 1u : 0u);
}

bool sameObject(JNIEnv* env, jobject first, jobject second) {
    if (!env || !first || !second)
        return false;
    const bool same = env->IsSameObject(first, second) == JNI_TRUE;
    clearException(env);
    return same;
}

void clearSnapshotLocked() {
    const std::uint64_t sequence = nextSnapshotSequenceLocked();
    g_snapshot = {};
    g_snapshot.entityId = -1;
    g_snapshot.sequence = sequence;
    g_processedBase = {};
}

bool snapshotMatchesLocked(int entityId, std::uint64_t engagementEpoch) {
    return g_snapshot.entityId == entityId &&
           g_snapshot.engagementEpoch == engagementEpoch;
}

void refreshSnapshotPendingLocked() {
    std::size_t pending = 0;
    if (g_replay.active &&
        snapshotMatchesLocked(g_replay.entityId, g_replay.engagementEpoch)) {
        ++pending;
    }
    for (const HeldPacket& held : g_queue) {
        if (snapshotMatchesLocked(held.entityId, held.engagementEpoch))
            ++pending;
    }
    const std::size_t maximum =
        static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
    g_snapshot.pendingPackets = static_cast<std::uint32_t>((std::min)(pending, maximum));
}

bool checkedAdd(std::int32_t base, std::int32_t delta, std::int32_t& result) {
    const std::int64_t sum = static_cast<std::int64_t>(base) +
                             static_cast<std::int64_t>(delta);
    if (sum < static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) ||
        sum > static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)())) {
        return false;
    }
    result = static_cast<std::int32_t>(sum);
    return true;
}

bool applyMovement(const Movement& movement, std::int32_t& x,
                   std::int32_t& y, std::int32_t& z) {
    if (movement.kind == MovementKind::Teleport) {
        x = movement.x;
        y = movement.y;
        z = movement.z;
        return true;
    }

    std::int32_t nextX = 0;
    std::int32_t nextY = 0;
    std::int32_t nextZ = 0;
    if (!checkedAdd(x, movement.x, nextX) ||
        !checkedAdd(y, movement.y, nextY) ||
        !checkedAdd(z, movement.z, nextZ)) {
        return false;
    }
    x = nextX;
    y = nextY;
    z = nextZ;
    return true;
}

void foldSnapshotLocked(const Movement& movement) {
    if (!g_snapshot.valid && movement.kind != MovementKind::Teleport)
        return;
    if (!applyMovement(movement, g_snapshot.serverX, g_snapshot.serverY,
                       g_snapshot.serverZ)) {
        g_snapshot.valid = false;
        g_emergencyDrain.store(true, std::memory_order_release);
        return;
    }
    g_snapshot.valid = true;
}

void rebuildSnapshotLocked() {
    if (!g_processedBase.valid) {
        g_snapshot.valid = false;
        g_snapshot.pendingPackets = 0;
        g_snapshot.sequence = nextSnapshotSequenceLocked();
        return;
    }

    g_snapshot = {};
    g_snapshot.valid = true;
    g_snapshot.entityId = g_processedBase.entityId;
    g_snapshot.serverX = g_processedBase.x;
    g_snapshot.serverY = g_processedBase.y;
    g_snapshot.serverZ = g_processedBase.z;
    g_snapshot.engagementEpoch = g_processedBase.engagementEpoch;
    g_snapshot.connectionEpoch = g_processedBase.connectionEpoch;

    if (!g_queue.empty())
        g_snapshot.connectionEpoch = g_queue.back().connectionEpoch;

    
    
    
    const bool wholeStream = g_wholeStream.load(std::memory_order_acquire);
    const bool replayIsBase = g_replay.active &&
                              g_replay.entityId == g_processedBase.entityId &&
                              g_replay.engagementEpoch == g_processedBase.engagementEpoch;
    if (replayIsBase) {
        if (!applyMovement(g_replay.movement, g_snapshot.serverX, g_snapshot.serverY,
                           g_snapshot.serverZ)) {
            g_snapshot.valid = false;
        }
    } else if (g_replay.active &&
               (!wholeStream ||
                g_replay.engagementEpoch != g_processedBase.engagementEpoch)) {
        g_snapshot.valid = false;
    }
    if (g_replay.active)
        g_snapshot.connectionEpoch = g_replay.connectionEpoch;
    for (const HeldPacket& held : g_queue) {
        const bool sameEntity = held.entityId == g_processedBase.entityId;
        const bool sameEpoch = held.engagementEpoch == g_processedBase.engagementEpoch;
        if (!sameEpoch || (!sameEntity && !wholeStream)) {
            g_snapshot.valid = false;
            continue;
        }
        if (!sameEntity || !g_snapshot.valid)
            continue;
        foldSnapshotLocked(held.movement);
    }
    refreshSnapshotPendingLocked();
    g_snapshot.sequence = nextSnapshotSequenceLocked();
}

void publishMovementLocked(const HeldPacket& held) {
    
    
    
    if (g_wholeStream.load(std::memory_order_acquire) && g_processedBase.valid &&
        g_processedBase.engagementEpoch == held.engagementEpoch) {
        rebuildSnapshotLocked();
        return;
    }
    if (!g_processedBase.valid ||
        g_processedBase.entityId != held.entityId ||
        g_processedBase.engagementEpoch != held.engagementEpoch) {
        g_snapshot.valid = false;
        g_snapshot.entityId = held.entityId;
        g_snapshot.engagementEpoch = held.engagementEpoch;
        g_snapshot.connectionEpoch = held.connectionEpoch;
        g_snapshot.pendingPackets = 0;
        g_snapshot.sequence = nextSnapshotSequenceLocked();
        return;
    }
    rebuildSnapshotLocked();
}

bool prefixForEntityLocked(JNIEnv* env, jobject sourceHandler, int entityId,
                           std::uint64_t& engagementEpoch) {
    if (!sameObject(env, sourceHandler, g_connectionHandler))
        return false;
    
    
    
    const bool wholeStream = g_wholeStream.load(std::memory_order_acquire);
    if (g_replay.active && (wholeStream || g_replay.entityId == entityId)) {
        engagementEpoch = g_replay.engagementEpoch;
        return true;
    }
    for (const HeldPacket& held : g_queue) {
        if (wholeStream || held.entityId == entityId) {
            engagementEpoch = held.engagementEpoch;
            return true;
        }
    }
    return false;
}





bool shouldRetainLocked(JNIEnv* env, jobject source, int entityId, bool& prefix,
                        std::uint64_t& streamEpoch) {
    const Backtrack::HoldIntent intent =
        ::backtrack ? ::backtrack->holdIntent() : Backtrack::HoldIntent{};
    prefix = prefixForEntityLocked(env, source, entityId, streamEpoch);
    
    
    
    
    const bool opens = g_wholeStream.load(std::memory_order_acquire)
                           ? intent.streamHold
                           : (intent.active && intent.targetEntityId == entityId);
    const bool newAdmission = g_operational.load(std::memory_order_acquire) && opens &&
                              pendingCountLocked() == 0;
    if (!prefix)
        streamEpoch = intent.engagementEpoch;
    return prefix || newAdmission;
}

jfieldID resolveField(JNIEnv* env, jclass cls,
                      std::initializer_list<const char*> names,
                      const char* signature) {
    if (!env || !cls)
        return nullptr;
    for (const char* name : names) {
        jfieldID field = env->GetFieldID(cls, name, signature);
        if (field)
            return field;
        clearException(env);
    }
    return nullptr;
}










jfieldID resolveFieldOrdered(JNIEnv* env, jclass cls, const char* label,
                             std::initializer_list<const char*> names,
                             const char* signature, std::size_t index) {
    jfieldID field = resolveField(env, cls, names, signature);
    if (!field) {
        const std::vector<jfieldID> ordered =
            jni_resolver::findFieldsBySignature(cls, signature, false);
        if (index < ordered.size()) {
            field = ordered[index];
            log_warn("[BacktrackGate] %s resolved by descriptor order (%s #%u)", label,
                     signature, static_cast<unsigned>(index));
        }
    }
    clearException(env);
    if (!field)
        log_err("[BacktrackGate] Unresolved member: %s (%s)", label, signature);
    return field;
}

jmethodID resolveProcessMethod(JNIEnv* env, jclass cls) {
    if (!env || !cls)
        return nullptr;

    for (const char* descriptor : {
             "(Lnet/minecraft/network/INetHandler;)V",
             "(Lnet/minecraft/network/play/INetHandlerPlayClient;)V"}) {
        const auto variants = jni_resolver::signatureVariants(descriptor);
        for (const char* name : {"processPacket", "func_148833_a", "a"}) {
            for (const std::string& variant : variants) {
                jmethodID method = env->GetMethodID(cls, name, variant.c_str());
                if (method)
                    return method;
                clearException(env);
            }
        }
        jmethodID method =
            jni_resolver::findUniqueMethodBySignature(cls, descriptor, false);
        clearException(env);
        if (method)
            return method;
    }

    
    
    
    
    
    
    
    if (Java::tiEnv) {
        jint count = 0;
        jmethodID* methods = nullptr;
        if (Java::tiEnv->GetClassMethods(cls, &count, &methods) == JVMTI_ERROR_NONE && methods) {
            jmethodID match = nullptr;
            jmethodID bridgeMatch = nullptr;
            for (jint i = 0; i < count && !match; ++i) {
                char* name = nullptr;
                char* sig = nullptr;
                jint modifiers = 0;
                if (Java::tiEnv->GetMethodName(methods[i], &name, &sig, nullptr) == JVMTI_ERROR_NONE) {
                    Java::tiEnv->GetMethodModifiers(methods[i], &modifiers);
                    const std::size_t len = sig ? std::strlen(sig) : 0;
                    const bool oneObjectArgVoid =
                        len > 4 && sig[0] == '(' && sig[1] == 'L' && sig[len - 3] == ';' &&
                        sig[len - 2] == ')' && sig[len - 1] == 'V' &&
                        std::strchr(sig, ';') == sig + (len - 3);
                    if ((modifiers & 0x0008) == 0 && oneObjectArgVoid &&
                        std::strstr(sig, "NetHandler") != nullptr) {
                        
                        
                        
                        if (modifiers & 0x0040) {
                            if (!bridgeMatch)
                                bridgeMatch = methods[i];
                        } else {
                            match = methods[i];
                            log_warn("[BacktrackGate] processPacket resolved structurally: %s%s",
                                     name ? name : "?", sig);
                        }
                    }
                }
                if (name)
                    Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(name));
                if (sig)
                    Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(sig));
            }
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(methods));
            if (!match && bridgeMatch) {
                match = bridgeMatch;
                log_warn("[BacktrackGate] processPacket resolved structurally via bridge method");
            }
            if (match)
                return match;
        }
    }
    return nullptr;
}

void resetResolvedIds(JNIEnv* env) {
    if (env) {
        if (g_s14)
            env->DeleteGlobalRef(g_s14);
        if (g_s18)
            env->DeleteGlobalRef(g_s18);
        if (g_networkManager)
            env->DeleteGlobalRef(g_networkManager);
        clearException(env);
    }
    g_s14 = nullptr;
    g_s18 = nullptr;
    g_networkManager = nullptr;
    g_packetListener = nullptr;
    g_s14EntityId = nullptr;
    g_s14X = nullptr;
    g_s14Y = nullptr;
    g_s14Z = nullptr;
    g_s18EntityId = nullptr;
    g_s18X = nullptr;
    g_s18Y = nullptr;
    g_s18Z = nullptr;
    g_s14Process = nullptr;
    g_s18Process = nullptr;
}

bool resolveMovementIds(JNIEnv* env) {
    jclass localS14 = nullptr;
    jclass localS18 = nullptr;
    const bool haveS14 =
        Java::AssignClass("net.minecraft.network.play.server.S14PacketEntity", localS14) && localS14;
    clearException(env);
    const bool haveS18 =
        Java::AssignClass("net.minecraft.network.play.server.S18PacketEntityTeleport", localS18) &&
        localS18;
    clearException(env);
    if (!haveS14 || !haveS18) {
        if (!haveS14)
            log_err("[BacktrackGate] S14PacketEntity is not loadable through the game class loader");
        if (!haveS18)
            log_err("[BacktrackGate] S18PacketEntityTeleport is not loadable through the game class loader");
        if (localS14)
            env->DeleteLocalRef(localS14);
        if (localS18)
            env->DeleteLocalRef(localS18);
        clearException(env);
        return false;
    }

    g_s14 = static_cast<jclass>(env->NewGlobalRef(localS14));
    g_s18 = static_cast<jclass>(env->NewGlobalRef(localS18));
    env->DeleteLocalRef(localS14);
    env->DeleteLocalRef(localS18);
    clearException(env);
    if (!g_s14 || !g_s18)
        return false;

    g_s14EntityId = resolveFieldOrdered(env, g_s14, "S14.entityId",
                                        {"entityId", "field_149074_a", "a"}, "I", 0);
    g_s14X = resolveFieldOrdered(env, g_s14, "S14.posX", {"posX", "field_149072_b", "b"}, "B", 0);
    g_s14Y = resolveFieldOrdered(env, g_s14, "S14.posY", {"posY", "field_149073_c", "c"}, "B", 1);
    g_s14Z = resolveFieldOrdered(env, g_s14, "S14.posZ", {"posZ", "field_149070_d", "d"}, "B", 2);
    g_s18EntityId = resolveFieldOrdered(env, g_s18, "S18.entityId",
                                        {"entityId", "field_149458_a", "a"}, "I", 0);
    g_s18X = resolveFieldOrdered(env, g_s18, "S18.posX", {"posX", "x", "field_149456_b", "b"}, "I", 1);
    g_s18Y = resolveFieldOrdered(env, g_s18, "S18.posY", {"posY", "y", "field_149457_c", "c"}, "I", 2);
    g_s18Z = resolveFieldOrdered(env, g_s18, "S18.posZ", {"posZ", "z", "field_149454_d", "d"}, "I", 3);
    g_s14Process = resolveProcessMethod(env, g_s14);
    g_s18Process = resolveProcessMethod(env, g_s18);
    if (!g_s14Process)
        log_err("[BacktrackGate] Unresolved member: S14.processPacket");
    if (!g_s18Process)
        log_err("[BacktrackGate] Unresolved member: S18.processPacket");

    return g_s14EntityId && g_s14X && g_s14Y && g_s14Z &&
           g_s18EntityId && g_s18X && g_s18Y && g_s18Z &&
           g_s14Process && g_s18Process;
}

bool resolvePacketListener(JNIEnv* env) {
    jclass local = nullptr;
    if (!Java::AssignClass("net.minecraft.network.NetworkManager", local) || !local) {
        clearException(env);
        return false;
    }
    g_networkManager = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    clearException(env);
    if (!g_networkManager)
        return false;

    const auto signatures =
        jni_resolver::signatureVariants("Lnet/minecraft/network/INetHandler;");
    for (const char* name : {"packetListener", "field_150744_m", "m"}) {
        for (const std::string& signature : signatures) {
            g_packetListener =
                env->GetFieldID(g_networkManager, name, signature.c_str());
            if (g_packetListener)
                return true;
            clearException(env);
        }
    }
    g_packetListener = jni_resolver::findUniqueFieldBySignature(
        g_networkManager, "Lnet/minecraft/network/INetHandler;", false);
    clearException(env);
    return g_packetListener != nullptr;
}

bool decodeMovement(JNIEnv* env, jobject packet, int& entityId,
                    Movement& movement) {
    if (env->IsInstanceOf(packet, g_s14) == JNI_TRUE) {
        entityId = env->GetIntField(packet, g_s14EntityId);
        movement.kind = MovementKind::Relative;
        movement.x = static_cast<std::int32_t>(env->GetByteField(packet, g_s14X));
        movement.y = static_cast<std::int32_t>(env->GetByteField(packet, g_s14Y));
        movement.z = static_cast<std::int32_t>(env->GetByteField(packet, g_s14Z));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }
        return true;
    }
    clearException(env);

    if (env->IsInstanceOf(packet, g_s18) == JNI_TRUE) {
        entityId = env->GetIntField(packet, g_s18EntityId);
        movement.kind = MovementKind::Teleport;
        movement.x = static_cast<std::int32_t>(env->GetIntField(packet, g_s18X));
        movement.y = static_cast<std::int32_t>(env->GetIntField(packet, g_s18Y));
        movement.z = static_cast<std::int32_t>(env->GetIntField(packet, g_s18Z));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }
        return true;
    }
    clearException(env);
    return false;
}

jobject sourceHandler(JNIEnv* env, jobject networkManager) {
    if (!env || !networkManager || !g_networkManager || !g_packetListener ||
        env->IsInstanceOf(networkManager, g_networkManager) != JNI_TRUE) {
        clearException(env);
        return nullptr;
    }
    jobject handler = env->GetObjectField(networkManager, g_packetListener);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (handler)
            env->DeleteLocalRef(handler);
        return nullptr;
    }
    return handler;
}

jobject currentHandler(JNIEnv* env) {
    if (!env)
        return nullptr;
    jobject handler = mc.getNetHandler();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (handler)
            env->DeleteLocalRef(handler);
        return nullptr;
    }
    return handler;
}

DetachedState detachLocked() {
    DetachedState detached;
    detached.packets.swap(g_queue);
    detached.handler = std::exchange(g_connectionHandler, nullptr);
    g_replay = {};
    clearSnapshotLocked();
    
    
    resetLatencyModelLocked();
    g_emergencyDrain.store(false, std::memory_order_release);
    return detached;
}

void deleteDetached(JNIEnv* env, DetachedState& detached) {
    if (!env)
        return;
    for (HeldPacket& held : detached.packets) {
        if (held.packet)
            env->DeleteGlobalRef(held.packet);
        held.packet = nullptr;
        clearException(env);
    }
    detached.packets.clear();
    if (detached.handler) {
        env->DeleteGlobalRef(detached.handler);
        detached.handler = nullptr;
        clearException(env);
    }
}

enum class ReplayResult { Sent, HandlerThrew, ConnectionChanged, Discarded };

ReplayResult replayPacket(JNIEnv* env, const HeldPacket& held,
                          jobject expectedHandler) {
    if (g_discarding.load(std::memory_order_acquire))
        return ReplayResult::Discarded;

    jobject liveHandler = currentHandler(env);
    if (!liveHandler)
        return ReplayResult::ConnectionChanged;
    if (!sameObject(env, liveHandler, expectedHandler)) {
        env->DeleteLocalRef(liveHandler);
        return ReplayResult::ConnectionChanged;
    }

    const jmethodID method = held.movement.kind == MovementKind::Teleport
                                 ? g_s18Process
                                 : g_s14Process;
    env->CallVoidMethod(held.packet, method, liveHandler);
    const bool threw = env->ExceptionCheck() == JNI_TRUE;
    clearException(env);
    env->DeleteLocalRef(liveHandler);
    clearException(env);
    return threw ? ReplayResult::HandlerThrew : ReplayResult::Sent;
}

bool worldPositionToProtocol(double value, std::int32_t& result) {
    if (!std::isfinite(value))
        return false;
    const double scaled = std::round(value * 32.0);
    if (scaled < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
        scaled > static_cast<double>((std::numeric_limits<std::int32_t>::max)())) {
        return false;
    }
    result = static_cast<std::int32_t>(scaled);
    return true;
}

bool readEntityBase(JNIEnv* env, jobject entity, std::int32_t& x,
                    std::int32_t& y, std::int32_t& z, bool& protocolExact) {
    protocolExact = false;
    if (!env || !entity || !JniCache::cls_Entity ||
        env->IsInstanceOf(entity, JniCache::cls_Entity) != JNI_TRUE) {
        clearException(env);
        return false;
    }
    clearException(env);

    if (JniCache::fid_serverPosX && JniCache::fid_serverPosY &&
        JniCache::fid_serverPosZ) {
        x = static_cast<std::int32_t>(env->GetIntField(entity, JniCache::fid_serverPosX));
        y = static_cast<std::int32_t>(env->GetIntField(entity, JniCache::fid_serverPosY));
        z = static_cast<std::int32_t>(env->GetIntField(entity, JniCache::fid_serverPosZ));
        if (!env->ExceptionCheck()) {
            protocolExact = true;
            return true;
        }
        env->ExceptionClear();
    }

    if (!JniCache::fid_posX || !JniCache::fid_posY || !JniCache::fid_posZ)
        return false;
    const double worldX = env->GetDoubleField(entity, JniCache::fid_posX);
    const double worldY = env->GetDoubleField(entity, JniCache::fid_posY);
    const double worldZ = env->GetDoubleField(entity, JniCache::fid_posZ);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return worldPositionToProtocol(worldX, x) &&
           worldPositionToProtocol(worldY, y) &&
           worldPositionToProtocol(worldZ, z);
}

}  

namespace BacktrackGate {

jboolean JNICALL NativeCanProcess(JNIEnv* env, jclass, jobject networkManager,
                                  jobject packet) {
    CallbackScope callback;
    if (!env || !packet || !g_initialized.load(std::memory_order_acquire) ||
        g_shuttingDown.load(std::memory_order_acquire) ||
        g_discarding.load(std::memory_order_acquire)) {
        return JNI_TRUE;
    }

    if (!g_callbackObserved.exchange(true, std::memory_order_acq_rel))
        log_debug("[BacktrackGate] Incoming hook callback observed");

    int entityId = -1;
    Movement movement;
    if (!decodeMovement(env, packet, entityId, movement) || entityId < 0)
        return JNI_TRUE;

    jobject source = sourceHandler(env, networkManager);
    if (!source) {
        bool ownedStream = false;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            ownedStream = pendingCountLocked() != 0;
        }
        if (!g_missingSourceReported.exchange(true, std::memory_order_acq_rel))
            log_err("[BacktrackGate] Incoming callback has no packetListener; retention cannot continue");
        if (ownedStream) {
            g_operational.store(false, std::memory_order_release);
            Discard(env);
            log_err("[BacktrackGate] Aborted retained stream because source ownership was lost");
        }
        return JNI_TRUE;
    }

    jobject live = currentHandler(env);
    if (!live || !sameObject(env, source, live)) {
        if (live)
            env->DeleteLocalRef(live);
        env->DeleteLocalRef(source);
        clearException(env);
        return JNI_TRUE;
    }
    env->DeleteLocalRef(live);

    bool shouldHold = false;
    bool prefix = false;
    bool capacityAbort = false;
    std::uint64_t streamEpoch = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        shouldHold = shouldRetainLocked(env, source, entityId, prefix, streamEpoch);
        capacityAbort = shouldHold && pendingCountLocked() >= kHardQueueLimit;
    }
    if (capacityAbort) {
        env->DeleteLocalRef(source);
        clearException(env);
        g_operational.store(false, std::memory_order_release);
        Discard(env);
        log_err("[BacktrackGate] Aborted retained stream at the hard queue limit");
        return JNI_TRUE;
    }
    if (!shouldHold) {
        env->DeleteLocalRef(source);
        clearException(env);
        return JNI_TRUE;
    }

    jobject packetGlobal = env->NewGlobalRef(packet);
    if (!packetGlobal) {
        clearException(env);
        env->DeleteLocalRef(source);
        g_operational.store(false, std::memory_order_release);
        Discard(env);
        log_err("[BacktrackGate] Aborted retained stream after packet ownership allocation failed");
        return JNI_TRUE;
    }

    bool queued = false;
    bool commitAbort = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const bool retain = shouldRetainLocked(env, source, entityId, prefix, streamEpoch);

        if (!g_discarding.load(std::memory_order_acquire) && retain) {
            if (pendingCountLocked() >= kHardQueueLimit) {
                commitAbort = true;
            } else {
                if (!g_connectionHandler) {
                    g_connectionHandler = env->NewGlobalRef(source);
                    clearException(env);
                    if (g_connectionHandler) {
                        ++g_connectionEpoch;
                        if (g_connectionEpoch == 0)
                            ++g_connectionEpoch;
                    } else {
                        commitAbort = true;
                    }
                }

                if (!commitAbort && sameObject(env, source, g_connectionHandler)) {
                    const std::uint64_t admitMs = nowMs();
                    g_queue.push_back({packetGlobal, entityId, streamEpoch,
                                       g_connectionEpoch, admitMs,
                                       nextReleaseDeadlineLocked(admitMs), movement});
                    packetGlobal = nullptr;
                    ++g_latency.admitsSinceService;
                    publishMovementLocked(g_queue.back());
                    queued = true;
                    
                    
                    
                    
                    const std::size_t cap =
                        g_softQueueLimit.load(std::memory_order_acquire);
                    const std::size_t pending = pendingCountLocked();
                    const bool overflow =
                        g_wholeStream.load(std::memory_order_acquire)
                            ? (pending >= kStreamQueueCeiling ||
                               static_cast<std::size_t>(g_snapshot.pendingPackets) >= cap)
                            : (pending >= cap);
                    if (overflow)
                        g_emergencyDrain.store(true, std::memory_order_release);
                } else if (!commitAbort && prefix) {
                    commitAbort = true;
                }
            }
        }
    }

    if (packetGlobal)
        env->DeleteGlobalRef(packetGlobal);
    env->DeleteLocalRef(source);
    clearException(env);

    if (commitAbort) {
        g_operational.store(false, std::memory_order_release);
        Discard(env);
        log_err("[BacktrackGate] Aborted retained stream before ownership commit");
        return JNI_TRUE;
    }
    if (queued && !g_firstPacketReported.exchange(true, std::memory_order_acq_rel))
        log_ok("[BacktrackGate] First movement packet retained");

    
    
    return queued ? JNI_FALSE : JNI_TRUE;
}

bool PrimeTarget(JNIEnv* env, jobject entity, int entityId,
                 std::uint64_t engagementEpoch) {
    if (!env || !entity || entityId < 0 || engagementEpoch == 0)
        return false;

    std::int32_t baseX = 0;
    std::int32_t baseY = 0;
    std::int32_t baseZ = 0;
    bool protocolExact = false;
    const bool readBase = readEntityBase(env, entity, baseX, baseY, baseZ,
                                         protocolExact);

    std::lock_guard<std::mutex> replayLock(g_replayMutex);
    std::lock_guard<std::mutex> stateLock(g_stateMutex);

    
    
    
    
    
    
    const bool wholeStream = g_wholeStream.load(std::memory_order_acquire);
    bool hasDifferentPrefix = false;
    if (g_replay.active) {
        hasDifferentPrefix = g_replay.engagementEpoch != engagementEpoch ||
                             (!wholeStream && g_replay.entityId != entityId);
    }
    for (const HeldPacket& held : g_queue) {
        if (held.engagementEpoch != engagementEpoch ||
            (!wholeStream && held.entityId != entityId)) {
            hasDifferentPrefix = true;
            break;
        }
    }
    if (hasDifferentPrefix)
        return false;

    if (!g_processedBase.valid ||
        g_processedBase.entityId != entityId ||
        g_processedBase.engagementEpoch != engagementEpoch) {
        if (!readBase)
            return false;
        g_processedBase.valid = true;
        g_processedBase.entityId = entityId;
        g_processedBase.engagementEpoch = engagementEpoch;
        g_processedBase.connectionEpoch = g_connectionEpoch;
        g_processedBase.x = baseX;
        g_processedBase.y = baseY;
        g_processedBase.z = baseZ;
    }
    (void)protocolExact;
    rebuildSnapshotLocked();
    return g_snapshot.valid;
}

int ServiceClientThread(JNIEnv* env, bool flushAll) {
    if (!env)
        return 0;

    std::lock_guard<std::mutex> replayLock(g_replayMutex);
    int sent = 0;

    
    
    
    
    
    
    
    
    int budget = kMaxReplayPerTick;
    if (!flushAll && !g_emergencyDrain.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        budget = releaseBudgetLocked();
    }

    for (; sent < budget; ++sent) {
        HeldPacket held;
        jobject expectedHandler = nullptr;
        jobject idleHandler = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            if (g_queue.empty()) {
                if (!g_replay.active && g_connectionHandler) {
                    idleHandler = std::exchange(g_connectionHandler, nullptr);
                    g_emergencyDrain.store(false, std::memory_order_release);
                    if (flushAll)
                        clearSnapshotLocked();
                }
            } else {
                
                
                
                const bool draining =
                    flushAll || g_emergencyDrain.load(std::memory_order_acquire);
                if (draining || nowMs() >= g_queue.front().releaseAtMs) {
                    held = std::move(g_queue.front());
                    g_queue.pop_front();
                    g_replay = {true, held.entityId, held.engagementEpoch,
                                held.connectionEpoch, held.movement};
                    expectedHandler = g_connectionHandler;
                    rebuildSnapshotLocked();
                }
            }
        }

        if (idleHandler) {
            env->DeleteGlobalRef(idleHandler);
            clearException(env);
        }
        if (!held.packet)
            break;

        const ReplayResult result = replayPacket(env, held, expectedHandler);
        if (result == ReplayResult::Sent) {
            env->DeleteGlobalRef(held.packet);
            held.packet = nullptr;
            clearException(env);

            jobject handlerToDelete = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                const bool ownsBase = g_processedBase.valid &&
                                      g_processedBase.entityId == held.entityId &&
                                      g_processedBase.engagementEpoch == held.engagementEpoch;
                
                
                
                
                
                const bool inert = !ownsBase &&
                                   g_wholeStream.load(std::memory_order_acquire) &&
                                   (!g_processedBase.valid ||
                                    g_processedBase.engagementEpoch == held.engagementEpoch);
                if (!inert &&
                    (!ownsBase || !applyMovement(held.movement, g_processedBase.x,
                                                 g_processedBase.y, g_processedBase.z))) {
                    g_processedBase.valid = false;
                    g_snapshot.valid = false;
                    g_emergencyDrain.store(true, std::memory_order_release);
                }
                g_processedBase.connectionEpoch = held.connectionEpoch;
                g_replay = {};
                rebuildSnapshotLocked();
                if (pendingCountLocked() == 0) {
                    g_emergencyDrain.store(false, std::memory_order_release);
                    handlerToDelete = std::exchange(g_connectionHandler, nullptr);
                    if (flushAll)
                        clearSnapshotLocked();
                }
            }
            if (handlerToDelete) {
                env->DeleteGlobalRef(handlerToDelete);
                clearException(env);
            }
            continue;
        }

        DetachedState detached;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_replay = {};
            detached = detachLocked();
        }
        if (held.packet) {
            env->DeleteGlobalRef(held.packet);
            held.packet = nullptr;
            clearException(env);
        }
        deleteDetached(env, detached);

        if (result == ReplayResult::HandlerThrew) {
            g_operational.store(false, std::memory_order_release);
            log_err("[BacktrackGate] Packet handler threw; ownership root discarded and retention disabled");
        } else if (result == ReplayResult::ConnectionChanged) {
            log_warn("[BacktrackGate] Connection changed; discarded retained movement");
        }
        break;
    }
    return sent;
}

TargetSnapshot GetTargetSnapshot() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_snapshot;
}

Status GetStatus() {
    Status status;
    status.ready = g_initialized.load(std::memory_order_acquire);
    status.operational = g_operational.load(std::memory_order_acquire);
    status.callbackObserved = g_callbackObserved.load(std::memory_order_acquire);
    std::lock_guard<std::mutex> lock(g_stateMutex);
    status.snapshotValid = g_snapshot.valid;
    const std::size_t count = pendingCountLocked();
    const std::size_t maximum =
        static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
    status.pendingPackets = static_cast<std::uint32_t>((std::min)(count, maximum));
    return status;
}

void Discard(JNIEnv* env) {
    if (!env)
        return;
    g_discarding.store(true, std::memory_order_release);
    DetachedState detached;
    {
        std::lock_guard<std::mutex> replayLock(g_replayMutex);
        std::lock_guard<std::mutex> stateLock(g_stateMutex);
        detached = detachLocked();
    }
    deleteDetached(env, detached);
    g_discarding.store(false, std::memory_order_release);
}

bool HasPending() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return pendingCountLocked() != 0;
}

bool HasPendingForCurrentConnection(JNIEnv* env) {
    if (!env)
        return false;

    jobject live = currentHandler(env);
    bool current = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        current = pendingCountLocked() != 0 &&
                  sameObject(env, live, g_connectionHandler);
    }
    if (live)
        env->DeleteLocalRef(live);
    clearException(env);
    return current;
}

bool NeedsEmergencyDrain() {
    return g_emergencyDrain.load(std::memory_order_acquire);
}

bool Init(JNIEnv* env) {
    if (g_initialized.load(std::memory_order_acquire))
        return true;
    if (!env)
        return false;

    g_shuttingDown.store(false, std::memory_order_release);
    g_operational.store(false, std::memory_order_release);
    resetResolvedIds(env);

    if (!resolveMovementIds(env)) {
        log_err("[BacktrackGate] Init failed: movement fields or processPacket methods unresolved");
        resetResolvedIds(env);
        return false;
    }
    if (!resolvePacketListener(env)) {
        log_err("[BacktrackGate] Init failed: NetworkManager.packetListener unresolved");
        resetResolvedIds(env);
        return false;
    }

    jclass hook = nullptr;
    if (!Java::AssignClass("com.cobble.agent.IncomingHook", hook))
        clearException(env);
    if (!hook && Java::ClassLoader) {
        hook = env->DefineClass("com/cobble/agent/IncomingHook", Java::ClassLoader,
                                reinterpret_cast<const jbyte*>(kIncomingHookBytecode),
                                static_cast<jint>(sizeof(kIncomingHookBytecode)));
        clearException(env);
    }
    if (!hook) {
        log_err("[BacktrackGate] Init failed: IncomingHook could not be defined");
        resetResolvedIds(env);
        return false;
    }

    JNINativeMethod method;
    method.name = const_cast<char*>("canProcess");
    method.signature = const_cast<char*>("(Ljava/lang/Object;Ljava/lang/Object;)Z");
    method.fnPtr = reinterpret_cast<void*>(&NativeCanProcess);
    if (env->RegisterNatives(hook, &method, 1) != JNI_OK) {
        clearException(env);
        env->DeleteLocalRef(hook);
        log_err("[BacktrackGate] Init failed: IncomingHook native registration failed");
        resetResolvedIds(env);
        return false;
    }

    g_incomingHook = static_cast<jclass>(env->NewGlobalRef(hook));
    clearException(env);
    if (!g_incomingHook) {
        env->UnregisterNatives(hook);
        clearException(env);
        env->DeleteLocalRef(hook);
        resetResolvedIds(env);
        return false;
    }
    env->DeleteLocalRef(hook);
    clearException(env);

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        clearSnapshotLocked();
    }
    g_callbackObserved.store(false, std::memory_order_release);
    g_firstPacketReported.store(false, std::memory_order_release);
    g_missingSourceReported.store(false, std::memory_order_release);
    g_emergencyDrain.store(false, std::memory_order_release);
    g_initialized.store(true, std::memory_order_release);
    log_ok("[BacktrackGate] Incoming bridge, movement decoder and connection source ready");
    return true;
}

bool Shutdown(JNIEnv* env, bool safeToUnregister) {
    g_operational.store(false, std::memory_order_release);
    g_shuttingDown.store(true, std::memory_order_release);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (g_callbacksInFlight.load(std::memory_order_acquire) != 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            log_err("[BacktrackGate] Timed out waiting for incoming callbacks");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!env || !safeToUnregister) {
        log_err("[BacktrackGate] Unsafe native shutdown refused");
        return false;
    }

    Discard(env);
    if (g_incomingHook) {
        const jint result = env->UnregisterNatives(g_incomingHook);
        const bool threw = env->ExceptionCheck() == JNI_TRUE;
        clearException(env);
        if (result != JNI_OK || threw) {
            log_err("[BacktrackGate] IncomingHook native unregister failed");
            return false;
        }
        env->DeleteGlobalRef(g_incomingHook);
        g_incomingHook = nullptr;
        clearException(env);
    }

    resetResolvedIds(env);
    g_initialized.store(false, std::memory_order_release);
    g_shuttingDown.store(false, std::memory_order_release);
    return true;
}

bool IsReady() {
    return g_initialized.load(std::memory_order_acquire);
}

void SetOperational(bool operational) {
    const bool next = operational && IsReady();
    const bool previous = g_operational.exchange(next, std::memory_order_acq_rel);
    if (next && !previous)
        log_ok("[BacktrackGate] Retention path operational");
    else if (!next && previous)
        log_warn("[BacktrackGate] Retention path disabled");
}

bool IsOperational() {
    return g_operational.load(std::memory_order_acquire);
}

void Configure(unsigned holdMs, unsigned maxHeldPackets, bool wholeStream) {
    std::uint64_t hold = static_cast<std::uint64_t>(holdMs);
    if (hold > kMaxRetentionMs)
        hold = kMaxRetentionMs;
    g_retentionMs.store(hold, std::memory_order_release);

    std::size_t cap = maxHeldPackets == 0u ? 1u : static_cast<std::size_t>(maxHeldPackets);
    if (cap > kHardQueueLimit)
        cap = kHardQueueLimit;
    g_softQueueLimit.store(cap, std::memory_order_release);

    g_wholeStream.store(wholeStream, std::memory_order_release);
}

}  
