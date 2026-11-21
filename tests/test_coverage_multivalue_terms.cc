/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Coverage tests for src/multivalue/generate_terms.cc: the accuracy terms a value is indexed under (checked by
// searching a shard for each expected term), and the queries that find a range of values through those terms
// (their exact structure for numeric ranges, and, for datetime and geo ranges, the documents found against an
// oracle that truncates by hand).

#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "analysis.hh"
#include "cartesian.h"
#include "database/schema.h"
#include "database/utils.h"
#include "datetime.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/query.h"
#include "engine/sync.h"
#include "engine_support.h"
#include "geospatial/circle.h"
#include "geospatial/geometry.h"
#include "geospatial/point.h"
#include "htm.h"
#include "multivalue/generate_terms.h"
#include "serialize.h"
#include "utype.hh"

namespace {

using Docs = std::set<engine::docid>;

std::time_t gm(Datetime::tm_t t) { return Datetime::timegm(t); }

std::string integer_field(const std::string& prefix) { return field_name(prefix, ctype_integer); }
std::string date_field(const std::string& prefix) { return field_name(prefix, ctype_date); }
std::string geo_field(const std::string& prefix) { return field_name(prefix, ctype_geo); }

Docs hits(engine::Shard& shard, const engine::Query& query) {
	const auto ids = testsupport::hit_ids(shard, query);
	return Docs(ids.begin(), ids.end());
}

Docs hits_of(engine::Shard& shard, const std::string& field, const std::string& term) {
	return hits(shard, engine::Query::term(field, term));
}

int keys = 0;

engine::docid add(engine::Shard& shard, engine::Document& doc) {
	return testsupport::add_keyed(shard, doc, "K" + std::to_string(++keys));
}

void commit(engine::Shard& shard) {
	engine::sync_wait(shard.commit(engine::Transaction::none));
}

// The query a list of field/term pairs makes, as the numeric and datetime ranges build it.
engine::Query any_of(const std::vector<std::pair<std::string, std::string>>& terms) {
	std::vector<engine::Query> queries;
	for (const auto& [field, term] : terms) {
		queries.push_back(engine::Query::term(field, term));
	}
	return engine::Query::combine(engine::Op::OR, std::move(queries));
}

// An integer value is indexed under the start of its bucket at each accuracy, the floor for a negative value, with
// the field's prefix and, in the second form, the global prefix too.
void test_integer_terms() {
	const std::vector<uint64_t> accuracy = { 10, 100 };
	const std::vector<std::string> prefix = { "ia", "ib" };
	const std::vector<std::string> global = { "ga", "gb" };
	auto shard = testsupport::scratch_shard("multivalue_integer");

	engine::Document own, both, negative, zero, above;
	GenerateTerms::integer(own, accuracy, prefix, 1234);
	GenerateTerms::integer(both, accuracy, prefix, global, 1239);
	GenerateTerms::integer(negative, accuracy, prefix, global, -1234);
	GenerateTerms::integer(zero, accuracy, prefix, 0);
	GenerateTerms::integer(above, accuracy, prefix, 1301);
	add(*shard, own);
	add(*shard, both);
	add(*shard, negative);
	add(*shard, zero);
	add(*shard, above);
	commit(*shard);

	auto at = [&](const std::string& prefix_, int64_t bucket) { return hits_of(*shard, integer_field(prefix_), Serialize::integer(bucket)); };
	CHECK(at("ia", 1230) == Docs({ 1, 2 }));
	CHECK(at("ib", 1200) == Docs({ 1, 2 }));
	CHECK(at("ia", 1240).empty());
	CHECK(at("ib", 1300) == Docs({ 5 }));
	CHECK(at("ia", 1300) == Docs({ 5 }));
	CHECK(at("ia", -1240) == Docs({ 3 }));
	CHECK(at("ib", -1300) == Docs({ 3 }));
	CHECK(at("ia", 0) == Docs({ 4 }));
	CHECK(at("ib", 0) == Docs({ 4 }));
	// The global fields hold the terms of the documents that gave a global prefix, and only theirs.
	CHECK(at("ga", 1230) == Docs({ 2 }));
	CHECK(at("gb", 1200) == Docs({ 2 }));
	CHECK(at("ga", -1240) == Docs({ 3 }));
	CHECK(at("gb", -1300) == Docs({ 3 }));
	CHECK(at("ga", 1300).empty());
}

void test_positive_terms() {
	const std::vector<uint64_t> accuracy = { 10, 1000 };
	const std::vector<std::string> prefix = { "pa", "pb" };
	const std::vector<std::string> global = { "qa", "qb" };
	auto shard = testsupport::scratch_shard("multivalue_positive");

	engine::Document own, both;
	GenerateTerms::positive(own, accuracy, prefix, 12345u);
	GenerateTerms::positive(both, accuracy, prefix, global, 12999u);
	add(*shard, own);
	add(*shard, both);
	commit(*shard);

	auto at = [&](const std::string& prefix_, uint64_t bucket) { return hits_of(*shard, integer_field(prefix_), Serialize::positive(bucket)); };
	CHECK(at("pa", 12340) == Docs({ 1 }));
	CHECK(at("pa", 12990) == Docs({ 2 }));
	CHECK(at("pb", 12000) == Docs({ 1, 2 }));
	CHECK(at("qa", 12990) == Docs({ 2 }));
	CHECK(at("qb", 12000) == Docs({ 2 }));
	CHECK(at("qa", 12340).empty());
}

// The units of time, from the finest.
const std::vector<UnitTime> units = {
	UnitTime::second, UnitTime::minute, UnitTime::hour, UnitTime::day, UnitTime::month,
	UnitTime::year, UnitTime::decade, UnitTime::century, UnitTime::millennium,
};

std::vector<uint64_t> accuracy_of(const std::vector<UnitTime>& list) {
	std::vector<uint64_t> accuracy;
	for (auto unit : list) {
		accuracy.push_back(toUType(unit));
	}
	return accuracy;
}

std::vector<std::string> prefixes(const std::string& stem, std::size_t n) {
	std::vector<std::string> out;
	for (std::size_t i = 0; i < n; ++i) {
		out.push_back(stem + std::to_string(i));
	}
	return out;
}

// 2023-07-19 13:45:56 UTC is indexed under the start of its second, minute, hour, day, month, year, decade,
// century and millennium: the epoch seconds of each start below.
void test_datetime_terms() {
	const std::vector<std::pair<UnitTime, double>> starts = {
		{ UnitTime::second, 1689774356 },  // 2023-07-19 13:45:56
		{ UnitTime::minute, 1689774300 },  // 2023-07-19 13:45:00
		{ UnitTime::hour, 1689771600 },    // 2023-07-19 13:00:00
		{ UnitTime::day, 1689724800 },     // 2023-07-19
		{ UnitTime::month, 1688169600 },   // 2023-07-01
		{ UnitTime::year, 1672531200 },    // 2023-01-01
		{ UnitTime::decade, 1577836800 },  // 2020-01-01
		{ UnitTime::century, 946684800 },  // 2000-01-01
		{ UnitTime::millennium, 946684800 },
	};
	const Datetime::tm_t tm(2023, 7, 19, 13, 45, 56);
	const auto accuracy = accuracy_of(units);
	const auto prefix = prefixes("da", units.size());
	const auto global = prefixes("dg", units.size());
	auto shard = testsupport::scratch_shard("multivalue_datetime");

	engine::Document own, both, other;
	GenerateTerms::datetime(own, accuracy, prefix, tm);
	GenerateTerms::datetime(both, accuracy, prefix, global, tm);
	GenerateTerms::datetime(other, accuracy, prefix, Datetime::tm_t(2023, 7, 19, 13, 45, 57));
	add(*shard, own);
	add(*shard, both);
	add(*shard, other);
	commit(*shard);

	for (std::size_t i = 0; i < starts.size(); ++i) {
		CHECK(static_cast<UnitTime>(accuracy[i]) == starts[i].first);
		const auto term = Serialize::timestamp(starts[i].second);
		// The next second differs from this one only at the second.
		CHECK(hits_of(*shard, date_field(prefix[i]), term) == (i == 0 ? Docs({ 1, 2 }) : Docs({ 1, 2, 3 })));
		CHECK(hits_of(*shard, date_field(global[i]), term) == Docs({ 2 }));
	}
	CHECK(hits_of(*shard, date_field(prefix[0]), Serialize::timestamp(1689774357.0)) == Docs({ 3 }));

	// An accuracy that is no unit adds no term, and the others keep theirs.
	engine::Document invalid, invalid_global;
	const std::vector<uint64_t> odd = { toUType(UnitTime::INVALID), toUType(UnitTime::day) };
	GenerateTerms::datetime(invalid, odd, { "xa", "xb" }, tm);
	GenerateTerms::datetime(invalid_global, odd, { "ya", "yb" }, { "za", "zb" }, tm);
	add(*shard, invalid);
	add(*shard, invalid_global);
	commit(*shard);
	CHECK(hits_of(*shard, date_field("xb"), Serialize::timestamp(1689724800.0)) == Docs({ 4 }));
	CHECK(hits_of(*shard, date_field("yb"), Serialize::timestamp(1689724800.0)) == Docs({ 5 }));
	CHECK(hits_of(*shard, date_field("zb"), Serialize::timestamp(1689724800.0)) == Docs({ 5 }));
	CHECK(hits_of(*shard, date_field("xa"), Serialize::timestamp(1689724800.0)).empty());
	CHECK(hits_of(*shard, date_field("za"), Serialize::timestamp(1689724800.0)).empty());
}

// A year of the datetime is positive: truncating to an accuracy that reaches year 0 gives the accuracy itself.
void test_year_truncation() {
	CHECK_EQ(GenerateTerms::year(2023, 1000), 2000);
	CHECK_EQ(GenerateTerms::year(2023, 100), 2000);
	CHECK_EQ(GenerateTerms::year(2023, 10), 2020);
	CHECK_EQ(GenerateTerms::year(2020, 10), 2020);
	CHECK_EQ(GenerateTerms::year(5, 1000), 1000);
	CHECK_EQ(GenerateTerms::year(999, 1000), 1000);
	CHECK_EQ(GenerateTerms::year(5, 10), 10);
	CHECK_EQ(GenerateTerms::year(0, 10), 10);
	CHECK_EQ(GenerateTerms::year(-35, 10), 10);

	// The same clamp in the generated terms: year 5 is indexed under the start of year 1000 at the millennium.
	engine::Document doc;
	auto shard = testsupport::scratch_shard("multivalue_year_clamp");
	GenerateTerms::datetime(doc, { toUType(UnitTime::millennium) }, { "ma" }, Datetime::tm_t(5, 3, 4));
	add(*shard, doc);
	commit(*shard);
	CHECK(hits_of(*shard, date_field("ma"), Serialize::timestamp(gm(Datetime::tm_t(1000)))) == Docs({ 1 }));
}

// A truncated double: toward zero, and saturating at the limits of the type.
void test_truncate() {
	CHECK_EQ(GenerateTerms::truncate<int64_t>(2.9), 2);
	CHECK_EQ(GenerateTerms::truncate<int64_t>(-2.9), -2);
	CHECK_EQ(GenerateTerms::truncate<int64_t>(1e30), INT64_MAX);
	CHECK_EQ(GenerateTerms::truncate<int64_t>(-1e30), INT64_MIN);
	CHECK_EQ(GenerateTerms::truncate<int64_t>(9223372036854775808.0), INT64_MAX);
	CHECK_EQ(GenerateTerms::truncate<int64_t>(-9223372036854775808.0), INT64_MIN);
	CHECK_EQ(GenerateTerms::truncate<uint64_t>(-5.0), uint64_t(0));
	CHECK_EQ(GenerateTerms::truncate<uint64_t>(7.5), uint64_t(7));
	CHECK_EQ(GenerateTerms::truncate<uint64_t>(1e30), UINT64_MAX);
}

// The query of a numeric range: whole buckets of the coarsest accuracy, then what is left of each end in finer
// ones, the finest widened to the buckets it touches.
void test_numeric_query_structure() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000 };
	const auto prefix = prefixes("n", accuracy.size());
	auto term = [&](std::size_t level, int64_t start) { return std::make_pair(integer_field(prefix[level]), Serialize::integer(start)); };

	// [1000, 2999] is two buckets of 1000.
	CHECK_EQ(GenerateTerms::numeric(int64_t(1000), int64_t(2999), accuracy, prefix).description(),
		any_of({ term(3, 1000), term(3, 2000) }).description());

	// [1005, 1034]: two buckets of 10 in the middle, the ends widened to single values.
	std::vector<std::pair<std::string, std::string>> expected = { term(1, 1010), term(1, 1020) };
	for (int64_t v : { 1005, 1006, 1007, 1008, 1009, 1030, 1031, 1032, 1033, 1034 }) {
		expected.push_back(term(0, v));
	}
	CHECK_EQ(GenerateTerms::numeric(int64_t(1005), int64_t(1034), accuracy, prefix).description(), any_of(expected).description());

	// The same range of positive values, with the terms under the positive encoding of the bucket starts.
	auto positive = [&](std::size_t level, uint64_t start) { return std::make_pair(integer_field(prefix[level]), Serialize::positive(start)); };
	CHECK_EQ(GenerateTerms::numeric(uint64_t(1000), uint64_t(2999), accuracy, prefix).description(),
		any_of({ positive(3, 1000), positive(3, 2000) }).description());

	// A negative range: [-20, -1] is the two buckets of 10 that start at -20 and -10.
	CHECK_EQ(GenerateTerms::numeric(int64_t(-20), int64_t(-1), accuracy, prefix).description(),
		any_of({ term(1, -20), term(1, -10) }).description());

	// An inverted range matches nothing; no accuracy matches everything; a range wider than the budget of the
	// coarsest accuracy has no terms (the value source alone decides).
	CHECK(GenerateTerms::numeric(int64_t(5), int64_t(1), accuracy, prefix).empty());
	CHECK(GenerateTerms::numeric(uint64_t(5), uint64_t(1), accuracy, prefix).empty());
	CHECK_EQ(GenerateTerms::numeric(int64_t(1), int64_t(5), {}, {}).description(), engine::Query::match_all().description());
	CHECK(GenerateTerms::numeric(int64_t(0), int64_t(1000000), accuracy, prefix).empty());
	CHECK_FALSE(GenerateTerms::numeric(int64_t(0), int64_t(999999), accuracy, prefix, 1, 1000).empty());
	CHECK(GenerateTerms::numeric(int64_t(0), int64_t(999999), accuracy, prefix, 1, 999).empty());
}

