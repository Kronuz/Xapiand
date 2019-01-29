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

/*
 * Define macro for endianess.
 */

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
