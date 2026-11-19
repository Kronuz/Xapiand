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


// Coverage tests for src/y2j: the YAML scalar and collection rules (core schema resolution, explicit tags, quoted
// styles, hex and octal integers, nesting limit, anchors and complex keys refused) decode to exactly this JSON.

#include "test_harness.h"

#include <cmath>
#include <cstring>
#include <string>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "y2j/y2j.h"

namespace {

struct Result {
	bool ok;
	std::string json;
	std::string message;
	size_t line;
};

Result load(const std::string& yaml, size_t max_depth = 0) {
	const char* message = nullptr;
	size_t line = 0;
	auto doc = y2j::yamlParseBytes(yaml.data(), yaml.size(), &message, &line, max_depth);
	Result r{ message == nullptr, {}, message ? message : "", line };
	if (r.ok) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		r.json = buffer.GetString();
	}
	return r;
}

void is(const std::string& yaml, const std::string& json) {
	const auto r = load(yaml);
	if (!r.ok || r.json != json) {
		std::fprintf(stderr, "  YAML %s -> ok=%d '%s' (%s), wanted '%s'\n", yaml.c_str(), r.ok, r.json.c_str(), r.message.c_str(), json.c_str());
	}
	CHECK(r.ok);
	CHECK_EQ(r.json, json);
}

void refused(const std::string& yaml, const std::string& message, size_t max_depth = 0) {
	const auto r = load(yaml, max_depth);
	if (r.ok || r.message != message) {
		std::fprintf(stderr, "  YAML %s -> ok=%d '%s', wanted refusal '%s'\n", yaml.c_str(), r.ok, r.message.c_str(), message.c_str());
	}
	CHECK_FALSE(r.ok);
	CHECK_EQ(r.message, message);
}

void test_plain_scalars() {
	is("a: 1\nb: 2.5\nc: true\nd: ~\ne: x\nf: null\n", "{\"a\":1,\"b\":2.5,\"c\":true,\"d\":null,\"e\":\"x\",\"f\":null}");
	is("- True\n- TRUE\n- False\n- FALSE\n- Null\n- NULL\n- tru\n- fals\n- nul\n- nope\n- Tx\n- Fx\n",
		"[true,true,false,false,null,null,\"tru\",\"fals\",\"nul\",\"nope\",\"Tx\",\"Fx\"]");
	is("- 0x1F\n- 0o17\n- -12\n- +7\n- 010\n- 1e3\n- .5\n- 1.\n- abc1\n- 12abc\n", "[31,15,-12,7,10,1000.0,0.5,1.0,\"abc1\",\"12abc\"]");
	is("- 0x\n- 0xZZ\n- 0o9\n- 0b1\n", "[\"0x\",\"0xZZ\",\"0o9\",\"0b1\"]");
	is("a:\nb: ''\n", "{\"a\":null,\"b\":\"\"}");
	is("- 9223372036854775807\n- -9223372036854775808\n", "[9223372036854775807,-9223372036854775808]");
}

void test_quoted_and_blocks() {
	is("a: 'true'\nb: \"1\"\nc: \"null\"\n", "{\"a\":\"true\",\"b\":\"1\",\"c\":\"null\"}");
	is("t: |\n  line1\n  line2\nf: >\n  folded\n  text\n", "{\"t\":\"line1\\nline2\\n\",\"f\":\"folded text\\n\"}");
	is("k: \"tab\\there\\u00e9\"\n", "{\"k\":\"tab\\thereé\"}");
}

void test_collections() {
	is("{a: 1, b: [1, 2, {c: d}], e: []}", "{\"a\":1,\"b\":[1,2,{\"c\":\"d\"}],\"e\":[]}");
	is("[]", "[]");
	is("{}", "{}");
	is("x:\n  - 1\n  - y: 2\n    z: [3]\n  -\n    - 4\n", "{\"x\":[1,{\"y\":2,\"z\":[3]},[4]]}");
	is("7", "7");
	is("just text", "\"just text\"");
}

