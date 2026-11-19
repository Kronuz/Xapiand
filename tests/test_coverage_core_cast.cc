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


// Coverage tests for src/cast.cc: each cast yields the value of the type it names, the numeric casts truncate a float
// and refuse a value out of range, text is read as a number only when it holds nothing but the number, and the cast
// words map to their field types. A cast of the wrong shape is a CastError.

#include "test_coverage_core.h"

#include <cstdint>
#include <limits>
#include <string>

#include "cast.h"
#include "database/schema.h"
#include "exception_engine.h"
#include "msgpack.h"

namespace {

constexpr int64_t I64_MIN = std::numeric_limits<int64_t>::min();
constexpr int64_t I64_MAX = std::numeric_limits<int64_t>::max();
constexpr uint64_t U64_MAX = std::numeric_limits<uint64_t>::max();

// An integer of the NEGATIVE_INTEGER type; zero is the only value of it that is not negative.
MsgPack neg(int64_t v) {
	return v == 0 ? MsgPack::NEGATIVE_INTEGER() : MsgPack(v);
}

MsgPack cast_word(const char* word, const MsgPack& value) {
	return MsgPack({ { word, value } });
}

}  // namespace

static void test_bare_number_text() {
	CHECK_TRUE(Cast::bare_number_text("1"));
	CHECK_TRUE(Cast::bare_number_text("-1.5e3"));
	CHECK_FALSE(Cast::bare_number_text(""));
	CHECK_FALSE(Cast::bare_number_text(" 1"));
	CHECK_FALSE(Cast::bare_number_text("1 "));
	CHECK_FALSE(Cast::bare_number_text("\t1"));
	CHECK_FALSE(Cast::bare_number_text("1\n"));
	CHECK_TRUE(Cast::bare_number_text("1 2"));  // only the two ends are checked; strict_stox refuses the rest
}

static void test_integer() {
	SEE(Cast::integer(MsgPack(uint64_t(7))), int64_t(7));
	SEE(Cast::integer(MsgPack(uint64_t(I64_MAX))), I64_MAX);
	CHECK_THROWS_WITH(Cast::integer(MsgPack(uint64_t(I64_MAX) + 1)), CastError, "out of range for integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack(U64_MAX)), CastError, "out of range for integer");
	SEE(Cast::integer(neg(-7)), int64_t(-7));
	SEE(Cast::integer(neg(I64_MIN)), I64_MIN);
	// A float truncates toward zero.
	SEE(Cast::integer(MsgPack(1.9)), int64_t(1));
	SEE(Cast::integer(MsgPack(-1.9)), int64_t(-1));
	SEE(Cast::integer(MsgPack(0.5)), int64_t(0));
	SEE(Cast::integer(MsgPack(-0x1p63)), I64_MIN);
	CHECK_THROWS_WITH(Cast::integer(MsgPack(0x1p63)), CastError, "out of range for integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack(-0x1.1p63)), CastError, "out of range for integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack(std::numeric_limits<double>::quiet_NaN())), CastError, "out of range for integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack(std::numeric_limits<double>::infinity())), CastError, "out of range for integer");
	// Text is a number only when it holds nothing but the number.
	SEE(Cast::integer(MsgPack("42")), int64_t(42));
	SEE(Cast::integer(MsgPack("-42")), int64_t(-42));
	CHECK_THROWS_WITH(Cast::integer(MsgPack(" 42")), CastError, "cannot be cast to integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack("42 ")), CastError, "cannot be cast to integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack("4x")), CastError, "cannot be cast to integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack("")), CastError, "cannot be cast to integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack("9223372036854775808")), CastError, "cannot be cast to integer");
	SEE(Cast::integer(MsgPack(true)), int64_t(1));
	SEE(Cast::integer(MsgPack(false)), int64_t(0));
	CHECK_THROWS_WITH(Cast::integer(MsgPack(nullptr)), CastError, "cannot be cast to integer");
	CHECK_THROWS_WITH(Cast::integer(MsgPack::ARRAY()), CastError, "cannot be cast to integer");
}

