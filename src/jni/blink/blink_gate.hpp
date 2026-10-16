#pragma once

#include "../java.h"

namespace BlinkGate {



bool ShouldSendPacket(JNIEnv* env, jobject packet);


jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass, jobject packet);

}  
