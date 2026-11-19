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
// aggregation reads values through Serialize::exact like a search (review C6). Every
// stem_language the schema accepts builds a snowball stemmer; tests/test_stem_parity.sh reads
// the "stem_language NAME stemmer STORED" lines this prints.

#include "test_harness.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "engine/text.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "serialize.h"
#include "snowball.hh"

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

// The first "_stem_language" value under obj, or nullptr.
static const MsgPack* find_stem_language(const MsgPack& obj) {
	if (!obj.is_map()) { return nullptr; }
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		if (it->str_view() == "_stem_language") { return &it.value(); }
		if (const auto* found = find_stem_language(it.value())) { return found; }
	}
	return nullptr;
}

static MsgPack text_field(std::string_view stem_language) {
	return MsgPack({ { "body", MsgPack({ { "_type", "text" }, { "_stem_language", stem_language } }) } });
}

// The stemmer name the schema stores for a text field with stem_language `name`.
static std::string stored_stemmer(std::string_view name) {
	Schema schema(Schema::get_initial_schema(), nullptr, "");
	schema.update(text_field(name));
	const auto* found = find_stem_language(schema.get_schema());
	return found != nullptr ? std::string(found->str_view()) : std::string();
}

// nb and nn stem as norwegian, german2 as german (Xapian 2.1.0's stems over snowball-data's
// vocabularies: jobs 261118-002155-22aee4, 261118-002203-9c6e13, 261118-002212-d6a754).
static void test_stem_aliases_store_their_stemmer() {
	CHECK_EQ(stored_stemmer("nb"), "norwegian");
	CHECK_EQ(stored_stemmer("nn"), "norwegian");
	CHECK_EQ(stored_stemmer("NB"), "norwegian");
	CHECK_EQ(stored_stemmer("german2"), "german");
	CHECK_EQ(stored_stemmer("no"), "no");
	CHECK_EQ(stored_stemmer("dutch"), "dutch");
	CHECK_EQ(snowball::stemmer(stored_stemmer("nb"))("aabakken"), "aabakk");
	CHECK_EQ(snowball::stemmer(stored_stemmer("german2"))("aalglatten"), "aalglatt");
	// The stored schema takes the alias again: it names the same stemmer.
	Schema first(Schema::get_initial_schema(), nullptr, "");
	first.update(text_field("nn"));
	Schema second(first.get_modified_schema(), nullptr, "");
	bool threw = false;
	try {
		second.update(text_field("nn"));
	} catch (const ClientError&) {
		threw = true;
	}
	CHECK_FALSE(threw);
}

// The refusal of `name`'s message, or "" when the schema accepts it.
static std::string stem_refusal(std::string_view name) {
	try {
		stored_stemmer(name);
	} catch (const ClientError& exc) {
		return std::string(exc.get_message());
	}
	return {};
}

// The names a refusal lists as accepted.
static std::vector<std::string> accepted_stem_languages() {
	const auto message = stem_refusal("klingon");
	const std::string_view marker = "the accepted languages are: ";
	const auto at = message.find(marker);
	std::vector<std::string> names;
	if (at == std::string::npos) { return names; }
	std::string_view rest = std::string_view(message).substr(at + marker.size());
	while (!rest.empty()) {
		const auto comma = rest.find(", ");
		names.emplace_back(rest.substr(0, comma));
		rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 2);
	}
	return names;
}

// Names the schema refuses (remove-not-refuse: none appears in docs either).
static const std::vector<std::string_view> removed_stem_languages{
	"earlyenglish",  // stems 2,849 of 36,534 words unlike english (job 261118-002220-8a7a7f)
	"english_lovins",  // Xapian 2.1.0 refuses it too (release-rows A4); lovins stays
	"english_porter",  // likewise; porter stays
};

// A refused name gets a 400 that names the accepted languages, each of which the schema accepts
// and stores as a stemmer snowball builds.
static void test_stem_refusal_names_the_accepted() {
	const auto names = accepted_stem_languages();
	CHECK_GT(names.size(), 40u);
	for (const auto& name : names) {
		CHECK_EQ(stem_refusal(name), "");
		const auto stemmer = stored_stemmer(name);
		std::printf("stem_language %s stemmer %s\n", name.c_str(), stemmer.c_str());
		if (!stemmer.empty()) { CHECK(!snowball::stemmer::canonical(stemmer).empty()); }
	}
	for (const auto name : removed_stem_languages) {
		CHECK_NE(stem_refusal(name), "");
		CHECK(std::find(names.begin(), names.end(), name) == names.end());
	}
}

// A schema stored with a raw name (as before the aliases) reads, as a search does, through the
// same stemmer name the write path uses: its text parses with a snowball stemmer.
static void test_stored_alias_reads_its_stemmer() {
	for (const auto& [stored, stemmer] : std::vector<std::pair<std::string, std::string>>{
	         {"nb", "norwegian"}, {"nn", "norwegian"}, {"NB", "norwegian"},
	         {"german2", "german"}, {"German2", "german"}, {"English", "english"}}) {
		Schema first(Schema::get_initial_schema(), nullptr, "");
		first.update(text_field("english"));
		MsgPack copy(*first.get_modified_schema());
		bool set = false;
		auto set_stored = [&](auto&& self, MsgPack& obj) -> void {
			if (!obj.is_map() || set) { return; }
			if (obj.find("_stem_language") != obj.end()) { obj["_stem_language"] = stored; set = true; return; }
			for (auto it = obj.begin(); it != obj.end(); ++it) { self(self, it.value()); }
		};
		set_stored(set_stored, copy);
		CHECK(set);
		Schema schema(std::make_shared<const MsgPack>(std::move(copy)), nullptr, "");
		const auto read = schema.get_data_field("body", false).first.stem_language;
		CHECK_EQ(read, stemmer);
		engine::TextConfig config;
		config.field = "body";
		config.stem_language = read;
		bool built = true;
		try {
			engine::parse_text("houses", config);
		} catch (const std::exception&) {
			built = false;
		}
		CHECK(built);
	}
}

TEST_MAIN(test_numeric_text_is_a_json_number, test_term_filter_reads_exactly, test_stem_aliases_store_their_stemmer, test_stem_refusal_names_the_accepted, test_stored_alias_reads_its_stemmer)
