/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
	void __assert_tb(const char* function, const char* filename, unsigned int line, const char* expression);
#ifdef __cplusplus
}
#endif // __cplusplus

#ifndef ASSERT

#include "config.h"           // for XAPIAND_ASSERTS

#ifndef NDEBUG
#ifndef XAPIAND_ASSERTS
#define XAPIAND_ASSERTS 1
#endif
#endif

#ifdef XAPIAND_ASSERTS

#define	ASSERT(e)	((void)0)

#else // XAPIAND_ASSERTS

#include "likely.h"

#if defined(__cplusplus)
#include <cassert>
#else
#include <assert.h>
#endif

#define ASSERT(e) \
	((void) (likely(e) ? ((void)0) : __assert_tb(__func__, __FILE__, __LINE__, #e)))

#endif // XAPIAND_ASSERTS

#ifdef assert
#undef assert
#endif // assert
#define assert ASSERT

#endif // ASSERT
