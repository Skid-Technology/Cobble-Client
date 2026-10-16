#pragma once

#include "../../jni/java.h"



namespace InventoryHooks {

bool install();


int getItemIdFromStack(JNIEnv* env, jobject itemStack);
}  
