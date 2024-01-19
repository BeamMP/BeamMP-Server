// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <array>
#include <cstdarg>
#include <string>

namespace Crypto {

constexpr auto time = __TIME__;
constexpr auto seed = static_cast<int>(time[7]) + static_cast<int>(time[6]) * 10 + static_cast<int>(time[4]) * 60 + static_cast<int>(time[3]) * 600 + static_cast<int>(time[1]) * 3600 + static_cast<int>(time[0]) * 36000;

// 1988, Stephen Park and Keith Miller
// "Random Number Generators: Good Ones Are Hard To Find", considered as "minimal standard"
// Park-Miller 31 bit pseudo-random number generator, implemented with G. Carta's optimisation:
// with 32-bit math and without division

template <int N>
struct RandomGenerator {
private:
    static constexpr unsigned a = 16807; // 7^5
    static constexpr unsigned m = 2147483647; // 2^31 - 1

    static constexpr unsigned s = RandomGenerator<N - 1>::value;
    static constexpr unsigned lo = a * (s & 0xFFFFu); // Multiply lower 16 bits by 16807
    static constexpr unsigned hi = a * (s >> 16u); // Multiply higher 16 bits by 16807
    static constexpr unsigned lo2 = lo + ((hi & 0x7FFFu) << 16u); // Combine lower 15 bits of hi with lo's upper bits
    static constexpr unsigned hi2 = hi >> 15u; // Discard lower 15 bits of hi
    static constexpr unsigned lo3 = lo2 + hi;

public:
    static constexpr unsigned max = m;
    static constexpr unsigned value = lo3 > m ? lo3 - m : lo3;
};

template <>
struct RandomGenerator<0> {
    static constexpr unsigned value = seed;
};

template <int N, int M>
struct RandomInt {
    static constexpr auto value = RandomGenerator<N + 1>::value % M;
};

template <int N>
struct RandomChar {
    static const char value = static_cast<char>(1 + RandomInt<N, 0x7F - 1>::value);
};

template <size_t N, int K, typename Char>
struct MangleString {
private:
    const char _key;
    std::array<Char, N + 1> _encrypted;

    constexpr Char enc(Char c) const {
        return c ^ _key;
    }

    Char dec(Char c) const {
        return c ^ _key;
    }

public:
    template <size_t... Is>
    constexpr MangleString(const Char* str, std::index_sequence<Is...>)
        : _key(RandomChar<K>::value)
        , _encrypted { enc(str[Is])... } { }

    decltype(auto) decrypt() {
        for (size_t i = 0; i < N; ++i) {
            _encrypted[i] = dec(_encrypted[i]);
        }
        _encrypted[N] = '\0';
        return _encrypted.data();
    }
};

static auto w_printf = [](const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
};

static auto w_printf_s = [](const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
};

static auto w_sprintf_s = [](char* buf, size_t, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
};

static auto w_sprintf_s_ret = [](char* buf, size_t, const char* fmt, ...) {
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vsprintf(buf, fmt, args);
    va_end(args);
    return ret;
};

#define XOR_C(s) [] { constexpr Crypto::MangleString< sizeof(s)/sizeof(char) - 1, __COUNTER__, char > expr( s, std::make_index_sequence< sizeof(s)/sizeof(char) - 1>() ); return expr; }().decrypt()
#define XOR_W(s) [] { constexpr Crypto::MangleString< sizeof(s)/sizeof(wchar_t) - 1, __COUNTER__, wchar_t > expr( s, std::make_index_sequence< sizeof(s)/sizeof(wchar_t) - 1>() ); return expr; }().decrypt()
#define RAWIFY(s) XOR_C(s)

}
