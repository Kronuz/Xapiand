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

// Coverage tests for src/multivalue/range.cc and geospatialrange.cc: the value sources of a range accept a
// document when one of its values (or ranges) falls inside, and rebuild from their serialized form or refuse it;
// a range of the query DSL becomes the source (with the accuracy terms of its type in front) or an error callers
// map to HTTP 400, for every field type.

#include "test_harness.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cartesian.h"
#include "database/schema.h"
#include "datetime_msgpack.h"
#include "engine/error.h"
#include "engine/query.h"
#include "exception_engine.h"
#include "geospatial/circle.h"
#include "geospatial/geometry.h"
#include "htm.h"
#include "length.h"
#include "msgpack.h"
#include "multivalue/generate_terms.h"
#include "multivalue/geospatialrange.h"
#include "multivalue/range.h"
#include "reserved/query_dsl.h"
#include "serialize.h"
#include "serialize_list.h"
#include "utype.hh"

namespace {

using namespace std::string_literals;

std::string multi(const std::vector<std::string>& values) {
	return StringList::serialize(values.begin(), values.end());
}

// A list whose second length is cut short: it does not decode.
// The templated constructors are inlined away in range.cc, so a source is built through unserialize.
std::unique_ptr<engine::PostingSource> make_range(engine::valueno slot, const std::string& from, const std::string& to) {
	return MultipleValueRange().unserialize(multi({ serialize_length(slot), from, to }));
}
std::unique_ptr<engine::PostingSource> make_ge(engine::valueno slot, const std::string& from) {
	return MultipleValueGE().unserialize(multi({ serialize_length(slot), from }));
}
std::unique_ptr<engine::PostingSource> make_le(engine::valueno slot, const std::string& to) {
	return MultipleValueLE().unserialize(multi({ serialize_length(slot), to }));
}

const std::string broken_list = std::string(1, SERIALIZED_LIST_MAGIC) + "\x05" "ab";

void test_range_source_accepts_a_value_inside() {
	auto range_p = make_range(7, "c"s, "f"s);
	auto& range = *range_p;
	CHECK_EQ(range.slot(), 7u);
	CHECK(range.accept("c"));
	CHECK(range.accept("f"));
	CHECK(range.accept("d"));
	CHECK_FALSE(range.accept("b"));
	CHECK_FALSE(range.accept("g"));
	CHECK(range.accept(multi({ "a", "d" })));
	CHECK(range.accept(multi({ "a", "c", "z" })));
	CHECK(range.accept(multi({ "f", "z" })));
	// A list that straddles the range without a value inside it is not accepted.
	CHECK_FALSE(range.accept(multi({ "a", "g" })));
	CHECK_FALSE(range.accept(multi({ "a", "b" })));
	CHECK_FALSE(range.accept(multi({ "g", "h" })));
	CHECK_FALSE(range.accept(""));
	CHECK_FALSE(range.accept(broken_list));
	CHECK_EQ(range.weight("d"), 1.0);
	CHECK_EQ(range.name(), "MultipleValueRange");
	CHECK_EQ(range.description(), "MultipleValueRange 7 c f");
}

void test_ge_source_accepts_a_value_at_or_above() {
	auto ge_p = make_ge(3, "m"s);
	auto& ge = *ge_p;
	CHECK_EQ(ge.slot(), 3u);
	CHECK(ge.accept("m"));
	CHECK(ge.accept("z"));
	CHECK_FALSE(ge.accept("l"));
	CHECK(ge.accept(multi({ "a", "n" })));
	CHECK_FALSE(ge.accept(multi({ "a", "b" })));
	CHECK_FALSE(ge.accept(""));
	CHECK_FALSE(ge.accept(broken_list));
	CHECK_EQ(ge.weight("m"), 1.0);
	CHECK_EQ(ge.name(), "MultipleValueGE");
	CHECK_EQ(ge.description(), "MultipleValueGE 3 m");
}

void test_le_source_accepts_a_value_at_or_below() {
	auto le_p = make_le(4, "m"s);
	auto& le = *le_p;
	CHECK_EQ(le.slot(), 4u);
	CHECK(le.accept("m"));
	CHECK(le.accept("a"));
	CHECK_FALSE(le.accept("n"));
	CHECK(le.accept(multi({ "a", "z" })));
	CHECK_FALSE(le.accept(multi({ "n", "z" })));
	CHECK_FALSE(le.accept(""));
	CHECK_FALSE(le.accept(broken_list));
	CHECK_EQ(le.weight("m"), 1.0);
	CHECK_EQ(le.name(), "MultipleValueLE");
	CHECK_EQ(le.description(), "MultipleValueLE 4 m");
}

template <typename Source>
void check_refused(const Source& prototype, const std::string& bytes, const std::string& message) {
	++testh::checks;
	try {
		prototype.unserialize(bytes);
	} catch (const engine::Error& e) {
		if (e.kind != engine::errc::invalid || e.reason() != message) {
			std::fprintf(stderr, "  FAIL %s:%d: wrong error: %s\n", __FILE__, __LINE__, e.reason().c_str());
			++testh::failures;
		}
		return;
	} catch (...) {
		std::fprintf(stderr, "  FAIL %s:%d: not an engine::Error\n", __FILE__, __LINE__);
		++testh::failures;
		return;
	}
	std::fprintf(stderr, "  FAIL %s:%d: accepted\n", __FILE__, __LINE__);
	++testh::failures;
}

// A source rebuilds from its serialized form with the same slot and bounds, whole or cloned.
void test_sources_round_trip() {
	auto range_p = make_range(7, "c"s, "f"s);
	auto& range = *range_p;
	auto range_back = range.unserialize(range.serialize());
	CHECK_EQ(range_back->description(), range.description());
	CHECK_EQ(range_back->serialize(), range.serialize());
	CHECK_EQ(range.clone()->description(), range.description());
	CHECK(range_back->accept("d"));
	CHECK_FALSE(range_back->accept("g"));

	auto ge_p = make_ge(3, "m"s);
	auto& ge = *ge_p;
	auto ge_back = ge.unserialize(ge.serialize());
	CHECK_EQ(ge_back->description(), "MultipleValueGE 3 m");
	CHECK_EQ(ge.clone()->description(), ge.description());
	CHECK(ge_back->accept("z"));

	auto le_p = make_le(4, "m"s);
	auto& le = *le_p;
	auto le_back = le.unserialize(le.serialize());
	CHECK_EQ(le_back->description(), "MultipleValueLE 4 m");
	CHECK_EQ(le.clone()->description(), le.description());
	CHECK(le_back->accept("a"));

	// The wrong number of parts, and parts that do not decode.
	const std::string one = serialize_length(7);
	check_refused(range, one, "Bad serialized MultipleValueRange");
	check_refused(range, multi({ serialize_length(7), "c" }), "Bad serialized MultipleValueRange");
	check_refused(range, multi({ serialize_length(7), "c", "f", "x" }), "Bad serialized MultipleValueRange");
	check_refused(range, broken_list, "Bad serialized MultipleValueRange");
	check_refused(ge, multi({ serialize_length(3), "m", "x" }), "Bad serialized MultipleValueGE");
	check_refused(ge, broken_list, "Bad serialized MultipleValueGE");
	check_refused(le, multi({ serialize_length(4), "m", "x" }), "Bad serialized MultipleValueLE");
	check_refused(le, broken_list, "Bad serialized MultipleValueLE");
}

required_spc_t spc(engine::valueno slot, FieldType type, std::vector<uint64_t> accuracy = {}, std::vector<std::string> prefix = {}) {
	required_spc_t out(slot, type, std::move(accuracy), std::move(prefix));
	out.flags.partials = true;
	out.error = HTM_MIN_ERROR;
	return out;
}

MsgPack range_of(const MsgPack* from, const MsgPack* to) {
	MsgPack obj = MsgPack::MAP();
	if (from) {
		obj[RESERVED_QUERYDSL_FROM] = *from;
	}
	if (to) {
		obj[RESERVED_QUERYDSL_TO] = *to;
	}
	return obj;
}

std::string described(const engine::Query& query) {
	return query.description();
}

std::string range_source(engine::valueno slot, const std::string& from, const std::string& to) {
	return described(engine::Query::source(make_range(slot, std::string(from), std::string(to))));
}

std::string ge_source(engine::valueno slot, const std::string& from) {
	return described(engine::Query::source(make_ge(slot, std::string(from))));
}

std::string le_source(engine::valueno slot, const std::string& to) {
	return described(engine::Query::source(make_le(slot, std::string(to))));
}

// A source with the terms of the accuracies filtering it.
std::string filtered(std::unique_ptr<engine::PostingSource> source, const engine::Query& terms) {
	return described(engine::Query::combine(engine::Op::FILTER, engine::Query::source(std::move(source)), terms));
}

// A numeric range of a field with a value slot: the source alone when the terms cannot cover the range, else the
// source filtered by the terms of the accuracies; an inverted range matches nothing; no bound matches all.
void test_numeric_range_query() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100 };
	const std::vector<std::string> prefix = { "r0", "r1", "r2" };
	const MsgPack neg(-3), pos(9), big(1000000000LL), small(5), fraction(2.5);
	const auto integer = spc(7, FieldType::integer, accuracy, prefix);

	// [-3, 9]: bounds in the encoding of their own sign, the terms of the range in front.
	const auto terms = GenerateTerms::numeric(int64_t(-3), int64_t(9), accuracy, prefix);
	CHECK_FALSE(terms.empty());
	CHECK_EQ(described(MultipleValueRange::getQuery(integer, range_of(&neg, &pos))),
		filtered(make_range(7, Serialize::integer(-3), Serialize::positive(9)), terms));

	// A range too wide for the terms is the source alone.
	CHECK_EQ(described(MultipleValueRange::getQuery(integer, range_of(&small, &big))),
		range_source(7, Serialize::positive(5), Serialize::positive(1000000000ULL)));

	// An open end is the one-sided source, and an open range filtered by terms is never wider than the source.
	CHECK_EQ(described(MultipleValueRange::getQuery(integer, range_of(&small, nullptr))), ge_source(7, Serialize::positive(5)));
	CHECK_EQ(described(MultipleValueRange::getQuery(integer, range_of(nullptr, &neg))), le_source(7, Serialize::integer(-3)));

	// Inverted: matches nothing. No bounds: everything.
	CHECK(MultipleValueRange::getQuery(integer, range_of(&pos, &neg)).empty());
	CHECK_EQ(described(MultipleValueRange::getQuery(integer, range_of(nullptr, nullptr))), engine::Query::match_all().description());

	// A floating field: the bound keeps its fraction in the slot's bytes and its truncation in the terms.
	const auto floating = spc(8, FieldType::floating, accuracy, prefix);
	const auto fterms = GenerateTerms::numeric(int64_t(2), int64_t(9), accuracy, prefix);
	CHECK_EQ(described(MultipleValueRange::getQuery(floating, range_of(&fraction, &pos))),
		filtered(make_range(8, Serialize::floating(2.5), Serialize::positive(9)), fterms));

	// A positive field.
	const auto positive = spc(9, FieldType::positive, accuracy, prefix);
	const MsgPack six(6);
	const auto pterms = GenerateTerms::numeric(uint64_t(6), uint64_t(9), accuracy, prefix);
	CHECK_EQ(described(MultipleValueRange::getQuery(positive, range_of(&six, &pos))),
		filtered(make_range(9, Serialize::positive(6), Serialize::positive(9)), pterms));

	// A bound that is not a number, or is NaN, is a query error naming the field's type.
	const MsgPack text("abc"), nan(std::nan(""));
	CHECK_THROW(MultipleValueRange::getQuery(integer, range_of(&text, &pos)), QueryParserError);
	CHECK_THROW(MultipleValueRange::getQuery(floating, range_of(&nan, &pos)), QueryParserError);
	std::string what;
	try {
		MultipleValueRange::getQuery(integer, range_of(&text, &pos));
	} catch (const QueryParserError& e) {
		what = e.what();
	}
	CHECK(what.find("Failed to serialize") != std::string::npos);
	CHECK(what.find("integer") != std::string::npos);
}

