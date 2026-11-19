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


// Coverage tests for src/serialize.cc: every serializer meets every value kind it accepts and each it refuses, the
// numeric, date and time conversions agree with their unserializers, the predicates read what they should, and the
// cast objects ({"_integer": ...}) pick their types. Findings pinned as they behave today are named in comments.

#include "test_harness.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "database/schema.h"
#include "database/utils.h"
#include "datetime.h"
#include "datetime_msgpack.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "rapidjson/document.h"
#include "serialize.h"
#include "serialize_list.h"
#include "xapiand_exception.h"

namespace {

constexpr auto I64_MAX = std::numeric_limits<int64_t>::max();
constexpr auto U64_MAX = std::numeric_limits<uint64_t>::max();

required_spc_t spc(FieldType type) {
	required_spc_t s;
	s.set_type(type);
	return s;
}

MsgPack js(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

MsgPack str(const char* s) { return MsgPack(std::string(s)); }
MsgPack num(int64_t v) { return MsgPack(v); }
MsgPack unum(uint64_t v) { return MsgPack(v); }
MsgPack flt(double v) { return MsgPack(v); }

constexpr auto V4 = "3c0f2be3-ff4f-40ab-b157-c51a81eff176";
constexpr auto V4B = "e47fcfdf-8db6-4469-a97f-57146dc41ced";
constexpr auto V6 = "2280898d-9810-6800-88b4-038db356b8d1";   // condensed and compacted: its encoded form is "~..."

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

// --- predicates -----------------------------------------------------------------------------------------------

void test_text_and_uuid_predicates() {
	CHECK(Serialize::isText(""));
	CHECK(Serialize::isText("hello world"));
	CHECK(!Serialize::isText("hello_world"));
	CHECK(!Serialize::isText(std::string(127, 'a')));
	CHECK(Serialize::isText(std::string(128, 'a')));

	const std::string bare = V4;
	const std::string braced = "{" + bare + "}";
	const std::string urn = "urn:uuid:" + bare;
	const std::string list = bare + ";" + V4B;
	const std::string holes = bare + ";;" + V4B;
	for (const auto& ok : { bare, braced, urn, list, holes, std::string("{;}") }) {
		CHECK(Serialize::isUUID(ok));
		CHECK(Serialize::possiblyUUID(ok));
	}
	for (const auto& bad : { std::string(), std::string("ab"), std::string("abc"), bare.substr(0, 35), bare.substr(0, 35) + "x",
			bare + ";abc", braced.substr(0, braced.size() - 1) }) {
		CHECK(!Serialize::isUUID(bad));
		CHECK(!Serialize::possiblyUUID(bad));
	}
	// An encoded UUID is "~" and base59; the codec decides whether a "~" word is one.
	const auto encoded = Unserialize::uuid(Serialize::uuid(V6), UUIDRepr::encoded);
	REQUIRE(!encoded.empty() && encoded[0] == '~');
	CHECK(Serialize::isUUID(encoded));
	CHECK(Serialize::possiblyUUID(encoded));
	CHECK(Serialize::isUUID(encoded + ";" + V4));
	CHECK(!Serialize::isUUID("~!!!!!!!!"));
	CHECK(!Serialize::possiblyUUID("~!!!!!!!!"));
	CHECK(!Serialize::isUUID("~0"));
	CHECK(!Serialize::possiblyUUID("~0"));
}

// --- exact: a value meets a typed field --------------------------------------------------------------------------

void test_exact_numbers() {
	const auto exact = [](FieldType type, const MsgPack& v) { return Serialize::exact("f", type, v); };
	// A type that is not checked takes the value as it is.
	CHECK_EQ(exact(FieldType::boolean, MsgPack(true)).boolean(), true);
	CHECK_EQ(exact(FieldType::geo, str("anything")).str(), std::string("anything"));

	// integer
	CHECK_EQ(exact(FieldType::integer, num(-5)).i64(), -5);
	CHECK_EQ(exact(FieldType::integer, num(5)).i64(), 5);
	CHECK_EQ(exact(FieldType::integer, unum(I64_MAX)).u64(), static_cast<uint64_t>(I64_MAX));
	CHECK_THROW(exact(FieldType::integer, unum(static_cast<uint64_t>(I64_MAX) + 1)), ClientError);
	CHECK_EQ(exact(FieldType::integer, flt(7.0)).i64(), 7);
	CHECK_EQ(exact(FieldType::integer, flt(-7.0)).i64(), -7);
	CHECK_THROW(exact(FieldType::integer, flt(7.5)), ClientError);
	CHECK_THROW(exact(FieldType::integer, flt(std::nan(""))), ClientError);
	CHECK_THROW(exact(FieldType::integer, flt(1e300)), ClientError);
	CHECK_THROW(exact(FieldType::integer, flt(0x1p53)), ClientError);   // from 2^53 on a double is no longer exact
	CHECK_EQ(exact(FieldType::integer, str("12")).i64(), 12);
	CHECK_EQ(exact(FieldType::integer, str("-12")).i64(), -12);
	CHECK_EQ(exact(FieldType::integer, str("1e2")).i64(), 100);
	CHECK_THROW(exact(FieldType::integer, str("1.5")), ClientError);
	CHECK_THROW(exact(FieldType::integer, str("abc")), ClientError);
	CHECK_THROW(exact(FieldType::integer, str("18446744073709551615")), ClientError);
	CHECK_THROW(exact(FieldType::integer, MsgPack(true)), ClientError);

	// positive
	CHECK_THROW(exact(FieldType::positive, num(-5)), ClientError);
	CHECK_EQ(exact(FieldType::positive, num(5)).u64(), 5u);
	CHECK_EQ(exact(FieldType::positive, unum(U64_MAX)).u64(), U64_MAX);
	CHECK_EQ(exact(FieldType::positive, flt(7.0)).u64(), 7u);
	CHECK_THROW(exact(FieldType::positive, flt(-7.0)), ClientError);
	CHECK_EQ(exact(FieldType::positive, str("18446744073709551615")).u64(), U64_MAX);
	CHECK_THROW(exact(FieldType::positive, str("-1")), ClientError);

	// floating keeps every number
	CHECK_EQ(exact(FieldType::floating, num(-5)).i64(), -5);
	CHECK(exact(FieldType::floating, flt(2.5)).f64() == 2.5);
	CHECK(exact(FieldType::floating, str("2.5")).f64() == 2.5);
	CHECK(exact(FieldType::floating, str("-18446744073709551616")).f64() == -18446744073709551616.0);
	CHECK(exact(FieldType::floating, str("1e300")).f64() == 1e300);
	CHECK_THROW(exact(FieldType::floating, str("1e400")), ClientError);
	CHECK_THROW(exact(FieldType::floating, flt(std::nan(""))), ClientError);
	// Text is a number only as JSON spells one.
	for (const char* bad : { "", "-", "01", "1.", ".5", "1e", "1e+", "+1", " 1", "1 ", "0x10", "1,5", "nan" }) {
		CHECK_THROW(exact(FieldType::floating, str(bad)), ClientError);
	}
	for (const char* good : { "0", "-0", "10", "0.5", "1E3", "1e-3", "1.5e+3" }) {
		CHECK_EQ(exact(FieldType::floating, str(good)).get_type() != MsgPack::Type::STR, true);
	}
}

void test_exact_strings_and_casts() {
	const auto exact = [](FieldType type, const MsgPack& v) { return Serialize::exact("f", type, v); };
	for (auto type : { FieldType::keyword, FieldType::text, FieldType::uuid }) {
		CHECK_EQ(exact(type, str("abc")).str(), std::string("abc"));
		CHECK_THROW(exact(type, num(5)), ClientError);
		CHECK_THROW(exact(type, MsgPack(true)), ClientError);
	}
	// A cast object converts on request.
	CHECK_EQ(exact(FieldType::integer, js(R"({"_integer":"5"})")).i64(), 5);
	CHECK_EQ(exact(FieldType::integer, js(R"({"_integer":5.9})")).i64(), 5);
	CHECK(exact(FieldType::floating, js(R"({"_float":"2.5"})")).f64() == 2.5);
	CHECK_EQ(exact(FieldType::keyword, js(R"({"_keyword":"k"})")).str(), std::string("k"));
	CHECK_EQ(exact(FieldType::text, js(R"({"_text":"t"})")).str(), std::string("t"));
	// A cast that cannot be made, or one that does not fit the type, is refused with the field's name.
	CHECK_THROW(exact(FieldType::integer, js(R"({"_integer":"x"})")), ClientError);
	CHECK_THROW(exact(FieldType::integer, js(R"({"x":1})")), ClientError);
	CHECK_THROW(exact(FieldType::integer, js(R"({"_text":"5"})")), ClientError);
	try {
		(void)exact(FieldType::integer, js(R"({"_integer":"x"})"));
		CHECK(false);
	} catch (const ClientError& e) {
		const std::string message = e.get_message();
		CHECK(message.find("'f'") != std::string::npos || message.find("\"f\"") != std::string::npos);
	}
}

// --- serializers by field type ------------------------------------------------------------------------------------

void test_numbers_by_type() {
	// integer(type, v)
	CHECK_EQ(Serialize::integer(FieldType::integer, -7), Serialize::integer(int64_t(-7)));
	CHECK_EQ(Serialize::integer(FieldType::floating, -7), Serialize::integer(int64_t(-7)));
	CHECK_EQ(Serialize::integer(FieldType::positive, 7), Serialize::positive(uint64_t(7)));
	CHECK_THROW(Serialize::integer(FieldType::positive, -7), SerializationError);
	CHECK_EQ(Serialize::integer(FieldType::datetime, 86400), Serialize::timestamp(86400.0));
	CHECK_EQ(Serialize::integer(FieldType::date, 86400), Serialize::timestamp(86400.0));
	CHECK_EQ(Serialize::integer(FieldType::time, 3600), Serialize::time(3600.0));
	CHECK_EQ(Serialize::integer(FieldType::timedelta, -3600), Serialize::timedelta(-3600.0));
	CHECK_THROW(Serialize::integer(FieldType::keyword, 1), SerializationError);
	CHECK_THROW(Serialize::integer(FieldType::boolean, 1), SerializationError);

	// positive(type, v)
	CHECK_EQ(Serialize::positive(FieldType::positive, U64_MAX), Serialize::positive(U64_MAX));
	CHECK_EQ(Serialize::positive(FieldType::floating, 7), Serialize::positive(uint64_t(7)));
	CHECK_EQ(Serialize::positive(FieldType::integer, 7), Serialize::integer(int64_t(7)));
	CHECK_THROW(Serialize::positive(FieldType::integer, static_cast<uint64_t>(I64_MAX) + 1), SerializationError);
	CHECK_EQ(Serialize::positive(FieldType::datetime, 86400), Serialize::timestamp(86400.0));
	CHECK_EQ(Serialize::positive(FieldType::date, 86400), Serialize::timestamp(86400.0));
	CHECK_EQ(Serialize::positive(FieldType::time, 3600), Serialize::time(3600.0));
	CHECK_EQ(Serialize::positive(FieldType::timedelta, 3600), Serialize::timedelta(3600.0));
	CHECK_THROW(Serialize::positive(FieldType::text, 1), SerializationError);

	// floating(type, v)
	CHECK_EQ(Serialize::floating(FieldType::floating, 2.5), Serialize::floating(2.5));
	CHECK_EQ(Serialize::floating(FieldType::datetime, 86400.5), Serialize::timestamp(86400.5));
	CHECK_EQ(Serialize::floating(FieldType::date, 86400.5), Serialize::timestamp(86400.5));
	CHECK_EQ(Serialize::floating(FieldType::time, 3600.5), Serialize::time(3600.5));
	CHECK_EQ(Serialize::floating(FieldType::timedelta, -3600.5), Serialize::timedelta(-3600.5));
	CHECK_THROW(Serialize::floating(FieldType::integer, 2.5), SerializationError);
	CHECK_THROW(Serialize::floating(FieldType::floating, std::nan("")), SerializationError);

	// boolean and geospatial refuse every other type
	CHECK_EQ(Serialize::boolean(FieldType::boolean, true), std::string("t"));
	CHECK_EQ(Serialize::boolean(FieldType::boolean, false), std::string("f"));
	CHECK_THROW(Serialize::boolean(FieldType::integer, true), SerializationError);
	CHECK_THROW(Serialize::geospatial(FieldType::keyword, str("POINT(1 2)")), SerializationError);

	// The values the encoders were told come back.
	CHECK_EQ(Unserialize::integer(Serialize::integer(FieldType::integer, -7)), -7);
	CHECK_EQ(Unserialize::positive(Serialize::positive(FieldType::positive, U64_MAX)), U64_MAX);
	CHECK(Unserialize::floating(Serialize::floating(FieldType::floating, -2.5)) == -2.5);
	CHECK(Unserialize::timestamp(Serialize::floating(FieldType::datetime, 86400.5)) == 86400.5);
}

void test_time_and_timedelta_ranges() {
	// time(double): 0 up to just under 724780 s; timedelta(double): +-362440 s.
	CHECK_EQ(Unserialize::time_d(Serialize::time(3600.25)), 3600.25);
	CHECK_EQ(Unserialize::timedelta_d(Serialize::timedelta(-3600.25)), -3600.25);
	CHECK_THROW(Serialize::time(1e9), SerializationError);
	CHECK_THROW(Serialize::time(-1.0), SerializationError);
	CHECK_THROW(Serialize::timedelta(1e9), SerializationError);
	CHECK_THROW(Serialize::timedelta(-1e9), SerializationError);
	// A stored value out of range is refused when read back as a time or timedelta.
	CHECK_THROW(Unserialize::time_d(Serialize::floating(1e9)), SerializationError);
	CHECK_THROW(Unserialize::timedelta_d(Serialize::floating(1e9)), SerializationError);
	CHECK_EQ(Unserialize::time_d(Serialize::floating(3600.0)), 3600.0);
	CHECK_EQ(Unserialize::timedelta_d(Serialize::floating(-3600.0)), -3600.0);

	// The (MsgPack, double&) forms report the value they encoded.
	double t = 0;
	CHECK_EQ(Serialize::time(unum(3600), t), Serialize::time(3600.0));
	CHECK_EQ(t, 3600.0);
	CHECK_EQ(Serialize::time(num(7200), t), Serialize::time(7200.0));
	CHECK_EQ(t, 7200.0);
	CHECK_EQ(Serialize::time(flt(7200.5), t), Serialize::time(7200.5));
	CHECK_EQ(t, 7200.5);
	CHECK_EQ(Serialize::time(str("10:30:00"), t), Serialize::timestamp(37800.0));
	CHECK_EQ(t, 37800.0);
	CHECK_THROW(Serialize::time(MsgPack(true), t), SerializationError);
	CHECK_THROW(Serialize::time(unum(2000000), t), SerializationError);

	CHECK_EQ(Serialize::timedelta(unum(3600), t), Serialize::timedelta(3600.0));
	CHECK_EQ(t, 3600.0);
	CHECK_EQ(Serialize::timedelta(num(-7200), t), Serialize::timedelta(-7200.0));
	CHECK_EQ(t, -7200.0);
	CHECK_EQ(Serialize::timedelta(flt(-7200.5), t), Serialize::timedelta(-7200.5));
	CHECK_EQ(t, -7200.5);
	CHECK_EQ(Serialize::timedelta(str("-01:30:00"), t), Serialize::timestamp(-5400.0));
	CHECK_EQ(t, -5400.0);
	CHECK_THROW(Serialize::timedelta(MsgPack(true), t), SerializationError);
	CHECK_THROW(Serialize::timedelta(unum(2000000), t), SerializationError);

	// The MsgPack and string forms.
	CHECK_EQ(Serialize::time(num(3600)), Serialize::timestamp(3600.0));
	CHECK_EQ(Serialize::time(str("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::time(std::string_view("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::timedelta(num(-3600)), Serialize::timestamp(-3600.0));
	CHECK_EQ(Serialize::timedelta(str("+01:30:00")), Serialize::timestamp(5400.0));
	CHECK_EQ(Serialize::timedelta(std::string_view("+01:30:00")), Serialize::timestamp(5400.0));
	CHECK_EQ(Unserialize::time(Serialize::time(std::string_view("10:30:00"))), std::string("10:30:00"));
	CHECK_EQ(Serialize::time(std::string_view(Unserialize::time(Serialize::time(std::string_view("10:30:00"))))), Serialize::time(std::string_view("10:30:00")));
	CHECK_EQ(Serialize::timedelta(Unserialize::timedelta(Serialize::timedelta(std::string_view("-01:30:00")))), Serialize::timedelta(std::string_view("-01:30:00")));
}

void test_datetime_forms() {
	const auto stamp = Serialize::datetime(std::string_view("2015-10-10T23:03:03.000"));
	CHECK_EQ(Serialize::datetime(str("2015-10-10T23:03:03.000")), stamp);
	CHECK_EQ(Serialize::datetime(std::string("2015-10-10T23:03:03.000")), stamp);
	CHECK_EQ(Unserialize::datetime(stamp), std::string("2015-10-10T23:03:03"));
	Datetime::tm_t tm;
	CHECK_EQ(Serialize::datetime(str("2015-10-10T23:03:03.000"), tm), stamp);
	CHECK_EQ(tm.year, 2015);
	CHECK_EQ(tm.mon, 10);
	CHECK_EQ(tm.day, 10);
	CHECK_EQ(Serialize::datetime(tm), stamp);
	CHECK_EQ(Serialize::datetime(js(R"({"_year":2015,"_month":10,"_day":10})")), Serialize::datetime(std::string_view("2015-10-10")));
	CHECK_EQ(Serialize::datetime(num(86400)), Serialize::timestamp(86400.0));
}

// The datetime serializer by field type, for each value kind.
void test_datetime_by_spec() {
	const auto day = Serialize::timestamp(86400.0);
	for (auto type : { FieldType::date, FieldType::datetime }) {
		CHECK_EQ(Serialize::datetime(spc(type), unum(86400)), day);
		CHECK_EQ(Serialize::datetime(spc(type), num(86400)), day);
		CHECK_EQ(Serialize::datetime(spc(type), flt(86400.0)), day);
		CHECK_EQ(Serialize::datetime(spc(type), str("1970-01-02")), day);
	}
	CHECK_EQ(Serialize::datetime(spc(FieldType::floating), unum(86400)), Serialize::positive(uint64_t(86400)));
	CHECK_EQ(Serialize::datetime(spc(FieldType::time), str("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::datetime(spc(FieldType::timedelta), str("+01:00:00")), Serialize::timestamp(3600.0));
	CHECK_THROW(Serialize::datetime(spc(FieldType::keyword), unum(1)), SerializationError);
	CHECK_THROW(Serialize::datetime(spc(FieldType::datetime), MsgPack(true)), SerializationError);
	CHECK_THROW(Serialize::datetime(spc(FieldType::datetime), MsgPack::ARRAY()), SerializationError);

	// A map is a datetime to its own parser; each type takes it as it can.
	const auto map = js(R"({"_year":1970,"_month":1,"_day":2})");
	const double seconds = Datetime::timestamp(Datetime::DatetimeParser(map));
	CHECK_EQ(seconds, 86400.0);
	CHECK_EQ(Serialize::datetime(spc(FieldType::floating), map), Serialize::floating(seconds));
	CHECK_EQ(Serialize::datetime(spc(FieldType::datetime), map), day);
	CHECK_EQ(Serialize::datetime(spc(FieldType::date), map), day);
	CHECK_EQ(Serialize::datetime(spc(FieldType::text), map), Datetime::iso8601(Datetime::DatetimeParser(map)));
	CHECK_THROW(Serialize::datetime(spc(FieldType::time), map), SerializationError);        // 86400 s is not a time of day
	CHECK_THROW(Serialize::datetime(spc(FieldType::timedelta), map), SerializationError);   // nor a timedelta
	CHECK_THROW(Serialize::datetime(spc(FieldType::boolean), map), SerializationError);
	const auto early = js(R"({"_year":1970,"_month":1,"_day":1,"_hour":1,"_min":30})");
	CHECK_EQ(Serialize::datetime(spc(FieldType::time), early), Serialize::time(5400.0));
	CHECK_EQ(Serialize::datetime(spc(FieldType::timedelta), early), Serialize::timedelta(5400.0));
}

void test_time_and_timedelta_by_spec() {
	for (auto fn : { 0, 1 }) {
		const auto go = [&](FieldType type, const MsgPack& v) {
			return fn == 0 ? Serialize::time(spc(type), v) : Serialize::timedelta(spc(type), v);
		};
		const auto at = [&](double seconds) { return fn == 0 ? Serialize::time(seconds) : Serialize::timedelta(seconds); };
		const auto type = fn == 0 ? FieldType::time : FieldType::timedelta;
		CHECK_EQ(go(type, unum(3600)), at(3600.0));
		CHECK_EQ(go(type, num(3600)), at(3600.0));
		CHECK_EQ(go(type, flt(3600.5)), at(3600.5));
		CHECK_EQ(go(FieldType::floating, flt(3600.5)), Serialize::floating(3600.5));
		CHECK_EQ(go(FieldType::keyword, str("abc")), std::string("abc"));
		CHECK_THROW(go(type, MsgPack(true)), SerializationError);
		CHECK_THROW(go(type, MsgPack::ARRAY()), SerializationError);
		CHECK_THROW(go(type, js(R"({"_hour":1})")), SerializationError);
	}
	CHECK_EQ(Serialize::time(spc(FieldType::time), str("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::timedelta(spc(FieldType::timedelta), str("-01:30:00")), Serialize::timestamp(-5400.0));
}

// --- text values ---------------------------------------------------------------------------------------------------

void test_numbers_from_text() {
	CHECK_EQ(Serialize::integer(std::string_view("-42")), Serialize::integer(int64_t(-42)));
	CHECK_EQ(Serialize::positive(std::string_view("42")), Serialize::positive(uint64_t(42)));
	CHECK_EQ(Serialize::floating(std::string_view("-2.5")), Serialize::floating(-2.5));
	CHECK_THROW(Serialize::integer(std::string_view("4x")), SerializationError);
	CHECK_THROW(Serialize::integer(std::string_view("")), SerializationError);
	CHECK_THROW(Serialize::integer(std::string_view("99999999999999999999")), SerializationError);
	CHECK_THROW(Serialize::positive(std::string_view("-1")), SerializationError);
	CHECK_THROW(Serialize::positive(std::string_view("4x")), SerializationError);
	CHECK_THROW(Serialize::positive(std::string_view("99999999999999999999")), SerializationError);
	CHECK_THROW(Serialize::floating(std::string_view("abc")), SerializationError);
	CHECK_THROW(Serialize::floating(std::string_view("1e999")), SerializationError);
	CHECK_THROW(Serialize::floating(std::string_view("nan")), SerializationError);
}

void test_boolean_text() {
	for (const char* yes : { "1", "t", "T", "true", "True", "TRUE", "tRuE" }) {
		CHECK_EQ(Serialize::boolean(std::string_view(yes)), std::string("t"));
	}
	for (const char* no : { "", "0", "f", "F", "false", "False", "FALSE" }) {
		CHECK_EQ(Serialize::boolean(std::string_view(no)), std::string("f"));
	}
	for (const char* bad : { "2", "x", "yes", "no", "trux", "xxxx", "fals1", "nope!", "truee", "tr" }) {
		CHECK_THROW(Serialize::boolean(std::string_view(bad)), SerializationError);
	}
	CHECK(Unserialize::boolean("t"));
	CHECK(!Unserialize::boolean("f"));
	CHECK_EQ(Serialize::boolean(true), std::string("t"));
	CHECK_EQ(Serialize::boolean(false), std::string("f"));
}

void test_uuid_text() {
	const auto one = Serialize::uuid(V4);
	CHECK_EQ(Unserialize::uuid(one), std::string(V4));
	CHECK_EQ(Serialize::uuid(std::string("{") + V4 + "}"), one);
	CHECK_EQ(Serialize::uuid(std::string("urn:uuid:") + V4), one);
	CHECK_EQ(Serialize::uuid(std::string(V4) + ";" + V4B), one + Serialize::uuid(V4B));
	CHECK_EQ(Serialize::uuid(std::string(V4) + ";;" + V4B), one + Serialize::uuid(V4B));
	CHECK_EQ(Unserialize::uuid(Serialize::uuid(std::string(V4) + ";" + V4B)), std::string(V4) + ";" + V4B);
	CHECK_THROW(Serialize::uuid(""), SerializationError);
	CHECK_THROW(Serialize::uuid("ab"), SerializationError);
	CHECK_THROW(Serialize::uuid("abc"), SerializationError);
	CHECK_THROW(Serialize::uuid(std::string(V4).substr(0, 35) + "x"), SerializationError);
	CHECK_THROW(Serialize::uuid(std::string(V4) + ";abc"), SerializationError);
	// The encoded form ("~" and base59) is what a condensed, compacted UUID prints as; both read back to the same bytes.
	const auto bytes = Serialize::uuid(V6);
	const auto encoded = Unserialize::uuid(bytes, UUIDRepr::encoded);
	REQUIRE(!encoded.empty() && encoded[0] == '~');
	CHECK_EQ(Serialize::uuid(encoded), bytes);
	CHECK_EQ(Unserialize::uuid(bytes), std::string(V6));
	CHECK_EQ(Unserialize::uuid(bytes, UUIDRepr::vanilla), std::string(V6));
	// A full 16-byte UUID has no condensed form: its encoded and plain forms are the same text.
	CHECK_EQ(Unserialize::uuid(one, UUIDRepr::encoded), std::string(V4));
	CHECK_THROW(Serialize::uuid("~!!!!!!!!"), SerializationError);
	CHECK_THROW(Serialize::uuid("~0"), SerializationError);
}

// --- by field type, from MsgPack and from text --------------------------------------------------------------------

void test_serialize_by_type() {
	const auto point = js(R"({"_point":{"_latitude":19.70,"_longitude":-101.19}})");
	const auto geo = Serialize::geospatial(point);
	CHECK_EQ(Serialize::geospatial(std::string_view("POINT(-101.19 19.70)")), geo);
	CHECK_NE(Serialize::geospatial(std::string_view("POINT(-101.20 19.71)")), geo);
	CHECK_EQ(Serialize::geospatial(FieldType::geo, point), geo);

	CHECK_EQ(Serialize::serialize(spc(FieldType::integer), num(-5)), Serialize::integer(int64_t(-5)));
	CHECK_EQ(Serialize::serialize(spc(FieldType::positive), unum(5)), Serialize::positive(uint64_t(5)));
	CHECK_EQ(Serialize::serialize(spc(FieldType::floating), flt(2.5)), Serialize::floating(2.5));
	CHECK_EQ(Serialize::serialize(spc(FieldType::datetime), str("2015-10-10")), Serialize::datetime(std::string_view("2015-10-10")));
	CHECK_EQ(Serialize::serialize(spc(FieldType::date), str("2015-10-10")), Serialize::datetime(std::string_view("2015-10-10")));
	CHECK_EQ(Serialize::serialize(spc(FieldType::time), str("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::serialize(spc(FieldType::timedelta), str("+01:30:00")), Serialize::timestamp(5400.0));
	CHECK_EQ(Serialize::serialize(spc(FieldType::boolean), MsgPack(true)), std::string("t"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::keyword), str("kw")), std::string("kw"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::text), str("some text")), std::string("some text"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::geo), point), geo);
	CHECK_EQ(Serialize::serialize(spc(FieldType::uuid), str(V4)), Serialize::uuid(V4));
	CHECK_THROW(Serialize::serialize(spc(FieldType::object), str("x")), SerializationError);
	CHECK_THROW(Serialize::serialize(spc(FieldType::empty), str("x")), SerializationError);

	using sv = std::string_view;
	CHECK_EQ(Serialize::serialize(spc(FieldType::integer), sv("-5")), Serialize::integer(int64_t(-5)));
	CHECK_EQ(Serialize::serialize(spc(FieldType::positive), sv("5")), Serialize::positive(uint64_t(5)));
	CHECK_EQ(Serialize::serialize(spc(FieldType::floating), sv("2.5")), Serialize::floating(2.5));
	CHECK_EQ(Serialize::serialize(spc(FieldType::datetime), sv("2015-10-10")), Serialize::datetime(sv("2015-10-10")));
	CHECK_EQ(Serialize::serialize(spc(FieldType::date), sv("2015-10-10")), Serialize::datetime(sv("2015-10-10")));
	CHECK_EQ(Serialize::serialize(spc(FieldType::time), sv("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::serialize(spc(FieldType::timedelta), sv("+01:30:00")), Serialize::timestamp(5400.0));
	CHECK_EQ(Serialize::serialize(spc(FieldType::boolean), sv("true")), std::string("t"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::keyword), sv("kw")), std::string("kw"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::text), sv("some text")), std::string("some text"));
	CHECK_EQ(Serialize::serialize(spc(FieldType::geo), sv("POINT(-101.19 19.70)")), geo);
	CHECK_EQ(Serialize::serialize(spc(FieldType::uuid), sv(V4)), Serialize::uuid(V4));
	CHECK_EQ(Serialize::serialize(spc(FieldType::text), std::string("owned")), std::string("owned"));
	CHECK_THROW(Serialize::serialize(spc(FieldType::object), sv("x")), SerializationError);

	// string(): the types a text value can be of, and the others refuse it.
	CHECK_EQ(Serialize::string(spc(FieldType::datetime), sv("2015-10-10")), Serialize::datetime(sv("2015-10-10")));
	CHECK_EQ(Serialize::string(spc(FieldType::date), sv("2015-10-10")), Serialize::datetime(sv("2015-10-10")));
	CHECK_EQ(Serialize::string(spc(FieldType::time), sv("10:30:00")), Serialize::timestamp(37800.0));
	CHECK_EQ(Serialize::string(spc(FieldType::timedelta), sv("+01:30:00")), Serialize::timestamp(5400.0));
	CHECK_EQ(Serialize::string(spc(FieldType::boolean), sv("false")), std::string("f"));
	CHECK_EQ(Serialize::string(spc(FieldType::keyword), sv("kw")), std::string("kw"));
	CHECK_EQ(Serialize::string(spc(FieldType::text), sv("tx")), std::string("tx"));
	CHECK_EQ(Serialize::string(spc(FieldType::geo), sv("POINT(-101.19 19.70)")), geo);
	CHECK_EQ(Serialize::string(spc(FieldType::uuid), sv(V4)), Serialize::uuid(V4));
	CHECK_THROW(Serialize::string(spc(FieldType::integer), sv("5")), SerializationError);
	CHECK_THROW(Serialize::string(spc(FieldType::floating), sv("5")), SerializationError);
}

void test_msgpack_by_spec() {
	const auto day = Serialize::timestamp(86400.0);
	// A boolean meets only a boolean field.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::boolean), MsgPack(true)), std::string("t"));
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::integer), MsgPack(true)), SerializationError);
	// An unsigned meets every numeric field it fits.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::positive), unum(7)), Serialize::positive(uint64_t(7)));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::integer), unum(7)), Serialize::integer(int64_t(7)));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::floating), unum(7)), Serialize::positive(uint64_t(7)));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::datetime), unum(86400)), day);
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::time), unum(3600)), Serialize::time(3600.0));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::timedelta), unum(3600)), Serialize::timedelta(3600.0));
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::integer), unum(static_cast<uint64_t>(I64_MAX) + 1)), SerializationError);
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::keyword), unum(7)), SerializationError);
	// A negative meets the signed ones.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::integer), num(-7)), Serialize::integer(int64_t(-7)));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::floating), num(-7)), Serialize::integer(int64_t(-7)));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::timedelta), num(-3600)), Serialize::timedelta(-3600.0));
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::positive), num(-7)), SerializationError);
	// A float meets floats and the date types, never an integer field.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::floating), flt(2.5)), Serialize::floating(2.5));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::datetime), flt(86400.0)), day);
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::integer), flt(2.5)), SerializationError);
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::floating), flt(std::nan(""))), SerializationError);
	// Text goes by the field's type.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::keyword), str("kw")), std::string("kw"));
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::datetime), str("1970-01-02")), day);
	// Anything else is not a value.
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::keyword), MsgPack::NIL()), SerializationError);
	CHECK_THROW(Serialize::MsgPack(spc(FieldType::keyword), MsgPack::ARRAY()), SerializationError);
}

