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


// Coverage tests for src/datetime_msgpack.cc: a datetime given as a number, a string or an object (date, time and
// datetime parts, with their aliases) parses to the instant it names, a time string is read in each of its forms
// with its zone applied, and every malformed part is a DatetimeError that names it. time_to_double and
// timedelta_to_double check the range of a number and read a string through the library's parsers.

#include "test_coverage_core.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "datetime_msgpack.h"
#include "msgpack.h"

namespace {

using Obj = MsgPack;

// "year-mon-day hour:min:sec+fsec"
std::string tm_str(const Datetime::tm_t& tm) {
	char buf[96];
	std::snprintf(buf, sizeof buf, "%d-%d-%d %d:%d:%d+%.6f", tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec, tm.fsec);
	return buf;
}

std::string parsed(const Obj& value) { return tm_str(Datetime::DatetimeParser(value)); }

// What parsing a `_time` string leaves in the time of day, on the epoch date.
std::string time_of(const std::string& text) { return parsed(Obj({ { "_time", text } })); }

Obj map_of(std::initializer_list<std::pair<const char*, Obj>> items) {
	Obj m = Obj::MAP();
	for (const auto& kv : items) {
		m[kv.first] = kv.second;
	}
	return m;
}

constexpr const char* EPOCH = "1970-1-1 0:0:0+0.000000";

}  // namespace

static void test_numbers_and_strings() {
	SEE(parsed(Obj(0)), std::string(EPOCH));
	SEE(parsed(Obj(86400)), std::string("1970-1-2 0:0:0+0.000000"));
	SEE(parsed(Obj(-1)), std::string("1969-12-31 23:59:59+0.000000"));
	SEE(parsed(Obj(86400.5)), std::string("1970-1-2 0:0:0+0.500000"));
	SEE(parsed(Obj("2021-03-04T05:06:07")), std::string("2021-3-4 5:6:7+0.000000"));
	SEE(parsed(Obj("2021-03-04")), std::string("2021-3-4 0:0:0+0.000000"));
	CHECK_THROWS_WITH(Datetime::DatetimeParser(Obj(true)), DatetimeError, "Datetime value must be numeric or string");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(Obj(nullptr)), DatetimeError, "Datetime value must be numeric or string");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(Obj::ARRAY()), DatetimeError, "Datetime value must be numeric or string");
}

// A timestamp names a year from 1 to INT_MAX on every platform: the conversion of an out-of-range double to whole
// seconds is undefined and x86 and arm64 disagree about it, so the range is checked before it.
static void test_timestamp_range() {
	SEE(parsed(Obj(-62135596800LL)), std::string("1-1-1 0:0:0+0.000000"));
	SEE(parsed(Obj(60000000000000000ULL)).substr(0, 5), std::string("19013"));
	for (const auto& bad : { Obj(-62135596801LL), Obj(-62135596800.5), Obj(INT64_MIN), Obj(18446744073709551615ULL), Obj(9223372036854775808ULL), Obj(1e30), Obj(-1e30), Obj(1.0 / 0.0), Obj(-1.0 / 0.0), Obj(std::nan("")) }) {
		CHECK_THROWS_WITH(Datetime::DatetimeParser(bad), DatetimeError, "Year is out of range");
	}
}

