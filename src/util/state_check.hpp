#pragma once





















#include "state.hpp"
#include "../sdk/jni/jni_cache.h"

namespace sdk::internal {


__forceinline bool gateA() {
    uint32_t flags = g_stateFlags.load(std::memory_order_relaxed);
    return (flags & 0x1) != 0 && flags != 0xDEAD;
}




__forceinline bool gateB() {
    if (g_initialChecksum == 0)
        return false;
    return computeChecksum() == g_initialChecksum;
}




__forceinline bool gateC() {
    if (g_initTime == 0)
        return false;
    uint64_t now = GetTickCount64();
    return (now - g_lastSync) <= MAX_DRIFT_MS;
}




inline bool (*g_validateFn)() = nullptr;
__forceinline bool gateD() {
    auto fn = g_validateFn;
    return fn && fn();
}



__forceinline bool gateAB() { return gateA() && gateB(); }
__forceinline bool gateAC() { return gateA() && gateC(); }
__forceinline bool gateBC() { return gateB() && gateC(); }
__forceinline bool gateABC() { return gateA() && gateB() && gateC(); }



inline void wireGateD() {
    g_validateFn = &validateState;
}

}  
