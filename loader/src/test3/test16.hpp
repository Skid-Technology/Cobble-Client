#pragma once
#include <intrin.h>
#include <Windows.h>
#include <cstdint>

class AntiDebug {
private:
    static inline uint64_t baseline_ = 0;


    static inline uint64_t rdtsc() { return __rdtsc(); }

public:

    static void calibrate() {

        uint64_t total = 0;
        for (int i = 0; i < 100; i++) {
            uint64_t start = rdtsc();
            _mm_pause();
            uint64_t end = rdtsc();
            total += (end - start);
        }
        baseline_ = total / 100;
    }


    template <typename Func>
    static bool timingTrap(Func&& func) {
        uint64_t start = rdtsc();
        func();
        uint64_t end = rdtsc();


        return (end - start) > (baseline_ * 50);
    }


    static bool hasHardwareBP() {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

        if (!GetThreadContext(GetCurrentThread(), &ctx)) {
            return false;
        }


        return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
    }


    static bool hasDebuggerWindows() {

        const char* debuggers[] = {
            "Qt5QWindowIcon",
            "OLLYDBG",
            "WinDbgFrameClass",
            "ID",
            "Zeta Debugger",
            "Rock Debugger",
            "Qt5152QWindowIcon"
        };

        for (const char* className : debuggers) {
            if (FindWindowA(className, nullptr)) {
                return true;
            }
        }

        return false;
    }


    static bool isDebugged() {

        if (IsDebuggerPresent()) {
            return true;
        }


        if (hasHardwareBP()) {
            return true;
        }


        if (hasDebuggerWindows()) {
            return true;
        }


        bool slow = timingTrap([] {
            volatile int x = 0;
            for (int i = 0; i < 10; i++) {
                x++;
            }
        });

        return slow;
    }


    static inline void smokeTest() {
        if (baseline_ == 0)
            return;


        uint64_t start = rdtsc();
        volatile int dummy = 0;
        dummy++;
        uint64_t end = rdtsc();


        if ((end - start) > (baseline_ * 100)) {
            *(int*)nullptr = 0;
        }
    }
};