void test_cast_objects() {
	const auto day = Serialize::timestamp(86400.0);
	const auto point = js(R"({"_point":{"_latitude":19.70,"_longitude":-101.19}})");
	const auto obj = [](FieldType type, const std::string& json) { return Serialize::object(spc(type), js(json)); };
	CHECK_EQ(obj(FieldType::integer, R"({"_integer":"5"})"), Serialize::integer(int64_t(5)));
	CHECK_EQ(obj(FieldType::positive, R"({"_positive":7})"), Serialize::positive(uint64_t(7)));
	CHECK_EQ(obj(FieldType::floating, R"({"_float":"1.5"})"), Serialize::floating(1.5));
	CHECK_EQ(obj(FieldType::boolean, R"({"_boolean":true})"), std::string("t"));
	CHECK_EQ(obj(FieldType::keyword, R"({"_keyword":"kw"})"), std::string("kw"));
	CHECK_EQ(obj(FieldType::text, R"({"_text":"tx"})"), std::string("tx"));
	CHECK_EQ(obj(FieldType::uuid, std::string(R"({"_uuid":")") + V4 + "\"}"), Serialize::uuid(V4));
	CHECK_EQ(obj(FieldType::datetime, R"({"_datetime":"1970-01-02"})"), day);
	CHECK_EQ(obj(FieldType::time, R"({"_time":"10:30:00"})"), Serialize::timestamp(37800.0));
	CHECK_EQ(obj(FieldType::timedelta, R"({"_timedelta":"+01:30:00"})"), Serialize::timestamp(5400.0));
	CHECK_EQ(obj(FieldType::geo, "{\"_ewkt\":\"POINT(-101.19 19.70)\"}"), Serialize::geospatial(point));
	CHECK_EQ(Serialize::object(spc(FieldType::geo), point), Serialize::geospatial(point));
	for (const char* kind : { "_circle", "_convex", "_polygon", "_chull", "_multipoint", "_multicircle", "_multipolygon", "_multichull",
			"_geometrycollection", "_geometryintersection" }) {
		// Each geospatial cast goes to the geospatial serializer, which reads its own forms; a value it refuses is its error.
		CHECK_THROW(Serialize::object(spc(FieldType::geo), js(std::string("{\"") + kind + "\":1}")), std::exception);
	}
	// The cast is the field's: a cast to another type than the field's is refused.
	CHECK_THROW(obj(FieldType::keyword, R"({"_integer":5})"), SerializationError);
	CHECK_THROW(obj(FieldType::integer, R"({"_boolean":true})"), SerializationError);

	// Not a cast: a key that is not reserved, an empty one, two entries, an empty map.
	CHECK_THROW(obj(FieldType::integer, R"({"x":1})"), SerializationError);
	CHECK_THROW(obj(FieldType::integer, R"({"":1})"), SerializationError);
	CHECK_THROW(obj(FieldType::integer, R"({"_integer":1,"_float":2})"), SerializationError);
	CHECK_THROW(obj(FieldType::integer, R"({})"), SerializationError);
	// Finding: `_date` and `_multiconvex` are cast words the Cast namespace knows, and `_lua` is one too, but
	// Serialize::object, guess_type and guess_serialize accept none of them.
	CHECK_THROW(obj(FieldType::date, R"({"_date":"1970-01-02"})"), SerializationError);
	CHECK_THROW(obj(FieldType::geo, R"({"_multiconvex":1})"), SerializationError);
	CHECK_THROW(obj(FieldType::geo, R"({"_lua":1})"), SerializationError);
	// ... and through Serialize::MsgPack the map takes the same road.
	CHECK_EQ(Serialize::MsgPack(spc(FieldType::integer), js(R"({"_integer":"5"})")), Serialize::integer(int64_t(5)));
}

