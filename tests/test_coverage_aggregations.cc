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


// Coverage tests for src/aggregations: the request parser refuses what it should with the error callers map to
// HTTP 400 (reserved but unimplemented kinds, unknown kinds and names, malformed filters, fields without values,
// nesting past the limit), and the match spy's own surface (name, description, clone, serialized forms, merging)
// returns exactly the values below.

#include "test_harness.h"

#include <cstdio>
#include <memory>
#include <string>

#include "aggregations/aggregations.h"
#include "aggregations/exception.h"
#include "database/schema.h"
#include "database/utils.h"
#include "engine/error.h"
#include "exception_engine.h"
#include "length.h"
#include "msgpack.h"
#include "rapidjson/document.h"
#include "serialize.h"

namespace {

MsgPack parse(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({ { "_type", "integer" }, { "_slot", 1001 } });
	props["score"] = MsgPack({ { "_type", "floating" }, { "_slot", 1002 } });
	props["cat"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1003 }, { "_index", "field_values" } });
	props["body"] = MsgPack({ { "_type", "text" }, { "_slot", 1004 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

bool refuses(const std::string& json) {
	try {
		AggregationMatchSpy spy(parse(json), make_schema());
	} catch (const AggregationError&) {
		return true;
	}
	return false;
}

void test_parser_refusals() {
	const char* bad[] = {
		// Reserved words the engine does not serve yet.
		R"({"_aggs":{"x":{"_cardinality":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_geo_bounds":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_geo_centroid":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_percentiles":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_percentiles_rank":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_scripted_metric":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_date_histogram":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_date_range":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_geo_distance":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_geo_trixels":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_missing":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_ip_range":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_geo_ip":{"_field":"age"}}}})",
		// Unknown kind, malformed names and shapes.
		R"({"_aggs":{"_x":{"_count":{"_field":"age"}}}})",
		R"({"_aggs":{"x":5}})",
		R"({"_aggs":5})",
		R"({"_aggregations":5})",
		// A metric needs its field, as a string.
		R"({"_aggs":{"x":{"_count":5}}})",
		R"({"_aggs":{"x":{"_count":{}}}})",
		R"({"_aggs":{"x":{"_count":{"_field":5}}}})",
		R"({"_aggs":{"x":{"_sum":{"_field":["age"]}}}})",
		// Terms need stored values: none on analyzed text.
		R"({"_aggs":{"x":{"_terms":{"_field":"body"}}}})",
		R"({"_aggs":{"x":{"_terms":{}}}})",
		R"({"_aggs":{"x":{"_terms":{"_field":5}}}})",
		R"({"_aggs":{"x":{"_terms":5}}})",
		R"({"_aggs":{"x":{"_values":{}}}})",
		R"({"_aggs":{"x":{"_values":{"_field":5}}}})",
		// A filter is an object holding a term object.
		R"({"_aggs":{"x":{"_filter":5}}})",
		R"({"_aggs":{"x":{"_filter":{}}}})",
		R"({"_aggs":{"x":{"_filter":{"_term":5}}}})",
	};
	for (const char* json : bad) {
		if (!refuses(json)) {
			std::fprintf(stderr, "  FAIL: accepted %s\n", json);
		}
		CHECK(refuses(json));
	}
}

void test_parser_accepts() {
	// Both spellings of the container key, the deprecated kind names and a nested kind chain.
	for (const char* json : {
		R"({"_aggs":{"x":{"_count":{"_field":"age"}}}})",
		R"({"_aggregations":{"x":{"_count":{"_field":"age"}}}})",
		R"({"_aggs":{"x":{"_value":{"_field":"cat"}}}})",
		R"({"_aggs":{"x":{"_term":{"_field":"cat"}}}})",
		R"({"_aggs":{"x":{"_terms":{"_field":"cat"},"_aggs":{"y":{"_avg":{"_field":"score"}}}}}})",
		R"({"_aggs":{"x":{"_filter":{"_term":{"cat":"a"}},"_aggs":{"y":{"_count":{"_field":"age"}}}}}})",
		R"({"_aggs":{"x":{"_filter":{"_term":{"cat":["a","b"]}}}}})",
	}) {
		if (refuses(json)) {
			std::fprintf(stderr, "  FAIL: refused %s\n", json);
		}
		CHECK_FALSE(refuses(json));
	}
}

void test_depth_limit() {
	// A chain of `levels` nested aggregations; the sixteenth is the last allowed.
	auto chain = [](size_t levels) {
		std::string json = R"({"_aggs":{"x":{"_count":{"_field":"age"}}}})";
		for (size_t i = 1; i < levels; ++i) {
			json = R"({"_aggs":{"x":{"_terms":{"_field":"cat"},)" + json.substr(1, json.size() - 2) + "}}}";
		}
		return json;
	};
	CHECK_FALSE(refuses(chain(AGGS_MAX_DEPTH)));
	CHECK(refuses(chain(AGGS_MAX_DEPTH + 1)));
	CHECK_THROW(AggregationMatchSpy::check_depth(parse(chain(AGGS_MAX_DEPTH + 1))), AggregationError);
	AggregationMatchSpy::check_depth(parse(chain(AGGS_MAX_DEPTH)));
	// A configuration that holds no aggregations is no deeper than nothing.
	AggregationMatchSpy::check_depth(parse("{}"));
	AggregationMatchSpy::check_depth(parse("5"));
	AggregationMatchSpy::check_depth(parse(R"({"_aggs":5})"));
}

void test_spy_surface() {
	const auto json = R"({"_aggs":{"n":{"_count":{"_field":"age"}},"s":{"_sum":{"_field":"score"}}}})";
	const auto config = parse(json);
	AggregationMatchSpy spy(config, make_schema());
	CHECK_EQ(spy.name(), std::string("AggregationMatchSpy"));
	CHECK_EQ(spy.description(), "AggregationMatchSpy(" + config.to_string() + ")");

	// With no document seen, every aggregation reports a zero count.
	const auto& result = spy.get_aggregation();
	CHECK_EQ(result.at("_aggregations").at("_doc_count").i64(), static_cast<int64_t>(0));
	CHECK(result.at("_aggregations").find("n") != result.at("_aggregations").end());
	CHECK(result.at("_aggregations").find("s") != result.at("_aggregations").end());

	// Clone and the serialized form both rebuild the same configuration.
	const auto clone = spy.clone();
	CHECK_EQ(clone->description(), spy.description());
	const auto rebuilt = spy.unserialize(spy.serialize());
	CHECK_EQ(rebuilt->description(), spy.description());
	CHECK_EQ(rebuilt->serialize(), spy.serialize());

	// A serialized form that is not a pair is refused, with the engine's invalid error.
	bool invalid = false;
	try {
		(void)spy.unserialize("");
	} catch (const engine::Error& e) {
		invalid = e.kind == engine::errc::invalid && e.reason() == "Bad serialized AggregationMatchSpy";
	}
	CHECK(invalid);
	invalid = false;
	try {
		(void)spy.unserialize("\x05zz");
	} catch (const engine::Error& e) {
		invalid = e.kind == engine::errc::invalid && e.reason() == "Bad serialized AggregationMatchSpy";
	}
	CHECK(invalid);
}

void test_merge() {
	const auto json = R"({"_aggs":{"n":{"_count":{"_field":"age"}}}})";
	AggregationMatchSpy a(parse(json), make_schema()), b(parse(json), make_schema());
	// Merging two empty results keeps the counts at zero, as bytes and as objects.
	a.merge_results(b.serialize_results());
	a.merge_results(b);
	CHECK_EQ(a.get_aggregation().at("_aggregations").at("_doc_count").i64(), static_cast<int64_t>(0));
	// The serialized results of a spy start with the document count, then (name, results) pairs.
	const auto bytes = b.serialize_results();
	const char* p = bytes.data();
	const char* end = p + bytes.size();
	CHECK_EQ(unserialize_length(&p, end), 0ULL);
	CHECK_EQ(std::string(unserialize_string(&p, end)), std::string("n"));
	const auto data = std::string(unserialize_string(&p, end));
	CHECK(p == end);

	// A result naming an aggregation the spy does not have, or carrying bytes past its own, is refused.
	const auto stranger = serialize_length(0) + serialize_string("zzz") + serialize_string(data);
	CHECK_THROW(a.merge_results(stranger), SerializationError);
	const auto junk = serialize_length(0) + serialize_string("n") + serialize_string(data + "x");
	CHECK_THROW(a.merge_results(junk), SerializationError);
}

}  // namespace

TEST_MAIN(test_parser_refusals, test_parser_accepts, test_depth_limit, test_spy_surface, test_merge)