static void test_positive() {
	SEE(Cast::positive(MsgPack(uint64_t(7))), uint64_t(7));
	SEE(Cast::positive(MsgPack(U64_MAX)), U64_MAX);
	SEE(Cast::positive(neg(0)), uint64_t(0));  // a NEGATIVE_INTEGER of either sign
	CHECK_THROWS_WITH(Cast::positive(neg(-1)), CastError, "out of range for positive");
	SEE(Cast::positive(MsgPack(1.9)), uint64_t(1));
	SEE(Cast::positive(MsgPack(-0.5)), uint64_t(0));  // truncates to zero before the range check
	SEE(Cast::positive(MsgPack(1.8e19)), uint64_t(18000000000000000000ULL));
	CHECK_THROWS_WITH(Cast::positive(MsgPack(-1.0)), CastError, "out of range for positive");
	CHECK_THROWS_WITH(Cast::positive(MsgPack(0x1p64)), CastError, "out of range for positive");
	CHECK_THROWS_WITH(Cast::positive(MsgPack(std::numeric_limits<double>::quiet_NaN())), CastError, "out of range for positive");
	SEE(Cast::positive(MsgPack("42")), uint64_t(42));
	SEE(Cast::positive(MsgPack("18446744073709551615")), U64_MAX);
	CHECK_THROWS_WITH(Cast::positive(MsgPack("-1")), CastError, "cannot be cast to positive");  // strtoull would wrap it
	CHECK_THROWS_WITH(Cast::positive(MsgPack(" 1")), CastError, "cannot be cast to positive");
	CHECK_THROWS_WITH(Cast::positive(MsgPack("x")), CastError, "cannot be cast to positive");
	CHECK_THROWS_WITH(Cast::positive(MsgPack("18446744073709551616")), CastError, "cannot be cast to positive");
	SEE(Cast::positive(MsgPack(true)), uint64_t(1));
	SEE(Cast::positive(MsgPack(false)), uint64_t(0));
	CHECK_THROWS_WITH(Cast::positive(MsgPack(nullptr)), CastError, "cannot be cast to positive");
}

static void test_floating() {
	SEE(Cast::floating(MsgPack(uint64_t(3))), 3.0);
	SEE(Cast::floating(neg(-3)), -3.0);
	SEE(Cast::floating(MsgPack(2.5)), 2.5);
	SEE(Cast::floating(MsgPack("2.5")), 2.5);
	SEE(Cast::floating(MsgPack("-1e3")), -1000.0);
	CHECK_THROWS_WITH(Cast::floating(MsgPack(" 2.5")), CastError, "cannot be cast to float");
	CHECK_THROWS_WITH(Cast::floating(MsgPack("2.5 ")), CastError, "cannot be cast to float");
	CHECK_THROWS_WITH(Cast::floating(MsgPack("x")), CastError, "cannot be cast to float");
	CHECK_THROWS_WITH(Cast::floating(MsgPack("")), CastError, "cannot be cast to float");
	SEE(Cast::floating(MsgPack(true)), 1.0);
	SEE(Cast::floating(MsgPack(false)), 0.0);
	CHECK_THROWS_WITH(Cast::floating(MsgPack(nullptr)), CastError, "cannot be cast to float");
	CHECK_THROWS_WITH(Cast::floating(MsgPack::MAP()), CastError, "cannot be cast to float");
}

static void test_string() {
	SEE(Cast::string(MsgPack(uint64_t(18446744073709551615ULL))), std::string("18446744073709551615"));
	SEE(Cast::string(neg(-12)), std::string("-12"));
	SEE(Cast::string(MsgPack(2.5)), std::string("2.5"));
	SEE(Cast::string(MsgPack("text")), std::string("text"));
	SEE(Cast::string(MsgPack(true)), std::string("true"));
	SEE(Cast::string(MsgPack(false)), std::string("false"));
	// Anything else is its JSON.
	SEE(Cast::string(MsgPack(nullptr)), std::string("null"));
	SEE(Cast::string(MsgPack({ { "a", 1 } })), MsgPack({ { "a", 1 } }).to_string());
}

