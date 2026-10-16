#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "../../jni/java.h"
#include "../jni/member_resolver.h"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/geometry.h"


struct RenderData {
    static inline bool initialized = false;
    static inline bool updated = false;
    static inline bool initFailed = false;
    static inline int initRetryCount = 0;
    static inline std::chrono::steady_clock::time_point lastInitAttempt{};

    
    static inline Matrix modelView;
    static inline Matrix projection;

    
    static inline int screenWidth = 0;
    static inline int screenHeight = 0;

    
    
    static inline int gameFps = 0;

    
    
    
    
    
    
    
    
    
    static inline bool guiCoversWorld = false;

    
    static inline jclass activeRenderInfoClass = nullptr;
    static inline jfieldID projectionField = nullptr;
    static inline jfieldID modelViewField = nullptr;

    static inline jclass floatBufferClass = nullptr;
    static inline jmethodID floatBufferGet = nullptr;

    
    static bool Init() {
        if (initialized)
            return true;

        
        if (initFailed) {
            if (initRetryCount >= 10)
                return false;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastInitAttempt).count();
            if (elapsed < 5)
                return false;
            
            initFailed = false;
            lastInitAttempt = now;
            initRetryCount++;
        }

        if (!Java::Env()) {
            initFailed = true;
            lastInitAttempt = std::chrono::steady_clock::now();
            return false;
        }

        
        if (!Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.renderer.ActiveRenderInfo"), activeRenderInfoClass)) {
            log_err("RenderData: ActiveRenderInfo class not found");
            initFailed = true;
            return false;
        }
        log_debug("RenderData: ActiveRenderInfo class found");

        
        projectionField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, OBFUSCATE_STRING("PROJECTION").c_str(),
                                                         OBFUSCATE_STRING("Ljava/nio/FloatBuffer;").c_str());
        if (projectionField) {
            log_debug("RenderData: Found PROJECTION (1.8.x style)");
        } else {
            Java::Env()->ExceptionClear();
            projectionField =
                Java::Env()->GetStaticFieldID(activeRenderInfoClass, OBFUSCATE_STRING("projection").c_str(),
                                              OBFUSCATE_STRING("Ljava/nio/FloatBuffer;").c_str());
            if (projectionField)
                log_debug("RenderData: Found projection (1.7.x style)");
        }
        if (!projectionField) {
            Java::Env()->ExceptionClear();
            projectionField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "field_178813_c",
                                                             "Ljava/nio/FloatBuffer;");
            if (projectionField)
                log_debug("RenderData: Found Forge SRG PROJECTION");
        }
        
        if (!projectionField) {
            Java::Env()->ExceptionClear();
            projectionField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "c", "Ljava/nio/FloatBuffer;");
            if (projectionField)
                log_debug("RenderData: Found Badlion/Notch PROJECTION");
        }
        
        
        if (!projectionField && Java::RuntimeObfuscated) {
            Java::Env()->ExceptionClear();
            projectionField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "c", "Ljava/nio/FloatBuffer;");
            if (projectionField)
                log_debug("RenderData: Found vanilla 1.8.9 PROJECTION");
        }

        modelViewField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, OBFUSCATE_STRING("MODELVIEW").c_str(),
                                                       OBFUSCATE_STRING("Ljava/nio/FloatBuffer;").c_str());
        if (modelViewField) {
            log_debug("RenderData: Found MODELVIEW (1.8.x style)");
        } else {
            Java::Env()->ExceptionClear();
            modelViewField =
                Java::Env()->GetStaticFieldID(activeRenderInfoClass, OBFUSCATE_STRING("modelview").c_str(),
                                              OBFUSCATE_STRING("Ljava/nio/FloatBuffer;").c_str());
            if (modelViewField)
                log_debug("RenderData: Found modelview (1.7.x style)");
        }
        if (!modelViewField) {
            Java::Env()->ExceptionClear();
            modelViewField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "field_178812_b",
                                                            "Ljava/nio/FloatBuffer;");
            if (modelViewField)
                log_debug("RenderData: Found Forge SRG MODELVIEW");
        }
        if (!modelViewField) {
            Java::Env()->ExceptionClear();
            modelViewField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "b", "Ljava/nio/FloatBuffer;");
            if (modelViewField)
                log_debug("RenderData: Found Badlion/Notch MODELVIEW");
        }

        if (!projectionField && !modelViewField) {
            
            
            const auto buffers = jni_resolver::findFieldsBySignature(
                activeRenderInfoClass, "Ljava/nio/FloatBuffer;", true);
            if (buffers.size() == 3) {
                modelViewField = buffers[0];
                projectionField = buffers[1];
                log_debug("RenderData: Found Badlion matrix fields by descriptor order");
            }
        }
        if (!modelViewField && Java::RuntimeObfuscated) {
            Java::Env()->ExceptionClear();
            modelViewField = Java::Env()->GetStaticFieldID(activeRenderInfoClass, "b", "Ljava/nio/FloatBuffer;");
            if (modelViewField)
                log_debug("RenderData: Found vanilla 1.8.9 MODELVIEW");
        }

        if (!projectionField || !modelViewField) {
            Java::Env()->ExceptionClear();
            if (!projectionField)
                log_err("RenderData: projection field not found!");
            if (!modelViewField)
                log_err("RenderData: modelview field not found!");
            log_err("RenderData: Missing matrix fields, init failed");
            initFailed = true;
            return false;
        }

        
        jclass fbLocal = Java::Env()->FindClass(OBFUSCATE_STRING("java/nio/FloatBuffer").c_str());
        if (!fbLocal) {
            initFailed = true;
            return false;
        }
        floatBufferClass = (jclass)Java::Env()->NewGlobalRef(fbLocal);
        Java::Env()->DeleteLocalRef(fbLocal);

        floatBufferGet = Java::Env()->GetMethodID(floatBufferClass, OBFUSCATE_STRING("get").c_str(),
                                                  OBFUSCATE_STRING("(I)F").c_str());
        if (!floatBufferGet) {
            initFailed = true;
            return false;
        }

        initialized = true;
        log_ok("RenderData initialized");
        return true;
    }

    static Matrix ReadMatrix(jobject floatBuffer, const char* name) {
        Matrix m{};
        if (!floatBuffer) {
            log_warn("RenderData: %s floatBuffer is null", name);
            return m;
        }

        void* directPtr = Java::Env()->GetDirectBufferAddress(floatBuffer);
        if (directPtr) {
            float* data = static_cast<float*>(directPtr);
            m.m00 = data[0];
            m.m01 = data[1];
            m.m02 = data[2];
            m.m03 = data[3];
            m.m10 = data[4];
            m.m11 = data[5];
            m.m12 = data[6];
            m.m13 = data[7];
            m.m20 = data[8];
            m.m21 = data[9];
            m.m22 = data[10];
            m.m23 = data[11];
            m.m30 = data[12];
            m.m31 = data[13];
            m.m32 = data[14];
            m.m33 = data[15];
            log_debug("RenderData: ReadMatrix %s via direct buffer. m00=%d, m11=%d, m22=%d, m33=%d",
                      name, (int)(m.m00 * 1000.0f), (int)(m.m11 * 1000.0f), (int)(m.m22 * 1000.0f), (int)(m.m33 * 1000.0f));
            return m;
        }

        if (!floatBufferGet) {
            log_warn("RenderData: %s floatBufferGet method is null", name);
            return m;
        }

        m.m00 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 0);
        m.m01 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 1);
        m.m02 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 2);
        m.m03 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 3);
        m.m10 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 4);
        m.m11 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 5);
        m.m12 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 6);
        m.m13 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 7);
        m.m20 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 8);
        m.m21 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 9);
        m.m22 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 10);
        m.m23 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 11);
        m.m30 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 12);
        m.m31 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 13);
        m.m32 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 14);
        m.m33 = Java::Env()->CallFloatMethod(floatBuffer, floatBufferGet, 15);

        log_debug("RenderData: ReadMatrix %s via CallFloatMethod. m00=%d, m11=%d, m22=%d, m33=%d",
                  name, (int)(m.m00 * 1000.0f), (int)(m.m11 * 1000.0f), (int)(m.m22 * 1000.0f), (int)(m.m33 * 1000.0f));
        return m;
    }

    
    static bool IsMatrixValid(const Matrix& m) {
        return m.m00 != 0.0f || m.m11 != 0.0f || m.m22 != 0.0f || m.m33 != 0.0f;
    }

    
    static void Update() {
        if (updated && IsMatrixValid(projection) && IsMatrixValid(modelView)) {
            return;
        }

        if (!initialized && !Init())
            return;

        
        jobject projBuffer = Java::Env()->GetStaticObjectField(activeRenderInfoClass, projectionField);
        if (projBuffer) {
            Matrix newProj = ReadMatrix(projBuffer, "PROJECTION");
            if (IsMatrixValid(newProj)) {
                projection = newProj;
            }
            Java::Env()->DeleteLocalRef(projBuffer);
        } else {
            log_warn("RenderData: GetStaticObjectField projectionField returned null");
        }

        
        jobject mvBuffer = Java::Env()->GetStaticObjectField(activeRenderInfoClass, modelViewField);
        if (mvBuffer) {
            Matrix newMv = ReadMatrix(mvBuffer, "MODELVIEW");
            if (IsMatrixValid(newMv)) {
                modelView = newMv;
            }
            Java::Env()->DeleteLocalRef(mvBuffer);
        } else {
            log_warn("RenderData: GetStaticObjectField modelViewField returned null");
        }

        
        bool wasUpdated = updated;
        updated = IsMatrixValid(projection) && IsMatrixValid(modelView);
        if (updated && !wasUpdated) {
            log_ok("RenderData: Successfully verified non-zero projection and modelview matrices!");
        } else if (!updated && wasUpdated) {
            log_warn("RenderData: Matrices went invalid (all zeros detected)!");
        }
    }

    
    static void Reset() {
        updated = false;
        modelView = Matrix{};
        projection = Matrix{};
        initFailed = false;
        initRetryCount = 0;
        lastInitAttempt = {};
    }
};
