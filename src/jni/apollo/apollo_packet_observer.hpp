#pragma once

#include "../java.h"

namespace ApolloPacketObserver {

void JNICALL NativeOnReceive(JNIEnv* env, jclass, jobject packet);


void BoostLogBudget(int count);

void ForceLogNametags(int count);


void SetDebugEntityEffects(bool enabled);


std::string GetCleanNameByUUID(const std::string& uuid);
void RegisterCleanName(const std::string& uuid, const std::string& name);
void ClearCleanNames();

}  
