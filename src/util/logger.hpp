#pragma once











#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstdarg>
#include <cstdio>

#ifdef COBBLE_DEV_BUILD

#include "inject_trace.hpp"

namespace cobble_log {




inline void write(const char* level, const char* fmt, ...) {
    static volatile LONG g_logLock = 0;

    
    while (InterlockedCompareExchange(&g_logLock, 1, 0) != 0) {
        Sleep(0);
    }

    static char buf[2048];

    
    
    
    
    int prefixLen = std::snprintf(buf, sizeof(buf), "[%s] ", level);
    if (prefixLen < 0 || prefixLen >= (int)sizeof(buf)) prefixLen = 0;

    
    va_list args;
    va_start(args, fmt);
    const size_t remaining = sizeof(buf) - prefixLen;
    int msgLen = std::vsnprintf(buf + prefixLen, remaining, fmt, args);
    va_end(args);

    if (msgLen < 0) msgLen = 0;
    
    if ((size_t)msgLen >= remaining)
        msgLen = (int)remaining - 1;

    
    buf[prefixLen + msgLen] = '\0';

    
    cobble_trace::write_step(buf);

    
    InterlockedExchange(&g_logLock, 0);
}

}  

class c_logger {
public:
    static void initialize() {
        
        
        cobble_trace::write_step("=== Logger initialized ===");
    }
    static void shutdown() {
        cobble_trace::write_step("=== Logger shutdown ===");
    }
};

#define log_debug(fmt, ...) ::cobble_log::write("DEBUG", fmt, ##__VA_ARGS__)
#define log_ok(fmt, ...) ::cobble_log::write(" OK  ", fmt, ##__VA_ARGS__)
#define log_err(fmt, ...) ::cobble_log::write("ERROR", fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) ::cobble_log::write("WARN ", fmt, ##__VA_ARGS__)
#define log_raw(fmt, ...) ::cobble_log::write(" --- ", fmt, ##__VA_ARGS__)

#else  

class c_logger {
public:
    static void initialize() {}
    static void shutdown() {}
};

#define log_debug(fmt, ...) ((void)0)
#define log_ok(fmt, ...) ((void)0)
#define log_err(fmt, ...) ((void)0)
#define log_warn(fmt, ...) ((void)0)
#define log_raw(fmt, ...) ((void)0)

#endif