static void test_object_parts() {
	SEE(parsed(Obj::MAP()), std::string(EPOCH));
	SEE(parsed(map_of({ { "_year", 2021 }, { "_month", 3 }, { "_day", 4 }, { "_hour", 5 }, { "_min", 6 }, { "_sec", 7 }, { "_fsec", 0.5 } })),
	    std::string("2021-3-4 5:6:7+0.500000"));
	// The long names are the same keys.
	SEE(parsed(map_of({ { "_year", 2021 }, { "_minute", 6 }, { "_second", 7 } })), std::string("2021-1-1 0:6:7+0.000000"));
	// A fraction of a second is an integer or a float.
	SEE(parsed(map_of({ { "_fsec", 2 } })), std::string("1970-1-1 0:0:0+2.000000"));
	SEE(parsed(Obj({ { "_fsec", Obj(-2) } })), std::string("1970-1-1 0:0:0+-2.000000"));
	// A date part and a time part.
	SEE(parsed(map_of({ { "_date", map_of({ { "_year", 2020 }, { "_month", 2 }, { "_day", 29 } }) },
	                    { "_time", map_of({ { "_hour", 23 }, { "_minute", 59 }, { "_second", 58 }, { "_fsec", 0.25 } }) } })),
	    std::string("2020-2-29 23:59:58+0.250000"));
	SEE(parsed(map_of({ { "_time", "10:30" } })), std::string("1970-1-1 10:30:0+0.000000"));
	// A datetime part holds the same keys, a date, a time and a time string.
	SEE(parsed(map_of({ { "_datetime", map_of({ { "_year", 2001 }, { "_month", 9 }, { "_day", 9 }, { "_hour", 1 }, { "_min", 46 }, { "_sec", 40 }, { "_fsec", 0.5 } }) } })),
	    std::string("2001-9-9 1:46:40+0.500000"));
	SEE(parsed(map_of({ { "_datetime", map_of({ { "_date", map_of({ { "_year", 1999 }, { "_month", 12 }, { "_day", 31 } }) }, { "_time", "23:59:59" } }) } })),
	    std::string("1999-12-31 23:59:59+0.000000"));
	SEE(parsed(map_of({ { "_datetime", map_of({ { "_minute", 7 }, { "_second", 8 } }) } })), std::string("1970-1-1 0:7:8+0.000000"));
	SEE(parsed(map_of({ { "_datetime", "12:34:56" } })), std::string("1970-1-1 12:34:56+0.000000"));
	// Leap days are valid only in leap years.
	SEE(parsed(map_of({ { "_year", 2000 }, { "_month", 2 }, { "_day", 29 } })), std::string("2000-2-29 0:0:0+0.000000"));
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_year", 1900 }, { "_month", 2 }, { "_day", 29 } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_month", 13 } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_day", 0 } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_year", 0 } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_year", Obj(-5) } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_month", Obj(-1) } })), DatetimeError, "Datetime is out of range");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_day", Obj(-1) } })), DatetimeError, "Datetime is out of range");
}

static void test_object_part_errors() {
	for (const char* key : { "_year", "_month", "_day", "_hour", "_min", "_sec" }) {
		const std::string want = std::string("'") + key + "' must be a positive integer value";
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { key, "x" } })), DatetimeError, want);
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { key, 1.5 } })), DatetimeError, want);
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_datetime", map_of({ { key, "x" } }) } })), DatetimeError, want);
	}
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_fsec", "x" } })), DatetimeError, "'_fsec' must be a numeric value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { "_fsec", "x" } }) } })), DatetimeError, "'_fsec' must be a numeric value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { "_hour", "x" } }) } })), DatetimeError, "'_hour' must be a positive integer value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { "_min", "x" } }) } })), DatetimeError, "'_min' must be a positive integer value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { "_sec", "x" } }) } })), DatetimeError, "'_sec' must be a positive integer value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", map_of({ { "_year", "x" } }) } })), DatetimeError, "'_year' must be a positive integer value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", map_of({ { "_month", "x" } }) } })), DatetimeError, "'_month' must be a positive integer value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", map_of({ { "_day", "x" } }) } })), DatetimeError, "'_day' must be a positive integer value");
	// A part of the wrong shape.
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", 5 } })), DatetimeError, "'_time' must be a map or string value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", 5 } })), DatetimeError, "'_date' must be a map value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", "2020-01-01" } })), DatetimeError, "'_date' must be a map value");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_datetime", 5 } })), DatetimeError, "'_datetime' must be a map value");
	// A key the part does not take.
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { "_year", 1 } }) } })), DatetimeError, "Unsupported Key: '_year' in time");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_date", map_of({ { "_hour", 1 } }) } })), DatetimeError, "Unsupported Key: '_hour' in date");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_datetime", map_of({ { "_zone", 1 } }) } })), DatetimeError, "Unsupported Key: '_zone' in datetime");
	CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_zone", 1 } })), std::exception, "Unsupported Datetime: '_zone'");
}

