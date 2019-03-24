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
#include <unordered_map>                          // for std::unordered_map
#include <vector>                                 // for std::vector

#include "config.h"                               // for HAVE_LIBCPP
#include "datetime.h"                             // for tm_t, timegm, to_tm_t
#include "itertools.hh"                           // for iterator::map, iterator::chain
#include "utils/math.hh"                          // for modulus
#include "reversed.hh"                            // for reversed
#include "schema.h"                               // for required_spc_t, FieldType, UnitTime
#include "utype.hh"                               // for toUType


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #define L_GENERATE_TERMS L_DIM_GREY


#ifndef L_GENERATE_TERMS
#define L_GENERATE_TERMS L_NOTHING
#endif

const char ctype_date    = required_spc_t::get_ctype(FieldType::DATETIME);
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


template <typename T>
struct Tree {
	bool leaf;
	size_t pos;

#ifdef HAVE_LIBCPP
	// [https://stackoverflow.com/a/27564183/167522]
	// In libstdc++, std::unordered_map doesn't take
	// uncomplete types but std::map does.
	std::unordered_map<T, Tree<T>> terms;
#else
	std::map<T, Tree<T>> terms;
#endif

	Tree() : leaf{false}, pos{0} {}
};


template <size_t mode, typename Tree>
static inline void
get_trixels(std::vector<std::string>& trixels, Tree* tree, const std::vector<uint64_t>& accuracy, size_t& max_terms)
{
	const auto accuracy_size = accuracy.size();
	const auto terms_size = tree->terms.size();
	size_t max_terms_level = tree->pos >= accuracy_size ? mode : tree->pos > 0 ? mode == 2 ? accuracy[tree->pos] / accuracy[tree->pos - 1] : 1 << (accuracy[tree->pos] - accuracy[tree->pos - 1]) : 0;

	for (const auto& t : tree->terms) {
		const auto size = t.second.terms.size();
		if ((!t.second.leaf && terms_size == 1 && size <= max_terms_level * 0.1) || tree->pos >= accuracy_size) {
			// Skip level if:
			//   It's a lonely (single node) non-leaf level with less than 10% of its children terms set
			//   It's the topmost "magic" level
			get_trixels<mode>(trixels, &t.second, accuracy, max_terms);
		} else {
			// Add level and...
			trixels.push_back(HTM::getTrixelName(t.first));
			// don't filter children if level is leaf or it has too many children
			if (!t.second.leaf && size <= max_terms && size < max_terms_level * 0.9) {
				// filter if there are less than 90% of its children terms set and there's still available room in max_terms
				if (size) {
					get_trixels<mode>(trixels, &t.second, accuracy, max_terms);
				}
				max_terms -= size;
			}
		}
	}
}


