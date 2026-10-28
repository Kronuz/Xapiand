/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Unit tests for GenerateTerms numeric range-term generation. Revived from the
// retired oldtests generate_terms suite, re-expressed structurally (does a range
// produce terms, and when does it collapse to the empty query) instead of pinning
// the exact Xapian query descriptions, which encode internal term bytes.

#include "test_harness.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "database/schema.h"
#include "database/utils.h"
#include "datetime.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "engine/xapian/shard.h"
#include "engine/xapian/xapian.h"
#include "multivalue/generate_terms.h"
#include "multivalue/range.h"
#include "exception_xapian.h"
#include "msgpack.h"
#include "utype.hh"

static void test_numeric_terms() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000, 10000, 100000 };
	const std::vector<std::string> prefix = { "N1", "N2", "N3", "N4", "N5", "N6" };

	// A range that fits within the accuracies produces a non-empty query.
	auto q = GenerateTerms::numeric(uint64_t(1200), uint64_t(2500), accuracy, prefix);
	CHECK_FALSE(q.empty());

	// A tight range still produces terms.
	auto q_small = GenerateTerms::numeric(uint64_t(1200), uint64_t(1200), accuracy, prefix);
	CHECK_FALSE(q_small.empty());

	// A large range still produces terms at the coarser accuracies.
	auto q_wide = GenerateTerms::numeric(uint64_t(0), uint64_t(1000000), accuracy, prefix);
	CHECK_FALSE(q_wide.empty());
}

static void test_numeric_signed() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000 };
	const std::vector<std::string> prefix = { "N1", "N2", "N3", "N4" };

	// The signed overload handles negative ranges.
	auto q = GenerateTerms::numeric(int64_t(-500), int64_t(500), accuracy, prefix);
	CHECK_FALSE(q.empty());
}

// The accuracy terms of the id field must not lead with the id term prefix, so that the id term is the
// only one that does; every other field keeps its own prefix.
static void test_id_accuracy_prefix() {
	CHECK_EQ(std::string(acc_term_prefix(DOCUMENT_ID_TERM_PREFIX)), std::string(DOCUMENT_ID_ACCURACY_TERM_PREFIX));
	CHECK_EQ(std::string(acc_term_prefix("age.")), std::string("age."));
	CHECK_EQ(std::string(acc_term_prefix("")), std::string());

	const std::vector<uint64_t> accuracy = { 10, 100 };
	std::vector<std::string> acc_prefix;
	for (auto acc : accuracy) {
		acc_prefix.push_back(std::string(acc_term_prefix(DOCUMENT_ID_TERM_PREFIX)) + get_prefix(acc));
	}
	engine::Document doc;
	GenerateTerms::integer(doc, accuracy, acc_prefix, 1234);
	const auto stored = doc.serialise();
	CHECK(stored.find(std::string(DOCUMENT_ID_ACCURACY_TERM_PREFIX) + get_prefix(100)) != std::string::npos);
	CHECK(stored.find(std::string(DOCUMENT_ID_TERM_PREFIX) + get_prefix(100)) == std::string::npos);
	CHECK_FALSE(GenerateTerms::numeric(int64_t(1200), int64_t(1300), accuracy, acc_prefix).empty());
}

