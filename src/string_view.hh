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

#include <ciso646>  // defines _LIBCPP_VERSION

#ifdef __has_include
#define HAS_INCLUDE(x) __has_include(x)
#else
#define HAS_INCLUDE(x) 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define GCC_VERSION 0
#endif

#if (HAS_INCLUDE(<string_view>) && \
      (__cplusplus > 201402L || _LIBCPP_VERSION)) || \
    (defined(_MSVC_LANG) && _MSVC_LANG > 201402L && _MSC_VER >= 1910)
#include <string_view>
#elif (HAS_INCLUDE(<experimental/string_view>) && \
       (GCC_VERSION == 0 || GCC_VERSION >= 501) && \
       __cplusplus >= 201402L)
#include <experimental/string_view>
namespace std {
    using std::experimental::string_view;
}
#else
#error "Need string_view (or experimental/string_view) to compile!"
#endif
