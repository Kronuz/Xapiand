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

#include "basic_string_metric.h"


/*
 * Longest Common Substring.
 *
 * Character-based metric.
 */
class LCSubstr : public StringMetric<LCSubstr> {

	friend class StringMetric<LCSubstr>;

	size_t lcs(const std::string& str1, const std::string& str2) const {
		const auto m = str1.length(), n = str2.length();

		std::vector<size_t> v1(n + 1);
		std::vector<size_t> v2(n + 1);

		size_t res = 0;
		for (size_t i = 0; i < m; ++i) {
			for (size_t j = 0; j < n; ++j) {
				if (str1[i] == str2[j]) {
					if (i == 0 || j == 0) {
						v2[j] = 1;
					} else {
						v2[j] = v1[j - 1] + 1;
					}
					if (v2[j] > res) {
						res = v2[j];
					}
				} else {
					v2[j] = 0;
				}
			}
			v2.swap(v1);
		}

		return res;
	}

	double _distance(const std::string& str1, const std::string& str2) const {
		return 1.0 - _similarity(str1, str2);
	}

	double _distance(const std::string& str2) const {
		return _distance(_str, str2);
	}

	double _similarity(const std::string& str1, const std::string& str2) const {
		return (double)lcs(str1, str2) / std::max(str1.length(), str2.length());
	}

	double _similarity(const std::string& str2) const {
		return _similarity(_str, str2);
	}

	std::string _description() const noexcept {
		return "Longest Common Substring";
	}

public:
	LCSubstr(bool icase=true)
		: StringMetric<LCSubstr>(icase) { }

	template <typename T>
	LCSubstr(T&& str, bool icase=true)
		: StringMetric<LCSubstr>(std::forward<T>(str), icase) { }
};
