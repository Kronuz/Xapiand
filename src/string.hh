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

#include <algorithm>          // for std::count
#include <cmath>              // for std::isinf, std::isnan
#include <chrono>             // for std::chrono
#include <ostream>            // for std::ostream
#include <string>             // for std::string
#include "string_view.hh"     // for std::string_view
#include <type_traits>        // for std::forward
#include <vector>             // for std::vector

#include "cassert.h"          // for ASSERT
#include "chars.hh"           // for chars::tolower
#include "fmt/format.h"       // for fmt::format
#include "log.h"              // for L_DEBUG_TRY
#include "repr.hh"            // for repr
#include "split.h"            // for Split
#include "static_string.hh"   // for static_string


namespace std {
	inline auto& to_string(std::string& str) {
		return str;
	}

	inline const auto& to_string(const std::string& str) {
		return str;
	}

	template <typename T, int N>
	inline auto to_string(const T (&s)[N]) {
		return std::string(s, N - 1);
	}

	inline auto to_string(const std::string_view& str) {
		return std::string(str);
	}

	template <std::size_t SN, typename ST>
	inline auto to_string(const static_string::static_string<SN, ST>& str) {
		return std::string(str.data(), str.size());
	}

	template <typename T, typename = std::enable_if_t<
		std::is_convertible<decltype(std::declval<T>().to_string()), std::string>::value
	>>
	inline auto to_string(const T& obj) {
		return obj.to_string();
	}
}


// overload << so that it's easy to convert to a string
template <typename T, typename = std::enable_if_t<
	std::is_convertible<decltype(std::declval<T>().to_string()), std::string>::value
>>
std::ostream& operator<<(std::ostream& os, const T& obj) {
	return os << obj.to_string();
}


namespace string {

template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter)
{
	auto it = values.begin();
	auto it_e = values.end();

	auto rit = values.rbegin();
	auto rit_e = values.rend();
	if (rit != rit_e) ++rit;
	auto it_l = rit != rit_e ? rit.base() : it_e;

	std::string result;

	if (it != it_e) {
		result.append(std::to_string(*it++));
	}
	for (; it != it_l; ++it) {
		result.append(delimiter.data(), delimiter.size());
		result.append(std::to_string(*it));
	}
	if (it != it_e) {
		result.append(last_delimiter.data(), last_delimiter.size());
		result.append(std::to_string(*it));
	}

	return result;
}


template <typename T>
inline std::string join(const std::vector<T>& values, std::string_view delimiter) {
	return join(values, delimiter, delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter, UnaryPredicate pred) {
	std::vector<T> filtered_values(values.size());
	filtered_values.erase(std::remove_copy_if(values.begin(), values.end(), filtered_values.begin(), pred), filtered_values.end());
	return join(filtered_values, delimiter, last_delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<std::is_invocable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, UnaryPredicate pred) {
	return join(values, delimiter, delimiter, pred);
}


template <typename S, typename T>
inline std::vector<S> split(const S& value, const T& sep) {
	std::vector<S> values;
	Split<S, T>::split(value, sep, std::back_inserter(values));
	return values;
}


template <typename... Args>
inline std::string format(std::string_view format, Args&&... args) {
	std::string str;
	try {
		str = fmt::format(format, std::forward<Args>(args)...);
	} catch(...) {
		L_EXC("Cannot format {}", repr(format));
		str = format;
	}
	return str;
}


inline std::string indent(std::string_view str, char sep, int level, bool indent_first=true) {
	std::string result;
	result.reserve(((indent_first ? 1 : 0) + std::count(str.begin(), str.end(), '\n')) * level);

	if (indent_first) {
		result.append(level, sep);
	}

	Split<std::string_view> lines(str, '\n');
	auto it = lines.begin();
	ASSERT(it != lines.end());
	for (; !it.last(); ++it) {
		const auto& line = *it;
		result.append(line);
		result.push_back('\n');
		result.append(level, sep);
	}
	const auto& line = *it;
	result.append(line);

	return result;
}


inline std::string left(std::string_view str, int width, bool fill = false) {
	std::string result;
	result.append(str.data(), str.size());
	if (fill) {
		for (auto idx = int(width - str.size()); idx > 0; --idx) {
			result += " ";
		}
	}
	return result;
}


inline std::string center(std::string_view str, int width, bool fill = false) {
	std::string result;
	auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2);
	width -= idx;
	width -= str.size();
	for (; idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
	if (fill) {
		for (; width > 0; --width) {
			result += " ";
		}
	}
	return result;
}


inline std::string right(std::string_view str, int width) {
	std::string result;
	for (auto idx = int(width - str.size()); idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
	return result;
}


inline std::string upper(std::string_view str) {
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), chars::toupper);
	return result;
}


inline std::string lower(std::string_view str) {
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), chars::tolower);
	return result;
}


inline bool startswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && text.compare(0, token.size(), token) == 0;
}


inline bool startswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(0) == ch;
}


inline bool hasupper(std::string_view str) {
	for (const auto& c : str) {
		if (isupper(c) != 0) {
			return true;
		}
	}

	return false;
}


inline bool endswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && std::equal(text.begin() + text.size() - token.size(), text.end(), token.begin());
}


inline bool endswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(text.size() - 1) == ch;
}


inline void toupper(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), chars::toupper);
}


inline void tolower(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), chars::tolower);
}


std::string from_bytes(size_t bytes, const char* prefix = "", bool colored = false);
std::string from_small_time(long double seconds, const char* prefix = "", bool colored = false);
std::string from_time(long double seconds, const char* prefix = "", bool colored = false);
std::string from_delta(long double nanoseconds, const char* prefix = "", bool colored = false);
std::string from_delta(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, const char* prefix = "", bool colored = false);

} // namespace string
