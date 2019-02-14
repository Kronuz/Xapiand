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

#include "generate_terms.h"

#include <set>                                    // for std::set
#include <unordered_set>                          // for std::unordered_set
#include <vector>                                 // for std::vector

#include "datetime.h"                             // for tm_t, timegm, to_tm_t
#include "itertools.hh"                           // for iterator::map, iterator::chain
#include "reversed.hh"                            // for reversed
#include "schema.h"                               // for required_spc_t, FieldType, UnitTime
#include "utype.hh"                               // for toUType


const char ctype_date    = required_spc_t::get_ctype(FieldType::DATE);
const char ctype_geo     = required_spc_t::get_ctype(FieldType::GEO);
const char ctype_integer = required_spc_t::get_ctype(FieldType::INTEGER);


template<typename T>
inline int
do_clz(T value) {
	int c = 0;
	while (value) {
		value >>= 1;
		++c;
	}
	return c;
}

#if HAVE_DECL___BUILTIN_CLZ
template<>
inline int
do_clz(unsigned value) {
    return __builtin_clz(value);
}
#endif

#if HAVE_DECL___BUILTIN_CLZL
template<>
inline int
do_clz(unsigned long value) {
    return __builtin_clzl(value);
}
#endif

#if HAVE_DECL___BUILTIN_CLZLL
template<>
inline int
do_clz(unsigned long long value) {
    return __builtin_clzll(value);
}
#endif


void
GenerateTerms::integer(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, int64_t value)
{
	auto it = acc_prefix.begin();
	for (const auto& acc : accuracy) {
		auto term_v = Serialise::integer(value - modulus(value, acc));
		doc.add_term(prefixed(term_v, *it, ctype_integer));
		++it;
	}
}


void
GenerateTerms::positive(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, uint64_t value)
{
	auto it = acc_prefix.begin();
	for (const auto& acc : accuracy) {
		auto term_v = Serialise::positive(value - modulus(value, acc));
		doc.add_term(prefixed(term_v, *it, ctype_integer));
		++it;
	}
}


void
GenerateTerms::date(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const Datetime::tm_t& tm)
{
	auto it = acc_prefix.begin();
	for (const auto& acc : accuracy) {
		switch (static_cast<UnitTime>(acc)) {
			case UnitTime::MILLENNIUM: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::CENTURY: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::DECADE: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::YEAR: {
				Datetime::tm_t _tm(tm.year);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::MONTH: {
				Datetime::tm_t _tm(tm.year, tm.mon);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::DAY: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::HOUR: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::MINUTE: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			case UnitTime::SECOND: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
				doc.add_term(prefixed(Serialise::timestamp(Datetime::timegm(_tm)), *it, ctype_date));
				break;
			}
			default:
				break;
		}
		++it;
	}
}


void
GenerateTerms::geo(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const std::vector<range_t>& ranges)
{
	// Index values and looking for terms generated by accuracy.
	const auto last_acc_pos = accuracy.size() - 1;

	// Convert accuracy to accuracy bits
	std::vector<uint64_t> acc_bits;
	acc_bits.resize(last_acc_pos + 1);
	for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
		acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
	}

	std::vector<std::unordered_set<uint64_t>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	auto id_trixels = HTM::getIdTrixels(ranges);

	for (const auto& id : id_trixels) {
		uint64_t last_pos = do_clz(id) - 64 + HTM_BITS_ID;
		last_pos &= ~1;  // Must be multiple of two.
		uint64_t val = id << last_pos;
		size_t pos = last_acc_pos;
		const auto it_e = acc_bits.rend();
		for (auto it = acc_bits.rbegin(); it != it_e && *it >= last_pos; ++it, --pos) {
			level_terms[pos].insert(val >> *it);
		}
	}

	// Insert terms generated by accuracy.
	for (size_t pos = 0; pos < level_terms.size(); ++pos) {
		const auto& terms = level_terms[pos];
		const auto& prefix = acc_prefix[last_acc_pos - pos];
		for (const auto& term : terms) {
			doc.add_term(prefixed(Serialise::positive(term), prefix, ctype_geo));
		}
	}
}


