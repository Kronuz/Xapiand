/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "../database_utils.h"
#include "../wkt_parser.h"
#include "datetime.h"

#include <unordered_set>
#include <vector>


#define MAX_TERMS             50
#define MAX_SERIALISE_LENGTH  18


// Returns the upper bound's size given the prefix's length, number of unions and union's length.
inline constexpr size_t get_upper_bound(size_t length_prefix, size_t number_unions, size_t length_union) noexcept {
	auto max_length_term = length_prefix + MAX_SERIALISE_LENGTH;
	return (max_length_term + length_union) * number_unions + max_length_term;
}


namespace GenerateTerms {
	template <typename T, typename = std::enable_if_t<std::is_arithmetic<std::decay_t<T>>::value>>
	std::pair<std::string, std::vector<std::string>> numeric(T start_, T end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix) {
		if (accuracy.empty() || end_ < start_) {
			return std::make_pair(std::string(), std::vector<std::string>());
		}

		std::string result_terms;
		std::vector<std::string> used_prefixes;
		used_prefixes.reserve(2);

		auto start = static_cast<int64_t>(start_);
		auto end = static_cast<int64_t>(end_);

		uint64_t size_r = end - start;

		// Find the upper or equal accuracy.
		size_t pos = 0, len = accuracy.size();
		while (pos < len && accuracy[pos] < size_r) {
			++pos;
		}

		// If there is a upper or equal accuracy.
		if (pos < len) {
			auto _acc = static_cast<int64_t>(accuracy[pos]);
			auto aux_start = start - (start % _acc);
			auto aux_end = end - (end % _acc);
			size_t num_unions = (aux_end - aux_start) / _acc;
			std::string prefix_dot;
			prefix_dot.reserve(acc_prefix[pos].length() + 1);
			prefix_dot.assign(acc_prefix[pos]).push_back(':');

			// Reserve upper bound.
			result_terms.reserve(get_upper_bound(prefix_dot.length(), num_unions, 4));

			used_prefixes.push_back(acc_prefix[pos]);

			while (aux_start != aux_end) {
				result_terms.append(prefix_dot).append(to_query_string(aux_start)).append(" OR ");
				aux_start +=_acc;
			}
			result_terms.append(prefix_dot).append(to_query_string(aux_end));
		}

		// If there is a lower accuracy.
		if (pos > 0) {
			--pos;
			auto _acc = static_cast<int64_t>(accuracy[pos]);
			start -= start % _acc;
			end -= end % _acc;
			size_t num_unions = (end - start) / _acc;
			// If terms are not too many terms (num_unions + 1).
			if (num_unions < MAX_TERMS) {
				std::string prefix_dot, lower_terms;
				prefix_dot.reserve(acc_prefix[pos].length() + 1);
				prefix_dot.assign(acc_prefix[pos]).push_back(':');
				// Reserve upper bound.
				lower_terms.reserve(get_upper_bound(prefix_dot.length(), num_unions, 4));

				used_prefixes.push_back(acc_prefix[pos]);

				while (start != end) {
					lower_terms.append(prefix_dot).append(to_query_string(start)).append(" OR ");
					start +=_acc;
				}
				lower_terms.append(prefix_dot).append(to_query_string(end));

				if (result_terms.empty()) {
					result_terms.assign(lower_terms);
				} else {
					result_terms.reserve(result_terms.length() + lower_terms.length() + 9);
					result_terms.insert(result_terms.begin(), '(');
					result_terms.append(") AND (").append(lower_terms).push_back(')');
				}
			}
		}

		return std::make_pair(std::move(result_terms), std::move(used_prefixes));
	}

	std::pair<std::string, std::vector<std::string>> date(double start_, double end_, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix);
	std::string millennium(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string century(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string decade(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string month(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string day(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string hour(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string minute(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);
	std::string second(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix);

	// Datetime only accepts year greater than 0.
	inline constexpr int year(int year, int accuracy) noexcept {
		year -= (year % accuracy);
		return (year > 0) ? year : accuracy;
	}

	std::pair<std::string, std::unordered_set<std::string>> geo(const std::vector<range_t>& ranges, const std::vector<double>& accuracy, const std::vector<std::string>& acc_prefix);
};