// The tuple of a datetime truncated to a unit of time: two datetimes in the same bucket have the same one.
std::tuple<int, int, int, int, int, int> bucket(const Datetime::tm_t& tm, UnitTime unit) {
	auto y = [&](int accuracy) { return GenerateTerms::year(tm.year, accuracy); };
	switch (unit) {
		case UnitTime::millennium: return { y(1000), 1, 1, 0, 0, 0 };
		case UnitTime::century: return { y(100), 1, 1, 0, 0, 0 };
		case UnitTime::decade: return { y(10), 1, 1, 0, 0, 0 };
		case UnitTime::year: return { tm.year, 1, 1, 0, 0, 0 };
		case UnitTime::month: return { tm.year, tm.mon, 1, 0, 0, 0 };
		case UnitTime::day: return { tm.year, tm.mon, tm.day, 0, 0, 0 };
		case UnitTime::hour: return { tm.year, tm.mon, tm.day, tm.hour, 0, 0 };
		case UnitTime::minute: return { tm.year, tm.mon, tm.day, tm.hour, tm.min, 0 };
		default: return { tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec };
	}
}

// The unit a range needs: the coarsest at which its ends differ.
UnitTime needed_unit(const Datetime::tm_t& s, const Datetime::tm_t& e) {
	const int diff = e.year - s.year;
	if (diff >= 1000) { return UnitTime::millennium; }
	if (diff >= 100) { return UnitTime::century; }
	if (diff >= 10) { return UnitTime::decade; }
	if (diff != 0) { return UnitTime::year; }
	if (e.mon != s.mon) { return UnitTime::month; }
	if (e.day != s.day) { return UnitTime::day; }
	if (e.hour != s.hour) { return UnitTime::hour; }
	if (e.min != s.min) { return UnitTime::minute; }
	return UnitTime::second;
}

