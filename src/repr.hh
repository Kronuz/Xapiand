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

#include <cstddef>            // for std::size_t
#include <string>             // for std::string
#include "string_view.hh"     // for std::string_view


std::string repr(const void* p, std::size_t size, bool friendly = true, char quote = '\'', std::size_t max_size = 0);

inline std::string repr(const void* p, const void* e, bool friendly = true, char quote = '\'', std::size_t max_size = 0) {
	return repr(p, static_cast<const char*>(e) - static_cast<const char*>(p), friendly, quote, max_size);
}

template <typename T, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<T>>::value or std::is_same<std::string_view, std::decay_t<T>>::value>>
inline std::string repr(T&& s, bool friendly = true, char quote = '\'', std::size_t max_size = 0) {
	return repr(s.data(), s.size(), friendly, quote, max_size);
}

inline std::string repr(std::string_view s, bool friendly = true, char quote = '\'', std::size_t max_size = 0) {
	return repr(s.data(), s.size(), friendly, quote, max_size);
}

template<typename T, std::size_t N>
inline std::string repr(T (&s)[N], bool friendly = true, char quote = '\'', std::size_t max_size = 0) {
	return repr(s, N - 1, friendly, quote, max_size);
}
