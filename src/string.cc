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

#include "string.hh"

#include "colors.h"

#include <cmath>              // for std::log, std::pow


static inline std::string humanize(long double delta, bool colored, const int i, const int n, const long double div, const char* prefix, const char* const units[], const long double scaling[], const char* const colors[], long double rounding) {
	long double num = delta;

	if (delta < 0) { delta = -delta; }
	int order = (delta == 0) ? n : -std::floor(std::log(delta) / div);
	order += i;
	if (order < 0) { order = 0;
	} else if (order > n) { order = n; }

	const char* color = colored ? colors[order] : "";
	num = std::round(rounding * num / scaling[order]) / rounding;
	const char* unit = units[order];
	const char* reset = colored ? colors[n + 1] : "";

	return string::format("%s%s%s%s%s", color, prefix, string::Number(static_cast<double>(num)), unit, reset);
}


static inline int find_val(long double val, const long double* input, int i=0) {
	return (*input == val) ? i : find_val(val, input + 1, i + 1);
}


static inline std::string _from_bytes(size_t bytes, const char* prefix, bool colored) {
	static const long double base = 1024;
	static const long double div = std::log(base);
	static const long double scaling[] = { std::pow(base, 8), std::pow(base, 7), std::pow(base, 6), std::pow(base, 5), std::pow(base, 4), std::pow(base, 3), std::pow(base, 2), std::pow(base, 1), 1 };
	static const char* const units[] = { "YiB", "ZiB", "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
	static const char* const colors[] = { BROWN, BROWN, BROWN, BROWN, DARK_ORANGE, YELLOW_GREEN, MEDIUM_SEA_GREEN, MEDIUM_SEA_GREEN, MEDIUM_SEA_GREEN, CLEAR_COLOR };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(bytes, colored, i, n, div, prefix, units, scaling, colors, 10.0L);
}

std::string string::from_bytes(size_t bytes, const char* prefix, bool colored) {
	return _from_bytes(bytes, prefix, colored);
}


static inline std::string _from_small_time(long double seconds, const char* prefix, bool colored) {
	static const long double base = 1000;
	static const long double div = std::log(base);
	static const long double scaling[] = { 1, std::pow(base, -1), std::pow(base, -2), std::pow(base, -3), std::pow(base, -4) };
	static const char* const units[] = { "s", "ms", R"(µs)", "ns", "ps" };
	static const char* const colors[] = { BROWN, DARK_ORANGE, YELLOW_GREEN, MEDIUM_SEA_GREEN, MEDIUM_SEA_GREEN, CLEAR_COLOR };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, prefix, units, scaling, colors, 1000.0L);
}

std::string string::from_small_time(long double seconds, const char* prefix, bool colored) {
	return _from_small_time(seconds, prefix, colored);
}


static inline std::string _from_time(long double seconds, const char* prefix, bool colored) {
	static const long double base = 60;
	static const long double div = std::log(base);
	static const long double scaling[] = { std::pow(base, 2), std::pow(base, 1), 1 };
	static const char* const units[] = { "hrs", "min", "s" };
	static const char* const colors[] = { DARK_ORANGE, YELLOW_GREEN, MEDIUM_SEA_GREEN, CLEAR_COLOR };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, prefix, units, scaling, colors, 100.0L);
}

std::string string::from_time(long double seconds, const char* prefix, bool colored) {
	return _from_time(seconds, prefix, colored);
}


static inline std::string _from_delta(long double nanoseconds, const char* prefix, bool colored) {
	long double seconds = nanoseconds / 1e9;  // convert nanoseconds to seconds (as a double)
	return (seconds < 1) ? _from_small_time(seconds, prefix, colored) : _from_time(seconds, prefix, colored);
}

std::string string::from_delta(long double nanoseconds, const char* prefix, bool colored) {
	return _from_delta(nanoseconds, prefix, colored);
}


std::string string::from_delta(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, const char* prefix, bool colored) {
	return _from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), prefix, colored);
}