// Documents indexed with GenerateTerms::integer and searched with GenerateTerms::numeric through the
// real engine: a range query must find every document whose value is in the range. (An empty query
// means "no term filter": the value-range source alone decides, so every document passes it.)
static void test_numeric_range_finds_every_value_in_range() {
	const std::vector<uint64_t> accuracy = { 100, 1000, 10000, 100000, 1000000, 100000000 };
	std::vector<std::string> prefix;
	for (size_t i = 0; i < accuracy.size(); ++i) {
		prefix.push_back("N" + std::to_string(i));
	}

	std::set<int64_t> set = { 0, 1, 99, 100, 101, 999, 1000, 1001, 1999, 2000, 2001, 2500, 99999, 100000, 999999, 1000000, 1000001,
		5000000, 99999999, 100000000, 150000000, 199999999, 200000000, 200000001, 250000000, 299999999, 300000000, 3000000000LL,
		-1, -99, -100, -101, -1000, -100000000, -200000000, -300000000, std::numeric_limits<int64_t>::min() + 1, std::numeric_limits<int64_t>::max() };
	for (int k = 16; k <= 40; ++k) {
		for (int d = -1; d <= 1; ++d) {
			set.insert((int64_t(1) << k) + d);
		}
	}
	const std::vector<int64_t> values(set.begin(), set.end());

	const auto path = (std::filesystem::current_path() / "test_generate_terms_shard").string();
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (auto v : values) {
		engine::Document doc;
		GenerateTerms::integer(doc, accuracy, prefix, v);
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));

	std::mt19937_64 rng(7);
	std::vector<std::pair<int64_t, int64_t>> ranges = { {0, 0}, {100, 100}, {0, 3000000000LL}, {1, 3000000000LL}, {1999, 4294967295LL},
		{100000, 134217729}, {2, 150416086}, {55, 2147483647}, {0, 250000000}, {268435455, 268435457}, {-5, 5}, {-300000000, 0} };
	for (int i = 0; i < 300; ++i) {
		auto a = values[rng() % values.size()], b = values[rng() % values.size()];
		ranges.push_back({std::min(a, b), std::max(a, b)});
	}

	size_t missing = 0;
	for (const auto& [lo, hi] : ranges) {
		auto query = GenerateTerms::numeric(lo, hi, accuracy, prefix);
		std::set<engine::docid> found;
		if (query.empty()) {
			for (size_t i = 0; i < values.size(); ++i) {
				found.insert(static_cast<engine::docid>(i + 1));
			}
		} else {
			auto prepared = engine::prepare(query);
			engine::SearchRequest request;
			request.query = prepared.get();
			request.maxitems = 1000;
			request.check_at_least = 1000;
			request.origin = "test";
			std::vector<engine::Shard*> shards{&shard};
			for (const auto& hit : engine::sync_wait(engine::search(shards, request)).hits) {
				found.insert(hit.did);
			}
		}
		for (size_t i = 0; i < values.size(); ++i) {
			if (values[i] >= lo && values[i] <= hi && !found.count(static_cast<engine::docid>(i + 1))) {
				++missing;
			}
		}
	}
	std::filesystem::remove_all(path);
	CHECK_EQ(missing, size_t(0));
}

// The same through the real engine for datetime ranges, over the default date and datetime accuracies.
static void test_datetime_range_finds_every_value_in_range() {
	const std::vector<std::vector<uint64_t>> lists = {
		{ toUType(UnitTime::hour), toUType(UnitTime::day), toUType(UnitTime::month), toUType(UnitTime::year), toUType(UnitTime::decade), toUType(UnitTime::century) },
		{ toUType(UnitTime::day), toUType(UnitTime::month), toUType(UnitTime::year), toUType(UnitTime::decade), toUType(UnitTime::century) },
		{ toUType(UnitTime::day), toUType(UnitTime::year) },
	};

	std::vector<double> stamps;
	for (int year : { 1899, 1900, 1969, 1970, 1971, 1999, 2000, 2001, 2019, 2020, 2037, 2038, 2039, 2100, 2300 }) {
		for (auto [mon, day, hour, min, sec] : std::vector<std::array<int, 5>>{ {1, 1, 0, 0, 0}, {1, 1, 0, 0, 1}, {2, 28, 23, 59, 59}, {3, 1, 0, 0, 0}, {6, 15, 12, 34, 56}, {12, 31, 23, 59, 59} }) {
			Datetime::tm_t tm(year, mon, day, hour, min, sec);
			stamps.push_back(static_cast<double>(Datetime::timegm(tm)));
		}
	}
	std::sort(stamps.begin(), stamps.end());

	const auto path = (std::filesystem::current_path() / "test_generate_terms_shard_date").string();
	std::mt19937_64 rng(11);
	size_t missing = 0;
	for (const auto& accuracy : lists) {
		std::vector<std::string> prefix;
		for (size_t i = 0; i < accuracy.size(); ++i) {
			prefix.push_back("D" + std::to_string(i));
		}
		std::filesystem::remove_all(path);
		auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
		auto& shard = *opened.shard;
		for (auto t : stamps) {
			engine::Document doc;
			GenerateTerms::datetime(doc, accuracy, prefix, Datetime::to_tm_t(static_cast<std::time_t>(t)));
			engine::sync_wait(shard.add(doc, {}));
		}
		engine::sync_wait(shard.commit(engine::Transaction::none));

		for (int i = 0; i < 400; ++i) {
			auto a = stamps[rng() % stamps.size()], b = stamps[rng() % stamps.size()];
			if (rng() % 2) {
				a += static_cast<double>(rng() % 100000) - 50000;
			}
			if (b < a) {
				std::swap(a, b);
			}
			auto query = GenerateTerms::datetime(a, b, accuracy, prefix);
			std::set<engine::docid> found;
			if (query.empty()) {
				for (size_t j = 0; j < stamps.size(); ++j) {
					found.insert(static_cast<engine::docid>(j + 1));
				}
			} else {
				auto prepared = engine::prepare(query);
				engine::SearchRequest request;
				request.query = prepared.get();
				request.maxitems = 1000;
				request.check_at_least = 1000;
				request.origin = "test";
				std::vector<engine::Shard*> shards{&shard};
				for (const auto& hit : engine::sync_wait(engine::search(shards, request)).hits) {
					found.insert(hit.did);
				}
			}
			for (size_t j = 0; j < stamps.size(); ++j) {
				if (stamps[j] >= a && stamps[j] <= b && !found.count(static_cast<engine::docid>(j + 1))) {
					++missing;
				}
			}
		}
	}
	std::filesystem::remove_all(path);
	CHECK_EQ(missing, size_t(0));
}

