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

#include <set>


/*
 * Metric based in Sørensen–Dice index.
 *
 * Token-based metric.
 */
class Sorensen_Dice : public StringMetric<Sorensen_Dice> {
	std::set<std::string> _str_bigrams;

	friend class StringMetric<Sorensen_Dice>;

	std::set<std::string> get_bigrams(const std::string& str) const {
		// Extract bigrams from str
		const size_t num_pairs = str.length() - 1;
		std::set<std::string> str_bigrams;
		for (size_t i = 0; i < num_pairs; ++i) {
			str_bigrams.insert(str.substr(i, 2));
		}
		return str_bigrams;
	}

	double _similarity(const std::string& str1, const std::string& str2) const {
		// Base case: if some string does not have bigrams.
		if (str1.length() < 2 || str2.length() < 2) {
			return 0;
		}

		// Extract bigrams from str1
		const auto str1_bigrams = get_bigrams(str1);

		// Extract bigrams from str2
		const auto str2_bigrams = get_bigrams(str2);

		// Find the intersection between the two sets.
		Counter c;
		std::set_intersection(str1_bigrams.begin(), str1_bigrams.end(), str2_bigrams.begin(),
			str2_bigrams.end(), std::back_inserter(c));

		// Returns similarity.
		return (2.0 * c.count) / (str1_bigrams.size() + str2_bigrams.size());
	}

	double _similarity(const std::string& str2) const {
		// Base case: if _str_bigrams or str2 does not have bigrams.
		if (_str_bigrams.empty() || str2.length() < 2) {
			return 0.0;
		}

		// Extract bigrams from str2
		const auto str2_bigrams = get_bigrams(str2);

		// Find the intersection between the two sets.
		Counter c;
		std::set_intersection(_str_bigrams.begin(), _str_bigrams.end(), str2_bigrams.begin(),
			str2_bigrams.end(), std::back_inserter(c));

		// Returns similarity.
		return (2.0 * c.count) / (_str_bigrams.size() + str2_bigrams.size());
	}

	/*
	 * It is not a proper distance metric as it does not possess
	 * the property of triangle inequality.
	 */

	double _distance(const std::string& str1, const std::string& str2) const {
		return 1.0 - _similarity(str1, str2);
	}

	double _distance(const std::string& str2) const {
		return 1.0 - _similarity(str2);
	}

	std::string _description() const noexcept {
		return "Sorensen Dice";
	}

public:
	Sorensen_Dice(bool icase=true)
		: StringMetric<Sorensen_Dice>(icase) { }

	template <typename T>
	Sorensen_Dice(T&& str, bool icase=true)
		: StringMetric<Sorensen_Dice>(std::forward<T>(str), icase),
		  _str_bigrams(get_bigrams(_str)) { }
};