// A NaN bound selects no well-defined range: a query error, on every numeric and date field type.
void test_nan_bound_is_refused() {
	const MsgPack nan(std::nan("")), pos(9);
	for (auto type : { FieldType::integer, FieldType::positive, FieldType::floating, FieldType::date, FieldType::datetime }) {
		const auto field = spc(3, type);
		CHECK_THROW(MultipleValueRange::getQuery(field, range_of(&nan, &pos)), QueryParserError);
		CHECK_THROW(MultipleValueRange::getQuery(field, range_of(&pos, &nan)), QueryParserError);
	}
}

// Strings: the source over the bounds serialized as the field's type; without a slot there is no source.
void test_string_range_query() {
	const MsgPack a("apple"), z("zebra"), yes(true), no(false);
	for (auto type : { FieldType::keyword, FieldType::text }) {
		const auto field = spc(5, type);
		CHECK_EQ(described(MultipleValueRange::getQuery(field, range_of(&a, &z))),
			range_source(5, Serialize::MsgPack(field, a), Serialize::MsgPack(field, z)));
		// M2: with no upper bound the end serializes empty and sorts below the start, so a lower-bound-only string range is empty.
		CHECK(MultipleValueRange::getQuery(field, range_of(&a, nullptr)).empty());
		CHECK_EQ(described(MultipleValueRange::getQuery(field, range_of(nullptr, &z))), le_source(5, Serialize::MsgPack(field, z)));
		CHECK(MultipleValueRange::getQuery(field, range_of(&z, &a)).empty());
		CHECK(MultipleValueRange::getQuery(spc(engine::BAD_VALUENO, type), range_of(&a, &z)).empty());
	}
	const auto boolean = spc(6, FieldType::boolean);
	CHECK_EQ(described(MultipleValueRange::getQuery(boolean, range_of(&no, &yes))),
		range_source(6, Serialize::boolean(false), Serialize::boolean(true)));
	CHECK(MultipleValueRange::getQuery(boolean, range_of(&yes, &no)).empty());
}