const std::vector<Datetime::tm_t> anchors = {
	{ 2023, 7, 19, 13, 45, 56 }, { 2023, 7, 19, 13, 45, 57 }, { 2023, 7, 19, 13, 46, 0 }, { 2023, 7, 19, 14, 0, 0 },
	{ 2023, 7, 20, 0, 0, 0 }, { 2023, 7, 31, 23, 59, 59 }, { 2023, 8, 1, 0, 0, 0 }, { 2023, 12, 31, 23, 59, 59 },
	{ 2024, 1, 1, 0, 0, 0 }, { 2024, 2, 29, 12, 0, 0 }, { 2026, 5, 5, 5, 5, 5 }, { 2029, 12, 31, 23, 59, 59 },
	{ 2030, 1, 1, 0, 0, 0 }, { 2035, 6, 15, 0, 0, 0 }, { 2099, 12, 31, 23, 59, 59 }, { 2100, 1, 1, 0, 0, 0 },
	{ 2150, 1, 1, 0, 0, 0 }, { 2999, 12, 31, 23, 59, 59 }, { 3000, 1, 1, 0, 0, 0 }, { 3500, 6, 1, 0, 0, 0 },
};

// A datetime range finds the documents whose date shares a bucket, at the unit the range needs, with a bucket
// between its ends' (and only those): every pair of anchors, against documents at each anchor.
void test_datetime_query_finds_the_buckets() {
	const auto accuracy = accuracy_of(units);
	const auto prefix = prefixes("dq", units.size());
	auto shard = testsupport::scratch_shard("multivalue_datetime_query");
	for (const auto& tm : anchors) {
		engine::Document doc;
		GenerateTerms::datetime(doc, accuracy, prefix, tm);
		add(*shard, doc);
	}
	commit(*shard);

	for (std::size_t i = 0; i < anchors.size(); ++i) {
		for (std::size_t j = i; j < anchors.size(); ++j) {
			const auto start = static_cast<double>(gm(anchors[i]));
			const auto end = static_cast<double>(gm(anchors[j]));
			const auto unit = needed_unit(anchors[i], anchors[j]);
			Docs expected;
			for (std::size_t k = 0; k < anchors.size(); ++k) {
				if (bucket(anchors[i], unit) <= bucket(anchors[k], unit) && bucket(anchors[k], unit) <= bucket(anchors[j], unit)) {
					expected.insert(static_cast<engine::docid>(k + 1));
				}
			}
			const auto query = GenerateTerms::datetime(start, end, accuracy, prefix);
			CHECK(hits(*shard, query) == expected);
		}
	}
}

