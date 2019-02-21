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

#include <type_traits>       // for std::enable_if_t, std::decay_t, std::is_integral
#include <stdexcept>         // for std::invalid_argument


template<typename T, typename M, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value && std::is_integral<std::decay_t<M>>::value>>
inline M modulus(T val, M mod) {
	if (mod < 0) {
		throw std::invalid_argument("Modulus must be positive");
	}
	if (val < 0) {
		val = -val;
		auto m = static_cast<M>(val) % mod;
		return m ? mod - m : m;
	}
	return static_cast<M>(val) % mod;
}


template<typename T>
inline T
add(T x, T y, bool& overflow)
{
	if (x + y < x) {
		overflow = true;
		return std::numeric_limits<T>::max();
	}
	return x + y;
}

template<typename T>
inline T
add(T x, T y)
{
	bool overflow;
	return add(x, y, overflow);
}


template<typename T>
inline T
sub(T x, T y, bool& overflow)
{
	if (x - y > x) {
		overflow = true;
		return std::numeric_limits<T>::min();
	}
	return x - y;
}

template<typename T>
inline T
sub(T x, T y)
{
	bool overflow;
	return sub(x, y, overflow);
}


template<typename T>
T min(const std::vector<uint64_t>& accuracy);

template<typename T>
T max(const std::vector<uint64_t>& accuracy);

template<>
inline double
min<double>(const std::vector<uint64_t>& accuracy)
{
	double min = std::numeric_limits<double>::min();

	if (accuracy.empty()) {
		return min;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(-min)) {
		return min;
	}

	return -static_cast<double>(back);
}

template<>
inline double
max<double>(const std::vector<uint64_t>& accuracy)
{
	double max = std::numeric_limits<double>::max();

	if (accuracy.empty()) {
		return max;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(max)) {
		return max;
	}

	return static_cast<double>(back);
}

template<>
inline long
min<long>(const std::vector<uint64_t>& accuracy)
{
	long min = std::numeric_limits<long>::min();

	if (accuracy.empty()) {
		return min;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(-min)) {
		return min;
	}

	return -static_cast<long>(back);
}

template<>
inline long
max<long>(const std::vector<uint64_t>& accuracy)
{
	long max = std::numeric_limits<long>::max();

	if (accuracy.empty()) {
		return max;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(max)) {
		return max;
	}

	return static_cast<long>(back);
}

template<>
inline long long
min<long long>(const std::vector<uint64_t>& accuracy)
{
	long long min = std::numeric_limits<long long>::min();

	if (accuracy.empty()) {
		return min;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(-min)) {
		return min;
	}

	return -static_cast<long long>(back);
}

template<>
inline long long
max<long long>(const std::vector<uint64_t>& accuracy)
{
	long long max = std::numeric_limits<long long>::max();

	if (accuracy.empty()) {
		return max;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	if (back > static_cast<uint64_t>(max)) {
		return max;
	}

	return static_cast<long long>(back);
}

template<>
inline unsigned long
min<unsigned long>(const std::vector<uint64_t>&)
{
	unsigned long min = std::numeric_limits<unsigned long>::min();

	return min;
}

template<>
inline unsigned long
max<unsigned long>(const std::vector<uint64_t>& accuracy)
{
	unsigned long max = std::numeric_limits<unsigned long>::max();

	if (accuracy.empty()) {
		return max;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	return static_cast<unsigned long>(back);
}

template<>
inline unsigned long long
min<unsigned long long>(const std::vector<uint64_t>&)
{
	unsigned long long min = std::numeric_limits<unsigned long long>::min();

	return min;
}

template<>
inline unsigned long long
max<unsigned long long>(const std::vector<uint64_t>& accuracy)
{
	unsigned long long max = std::numeric_limits<unsigned long long>::max();

	if (accuracy.empty()) {
		return max;
	}

	uint64_t back = accuracy.back();
	back = add(back, back);

	return static_cast<unsigned long long>(back);
}