// Dates: a closed range is filtered by the terms of the date accuracies; an open one is the source alone.
void test_date_range_query() {
	const std::vector<uint64_t> accuracy = { toUType(UnitTime::day), toUType(UnitTime::month), toUType(UnitTime::year) };
	const std::vector<std::string> prefix = { "t0", "t1", "t2" };
	const MsgPack from("2020-03-01T00:00:00"), to("2020-03-09T12:00:00");
	const auto start = Datetime::timestamp(Datetime::DatetimeParser(from));
	const auto end = Datetime::timestamp(Datetime::DatetimeParser(to));
	for (auto type : { FieldType::datetime, FieldType::date }) {
		const auto field = spc(4, type, accuracy, prefix);
		CHECK_EQ(described(MultipleValueRange::getQuery(field, range_of(&from, &to))),
			filtered(make_range(4, Serialize::timestamp(start), Serialize::timestamp(end)),
				GenerateTerms::datetime(start, end, accuracy, prefix)));
		CHECK_EQ(described(MultipleValueRange::getQuery(field, range_of(&from, nullptr))), ge_source(4, Serialize::timestamp(start)));
		CHECK_EQ(described(MultipleValueRange::getQuery(field, range_of(nullptr, &to))), le_source(4, Serialize::timestamp(end)));
		CHECK(MultipleValueRange::getQuery(field, range_of(&to, &from)).empty());

		// No accuracy terms fit: the source alone.
		const auto narrow = spc(4, type, { toUType(UnitTime::second) }, { "u0" });
		CHECK_EQ(described(MultipleValueRange::getQuery(narrow, range_of(&from, &to))),
			range_source(4, Serialize::timestamp(start), Serialize::timestamp(end)));
	}
}

