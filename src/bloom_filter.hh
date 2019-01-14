/*
 * Copyright (C) 2018,2019 Dubalu LLC. All rights reserved.
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

#include "cassert.h"      // for ASSERT
#include "hashes.hh"      // for xxh64::hash, fnv1ah64::hash


template <size_t N=131072>
class BloomFilter {
	// [http://blog.michaelschmatz.com/2016/04/11/how-to-write-a-bloom-filter-cpp/]
	// [http://citeseer.ist.psu.edu/viewdoc/download;jsessionid=4060353E67A356EF9528D2C57C064F5A?doi=10.1.1.152.579&rep=rep1&type=pdf]
	// P = 0.000001
	// k = -ln(P) / ln(2)
	// k = 13.815510558 / 0.6931471806
	// k = 19.9315685693
	// k = 20
	// m = N * k / ln(2)
	// m = N * 19.9315685693 / 0.6931471806
	// m = N * 28.7551751321
	// m = N * 32
	// if N = 8192
	//    m = 8192 * 32
	//    m = 262,144 bits => 32,768 bytes => 32K
	// if N = 131072
	//    m = 131,072 * 32
	//    m = 4,194,304 bits => 524,288 bytes => 512K
	// if N = 262144
	//    m = 262,144 * 32
	//    m = 8,388,608 bits => 1,048,576 bytes => 1M
	static constexpr size_t k = 20;
	static constexpr size_t m = N * 32;
	std::bitset<m> bits;

	auto hash(const char* data, size_t len, uint64_t salt) const {
		ASSERT(salt);
		return std::make_pair(
			xxh64::hash(data, len),
			fnv1ah64::hash(data, len) * salt
		);
	}

public:
	void add(const char* data, size_t len, uint64_t salt = 1) {
		auto hashes = hash(data, len, salt);
		for (auto n = k; n; --n) {
			bits[(hashes.first + n * hashes.second) % m] = true;
		}
	}

	bool contains(const char* data, size_t len, uint64_t salt = 1) const {
		auto hashes = hash(data, len, salt);
		for (auto n = k; n; --n) {
			if (!bits[(hashes.first + n * hashes.second) % m]) {
				return false;
			}
		}
		return true;
	}
};