// The structure of one range: 2020-03-01 to 2020-03-09 12:00 needs days, in the month's bucket: the end first,
// then each day from the start, and the month's term in an AND.
void test_datetime_query_structure() {
	const std::vector<UnitTime> list = { UnitTime::hour, UnitTime::day, UnitTime::month, UnitTime::year };
	const auto accuracy = accuracy_of(list);
	const auto prefix = prefixes("ds", list.size());
	auto day = [&](int d) { return std::make_pair(date_field(prefix[1]), Serialize::timestamp(gm(Datetime::tm_t(2020, 3, d)))); };

	const auto start = static_cast<double>(gm(Datetime::tm_t(2020, 3, 1)));
	const auto end = static_cast<double>(gm(Datetime::tm_t(2020, 3, 9, 12)));
	std::vector<std::pair<std::string, std::string>> days = { day(9) };
	for (int d = 1; d <= 8; ++d) {
		days.push_back(day(d));
	}
	const auto month = any_of({ { date_field(prefix[2]), Serialize::timestamp(gm(Datetime::tm_t(2020, 3, 1))) } });
	CHECK_EQ(GenerateTerms::datetime(start, end, accuracy, prefix).description(),
		engine::Query::combine(engine::Op::AND, month, any_of(days)).description());

	// An accuracy list without the needed unit keeps the coarser one: the month's term only.
	const auto coarse = accuracy_of({ UnitTime::month, UnitTime::year });
	CHECK_EQ(GenerateTerms::datetime(start, end, coarse, { "c0", "c1" }).description(),
		any_of({ { date_field("c0"), Serialize::timestamp(gm(Datetime::tm_t(2020, 3, 1))) } }).description());

	// Only finer ones than the range needs: no term filters it.
	CHECK(GenerateTerms::datetime(start, end, accuracy_of({ UnitTime::hour }), { "f0" }).empty());

	// An inverted range matches nothing; no accuracy matches everything.
	CHECK(GenerateTerms::datetime(end, start, accuracy, prefix).empty());
	CHECK_EQ(GenerateTerms::datetime(start, end, {}, {}).description(), engine::Query::match_all().description());
}

