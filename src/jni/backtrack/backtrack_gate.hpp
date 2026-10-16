#pragma once

#include <cstdint>

#include "../java.h"

namespace BacktrackGate {

struct TargetSnapshot {
    bool valid = false;
    int entityId = -1;
    std::int32_t serverX = 0;
    std::int32_t serverY = 0;
    std::int32_t serverZ = 0;
    std::uint64_t engagementEpoch = 0;
    std::uint64_t connectionEpoch = 0;
    std::uint64_t sequence = 0;
    std::uint32_t pendingPackets = 0;
};

struct Status {
    bool ready = false;
    bool operational = false;
    bool callbackObserved = false;
    bool snapshotValid = false;
    std::uint32_t pendingPackets = 0;
};

bool Init(JNIEnv* env);
bool Shutdown(JNIEnv* env, bool safeToUnregister);
bool IsReady();

void SetOperational(bool operational);
bool IsOperational();











void Configure(unsigned holdMs, unsigned maxHeldPackets, bool wholeStream);

jboolean JNICALL NativeCanProcess(JNIEnv* env, jclass cls, jobject networkManager,
                                  jobject packet);



bool PrimeTarget(JNIEnv* env, jobject entity, int entityId,
                 std::uint64_t engagementEpoch);



int ServiceClientThread(JNIEnv* env, bool flushAll);

TargetSnapshot GetTargetSnapshot();
Status GetStatus();
void Discard(JNIEnv* env);
bool HasPending();
bool HasPendingForCurrentConnection(JNIEnv* env);
bool NeedsEmergencyDrain();

}  