void
GenerateTerms::integer(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
	const std::vector<std::string>& acc_global_prefix, int64_t value)
{
	auto it = acc_prefix.begin();
	auto itg = acc_global_prefix.begin();
	for (const auto& acc : accuracy) {
		auto term_v = Serialise::integer(value - modulus(value, acc));
		doc.add_term(prefixed(term_v, *it, ctype_integer));
		doc.add_term(prefixed(term_v, *itg, ctype_integer));
		++it;
		++itg;
	}
}


void
GenerateTerms::positive(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
	const std::vector<std::string>& acc_global_prefix, uint64_t value)
{
	auto it = acc_prefix.begin();
	auto itg = acc_global_prefix.begin();
	for (const auto& acc : accuracy) {
		auto term_v = Serialise::positive(value - modulus(value, acc));
		doc.add_term(prefixed(term_v, *it, ctype_integer));
		doc.add_term(prefixed(term_v, *itg, ctype_integer));
		++it;
		++itg;
	}
}


void
GenerateTerms::date(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
	const std::vector<std::string>& acc_global_prefix, const Datetime::tm_t& tm)
{
	auto it = acc_prefix.begin();
	auto itg = acc_global_prefix.begin();
	for (const auto& acc : accuracy) {
		switch (static_cast<UnitTime>(acc)) {
			case UnitTime::MILLENNIUM: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::CENTURY: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::DECADE: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::YEAR: {
				Datetime::tm_t _tm(tm.year);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::MONTH: {
				Datetime::tm_t _tm(tm.year, tm.mon);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::DAY: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::HOUR: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::MINUTE: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			case UnitTime::SECOND: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
				auto term_v = Serialise::timestamp(Datetime::timegm(_tm));
				doc.add_term(prefixed(term_v, *it, ctype_date));
				doc.add_term(prefixed(term_v, *itg, ctype_date));
				break;
			}
			default:
				break;
		}
		++it;
		++itg;
	}
}


void
GenerateTerms::geo(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
	const std::vector<std::string>& acc_global_prefix, const std::vector<range_t>& ranges)
{
	// Index values and looking for terms generated by accuracy.
	const auto last_acc_pos = accuracy.size() - 1;

	// Convert accuracy to accuracy bits
	std::vector<uint64_t> acc_bits;
	acc_bits.resize(last_acc_pos + 1);
	for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
		acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
	}

	std::vector<std::unordered_set<uint64_t>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	auto id_trixels = HTM::getIdTrixels(ranges);

	for (const auto& id : id_trixels) {
		uint64_t last_pos = do_clz(id) - 64 + HTM_BITS_ID;
		last_pos &= ~1;  // Must be multiple of two.
		uint64_t val = id << last_pos;
		size_t pos = last_acc_pos;
		const auto it_e = acc_bits.rend();
		for (auto it = acc_bits.rbegin(); it != it_e && *it >= last_pos; ++it, --pos) {
			level_terms[pos].insert(val >> *it);
		}
	}

	// Insert terms generated by accuracy.
	for (size_t pos = 0; pos < level_terms.size(); ++pos) {
		const auto& terms = level_terms[pos];
		const auto& prefix = acc_prefix[last_acc_pos - pos];
		const auto& gprefix = acc_global_prefix[pos];
		for (const auto& term : terms) {
			const auto term_s = Serialise::positive(term);
			doc.add_term(prefixed(term_s, prefix, ctype_geo));
			doc.add_term(prefixed(term_s, gprefix, ctype_geo));
		}
	}
}


