#include "inventory_hooks.h"
#include "../../sdk/jni/jni_cache.h"
#include "../logger.hpp"

int InventoryHooks::getItemIdFromStack(JNIEnv* env, jobject itemStack) {
    if (!itemStack || !JniCache::mid_getItem || !JniCache::cls_Item || !JniCache::mid_getIdFromItem)
        return -1;

    jobject item = env->CallObjectMethod(itemStack, JniCache::mid_getItem);
    if (!item)
        return -1;

    int id = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, item);
    env->DeleteLocalRef(item);
    return id;
}

bool InventoryHooks::install() {
    if (!Java::tiEnv) {
        log_err("[InventoryHooks] JVMTI not available");
        return false;
    }

    
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    callbacks.MethodEntry = [](jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method) {
        char* methodName = nullptr;
        jvmti_env->GetMethodName(method, &methodName, nullptr, nullptr);

        if (methodName && strcmp(methodName, "setInventorySlotContents") == 0) {
            
            jclass declaringClass = nullptr;
            jvmti_env->GetMethodDeclaringClass(method, &declaringClass);

            if (declaringClass) {
                char* classSig = nullptr;
                jvmti_env->GetClassSignature(declaringClass, &classSig, nullptr);

                
                if (classSig && strstr(classSig, "InventoryPlayer")) {
                    
                    jint slot = -1;
                    jvmtiError slotErr = jvmti_env->GetLocalInt(thread, 0, 1, &slot);

                    jobject itemStack = nullptr;
                    jvmtiError stackErr = jvmti_env->GetLocalObject(thread, 0, 2, &itemStack);

                    if (slotErr == JVMTI_ERROR_NONE && stackErr == JVMTI_ERROR_NONE) {
                        
                        int itemId = -1;
                        if (itemStack) {
                            itemId = InventoryHooks::getItemIdFromStack(jni_env, itemStack);
                        }

                        
                        
                        (void)itemId;  

                        if (itemStack) {
                            jni_env->DeleteLocalRef(itemStack);
                        }
                    }
                }

                if (classSig)
                    jvmti_env->Deallocate((unsigned char*)classSig);
                jni_env->DeleteLocalRef(declaringClass);
            }
        }

        if (methodName)
            jvmti_env->Deallocate((unsigned char*)methodName);
    };

    
    jvmtiError err = Java::tiEnv->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        log_err("[InventoryHooks] Failed to set callbacks: %d", err);
        return false;
    }

    
    err = Java::tiEnv->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, nullptr);
    if (err != JVMTI_ERROR_NONE) {
        log_err("[InventoryHooks] Failed to enable Method Entry: %d", err);
        return false;
    }

    log_ok("[InventoryHooks] Installed successfully");
    return true;
}
