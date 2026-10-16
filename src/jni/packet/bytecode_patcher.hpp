#pragma once

#include "../java.h"

namespace BytecodePatcher {

struct HookTargets {
    bool outgoing = false;
    bool incoming = false;
    bool incomingBacktrack = false;  
    bool customPayload = false;
    bool teams = false;
    bool lunarNametags = false;
    bool slowdown = false;  
    bool renderGhost = false;  
    bool backtrack = false;  
};

void SetTargets(const HookTargets& targets);





void SetHideVanillaNames(bool on);
bool ApplyHideVanillaNames(JNIEnv* env);

bool RegisterHooks(JNIEnv* env, jboolean(JNICALL* shouldSend)(JNIEnv*, jclass, jobject),
                   void (JNICALL* onReceive)(JNIEnv*, jclass, jobject));

bool InstallBytecodeHook(JNIEnv* env);




bool IsIncomingBacktrackReady();



bool Shutdown(JNIEnv* env);

}  
