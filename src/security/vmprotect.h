#pragma once



#ifdef COBBLE_DEV_BUILD

inline void VMProtectBeginUltra(const char*) {}
inline void VMProtectEnd() {}

#else

#include <VMProtectSDK.h>





#ifdef COBBLE_VM_MUTATE_ALL
#define COBBLE_DLL_ULTRA_START(name) VMProtectBeginMutation(name);
#else
#define COBBLE_DLL_ULTRA_START(name) VMProtectBeginUltra(name);
#endif



#define COBBLE_DLL_MUTATION_START(name) VMProtectBeginMutation(name);

#endif

#define COBBLE_DLL_ULTRA_END() VMProtectEnd();
