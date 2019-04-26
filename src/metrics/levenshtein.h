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
 * Levenshtein distance or edit distance.
 *
 * Character-based metric.
 */
class Levenshtein : public StringMetric<Levenshtein> {
	size_t _subst_cost;
	size_t _ins_del_cost;
	size_t _maxCost;

	friend class StringMetric<Levenshtein>;

	double _distance(std::string_view str1, std::string_view str2) const {
		const auto len1 = str1.size(), len2 = str2.size();
		std::vector<size_t> col(len2 + 1), prev_col(len2 + 1);

		for (size_t i = 0; i <= len2; ++i) {
			prev_col[i] = i * _ins_del_cost;
		}

		for (size_t i = 0; i < len1; ++i) {
			col[0] = i + 1;
			for (size_t j = 0; j < len2; ++j) {
				col[j + 1] = std::min({ prev_col[j + 1] + _ins_del_cost, col[j] + _ins_del_cost, prev_col[j] + (str1[i] == str2[j] ? 0 : _subst_cost) });
			}
			col.swap(prev_col);
		}

		return (double)prev_col[len2] / (_maxCost * std::max(len1, len2));
	}

	double _distance(std::string_view str2) const {
		return _distance(_str, str2);
	}

	double _similarity(std::string_view str1, std::string_view str2) const {
		return 1.0 - _distance(str1, str2);
	}

	double _similarity(std::string_view str2) const {
		return 1.0 - _distance(_str, str2);
	}

	std::string_view _name() const noexcept {
		return "Levenshtein";
	}

	std::string _description() const noexcept {
		return "Levenshtein";
	}

public:
	Levenshtein(bool icase=true, size_t subst_cost=1.0, size_t ins_del_cost=1.0)
		: StringMetric<Levenshtein>(icase),
		  _subst_cost(subst_cost),
		  _ins_del_cost(ins_del_cost),
		  _maxCost(std::max(_subst_cost, _ins_del_cost)) { }

	template <typename T>
	Levenshtein(T&& str, bool icase=true, size_t subst_cost=1.0, size_t ins_del_cost=1.0)
		: StringMetric<Levenshtein>(std::forward<T>(str), icase),
		  _subst_cost(subst_cost),
		  _ins_del_cost(ins_del_cost),
		  _maxCost(std::max(_subst_cost, _ins_del_cost)) { }

	std::string serialise() const override {
		std::string serialised;
		serialised += StringMetric<Levenshtein>::serialise();
		serialised += serialise_length(_subst_cost);
		serialised += serialise_length(_ins_del_cost);
		serialised += serialise_length(_maxCost);

		return serialised;
	}

	void unserialise(const char** p, const char* p_end) override {
		StringMetric<Levenshtein>::unserialise(p, p_end);
		_subst_cost = unserialise_length(p, p_end);
		_ins_del_cost = unserialise_length(p, p_end);
		_maxCost = unserialise_length(p, p_end);
	}
};