// Ranges on terms-only fields (no value slot): there is no exact check behind the terms, so a range the
// terms cannot cover exactly (an open end, or wider than the budget) must be an error, never an empty answer.
static MsgPack range_obj(const MsgPack* from, const MsgPack* to) {
	MsgPack obj = MsgPack::MAP();
	if (from) {
		obj["_from"] = *from;
	}
	if (to) {
		obj["_to"] = *to;
	}
	return obj;
}

static std::set<engine::docid> search_hits(engine::Shard& shard, const engine::Query& query) {
	std::set<engine::docid> found;
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1000;
	request.check_at_least = 1000;
	request.origin = "test";
	std::vector<engine::Shard*> shards{&shard};
	for (const auto& hit : engine::sync_wait(engine::search(shards, request)).hits) {
		found.insert(hit.did);
	}
	return found;
}

static required_spc_t make_spc(engine::valueno slot, FieldType type, const std::vector<uint64_t>& accuracy) {
	std::vector<std::string> prefix;
	for (size_t i = 0; i < accuracy.size(); ++i) {
		prefix.push_back("T" + std::to_string(i));
	}
	return required_spc_t(slot, type, accuracy, prefix);
}

static void test_terms_only_numeric_range() {
	const std::vector<uint64_t> accuracy = { 100, 1000, 10000 };
	const std::vector<int64_t> values = { -5000000, -1, 0, 99, 100, 5000, 123456, 4000000, 40000000 };
	const auto path = (std::filesystem::current_path() / "test_generate_terms_shard_terms_only").string();
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	auto spc = make_spc(engine::BAD_VALUENO, FieldType::integer, accuracy);
	for (auto v : values) {
		engine::Document doc;
		GenerateTerms::integer(doc, spc.accuracy, spc.acc_prefix, v);
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));

	// Closed ranges within the budget find every value inside.
	size_t missing = 0;
	for (const auto& [lo, hi] : std::vector<std::pair<int64_t, int64_t>>{ {0, 0}, {-1, 100}, {99, 5000}, {1, 123456}, {-6000000, 30000000} }) {
		MsgPack from(lo), to(hi);
		auto found = search_hits(shard, MultipleValueRange::getQuery(spc, range_obj(&from, &to)));
		for (size_t i = 0; i < values.size(); ++i) {
			if (values[i] >= lo && values[i] <= hi && !found.count(static_cast<engine::docid>(i + 1))) {
				++missing;
			}
		}
	}
	CHECK_EQ(missing, size_t(0));

	// An inverted range is empty, not an error.
	MsgPack five(5), one(1);
	CHECK(MultipleValueRange::getQuery(spc, range_obj(&five, &one)).empty());

	// Open ends and ranges of more than 4096 coarsest buckets are errors.
	MsgPack zero(0), wide(1000000000LL);
	CHECK_THROW(MultipleValueRange::getQuery(spc, range_obj(&five, nullptr)), TermsOnlyRangeError);
	CHECK_THROW(MultipleValueRange::getQuery(spc, range_obj(nullptr, &five)), TermsOnlyRangeError);
	CHECK_THROW(MultipleValueRange::getQuery(spc, range_obj(&zero, &wide)), TermsOnlyRangeError);
	std::string what;
	try {
		MultipleValueRange::getQuery(spc, range_obj(&five, nullptr));
	} catch (const QueryParserError& exc) {
		what = exc.what();
	}
	CHECK(what.find("open-ended") != std::string::npos);
	what.clear();
	try {
		MultipleValueRange::getQuery(spc, range_obj(&zero, &wide));
	} catch (const QueryParserError& exc) {
		what = exc.what();
	}
	CHECK(what.find("too wide") != std::string::npos);

	// The same for the other numeric kinds.
	for (auto type : { FieldType::positive, FieldType::floating }) {
		auto s = make_spc(engine::BAD_VALUENO, type, accuracy);
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(&five, nullptr)), TermsOnlyRangeError);
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(nullptr, &five)), TermsOnlyRangeError);
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(&zero, &wide)), TermsOnlyRangeError);
		CHECK_FALSE(MultipleValueRange::getQuery(s, range_obj(&zero, &five)).empty());
	}
	MsgPack day(86399), tiny(1);
	for (auto type : { FieldType::time, FieldType::timedelta }) {
		auto s = make_spc(engine::BAD_VALUENO, type, { 1, 10 });
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(&tiny, nullptr)), TermsOnlyRangeError);
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(nullptr, &tiny)), TermsOnlyRangeError);
		CHECK_THROW(MultipleValueRange::getQuery(s, range_obj(&zero, &day)), TermsOnlyRangeError);
		CHECK_FALSE(MultipleValueRange::getQuery(s, range_obj(&zero, &tiny)).empty());
	}

	// With a value slot the source decides: open ends and wide ranges stay queries.
	auto with_slot = make_spc(7, FieldType::integer, accuracy);
	CHECK_FALSE(MultipleValueRange::getQuery(with_slot, range_obj(&five, nullptr)).empty());
	CHECK_FALSE(MultipleValueRange::getQuery(with_slot, range_obj(nullptr, &five)).empty());
	CHECK_FALSE(MultipleValueRange::getQuery(with_slot, range_obj(&zero, &wide)).empty());
	std::filesystem::remove_all(path);
}

