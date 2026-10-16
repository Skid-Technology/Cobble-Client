#pragma once

#include "../java.h"















namespace BacktrackGhostGate {



void JNICALL NativeOnRenderWorld(JNIEnv* env, jclass, jobject viewEntity);


bool Init(JNIEnv* env);



bool Shutdown(JNIEnv* env, bool safeToUnregister = true);

bool IsReady();

}  