// --- guessing a type ----------------------------------------------------------------------------------------------

void test_guess_type() {
	CHECK(Serialize::guess_type(num(-5)) == FieldType::integer);
	CHECK(Serialize::guess_type(unum(5)) == FieldType::integer);
	CHECK(Serialize::guess_type(unum(I64_MAX)) == FieldType::integer);
	CHECK(Serialize::guess_type(unum(static_cast<uint64_t>(I64_MAX) + 1)) == FieldType::positive);
	CHECK(Serialize::guess_type(flt(2.5)) == FieldType::floating);
	CHECK(Serialize::guess_type(MsgPack(true)) == FieldType::boolean);

	struct Case {
		const char* text;
		FieldType type;
	};
	const Case texts[] = {
		{ V4, FieldType::uuid },
		{ "2015-10-10", FieldType::date },
		{ "2015-10-10T23:03:03.000", FieldType::datetime },
		{ "10:30:00", FieldType::time },
		{ "+01:30:00", FieldType::timedelta },
		{ "POINT(-101.19 19.70)", FieldType::geo },
		{ "-5", FieldType::integer },
		{ "12345678901234567890", FieldType::positive },
		{ "1.5", FieldType::floating },
		{ "1e5", FieldType::floating },
		{ "hello", FieldType::keyword },
		{ "hello world", FieldType::text },
		{ "", FieldType::text },
	};
	for (const auto& c : texts) {
		const auto got = Serialize::guess_type(str(c.text));
		if (got != c.type) {
			std::fprintf(stderr, "  FAIL: guess_type('%s') is %s, expected %s\n", c.text, std::string(enum_name(got)).c_str(), std::string(enum_name(c.type)).c_str());
		}
		CHECK(got == c.type);
	}

	struct Cast {
		const char* json;
		FieldType type;
	};
	const Cast casts[] = {
		{ R"({"_integer":1})", FieldType::integer },
		{ R"({"_positive":1})", FieldType::positive },
		{ R"({"_float":1})", FieldType::floating },
		{ R"({"_boolean":true})", FieldType::boolean },
		{ R"({"_keyword":"k"})", FieldType::keyword },
		{ R"({"_text":"t"})", FieldType::text },
		{ R"({"_uuid":"u"})", FieldType::uuid },
		{ R"({"_datetime":"d"})", FieldType::datetime },
		{ R"({"_time":"t"})", FieldType::time },
		{ R"({"_timedelta":"t"})", FieldType::timedelta },
		{ R"({"_ewkt":"e"})", FieldType::geo },
		{ R"({"_point":1})", FieldType::geo },
		{ R"({"_circle":1})", FieldType::geo },
		{ R"({"_convex":1})", FieldType::geo },
		{ R"({"_polygon":1})", FieldType::geo },
		{ R"({"_chull":1})", FieldType::geo },
		{ R"({"_multipoint":1})", FieldType::geo },
		{ R"({"_multicircle":1})", FieldType::geo },
		{ R"({"_multipolygon":1})", FieldType::geo },
		{ R"({"_multichull":1})", FieldType::geo },
		{ R"({"_geometrycollection":1})", FieldType::geo },
		{ R"({"_geometryintersection":1})", FieldType::geo },
	};
	for (const auto& c : casts) {
		const auto got = Serialize::guess_type(js(c.json));
		if (got != c.type) {
			std::fprintf(stderr, "  FAIL: guess_type(%s) is %s\n", c.json, std::string(enum_name(got)).c_str());
		}
		CHECK(got == c.type);
	}
	CHECK_THROW(Serialize::guess_type(js(R"({"x":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_type(js(R"({"":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_type(js(R"({"_integer":1,"_float":2})")), SerializationError);
	CHECK_THROW(Serialize::guess_type(js(R"({"_date":"1970-01-02"})")), SerializationError);
	CHECK_THROW(Serialize::guess_type(js(R"({"_multiconvex":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_type(MsgPack::NIL()), SerializationError);
	CHECK_THROW(Serialize::guess_type(MsgPack::ARRAY()), SerializationError);
}

void test_guess_serialize() {
	const auto point = js(R"({"_point":{"_latitude":19.70,"_longitude":-101.19}})");
	using P = std::pair<FieldType, std::string>;
	const auto is = [](const MsgPack& v, FieldType type, const std::string& bytes) {
		const P got = Serialize::guess_serialize(v);
		if (got.first != type || got.second != bytes) {
			std::fprintf(stderr, "  FAIL: guess_serialize(%s) is (%s, %zu bytes)\n", v.to_string().c_str(), std::string(enum_name(got.first)).c_str(), got.second.size());
		}
		CHECK(got.first == type);
		CHECK_EQ(got.second, bytes);
	};
	is(num(-5), FieldType::integer, Serialize::integer(int64_t(-5)));
	is(unum(5), FieldType::integer, Serialize::integer(int64_t(5)));
	is(unum(U64_MAX), FieldType::positive, Serialize::positive(U64_MAX));
	is(flt(2.5), FieldType::floating, Serialize::floating(2.5));
	is(MsgPack(true), FieldType::boolean, std::string("t"));
	is(str(V4), FieldType::uuid, Serialize::uuid(V4));
	is(str("2015-10-10T23:03:03.000"), FieldType::datetime, Serialize::datetime(std::string_view("2015-10-10T23:03:03.000")));
	is(str("10:30:00"), FieldType::time, Serialize::timestamp(37800.0));
	is(str("+01:30:00"), FieldType::timedelta, Serialize::timestamp(5400.0));
	is(str("POINT(-101.19 19.70)"), FieldType::geo, Serialize::geospatial(point));
	is(str("-5"), FieldType::integer, Serialize::integer(int64_t(-5)));
	is(str("12345678901234567890"), FieldType::positive, Serialize::positive(uint64_t(12345678901234567890ULL)));
	is(str("1.5"), FieldType::floating, Serialize::floating(1.5));
	is(str("hello"), FieldType::keyword, std::string("hello"));         // no digit or sign: the fast path
	is(str("hello world"), FieldType::text, std::string("hello world"));
	is(str(""), FieldType::text, std::string());
	is(str("a-b c"), FieldType::text, std::string("a-b c"));            // a sign, so the typed readers try and refuse it
	is(str("a-b"), FieldType::keyword, std::string("a-b"));

	is(js(R"({"_integer":"5"})"), FieldType::integer, Serialize::integer(int64_t(5)));
	is(js(R"({"_positive":7})"), FieldType::positive, Serialize::positive(uint64_t(7)));
	is(js(R"({"_float":"1.5"})"), FieldType::floating, Serialize::floating(1.5));
	is(js(R"({"_boolean":true})"), FieldType::boolean, std::string("t"));
	is(js(R"({"_keyword":"kw"})"), FieldType::keyword, std::string("kw"));
	is(js(R"({"_text":"tx"})"), FieldType::text, std::string("tx"));
	is(js(std::string(R"({"_uuid":")") + V4 + "\"}"), FieldType::uuid, Serialize::uuid(V4));
	is(js(R"({"_datetime":"1970-01-02"})"), FieldType::datetime, Serialize::timestamp(86400.0));
	is(point, FieldType::geo, Serialize::geospatial(point));
	for (const char* kind : { "_ewkt", "_circle", "_convex", "_polygon", "_chull", "_multipoint", "_multicircle", "_multipolygon", "_multichull",
			"_geometrycollection", "_geometryintersection" }) {
		CHECK_THROW(Serialize::guess_serialize(js(std::string("{\"") + kind + "\":1}")), std::exception);
	}
	// Finding: a `_time` or `_timedelta` cast goes through the datetime parser, so a clock text is refused here while
	// Serialize::object takes it, and a number gets the same bytes whichever type it is cast to.
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_time":"10:30:00"})")), std::exception);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_timedelta":"+01:30:00"})")), std::exception);
	is(js(R"({"_time":3600})"), FieldType::time, Serialize::timestamp(3600.0));
	is(js(R"({"_timedelta":3600})"), FieldType::timedelta, Serialize::timestamp(3600.0));

	CHECK_THROW(Serialize::guess_serialize(js(R"({"x":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_integer":1,"_float":2})")), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_date":"1970-01-02"})")), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_multiconvex":1})")), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(MsgPack::NIL()), SerializationError);
	CHECK_THROW(Serialize::guess_serialize(MsgPack::ARRAY()), SerializationError);
}

