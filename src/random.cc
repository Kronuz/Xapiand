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

#include "random.hh"

#include <algorithm>             // for uniform_int_distribution
#include <random>                // for std::random_device, std::mt19937_64


auto& rng() {
	// Initialize Mersennes' twister using Random device engine (usually based
	// on /dev/random on UNIX-like systems) to generate the seed
	static thread_local std::mt19937_64 rng(std::random_device{}());
	return rng;
}


double
random_real(double initial, double last)
{
	std::uniform_real_distribution<double> distribution(initial, last);
	return distribution(rng());
}


std::uint64_t
random_int(std::uint64_t initial, std::uint64_t last)
{
	std::uniform_int_distribution<std::uint64_t> distribution(initial, last);
	return distribution(rng());
}


std::chrono::milliseconds
random_time(std::chrono::milliseconds initial, std::chrono::milliseconds last)
{
	std::uniform_int_distribution<std::chrono::milliseconds::rep> distribution(initial.count(), last.count());
	return std::chrono::milliseconds(distribution(rng()));
}


std::string
random_string(size_t length)
{
    static auto& chrs = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static thread_local std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

    std::string s;
    s.reserve(length);

	auto& r = rng();
    while (length--) {
        s.push_back(chrs[pick(r)]);
    }

    return s;
}