static void test_boolean() {
	SEE(Cast::boolean(MsgPack(uint64_t(0))), false);
	SEE(Cast::boolean(MsgPack(uint64_t(9))), true);
	SEE(Cast::boolean(neg(0)), false);
	SEE(Cast::boolean(neg(-1)), true);
	SEE(Cast::boolean(MsgPack(0.0)), false);
	SEE(Cast::boolean(MsgPack(0.25)), true);
	SEE(Cast::boolean(MsgPack(true)), true);
	SEE(Cast::boolean(MsgPack(false)), false);
	// Text is false for "", "0", "f", "F" and "false" in any case; any other text is true.
	for (const char* falsy : { "", "0", "f", "F", "false", "False", "FALSE", "fAlSe" }) {
		SEE(Cast::boolean(MsgPack(falsy)), false);
	}
	for (const char* truthy : { "1", "t", "T", "true", "yes", "no", "00", "fals", "falsey", "Falsy", "tr", "x" }) {
		SEE(Cast::boolean(MsgPack(truthy)), true);
	}
	CHECK_THROWS_WITH(Cast::boolean(MsgPack(nullptr)), CastError, "cannot be cast to boolean");
	CHECK_THROWS_WITH(Cast::boolean(MsgPack::ARRAY()), CastError, "cannot be cast to boolean");
}

static void test_uuid_and_geo_text() {
	SEE(Cast::uuid(MsgPack("a2d6e3b4-0000-1000-8000-010000000000")), std::string("a2d6e3b4-0000-1000-8000-010000000000"));
	CHECK_THROWS_WITH(Cast::uuid(MsgPack(uint64_t(1))), CastError, "cannot be cast to uuid");
	SEE(Cast::ewkt(MsgPack("POINT(1 2)")), std::string("POINT(1 2)"));
	CHECK_THROWS_WITH(Cast::ewkt(MsgPack::MAP()), CastError, "cannot be cast to ewkt");
}

static void test_date_time_timedelta() {
	// Each passes a number, text (and, for a datetime, an object) through as it is.
	for (const auto& v : { MsgPack(uint64_t(5)), neg(-5), MsgPack(1.5), MsgPack("2020-01-02") }) {
		SEE(Cast::datetime(v).to_string(), v.to_string());
		SEE(Cast::time(v).to_string(), v.to_string());
		SEE(Cast::timedelta(v).to_string(), v.to_string());
	}
	const MsgPack obj({ { "_year", 2020 } });
	SEE(Cast::datetime(obj).to_string(), obj.to_string());
	CHECK_THROWS_WITH(Cast::time(obj), CastError, "cannot be cast to time");
	CHECK_THROWS_WITH(Cast::timedelta(obj), CastError, "cannot be cast to timedelta");
	CHECK_THROWS_WITH(Cast::datetime(MsgPack(true)), CastError, "cannot be cast to datetime");
	CHECK_THROWS_WITH(Cast::time(MsgPack(nullptr)), CastError, "cannot be cast to time");
	CHECK_THROWS_WITH(Cast::timedelta(MsgPack::ARRAY()), CastError, "cannot be cast to timedelta");
}

