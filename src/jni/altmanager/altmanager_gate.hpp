#pragma once

#include "../java.h"

#include <string>

namespace AltManagerGate {

void MaybeSpoofLoginPacket(JNIEnv *env, jobject packet);




bool SetSession(JNIEnv* env, const char* username, const char* uuid,
                const char* token, const char* sessionType,
                bool overwriteEmpty = false);
bool GetSession(JNIEnv* env, std::string& username, std::string& uuid,
                std::string& token, std::string& sessionType);

void Shutdown(JNIEnv* env);

} 
