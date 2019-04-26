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

#include <set>


/*
 * Metric based in Jaccard index, also known as the
 * Jaccard similarity coefficient index.
 *
 * Token-based metric.
 */
class Jaccard : public StringMetric<Jaccard> {
	std::set<char> _set_str;

	friend class StringMetric<Jaccard>;

	double _similarity(std::string_view str1, std::string_view str2) const {
		const std::set<char> set_str1(str1.begin(), str1.end());
		const std::set<char> set_str2(str2.begin(), str2.end());

		// Find the count intersection between the two usets.
		Counter c;
		std::set_intersection(set_str1.begin(), set_str1.end(), set_str2.begin(),
			set_str2.end(), std::back_inserter(c));

		// Returns Jaccard distance.
		return c.count / (double)(set_str1.size() + set_str2.size() - c.count);
	}

	double _similarity(std::string_view str2) const {
		const std::set<char> set_str2(str2.begin(), str2.end());

		// Find the count intersection between the two usets.
		Counter c;
		std::set_intersection(_set_str.begin(), _set_str.end(), set_str2.begin(),
			set_str2.end(), std::back_inserter(c));

		// Returns Jaccard distance.
		return c.count / (double)(_set_str.size() + set_str2.size() - c.count);
	}

	double _distance(std::string_view str1, std::string_view str2) const {
		return 1.0 - _similarity(str1, str2);
	}

	double _distance(std::string_view str2) const {
		return 1.0 - _similarity(str2);
	}

	std::string_view _name() const noexcept {
		return "Jaccard";
	}

	std::string _description() const noexcept {
		return "Jaccard";
	}

public:
	Jaccard(bool icase=true)
		: StringMetric<Jaccard>(icase) { }

	template <typename T>
	Jaccard(T&& str, bool icase=true)
		: StringMetric<Jaccard>(std::forward<T>(str), icase),
		  _set_str(_str.begin(), _str.end()) { }

	std::string serialise() const override {
		std::string serialised;
		serialised += StringMetric<Jaccard>::serialise();
		return serialised;
	}

	void unserialise(const char** p, const char* p_end) override {
		StringMetric<Jaccard>::unserialise(p, p_end);
		_set_str = std::set<char>(_str.begin(), _str.end());
	}
};
