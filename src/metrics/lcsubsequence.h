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

#include "basic_string_metric.h"


/*
 * Longest Common Subsequence.
 *
 * Character-based metric.
 */
class LCSubsequence : public StringMetric<LCSubsequence> {

	friend class StringMetric<LCSubsequence>;

	size_t lcs(std::string_view str1, std::string_view str2) const {
		const auto m = str1.size(), n = str2.size();

		std::vector<size_t> v1(n + 1, 0);
		std::vector<size_t> v2(n + 1);

		for (size_t i = 1; i <= m; ++i) {
			for (size_t j = 1; j <= n; ++j) {
				if (str1[i - 1] == str2[j - 1]) {
					v2[j] = v1[j - 1] + 1;
				} else {
					v2[j] = std::max(v2[j - 1], v1[j]);
				}
			}
			v2.swap(v1);
		}

		return v1[n];
	}

	double _distance(std::string_view str1, std::string_view str2) const {
		return 1.0 - _similarity(str1, str2);
	}

	double _distance(std::string_view str2) const {
		return _distance(_str, str2);
	}

	double _similarity(std::string_view str1, std::string_view str2) const {
		return (double)lcs(str1, str2) / std::max(str1.size(), str2.size());
	}

	double _similarity(std::string_view str2) const {
		return _similarity(_str, str2);
	}

	std::string_view _name() const noexcept {
		return "LCSubsequence";
	}

	std::string _description() const noexcept {
		return "Longest Common Subsequence";
	}

public:
	LCSubsequence(bool icase=true)
		: StringMetric<LCSubsequence>(icase) { }

	template <typename T>
	LCSubsequence(T&& str, bool icase=true)
		: StringMetric<LCSubsequence>(std::forward<T>(str), icase) { }
};