// Times and time deltas are seconds: the terms are those of the whole seconds of the bounds.
void test_time_range_query() {
	const std::vector<uint64_t> accuracy = { 1, 10, 100, 1000 };
	const std::vector<std::string> prefix = { "s0", "s1", "s2", "s3" };

	const MsgPack t_from("10:00:00"), t_to("10:30:00");
	const auto ts = Datetime::time_to_double(t_from);
	const auto te = Datetime::time_to_double(t_to);
	const auto time = spc(2, FieldType::time, accuracy, prefix);
	CHECK_EQ(described(MultipleValueRange::getQuery(time, range_of(&t_from, &t_to))),
		filtered(make_range(2, Serialize::timestamp(ts), Serialize::timestamp(te)),
			GenerateTerms::numeric(GenerateTerms::truncate<int64_t>(ts), GenerateTerms::truncate<int64_t>(te), accuracy, prefix)));
	CHECK_EQ(described(MultipleValueRange::getQuery(time, range_of(&t_from, nullptr))), ge_source(2, Serialize::timestamp(ts)));
	CHECK_EQ(described(MultipleValueRange::getQuery(time, range_of(nullptr, &t_to))), le_source(2, Serialize::timestamp(te)));
	CHECK(MultipleValueRange::getQuery(time, range_of(&t_to, &t_from)).empty());

	const MsgPack d_from("+01:30:00"), d_to("+02:00:00");
	const auto ds = Datetime::timedelta_to_double(d_from);
	const auto de = Datetime::timedelta_to_double(d_to);
	const auto delta = spc(3, FieldType::timedelta, accuracy, prefix);
	CHECK_EQ(described(MultipleValueRange::getQuery(delta, range_of(&d_from, &d_to))),
		filtered(make_range(3, Serialize::timestamp(ds), Serialize::timestamp(de)),
			GenerateTerms::numeric(GenerateTerms::truncate<int64_t>(ds), GenerateTerms::truncate<int64_t>(de), accuracy, prefix)));
	CHECK_EQ(described(MultipleValueRange::getQuery(delta, range_of(&d_from, nullptr))), ge_source(3, Serialize::timestamp(ds)));
	CHECK_EQ(described(MultipleValueRange::getQuery(delta, range_of(nullptr, &d_to))), le_source(3, Serialize::timestamp(de)));
	CHECK(MultipleValueRange::getQuery(delta, range_of(&d_to, &d_from)).empty());
}

