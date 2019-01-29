/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#ifndef __has_builtin         // Optional of course
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers
#endif

#if defined(__linux__)
	#include <endian.h>
#elif defined(__APPLE__)
	#include <machine/endian.h>
	#define __BYTE_ORDER    BYTE_ORDER
	#define __BIG_ENDIAN    BIG_ENDIAN
	#define __LITTLE_ENDIAN LITTLE_ENDIAN
	#define __PDP_ENDIAN    PDP_ENDIAN
#elif defined(__FreeBSD__)
	#include <sys/endian.h>
	#define __BYTE_ORDER    _BYTE_ORDER
	#define __BIG_ENDIAN    _BIG_ENDIAN
	#define __LITTLE_ENDIAN _LITTLE_ENDIAN
	#define __PDP_ENDIAN    _PDP_ENDIAN
#elif defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
	#include <include/endian.h>
	#define __BYTE_ORDER    BYTE_ORDER
	#define __BIG_ENDIAN    BIG_ENDIAN
	#define __LITTLE_ENDIAN LITTLE_ENDIAN
	#define __PDP_ENDIAN    PDP_ENDIAN
#endif


#if (defined(__clang__) && __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap64)) || (defined(__GNUC__ ) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
//  GCC and Clang recent versions provide intrinsic byte swaps via builtins
//  prior to 4.8, gcc did not provide __builtin_bswap16 on some platforms so we emulate it
//  see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52624
//  Clang has a similar problem, but their feature test macros make it easier to detect
#  ifndef bswap16
#    if (defined(__clang__) && __has_builtin(__builtin_bswap16)) || (defined(__GNUC__) &&(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
#      define bswap16(x) __builtin_bswap16((x))
#    else
#      define bswap16(x) (__builtin_bswap32((x) << 16))
#    endif
#  endif
#  ifndef bswap32
#    define bswap32(x) __builtin_bswap32((x))
#  endif
#  ifndef bswap64
#    define bswap64(x) __builtin_bswap64((x))
#  endif

#elif defined(__linux__)
// Linux systems provide the byteswap.h header, with
// don't check for obsolete forms defined(linux) and defined(__linux) on the theory that
// compilers that predefine only these are so old that byteswap.h probably isn't present.
#  include <byteswap.h>
#  ifndef bswap16
#    define bswap16(x) bswap_16((x))
#  endif
#  ifndef bswap32
#    define bswap32(x) bswap_32((x))
#  endif
#  ifndef bswap64
#    define bswap64(x) bswap_64((x))
#  endif

#elif defined(_MSC_VER)
// Microsoft documents these as being compatible since Windows 95 and specificly
// lists runtime library support since Visual Studio 2003 (aka 7.1).
#  include <cstdlib>
#  ifndef bswap16
#    define bswap16(x) _byteswap_ushort((x))
#  endif
#  ifndef bswap32
#    define bswap32(x) _byteswap_ulong((x))
#  endif
#  ifndef bswap64
#    define bswap64(x) _byteswap_uint64((x))
#  endif

#else
#  ifndef bswap16
#    define bswap16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#  endif
#  ifndef bswap32
#    define bswap32(x) ((bswap16(((x) & 0xFFFF0000) >> 16)) | ((bswap16((x) & 0x0000FFFF)) << 16))
#  endif
#  ifndef bswap64
#    define bswap64(x) ((bswap64(((x) & 0xFFFFFFFF00000000) >> 32)) | ((bswap64((x) & 0x00000000FFFFFFFF)) << 32))
#  endif
#endif


#if __BYTE_ORDER == __LITTLE_ENDIAN
#  ifndef htobe16
#    define htobe16(x) bswap16((x))
#  endif
#  ifndef htobe32
#    define htobe32(x) bswap32((x))
#  endif
#  ifndef htobe64
#    define htobe64(x) bswap64((x))
#  endif
#  ifndef htole16
#    define htole16(x) ((uint16_t)(x))
#  endif
#  ifndef htole32
#    define htole32(x) ((uint32_t)(x))
#  endif
#  ifndef htole64
#    define htole64(x) ((uint64_t)(x))
#  endif

#  ifndef be16toh
#    define be16toh(x) bswap16((x))
#  endif
#  ifndef be32toh
#    define be32toh(x) bswap32((x))
#  endif
#  ifndef be64toh
#    define be64toh(x) bswap64((x))
#  endif
#  ifndef le16toh
#    define le16toh(x) ((uint16_t)(x))
#  endif
#  ifndef le32toh
#    define le32toh(x) ((uint32_t)(x))
#  endif
#  ifndef le64toh
#    define le64toh(x) ((uint64_t)(x))
#  endif

// HTM's trixel's ids are represented in 7 bytes.
#  define htobe56(x) (bswap64((x) << 8))
#  define htole56(x) ((uint64_t)(x) & 0xffffffffffffffULL)
#  define be56toh(x) (bswap64((x) << 8))
#  define le56toh(x) ((uint64_t)(x) & 0xffffffffffffffULL)

#elif __BYTE_ORDER == __BIG_ENDIAN
#  ifndef htobe16
#    define htobe16(x) ((uint16_t)(x))
#  endif
#  ifndef htobe32
#    define htobe32(x) ((uint32_t)(x))
#  endif
#  ifndef htobe64
#    define htobe64(x) ((uint64_t)(x))
#  endif
#  ifndef htole16
#    define htole16(x) bswap16((x))
#  endif
#  ifndef htole32
#    define htole32(x) bswap32((x))
#  endif
#  ifndef htole64
#    define htole64(x) bswap64((x))
#  endif

#  ifndef be16toh
#    define be16toh(x) ((uint16_t)(x))
#  endif
#  ifndef be32toh
#    define be32toh(x) ((uint32_t)(x))
#  endif
#  ifndef be64toh
#    define be64toh(x) ((uint64_t)(x))
#  endif
#  ifndef le16toh
#    define le16toh(x) bswap16((x))
#  endif
#  ifndef le32toh
#    define le32toh(x) bswap32((x))
#  endif
#  ifndef le64toh
#    define le64toh(x) bswap64((x))
#  endif

// HTM's trixel's ids are represented in 7 bytes.
#  define htobe56(x) ((uint64_t)(x) & 0xffffffffffffff)
#  define htole56(x) (bswap64((x) << 8))
#  define be56toh(x) ((uint64_t)(x) & 0xffffffffffffff)
#  define le56toh(x) (bswap64((x) << 8))

#else
#  error "unable to determine endianess!"
#endif
