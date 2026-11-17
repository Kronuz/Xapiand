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

// D92 follow-ups: numeric text reads as JSON numbers do (review C5), and the _term filter
// aggregation reads values through Serialize::exact like a search (review C6).

#include "test_harness.h"

#include <memory>
#include <string>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "serialize.h"

static bool refuses(FieldType type, const char* text) {
	try {
		Serialize::exact("f", type, MsgPack(text));
	} catch (const ClientError& exc) {
		const std::string message = exc.get_message();
		return message.find("'f'") != std::string::npos && message.find(text) != std::string::npos;
	}
	return false;
}

// A1: text on a numeric field is a JSON number or a 400.
static void test_numeric_text_is_a_json_number() {
	for (const auto type : {FieldType::integer, FieldType::positive, FieldType::floating}) {
		for (const auto* text : {"0x10", "+12", " 12", "12 ", "012", ".5", "1.", "1e", "", "-"}) {
			CHECK(refuses(type, text));
		}
	}
	CHECK_EQ(Serialize::exact("f", FieldType::integer, MsgPack("-12")).to_string(), "-12");
	CHECK_EQ(Serialize::exact("f", FieldType::integer, MsgPack("12.0")).to_string(), "12");
	CHECK_EQ(Serialize::exact("f", FieldType::positive, MsgPack("12")).to_string(), "12");
	CHECK_EQ(Serialize::exact("f", FieldType::floating, MsgPack("1.5e2")).f64(), 150.0);
	CHECK_EQ(Serialize::exact("f", FieldType::floating, MsgPack("-0.5")).f64(), -0.5);
}

static std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({ { "_type", "integer" }, { "_slot", 1001 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

static bool filter_refuses(MsgPack value) {
	MsgPack term = MsgPack::MAP();
	term["age"] = std::move(value);
	MsgPack body = MsgPack::MAP();
	body["_filter"] = MsgPack({ { "_term", std::move(term) } });
	body["_aggs"] = MsgPack({ { "n", MsgPack({ { "_count", MsgPack({ { "_field", "age" } }) } }) } });
	MsgPack config = MsgPack::MAP();
	config["_aggs"] = MsgPack({ { "x", std::move(body) } });
	try {
		AggregationMatchSpy spy(config, make_schema());
	} catch (const ClientError& exc) {
		return std::string(exc.get_message()).find("'age'") != std::string::npos;
	}
	return false;
}

// A2: the _term filter takes 12.0 on an integer field and refuses 12.5, as a search does.
static void test_term_filter_reads_exactly() {
	CHECK(!filter_refuses(MsgPack(12.0)));
	CHECK(!filter_refuses(MsgPack(12)));
	CHECK(filter_refuses(MsgPack(12.5)));
	CHECK(filter_refuses(MsgPack("0x10")));
	MsgPack list = MsgPack::ARRAY();
	list.push_back(MsgPack(1));
	list.push_back(MsgPack(1.5));
	CHECK(filter_refuses(list));
}

TEST_MAIN(test_numeric_text_is_a_json_number, test_term_filter_reads_exactly)
