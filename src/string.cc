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

#include "string.hh"

#include <cmath>              // for std::log, std::floorl, std::pow


static inline std::string humanize(long double delta, bool colored, const int i, const int n, const long double div, const char* const units[], const long double scaling[], const char* const colors[], long double rounding) {
	long double num = delta;

	if (delta < 0) delta = -delta;
	int order = (delta == 0) ? n : -std::floor(std::log(delta) / div);
	order += i;
	if (order < 0) order = 0;
	else if (order > n) order = n;

	const char* color = colored ? colors[order] : "";
	num = roundl(rounding * num / scaling[order]) / rounding;
	const char* unit = units[order];
	const char* reset = colored ? colors[n + 1] : "";

	return format_string("%s%Lg%s%s", color, num, unit, reset);
}


static inline int find_val(long double val, const long double* input, int i=0) {
	return (*input == val) ? i : find_val(val, input + 1, i + 1);
}


inline std::string _bytes_string(size_t bytes, bool colored) {
	static const long double base = 1024;
	static const long double div = std::log(base);
	static const long double scaling[] = { std::pow(base, 8), std::pow(base, 7), std::pow(base, 6), std::pow(base, 5), std::pow(base, 4), std::pow(base, 3), std::pow(base, 2), std::pow(base, 1), 1 };
	static const char* const units[] = { "YiB", "ZiB", "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
	static const char* const colors[] = { "\033[1;31m", "\033[1;31m", "\033[1;31m", "\033[1;31m", "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0;32m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(bytes, colored, i, n, div, units, scaling, colors, 10.0L);
}

std::string bytes_string(size_t bytes, bool colored) {
	return _bytes_string(bytes, colored);
}


inline std::string _small_time_string(long double seconds, bool colored) {
	static const long double base = 1000;
	static const long double div = std::log(base);
	static const long double scaling[] = { 1, std::pow(base, -1), std::pow(base, -2), std::pow(base, -3), std::pow(base, -4) };
	static const char* const units[] = { "s", "ms", "\xc2\xb5s", "ns", "ps" };
	static const char* const colors[] = { "\033[1;31m", "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, units, scaling, colors, 1000.0L);
}

std::string small_time_string(long double seconds, bool colored) {
	return _small_time_string(seconds, colored);
}


inline std::string _time_string(long double seconds, bool colored) {
	static const long double base = 60;
	static const long double div = std::log(base);
	static const long double scaling[] = { std::pow(base, 2), std::pow(base, 1), 1 };
	static const char* const units[] = { "hrs", "min", "s" };
	static const char* const colors[] = { "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, units, scaling, colors, 100.0L);
}

std::string time_string(long double seconds, bool colored) {
	return _time_string(seconds, colored);
}


inline std::string _delta_string(long double nanoseconds, bool colored) {
	long double seconds = nanoseconds / 1e9;  // convert nanoseconds to seconds (as a double)
	return (seconds < 1) ? _small_time_string(seconds, colored) : _time_string(seconds, colored);
}

std::string delta_string(long double nanoseconds, bool colored) {
	return _delta_string(nanoseconds, colored);
}


std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored) {
	return _delta_string(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), colored);
}
