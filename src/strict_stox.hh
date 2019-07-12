/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <algorithm>          // for std::min
#include <cstdio>             // for std::size_t
#include <cstdlib>            // for std::strto*
#include <cstring>            // for std::strncpy
#include <limits>             // for std::numeric_limits
#include <string_view>        // for std::string_view
#include <errno.h>            // for errno
#include <type_traits>        // for std::true_type, std::false_type
#include <utility>            // for std::swap

#include "exception.h"        // for InvalidArgument, OutOfRange


// Types conversion strict_stox and strict_nothrow_stox
template <typename F>
class Stox {
	const char * const name;
	F func;

	template <typename T, typename... Args>
	auto _stox(std::true_type, const std::string_view& str, std::size_t* idx, Args&&... args) noexcept {
		char buf[64]{};
		size_t size = std::min(str.size(), sizeof(buf) - 1);
		std::strncpy(buf, str.data(), size);
		auto end = buf + size;
		auto ptr = const_cast<char*>(end);
		auto r = func(buf, &ptr, std::forward<Args>(args)...);
		if (errno) return static_cast<decltype(r)>(0);
		if (ptr == buf) {
			errno = EINVAL;
			return static_cast<decltype(r)>(0);
		}
		if (idx) {
			*idx = static_cast<size_t>(ptr - buf);
		} else if (ptr != end || size != str.size()) {
			errno = EINVAL;
			return static_cast<decltype(r)>(0);
		}
		return r;
	}

	template <typename T, typename... Args>
	auto _stox(std::false_type, const std::string_view& str, std::size_t* idx, Args&&... args) noexcept {
		auto r = _stox<void>(std::true_type{}, str, idx, std::forward<Args>(args)...);
		if (errno) return static_cast<T>(0);
		if (r < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < r) {
			errno = ERANGE;
			return static_cast<T>(0);
		}
		return static_cast<T>(r);
	}

public:
	Stox(const char* _name, F _func)
		: name(_name),
		  func(_func) { }

	template <typename T, typename... Args>
	auto stox_nothrow(int* errno_save, const std::string_view& str, std::size_t* idx, Args&&... args) {
		auto _errno = errno;
		errno = 0;
		auto r = _stox<T>(std::is_same<void, T>{}, str, idx, std::forward<Args>(args)...);
		std::swap(errno, _errno);
		if (errno_save != nullptr) {
			*errno_save = _errno;
		}
		return r;
	}

	template <typename T, typename... Args>
	auto stox_throw(const std::string_view& str, std::size_t* idx, Args&&... args) {
		auto _errno = errno;
		errno = 0;
		auto r = _stox<T>(std::is_same<void, T>{}, str, idx, std::forward<Args>(args)...);
		std::swap(errno, _errno);
		switch (_errno) {
			case EINVAL:
				THROW(InvalidArgument, "{}: Cannot convert value: {}", name, str);
			case ERANGE:
				THROW(OutOfRange, "{}: Out of range value: {}", name, str);
			default:
				break;
		}
		return r;
	}
};
#define STOXIFY_BASE(name, func, T) \
static Stox<decltype(&func)> _strict_##name(#name, func); \
inline auto strict_##name(int* errno_save, std::string_view str, std::size_t* idx = nullptr, int base = 10) noexcept { \
	return _strict_##name.stox_nothrow<T>(errno_save, str, idx, base); \
} \
inline auto strict_##name(std::string_view str, std::size_t* idx = nullptr, int base = 10) { \
	return _strict_##name.stox_throw<T>(str, idx, base); \
}
#define STOXIFY(name, func, T) \
static Stox<decltype(&func)> _strict_##name(#name, func); \
inline auto strict_##name(int* errno_save, std::string_view str, std::size_t* idx = nullptr) noexcept { \
	return _strict_##name.stox_nothrow<T>(errno_save, str, idx); \
} \
inline auto strict_##name(std::string_view str, std::size_t* idx = nullptr) { \
	return _strict_##name.stox_throw<T>(str, idx); \
}
STOXIFY_BASE(stoul, std::strtoul, void)
STOXIFY_BASE(stoull, std::strtoull, void)
STOXIFY_BASE(stoi, std::strtol, int)
STOXIFY_BASE(stou, std::strtoul, unsigned)
STOXIFY_BASE(stol, std::strtol, void)
STOXIFY_BASE(stoll, std::strtoll, void)
STOXIFY_BASE(stoz, std::strtoull, std::size_t)
STOXIFY(stof, std::strtof, void)
STOXIFY(stod, std::strtod, void)
STOXIFY(stold, std::strtold, void)
#undef STOXIFY_BASE
#undef STOXIFY
