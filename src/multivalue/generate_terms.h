/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "analysis.hh"                            // for analysis::stop_strategy, analysis::stem_strategy
#include "database/utils.h"                       // for prefixed
#include "engine/document.h"                       // for engine::Document
#include "htm.h"                       // for HTM_BITS_ID, range_t (ptr only)
#include "serialise.h"                            // for serialise
#include "engine/query.h"                         // for engine::Query, Op


constexpr size_t MAX_TERMS_LEVEL        = 256;
constexpr size_t MAX_TERMS              = 128;
constexpr size_t MAX_SERIALISE_LENGTH   = 18;


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

	void integer(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, int64_t value);
	void positive(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, uint64_t value);
	void datetime(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const Datetime::tm_t& tm);
	void geo(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const std::vector<range_t>& ranges);


	/*
	 * Add generated terms by accuracy for field and global values in doc.
	 */

	void integer(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, int64_t value);
	void positive(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, uint64_t value);
	void datetime(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, const Datetime::tm_t& tm);
	void geo(engine::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
		const std::vector<std::string>& acc_global_prefix, const std::vector<range_t>& ranges);

	void text(engine::Document& doc,
		const std::string& prefix, const std::string& value,
		bool positions, engine::termcount weight,
		bool cjk_ngram, bool cjk_words,
		const std::string& language, const std::string& stem_language,
		analysis::stop_strategy stop_strategy,
		analysis::stem_strategy stem_strategy);

	/*
	 * Generate terms for numerical ranges: at least one matches every value in [start, end], and some
	 * may match values just outside it, so pair them with an exact value check. Empty when the range
	 * needs more than `max_terms` buckets of the coarsest accuracy.
	 */
	engine::Query numeric(int64_t start, int64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, engine::termcount wqf=1, size_t max_terms=MAX_TERMS);

	engine::Query numeric(uint64_t start, uint64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, engine::termcount wqf=1, size_t max_terms=MAX_TERMS);

	/*
	 * Generate terms for datetime ranges.
	 */
	engine::Query datetime(double start_, double end_, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, engine::termcount wqf=1);

	/*
	 * Auxiliar functions for datetime ranges.
	 */

	engine::Query millennium(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query century(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query decade(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query month(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query day(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query hour(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query minute(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);
	engine::Query second(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, engine::termcount wqf=1);

	// Datetime only accepts year greater than 0.
	inline constexpr int year(int year, int accuracy) noexcept {
		year -= (year % accuracy);
		return (year > 0) ? year : accuracy;
	}

	/*
	 * Generate ters for geospatial ranges.
	 */
	engine::Query geo(const std::vector<range_t>& ranges, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, engine::termcount wqf=1);
};