static void test_terms_only_datetime_range() {
	const std::vector<uint64_t> accuracy = { toUType(UnitTime::day), toUType(UnitTime::month), toUType(UnitTime::year) };
	MsgPack a("2020-03-01T00:00:00"), b("2020-03-09T12:00:00"), early("1000-01-01T00:00:00"), late("5000-01-01T00:00:00");
	for (auto type : { FieldType::datetime, FieldType::date }) {
		auto spc = make_spc(engine::BAD_VALUENO, type, accuracy);
		CHECK_THROW(MultipleValueRange::getQuery(spc, range_obj(&a, nullptr)), TermsOnlyRangeError);
		CHECK_THROW(MultipleValueRange::getQuery(spc, range_obj(nullptr, &b)), TermsOnlyRangeError);
		CHECK_FALSE(MultipleValueRange::getQuery(spc, range_obj(&a, &b)).empty());

		// A span with no usable accuracy among the terms has no cover.
		auto only_year = make_spc(engine::BAD_VALUENO, type, { toUType(UnitTime::year) });
		CHECK_THROW(MultipleValueRange::getQuery(only_year, range_obj(&early, &late)), TermsOnlyRangeError);

		// With a value slot the same ranges are queries.
		auto with_slot = make_spc(9, type, { toUType(UnitTime::year) });
		CHECK_FALSE(MultipleValueRange::getQuery(with_slot, range_obj(&a, nullptr)).empty());
		CHECK_FALSE(MultipleValueRange::getQuery(with_slot, range_obj(&early, &late)).empty());
	}
}

TEST_MAIN(test_numeric_terms, test_numeric_signed, test_id_accuracy_prefix, test_numeric_range_finds_every_value_in_range, test_datetime_range_finds_every_value_in_range, test_terms_only_numeric_range, test_terms_only_datetime_range)
