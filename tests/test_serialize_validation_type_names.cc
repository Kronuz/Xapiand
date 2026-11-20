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

// Type names: every real name maps to its type and every other word is refused. A phf `find` of a word not in the
// table is npos, so a case label whose key the table lacks is npos too and catches every unknown word.

#include "test_harness.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "database/schema.h"
#include "exception_engine.h"
#include "serialize.h"

namespace {

using Types = std::array<FieldType, SPC_TOTAL_TYPES>;

// The words coverage-areas tried, which are no type name of Unserialize::get_field_type.
const char* const NOT_FIELD_TYPE_NAMES[] = { "zzz", "nonsense", "bogus", "unknown", "uuid", "date", "q", "xyz", "booleans" };

struct Name {
	const char* name;
	FieldType type;
};

const Name FIELD_TYPE_NAMES[] = {
	{ " ", FieldType::empty }, { "e", FieldType::empty }, { "empty", FieldType::empty },
	{ "a", FieldType::array }, { "array", FieldType::array },
	{ "b", FieldType::boolean }, { "boolean", FieldType::boolean },
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

// Concrete schema type names and the types they name; "array/", "foreign/" and "foreign/array/" prefix each.
const Name SCHEMA_TYPE_NAMES[] = {
	{ "object", FieldType::object }, { "boolean", FieldType::boolean }, { "date", FieldType::date },
	{ "datetime", FieldType::datetime }, { "float", FieldType::floating }, { "floating", FieldType::floating },
	{ "geo", FieldType::geo }, { "geospatial", FieldType::geo }, { "integer", FieldType::integer },
	{ "positive", FieldType::positive }, { "script", FieldType::script }, { "term", FieldType::keyword },
	{ "keyword", FieldType::keyword }, { "string", FieldType::text }, { "text", FieldType::text },
	{ "time", FieldType::time }, { "timedelta", FieldType::timedelta }, { "uuid", FieldType::uuid },
};

std::string lower(std::string_view word) {
	std::string out(word);
	for (auto& c : out) {
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}
	}
	return out;
}

// 10,000 random words that are no name in `names`, over letters, digits, '/' and ' ', one to twelve long, from a fixed seed.
std::vector<std::string> random_words(const std::set<std::string>& names) {
	std::mt19937_64 rng(0x5e7a11d47e5ULL);
	static constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/ ";
	std::uniform_int_distribution<std::size_t> length(1, 12);
	std::uniform_int_distribution<std::size_t> letter(0, alphabet.size() - 1);
	std::vector<std::string> words;
	words.reserve(10000);
	while (words.size() < 10000) {
		std::string word;
		for (auto n = length(rng); n != 0; --n) {
			word.push_back(alphabet[letter(rng)]);
		}
		if (names.count(lower(word)) == 0) {
			words.push_back(std::move(word));
		}
	}
	return words;
}

bool field_type_refuses(std::string_view word) {
	try {
		(void)Unserialize::get_field_type(word);
	} catch (const SerializationError&) {
		return true;
	}
	return false;
}

bool schema_type_refuses(std::string_view word) {
	try {
		(void)required_spc_t::get_types(word);
	} catch (const ClientError&) {
		return true;
	}
	return false;
}

void test_field_type_names_map_to_their_types() {
	for (const auto& n : FIELD_TYPE_NAMES) {
		const auto got = Unserialize::get_field_type(n.name);
		if (got != n.type) {
			std::fprintf(stderr, "  FAIL: get_field_type('%s') is %s\n", n.name, std::string(enum_name(got)).c_str());
		}
		CHECK(got == n.type);
	}
	// Names are case-insensitive, as the table's hash is.
	CHECK(Unserialize::get_field_type("Text") == FieldType::text);
	CHECK(Unserialize::get_field_type("BOOLEAN") == FieldType::boolean);
}

void test_field_type_refuses_every_other_word() {
	std::set<std::string> names;
	for (const auto& n : FIELD_TYPE_NAMES) {
		names.insert(n.name);
	}
	for (const char* word : NOT_FIELD_TYPE_NAMES) {
		CHECK(field_type_refuses(word));
	}
	int tried = 0;
	int accepted = 0;
	for (const auto& word : random_words(names)) {
		++tried;
		if (!field_type_refuses(word)) {
			if (++accepted <= 5) {
				std::fprintf(stderr, "  FAIL: get_field_type('%s') is not refused\n", word.c_str());
			}
		}
	}
	CHECK_EQ(tried, 10000);
	CHECK_EQ(accepted, 0);
}

void test_schema_type_names_map_to_their_types() {
	const auto check = [](const std::string& name, const Types& want) {
		const auto& got = required_spc_t::get_types(name);
		if (got != want) {
			std::fprintf(stderr, "  FAIL: get_types('%s') is %s\n", name.c_str(), std::string(required_spc_t::get_str_type(got)).c_str());
		}
		CHECK(got == want);
	};
	for (const auto& n : SCHEMA_TYPE_NAMES) {
		const std::string name = n.name;
		check(name, Types{ FieldType::empty, FieldType::empty, n.type });
		check("array/" + name, Types{ FieldType::empty, FieldType::array, n.type });
		check("foreign/" + name, Types{ FieldType::foreign, FieldType::empty, n.type });
		check("foreign/array/" + name, Types{ FieldType::foreign, FieldType::array, n.type });
	}
	check("array", Types{ FieldType::empty, FieldType::array, FieldType::empty });
	check("foreign", Types{ FieldType::foreign, FieldType::empty, FieldType::empty });
	// What get_str_type writes, get_types reads back.
	for (const auto& n : SCHEMA_TYPE_NAMES) {
		const Types types{ FieldType::foreign, FieldType::array, n.type };
		CHECK(required_spc_t::get_types(required_spc_t::get_str_type(types)) == types);
	}
}

void test_schema_type_refuses_every_other_word() {
	std::set<std::string> names = { "array", "foreign", "foreign/array", "array/foreign" };
	for (const auto& n : SCHEMA_TYPE_NAMES) {
		const std::string name = n.name;
		for (const auto& form : { name, "array/" + name, name + "/array", "foreign/" + name, name + "/foreign" }) {
			names.insert(form);
		}
	}
	for (const char* word : { "zzz", "nonsense", "bogus", "unknown", "undefined", "booleans", "geos", "array/bogus", "foreign/zzz" }) {
		CHECK(schema_type_refuses(word));
	}
	int tried = 0;
	int accepted = 0;
	for (const auto& word : random_words(names)) {
		++tried;
		if (!schema_type_refuses(word)) {
			if (++accepted <= 5) {
				std::fprintf(stderr, "  FAIL: get_types('%s') is not refused\n", word.c_str());
			}
		}
	}
	CHECK_EQ(tried, 10000);
	CHECK_EQ(accepted, 0);
}

}  // namespace

TEST_MAIN(test_field_type_names_map_to_their_types, test_field_type_refuses_every_other_word,
	test_schema_type_names_map_to_their_types, test_schema_type_refuses_every_other_word)
