/*
 * Copyright (c) 2026 Dubalu LLC
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


// Unit tests for the host-side text parser: queryparser's tree, lowered to an engine query. The
// recorded A/B decisions (parse-ab.jsonl, the 706 rows the first run inside Xapiand ruled on) must
// come out as recorded, and the lowering must give the tree queryparser itself describes.

#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "engine/query.h"
#include "engine/text.h"
#include "database/utils.h"
#include "exception_engine.h"
#include "jsonl.hh"

namespace {

using engine::Op;
using engine::Query;
using engine::Stem;
namespace text = engine::text;

// The stop words are read from the source tree.
const int data_path = (::setenv("XAPIAND_DATA_PATH", XAPIAND_SOURCE_DIR, 1), 0);

constexpr unsigned base = text::PHRASE | text::LOVEHATE | text::FUZZY;

engine::TextConfig
config(unsigned flags, Op op = Op::INVALID, const char* stop = "", const char* stem = "", Stem strategy = Stem::none)
{
	engine::TextConfig c;
	c.field = "XTfield";
	c.default_op = op;
	c.flags = flags;
	c.stop_language = stop;
	c.stem_language = stem;
	c.stem = strategy;
	c.partial_max_expansion = 50;
	c.fuzzy_max_expansion = 0;
	return c;
}

// The description queryparser gives its own tree for the text.
std::string
described(const std::string& text, const engine::TextConfig& c)
{
	std::string tree;
	engine::parse_text(text, c, &tree);
	return tree;
}

// The nodes of a lowered query that stand for what the description counts: terms with a position,
// wildcards and edit-distance expansions.
struct Leaves {
	std::size_t terms = 0, wildcards = 0, fuzzy = 0;
};

Leaves
leaves_of(const Query& query)
{
	Leaves leaves;
	std::vector<const engine::QueryNode*> stack;
	if (query.node() != nullptr) { stack.push_back(query.node()); }
	while (!stack.empty()) {
		const auto* node = stack.back();
		stack.pop_back();
		leaves.terms += node->kind == engine::QueryKind::term && node->pos != 0;
		leaves.wildcards += node->kind == engine::QueryKind::wildcard;
		leaves.fuzzy += node->kind == engine::QueryKind::edit_distance;
		for (const auto& sub : node->subqueries) {
			if (sub.node() != nullptr) { stack.push_back(sub.node()); }
		}
	}
	return leaves;
}

std::size_t
count_of(const std::string& text, const std::string& what)
{
	std::size_t n = 0;
	for (auto at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) { ++n; }
	return n;
}

// One row of a vector file as a text configuration; false when it sets a flag the engine has no word for.
bool
row_config(std::map<std::string, std::string>& r, engine::TextConfig& c)
{
	static const std::map<std::string, unsigned> flags = {
		{ "boolean", text::BOOLEAN }, { "phrase", text::PHRASE }, { "lovehate", text::LOVEHATE },
		{ "wildcard", text::WILDCARD }, { "wildcard_single", text::WILDCARD_SINGLE },
		{ "wildcard_multi", text::WILDCARD_MULTI }, { "partial", text::PARTIAL }, { "fuzzy", text::FUZZY },
		{ "cjk_ngram", text::CJK_NGRAM },
	};
	static const std::map<std::string, Op> ops = {
		{ "unset", Op::INVALID }, { "and", Op::AND }, { "or", Op::OR }, { "near", Op::NEAR }, { "phrase", Op::PHRASE },
		{ "synonym", Op::SYNONYM }, { "max", Op::MAX },
	};
	static const std::map<std::string, Stem> stems = {
		{ "unset", Stem::none }, { "none", Stem::none }, { "some", Stem::some }, { "all", Stem::all }, { "all_z", Stem::all_z },
	};
	c = engine::TextConfig();
	std::istringstream fs(r["flags"]);
	for (std::string f; std::getline(fs, f, '|');) {
		auto it = flags.find(f);
		if (it == flags.end()) {
			return false;
		}
		c.flags |= it->second;
	}
	auto op = ops.find(r["default_op"]);
	auto stem = stems.find(r["stem"]);
	if (op == ops.end() || stem == stems.end() || r["stop_strategy"] != "default") {
		return false;
	}
	c.default_op = op->second;
	c.stem = stem->second;
	c.stop_language = r["stopper"] == "none" ? "" : r["stopper"];
	c.stem_language = r["stem_lang"] == "none" ? "" : r["stem_lang"];
	c.field = r["prefix"];
	c.partial_max_expansion = static_cast<engine::termcount>(std::stoul(r["partial_max"]));
	c.fuzzy_max_expansion = static_cast<engine::termcount>(std::stoul(r["fuzzy_max"]));
	return true;
}

// The rows of a vector file, or none when it cannot be opened.
std::vector<std::map<std::string, std::string>>
rows(const char* file)
{
	std::vector<std::map<std::string, std::string>> result;
	std::ifstream in(std::string(QUERYPARSER_VECTORS) + "/" + file);
	for (std::string line; std::getline(in, line);) {
		result.push_back(jsonl::row(line));
	}
	return result;
}

}  // namespace


// The recorded rows that lower to a SYNONYM of anything but words.
constexpr std::size_t INVALID_ROWS = 4;

void
test_ab_decisions_stand()
{
	// Every recorded decision: queryparser's tree reads as recorded, and the lowering errors as recorded.
	auto all = rows("parse-ab.jsonl");
	REQUIRE_EQ(all.size(), 706U);
	std::size_t errors = 0, invalid = 0;
	for (auto& r : all) {
		engine::TextConfig c;
		if (r["default_op"] == "elite_set") {
			continue;  // the operator is not offered; the library still records rows for it
		}
		REQUIRE(row_config(r, c));
		if (r["error"].empty()) {
			try {
				const auto tree = described(r["query"], c);
				CHECK_EQ(tree, r["description"]);
			} catch (const QueryParserError& exc) {
				// A SYNONYM of anything but words is not a valid query, whatever the library records.
				CHECK_TRUE(exc.kind == engine::errc::syntax);
				CHECK_TRUE(exc.reason().find("is not a valid query") != std::string::npos);
				++invalid;
			}
		} else {
			++errors;
			std::string message;
			try {
				engine::parse_text(r["query"], c);
			} catch (const QueryParserError& exc) {
				CHECK_TRUE(exc.kind == engine::errc::syntax);
				message = exc.reason();
			}
			// The one operand queryparser cannot take is reported as not a valid query.
			const bool operand = r["error"] == "OP_NEAR and OP_PHRASE only currently support leaf subqueries";
			CHECK_EQ(message, operand ? "A PHRASE or NEAR of anything but words is not a valid query" : r["error"]);
		}
	}
	CHECK_EQ(errors, 25U);
	CHECK_EQ(invalid, INVALID_ROWS);
}

void
test_lowering_is_the_tree()
{
	// The engine query has the leaves queryparser's description names, for every row of the larger vector files:
	// a term for each "@position", a wildcard for each WILDCARD and an edit-distance node for each EDIT_DISTANCE.
	std::size_t read = 0, failed = 0;
	for (const char* file : { "parse-v2.jsonl", "parse-v3.jsonl", "parse-ab.jsonl" }) {
		auto all = rows(file);
		REQUIRE(!all.empty());
		for (auto& r : all) {
			engine::TextConfig c;
			if (!row_config(r, c)) {
				continue;
			}
			std::string tree;
			try {
				const auto lowered = engine::parse_text(r["query"], c, &tree);
				const auto leaves = leaves_of(lowered);
				CHECK_EQ(leaves.terms, count_of(tree, "@"));
				CHECK_EQ(leaves.wildcards, count_of(tree, "WILDCARD "));
				CHECK_EQ(leaves.fuzzy, count_of(tree, "EDIT_DISTANCE "));
				++read;
			} catch (const QueryParserError&) {
				++failed;
			}
		}
	}
	CHECK_GT(read, 5000U);
	CHECK_GT(failed, 0U);
}

void
test_hate_groups_terminate()
{
	// With LOVEHATE and no BOOLEAN, a group of only hated items is read again as if none were marked; that once
	// linked the group's last node to itself and the lowering never ended. Each text now reads as plain words.
	struct Case { const char* text; const char* or_tree; const char* and_tree; };
	for (const auto& c : {
		Case{ "-(a b) -c", "Query((XTfielda@1 OR XTfieldb@2 OR XTfieldc@3))", "Query((XTfielda@1 AND XTfieldb@2 AND XTfieldc@3))" },
		Case{ "x -(a b) -c", "Query((XTfieldx@1 AND_NOT (XTfielda@2 OR XTfieldb@3 OR XTfieldc@4)))",
		      "Query((XTfieldx@1 AND_NOT ((XTfielda@2 AND XTfieldb@3) OR XTfieldc@4)))" },
		Case{ "-(a b c) -d -e", "Query((XTfielda@1 OR XTfieldb@2 OR XTfieldc@3 OR XTfieldd@4 OR XTfielde@5))",
		      "Query((XTfielda@1 AND XTfieldb@2 AND XTfieldc@3 AND XTfieldd@4 AND XTfielde@5))" },
	}) {
		for (auto op : { Op::OR, Op::AND }) {
			std::string tree;
			engine::parse_text(c.text, config(text::LOVEHATE, op), &tree);
			const std::string want = op == Op::OR ? c.or_tree : c.and_tree;
			CHECK_TRUE(tree == want);
			if (tree != want) { std::fprintf(stderr, "%s: got %s, want %s\n", c.text, tree.c_str(), want.c_str()); }
		}
	}
}

void
test_errors_are_engine_errors()
{
	// A syntax error is a client error of kind syntax, with queryparser's message.
	try {
		engine::parse_text("AND AND", config(base | text::BOOLEAN, Op::AND));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
		CHECK_FALSE(exc.reason().empty());
	}

	// NEAR over phrases is a client mistake too, no longer a server error.
	try {
		engine::parse_text("\"a b\" \"c d\"", config(base, Op::NEAR));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
		CHECK_EQ(exc.reason(), "A PHRASE or NEAR of anything but words is not a valid query");
	}
	// So is a PHRASE over a wildcard or a fuzzy word, which only a Query built by its factories can reach.
	for (const char* text : { "first se*", "first seen~" }) {
		try {
			engine::parse_text(text, config(base | text::WILDCARD | text::FUZZY, Op::PHRASE));
			CHECK_TRUE(false);
		} catch (const QueryParserError& exc) {
			CHECK_EQ(exc.reason(), "A PHRASE or NEAR of anything but words is not a valid query");
		}
	}
}

void
test_unreadable_settings_are_invalid()
{
	CHECK_THROW(engine::parse_text("quick", config(base, Op::INVALID, "", "no-such-language", Stem::some)), InvalidTextConfigError);
	CHECK_THROW(engine::parse_text("quick", config(base, Op::AND_NOT)), InvalidTextConfigError);
	try {
		engine::parse_text("quick", config(base, Op::FILTER));
	} catch (const InvalidTextConfigError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::invalid);
	}

	// The reason carries the language as given, braces and all.
	try {
		engine::parse_text("quick", config(base, Op::INVALID, "", "no{}such", Stem::some));
		CHECK_TRUE(false);
	} catch (const InvalidTextConfigError& exc) {
		CHECK_EQ(exc.reason(), "Language code no{}such unknown");
	}
}

void
test_text_without_words_matches_nothing()
{
	CHECK_TRUE(engine::parse_text("", config(base)).empty());
	CHECK_TRUE(engine::parse_text(" \t ", config(base)).empty());
	CHECK_TRUE(engine::parse_text("!!! ???", config(base, Op::INVALID, "en")).empty());
	CHECK_FALSE(engine::parse_text("quick", config(base, Op::INVALID, "en")).empty());

	// Stop words go from a group with other words and stay in a group of their own.
	CHECK_EQ(described("the quick", config(base, Op::INVALID, "en")), "Query(XTfieldquick@2)");
	const auto parsed = engine::parse_text("the quick", config(base, Op::INVALID, "en"));
	CHECK_TRUE(parsed.node() != nullptr && parsed.node()->kind == engine::QueryKind::term);
	CHECK_EQ(parsed.node()->field, "XTfield");
	CHECK_EQ(parsed.node()->str, "quick");
	CHECK_EQ(parsed.node()->pos, 2u);
	CHECK_FALSE(engine::parse_text("the and of", config(base, Op::INVALID, "en")).empty());
}

void
test_nesting_is_not_recursive()
{
	// 200,000 brackets: the tree is built and lowered without the call stack.
	const std::string deep = std::string(200000, '(') + "a" + std::string(200000, ')');
	CHECK_FALSE(engine::parse_text(deep, config(base | text::BOOLEAN, Op::AND)).empty());

	// Brackets that each hold an operator: the lowered tree is as deep as the text, up to the cap.
	const auto nest = [](int levels) {
		std::string nested;
		std::string closing;
		for (int i = 0; i < levels; ++i) {
			nested += (i % 2 ? "a OR (" : "a AND (");
			closing += ")";
		}
		return nested + "z" + closing;
	};
	const auto cfg = config(base | text::BOOLEAN, Op::OR);
	const int cap = static_cast<int>(engine::MAX_QUERY_DEPTH);
	const auto lowered = engine::parse_text(nest(cap - 2), cfg);
	CHECK_FALSE(lowered.empty());
	CHECK_TRUE(lowered.depth() <= engine::MAX_QUERY_DEPTH);
	CHECK_FALSE(lowered.description().empty());

	// Past the cap the text is a client error, however deep (and cheap to refuse).
	for (const int levels : { cap + 1, cap * 4, 50000 }) {
		try {
			engine::parse_text(nest(levels), cfg);
			CHECK_TRUE(false);
		} catch (const QueryDepthError& exc) {
			CHECK_TRUE(std::string(exc.what()).find("nested too deeply") != std::string::npos);
			CHECK_TRUE(dynamic_cast<const QueryParserError*>(&exc) != nullptr);
		}
	}
}

void
test_near_of_a_boolean_expression()
{
	// _near over "(barack OR michelle) AND obama" once reached a heap overflow in the parser. NEAR
	// takes words only, so the bracket is a client error: the answer is a 400, not a corrupted heap.
	try {
		engine::parse_text("(barack OR michelle) AND obama", config(base, Op::NEAR));
		CHECK_TRUE(false);
	} catch (const QueryParserError& exc) {
		CHECK_TRUE(exc.kind == engine::errc::syntax);
	}
	// With BOOLEAN the operators join the groups, and NEAR joins only the words between them.
	CHECK_EQ(described("(barack OR michelle) AND obama", config(base | text::BOOLEAN, Op::NEAR)),
		"Query(((XTfieldbarack@1 OR XTfieldmichelle@2) AND XTfieldobama@3))");
}

// A stem is a term of the stems' field ('Z' and the name), whatever the engine: the parser leads it with the
// 'Z' and the field's name, and the seam takes the field apart from the stem.
void
test_stems_are_terms_of_the_stem_field()
{
	for (const auto strategy : { Stem::some, Stem::all_z }) {
		auto cfg = config(base, Op::INVALID, "en", "en", strategy);
		const auto query = engine::parse_text("running", cfg);
		const auto* node = query.node();
		CHECK_TRUE(node != nullptr);
		if (node == nullptr) { continue; }
		// "running" stems to "run": the stem is under the stems' field, bare.
		std::vector<std::pair<std::string, std::string>> seen;
		if (node->kind == engine::QueryKind::term) {
			seen.emplace_back(node->field, node->str);
		} else {
			for (const auto& sub : node->subqueries) { seen.emplace_back(sub.node()->field, sub.node()->str); }
		}
		bool stem_found = false;
		for (const auto& [field, bytes] : seen) {
			if (field == "ZXTfield") { stem_found = true; CHECK_EQ(bytes, "run"); }
			else { CHECK_EQ(field, "XTfield"); }
		}
		CHECK_TRUE(stem_found);
		// The description names the stem under its field.
		CHECK_TRUE(described("running", cfg).find("ZXTfieldrun") != std::string::npos);
	}
	// Without a stemmer no field but the word's.
	const auto plain = engine::parse_text("running", config(base));
	CHECK_EQ(plain.node()->field, "XTfield");
}

void
test_wildcard_pattern_flags()
{
	const auto flags = [](const char* text) {
		const auto query = engine::parse_text(text, config(base | text::WILDCARD | text::WILDCARD_SINGLE | text::WILDCARD_MULTI));
		const auto* node = query.node();
		CHECK_TRUE(node != nullptr && node->kind == engine::QueryKind::wildcard);
		return node == nullptr ? 0u : node->pattern_flags;
	};
	CHECK_EQ(flags("fo?b"), engine::PATTERN_SINGLE);
	CHECK_EQ(flags("fo*b"), engine::PATTERN_MULTI);
	CHECK_EQ(flags("f?o*b"), engine::PATTERN_SINGLE | engine::PATTERN_MULTI);
}

void
test_all_wildcard_patterns()
{
	// Match-all only when the pattern matches every term: a '*' and at most one '?'. The others filter by length.
	auto cfg = config(base | text::WILDCARD | text::WILDCARD_SINGLE | text::WILDCARD_MULTI);
	cfg.field.clear();
	for (const char* text : { "**", "*?", "?*" }) {
		const auto query = engine::parse_text(text, cfg);
		CHECK_TRUE(query.node() != nullptr && query.node()->kind == engine::QueryKind::term && query.node()->str.empty());
	}
	for (const char* text : { "??", "*??" }) {
		const auto query = engine::parse_text(text, cfg);
		CHECK_TRUE(query.node() != nullptr && query.node()->kind == engine::QueryKind::wildcard && query.node()->str == text);
	}
	// A lone '*' or '?' is not a wildcard to the parser: it reads as no words at all.
	CHECK_TRUE(engine::parse_text("*", cfg).empty());
	CHECK_TRUE(engine::parse_text("?", cfg).empty());
	// Behind a field prefix the pattern is a prefix filter, never match-all.
	const auto prefixed = engine::parse_text("**", config(base | text::WILDCARD | text::WILDCARD_MULTI));
	CHECK_TRUE(prefixed.node() != nullptr && prefixed.node()->kind == engine::QueryKind::wildcard);
}

// json_depth reads the nesting of a JSON text as the parsed document has it, on random documents.
void
test_json_depth_matches_the_document()
{
	std::uint64_t state = 88172645463325252ULL;
	const auto next = [&](std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	};
	// A random value; strings carry brackets, quotes and escapes.
	const auto make = [&](auto&& self, int budget) -> std::string {
		switch (budget <= 0 ? next(3) : next(5)) {
			case 0: return std::to_string(next(100));
			case 1: return "\"[{\\\"]}\\\\\"";
			case 2: return "null";
			case 3: {
				std::string out = "[";
				for (unsigned i = 0, n = next(4); i < n; ++i) {
					out += (i ? "," : "") + self(self, budget - 1);
				}
				return out + "]";
			}
			default: {
				std::string out = "{";
				for (unsigned i = 0, n = next(4); i < n; ++i) {
					out += (i ? ",\"k\":" : "\"k\":") + self(self, budget - 1);
				}
				return out + "}";
			}
		}
	};
	const auto levels = [](const rapidjson::Value& root) {
		std::vector<std::pair<const rapidjson::Value*, std::size_t>> stack{ { &root, 1 } };
		std::size_t deepest = 0;
		while (!stack.empty()) {
			const auto [value, depth] = stack.back();
			stack.pop_back();
			if (value->IsArray()) {
				deepest = std::max(deepest, depth);
				for (const auto& item : value->GetArray()) {
					stack.push_back({ &item, depth + 1 });
				}
			} else if (value->IsObject()) {
				deepest = std::max(deepest, depth);
				for (const auto& member : value->GetObject()) {
					stack.push_back({ &member.value, depth + 1 });
				}
			}
		}
		return deepest;
	};
	for (int round = 0; round < 3000; ++round) {
		const auto text = make(make, 1 + static_cast<int>(next(8)));
		rapidjson::Document doc;
		json_load(doc, text);
		CHECK_EQ(json_depth(text), levels(doc));
	}
	// Past the limit the text is a client error, before it is parsed; at the limit it loads.
	const auto nested = [](std::size_t n) { return std::string(n, '[') + std::string(n, ']'); };
	rapidjson::Document doc;
	json_load(doc, nested(MAX_JSON_DEPTH));
	for (const auto n : { MAX_JSON_DEPTH + 1, std::size_t(100000) }) {
		try {
			json_load(doc, nested(n));
			CHECK_TRUE(false);
		} catch (const ClientError& exc) {
			CHECK_TRUE(std::string(exc.what()).find("nested too deeply") != std::string::npos);
		}
	}
	// Brackets in strings and after escapes do not count; random bytes never fail the scan.
	CHECK_EQ(json_depth("[\"[[[\\\"[[\",{\"a\":\"}}\"}]"), std::size_t(2));
	for (int round = 0; round < 3000; ++round) {
		std::string bytes;
		for (unsigned i = 0, n = next(64); i < n; ++i) {
			bytes.push_back("[]{}\"\\a"[next(8)]);
		}
		CHECK_TRUE(json_depth(bytes) <= bytes.size());
	}
}

// A MessagePack or YAML body nested past MAX_JSON_DEPTH is a client error before it is copied or built;
// at the limit it loads, and a body far past it (the size of a small request) is refused the same way.
void
test_body_depth_per_format()
{
	const auto refused = [](auto&& load) {
		try {
			load();
			return false;
		} catch (const ClientError& exc) {
			return std::string(exc.what()).find("nested too deeply") != std::string::npos;
		}
	};
	const auto refused_as = [](auto&& load, const char* what) {
		try {
			load();
			return false;
		} catch (const ClientError& exc) {
			return std::string(exc.what()).find(what) != std::string::npos;
		}
	};
	// MessagePack: n nested one-element arrays (0x91) or maps (0x81 "f"), around an integer.
	const auto arrays = [](std::size_t n) { return std::string(n, '\x91') + '\x01'; };
	const auto maps = [](std::size_t n) {
		std::string out;
		for (std::size_t i = 0; i < n; ++i) {
			out += "\x81\xa1" "f";
		}
		return out + '\x01';
	};
	for (const auto make : { +arrays, +maps }) {
		CHECK_EQ(msgpack_load(make(MAX_JSON_DEPTH)).size(), std::size_t(1));
		for (const auto n : { MAX_JSON_DEPTH + 1, std::size_t(400), std::size_t(100000) }) {
			CHECK_TRUE(refused([&] { msgpack_load(make(n)); }));
		}
	}
	// A header that promises more than the body holds is a decode error, not a crash.
	try {
		msgpack_load(std::string("\xdd\xff\xff\xff\xff\x91", 6));
	} catch (const std::exception&) {
	}
	// YAML: n nested flow sequences, flow maps and block maps.
	const auto flow_seq = [](std::size_t n) { return std::string(n, '[') + "1" + std::string(n, ']'); };
	const auto flow_map = [](std::size_t n) {
		std::string out;
		for (std::size_t i = 0; i < n; ++i) {
			out += "{f: ";
		}
		return out + "1" + std::string(n, '}');
	};
	const auto block_map = [](std::size_t n) {
		std::string out;
		for (std::size_t i = 0; i < n; ++i) {
			out += std::string(i, ' ') + "f:\n";
		}
		return out + std::string(n, ' ') + "1\n";
	};
	for (const auto make : { +flow_seq, +flow_map, +block_map }) {
		rapidjson::Document doc;
		yaml_load(doc, make(MAX_JSON_DEPTH));
		CHECK_TRUE(!doc.IsNull());
		for (const auto n : { MAX_JSON_DEPTH + 1, std::size_t(400), std::size_t(1000) }) {
			CHECK_TRUE(refused([&] { rapidjson::Document d; yaml_load(d, make(n)); }));
		}
	}
	// A document of a bare scalar loads; one without a value (empty, blank, comments, markers) is a client error.
	for (const char* text : { "foo", "1", "~", "\"a: b\"", "--- x" }) {
		rapidjson::Document doc;
		yaml_load(doc, text);
		CHECK_FALSE(doc.IsObject());
	}
	for (const char* text : { "", "\n", "  \n# only a comment\n" }) {
		CHECK_TRUE(refused_as([&] { rapidjson::Document doc; yaml_load(doc, text); }, "no document"));
	}
	// Anchors and aliases are refused before any expansion: the chain, the billion laughs, a key, an unknown name.
	const auto not_supported = [&](const std::string& text) {
		return refused_as([&] { rapidjson::Document doc; yaml_load(doc, text); }, "anchors and aliases are not supported");
	};
	std::string chain;
	for (std::size_t i = 0; i < 800; ++i) {
		chain += "- &a" + std::to_string(i) + " [" + (i ? "*a" + std::to_string(i - 1) : std::string("x")) + "]\n";
	}
	CHECK_TRUE(not_supported(chain));
	for (std::size_t levels = 5; levels <= 9; ++levels) {
		std::string laughs = "a0: &a0 [\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\"]\n";
		for (std::size_t i = 1; i < levels; ++i) {
			const auto prev = "*a" + std::to_string(i - 1);
			laughs += "a" + std::to_string(i) + ": &a" + std::to_string(i) + " [" + prev + "," + prev + "," + prev + "," + prev + "," + prev + "," + prev + "," + prev + "," + prev + "," + prev + "]\n";
		}
		CHECK_TRUE(not_supported(laughs));
	}
	for (const char* text : { "a: &x 1\nb: *x", "&x [1]", "a: &x {b: 1}", "{&k a: 1}", "{*k : 1}", "a: &x 1\n*x : 2", "a: *x", "[*x]", "&x [*x" }) {
		CHECK_TRUE(not_supported(text));
	}
	// Plain bodies still load, with strings that only look like anchors or aliases.
	for (const char* text : { "a: [1, 2, {b: c}]\nd: \"&x\"\ne: '*y'\nf: a&b*c\n", "- x\n- y\n" }) {
		rapidjson::Document doc;
		yaml_load(doc, text);
		CHECK_FALSE(doc.IsNull());
	}
	// Truncated documents stay errors, with the partial document dropped.
	for (const char* text : { "a: [1,\n", "{a: [" }) {
		CHECK_TRUE(refused_as([&] { rapidjson::Document doc; yaml_load(doc, text); }, "JSON parse error"));
	}
}

TEST_MAIN(test_hate_groups_terminate, test_ab_decisions_stand, test_lowering_is_the_tree, test_errors_are_engine_errors, test_unreadable_settings_are_invalid, test_text_without_words_matches_nothing, test_nesting_is_not_recursive, test_json_depth_matches_the_document, test_body_depth_per_format, test_near_of_a_boolean_expression, test_stems_are_terms_of_the_stem_field, test_wildcard_pattern_flags, test_all_wildcard_patterns)