// --- document ids -------------------------------------------------------------------------------------------------

void test_ids() {
	CHECK_EQ(Serialize::id(spc(FieldType::integer), num(-5)), Serialize::integer(int64_t(-5)));
	CHECK_EQ(Serialize::id(spc(FieldType::integer), unum(5)), Serialize::positive(uint64_t(5)));
	CHECK_EQ(Serialize::id(spc(FieldType::integer), str("7")), Serialize::integer(int64_t(7)));
	CHECK_EQ(Serialize::id(spc(FieldType::positive), unum(U64_MAX)), Serialize::positive(U64_MAX));
	CHECK_EQ(Serialize::id(spc(FieldType::floating), flt(2.5)), Serialize::floating(2.5));
	CHECK_EQ(Serialize::id(spc(FieldType::floating), num(7)), Serialize::integer(int64_t(7)));
	CHECK_EQ(Serialize::id(spc(FieldType::keyword), str("doc-1")), std::string("doc-1"));
	CHECK_EQ(Serialize::id(spc(FieldType::uuid), str(V4)), Serialize::uuid(V4));
	CHECK_THROW(Serialize::id(spc(FieldType::integer), str("abc")), ClientError);
	CHECK_THROW(Serialize::id(spc(FieldType::positive), num(-1)), ClientError);
	CHECK_EQ(Serialize::id(uint64_t(9)), Serialize::positive(uint64_t(9)));

	// The type an id of an unset index takes, and its bytes.
	using P = std::pair<FieldType, std::string>;
	const P a = Serialize::guess_id(num(-5));
	CHECK(a.first == FieldType::integer);
	CHECK_EQ(a.second, Serialize::integer(int64_t(-5)));
	const P b = Serialize::guess_id(unum(U64_MAX));
	CHECK(b.first == FieldType::positive);
	CHECK_EQ(b.second, Serialize::positive(U64_MAX));
	const P c = Serialize::guess_id(flt(2.5));
	CHECK(c.first == FieldType::floating);
	CHECK_EQ(c.second, Serialize::floating(2.5));
	const P d = Serialize::guess_id(str("doc-1"));
	CHECK(d.first == FieldType::keyword);
	CHECK_EQ(d.second, std::string("doc-1"));
	const P e = Serialize::guess_id(str("hello world"));
	CHECK(e.first == FieldType::text);

	// Reading an id back.
	CHECK(Unserialize::id(Serialize::id(uint64_t(7))) == std::optional<uint64_t>(7));
	CHECK(Unserialize::id(Serialize::integer(int64_t(7))) == std::optional<uint64_t>(7));
	CHECK(!Unserialize::id(Serialize::floating(2.5)).has_value());
	CHECK(!Unserialize::id(Serialize::integer(int64_t(-7))).has_value());
	CHECK(!Unserialize::id(std::string_view("\x00", 1)).has_value());
	CHECK_EQ(Unserialize::id(FieldType::integer, Serialize::integer(int64_t(-7))).i64(), -7);
	CHECK_EQ(Unserialize::id(FieldType::integer, Serialize::positive(uint64_t(1) << 63)).i64(), I64_MAX);   // saturates
	CHECK_EQ(Unserialize::id(FieldType::positive, Serialize::positive(U64_MAX)).u64(), U64_MAX);
	CHECK_EQ(Unserialize::id(FieldType::positive, Serialize::integer(int64_t(-7))).u64(), 0u);              // saturates
	CHECK(Unserialize::id(FieldType::floating, Serialize::floating(2.5)).f64() == 2.5);
	CHECK(Unserialize::id(FieldType::keyword, Serialize::floating(2.5)).f64() == 2.5);
	CHECK_THROW(Unserialize::id(FieldType::integer, std::string_view("\x00", 1)), SerializationError);
	CHECK_THROW(Unserialize::id(FieldType::integer, std::string_view()), SerializationError);
}

