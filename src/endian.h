/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#ifndef __has_builtin         // Optional of course
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers
#endif

#if (defined(__clang__) && __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap64)) || (defined(__GNUC__ ) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
//  GCC and Clang recent versions provide intrinsic byte swaps via builtins
//  prior to 4.8, gcc did not provide __builtin_bswap16 on some platforms so we emulate it
//  see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52624
//  Clang has a similar problem, but their feature test macros make it easier to detect
#  if (defined(__clang__) && __has_builtin(__builtin_bswap16)) || (defined(__GNUC__) &&(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
#    define bswap16(x) (__builtin_bswap16(x))
#  else
#    define bswap16(x) (__builtin_bswap32((x) << 16))
#  endif
#  define bswap32(x) (__builtin_bswap32(x))
#  define bswap64(x) (__builtin_bswap64(x))
#elif defined(__linux__)
// Linux systems provide the byteswap.h header, with
// don't check for obsolete forms defined(linux) and defined(__linux) on the theory that
// compilers that predefine only these are so old that byteswap.h probably isn't present.
#  include <byteswap.h>
#  define bswap16(x) (bswap_16(x))
#  define bswap32(x) (bswap_32(x))
#  define bswap64(x) (bswap_64(x))
#elif defined(_MSC_VER)
// Microsoft documents these as being compatible since Windows 95 and specificly
// lists runtime library support since Visual Studio 2003 (aka 7.1).
#  include <cstdlib>
#  define bswap16(x) (_byteswap_ushort(x))
#  define bswap32(x) (_byteswap_ulong(x))
#  define bswap64(x) (_byteswap_uint64(x))
#else
#  define bswap16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#  define bswap32(x) ((bswap16(((x) & 0xFFFF0000) >> 16)) | ((bswap16((x) & 0x0000FFFF)) << 16))
#  define bswap64(x) ((bswap64(((x) & 0xFFFFFFFF00000000) >> 32)) | ((bswap64((x) & 0x00000000FFFFFFFF)) << 32))
#endif


#if __BYTE_ORDER == __BIG_ENDIAN
// No translation needed for big endian system.
#  define htobe16(val) (val) // uint16_t, short in 2 bytes
#  define htole16(val) bswap16(val)
#  define be16toh(val) (val) // uint16_t, short in 2 bytes
#  define le16toh(val) bswap16(val)

#  define htobe32(val) (val) // Unsigned int is represent in 4 bytes
#  define htole32(val) bswap32(val)
#  define be32toh(val) (val) // Unsigned int is represent in 4 bytes
#  define le32toh(val) bswap32(val)

#  define htobe56(val) (val) // HTM's trixel's ids are represent in 7 bytes.
#  define htole56(val) (bswap64((val) << 8))
#  define be56toh(val) (val) // HTM's trixel's ids are represent in 7 bytes.
#  define le56toh(val) (bswap64((val) << 8))

#  define htobe64(val) (val) // uint64_t is represent in 8 bytes
#  define htole64(val) bswap64(val)
#  define be64toh(val) (val) // uint64_t is represent in 8 bytes
#  define le64toh(val) bswap64(val)

#elif __BYTE_ORDER == __LITTLE_ENDIAN
// Swap 7 byte, 56 bit values. (If it is not big endian, It is considered little endian)
#  define htobe16(val) bswap16(val)
#  define htole16(val) (val)
#  define be16toh(val) bswap16(val)
#  define le16toh(val) (val)

#  define htobe32(val) bswap32(val)
#  define htole32(val) (val)
#  define be32toh(val) bswap32(val)
#  define le32toh(val) (val)

#  define htobe56(val) (bswap64((val) << 8))
#  define htole56(val) (val)
#  define be56toh(val) (bswap64((val) << 8))
#  define le56toh(val) (val)

#  define htobe64(val) bswap64(val)
#  define htole64(val) (val)
#  define be64toh(val) bswap64(val)
#  define le64toh(val) (val)
#endif
