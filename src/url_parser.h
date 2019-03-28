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

#include <cstdio>
#include <string>
#include <string_view>       // for std::string_view


#define COMMAND__ ":"
constexpr const char command__ = COMMAND__[0];


std::string urldecode(const void *p, size_t size, char plus = ' ', char amp = '&', char colon = ';', char eq = '=', char encoded = '\0');
inline std::string urldecode(std::string_view string, char plus = ' ', char amp = '&', char colon = ';', char eq = '=', char encoded = '\0') {
	return urldecode(string.data(), string.size(), plus, amp, colon, eq, encoded);
}
template<typename T, std::size_t N_PLUS_1>
inline std::string urldecode(T (&s)[N_PLUS_1], char plus = ' ', char amp = '&', char colon = ';', char eq = '=', char encoded = '\0') {
	return urldecode(s, N_PLUS_1 - 1, plus, amp, colon, eq, encoded);
}


class QueryParser {
	std::string query;
	std::string query_decoded;

public:
	const char *off;
	size_t len;

	QueryParser();

	void clear() noexcept;
	void rewind() noexcept;
	int init(std::string_view q);

	template<typename T, std::size_t N_PLUS_1>
	int next(T (&s)[N_PLUS_1]) {
		return next(s, N_PLUS_1 - 1);
	}
	int next(const char *name, size_t name_len);

	std::string_view get();
};


class PathParser {
	std::string path;
	std::string path_decoded;

	const char *off;

public:
	enum class State : uint8_t {
		ID_SLC,               // Id and selector
		SLF,                  // Selector found
		SLC,                  // Selector state
		SLB,                  // Selector brackets
		SLB_SUB,              // Selector brackets subfield
		SLB_SPACE_OR_COMMA,   // Expecting space or comma
		CMD,
		NCM,
		ID,
		PTH,
		HST,
		END,
		INVALID_STATE,
		INVALID_NSP,
		INVALID_HST
	};

	size_t len_pth;
	const char *off_pth;
	size_t len_hst;
	const char *off_hst;
	size_t len_slc;
	const char *off_slc;
	size_t len_id;
	const char *off_id;
	size_t len_cmd;
	const char *off_cmd;

	PathParser();

	void clear() noexcept;
	void rewind() noexcept;
	State init(std::string_view p);
	State next();

	bool has_pth() noexcept;

	std::string_view get_pth();
	std::string_view get_hst();
	std::string_view get_id();
	std::string_view get_slc();
	std::string_view get_cmd();
};
