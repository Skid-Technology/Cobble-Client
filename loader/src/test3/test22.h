#pragma once
#include <array>
#include <cstddef>
#include <utility>




template <size_t N, int K>
struct XorString {
private:
    const char _key;
    std::array<char, N + 1> _encrypted;

    constexpr char enc(char c) const {
        return c ^ _key ^ K;
    }

    char dec(char c) const {
        return c ^ _key ^ K;
    }

public:
    template <size_t... Is>
    constexpr __forceinline XorString(const char* str, std::index_sequence<Is...>)
        : _key((char)(N % 255 + 1)), _encrypted{ enc(str[Is])... } {}

    __forceinline const char* decrypt() {
        for (size_t i = 0; i < N; ++i) {
            _encrypted[i] = dec(_encrypted[i]);
        }
        _encrypted[N] = '\0';
        return _encrypted.data();
    }
};


#define _xor_(str) (XorString<sizeof(str) - 1, __COUNTER__>(str, std::make_index_sequence<sizeof(str) - 1>()).decrypt())


template <size_t N, int K>
struct XorWString {
private:
    const wchar_t _key;
    std::array<wchar_t, N + 1> _encrypted;

    constexpr wchar_t enc(wchar_t c) const {
        return c ^ _key ^ K;
    }

    wchar_t dec(wchar_t c) const {
        return c ^ _key ^ K;
    }

public:
    template <size_t... Is>
    constexpr __forceinline XorWString(const wchar_t* str, std::index_sequence<Is...>)
        : _key((wchar_t)((N % 255) + 1)), _encrypted{ enc(str[Is])... } {}

    __forceinline const wchar_t* decrypt() {
        for (size_t i = 0; i < N; ++i) {
            _encrypted[i] = dec(_encrypted[i]);
        }
        _encrypted[N] = L'\0';
        return _encrypted.data();
    }
};

#define _xorw_(str) (XorWString<(sizeof(str)/sizeof(wchar_t)) - 1, __COUNTER__>(str, std::make_index_sequence<(sizeof(str)/sizeof(wchar_t)) - 1>()).decrypt())
