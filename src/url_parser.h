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

#include <cstdio>
#include <string>
#include <string_view>       // for std::string_view


#define COMMAND__ ":"
constexpr const char command__ = COMMAND__[0];


std::string urldecode(const void *p, size_t size, char plus = ' ', char amp = '&', char colon = ';', char eq = '=');
inline std::string urldecode(std::string_view string, char plus = ' ', char amp = '&', char colon = ';', char eq = '=') {
	return urldecode(string.data(), string.size(), plus, amp, colon, eq);
}
template<typename T, std::size_t N_PLUS_1>
inline std::string urldecode(T (&s)[N_PLUS_1], char plus = ' ', char amp = '&', char colon = ';', char eq = '=') {
	return urldecode(s, N_PLUS_1 - 1, plus, amp, colon, eq);
}


class QueryParser {
	std::string query;

public:
	size_t len;
	const char *off;

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
	const char *off;

public:
	enum class State : uint8_t {
		SLC,
		SLB,
		NCM,
		PMT,
		CMD,
		ID,
		NSP,
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
	size_t len_nsp;
	const char *off_nsp;
	size_t len_pmt;
	const char *off_pmt;
	size_t len_ppmt;
	const char *off_ppmt;
	size_t len_cmd;
	const char *off_cmd;
	size_t len_slc;
	const char *off_slc;
	size_t len_id;
	const char *off_id;

	PathParser();

	void clear() noexcept;
	void rewind() noexcept;
	State init(std::string_view p);
	State next();

	void skip_id() noexcept;

	std::string_view get_pth();
	std::string_view get_hst();
	std::string_view get_nsp();
	std::string_view get_pmt();
	std::string_view get_ppmt();
	std::string_view get_cmd();
	std::string_view get_id();
	std::string_view get_slc();
};