// --- reading values back ------------------------------------------------------------------------------------------

void test_unserialize_msgpack() {
	CHECK(Unserialize::MsgPack(FieldType::floating, Serialize::floating(2.5)).f64() == 2.5);
	CHECK_EQ(Unserialize::MsgPack(FieldType::integer, Serialize::integer(int64_t(-7))).i64(), -7);
	CHECK_EQ(Unserialize::MsgPack(FieldType::positive, Serialize::positive(uint64_t(7))).u64(), 7u);
	CHECK_EQ(Unserialize::MsgPack(FieldType::datetime, Serialize::timestamp(86400.0)).str(), std::string("1970-01-02T00:00:00"));
	CHECK_EQ(Unserialize::MsgPack(FieldType::date, Serialize::timestamp(86400.0)).str(), std::string("1970-01-02T00:00:00"));
	CHECK_EQ(Unserialize::MsgPack(FieldType::time, Serialize::timestamp(37800.0)).str(), std::string("10:30:00"));
	CHECK_EQ(Unserialize::MsgPack(FieldType::timedelta, Serialize::timestamp(5400.0)).str(), Datetime::timedelta_to_string(5400.0));
	CHECK_EQ(Unserialize::MsgPack(FieldType::boolean, "t").boolean(), true);
	CHECK_EQ(Unserialize::MsgPack(FieldType::boolean, "f").boolean(), false);
	CHECK_EQ(Unserialize::MsgPack(FieldType::keyword, "kw").str(), std::string("kw"));
	CHECK_EQ(Unserialize::MsgPack(FieldType::text, "tx").str(), std::string("tx"));
	CHECK_EQ(Unserialize::MsgPack(FieldType::uuid, Serialize::uuid(V4)).str(), std::string(V4));
	CHECK_THROW(Unserialize::MsgPack(FieldType::object, "x"), SerializationError);
	CHECK_THROW(Unserialize::MsgPack(FieldType::empty, "x"), SerializationError);

	// A geospatial value is its ranges and centroids.
	const std::vector<range_t> ranges = { range_t(1, 2), range_t(10, 20) };
	const std::vector<Cartesian> centroids = { Cartesian(0.5, -0.25, 0.75) };
	const auto geo = Unserialize::MsgPack(FieldType::geo, Serialize::ranges_centroids(ranges, centroids));
	CHECK_EQ(geo.at("Ranges").size(), 2u);
	CHECK_EQ(geo.at("Ranges").at(1).at(0).u64(), 10u);
	CHECK_EQ(geo.at("Ranges").at(1).at(1).u64(), 20u);
	CHECK_EQ(geo.at("Centroids").size(), 1u);
	CHECK(near(geo.at("Centroids").at(0).at(0).f64(), 0.5));
	CHECK(near(geo.at("Centroids").at(0).at(1).f64(), -0.25));
	CHECK(near(geo.at("Centroids").at(0).at(2).f64(), 0.75));
	const auto no_centroids = Unserialize::MsgPack(FieldType::geo, Serialize::ranges_centroids(ranges, {}));
	CHECK_EQ(no_centroids.at("Ranges").size(), 2u);

	// Bytes that are not a number.
	CHECK_THROW(Unserialize::invalid_number("x"), SerializationError);
	CHECK_THROW(Unserialize::number(std::string_view("\x00", 1)), SerializationError);
	CHECK_THROW(Unserialize::integer(std::string_view("\x00", 1)), SerializationError);
	CHECK_THROW(Unserialize::positive(std::string_view("\x00", 1)), SerializationError);
	CHECK_THROW(Unserialize::version(std::string_view("\x00", 1)), SerializationError);
	CHECK_EQ(Unserialize::version(Serialize::version(41)), 41u);
	// A fraction truncates and a value out of range saturates.
	CHECK_EQ(Unserialize::integer(Serialize::floating(-2.5)), -2);
	CHECK_EQ(Unserialize::integer(Serialize::floating(1e30)), I64_MAX);
	CHECK_EQ(Unserialize::positive(Serialize::floating(2.5)), 2u);
	CHECK_EQ(Unserialize::positive(Serialize::floating(1e30)), U64_MAX);
}