// An object's hour, minute and second are range-checked like a time string's: 0-23, 0-59, 0-59.
static void test_object_time_is_range_checked() {
	SEE(parsed(map_of({ { "_hour", 23 }, { "_min", 59 }, { "_sec", 59 }, { "_fsec", 0.5 } })), std::string("1970-1-1 23:59:59+0.500000"));
	SEE(parsed(map_of({ { "_hour", 0 }, { "_min", 0 }, { "_sec", 0 } })), std::string(EPOCH));
	for (const auto& [key, bad] : std::initializer_list<std::pair<const char*, int>>{ { "_hour", 24 }, { "_hour", 99 }, { "_min", 60 }, { "_min", 99 },
	                                                                                  { "_minute", 60 }, { "_sec", 60 }, { "_second", 99 } }) {
		const std::string want = std::string("'") + (std::string(key) == "_minute" ? "_min" : std::string(key) == "_second" ? "_sec" : key) + "' is out of range";
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { key, bad } })), DatetimeError, want);
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", map_of({ { key, bad } }) } })), DatetimeError, want);
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_datetime", map_of({ { key, bad } }) } })), DatetimeError, want);
	}
	for (const char* key : { "_hour", "_min", "_sec" }) {
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { key, Obj(-1) } })), DatetimeError, std::string("'") + key + "' is out of range");
	}
}

static void test_time_strings() {
	SEE(time_of("10:30"), std::string("1970-1-1 10:30:0+0.000000"));
	SEE(time_of("23:59"), std::string("1970-1-1 23:59:0+0.000000"));
	SEE(time_of("10:30:45"), std::string("1970-1-1 10:30:45+0.000000"));
	SEE(time_of("10:30:45.5"), std::string("1970-1-1 10:30:45+0.500000"));
	SEE(time_of("10:30:45.123456789"), std::string("1970-1-1 10:30:45+0.123457"));  // to the microsecond
	SEE(time_of("10:30:45.9999999"), std::string("1970-1-1 10:30:45+0.999999"));     // never reaches the next second
	// A zone moves the time to UTC.
	SEE(time_of("10:30:45+01:00"), std::string("1970-1-1 9:30:45+0.000000"));
	SEE(time_of("10:30:45-02:30"), std::string("1970-1-1 13:0:45+0.000000"));
	SEE(time_of("10:30:45.5+01:15"), std::string("1970-1-1 9:15:45+0.500000"));
	SEE(time_of("10:30:45.25-00:30"), std::string("1970-1-1 11:0:45+0.250000"));
	SEE(time_of("00:30:00+01:00"), std::string("1969-12-31 23:30:0+0.000000"));  // across midnight, onto the day before
	// The zone applies to the whole datetime, whatever the order of its parts.
	SEE(parsed(map_of({ { "_date", map_of({ { "_year", 2020 }, { "_month", 3 }, { "_day", 1 } }) }, { "_time", "00:30:00+01:00" } })),
	    std::string("2020-2-29 23:30:0+0.000000"));
	SEE(parsed(map_of({ { "_time", "00:30:00+01:00" }, { "_date", map_of({ { "_year", 2020 }, { "_month", 3 }, { "_day", 1 } }) } })),
	    std::string("2020-2-29 23:30:0+0.000000"));
	SEE(parsed(map_of({ { "_datetime", map_of({ { "_time", "00:30:00+01:00" }, { "_date", map_of({ { "_year", 2020 }, { "_month", 3 }, { "_day", 1 } }) } }) } })),
	    std::string("2020-2-29 23:30:0+0.000000"));
	SEE(parsed(map_of({ { "_time", "23:30:00.5-02:30" }, { "_year", 2020 }, { "_month", 12 }, { "_day", 31 } })),
	    std::string("2021-1-1 2:0:0+0.500000"));
}

// A datetime with an offset is the instant of its UTC form: positive, negative and half-hour offsets, in a string
// and in a map.
static void test_zone_is_the_same_instant() {
	for (const auto& [local, utc] : std::initializer_list<std::pair<const char*, const char*>>{
	         { "2020-01-02T10:00:00+02:00", "2020-01-02T08:00:00Z" },
	         { "2020-01-02T01:00:00+02:00", "2020-01-01T23:00:00Z" },
	         { "2020-01-02T10:00:00-05:00", "2020-01-02T15:00:00Z" },
	         { "2020-12-31T22:00:00-05:00", "2021-01-01T03:00:00Z" },
	         { "2020-01-02T10:00:00+05:30", "2020-01-02T04:30:00Z" },
	         { "2020-03-01T00:15:00+05:30", "2020-02-29T18:45:00Z" },
	         { "2020-01-02T10:00:00-03:30", "2020-01-02T13:30:00Z" } }) {
		SEE(tm_str(Datetime::DatetimeParser(std::string_view(local))), tm_str(Datetime::DatetimeParser(std::string_view(utc))));
	}
	for (const auto& [time, hour, min] : std::initializer_list<std::tuple<const char*, int, int>>{
	         { "10:00:00+02:00", 8, 0 }, { "10:00:00-05:00", 15, 0 }, { "10:00:00+05:30", 4, 30 }, { "10:00:00-03:30", 13, 30 } }) {
		const auto want = Datetime::DatetimeParser(map_of({ { "_year", 2020 }, { "_month", 1 }, { "_day", 2 }, { "_hour", hour }, { "_min", min } }));
		SEE(tm_str(Datetime::DatetimeParser(map_of({ { "_time", time }, { "_year", 2020 }, { "_month", 1 }, { "_day", 2 } }))), tm_str(want));
		SEE(tm_str(Datetime::DatetimeParser(map_of({ { "_year", 2020 }, { "_month", 1 }, { "_day", 2 }, { "_time", time } }))), tm_str(want));
	}
}

