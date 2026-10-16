#pragma once

#include <functional>
#include <string>
#include <vector>
#include "../../jni/java.h"








namespace LunarPacketListener {


bool Init();


void Shutdown();


bool IsReady();


void setDebugLogging(bool enabled);
bool isDebugLogging();


struct NametagEntry {
    std::string playerName;
    std::string uuid;
    std::vector<std::string> lines;  
};

using NametagUpdateCallback = std::function<void(const std::vector<NametagEntry>& entries)>;
using NametagOverrideCallback =
    std::function<void(const std::string& playerUuid, const std::vector<std::string>& lines)>;


void onNametagUpdate(NametagUpdateCallback callback);
void onNametagOverride(NametagOverrideCallback callback);


void Tick();


bool processIncomingPacket(JNIEnv* env, jobject packet);


jclass getNametagsUpdateClass();
jclass getNametagsOverrideClass();

}  
