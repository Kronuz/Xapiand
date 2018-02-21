/*
 * Copyright (c) 2015 Daniel Kirchner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <string>
#include <cstdint>

#include "string_view.h"
#include "lz4/xxhash.h"


//             _               _
// __  ____  _| |__   __ _ ___| |__
// \ \/ /\ \/ / '_ \ / _` / __| '_ \
//  >  <  >  <| | | | (_| \__ \ | | |
// /_/\_\/_/\_\_| |_|\__,_|___/_| |_|

class xxh64 {
	static constexpr uint64_t PRIME1 = 11400714785074694791ULL;
	static constexpr uint64_t PRIME2 = 14029467366897019727ULL;
	static constexpr uint64_t PRIME3 =  1609587929392839161ULL;
	static constexpr uint64_t PRIME4 =  9650029242287828579ULL;
	static constexpr uint64_t PRIME5 =  2870177450012600261ULL;

	static constexpr uint64_t rotl(uint64_t x, int r) {
		return ((x << r) | (x >> (64 - r)));
	}
	static constexpr uint64_t mix1(const uint64_t h, const uint64_t prime, int rshift) {
		return (h ^ (h >> rshift)) * prime;
	}
	static constexpr uint64_t mix2(const uint64_t p, const uint64_t v = 0) {
		return rotl (v + p * PRIME2, 31) * PRIME1;
	}
	static constexpr uint64_t mix3(const uint64_t h, const uint64_t v) {
		return (h ^ mix2 (v)) * PRIME1 + PRIME4;
	}
#ifdef XXH64_BIG_ENDIAN
	static constexpr uint32_t endian32(const char *v) {
		return uint32_t(uint8_t(v[3]))|(uint32_t(uint8_t(v[2]))<<8)
			   |(uint32_t(uint8_t(v[1]))<<16)|(uint32_t(uint8_t(v[0]))<<24);
	}
	static constexpr uint64_t endian64(const char *v) {
		return uint64_t(uint8_t(v[7]))|(uint64_t(uint8_t(v[6]))<<8)
			   |(uint64_t(uint8_t(v[5]))<<16)|(uint64_t(uint8_t(v[4]))<<24)
			   |(uint64_t(uint8_t(v[3]))<<32)|(uint64_t(uint8_t(v[2]))<<40)
			   |(uint64_t(uint8_t(v[1]))<<48)|(uint64_t(uint8_t(v[0]))<<56);
	}
#else
	static constexpr uint32_t endian32(const char *v) {
		return uint32_t(uint8_t(v[0]))|(uint32_t(uint8_t(v[1]))<<8)
			   |(uint32_t(uint8_t(v[2]))<<16)|(uint32_t(uint8_t(v[3]))<<24);
	}
	static constexpr uint64_t endian64 (const char *v) {
		return uint64_t(uint8_t(v[0]))|(uint64_t(uint8_t(v[1]))<<8)
			   |(uint64_t(uint8_t(v[2]))<<16)|(uint64_t(uint8_t(v[3]))<<24)
			   |(uint64_t(uint8_t(v[4]))<<32)|(uint64_t(uint8_t(v[5]))<<40)
			   |(uint64_t(uint8_t(v[6]))<<48)|(uint64_t(uint8_t(v[7]))<<56);
	}
#endif
	static constexpr uint64_t fetch64(const char *p, const uint64_t v = 0) {
		return mix2 (endian64 (p), v);
	}
	static constexpr uint64_t fetch32(const char *p) {
		return uint64_t (endian32 (p)) * PRIME1;
	}
	static constexpr uint64_t fetch8(const char *p) {
		return uint8_t (*p) * PRIME5;
	}
	static constexpr uint64_t finalize(const uint64_t h, const char *p, uint64_t len) {
		return (len >= 8) ? (finalize (rotl (h ^ fetch64 (p), 27) * PRIME1 + PRIME4, p + 8, len - 8)) :
			   ((len >= 4) ? (finalize (rotl (h ^ fetch32 (p), 23) * PRIME2 + PRIME3, p + 4, len - 4)) :
				((len > 0) ? (finalize (rotl (h ^ fetch8 (p), 11) * PRIME1, p + 1, len - 1)) :
				 (mix1 (mix1 (mix1 (h, PRIME2, 33), PRIME3, 29), 1, 32))));
	}
	static constexpr uint64_t h32bytes(const char *p, uint64_t len, const uint64_t v1,const uint64_t v2, const uint64_t v3, const uint64_t v4) {
		return (len >= 32) ? h32bytes (p + 32, len - 32, fetch64 (p, v1), fetch64 (p + 8, v2), fetch64 (p + 16, v3), fetch64 (p + 24, v4)) :
			   mix3 (mix3 (mix3 (mix3 (rotl (v1, 1) + rotl (v2, 7) + rotl (v3, 12) + rotl (v4, 18), v1), v2), v3), v4);
	}
	static constexpr uint64_t h32bytes(const char *p, uint64_t len, const uint64_t seed) {
		return h32bytes (p, len, seed + PRIME1 + PRIME2, seed + PRIME2, seed, seed - PRIME1);
	}

public:
	static constexpr uint64_t hash(const char *p, uint64_t len, uint64_t seed=0) {
		return finalize((len >= 32 ? h32bytes(p, len, seed) : seed + PRIME5) + len, p + (len & ~0x1F), len & 0x1F);
	}

	template <size_t N>
	static constexpr uint64_t hash(const char(&s)[N], uint64_t seed=0) {
		return hash(s, N - 1, seed);
	}

	static uint64_t hash(string_view str, uint64_t seed=0) {
		return XXH64(str.data(), str.size(), seed);
	}
};


class xxh32 {
	/* constexpr xxh32::hash() not implemented! */

