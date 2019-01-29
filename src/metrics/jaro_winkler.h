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

#include "jaro.h"


/*
 * The Jaro–Winkler distance.
 *
 * It is a variant of the Jaro distance metric.
 */
class Jaro_Winkler : public StringMetric<Jaro_Winkler> {
	Jaro _jaro;
	double _p;  // Scaling factor.
	double _bt; // Boost threshold.

	const size_t MAX_PREFIX_LEN{4};
	const double MAX_P{0.25};
	const double MAX_BT{1.0};

	friend StringMetric<Jaro_Winkler>;

	size_t len_common_prefix(const std::string& str1, const std::string& str2) const {
		if (str1.length() > str2.length()) {
			return std::min(MAX_PREFIX_LEN, (size_t)std::distance(str2.begin(), std::mismatch(str2.begin(), str2.end(), str1.begin()).first));
		} else {
			return std::min(MAX_PREFIX_LEN, (size_t)std::distance(str1.begin(), std::mismatch(str1.begin(), str1.end(), str2.begin()).first));
		}
	}

	double _similarity(const std::string& str1, const std::string& str2) const {
		const double jd = _jaro._similarity(str1, str2);

		if (jd < _bt) {
			return jd;
		}

		return jd + (len_common_prefix(str1, str2) * _p * (1.0 - jd));
	}

	double _similarity(const std::string& str2) const {
		return _similarity(_str, str2);
	}

	double _distance(const std::string& str1, const std::string& str2) const {
		return 1.0 - _similarity(str1, str2);
	}

	double _distance(const std::string& str2) const {
		return 1.0 - _similarity(_str, str2);
	}

	std::string _description() const noexcept {
		return "Jaro Winkler";
	}

public:
	Jaro_Winkler(bool icase=true, double p=0.1, double bt=0.7)
		: StringMetric<Jaro_Winkler>(icase),
		  _p(p),
		  _bt(bt) { }

	template <typename T>
	Jaro_Winkler(T&& str, bool icase=true, double p=0.1, double bt=0.7)
		: StringMetric<Jaro_Winkler>(std::forward<T>(str), icase),
		  _p(p),
		  _bt(bt)
	{
		if (_p < 0.0 || _p > MAX_P) {
			throw std::invalid_argument("_p should be positive and not exceed " + std::to_string(MAX_P));
		}
		if (_bt < 0.0 || _bt > MAX_BT) {
			throw std::invalid_argument("_bt should be positive and not exceed " + std::to_string(MAX_BT));
		}
	}
};