template <size_t mode, typename Tree, typename S>
static inline void
print(Tree* tree, const std::vector<uint64_t>& accuracy, const std::vector<S>& acc_prefix, char field_type, size_t& max_terms, int level = 0)
{
	std::string indent;
	for (int i = 0; i < level; ++i) {
		indent.append("  ");
	}

	const auto accuracy_size = accuracy.size();
	const auto terms_size = tree->terms.size();
	size_t max_terms_level = tree->pos >= accuracy_size ? mode : tree->pos > 0 ? mode == 2 ? accuracy[tree->pos] / accuracy[tree->pos - 1] : 1 << (accuracy[tree->pos] - accuracy[tree->pos - 1]) : 0;
	const auto& prefix = tree->pos >= accuracy_size ? "" : acc_prefix[tree->pos];

	for (const auto& t : tree->terms) {
		const auto size = t.second.terms.size();
		if ((!t.second.leaf && terms_size == 1 && size <= max_terms_level * 0.1) || tree->pos >= accuracy_size) {
			// Skip level if:
			//   It's a lonely (single node) non-leaf level with less than 10% of its children terms set
			//   It's the topmost "magic" level
			L_GREY("{}{} - {}:{}:{} ({}/{}) (skipped)", indent, tree->pos, t.first, mode == 2 ? "" : HTM::getTrixelName(t.first), repr(prefixed(Serialise::serialise(t.first), prefix, field_type)), size, max_terms_level);
			print<mode>(&t.second, accuracy, acc_prefix, field_type, max_terms, level + 2);
		} else {
			// Add level and...
			// don't filter children if level is leaf or it has too many children
			if (!t.second.leaf && size <= max_terms && size < max_terms_level * 0.9) {
				// filter if there are less than 90% of its children terms set and there's still available room in max_terms
				L_GREY("{}{} - {}:{}:{} ({}/{}) (filtered)", indent, tree->pos, t.first, mode == 2 ? "" : HTM::getTrixelName(t.first), repr(prefixed(Serialise::serialise(t.first), prefix, field_type)), size, max_terms_level);
				if (size) {
					print<mode>(&t.second, accuracy, acc_prefix, field_type, max_terms, level + 2);
				}
				max_terms -= size;
			} else {
				L_GREY("{}{} - {}:{}:{} ({}/{}) (whole)", indent, tree->pos, t.first, mode == 2 ? "" : HTM::getTrixelName(t.first), repr(prefixed(Serialise::serialise(t.first), prefix, field_type)), size, max_terms_level);
			}
		}
	}
}


