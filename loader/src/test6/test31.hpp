#pragma once




#include <cstdint>
#include <ctime>
#include <utility>

namespace obf {





#if defined(_MSC_VER)
    #define CF_NOOPT __pragma(optimize("", off))
    #define CF_OPT   __pragma(optimize("", on))
    #define CF_FORCEINLINE __forceinline
    #define CF_NOINLINE __declspec(noinline)
    #define CF_ASSUME(x) __assume(x)
#else
    #define CF_NOOPT
    #define CF_OPT
    #define CF_FORCEINLINE inline
    #define CF_NOINLINE
    #define CF_ASSUME(x) ((void)0)
#endif





inline volatile int _cf_global_seed = 777 + (int)__TIME__[1] + (int)__TIME__[2] * 17;





#define CF_NOP() \
    do { \
        volatile int __cf_nop_val = __COUNTER__ ^ (int)__LINE__; \
        obf::_cf_global_seed = (obf::_cf_global_seed ^ __cf_nop_val ^ (int)std::time(nullptr)); \
        (void)__cf_nop_val; \
    } while(0)






#define CF_MBA_ADD(a, b) ( \
    ( ((unsigned int)(a) | (unsigned int)(b)) + ((unsigned int)(a) & (unsigned int)(b)) ) \
    - ( ((unsigned int)(a) ^ (unsigned int)(b)) ) \
    + ( ((unsigned int)(a) ^ (unsigned int)(b)) + 2U * ((unsigned int)(a) & (unsigned int)(b)) ) \
    - ( ((unsigned int)(a) | (unsigned int)(b)) - ((unsigned int)(a) & (unsigned int)(b)) ) \
)

#define CF_MBA_SUB(a, b) ( \
    ( ((unsigned int)(a)) + (~(unsigned int)(b)) + 1U ) \
    ^ ( ((unsigned int)(a)) ^ ((~(unsigned int)(b)) ^ 1U) ) \
)

#define CF_MBA_XOR(a, b) ( \
    ( (((unsigned int)(a)) & (~(unsigned int)(b))) | ((~(unsigned int)(a)) & ((unsigned int)(b))) ) \
    ^ ( ((unsigned int)(a) | (unsigned int)(b)) - ((unsigned int)(a) & (unsigned int)(b)) ) \
)

#define CF_MBA_NOT(a) ( \
    ( ((unsigned int)(a)) ^ 0xFFFFFFFFU ) \
    ^ ( ~((unsigned int)(a)) ) \
)







#define CF_OPAQUE_TRUE() \
    ( \
        ((~0u >> 1) > 0u) && \
        ((__LINE__ | (unsigned int)obf::_cf_global_seed) >= 0u) && \
        ((((unsigned int)obf::_cf_global_seed | 0xCAFEBABEu) ^ 0xCAFEBABEu) <= (unsigned int)obf::_cf_global_seed \
            || true) \
    )

#define CF_OPAQUE_FALSE() \
    ( \
        ((unsigned int)obf::_cf_global_seed == 0xDEADBEEF) && \
        (((unsigned int)(__LINE__) ^ (unsigned int)obf::_cf_global_seed) == 0xDEADBEEF) && \
        false \
    )





namespace junk {

CF_FORCEINLINE static int func_1() {
    volatile int x = __COUNTER__ + 11 + _cf_global_seed;
    x ^= (0xDEADBEEFU + (int)__TIME__[0]);
    x += (int)std::time(nullptr);
    return x;
}

CF_FORCEINLINE static int func_2() {
    volatile int x = (__COUNTER__ * 3) ^ (_cf_global_seed);
    for(int i = 0; i < ((__COUNTER__ % 2) + 2); ++i) x ^= i;
    return x;
}

CF_FORCEINLINE static int func_3() {
    volatile int x = (0x1234 | (99 + __COUNTER__)) ^ _cf_global_seed;
    x &= (0xFFFFU + (int)__TIME__[1]);
    x ^= (x << (((__COUNTER__) % 3) + 1) % 32);
    return x;
}

CF_FORCEINLINE static int func_4() {
    volatile int x = __COUNTER__ ^ _cf_global_seed;
    volatile int y = 7 + (__COUNTER__ % 5);
    x += y * y;
    x ^= y;
    x = (x << 1) | ((unsigned int)x >> 31);
    return x;
}

CF_FORCEINLINE static int func_5() {
    volatile int x = (((int)std::time(nullptr) ^ _cf_global_seed + __COUNTER__) % 7);
    if (x == 0) x = 1;
    x = (x * 13) + 71;
    return x;
}

CF_FORCEINLINE static int func_6() {
    volatile int x = (int)std::time(nullptr) ^ (_cf_global_seed * __COUNTER__);
    int z = ((x & 0xFF) * 0x1F1F1F1F) ^ (__COUNTER__ + _cf_global_seed);
    x = z ^ (x << 2);
    x ^= ((int)__TIME__[2] | (int)__TIME__[3]);
    return x;
}

CF_FORCEINLINE static int func_7() {
    volatile int x = ((int)std::time(nullptr) + _cf_global_seed + __COUNTER__) ^ 0x0F0F0F0F;
    x = ((x << 3) | ((unsigned int)x >> 29)) ^ (int)__LINE__;
    return x;
}

using junk_func_ptr = int(*)();
static junk_func_ptr table[] = {
    func_1, func_2, func_3, func_4, func_5, func_6, func_7
};
constexpr size_t table_size = sizeof(table) / sizeof(junk_func_ptr);

}


