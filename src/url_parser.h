/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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

#include <cstdio>
#include <string>


#define COMMAND_PREFIX ":"


std::string urldecode(const void *p, size_t size);
inline std::string urldecode(const std::string& string) {
	return urldecode(string.data(), string.size());
}
template<typename T, std::size_t N>
inline std::string urldecode(T (&s)[N]) {
	return urldecode(s, N - 1);
}


class QueryParser {
	std::string query;

public:
	size_t len;
	const char *off;

	QueryParser();

	void clear() noexcept;
	void rewind() noexcept;
	int init(const std::string& q);
	int next(const char *name);

	std::string get();
};


class PathParser {
	std::string path;
	const char *off;

public:
	enum class State : uint8_t {
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
	size_t len_id;
	const char *off_id;

	PathParser();

	void clear() noexcept;
	void rewind() noexcept;
	State init(const std::string& p);
	State next();

	void skip_id() noexcept;

	std::string get_pth();
	std::string get_hst();
	std::string get_nsp();
	std::string get_pmt();
	std::string get_ppmt();
	std::string get_cmd();
	std::string get_id();
};
