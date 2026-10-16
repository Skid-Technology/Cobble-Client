#pragma once

#include "../java.h"

namespace NoItemReleaseGate {

jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass, jobject packet);

}  