static void test_hash_type_and_field_type() {
	struct Row { const char* word; FieldType type; };
	const Row rows[] = {
		{ "_integer", FieldType::integer }, { "_positive", FieldType::positive }, { "_float", FieldType::floating },
		{ "_boolean", FieldType::boolean }, { "_keyword", FieldType::keyword }, { "_text", FieldType::text },
		{ "_uuid", FieldType::uuid }, { "_datetime", FieldType::datetime }, { "_time", FieldType::time },
		{ "_timedelta", FieldType::timedelta }, { "_ewkt", FieldType::geo }, { "_point", FieldType::geo },
		{ "_circle", FieldType::geo }, { "_convex", FieldType::geo }, { "_polygon", FieldType::geo },
		{ "_chull", FieldType::geo }, { "_multipoint", FieldType::geo }, { "_multicircle", FieldType::geo },
		{ "_multipolygon", FieldType::geo }, { "_multichull", FieldType::geo }, { "_geometrycollection", FieldType::geo },
		{ "_geometryintersection", FieldType::geo }, { "_lua", FieldType::script },
	};
	for (const auto& row : rows) {
		CHECK_TRUE(Cast::get_field_type(row.word) == row.type);
	}
	CHECK_TRUE(Cast::get_hash_type("_integer") == Cast::HashType::INTEGER);
	CHECK_TRUE(Cast::get_hash_type("_date") == Cast::HashType::DATE);
	CHECK_TRUE(Cast::get_hash_type("_lua") == Cast::HashType::LUA);
	CHECK_TRUE(Cast::get_hash_type("_multiconvex") == Cast::HashType::MULTICONVEX);
	CHECK_THROWS_WITH(Cast::get_field_type(""), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::get_field_type("integer"), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::get_field_type("_nonsense"), CastError, "Unknown cast type");
	// Product bug (cast-words), fixed by quest serialize-validation (D130, in review; changes src/cast.cc): _date and
	// _multiconvex are cast words but get_field_type has no row for them, so both are refused. Whichever quest lands
	// second flips these two lines.
	CHECK_THROWS_WITH(Cast::get_field_type("_date"), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::get_field_type("_multiconvex"), CastError, "Unknown cast type");
}

static void test_cast_map() {
	SEE(Cast::cast(cast_word("_integer", MsgPack("12"))).i64(), int64_t(12));
	SEE(Cast::cast(cast_word("_positive", MsgPack(12.9))).u64(), uint64_t(12));
	SEE(Cast::cast(cast_word("_float", MsgPack("2.5"))).f64(), 2.5);
	SEE(Cast::cast(cast_word("_boolean", MsgPack("F"))).boolean(), false);
	SEE(Cast::cast(cast_word("_keyword", MsgPack(5))).str(), std::string("5"));
	SEE(Cast::cast(cast_word("_text", MsgPack(true))).str(), std::string("true"));
	SEE(Cast::cast(cast_word("_uuid", MsgPack("u"))).str(), std::string("u"));
	SEE(Cast::cast(cast_word("_date", MsgPack("2020-01-02"))).str(), std::string("2020-01-02"));
	SEE(Cast::cast(cast_word("_datetime", MsgPack(5))).u64(), uint64_t(5));
	SEE(Cast::cast(cast_word("_time", MsgPack("10:00"))).str(), std::string("10:00"));
	SEE(Cast::cast(cast_word("_timedelta", MsgPack("+1:00"))).str(), std::string("+1:00"));
	SEE(Cast::cast(cast_word("_ewkt", MsgPack("POINT(1 2)"))).str(), std::string("POINT(1 2)"));
	// A geometry is returned whole, with its cast word.
	for (const char* geo : { "_point", "_circle", "_convex", "_polygon", "_chull", "_multipoint", "_multicircle", "_multipolygon",
	                         "_multichull", "_geometrycollection", "_geometryintersection" }) {
		const auto m = cast_word(geo, MsgPack({ { "_latitude", 1 } }));
		SEE(Cast::cast(m).to_string(), m.to_string());
	}
	// A comment key is skipped; the first other key is the cast.
	SEE(Cast::cast(MsgPack({ { "#note", "x" }, { "_integer", "3" } })).i64(), int64_t(3));
	CHECK_THROWS_WITH(Cast::cast(cast_word("_nonsense", MsgPack(1))), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::cast(MsgPack({ { "plain", 1 } })), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::cast(MsgPack::MAP()), CastError, "Expected map with one valid cast");
	CHECK_THROWS_WITH(Cast::cast(MsgPack({ { "#only", "comment" } })), CastError, "Expected map with one valid cast");
	// The words that have a hash but no case: _multiconvex and _lua are not casts.
	CHECK_THROWS_WITH(Cast::cast(cast_word("_multiconvex", MsgPack(1))), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::cast(cast_word("_lua", MsgPack(1))), CastError, "Unknown cast type");
	CHECK_THROWS_WITH(Cast::cast(cast_word("_integer", MsgPack("x"))), CastError, "cannot be cast to integer");
}