// A date's seconds are its floor: half a second before a boundary is in the earlier bucket, before 1970 too.
void test_datetime_query_floors_the_seconds() {
	const auto accuracy = accuracy_of({ UnitTime::day });
	const std::vector<std::string> prefix = { "fl" };
	const auto dec31 = Serialize::timestamp(gm(Datetime::tm_t(1969, 12, 31)));
	const auto jan1 = Serialize::timestamp(gm(Datetime::tm_t(1970, 1, 1)));
	CHECK_EQ(GenerateTerms::datetime(-0.5, -0.5, accuracy, prefix).description(), any_of({ { date_field("fl"), dec31 } }).description());
	CHECK_EQ(GenerateTerms::datetime(0.0, 0.5, accuracy, prefix).description(), any_of({ { date_field("fl"), jan1 } }).description());
}

std::vector<range_t> cover(const Geometry& geometry) {
	return geometry.getRanges(true, HTM_MIN_ERROR);
}

Cartesian at(double lat, double lon) {
	return Cartesian(lat, lon, 0, Cartesian::Units::DEGREES);
}

// The stop words are read from <data path>/stopwords: the source tree, found from the working directory (the source
// root for the coverage harness, the build directory under ctest).
void use_source_stop_words() {
	for (const char* root : { ".", ".." }) {
		if (std::filesystem::is_directory(std::filesystem::path(root) / "stopwords")) {
			::setenv("XAPIAND_DATA_PATH", std::filesystem::absolute(root).c_str(), 1);
			return;
		}
	}
}