// A centroid is a unit vector: the distance is the arc cosine of the dot product.
Cartesian at(double lat, double lon) {
	Cartesian point(lat, lon, 0, Cartesian::Units::DEGREES);
	point.normalize();
	return point;
}

// A geo range takes one EWKT bound, either end; two are an error. Other field types have no range: an empty query.
void test_geo_range_query_and_other_types() {
	const std::vector<uint64_t> accuracy = { 3, 5, 8, 10, 12, 15 };
	const std::vector<std::string> prefix = { "g0", "g1", "g2", "g3", "g4", "g5" };
	const auto geo = spc(6, FieldType::geo, accuracy, prefix);
	const MsgPack circle("CIRCLE(-101.192222 19.702778, 1000)"), other("CIRCLE(2.3522 48.8566, 1000)");

	const auto ranges = Circle(at(19.702778, -101.192222), 1000).getRanges(true, HTM_MIN_ERROR);
	const auto centroids = Circle(at(19.702778, -101.192222), 1000).getCentroids();
	const auto expected = filtered(std::make_unique<GeoSpatialRange>(6, ranges, centroids), GenerateTerms::geo(ranges, accuracy, prefix));
	CHECK_EQ(described(MultipleValueRange::getQuery(geo, range_of(&circle, nullptr))), expected);
	CHECK_EQ(described(MultipleValueRange::getQuery(geo, range_of(nullptr, &circle))), expected);
	CHECK_EQ(described(GeoSpatialRange::getQuery(geo, circle)), expected);
	CHECK_THROW(MultipleValueRange::getQuery(geo, range_of(&circle, &other)), QueryParserError);

	// Without a value slot the terms alone answer; without accuracy the source alone does.
	const auto terms_only = spc(engine::BAD_VALUENO, FieldType::geo, accuracy, prefix);
	CHECK_EQ(described(MultipleValueRange::getQuery(terms_only, range_of(&circle, nullptr))), described(GenerateTerms::geo(ranges, accuracy, prefix)));
	const auto no_terms = spc(6, FieldType::geo, { 25 }, { "gx" });
	const auto wide = GeoSpatialRange::getQuery(no_terms, MsgPack("CIRCLE(-101.192222 19.702778, 1000000)"));
	CHECK_FALSE(wide.empty());

	// A shape that is not geometry is a geospatial error.
	const MsgPack shapeless("not a shape");
	CHECK_THROW(MultipleValueRange::getQuery(geo, range_of(&shapeless, nullptr)), QueryParserError);

	// A type with no range gives nothing.
	const MsgPack one(1), two(2);
	CHECK(MultipleValueRange::getQuery(spc(1, FieldType::object), range_of(&one, &two)).empty());
	CHECK(MultipleValueRange::getQuery(spc(1, FieldType::script), range_of(&one, &two)).empty());
}

