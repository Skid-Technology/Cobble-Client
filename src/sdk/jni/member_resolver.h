#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "../../util/logger.hpp"
#include "../../jni/java.h"




namespace jni_resolver {

inline std::vector<std::string> signatureVariants(const char* signature) {
    std::vector<std::string> variants;
    if (!signature)
        return variants;

    variants.emplace_back(signature);
    std::string notchSignature = Java::NotchDescriptor(signature);
    if (notchSignature != signature)
        variants.push_back(std::move(notchSignature));
    return variants;
}

inline bool signatureMatches(const char* actual, const std::vector<std::string>& expected) {
    if (!actual)
        return false;
    for (const auto& candidate : expected) {
        if (candidate == actual)
            return true;
    }
    return false;
}

inline jmethodID findUniqueMethodBySignature(jclass cls, const char* signature, bool isStatic) {
    if (!cls || !signature || !Java::tiEnv)
        return nullptr;

    jint count = 0;
    jmethodID* methods = nullptr;
    if (Java::tiEnv->GetClassMethods(cls, &count, &methods) != JVMTI_ERROR_NONE || !methods)
        return nullptr;

    jmethodID match = nullptr;
    jint matches = 0;
    std::string matchName;
    std::string matchSignature;
    const auto expectedSignatures = signatureVariants(signature);

    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* sig = nullptr;
        jint modifiers = 0;
        const bool named = Java::tiEnv->GetMethodName(methods[i], &name, &sig, nullptr) == JVMTI_ERROR_NONE;
        const bool hasModifiers = Java::tiEnv->GetMethodModifiers(methods[i], &modifiers) == JVMTI_ERROR_NONE;
        const bool staticMatch = !hasModifiers || ((modifiers & 0x0008) != 0) == isStatic;

        if (named && signatureMatches(sig, expectedSignatures) && staticMatch) {
            match = methods[i];
            ++matches;
            if (matches == 1 && name) {
                matchName = name;
                matchSignature = sig;
            }
        }

        if (name)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(name));
        if (sig)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(sig));
    }

    Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(methods));

    if (matches == 1) {
        log_debug("[JniCache] descriptor fallback method: %s%s", matchName.c_str(), matchSignature.c_str());
        return match;
    }
    return nullptr;
}

inline jfieldID findUniqueFieldBySignature(jclass cls, const char* signature, bool isStatic) {
    if (!cls || !signature || !Java::tiEnv)
        return nullptr;

    jint count = 0;
    jfieldID* fields = nullptr;
    if (Java::tiEnv->GetClassFields(cls, &count, &fields) != JVMTI_ERROR_NONE || !fields)
        return nullptr;

    jfieldID match = nullptr;
    jint matches = 0;
    std::string matchName;
    std::string matchSignature;
    const auto expectedSignatures = signatureVariants(signature);

    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* sig = nullptr;
        jint modifiers = 0;
        const bool named = Java::tiEnv->GetFieldName(cls, fields[i], &name, &sig, nullptr) == JVMTI_ERROR_NONE;
        const bool hasModifiers = Java::tiEnv->GetFieldModifiers(cls, fields[i], &modifiers) == JVMTI_ERROR_NONE;
        const bool staticMatch = !hasModifiers || ((modifiers & 0x0008) != 0) == isStatic;

        if (named && signatureMatches(sig, expectedSignatures) && staticMatch) {
            match = fields[i];
            ++matches;
            if (matches == 1 && name) {
                matchName = name;
                matchSignature = sig;
            }
        }

        if (name)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(name));
        if (sig)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(sig));
    }

    Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(fields));

    if (matches == 1) {
        log_debug("[JniCache] descriptor fallback field: %s%s", matchName.c_str(), matchSignature.c_str());
        return match;
    }
    return nullptr;
}

inline std::vector<jfieldID> findFieldsBySignature(jclass cls, const char* signature, bool isStatic) {
    std::vector<jfieldID> matches;
    if (!cls || !signature || !Java::tiEnv)
        return matches;

    jint count = 0;
    jfieldID* fields = nullptr;
    if (Java::tiEnv->GetClassFields(cls, &count, &fields) != JVMTI_ERROR_NONE || !fields)
        return matches;
    const auto expectedSignatures = signatureVariants(signature);

    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* sig = nullptr;
        jint modifiers = 0;
        const bool named = Java::tiEnv->GetFieldName(cls, fields[i], &name, &sig, nullptr) == JVMTI_ERROR_NONE;
        const bool hasModifiers = Java::tiEnv->GetFieldModifiers(cls, fields[i], &modifiers) == JVMTI_ERROR_NONE;
        const bool staticMatch = !hasModifiers || ((modifiers & 0x0008) != 0) == isStatic;

        if (named && signatureMatches(sig, expectedSignatures) && staticMatch)
            matches.push_back(fields[i]);

        if (name)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(name));
        if (sig)
            Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(sig));
    }

    Java::tiEnv->Deallocate(reinterpret_cast<unsigned char*>(fields));
    return matches;
}

}  