public:
	static uint32_t hash(string_view str, uint32_t seed=0) {
		return XXH32(str.data(), str.size(), seed);
	}
};


constexpr uint32_t operator"" _XXH(const char* s, size_t size) {
	return xxh64::hash(s, size, 0);
}


//   __            _
//  / _|_ ____   _/ | __ _
// | |_| '_ \ \ / / |/ _` |
// |  _| | | \ V /| | (_| |
// |_| |_| |_|\_/ |_|\__,_|

template <typename T, T prime, T offset>
struct fnv1a {
	static constexpr T hash(const char *p, std::size_t len) {
        return len == 1 ? (offset ^ static_cast<unsigned char>(*p)) * prime : (hash(p, len - 1) ^ static_cast<unsigned char>(p[len - 1])) * prime;
	}

	template <size_t N>
	static constexpr T hash(const char(&s)[N]) {
		return hash(s, N - 1);
	}

	static T hash(string_view str) {
        T hash = offset;
		for (char c : str) {
            hash = (hash ^ static_cast<unsigned char>(c)) * prime;
        }
        return hash;
    }
};
using fnv1a32 = fnv1a<std::uint32_t, 0x1000193UL, 0x811c9dc5UL>;
using fnv1a64 = fnv1a<std::uint64_t, 0x100000001b3ULL, 0xcbf29ce484222325ULL>;
// using fnv1a128 = fnv1a<__uint128_t, 0x10000000000000000000159ULL, 0xcf470aac6cb293d2f52f88bf32307f8fULL>;


//      _        _               _               _
//  ___| |_ _ __(_)_ __   __ _  | |__   __ _ ___| |__
// / __| __| '__| | '_ \ / _` | | '_ \ / _` / __| '_ \
// \__ \ |_| |  | | | | | (_| | | | | | (_| \__ \ | | |
// |___/\__|_|  |_|_| |_|\__, | |_| |_|\__,_|___/_| |_|
//                       |___/
template <typename T>
struct strh {
	static constexpr std::uint64_t hash(const char *p, std::size_t len) {
        return len == 1 ? static_cast<unsigned char>(*p) : (hash(p, len - 1) * 256) + static_cast<unsigned char>(p[len - 1]);
	}

	template <size_t N>
	static constexpr std::uint64_t hash(const char(&s)[N]) {
		return hash(s, N - 1);
	}

	static std::uint64_t hash(string_view str) {
		std::uint64_t hash = 0;
		for (char c : str) {
			hash = (hash * 256) + static_cast<unsigned char>(c);
		}
		return hash;
	}
};
using strh64 = strh<std::uint64_t>;
using strh128 = strh<__uint128_t>;