// A point is indexed under the cell of each accuracy that holds it, and a geo range finds the points whose cells
// its cover touches: one around the point finds it, one around another place does not. The cover is a superset,
// so a point well inside a large circle is found too.
void test_geo_terms_and_query() {
	const std::vector<uint64_t> accuracy = { 3, 5, 8, 10, 12, 15 };
	const auto prefix = prefixes("gp", accuracy.size());
	const auto global = prefixes("gg", accuracy.size());
	auto shard = testsupport::scratch_shard("multivalue_geo");

	const auto morelia = at(19.702778, -101.192222);
	const auto close_by = at(19.7037, -101.1912);
	const auto east = at(19.70, -101.0);
	const auto paris = at(48.8566, 2.3522);
	engine::Document d_morelia, d_close, d_east, d_paris;
	GenerateTerms::geo(d_morelia, accuracy, prefix, cover(Point(morelia)));
	GenerateTerms::geo(d_close, accuracy, prefix, global, cover(Point(close_by)));
	GenerateTerms::geo(d_east, accuracy, prefix, cover(Point(east)));
	GenerateTerms::geo(d_paris, accuracy, prefix, cover(Point(paris)));
	add(*shard, d_morelia);
	add(*shard, d_close);
	add(*shard, d_east);
	add(*shard, d_paris);
	commit(*shard);

	auto find = [&](double meters, const std::vector<std::string>& by) {
		return hits(*shard, GenerateTerms::geo(cover(Circle(morelia, meters)), accuracy, by));
	};
	const auto near = find(1000, prefix);
	CHECK(near.count(1) == 1);
	CHECK(near.count(2) == 1);
	CHECK(near.count(4) == 0);
	// 20 km away is outside a 1 km circle's cover and inside a 50 km one's.
	CHECK(near.count(3) == 0);
	const auto wide = find(50000, prefix);
	CHECK(wide.count(1) == 1);
	CHECK(wide.count(2) == 1);
	CHECK(wide.count(3) == 1);
	CHECK(wide.count(4) == 0);
	// The global prefixes are indexed and looked up in the same accuracy order, as the field's own are: the list that
	// indexed the document finds it, the reversed list does not.
	CHECK(find(1000, global) == Docs({ 2 }));
	CHECK(find(1000, std::vector<std::string>(global.rbegin(), global.rend())).empty());

	// Nothing to cover is no query; no accuracy prefix at all is every document.
	CHECK(GenerateTerms::geo({}, accuracy, prefix).empty());
	CHECK_EQ(GenerateTerms::geo(cover(Circle(morelia, 1000)), accuracy, {}).description(), engine::Query::match_all().description());
	// A cover of cells larger than the coarsest accuracy has no terms.
	CHECK(GenerateTerms::geo(cover(Circle(morelia, 1000000)), { 25 }, { "gx" }).empty());
}

