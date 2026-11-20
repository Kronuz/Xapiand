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


// D130: `_date` is a cast like `_datetime`, `_multiconvex` names a geometry, and `_lua` is a script, refused as a value
// with a 400 that says so.

#include "test_harness.h"

#include <cstdio>
#include <string>
#include <utility>

#include "cast.h"
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

void test_date_is_served() {
	const auto day = Serialize::timestamp(86400.0);
	CHECK(Cast::get_field_type("_date") == FieldType::date);
	CHECK(Serialize::guess_type(js(R"({"_date":"1970-01-02"})")) == FieldType::date);
	for (const char* json : { R"({"_date":"1970-01-02"})", R"({"_date":86400})" }) {
		std::pair<FieldType, std::string> got;
		try {
			got = Serialize::guess_serialize(js(json));
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  FAIL: guess_serialize(%s) threw: %s\n", json, e.what());
		}
		CHECK(got.first == FieldType::date);
		CHECK_EQ(got.second, day);
		for (auto type : { FieldType::date, FieldType::datetime }) {
			try {
				CHECK_EQ(Serialize::object(spc(type), js(json)), day);
			} catch (const std::exception& e) {
				std::fprintf(stderr, "  FAIL: object(%s) threw: %s\n", json, e.what());
				CHECK(false);
			}
		}
	}
	CHECK_EQ(Cast::cast(js(R"({"_date":"1970-01-02"})")), Cast::cast(js(R"({"_datetime":"1970-01-02"})")));
}

void test_multiconvex_is_a_geometry() {
	CHECK(Cast::get_field_type("_multiconvex") == FieldType::geo);
	CHECK(Serialize::guess_type(js(R"({"_multiconvex":[]})")) == FieldType::geo);
	const auto value = js(R"({"_multiconvex":[]})");
	CHECK_EQ(Cast::cast(value), value);
}

template <typename E, typename F>
void refused_as_script(const char* what, F&& f) {
	bool threw = false;
	try {
		f();
	} catch (const E& e) {
		threw = true;
		const std::string message = e.get_message();
		if (message.find("_lua") == std::string::npos || message.find("script") == std::string::npos) {
			std::fprintf(stderr, "  FAIL: %s: message does not name _lua as a script: %s\n", what, message.c_str());
			CHECK(false);
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "  FAIL: %s threw another error: %s\n", what, e.what());
	}
	CHECK(threw);
}

void test_lua_is_refused_as_a_script() {
	const auto value = js(R"({"_lua":"return 1"})");
	refused_as_script<CastError>("Cast::cast", [&] { (void)Cast::cast(value); });
	refused_as_script<SerializationError>("Serialize::object", [&] { (void)Serialize::object(spc(FieldType::keyword), value); });
	refused_as_script<SerializationError>("Serialize::MsgPack", [&] { (void)Serialize::MsgPack(spc(FieldType::integer), value); });
	refused_as_script<SerializationError>("Serialize::guess_type", [&] { (void)Serialize::guess_type(value); });
	refused_as_script<SerializationError>("Serialize::guess_serialize", [&] { (void)Serialize::guess_serialize(value); });
	// A script field still reads `_lua` as its type.
	CHECK(Cast::get_field_type("_lua") == FieldType::script);
}

}  // namespace

TEST_MAIN(test_date_is_served, test_multiconvex_is_a_geometry, test_lua_is_refused_as_a_script)
