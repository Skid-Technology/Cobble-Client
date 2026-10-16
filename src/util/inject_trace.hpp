#pragma once





















#include <Windows.h>

#ifdef COBBLE_DEV_BUILD

namespace cobble_trace {



inline HANDLE& trace_handle() {
    static HANDLE h = INVALID_HANDLE_VALUE;
    return h;
}

inline HANDLE open_trace_handle_once() {
    HANDLE& hRef = trace_handle();
    if (hRef != INVALID_HANDLE_VALUE)
        return hRef;

    char path[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 24)
        return INVALID_HANDLE_VALUE;
    const char* fname = "\\cobble_trace.log";
    for (DWORD i = 0; fname[i] && n + i < MAX_PATH - 1; ++i)
        path[n + i] = fname[i];
    path[n + 17] = '\0';

    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    
    
    HANDLE prev = (HANDLE)InterlockedCompareExchangePointer((PVOID*)&hRef,
                                                             (PVOID)h, (PVOID)INVALID_HANDLE_VALUE);
    if (prev != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return prev;
    }
    return h;
}

inline void write_step(const char* step) {
    if (!step) return;

    HANDLE h = open_trace_handle_once();
    if (h == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    char buf[2048];
    int len = wsprintfA(buf, "[%02u:%02u:%02u.%03u] tid=%lu pid=%lu  %s\r\n",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                        GetCurrentThreadId(), GetCurrentProcessId(), step);
    if (len > 0) {
        DWORD written = 0;
        
        
        
        WriteFile(h, buf, (DWORD)len, &written, nullptr);
    }
}

inline void clear() {
    
    
    HANDLE& hRef = trace_handle();
    HANDLE old = (HANDLE)InterlockedExchangePointer((PVOID*)&hRef, (PVOID)INVALID_HANDLE_VALUE);
    if (old != INVALID_HANDLE_VALUE)
        CloseHandle(old);

    char path[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 24) return;
    const char* fname = "\\cobble_trace.log";
    for (DWORD i = 0; fname[i] && n + i < MAX_PATH - 1; ++i) path[n + i] = fname[i];
    path[n + 17] = '\0';
    DeleteFileA(path);
}

}  

#define COBBLE_TRACE(step) ::cobble_trace::write_step(step)
#define COBBLE_TRACE_RESET() ::cobble_trace::clear()




#define COBBLE_TRACE_ONCE(step)                  \
    do {                                         \
        static bool _cobble_trace_once = false;  \
        if (!_cobble_trace_once) {               \
            _cobble_trace_once = true;           \
            ::cobble_trace::write_step(step);    \
        }                                        \
    } while (0)

#else  

#define COBBLE_TRACE(step) ((void)0)
#define COBBLE_TRACE_RESET() ((void)0)
#define COBBLE_TRACE_ONCE(step) ((void)0)

#endif