// --- geospatial bytes ---------------------------------------------------------------------------------------------

void test_geospatial_bytes() {
	const std::vector<range_t> ranges = { range_t(1, 2), range_t(0x010203040506ULL, 0x0a0b0c0d0e0fULL) };
	const std::vector<Cartesian> centroids = { Cartesian(0.5, -0.25, 0.75), Cartesian(-1.0, 0.0, 1.0) };

	const auto trixel = Serialize::trixel_id(0x01020304050607ULL);
	CHECK_EQ(trixel.size(), static_cast<size_t>(HTM_BYTES_ID));
	CHECK_EQ(Unserialize::trixel_id(trixel), 0x01020304050607ULL);
	CHECK_THROW(Unserialize::trixel_id(trixel.substr(1)), SerializationError);
	CHECK_THROW(Unserialize::trixel_id(trixel + "x"), SerializationError);

	const auto range = Serialize::range(ranges[1]);
	CHECK_EQ(range.size(), static_cast<size_t>(SERIALIZED_LENGTH_RANGE));
	const auto back = Unserialize::range(range);
	CHECK_EQ(back.start, ranges[1].start);
	CHECK_EQ(back.end, ranges[1].end);
	CHECK_THROW(Unserialize::range(range.substr(1)), SerializationError);

	const auto cart = Serialize::cartesian(centroids[0]);
	CHECK_EQ(cart.size(), static_cast<size_t>(SERIALIZED_LENGTH_CARTESIAN));
	const auto c = Unserialize::cartesian(cart);
	CHECK(near(c.x, 0.5) && near(c.y, -0.25) && near(c.z, 0.75));
	CHECK_THROW(Unserialize::cartesian(cart.substr(1)), SerializationError);
	CHECK_THROW(Unserialize::cartesian(""), SerializationError);

	// Both lists, then each alone.
	const auto both = Serialize::ranges_centroids(ranges, centroids);
	const auto pair = Unserialize::ranges_centroids(both);
	CHECK_EQ(pair.first.size(), 2u);
	CHECK_EQ(pair.second.size(), 2u);
	CHECK_EQ(pair.first.front().end, 2u);
	CHECK_EQ(pair.first.back().end, 0x0a0b0c0d0e0fULL);
	CHECK(near(pair.second.back().x, -1.0));
	CHECK_EQ(Unserialize::ranges(both).size(), 2u);
	CHECK_EQ(Unserialize::centroids(both).size(), 2u);

	// A list holds its parts length-prefixed; a lone part is its own bytes (its first byte must not be the list's marker).
	const auto only_ranges = Serialize::ranges_centroids(ranges, {});
	CHECK_EQ(Unserialize::ranges(only_ranges).size(), 2u);
	CHECK_EQ(Unserialize::centroids(only_ranges).size(), 0u);
	CHECK_EQ(Unserialize::ranges_centroids(only_ranges).first.size(), 2u);
	CHECK_EQ(Unserialize::ranges_centroids(only_ranges).second.size(), 0u);

	const auto only_centroids = Serialize::centroids(centroids);
	CHECK_EQ(Unserialize::ranges(only_centroids).size(), 0u);
	CHECK_EQ(Unserialize::centroids(only_centroids).size(), 2u);

	const auto lone = Serialize::ranges({ range_t(0x01020304050607ULL, 0x0a0b0c0d0e0f10ULL) });
	CHECK_EQ(lone.size(), static_cast<size_t>(SERIALIZED_LENGTH_RANGE));
	CHECK_EQ(Unserialize::ranges(lone).size(), 1u);
	CHECK_EQ(Unserialize::ranges(lone).front().start, 0x01020304050607ULL);
	CHECK_EQ(Unserialize::centroids(lone).size(), 0u);
	CHECK_EQ(Unserialize::ranges_centroids(lone).first.size(), 1u);
	CHECK_EQ(Unserialize::ranges_centroids(lone).second.size(), 0u);

	// Nothing at all, and too many parts.
	CHECK_EQ(Unserialize::ranges("").size(), 0u);
	CHECK_EQ(Unserialize::centroids("").size(), 0u);
	CHECK_EQ(Unserialize::ranges_centroids("").first.size(), 0u);
	CHECK_EQ(Unserialize::ranges_centroids("").second.size(), 0u);
	const std::vector<std::string> three = { "a", "b", "c" };
	const auto crowded = StringList::serialize(three.begin(), three.end());
	CHECK_THROW(Unserialize::ranges_centroids(crowded), SerializationError);
	CHECK_THROW(Unserialize::ranges(crowded), SerializationError);
	CHECK_THROW(Unserialize::centroids(crowded), SerializationError);

	// The hash of a range list is its length-prefixed hash: equal lists agree and different ones do not.
	CHECK_EQ(Serialize::ranges_hash(ranges), Serialize::ranges_hash(ranges));
	CHECK_NE(Serialize::ranges_hash(ranges), Serialize::ranges_hash({ range_t(1, 2) }));
	CHECK_EQ(Serialize::serialize(ranges), Serialize::ranges(ranges));
}

