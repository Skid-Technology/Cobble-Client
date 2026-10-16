#pragma once




#ifdef COBBLE_DEV_BUILD

inline void VMProtectBeginUltra(const char*) {}
inline void VMProtectBeginMutation(const char*) {}
inline void VMProtectEnd() {}
inline bool VMProtectIsProtected() { return false; }
inline bool VMProtectIsDebuggerPresent(bool) { return false; }
inline bool VMProtectIsVirtualMachinePresent() { return false; }
inline bool VMProtectIsValidImageCRC() { return true; }

#else

#include <VMProtectSDK.h>

#endif


#define CHECK_PROTECTION(var, val)     do { (var) = VMProtectIsProtected() ? (val) : 0; } while (0)
#define CHECK_DEBUGGER(var, val)       do { (var) = VMProtectIsDebuggerPresent(true) ? 1 : (val); } while (0)
#define CHECK_VIRTUAL_PC(var, val)     do { (var) = VMProtectIsVirtualMachinePresent() ? 1 : (val); } while (0)
#define CHECK_CODE_INTEGRITY(var, val) do { (var) = VMProtectIsValidImageCRC() ? (val) : 0; } while (0)



#define COBBLE_VM_CRITICAL_START  VMProtectBeginUltra(__FUNCTION__);
#define COBBLE_VM_CRITICAL_END    VMProtectEnd();
#define COBBLE_VM_HIGH_START      VMProtectBeginUltra(__FUNCTION__);
#define COBBLE_VM_HIGH_END        VMProtectEnd();
#define COBBLE_HWID_BEGIN         VMProtectBeginUltra(__FUNCTION__);
#define COBBLE_HWID_END           VMProtectEnd();
#define COBBLE_STR_ENCRYPTW_BEGIN VMProtectBeginMutation(__FUNCTION__);
#define COBBLE_STR_ENCRYPTW_END   VMProtectEnd();
