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

#include <cstdint>                                // for uint64_t
#include <stddef.h>                               // for size_t
#include <string>                                 // for string
#include <sys/types.h>                            // for uint64_t, int64_t
#include <type_traits>                            // for decay_t, enable_if_t, is_integral
#include <vector>                                 // for vector, allocator

#include "database_utils.h"                       // for prefixed
#include "geospatial/htm.h"                       // for HTM_BITS_ID, range_t (ptr only)
#include "modulus.hh"                             // for modulus
#include "serialise.h"                            // for serialise
#include "xapian.h"                               // for Query, Query::op::OP_OR, Query::op::OP_AND


constexpr size_t MAX_TERMS            = 50;
constexpr size_t MAX_SERIALISE_LENGTH = 18;


extern const char ctype_date;
extern const char ctype_geo;
extern const char ctype_integer;


// Returns the upper bound's length given the prefix's length, number of unions and union's length.
inline constexpr size_t get_upper_bound(size_t length_prefix, size_t number_unions, size_t length_union) noexcept {
	auto max_length_term = length_prefix + MAX_SERIALISE_LENGTH;
	return (max_length_term + length_union) * number_unions + max_length_term;
}


namespace GenerateTerms {
	/*
	 * Add generated terms by accuracy for field or global values in doc.
	 */

	void integer(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, int64_t value);
	void positive(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, uint64_t value);
	void date(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const Datetime::tm_t& tm);
	void geo(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const std::vector<range_t>& ranges);


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
		const std::vector<std::string>& acc_global_prefix, const std::vector<range_t>& ranges);


	/*
	 * Generate terms for numerical ranges.
	 */
	template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
	Xapian::Query numeric(T start, T end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf=1) {
		Xapian::Query query;

		if (accuracy.empty() || end < start) {
			return query;
		}

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
			std::string prefix_up = acc_prefix[pos];

			// If there is a lower accuracy.
			if (pos > 0) {
				--pos;
				auto low_acc = accuracy[pos];
				T low_start = start - modulus(start, low_acc);
				T low_end = end - modulus(end, low_acc);
				// If terms are not too many terms (num_unions + 1).
				if (static_cast<size_t>((low_end - low_start) / low_acc) < MAX_TERMS) {
					std::string prefix_low = acc_prefix[pos];

					if (up_start == up_end) {
						size_t num_unions = (low_end - low_start) / low_acc;
						if (num_unions == 0) {
							query = Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						} else {
							query = Xapian::Query(prefixed(Serialise::serialise(up_start), prefix_up, ctype_integer), wqf);
							Xapian::Query query_low(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
							while (low_start != low_end) {
								low_start += low_acc;
								query_low = Xapian::Query(Xapian::Query::OP_OR, query_low, Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf));
							}
							query = Xapian::Query(Xapian::Query::OP_AND, query, query_low);
						}

					} else {
						size_t num_unions1 = (up_end - low_start) / low_acc;
						if (num_unions1 == 0) {
							query = Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						} else {
							query = Xapian::Query(prefixed(Serialise::serialise(up_start), prefix_up, ctype_integer), wqf);
							Xapian::Query query_low(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
							while (low_start < up_end) {
								low_start += low_acc;
								if (low_start < up_end) {
									query_low = Xapian::Query(Xapian::Query::OP_OR, query_low, Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf));
								}
							}
							query = Xapian::Query(Xapian::Query::OP_AND, query, query_low);
						}

						size_t num_unions2 = (low_end - low_start) / low_acc;
						if (num_unions2 == 0) {
							Xapian::Query query_low(prefixed(Serialise::serialise(low_end), prefix_low, ctype_integer), wqf);
							query = Xapian::Query(Xapian::Query::OP_OR, query, query_low);
						} else {
							Xapian::Query query_up(prefixed(Serialise::serialise(up_end), prefix_up, ctype_integer), wqf);
							Xapian::Query query_low(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
							while (low_start != low_end) {
								low_start += low_acc;
								query_low = Xapian::Query(Xapian::Query::OP_OR, query_low, Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf));
							}
							query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(Xapian::Query::OP_AND, query_up, query_low));
						}
					}
					return query;
				}
			}

			// Only upper accuracy.
			query = Xapian::Query(prefixed(Serialise::serialise(up_end), prefix_up, ctype_integer), wqf);
			if (up_start != up_end) {
				query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(prefixed(Serialise::serialise(up_start), prefix_up, ctype_integer), wqf));
			}

		} else if (pos > 0) { // If only there is a lower accuracy.
			--pos;
			auto low_acc = accuracy[pos];
			start -= modulus(start, low_acc);
			end -= modulus(end, low_acc);
			size_t num_unions = (end - start) / low_acc;
			// If terms are not too many terms (num_unions + 1).
			if (num_unions < MAX_TERMS) {
				std::string prefix = acc_prefix[pos];
				// Reserve upper bound.
				query = Xapian::Query(prefixed(Serialise::serialise(end), prefix, ctype_integer), wqf);
				while (start != end) {
					query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(prefixed(Serialise::serialise(start), prefix, ctype_integer), wqf));
					start += low_acc;
				}
			}
		}

		return query;
	}

	/*
	 * Generate Terms for date ranges.
	 */
	Xapian::Query date(double start_, double end_, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf=1);

	/*
	 * Auxiliar functions for date ranges.
	 */

	Xapian::Query millennium(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query century(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query decade(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query month(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query day(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query hour(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query minute(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);
	Xapian::Query second(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf=1);

	// Datetime only accepts year greater than 0.
	inline constexpr int year(int year, int accuracy) noexcept {
		year -= (year % accuracy);
		return (year > 0) ? year : accuracy;
	}

	/*
	 * Generate ters for geospatial ranges.
	 */
	Xapian::Query geo(const std::vector<range_t>& ranges, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf=1);
};
