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


// An aggregation type the dispatch does not know is refused with a 400 naming it, not taken as `_aggs`.

#include "test_harness.h"

#include <memory>
#include <string>

#include "aggregations/aggregations.h"
#include "aggregations/exception.h"
#include "database/schema.h"
#include "database/utils.h"
#include "msgpack.h"
#include "rapidjson/document.h"

namespace {

MsgPack js(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({ { "_type", "integer" }, { "_slot", 1001 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

void refused(const std::string& json, const std::string& word) {
	const auto schema = make_schema();
	bool threw = false;
	try {
		Aggregation aggregation(js(json), schema);
	} catch (const AggregationError& e) {
		threw = true;
		const std::string message = e.get_message();
		CHECK(message.find(word) != std::string::npos);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "  FAIL: %s threw a non-400: %s\n", json.c_str(), e.what());
	}
	if (!threw) {
		std::fprintf(stderr, "  FAIL: %s was accepted\n", json.c_str());
	}
	CHECK(threw);
}

void accepted(const std::string& json) {
	const auto schema = make_schema();
	try {
		Aggregation aggregation(js(json), schema);
		CHECK(true);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "  FAIL: %s threw: %s\n", json.c_str(), e.what());
		CHECK(false);
	}
}

void test_unknown_types_are_refused() {
	refused(R"({"_aggs":{"x":{"_bogus":{"_field":"age"}}}})", "_bogus");
	refused(R"({"_aggs":{"x":{"bogus":{"_field":"age"}}}})", "bogus");
	refused(R"({"_aggs":{"x":{"_counts":{"_field":"age"}}}})", "_counts");
	refused(R"({"_aggs":{"x":{"_aggs":{},"_bogus":{"_field":"age"}}}})", "_bogus");
	refused(R"({"_aggregations":{"x":{"#note":"n","_nope":{"_field":"age"}}}})", "_nope");
}

// A nested unknown type is refused at construction, before any bucket forms, under every bucket kind and at any depth.
void test_nested_unknown_types_are_refused_before_any_bucket_forms() {
	const char* kinds[] = {
		R"("_terms":{"_field":"age"})",
		R"("_values":{"_field":"age"})",
		R"("_histogram":{"_field":"age","_interval":10})",
		R"("_range":{"_field":"age","_ranges":[{"_to":10}]})",
	};
	for (const char* kind : kinds) {
		const std::string head = std::string(R"({"_aggs":{"x":{)") + kind;
		refused(head + R"(,"_aggs":{"y":{"_bogus":{"_field":"age"}}}}}})", "_bogus");
		refused(head + R"(,"_aggs":{"y":{"_terms":{"_field":"age"},"_aggs":{"z":{"_nope":{"_field":"age"}}}}}}}})", "_nope");
		accepted(head + R"(,"_aggs":{"y":{"_max":{"_field":"age"}}}}}})");
	}
}


// The type may follow the sub-aggregations or a comment, as before.
void test_known_types_still_dispatch() {
	accepted(R"({"_aggs":{"x":{"_count":{"_field":"age"}}}})");
	accepted(R"({"_aggs":{"x":{"_aggs":{},"_count":{"_field":"age"}}}})");
	accepted(R"({"_aggs":{"x":{"_aggregations":{},"_count":{"_field":"age"}}}})");
	accepted(R"({"_aggs":{"x":{"#note":"n","_count":{"_field":"age"}}}})");
	accepted(R"({"_aggs":{"x":{"_terms":{"_field":"age"},"_aggs":{"y":{"_max":{"_field":"age"}}}}}})");
	accepted(R"({"_aggs":{"x":{"_count":{"_field":"age"},"#note":"n"}}})");
	accepted(R"({"_aggs":{"x":{"_terms":{"_field":"age"},"#note":"n","_aggs":{"y":{"_max":{"_field":"age"}}}}}})");
}

// An aggregation holds exactly one type: none (empty, comment-only, `_aggs`-only) or a second one is a 400 naming it.
void test_typeless_and_multi_type_aggregations_are_refused() {
	refused(R"({"_aggs":{"empty_agg":{}}})", "empty_agg");
	refused(R"({"_aggs":{"note_agg":{"#note":"n"}}})", "note_agg");
	refused(R"({"_aggs":{"subs_agg":{"_aggs":{"y":{"_count":{"_field":"age"}}}}}})", "subs_agg");
	refused(R"({"_aggs":{"two_agg":{"_count":{"_field":"age"},"_max":{"_field":"age"}}}})", "two_agg");
	refused(R"({"_aggs":{"x":{"_count":{"_field":"age"},"_bogus":1}}})", "_bogus");
	refused(R"({"_aggs":{"y":{"_max":{"_field":"age"}},"late_agg":{}}})", "late_agg");
	refused(R"({"_aggs":{"x":{"_terms":{"_field":"age"},"_aggs":{"inner_agg":{}}}}})", "inner_agg");
}

// `_missing` is not an aggregation type: it is refused like any unknown one, not as "reserved". As a key inside a bucket body it is an unknown key, ignored like the others.
void test_missing_is_an_unknown_type() {
	refused(R"({"_aggs":{"m":{"_missing":{"_field":"age"}}}})", "is not valid for");
	accepted(R"({"_aggs":{"m":{"_terms":{"_field":"age","_missing":"N/A"}}}})");
}

}  // namespace

TEST_MAIN(test_unknown_types_are_refused, test_nested_unknown_types_are_refused_before_any_bucket_forms, test_known_types_still_dispatch, test_typeless_and_multi_type_aggregations_are_refused, test_missing_is_an_unknown_type)