Xapian::Query
GenerateTerms::date(double start_, double end_, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	if (accuracy.empty() || end_ < start_) {
		return Xapian::Query();
	}

	auto tm_s = Datetime::to_tm_t(static_cast<std::time_t>(start_));
	auto tm_e = Datetime::to_tm_t(static_cast<std::time_t>(end_));

	uint64_t diff = tm_e.year - tm_s.year, acc = -1;
	// Find the accuracy needed.
	if (diff != 0u) {
		if (diff >= 1000) {
			acc = toUType(UnitTime::MILLENNIUM);
		} else if (diff >= 100) {
			acc = toUType(UnitTime::CENTURY);
		} else if (diff >= 10) {
			acc = toUType(UnitTime::DECADE);
		} else {
			acc = toUType(UnitTime::YEAR);
		}
	} else if (tm_e.mon != tm_s.mon) {
		acc = toUType(UnitTime::MONTH);
	} else if (tm_e.day != tm_s.day) {
		acc = toUType(UnitTime::DAY);
	} else if (tm_e.hour != tm_s.hour) {
		acc = toUType(UnitTime::HOUR);
	} else if (tm_e.min != tm_s.min) {
		acc = toUType(UnitTime::MINUTE);
	} else {
		acc = toUType(UnitTime::SECOND);
	}

	// Find the upper or equal accuracy.
	uint64_t pos = 0, len = accuracy.size();
	while (pos < len && accuracy[pos] <= acc) {
		++pos;
	}

	Xapian::Query query_upper;
	Xapian::Query query_needed;

	// If there is an upper accuracy.
	if (pos < len) {
		auto c_tm_s = tm_s;
		auto c_tm_e = tm_e;
		switch (static_cast<UnitTime>(accuracy[pos])) {
			case UnitTime::MILLENNIUM:
				query_upper = millennium(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::CENTURY:
				query_upper = century(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::DECADE:
				query_upper = decade(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::YEAR:
				query_upper = year(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::MONTH:
				query_upper = month(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::DAY:
				query_upper = day(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::HOUR:
				query_upper = hour(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::MINUTE:
				query_upper = minute(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::SECOND:
				query_upper = second(c_tm_s, c_tm_e, acc_prefix[pos], wqf);
				break;
			default:
				break;
		}
	}

	// If there is the needed accuracy.
	if (pos > 0 && acc == accuracy[--pos]) {
		switch (static_cast<UnitTime>(accuracy[pos])) {
			case UnitTime::MILLENNIUM:
				query_needed = millennium(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::CENTURY:
				query_needed = century(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::DECADE:
				query_needed = decade(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::YEAR:
				query_needed = year(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::MONTH:
				query_needed = month(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::DAY:
				query_needed = day(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::HOUR:
				query_needed = hour(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::MINUTE:
				query_needed = minute(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			case UnitTime::SECOND:
				query_needed = second(tm_s, tm_e, acc_prefix[pos], wqf);
				break;
			default:
				break;
		}
	}

	if (!query_upper.empty() && !query_needed.empty()) {
		return Xapian::Query(Xapian::Query::OP_AND, query_upper, query_needed);
	}
	if (!query_upper.empty()) {
		return query_upper;
	}
	return query_needed;
}


Xapian::Query
GenerateTerms::millennium(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_s.mon = tm_e.day = tm_e.mon = 1;
	tm_s.year = year(tm_s.year, 1000);
	tm_e.year = year(tm_e.year, 1000);
	size_t num_unions = (tm_e.year - tm_s.year) / 1000;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.year != tm_e.year) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			tm_s.year += 1000;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::century(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_s.mon = tm_e.day = tm_e.mon = 1;
	tm_s.year = year(tm_s.year, 100);
	tm_e.year = year(tm_e.year, 100);
	size_t num_unions = (tm_e.year - tm_s.year) / 100;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.year != tm_e.year) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			tm_s.year += 100;
		}
	}

	return query;
}


Xapian::Query
GenerateTerms::decade(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_s.mon = tm_e.day = tm_e.mon = 1;
	tm_s.year = year(tm_s.year, 10);
	tm_e.year = year(tm_e.year, 10);
	size_t num_unions = (tm_e.year - tm_s.year) / 10;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.year != tm_e.year) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			tm_s.year += 10;
		}
	}

	return query;
}


Xapian::Query
GenerateTerms::year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_s.mon = tm_e.day = tm_e.mon = 1;
	size_t num_unions = tm_e.year - tm_s.year;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.year != tm_e.year) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.year;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::month(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_e.day = 1;
	size_t num_unions = tm_e.mon - tm_s.mon;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.mon != tm_e.mon) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.mon;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::day(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	size_t num_unions = tm_e.day - tm_s.day;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.day != tm_e.day) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.day;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::hour(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_e.sec = tm_e.min = 0;
	size_t num_unions = tm_e.hour - tm_s.hour;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.hour != tm_e.hour) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.hour;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::minute(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_e.sec = 0;
	size_t num_unions = tm_e.min - tm_s.min;
	if (num_unions < MAX_TERMS) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.min != tm_e.min) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.min;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::second(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	size_t num_unions = tm_e.sec - tm_s.sec;
	if (num_unions < MAX_TERMS) {
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.sec != tm_e.sec) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			++tm_s.sec;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}


Xapian::Query
GenerateTerms::geo(const std::vector<range_t>& ranges, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	// The user does not specify the accuracy.
	if (acc_prefix.empty() || ranges.empty()) {
		return Xapian::Query();
	}

	const auto last_acc_pos = accuracy.size() - 1;

	// Convert accuracy to accuracy bits
	std::vector<uint64_t> acc_bits;
	acc_bits.resize(last_acc_pos + 1);
	for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
		acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
	}

	std::vector<std::unordered_set<uint64_t>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	auto id_trixels = HTM::getIdTrixels(ranges);

	size_t level_terms_size = 0;

	for (const auto& id : id_trixels) {
		uint64_t bits = do_clz(id) - 64 + HTM_BITS_ID;
		bits &= ~1;  // Must be multiple of two.
		uint64_t val = id << bits;
		size_t pos = last_acc_pos;
		const auto it_e = acc_bits.rend();
		for (auto it = acc_bits.rbegin(); it != it_e && *it >= bits; ++it, --pos) { }
		if (pos != last_acc_pos) {
			++pos;
			level_terms[pos].insert(val >> acc_bits[pos]);
			if (level_terms_size < pos + 1) {
				level_terms_size = pos + 1;
			}
		}
	}

	// The search has biggger trixels that the biggest trixel in accuracy.
	if (!level_terms_size) {
		return Xapian::Query();
	}

	// Simplify terms.
	const auto last_pos = level_terms_size - 1;
	for (size_t prev_pos = 0; prev_pos <= last_pos && prev_pos != last_acc_pos; ++prev_pos) {
		auto& terms = level_terms[prev_pos];
		if (!terms.empty()) {
			size_t pos = prev_pos + 1;
			auto& next_terms = level_terms[pos];
			if (level_terms_size < pos + 1) {
				level_terms_size = pos + 1;
			}
			uint64_t acc = acc_bits[pos] - acc_bits[prev_pos];
			for (const auto& term : terms) {
				next_terms.insert(term >> acc);
			}
		}
	}

	Xapian::Query query;
	auto u_pos = level_terms_size - 1;
	auto& u_terms = level_terms[u_pos];
	if (u_terms.size() < MAX_TERMS) {
		if (u_pos == last_pos) {
			// All terms are in last_pos and last_pos == last_acc_pos
			// Process only upper terms (lower terms discarded).
			auto upper = itertools::transform([&](uint64_t term){
				return Xapian::Query(prefixed(Serialise::positive(term), acc_prefix[last_acc_pos - u_pos], ctype_geo), wqf);
			}, u_terms.begin(), u_terms.end());
			query = Xapian::Query(Xapian::Query::OP_OR, upper.begin(), upper.end());
		} else {
			auto l_pos = u_pos - 1;
			while (level_terms[l_pos].empty()) {
				--l_pos;
			}
			auto& l_terms = level_terms[l_pos];
			if (u_terms.size() == l_terms.size()) {
				// All terms has a upper term, but for each lower term is a upper term.
				// Process only lower terms.
				auto lower = itertools::transform([&](uint64_t term){
					return Xapian::Query(prefixed(Serialise::positive(term), acc_prefix[last_acc_pos - l_pos], ctype_geo), wqf);
				}, l_terms.begin(), l_terms.end());
				query = Xapian::Query(Xapian::Query::OP_OR, lower.begin(), lower.end());
			} else if (l_terms.size() < MAX_TERMS) {
				// Process upper terms *and* lower.
				auto lower = itertools::transform([&](uint64_t term){
					return Xapian::Query(prefixed(Serialise::positive(term), acc_prefix[last_acc_pos - l_pos], ctype_geo), wqf);
				}, l_terms.begin(), l_terms.end());
				auto lower_query = Xapian::Query(Xapian::Query::OP_OR, lower.begin(), lower.end());
				auto upper = itertools::transform([&](uint64_t term){
					return Xapian::Query(prefixed(Serialise::positive(term), acc_prefix[last_acc_pos - u_pos], ctype_geo), wqf);
				}, u_terms.begin(), u_terms.end());
				auto upper_query = Xapian::Query(Xapian::Query::OP_OR, upper.begin(), upper.end());
				query = Xapian::Query(Xapian::Query::OP_AND, upper_query, lower_query);
			} else {
				// Process only upper terms.
				auto upper = itertools::transform([&](uint64_t term){
					return Xapian::Query(prefixed(Serialise::positive(term), acc_prefix[last_acc_pos - u_pos], ctype_geo), wqf);
				}, u_terms.begin(), u_terms.end());
				query = Xapian::Query(Xapian::Query::OP_OR, upper.begin(), upper.end());
			}
		}
	}

	return query;
}


template <typename T, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value>>
static inline Xapian::Query
_numeric(T start, T end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
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
		const std::string& prefix_up = acc_prefix[pos];

		// If there is a lower accuracy.
		if (pos > 0) {
			--pos;
			auto low_acc = accuracy[pos];
			T low_start = start - modulus(start, low_acc);
			T low_end = end - modulus(end, low_acc);
			// If terms are not too many terms (num_unions + 1).
			if (static_cast<size_t>((low_end - low_start) / low_acc) < MAX_TERMS) {
				const std::string& prefix_low = acc_prefix[pos];

				if (up_start == up_end) {
					size_t num_unions = (low_end - low_start) / low_acc;
					if (num_unions == 0) {
						query = Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
					} else {
						std::vector<Xapian::Query> query_low;
						query_low.reserve(num_unions);
						query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						while (low_start != low_end) {
							low_start += low_acc;
							query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						}
						query = Xapian::Query(Xapian::Query::OP_AND,
							Xapian::Query(prefixed(Serialise::serialise(up_start), prefix_up, ctype_integer), wqf),
							Xapian::Query(Xapian::Query::OP_OR, query_low.begin(), query_low.end()));
					}

				} else {
					size_t num_unions1 = (up_end - low_start) / low_acc;
					if (num_unions1 == 0) {
						query = Xapian::Query(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
					} else {
						std::vector<Xapian::Query> query_low;
						query_low.reserve(num_unions1);
						query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						while (low_start < up_end) {
							low_start += low_acc;
							if (low_start < up_end) {
								query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
							}
						}
						query = Xapian::Query(Xapian::Query::OP_AND,
							Xapian::Query(prefixed(Serialise::serialise(up_start), prefix_up, ctype_integer), wqf),
							Xapian::Query(Xapian::Query::OP_OR, query_low.begin(), query_low.end()));
					}

					size_t num_unions2 = (low_end - low_start) / low_acc;
					if (num_unions2 == 0) {
						Xapian::Query query_low(prefixed(Serialise::serialise(low_end), prefix_low, ctype_integer), wqf);
						query = Xapian::Query(Xapian::Query::OP_OR, query, query_low);
					} else {
						std::vector<Xapian::Query> query_low;
						query_low.reserve(num_unions2);
						query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						while (low_start != low_end) {
							low_start += low_acc;
							query_low.emplace_back(prefixed(Serialise::serialise(low_start), prefix_low, ctype_integer), wqf);
						}
						;
						query = Xapian::Query(Xapian::Query::OP_OR,
							query,
							Xapian::Query(Xapian::Query::OP_AND,
								Xapian::Query(prefixed(Serialise::serialise(up_end), prefix_up, ctype_integer), wqf),
								Xapian::Query(Xapian::Query::OP_OR, query_low.begin(), query_low.end())));
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
			const std::string& prefix = acc_prefix[pos];
			// Reserve upper bound.
			std::vector<Xapian::Query> query_low;
			query_low.reserve(num_unions);
			query_low.emplace_back(prefixed(Serialise::serialise(end), prefix, ctype_integer), wqf);
			while (start != end) {
				query_low.emplace_back(prefixed(Serialise::serialise(start), prefix, ctype_integer), wqf);
				start += low_acc;
			}
			query = Xapian::Query(Xapian::Query::OP_OR, query_low.begin(), query_low.end());
		}
	}

	return query;
}


Xapian::Query
GenerateTerms::numeric(int64_t start, int64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	return _numeric(start, end, accuracy, acc_prefix, wqf);
}


Xapian::Query
GenerateTerms::numeric(uint64_t start, uint64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	return _numeric(start, end, accuracy, acc_prefix, wqf);
}