void test_tags() {
	is("- !!str 123\n- !!int '42'\n- !!int 0x10\n- !!float '1.5'\n- !!bool true\n- !!bool False\n- !!null ~\n- !!null null\n",
		"[\"123\",42,16,1.5,true,false,null,null]");
	const auto r = load("a: !!float .nan\n");
	CHECK(r.ok);
	// NaN and infinities have no JSON text, so check the typed values instead of the writer's output.
	const char* message = nullptr;
	size_t line = 0;
	const std::string yaml = "[!!float .nan, !!float .NaN, !!float .NAN, !!float -.inf, !!float +.inf, !!float 1e2]";
	auto doc = y2j::yamlParseBytes(yaml.data(), yaml.size(), &message, &line);
	REQUIRE(doc.IsArray());
	REQUIRE_EQ(doc.Size(), static_cast<rapidjson::SizeType>(6));
	CHECK(std::isnan(doc[0].GetDouble()));
	CHECK(std::isnan(doc[1].GetDouble()));
	CHECK(std::isnan(doc[2].GetDouble()));
	CHECK(std::isinf(doc[3].GetDouble()) && doc[3].GetDouble() < 0);
	CHECK(std::isinf(doc[4].GetDouble()) && doc[4].GetDouble() > 0);
	CHECK_EQ(doc[5].GetDouble(), 100.0);
	refused("- !!int abc\n", "Scalar tag could not be resolved.");
	refused("- !!float abc\n", "Scalar tag could not be resolved.");
	refused("- !!bool maybe\n", "Scalar tag could not be resolved.");
	refused("- !!null x\n", "Scalar tag could not be resolved.");
	refused("- !!binary x\n", "Scalar tag could not be resolved.");
}

void test_refusals() {
	refused("a: &x 1\nb: *x\n", "YAML anchors and aliases are not supported");
	refused("- &s [1]\n", "YAML anchors and aliases are not supported");
	refused("&m {a: 1}\n", "YAML anchors and aliases are not supported");
	refused("*x\n", "YAML anchors and aliases are not supported");
	refused("", "YAML has no document");
	refused("# only a comment\n", "YAML has no document");
	// Syntax errors carry libyaml's own problem text and a line.
	const auto r = load("a: [1, 2\nb: 3\n");
	CHECK_FALSE(r.ok);
	CHECK(!r.message.empty());
	CHECK_FALSE(load("a: 'unterminated\n").ok);
	CHECK_FALSE(load("\t- tab\n").ok);
	// Nesting: the limit counts open sequences and mappings.
	CHECK(load("[[1]]", 2).ok);
	refused("[[[1]]]", "YAML is nested too deeply", 2);
	refused("a: {b: {c: 1}}", "YAML is nested too deeply", 2);
	CHECK(load("a: {b: 1}", 2).ok);
	CHECK(load("[[[[1]]]]", 0).ok);
	const auto deep = load("[[[1]]]", 1);
	CHECK_EQ(deep.line, static_cast<size_t>(0));
	const auto lined = load("a: 1\nb: [[2]]\n", 2);
	CHECK_FALSE(lined.ok);
	CHECK_EQ(lined.line, static_cast<size_t>(1));
}

void test_complex_keys() {
	// A key that is itself a collection decodes to the fixed marker key.
	// (the marker key carries its terminating NUL, which the writer prints as \u0000)
	const auto seq = load("? [a, b]\n: 1\nx: 2\n");
	CHECK(seq.ok);
	CHECK_EQ(seq.json.rfind("{\"COMPLEX YAML KEYS ARE NOT SUPPORTED", 0), static_cast<size_t>(0));
	CHECK(seq.json.find(":1,\"x\":2}") != std::string::npos);
	const auto map = load("? {a: 1}\n: 1\n");
	CHECK(map.ok);
	CHECK_EQ(map.json.rfind("{\"COMPLEX YAML KEYS ARE NOT SUPPORTED", 0), static_cast<size_t>(0));
	CHECK(map.json.find(":1}") != std::string::npos);
	const auto nested = load("? [[a], {b: [c]}]\n: 1\nx: 2\n");
	CHECK(nested.ok);
	CHECK_EQ(nested.json.rfind("{\"COMPLEX YAML KEYS ARE NOT SUPPORTED", 0), static_cast<size_t>(0));
	CHECK(nested.json.find(":1,\"x\":2}") != std::string::npos);
}

void test_no_error_outputs() {
	// The error pointers are optional.
	auto doc = y2j::yamlParseBytes("a: &x 1", 7, nullptr, nullptr);
	CHECK(doc.IsNull());
	auto bad = y2j::yamlParseBytes("[1", 2, nullptr, nullptr);
	CHECK(bad.IsNull());
	auto empty = y2j::yamlParseBytes("", 0, nullptr, nullptr);
	CHECK(empty.IsNull());
	auto deep = y2j::yamlParseBytes("[[1]]", 5, nullptr, nullptr, 1);
	CHECK(deep.IsNull());
}

}  // namespace

TEST_MAIN(test_plain_scalars, test_quoted_and_blocks, test_collections, test_tags, test_refusals, test_complex_keys, test_no_error_outputs)
