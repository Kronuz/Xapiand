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

// A cast object serializes the same whether its type is guessed or the field's: `{"_time": ...}` and
// `{"_timedelta": ...}` are a time and a timedelta, not a datetime.

#include "test_harness.h"

#include <string>
#include <utility>

#include "database/schema.h"
#include "database/utils.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "rapidjson/document.h"
#include "serialize.h"

namespace {

MsgPack js(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

required_spc_t spc(FieldType type) {
	required_spc_t s;
	s.set_type(type);
	return s;
}

void is(const std::string& json, FieldType type, const std::string& bytes) {
	const auto value = js(json);
	std::pair<FieldType, std::string> got;
	try {
		got = Serialize::guess_serialize(value);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "  FAIL: guess_serialize(%s) threw: %s\n", json.c_str(), e.what());
		CHECK(false);
		return;
	}
	CHECK(got.first == type);
	CHECK_EQ(got.second, bytes);
	// The guessed bytes are the field's: Serialize::object with the guessed type agrees.
	CHECK_EQ(Serialize::object(spc(type), value), bytes);
}

void test_time_text_casts() {
	is(R"({"_time":"10:30:00"})", FieldType::time, Serialize::time(37800.0));
	is(R"({"_time":"00:00:00.5"})", FieldType::time, Serialize::time(0.5));
	is(R"({"_time":3600})", FieldType::time, Serialize::time(3600.0));
	is(R"({"_time":3600.25})", FieldType::time, Serialize::time(3600.25));
}

void test_timedelta_text_casts() {
	is(R"({"_timedelta":"+01:30:00"})", FieldType::timedelta, Serialize::timedelta(5400.0));
	is(R"({"_timedelta":"-01:30:00"})", FieldType::timedelta, Serialize::timedelta(-5400.0));
	is(R"({"_timedelta":3600})", FieldType::timedelta, Serialize::timedelta(3600.0));
	is(R"({"_timedelta":-3600})", FieldType::timedelta, Serialize::timedelta(-3600.0));
}

// A value out of the type's range is refused, as the field's serializer refuses it.
void test_out_of_range_casts_are_refused() {
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_time":-1})")), ClientError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_time":1000000000})")), ClientError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_timedelta":1000000000})")), ClientError);
	CHECK_THROW(Serialize::guess_serialize(js(R"({"_time":"not a time"})")), std::exception);
}

// Every other cast object guesses to the bytes the field's serializer writes.
void test_other_casts_agree_with_the_field() {
	is(R"({"_integer":"-5"})", FieldType::integer, Serialize::integer(int64_t(-5)));
	is(R"({"_positive":5})", FieldType::positive, Serialize::positive(uint64_t(5)));
	is(R"({"_float":2.5})", FieldType::floating, Serialize::floating(2.5));
	is(R"({"_boolean":true})", FieldType::boolean, std::string("t"));
	is(R"({"_keyword":"kw"})", FieldType::keyword, std::string("kw"));
	is(R"({"_text":"tx"})", FieldType::text, std::string("tx"));
	is(R"({"_uuid":"3c0f2be3-ff4f-40ab-b157-c51a81eff176"})", FieldType::uuid, Serialize::uuid("3c0f2be3-ff4f-40ab-b157-c51a81eff176"));
	is(R"({"_datetime":"1970-01-02"})", FieldType::datetime, Serialize::timestamp(86400.0));
	is(R"({"_datetime":86400})", FieldType::datetime, Serialize::timestamp(86400.0));
	is(R"({"_point":{"_latitude":19.70,"_longitude":-101.19}})", FieldType::geo, Serialize::geospatial(std::string_view("POINT(-101.19 19.70)")));
	is(R"j({"_ewkt":"POINT(-101.19 19.70)"})j", FieldType::geo, Serialize::geospatial(std::string_view("POINT(-101.19 19.70)")));
}

}  // namespace

TEST_MAIN(test_time_text_casts, test_timedelta_text_casts, test_out_of_range_casts_are_refused, test_other_casts_agree_with_the_field)