// Text is split into words by the stop list of its language, and an unknown stem language is the engine's invalid
// error naming it.
void test_text_stop_words_and_languages() {
	use_source_stop_words();
	auto shard = testsupport::scratch_shard("multivalue_text");
	engine::Document kept, dropped;
	GenerateTerms::text(kept, "tx", "the quick fox", false, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none);
	GenerateTerms::text(dropped, "tx", "the quick fox", false, 1, false, "en", "", analysis::stop_strategy::all, analysis::stem_strategy::none);
	add(*shard, kept);
	add(*shard, dropped);
	commit(*shard);
	CHECK(hits_of(*shard, "tx", "the") == Docs({ 1 }));
	CHECK(hits_of(*shard, "tx", "quick") == Docs({ 1, 2 }));
	CHECK(hits_of(*shard, "tx", "fox") == Docs({ 1, 2 }));

	engine::Document doc;
	try {
		GenerateTerms::text(doc, "tx", "the quick fox", false, 1, false, "", "klingon", analysis::stop_strategy::none, analysis::stem_strategy::all);
		CHECK(false);
	} catch (const engine::Error& e) {
		CHECK(e.kind == engine::errc::invalid);
		CHECK_EQ(e.reason(), "Language code klingon unknown");
	}
}

}  // namespace

TEST_MAIN(
	test_integer_terms,
	test_positive_terms,
	test_datetime_terms,
	test_year_truncation,
	test_truncate,
	test_numeric_query_structure,
	test_datetime_query_finds_the_buckets,
	test_datetime_query_structure,
	test_datetime_query_floors_the_seconds,
	test_geo_terms_and_query,
	test_text_stop_words_and_languages
)