static void test_cast_by_field_type() {
	SEE(Cast::cast(FieldType::integer, MsgPack("-3")).i64(), int64_t(-3));
	SEE(Cast::cast(FieldType::positive, MsgPack("3")).u64(), uint64_t(3));
	SEE(Cast::cast(FieldType::floating, MsgPack(3)).f64(), 3.0);
	SEE(Cast::cast(FieldType::boolean, MsgPack(0)).boolean(), false);
	SEE(Cast::cast(FieldType::keyword, MsgPack(3)).str(), std::string("3"));
	SEE(Cast::cast(FieldType::text, MsgPack(2.5)).str(), std::string("2.5"));
	SEE(Cast::cast(FieldType::uuid, MsgPack("u")).str(), std::string("u"));
	SEE(Cast::cast(FieldType::date, MsgPack("2020-01-02")).str(), std::string("2020-01-02"));
	SEE(Cast::cast(FieldType::datetime, MsgPack(1.5)).f64(), 1.5);
	SEE(Cast::cast(FieldType::time, MsgPack(5)).u64(), uint64_t(5));
	SEE(Cast::cast(FieldType::timedelta, MsgPack(-5)).i64(), int64_t(-5));
	CHECK_THROWS_WITH(Cast::cast(FieldType::integer, MsgPack(nullptr)), CastError, "cannot be cast to integer");
	// A script is an object; geo is an object or text.
	const MsgPack script({ { "_lua", "return 1" } });
	SEE(Cast::cast(FieldType::script, script).to_string(), script.to_string());
	CHECK_THROWS_WITH(Cast::cast(FieldType::script, MsgPack("x")), CastError, "cannot be cast to script");
	SEE(Cast::cast(FieldType::geo, script).to_string(), script.to_string());
	SEE(Cast::cast(FieldType::geo, MsgPack("POINT(1 2)")).str(), std::string("POINT(1 2)"));
	CHECK_THROWS_WITH(Cast::cast(FieldType::geo, MsgPack(1)), CastError, "cannot be cast to geo");
	// A field of no type yet reads text as the first number type that holds it.
	SEE(Cast::cast(FieldType::empty, MsgPack("-7")).i64(), int64_t(-7));
	SEE(Cast::cast(FieldType::empty, MsgPack("7")).i64(), int64_t(7));
	SEE(Cast::cast(FieldType::empty, MsgPack("18446744073709551615")).u64(), U64_MAX);
	SEE(Cast::cast(FieldType::empty, MsgPack("2.5")).f64(), 2.5);
	SEE(Cast::cast(FieldType::empty, MsgPack("2.5x")).str(), std::string("2.5x"));
	SEE(Cast::cast(FieldType::empty, MsgPack("word")).str(), std::string("word"));
	CHECK_THROWS_WITH(Cast::cast(FieldType::empty, MsgPack(5)), CastError, "cannot be cast");
	CHECK_THROWS_WITH(Cast::cast(FieldType::object, MsgPack("x")), CastError, "cannot be cast");
	CHECK_THROWS_WITH(Cast::cast(FieldType::array, MsgPack::ARRAY()), CastError, "cannot be cast");
}

TEST_MAIN(test_bare_number_text, test_integer, test_positive, test_floating, test_string, test_boolean, test_uuid_and_geo_text,
          test_date_time_timedelta, test_hash_type_and_field_type, test_cast_map, test_cast_by_field_type)
