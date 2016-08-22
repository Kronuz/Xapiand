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
#include "../datetime.h"
#include "../wkt_parser.h"

#include <unordered_set>
#include <vector>
#include <xapian.h>


constexpr size_t MAX_TERMS            = 50;
constexpr size_t MAX_SERIALISE_LENGTH = 18;
constexpr size_t START_POS            = SIZE_BITS_ID - 4;


// Returns the upper bound's length given the prefix's length, number of unions and union's length.
inline constexpr size_t get_upper_bound(size_t length_prefix, size_t number_unions, size_t length_union) noexcept {
	auto max_length_term = length_prefix + MAX_SERIALISE_LENGTH;
	return (max_length_term + length_union) * number_unions + max_length_term;
}


namespace GenerateTerms {
	inline uint64_t modulus(uint64_t val, uint64_t mod) {
		return val % mod;
	}

	inline uint64_t modulus(int64_t val, uint64_t mod) {
		if (val < 0) {
			val = -val;
			auto m = static_cast<uint64_t>(val) % mod;
			return m ? mod - m : m;
		}
		return static_cast<uint64_t>(val) % mod;
	}

	/*
	 * Add generated terms by accuracy for field or global values in doc.
	 */

	void integer(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, int64_t value);
	void positive(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, uint64_t value);
	void date(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const Datetime::tm_t& tm);
	void geo(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const RangeList& ranges);


	/*
	 * Add generated terms by accuracy for field and global values in doc.
	 */

	void integer(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, int64_t value);
	void positive(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, uint64_t value);
	void date(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, const Datetime::tm_t& tm);
	void geo(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, const RangeList& ranges);


	/*
	 * Generate terms for numerical ranges.
	 */
	template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
	std::pair<std::string, std::vector<std::string>> numeric(T start, T end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix) {
		if (accuracy.empty() || end < start) {
			return std::make_pair(std::string(), std::vector<std::string>());
		}

		std::string result_terms;
		std::vector<std::string> used_prefixes;
		used_prefixes.reserve(2);

		uint64_t size_r = end - start;

		// Find the upper or equal accuracy.
		size_t pos = 0, len = accuracy.size();
		while (pos < len && accuracy[pos] < size_r) {
			++pos;
		}

		// If there is a upper or equal accuracy.
		if (pos < len) {
			auto up_acc = accuracy[pos];
			T up_start = start - modulus(start, up_acc);
			T up_end = end - modulus(end, up_acc);
			std::string prefix_dot_up;
			prefix_dot_up.reserve(acc_prefix[pos].length() + 1);
			prefix_dot_up.assign(acc_prefix[pos]).push_back(':');

			used_prefixes.push_back(acc_prefix[pos]);

			if (pos > 0) {
				--pos;
				auto low_acc = accuracy[pos];
				T low_start = start - modulus(start, low_acc);
				T low_end = end - modulus(end, low_acc);
				std::string prefix_dot_low;
				prefix_dot_low.reserve(acc_prefix[pos].length() + 1);
				prefix_dot_low.assign(acc_prefix[pos]).push_back(':');

				used_prefixes.push_back(acc_prefix[pos]);

				if (up_start == up_end) {
					size_t num_unions = (low_end - low_start) / low_acc;
					if (num_unions == 0) {
						result_terms.reserve(get_upper_bound(prefix_dot_low.length(), num_unions, 4));
						result_terms.append(prefix_dot_low).append(to_query_string(low_start));
					} else {
						result_terms.reserve(get_upper_bound(prefix_dot_low.length(), num_unions + 1, 4) + 2);
						result_terms.append(prefix_dot_up).append(to_query_string(up_start)).append(" AND (");
						while (low_start != low_end) {
							result_terms.append(prefix_dot_low).append(to_query_string(low_start)).append(" OR ");
							low_start += low_acc;
						}
						result_terms.append(prefix_dot_low).append(to_query_string(low_end));
						result_terms.push_back(')');
					}
				} else {
					size_t num_unions1 = (up_end - low_start) / low_acc;
					if (num_unions1 == 0) {
						result_terms.reserve(get_upper_bound(prefix_dot_low.length(), num_unions1, 4));
						result_terms.append(prefix_dot_low).append(to_query_string(low_start));
					} else {
						result_terms.reserve(get_upper_bound(prefix_dot_low.length(), num_unions1 + 1, 4) + 2);
						result_terms.append(prefix_dot_up).append(to_query_string(up_start)).append(" AND (");
						while (low_start < up_end) {
							result_terms.append(prefix_dot_low).append(to_query_string(low_start));
							low_start += low_acc;
							if (low_start < up_end) {
								result_terms.append(" OR ");
							}
						}
						result_terms.push_back(')');
					}
					size_t num_unions2 = (low_end - low_start) / low_acc;
					if (num_unions2 == 0) {
						result_terms.reserve(result_terms.length() + get_upper_bound(prefix_dot_low.length(), num_unions2, 4) + 6);
						result_terms.insert(result_terms.begin(), '(');
						result_terms.append(") OR ").append(prefix_dot_low).append(to_query_string(low_end));
					} else {
						result_terms.reserve(result_terms.length() + get_upper_bound(prefix_dot_low.length(), num_unions2 + 1, 4) + 10);
						result_terms.insert(result_terms.begin(), '(');
						result_terms.append(") OR (");
						result_terms.append(prefix_dot_up).append(to_query_string(up_end)).append(" AND (");
						while (low_start != low_end) {
							result_terms.append(prefix_dot_low).append(to_query_string(low_start)).append(" OR ");
							low_start += low_acc;
						}
						result_terms.append(prefix_dot_low).append(to_query_string(low_end));
						result_terms.append("))");
					}
				}
			} else {
				// Reserve upper bound.
				result_terms.reserve(get_upper_bound(prefix_dot_up.length(), up_start != up_end, 4));
				if (up_start != up_end) {
					result_terms.append(prefix_dot_up).append(to_query_string(up_start)).append(" OR ");
				}
				result_terms.append(prefix_dot_up).append(to_query_string(up_end));
			}
		} else {
			// If only there is a lower accuracy.
			if (pos > 0) {
				--pos;
				auto low_acc = accuracy[pos];
				start -= modulus(start, low_acc);
				end -= modulus(end, low_acc);
				size_t num_unions = (end - start) / low_acc;
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
						start += low_acc;
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
		}

		return std::make_pair(std::move(result_terms), std::move(used_prefixes));
	}

	/*
	 * Generate Terms for date ranges.
	 */
	std::pair<std::string, std::vector<std::string>> date(double start_, double end_, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix);

	/*
	 * Auxiliar functions for date ranges.
	 */

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

	/*
	 * Generate ters for geospatial ranges.
	 */
	std::pair<std::string, std::unordered_set<std::string>> geo(const std::vector<range_t>& ranges, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix);
};
