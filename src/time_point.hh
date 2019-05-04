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

#include <chrono>            // for std::chrono
#include <cmath>             // for std::pow, std::floor, std::log10
#include <errno.h>           // for errno
#include <time.h>            // for nanosleep


inline void nanosleep(unsigned long long nsec) {
	if (nsec > 0) {
		struct timespec ts;
		ts.tv_sec = static_cast<long>(nsec / 1000000000);
		ts.tv_nsec = static_cast<long>(nsec % 1000000000);
		while (nanosleep(&ts, &ts) < 0 && errno == EINTR) { }
	}
}


struct Clk {
unsigned long long mul;

Clk() {
	auto a = std::chrono::steady_clock::now();
	nanosleep(5000000);  // sleep for 5 milliseconds
	auto b = std::chrono::steady_clock::now();
	auto delta = *reinterpret_cast<unsigned long long*>(&b) - *reinterpret_cast<unsigned long long*>(&a);
	mul = 1000000 / static_cast<unsigned long long>(std::pow(10, std::floor(std::log10(delta))));
}

template <typename T>
unsigned long long time_point_to_ullong(std::chrono::time_point<T> t) const {
	return *reinterpret_cast<unsigned long long*>(&t) * mul;
}

template <typename T = std::chrono::steady_clock>
std::chrono::time_point<T> time_point_from_ullong(unsigned long long t) const {
	t /= mul;
	return *reinterpret_cast<std::chrono::time_point<T>*>(&t);
}

static const Clk& clk() {
	static const Clk clk;
	return clk;
}
};


template <typename T>
inline unsigned long long
time_point_to_ullong(std::chrono::time_point<T> t) {
	return Clk::clk().time_point_to_ullong<T>(t);
}


template <typename T = std::chrono::steady_clock>
inline std::chrono::time_point<T>
time_point_from_ullong(unsigned long long t) {
	return Clk::clk().time_point_from_ullong<T>(t);
}
