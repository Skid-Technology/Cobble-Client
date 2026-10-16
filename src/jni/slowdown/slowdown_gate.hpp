#pragma once

#include "../../jni/java.h"

namespace SlowDownGate {



void JNICALL NativeOnSlowDown(JNIEnv* env, jclass, jobject playerInstance);


bool Init(JNIEnv* env);



bool Shutdown(JNIEnv* env, bool safeToUnregister = true);

}  
