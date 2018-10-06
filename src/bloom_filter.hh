/*
 * Copyright (C) 2018 Dubalu LLC. All rights reserved.
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

#include <bitset>         // for std::bitset
#include <utility>        // for std::make_pair

#include "hashes.hh"


template <size_t N>
class BloomFilter {
	// [http://blog.michaelschmatz.com/2016/04/11/how-to-write-a-bloom-filter-cpp/]
	// [http://citeseer.ist.psu.edu/viewdoc/download;jsessionid=4060353E67A356EF9528D2C57C064F5A?doi=10.1.1.152.579&rep=rep1&type=pdf]
	// P = 0.00001
	// k = -ln(P) / ln(2)
	// k = 11.512925465 / 0.6931471806
	// k = 16.6096404735
	// k = 17
	// m = N * k / ln(2)
	// m = N * 16.6096404735 / 0.6931471806
	// m = N * 23.9626459407
	// m = N * 24
	static constexpr size_t k = 17;
	static constexpr size_t m = N * 24;
	std::bitset<m> bits;

	auto hash(const char* data, size_t len) const {
		return std::make_pair(
			xxh64::hash(data, len),
			fnv1ah64::hash(data, len)
		);
	}

public:
	void add(const char* data, size_t len) {
		auto hashes = hash(data, len);
		for (auto n = k; n; --n) {
			bits[(hashes.first + n * hashes.second) % m] = true;
		}
	}

	bool contains(const char* data, size_t len) const {
		auto hashes = hash(data, len);
		for (auto n = k; n; --n) {
			if (!bits[(hashes.first + n * hashes.second) % m]) {
				return false;
			}
		}
		return true;
	}
};
