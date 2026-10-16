#pragma once

#include <string>
#include "../../jni/java.h"


inline std::string jstringToString(jstring jstr) {
    if (!jstr)
        return "";

    const char* chars = Java::Env()->GetStringUTFChars(jstr, nullptr);
    if (!chars)
        return "";

    std::string result(chars);
    Java::Env()->ReleaseStringUTFChars(jstr, chars);
    return result;
}