// --- type names ---------------------------------------------------------------------------------------------------

void test_field_type_names() {
	struct Case {
		const char* name;
		FieldType type;
	};
	const Case cases[] = {
		{ " ", FieldType::empty }, { "e", FieldType::empty }, { "empty", FieldType::empty },
		{ "a", FieldType::array }, { "array", FieldType::array },
		{ "b", FieldType::boolean },
		{ "d", FieldType::datetime }, { "datetime", FieldType::datetime },
		{ "f", FieldType::floating }, { "float", FieldType::floating }, { "floating", FieldType::floating },
		{ "g", FieldType::geo }, { "geospatial", FieldType::geo },
		{ "i", FieldType::integer }, { "integer", FieldType::integer },
		{ "o", FieldType::object }, { "object", FieldType::object },
		{ "p", FieldType::positive }, { "positive", FieldType::positive },
		{ "k", FieldType::keyword }, { "keyword", FieldType::keyword }, { "term", FieldType::keyword },
		{ "u", FieldType::uuid },
		{ "x", FieldType::script }, { "script", FieldType::script },
		{ "s", FieldType::text }, { "string", FieldType::text }, { "t", FieldType::text }, { "text", FieldType::text },
		{ "time", FieldType::time },
		{ "foreign", FieldType::foreign },
		{ "timedelta", FieldType::timedelta },
	};
	for (const auto& c : cases) {
		const auto got = Unserialize::get_field_type(c.name);
		if (got != c.type) {
			std::fprintf(stderr, "  FAIL: get_field_type('%s') is %s\n", c.name, std::string(enum_name(got)).c_str());
		}
		CHECK(got == c.type);
	}
	// Finding: "boolean" shares a case label with "datetime", so the full name of the boolean type reads as datetime
	// (the one-letter "b" is right).
	CHECK(Unserialize::get_field_type("boolean") == FieldType::datetime);
	// A word that is no type is refused; the perfect hash is not a membership test, so only some words reach the error.
	int refused = 0;
	for (const char* word : { "zzz", "nonsense", "bogus", "unknown", "uuid", "date", "q", "xyz", "booleans", "Text" }) {
		try {
			(void)Unserialize::get_field_type(word);
		} catch (const SerializationError&) {
			++refused;
		}
	}
	CHECK_GE(refused, 1);
}

}  // namespace

TEST_MAIN(test_text_and_uuid_predicates, test_exact_numbers, test_exact_strings_and_casts, test_numbers_by_type,
	test_time_and_timedelta_ranges, test_datetime_forms, test_datetime_by_spec, test_time_and_timedelta_by_spec,
	test_numbers_from_text, test_boolean_text, test_uuid_text, test_serialize_by_type, test_msgpack_by_spec, test_cast_objects,
	test_guess_type, test_guess_serialize, test_ids, test_unserialize_msgpack, test_geospatial_bytes, test_field_type_names)