static void test_time_string_errors() {
	const std::string format = "Error format in _time: ";
	for (const char* bad : { "10-30", "1030", "ab:cd", "1a:30", "10:3a", "10:30:4a", "10:30:45x", "10:30:45xyz00", "10:30:45.5x",
	                         "10:30:45.5+0100", "10:30:45.5+01-00", "10:30:45.5+01:00x", "10:30:45.5+0a:00", "10:30:45+0100x",
	                         "10:30:45+0a:00", "10:30:45+01:0a", "10:30:45.5+01:0a", "10:30:45.5.5", "10:30:45.5-", "" }) {
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", bad } })), DatetimeError, format + bad);
	}
	for (const char* bad : { "24:00", "10:60", "24:00:00", "10:60:00", "10:30:60", "10:30:45+24:00", "10:30:45+01:60",
	                         "10:30:45.5+24:00", "10:30:45.5+01:60" }) {
		CHECK_THROWS_WITH(Datetime::DatetimeParser(map_of({ { "_time", bad } })), DatetimeError, std::string("Time: ") + bad + " is out of range");
	}
}

static void test_time_to_double() {
	SEE(Datetime::time_to_double(Obj(0)), 0.0);
	SEE(Datetime::time_to_double(Obj::NEGATIVE_INTEGER()), 0.0);
	SEE(Datetime::time_to_double(Obj(86399)), 86399.0);
	SEE(Datetime::time_to_double(Obj(724779)), 724779.0);
	SEE(Datetime::time_to_double(Obj(0.25)), 0.25);
	SEE(Datetime::time_to_double(Obj(-0.1)), -0.1);
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj(724780)), TimeError, "out of range");
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj(-1)), TimeError, "out of range");
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj(724780.5)), TimeError, "out of range");
	SEE(Datetime::time_to_double(Obj("10:30:00")), 37800.0);
	SEE(Datetime::time_to_double(Obj("10:30:00")), Datetime::time_to_double(Datetime::TimeParser("10:30:00")));
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj("not a time")), TimeError, "");
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj(true)), TimeError, "Time must be numeric or string");
	CHECK_THROWS_WITH(Datetime::time_to_double(Obj::ARRAY()), TimeError, "Time must be numeric or string");
}

static void test_timedelta_to_double() {
	SEE(Datetime::timedelta_to_double(Obj(0)), 0.0);
	SEE(Datetime::timedelta_to_double(Obj::NEGATIVE_INTEGER()), 0.0);
	SEE(Datetime::timedelta_to_double(Obj(362439)), 362439.0);
	SEE(Datetime::timedelta_to_double(Obj(-362439)), -362439.0);
	SEE(Datetime::timedelta_to_double(Obj(-1.5)), -1.5);
	SEE(Datetime::timedelta_to_double(Obj(1.5)), 1.5);
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj(362440)), TimedeltaError, "out of range");
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj(-362440)), TimedeltaError, "out of range");
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj(362440.5)), TimedeltaError, "out of range");
	SEE(Datetime::timedelta_to_double(Obj("+01:30:00")), 5400.0);
	SEE(Datetime::timedelta_to_double(Obj("-00:00:30")), -30.0);
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj("not a delta")), TimedeltaError, "");
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj(false)), TimedeltaError, "Timedelta must be numeric or string");
	CHECK_THROWS_WITH(Datetime::timedelta_to_double(Obj::MAP()), TimedeltaError, "Timedelta must be numeric or string");
}

TEST_MAIN(test_numbers_and_strings, test_timestamp_range, test_object_parts, test_object_part_errors, test_object_time_is_range_checked,
          test_time_strings, test_zone_is_the_same_instant, test_time_string_errors, test_time_to_double, test_timedelta_to_double)
