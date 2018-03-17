/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "xapiand.h"

#include <algorithm>          // for std::count
#include <chrono>             // for std::chrono
#include <cstdio>             // for std::vsnprintf
#include <string>             // for std::string
#include <string_view>        // for std::string_view
#include <type_traits>        // for std::forward
#include <vector>             // for std::vector

#include "fmt/printf.h"       // fmt::printf_args, fmt::vsprintf, fmt::make_printf_args
#include "static_string.hh"   // for static_string
#include "split.h"            // for Split


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
	os << obj.to_string();
}


namespace string {

// converts a character to lowercase
constexpr static char tolower(char c) noexcept {
	constexpr char _[256]{
		'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
		'\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
		'\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
		'\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
		   ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '"',
		   '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
		   '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
		   '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
		   '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
		   'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
		   'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
		   'x',    'y',    'z',    '[',    '\\',   ']',    '^',    '_',
		   '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
		   'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
		   'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
		   'x',    'y',    'z',    '{',    '|',    '}',    '~', '\x7f',
		'\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
		'\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
		'\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
		'\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
		'\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
		'\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
		'\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
		'\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
		'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
		'\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
		'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
		'\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
		'\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
		'\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
		'\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
		'\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
	};
	return _[static_cast<unsigned char>(c)];
}


// converts a character to uppercase
constexpr static char toupper(char c) noexcept {
	constexpr char _[256]{
		'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
		'\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
		'\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
		'\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
		   ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '"',
		   '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
		   '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
		   '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
		   '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
		   'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
		   'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
		   'X',    'Y',    'Z',    '[',    '\\',   ']',    '^',    '_',
		   '`',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
		   'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
		   'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
		   'X',    'Y',    'Z',    '{',    '|',    '}',    '~', '\x7f',
		'\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87',
		'\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
		'\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
		'\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
		'\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
		'\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
		'\xb0', '\xb1', '\xb2', '\xb3', '\xb4', '\xb5', '\xb6', '\xb7',
		'\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
		'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7',
		'\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
		'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
		'\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
		'\xe0', '\xe1', '\xe2', '\xe3', '\xe4', '\xe5', '\xe6', '\xe7',
		'\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
		'\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7',
		'\xf8', '\xf9', '\xfa', '\xfb', '\xfc', '\xfd', '\xfe', '\xff',
	};
	return _[static_cast<unsigned char>(c)];
}


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


template <class T, class... Args>
struct _is_callable {
	template <class U> static auto test(U*p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
	template <class U> static auto test(...) -> decltype(std::false_type());
	static constexpr auto value = decltype(test<T>(nullptr))::value;
};


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<_is_callable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, std::string_view last_delimiter, UnaryPredicate pred) {
	std::vector<T> filtered_values(values.size());
	std::remove_copy_if(values.begin(), values.end(), filtered_values.begin(), pred);
	return join(filtered_values, delimiter, last_delimiter);
}


template <typename T, typename UnaryPredicate, typename = std::enable_if_t<_is_callable<UnaryPredicate, T>::value>>
inline std::string join(const std::vector<T>& values, std::string_view delimiter, UnaryPredicate pred) {
	return join(values, delimiter, delimiter, pred);
}


template <typename T>
inline std::vector<std::string_view> split(std::string_view value, const T& sep) {
	std::vector<std::string_view> values;
	Split<T>::split(value, sep, std::back_inserter(values));
	return values;
}


template <typename... Args>
inline std::string format(std::string_view format, Args&&... args) {
	return fmt::vsprintf(format, fmt::make_printf_args(std::forward<Args>(args)...));
}


inline std::string indent(std::string_view str, char sep, int level, bool indent_first=true) {
	std::string result;
	result.reserve(((indent_first ? 1 : 0) + std::count(str.begin(), str.end(), '\n')) * level);

	if (indent_first) {
		result.append(level, sep);
	}

	Split<char> lines(str, '\n');
	auto it = lines.begin();
	assert(it != lines.end());
	for (; !it.last(); ++it) {
		const auto& line = *it;
		result.append(line);
		result.push_back('\n');
		result.append(level, sep);
	}
	const auto& line = *it;
	result.append(line);
	result.push_back('\n');

	return result;
}


inline std::string center(std::string_view str, int width) {
	std::string result;
	for (auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2); idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
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
	std::transform(str.begin(), str.end(), std::back_inserter(result), string::toupper);
	return result;
}


inline std::string lower(std::string_view str) {
	std::string result;
	std::transform(str.begin(), str.end(), std::back_inserter(result), string::tolower);
	return result;
}


inline bool startswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && text.compare(0, token.size(), token) == 0;
}


inline bool startswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(0) == ch;
}


inline bool endswith(std::string_view text, std::string_view token) {
	return text.size() >= token.size() && std::equal(text.begin() + text.size() - token.size(), text.end(), token.begin());
}


inline bool endswith(std::string_view text, char ch) {
	return text.size() >= 1 && text.at(text.size() - 1) == ch;
}


inline void to_upper(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), string::toupper);
}


inline void to_lower(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), string::tolower);
}


std::string from_bytes(size_t bytes, bool colored = false);
std::string from_small_time(long double seconds, bool colored = false);
std::string from_time(long double seconds, bool colored = false);
std::string from_delta(long double nanoseconds, bool colored = false);
std::string from_delta(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored = false);

} // namespace string
