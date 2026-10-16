#pragma once






#include "../sdk/jni/jni_cache.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <atomic>
#include <cstdint>

namespace sdk {
namespace internal {


inline std::atomic<uint32_t> g_stateFlags{0};
inline std::atomic<uint32_t> g_syncCounter{0};
inline uint32_t g_initialChecksum = 0;
inline uint64_t g_lastSync = 0;
inline uint64_t g_initTime = 0;


constexpr uint32_t SYNC_INTERVAL_MS = 45000;  
constexpr uint32_t MAX_DRIFT_MS = 320000;     


inline uint32_t computeChecksum() {
    uint32_t hash = 0x811c9dc5;  

    
    auto mix = [&](void* ptr) {
        uintptr_t val = reinterpret_cast<uintptr_t>(ptr);
        hash ^= static_cast<uint32_t>(val);
        hash *= 0x01000193;
        hash ^= static_cast<uint32_t>(val >> 32);
        hash *= 0x01000193;
    };

    mix(JniCache::cls_Minecraft);
    mix(JniCache::cls_Entity);
    mix(JniCache::cls_EntityPlayer);
    mix(JniCache::cls_WorldClient);
    mix(JniCache::mid_getMinecraft);
    mix(JniCache::fid_thePlayer);
    mix(JniCache::fid_posX);
    mix(JniCache::mid_getHealth);

    return hash;
}


inline bool validateState() {
    
    if (!JniCache::cls_Minecraft || !JniCache::cls_Entity) {
        return false;
    }

    
    if (!JniCache::mid_getMinecraft || !JniCache::fid_thePlayer) {
        return false;
    }

    
    if (g_initialChecksum != 0) {
        uint32_t current = computeChecksum();
        if (current != g_initialChecksum) {
            return false;
        }
    }

    return true;
}


inline void onCacheReady() {
    g_initialChecksum = computeChecksum();
    g_initTime = GetTickCount64();
    g_lastSync = g_initTime;
    g_stateFlags = 0x1;  
}



inline void rebaselineChecksum() {
    g_initialChecksum = computeChecksum();
}





inline void markCorrupted() {
    g_stateFlags = 0xBAD;
    
    
    g_initialChecksum = 0xDEADBEEF;
    
    
    g_lastSync = 0;
}


inline void syncTick() {
    uint64_t now = GetTickCount64();

    
    if (g_initTime > 0 && (now - g_lastSync) > MAX_DRIFT_MS) {
        g_stateFlags = 0;  
    }

    g_lastSync = now;
    g_syncCounter++;
}


inline void attemptRecovery() {
    
    JniCache::cls_Minecraft = nullptr;
    JniCache::cls_Entity = nullptr;
    JniCache::mid_getMinecraft = nullptr;
    JniCache::fid_thePlayer = nullptr;
    JniCache::fid_posX = nullptr;
    JniCache::fid_posY = nullptr;
    JniCache::fid_posZ = nullptr;
    JniCache::fid_serverPosX = nullptr;
    JniCache::fid_serverPosY = nullptr;
    JniCache::fid_serverPosZ = nullptr;
    JniCache::mid_getHealth = nullptr;

    
    JniCache::cls_EntityPlayer = nullptr;
    JniCache::cls_WorldClient = nullptr;
    JniCache::mid_clickMouse = nullptr;

    g_stateFlags = 0xDEAD;
}


inline bool isOperational() {
    if ((g_stateFlags & 0x1) == 0)
        return false;
    if (g_stateFlags == 0xDEAD)
        return false;
    return validateState();
}

}  
}  