#define CF_JUNK() \
    (obf::junk::table[ \
        ((obf::_cf_global_seed ^ __COUNTER__ ^ (int)std::time(nullptr) ^ (int)__LINE__) & 0x7FFFFFFF) % obf::junk::table_size \
    ]())






#define CF_CHECK(condition) \
    (CF_OPAQUE_TRUE() && (condition))


#define CF_SECURITY_PROLOGUE() \
    do { \
        CF_NOP(); \
        CF_JUNK(); \
        obf::_cf_global_seed = CF_MBA_XOR(obf::_cf_global_seed, (int)__LINE__); \
    } while(0)


#define CF_SECURITY_EPILOGUE() \
    do { \
        obf::_cf_global_seed = CF_MBA_ADD(obf::_cf_global_seed, CF_JUNK()); \
        CF_NOP(); \
    } while(0)


#define CF_CRITICAL_BEGIN() CF_SECURITY_PROLOGUE()
#define CF_CRITICAL_END() CF_SECURITY_EPILOGUE()






constexpr uint32_t state_seed(const char* str, uint32_t offset = 0) {
    return str[offset] == '\0' ? 0x5A5A5A5A : str[offset] + 0x9E3779B9 + state_seed(str, offset + 1);
}


#define OBFUSCATE_BEGIN                                                                     \
    {                                                                                       \
        volatile int __obf_state = (obf::state_seed(__FILE__ __FUNCTION__) ^ __LINE__) % 5; \
        while (true) {                                                                      \
            switch (__obf_state) {
#define OBFUSCATE_CASE(n)          \
    case n:                        \
        __obf_state = (n + 1) % 5; \
        break;

#define OBFUSCATE_CASE_EXEC(n, code) \
    case n: {                        \
        code;                        \
        __obf_state = (n + 1) % 5;   \
    } break;

#define OBFUSCATE_CASE_FINAL(n, code) \
    case n: {                         \
        code;                         \
        goto __obf_exit;              \
    } break;

#define OBFUSCATE_END       \
    default:                \
        __debugbreak();     \
        ;                 \
        }                   \
        }                   \
    __obf_exit:;            \
        }


template <auto Func, uint32_t Seed = state_seed(__FILE__)>
struct OpaqueDispatcher {
    template <typename... Args>
    static inline auto call(Args&&... args) {

        volatile int state = (Seed ^ 0x5A5A5A5A) % 7;

        for (int i = 0; i < 10; i++) {
            switch (state) {
                case 0:
                    state = 4;
                    break;
                case 1:
                    state = 5;
                    break;
                case 2:
                    return Func(std::forward<Args>(args)...);
                case 3:
                    state = 1;
                    break;
                case 4:
                    state = 3;
                    break;
                case 5:
                    state = 2;
                    break;
                case 6:
                    state = (Seed & 1) ? 3 : 0;
                    break;
                default:
                    __debugbreak();
            }
        }


        return Func(std::forward<Args>(args)...);
    }
};


#define OBFUSCATED_CALL(func, ...) obf::OpaqueDispatcher<&func>::call(__VA_ARGS__)





#define JUNK_CODE_1               \
    {                             \
        volatile int __junk = 0;  \
        __junk = __junk * 2 + 1;  \
        if (__junk == 0xDEADBEEF) \
            __debugbreak();       \
    }

#define JUNK_CODE_2                           \
    {                                         \
        volatile uint64_t __junk = __rdtsc(); \
        __junk ^= 0x5A5A5A5A5A5A5A5A;         \
        if (__junk == 0)                      \
            __debugbreak();                   \
    }

}