template <size_t mode, typename Tree, typename S>
static inline Xapian::Query
get_query(Tree* tree, const std::vector<uint64_t>& accuracy, const std::vector<S>& acc_prefix, Xapian::termcount wqf, char field_type, size_t& max_terms)
{
	const auto accuracy_size = accuracy.size();
	const auto terms_size = tree->terms.size();
	size_t max_terms_level = tree->pos >= accuracy_size ? mode : tree->pos > 0 ? mode == 2 ? accuracy[tree->pos] / accuracy[tree->pos - 1] : 1 << (accuracy[tree->pos] - accuracy[tree->pos - 1]) : 0;
	const auto& prefix = tree->pos >= accuracy_size ? "" : acc_prefix[tree->pos];

	std::vector<Xapian::Query> queries;
	queries.reserve(terms_size);
	for (const auto& t : tree->terms) {
		const auto size = t.second.terms.size();
		if ((!t.second.leaf && terms_size == 1 && size <= max_terms_level * 0.1) || tree->pos >= accuracy_size) {
			// Skip level if:
			//   It's a lonely (single node) non-leaf level with less than 10% of its children terms set
			//   It's the topmost "magic" level
			return get_query<mode>(&t.second, accuracy, acc_prefix, wqf, field_type, max_terms);
		} else {
			// Add level and...
			auto query = Xapian::Query(prefixed(Serialise::serialise(t.first), prefix, field_type), wqf);
			// don't filter children if level is leaf or it has too many children
			if (!t.second.leaf && size <= max_terms && size < max_terms_level * 0.9) {
				// filter if there are less than 90% of its children terms set and there's still available room in max_terms
				if (size) {
					query = Xapian::Query(Xapian::Query::OP_AND, query,
						get_query<mode>(&t.second, accuracy, acc_prefix, wqf, field_type, max_terms)
					);
				}
				max_terms -= size;
			}
			queries.push_back(query);
		}
	}
	return Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
}


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
GenerateTerms::datetime(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, const Datetime::tm_t& tm)
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
			case UnitTime::INVALID:
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
	std::vector<uint64_t> inv_acc_bits;
	inv_acc_bits.resize(last_acc_pos + 1);
	if (accuracy.size() != 0) {
		for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
			inv_acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
		}
	}

	std::vector<std::unordered_set<uint64_t>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	auto id_trixels = HTM::getIdTrixels(ranges);

	for (const auto& id : id_trixels) {
		uint64_t last_pos = do_clz(id) - 64 + HTM_BITS_ID;
		last_pos &= ~1;  // Must be multiple of two.
		uint64_t val = id << last_pos;
		size_t pos = last_acc_pos;
		const auto it_e = inv_acc_bits.rend();
		for (auto it = inv_acc_bits.rbegin(); it != it_e && *it >= last_pos; ++it, --pos) {
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
GenerateTerms::datetime(Xapian::Document& doc, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix,
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
			case UnitTime::INVALID:
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
	std::vector<uint64_t> inv_acc_bits;
	inv_acc_bits.resize(last_acc_pos + 1);
	for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
		inv_acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
	}

	std::vector<std::unordered_set<uint64_t>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	auto id_trixels = HTM::getIdTrixels(ranges);

	for (const auto& id : id_trixels) {
		uint64_t last_pos = do_clz(id) - 64 + HTM_BITS_ID;
		last_pos &= ~1;  // Must be multiple of two.
		uint64_t val = id << last_pos;
		size_t pos = last_acc_pos;
		const auto it_e = inv_acc_bits.rend();
		for (auto it = inv_acc_bits.rbegin(); it != it_e && *it >= last_pos; ++it, --pos) {
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
GenerateTerms::datetime(double start_, double end_, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
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
			case UnitTime::INVALID:
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
			case UnitTime::INVALID:
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


template <int accuracy>
static inline Xapian::Query
_year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_s.mon = tm_e.day = tm_e.mon = 1;
	tm_s.year = GenerateTerms::year(tm_s.year, accuracy);
	tm_e.year = GenerateTerms::year(tm_e.year, accuracy);
	size_t num_unions = (tm_e.year - tm_s.year) / accuracy;
	if (num_unions < MAX_TERMS_LEVEL) {
		// Reserve upper bound.
		std::vector<Xapian::Query> queries;
		queries.reserve(num_unions);
		queries.emplace_back(prefixed(Serialise::serialise(tm_e), prefix, ctype_date), wqf);
		while (tm_s.year != tm_e.year) {
			queries.emplace_back(prefixed(Serialise::serialise(tm_s), prefix, ctype_date), wqf);
			tm_s.year += accuracy;
		}
		query = Xapian::Query(Xapian::Query::OP_OR, queries.begin(), queries.end());
	}

	return query;
}



Xapian::Query
GenerateTerms::millennium(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	return _year<1000>(tm_s, tm_e, prefix, wqf);
}


Xapian::Query
GenerateTerms::century(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	return _year<100>(tm_s, tm_e, prefix, wqf);
}


Xapian::Query
GenerateTerms::decade(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	return _year<10>(tm_s, tm_e, prefix, wqf);
}


Xapian::Query
GenerateTerms::year(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	return _year<1>(tm_s, tm_e, prefix, wqf);
}


Xapian::Query
GenerateTerms::month(Datetime::tm_t& tm_s, Datetime::tm_t& tm_e, const std::string& prefix, Xapian::termcount wqf)
{
	Xapian::Query query;

	tm_s.sec = tm_s.min = tm_s.hour = tm_e.sec = tm_e.min = tm_e.hour = 0;
	tm_s.day = tm_e.day = 1;
	size_t num_unions = tm_e.mon - tm_s.mon;
	if (num_unions < MAX_TERMS_LEVEL) {
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
	if (num_unions < MAX_TERMS_LEVEL) {
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
	if (num_unions < MAX_TERMS_LEVEL) {
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
	if (num_unions < MAX_TERMS_LEVEL) {
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
	if (num_unions < MAX_TERMS_LEVEL) {
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
	std::vector<uint64_t> inv_acc_bits;
	inv_acc_bits.resize(last_acc_pos + 1);
	std::vector<std::string_view> inv_acc_prefix;
	inv_acc_prefix.resize(last_acc_pos + 1);
	for (size_t pos = 0; pos <= last_acc_pos; ++pos) {
		inv_acc_bits[pos] = HTM_START_POS - (accuracy[last_acc_pos - pos] * 2);
		inv_acc_prefix[pos] = acc_prefix[last_acc_pos - pos];
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
		const auto it_e = inv_acc_bits.rend();
		for (auto it = inv_acc_bits.rbegin(); it != it_e && *it >= bits; ++it, --pos) { }
		if (pos != last_acc_pos) {
			++pos;
			level_terms[pos].insert(val >> inv_acc_bits[pos]);
			if (level_terms_size < pos + 1) {
				level_terms_size = pos + 1;
			}
		}
	}

	// The search has biggger trixels that the biggest trixel in accuracy.
	if (!level_terms_size) {
		return Xapian::Query();
	}

	// Generate tree.
	Tree<uint64_t> root;
	root.pos = level_terms_size;
	for (size_t pos = 0; pos < level_terms_size; ++pos) {
		const auto& terms = level_terms[pos];
		const auto acc = inv_acc_bits[pos];
		for (const auto& term : terms) {
			auto current = &root;
			for (size_t current_pos = level_terms_size; current_pos > pos; --current_pos) {
				current->pos = current_pos;
				auto current_term = current_pos <= last_acc_pos ? term >> (inv_acc_bits[current_pos] - acc) : 0;
				current = &current->terms[current_term];
			}
			current->pos = pos;
			current = &current->terms[term];
			current->leaf = true;
		}
	}

	size_t max_terms = MAX_TERMS;

	// std::vector<std::string> trixels;
	// get_trixels<8>(trixels, &root, inv_acc_bits, max_terms);
	// HTM::writeGoogleMap("GoogleMap.py", "GoogleMap.html", nullptr, trixels);
	// max_terms = MAX_TERMS;

	// print<8>(&root, inv_acc_bits, inv_acc_prefix, ctype_geo, max_terms);
	// max_terms = MAX_TERMS;

	// Create query.
	return get_query<8>(&root, inv_acc_bits, inv_acc_prefix, wqf, ctype_geo, max_terms);
}


template <typename T>
Xapian::Query
_numeric(T start, T end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf, size_t max_terms, size_t max_terms_level)
{
	if (accuracy.empty() || end < start) {
		return Xapian::Query();
	}

	const auto last_acc_pos = accuracy.size() - 1;
	std::vector<std::vector<T>> level_terms;
	level_terms.resize(last_acc_pos + 1);

	size_t total = 0;
	size_t level_terms_size = 0;

	const auto max_acc = max<T>(accuracy);
	const auto min_acc = min<T>(accuracy);
	auto lower_end = start <= min_acc ? min_acc : max_acc;
	auto upper_start = end >= max_acc ? max_acc : min_acc;
	if (end < lower_end) {
		lower_end = end;
	}
	if (start > upper_start) {
		upper_start = start;
	}

	bool invalid_initial = true;
	T initial;
	bool invalid_final = true;
	T final;

	L_GENERATE_TERMS("(start, end): {}..{}", start, end);

	const auto last_pos = accuracy.size() - 1;
	auto pos = last_pos;
	do {
		T acc;
		if (accuracy[pos] < std::numeric_limits<T>::max()) {
			acc = accuracy[pos];
		} else {
			acc = std::numeric_limits<T>::max();
		}

		auto lower_start = add<T>(start, acc - 1);
		lower_start = sub<T>(lower_start, modulus(lower_start, acc));
		if (start == std::numeric_limits<T>::min()) {
			lower_start = std::numeric_limits<T>::min();
		}

		auto upper_end = sub<T>(end, modulus(end, acc));
		if (end == std::numeric_limits<T>::max()) {
			upper_end = std::numeric_limits<T>::max();
		}

		if (lower_start < upper_end || pos == 0) {
			L_GENERATE_TERMS("pos={} <acc={}> -> [lower_start={}, lower_end={}, upper_start={}, upper_end={}]", pos, acc, lower_start, lower_end, upper_start, upper_end);

			if (lower_end > upper_end) {
				lower_end = add<T>(upper_end, acc);
			}
			if (upper_start < lower_end) {
				upper_start = sub<T>(lower_end, acc);
			}

			if (lower_start <= lower_end) {
				size_t num_unions = (lower_end - lower_start) / acc;
				L_GENERATE_TERMS("  [lower_start={}, lower_end={}] (num_unions={})",lower_start, lower_end, num_unions);
				if (num_unions > max_terms_level || total + num_unions > max_terms) {
					lower_start = std::numeric_limits<T>::min();
					if (pos != last_pos && !invalid_initial) {
						level_terms[pos].push_back(initial);
						// L_GENERATE_TERMS("      >> {} (initial)", initial);
					}
				} else {
					L_GENERATE_TERMS("    LOWER: lower_start={} -> lower_end={}", lower_start, lower_end);
					initial = sub<T>(lower_start, acc, invalid_initial);
					invalid_initial = initial <= min_acc;
					for (auto lower = lower_start; lower < lower_end; lower = add<T>(lower, acc, invalid_initial), ++total) {
						level_terms[pos].push_back(lower);
						// L_GENERATE_TERMS("      >> {}", lower);
					}
					if (pos == 0 && !invalid_initial) {
						level_terms[pos].push_back(initial);
						// L_GENERATE_TERMS("      >> {} (initial)", initial);
					}
				}
				if (level_terms_size < pos + 1) {
					level_terms_size = pos + 1;
				}
			}

			if (upper_start <= upper_end) {
				size_t num_unions = (upper_end - upper_start) / acc;
				L_GENERATE_TERMS("  [upper_start={}, upper_end={}] (num_unions={})", upper_start, upper_end, num_unions);
				if (num_unions > max_terms_level || total + num_unions > max_terms) {
					upper_end = std::numeric_limits<T>::max();
					if (pos != last_pos && !invalid_final) {
						level_terms[pos].push_back(final);
						// L_GENERATE_TERMS("      >> {} (final)", final);
					}
				} else {
					L_GENERATE_TERMS("    UPPER: upper_start={} -> upper_end={}", upper_start, upper_end);
					final = add<T>(upper_end, acc, invalid_final);
					invalid_final = final >= max_acc || lower_start >= upper_end;
					for (auto upper = upper_end; upper > upper_start; upper = sub<T>(upper, acc, invalid_final), ++total) {
						level_terms[pos].push_back(upper);
						// L_GENERATE_TERMS("      >> {}", upper);
					}
					if (pos == 0 && !invalid_final) {
						level_terms[pos].push_back(final);
						// L_GENERATE_TERMS("      >> {} (final)", final);
					}
				}
				if (level_terms_size < pos + 1) {
					level_terms_size = pos + 1;
				}
			}

			lower_end = lower_start;
			upper_start = upper_end;
		}
	} while (pos--);

	// The search has biggger ranges that the biggest range in accuracy.
	if (!level_terms_size) {
		return Xapian::Query();
	}

	// Generate tree.
	Tree<T> root;
	root.pos = level_terms_size;
	for (pos = 0; pos < level_terms_size; ++pos) {
		const auto& terms = level_terms[pos];
		for (const auto& term : terms) {
			auto current = &root;
			for (size_t current_pos = level_terms_size; current_pos > pos; --current_pos) {
				current->pos = current_pos;
				auto current_term = current_pos <= last_pos ? term - modulus(term, accuracy[current_pos]) : 0;
				current = &current->terms[current_term];
			}
			current->pos = pos;
			current = &current->terms[term];
			current->leaf = true;
		}
	}

	// print<2>(&root, accuracy, acc_prefix, ctype_integer, max_terms);
	// max_terms = MAX_TERMS;

	// Create query.
	return get_query<2>(&root, accuracy, acc_prefix, wqf, ctype_integer, max_terms);
}


Xapian::Query
GenerateTerms::numeric(int64_t start, int64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	return _numeric(start, end, accuracy, acc_prefix, wqf, MAX_TERMS, MAX_TERMS_LEVEL);
}


Xapian::Query
GenerateTerms::numeric(uint64_t start, uint64_t end, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix, Xapian::termcount wqf)
{
	return _numeric(start, end, accuracy, acc_prefix, wqf, MAX_TERMS, MAX_TERMS_LEVEL);
}
