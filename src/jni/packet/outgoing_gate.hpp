#pragma once

#include "../java.h"






namespace OutgoingPacketGate {



jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass, jobject packet);

}  