// A lone range serializes without the list magic, so an id below 2^48 starts with a NUL and decodes as a list: the ids
// here are HTM-sized.
range_t hr(std::uint64_t start, std::uint64_t end) {
	constexpr std::uint64_t base = std::uint64_t(0x10) << 48;
	return range_t(base + start, base + end);
}

// A geo source accepts a document when one of its ranges overlaps one of the search's; its weight is pi less the
// smallest angle between their centroids.
void test_geospatial_source() {
	const std::vector<range_t> search = { hr(10, 20), hr(40, 50) };
	const std::vector<Cartesian> centroids = { at(0, 0) };
	GeoSpatialRange source(7, search, centroids);
	CHECK_EQ(source.slot(), 7u);
	CHECK_EQ(source.name(), "GeoSpatialRange");
	CHECK_EQ(source.description(), "GeoSpatialRange 7");
	CHECK_EQ(source.max_weight(), M_PI);

	auto value = [](const std::vector<range_t>& ranges, const std::vector<Cartesian>& cs) { return Serialize::ranges_centroids(ranges, cs); };
	CHECK(source.accept(value({ hr(15, 16) }, {})));
	CHECK(source.accept(value({ hr(50, 60) }, {})));
	CHECK(source.accept(value({ hr(5, 10) }, {})));
	CHECK(source.accept(value({ hr(25, 30), hr(45, 46) }, {})));
	CHECK_FALSE(source.accept(value({ hr(21, 39) }, {})));
	CHECK_FALSE(source.accept(value({ hr(5, 9) }, {})));
	CHECK_FALSE(source.accept(value({ hr(51, 60) }, {})));
	CHECK_FALSE(source.accept(value({ hr(25, 30), hr(60, 70) }, {})));
	CHECK_FALSE(source.accept(value({}, {})));
	CHECK_FALSE(source.accept("\xff\xff"));

	// The weight is pi minus the closest centroid's angle: 10 degrees to the north.
	const auto weight = source.weight(value({ hr(15, 16) }, { at(30, 0), at(10, 0) }));
	CHECK_LT(std::fabs(weight - (M_PI - M_PI / 18)), 1e-2);
	// A document without centroids, or that does not decode, weighs nothing; so does a source without centroids.
	CHECK_EQ(source.weight(value({ hr(15, 16) }, {})), 0.0);
	CHECK_EQ(source.weight("\xff\xff"), 0.0);
	GeoSpatialRange bare(7, search, std::vector<Cartesian>{});
	CHECK_EQ(bare.weight(value({ hr(15, 16) }, { at(10, 0) })), 0.0);

	// Rebuilds from its serialized form.
	auto back = source.unserialize(source.serialize());
	CHECK_EQ(back->description(), "GeoSpatialRange 7");
	CHECK_EQ(back->serialize(), source.serialize());
	CHECK(back->accept(value({ hr(15, 16) }, {})));
	CHECK_FALSE(back->accept(value({ hr(21, 39) }, {})));
	CHECK_EQ(source.clone()->serialize(), source.serialize());
	CHECK_LT(std::fabs(back->weight(value({ hr(15, 16) }, { at(10, 0) })) - (M_PI - M_PI / 18)), 1e-2);

	check_refused(source, serialize_length(7), "Bad serialized GeoSpatialRange");
	check_refused(source, multi({ serialize_length(7), "x", "y" }), "Bad serialized GeoSpatialRange");
	check_refused(source, broken_list, "Bad serialized GeoSpatialRange");
	check_refused(source, multi({ serialize_length(7), "\xff\xff" }), "Bad serialized GeoSpatialRange");
}

}  // namespace

TEST_MAIN(
	test_range_source_accepts_a_value_inside,
	test_ge_source_accepts_a_value_at_or_above,
	test_le_source_accepts_a_value_at_or_below,
	test_sources_round_trip,
	test_numeric_range_query,
	test_nan_bound_is_refused,
	test_string_range_query,
	test_date_range_query,
	test_time_range_query,
	test_geo_range_query_and_other_types,
	test_geospatial_source
)
