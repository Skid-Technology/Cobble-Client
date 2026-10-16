#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace obf {


constexpr uint32_t compile_seed() {
    return (__TIME__[7] - '0') * 1 + (__TIME__[6] - '0') * 10 + (__TIME__[4] - '0') * 60 + (__TIME__[3] - '0') * 600 +
           (__TIME__[1] - '0') * 3600 + (__TIME__[0] - '0') * 36000;
}


template <size_t N>
class ObfString {
private:
    std::array<char, N> data_;


    static constexpr char encrypt(char c, size_t i, uint32_t seed) {

        uint32_t k = (seed + static_cast<uint32_t>(i)) * 0x41C64E6D + 0x3039;
        return c ^ static_cast<char>((k & 0xFF) ^ (i & 0xFF));
    }

public:

    template <size_t... I>
    constexpr ObfString(const char* str, std::index_sequence<I...>) : data_{encrypt(str[I], I, compile_seed())...} {}


    inline std::string decrypt() const {
        std::string result;
        result.reserve(N - 1);

        uint32_t seed = compile_seed();
        for (size_t i = 0; i < N - 1; i++) {

            uint32_t k = (seed + static_cast<uint32_t>(i)) * 0x41C64E6D + 0x3039;
            result += data_[i] ^ static_cast<char>((k & 0xFF) ^ (i & 0xFF));
        }

        return result;
    }


    inline const char* c_str() const {
        static thread_local std::string cached;
        cached = decrypt();
        return cached.c_str();
    }
};

}


#define OBFS(str)                                                                                     \
    [] {                                                                                              \
        constexpr auto s = obf::ObfString<sizeof(str)>(str, std::make_index_sequence<sizeof(str)>{}); \
        return s.decrypt();                                                                           \
    }()


#define OBFS_CSTR(str)                                                                                       \
    [] {                                                                                                     \
        constexpr static auto s = obf::ObfString<sizeof(str)>(str, std::make_index_sequence<sizeof(str)>{}); \
        return s.c_str();                                                                                    \
    }()
