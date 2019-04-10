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

#include <array>                                  // for std::array
#include <cstdio>                                 // for strerror_r
#include <cstddef>                                // for std::size_t
#include <cstring>                                // for strerror_r
#include <errno.h>
#include <string>                                 // for std::string


namespace error {

namespace detail {

inline const auto& errnos() {
	static const auto _errnos = []{
		constexpr size_t num_errors = []{
			std::size_t max = 0;
			#define __ERRNO(name) if (name + 1 > max) max = name + 1;
			#include "errnos.h"
			#undef __ERRNO
			return max < 256 ? 256 : max;
		}();
		std::array<std::string, num_errors> _names{};
		std::array<std::string, num_errors> _descriptions{};
		_names[0] = "UNDEFINED";
		#define __ERRNO(name) _names[name] = #name;
		#include "errnos.h"
		#undef __ERRNO
		for (size_t i = 0; i < num_errors; ++i) {
			char description[100];
			int errnum = static_cast<int>(i);
			if (strerror_r(errnum, description, sizeof(description)) == 0) {
				_descriptions[i] = description;
			} else {
				snprintf(description, sizeof(description), "Unknown error: %d", errnum);
				_descriptions[i] = description;
			}
		}
		return std::make_pair(_names, _descriptions);
	}();
	return _errnos;
}

}  /* namespace detail */


inline const auto&
name(int errnum)
{
	auto& _errnos = detail::errnos().first;
	if (errnum >= 0 && errnum < static_cast<int>(_errnos.size())) {
		auto& name = _errnos[errnum];
		if (!name.empty()) {
			return name;
		}
	}
	static const std::string _unknown = "UNKNOWN";
	return _unknown;
}


inline const auto&
description(int errnum)
{
	auto& _errnos = detail::errnos().second;
	if (errnum >= 0 && errnum < static_cast<int>(_errnos.size())) {
		auto& name = _errnos[errnum];
		if (!name.empty()) {
			return name;
		}
	}
	static const std::string _unknown = "Unknown error";
	return _unknown;
}

}  /* namespace error */
