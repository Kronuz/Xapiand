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

// Unit tests for the seam's fields (fields-and-keys X6): a term is a field and the bytes under it.
// The Xapian engine joins them back into today's term byte for byte, so each test builds the Xapian
// objects by hand from the joined strings and compares; the strata engine's interim mapping joins
// them the same way until it resolves fields to ids.

#include "test_harness.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "analysis.hh"
#include "config.h"
#include "engine/query.h"
#include "engine/strata/document.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/query.h"
#include "engine/xapian/text.h"
#include "engine/xapian/xapian.h"
#include "multivalue/generate_terms.h"

namespace {

using engine::FieldKind;
using engine::Op;
using engine::Query;
using engine::xapian::to_xapian;

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

// Field names as Xapiand makes them: a path prefix and a type character, an accuracy level, an HTM
// level, a stem field, the bare prefix of a null marker, a global field, and the empty name.
const std::vector<std::string> field_names = {
	"name.firstName.S", "Zname.firstName.S", "name.", "<186a0>.N", "checkin.<a>.G", "style.pants.S",
	"N", "QK", std::string("\x01\x80\xff", 3), "",
};

std::vector<Xapian::docid> run(const Xapian::Database& db, const Xapian::Query& query) {
	Xapian::Enquire enquire(db);
	enquire.set_query(query);
	std::vector<Xapian::docid> dids;
	auto mset = enquire.get_mset(0, 100, 100);
	for (auto it = mset.begin(); it != mset.end(); ++it) {
		dids.push_back(*it);
	}
	return dids;
}

}  // namespace

// A field is declared by its name, once per document; a later call can only widen its kind.
static void test_field_refs() {
	engine::Document doc;
	const auto a = doc.field("a");
	const auto b = doc.field("b", FieldKind::text);
	CHECK_TRUE(a.id != b.id);
	CHECK_EQ(doc.field("a").id, a.id);
	CHECK_EQ(doc.field("b", FieldKind::boolean).id, b.id);
	CHECK_EQ(doc.field("").id, doc.field("").id);  // the empty name is a field

	// Many fields, and a moved document, keep every reference.
	std::vector<engine::FieldRef> refs;
	for (int i = 0; i < 500; ++i) {
		refs.push_back(doc.field("f" + std::to_string(i)));
	}
	engine::Document moved(std::move(doc));
	for (int i = 0; i < 500; ++i) {
		CHECK_EQ(moved.field("f" + std::to_string(i)).id, refs[i].id);
	}
	moved.add_term(refs[7], "x");
	CHECK_EQ(moved.serialise(), [&] {
		Xapian::Document x;
		x.add_term("f7x", 1);
		return engine::xapian::from_xapian(x).serialise();
	}());
}

// A document built through fields is the Xapian document of the joined terms, serialised byte for byte.
static void test_xapian_terms_are_joined() {
	Lcg rng{ 7 };
	for (int round = 0; round < 200; ++round) {
		engine::Document doc;
		Xapian::Document xdoc;
		for (int i = 0; i < 40; ++i) {
			const auto& name = field_names[rng.next(static_cast<std::uint32_t>(field_names.size()))];
			// Empty terms are the field's marker; a field-less empty term is no Xapian term, so skip it.
			const std::string term = rng.next(5) == 0 ? std::string() : "t" + std::to_string(rng.next(6));
			if (name.empty() && term.empty()) {
				continue;
			}
			const auto field = doc.field(name, static_cast<FieldKind>(rng.next(3)));
			switch (rng.next(3)) {
				case 0: {
					const auto wdf = rng.next(4);
					doc.add_term(field, term, wdf);
					xdoc.add_term(name + term, wdf);
					break;
				}
				case 1:
					doc.add_boolean_term(field, term);
					xdoc.add_boolean_term(name + term);
					break;
				default: {
					const auto pos = 1 + rng.next(5);
					const auto wdf = rng.next(3);
					doc.add_posting(field, term, pos, wdf);
					xdoc.add_posting(name + term, pos, wdf);
					break;
				}
			}
		}
		CHECK_EQ(doc.serialise(), engine::xapian::from_xapian(xdoc).serialise());
	}
}

// The marker term of a null value is the field's name alone; and the Xapian engine, which joins
// the two, cannot tell ("ab", "c") from ("a", "bc").
static void test_marker_and_joined_ambiguity() {
	engine::Document doc;
	doc.add_boolean_term(doc.field("name."), std::string());
	Xapian::Document xdoc;
	xdoc.add_boolean_term("name.");
	CHECK_EQ(doc.serialise(), engine::xapian::from_xapian(xdoc).serialise());

	engine::Document joined;
	joined.add_term(joined.field("ab"), "c", 1);
	joined.add_term(joined.field("a"), "bc", 2);
	Xapian::Document one;
	one.add_term("abc", 3);
	CHECK_EQ(joined.serialise(), engine::xapian::from_xapian(one).serialise());
}

// A query node names its field apart from its term; the Xapian query is the joined one.
static void test_query_nodes() {
	const std::string field = "name.firstName.S";
	CHECK_EQ(to_xapian(Query::term(field, "bob", 2, 3)).get_description(), Xapian::Query(field + "bob", 2, 3).get_description());
	CHECK_EQ(to_xapian(Query::term(field, "")).get_description(), Xapian::Query(field).get_description());  // the marker
	CHECK_EQ(to_xapian(Query::match_all()).get_description(), Xapian::Query::MatchAll.get_description());
	CHECK_EQ(to_xapian(Query::term("", "")).get_description(), Xapian::Query::MatchAll.get_description());

	CHECK_EQ(to_xapian(Query::wildcard(field, "bo", 50, engine::Expansion::most_frequent)).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, field + "bo", 50, Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT, Xapian::Query::OP_SYNONYM).get_description());
	CHECK_EQ(to_xapian(Query::wildcard(field, "b?*", 7, engine::Expansion::first, engine::PATTERN_SINGLE | engine::PATTERN_MULTI, Op::OR)).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, field + "b?*", 7, Xapian::Query::WILDCARD_LIMIT_FIRST | Xapian::Query::WILDCARD_PATTERN_SINGLE | Xapian::Query::WILDCARD_PATTERN_MULTI, Xapian::Query::OP_OR).get_description());
	// Any term in the field: the Xapian engine reads the name as a prefix.
	CHECK_EQ(to_xapian(Query::field("name.")).get_description(),
		Xapian::Query(Xapian::Query::OP_WILDCARD, "name.", 0, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::Query::OP_SYNONYM).get_description());

	// The edit distance's prefix length counts from the term: the field's bytes lead the Xapian pattern.
	for (std::size_t prefix_length : { 0u, 1u, 3u }) {
		CHECK_EQ(to_xapian(Query::edit_distance(field, "bobby", 5, engine::Expansion::error, Op::OR, 2, prefix_length)).get_description(),
			Xapian::Query(Xapian::Query::OP_EDIT_DISTANCE, field + "bobby", 5, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::Query::OP_OR, 2, field.size() + prefix_length).get_description());
	}

	engine::TextConfig config;
	config.field = field;
	CHECK_EQ(to_xapian(Query::text("bob smith", config)).get_description(), [&] {
		Xapian::QueryParser parser;
		return parser.parse_query("bob smith", 0, field).get_description();
	}());
}

// The same word under two fields is two terms: a query on a field finds the documents with that field's
// term only. A field mapped to the wrong prefix, on either side, breaks this.
static void test_field_selects_terms() {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	const std::vector<std::pair<std::string, std::string>> docs = {
		{ "title.S", "apple" }, { "body.S", "apple" }, { "title.S", "pear" }, { "body.S", "apple" }, { "title.S", "apple" },
	};
	for (const auto& [name, word] : docs) {
		engine::Document doc;
		doc.add_term(doc.field(name), word, 1);
		db.add_document(engine::xapian::xapian_document(doc));
	}
	const auto query = [&](const std::string& field, const std::string& word) { return run(db, to_xapian(Query::term(field, word))); };
	CHECK_EQ(query("title.S", "apple"), (std::vector<Xapian::docid>{ 1, 5 }));
	CHECK_EQ(query("body.S", "apple"), (std::vector<Xapian::docid>{ 2, 4 }));
	CHECK_EQ(query("title.S", "pear"), (std::vector<Xapian::docid>{ 3 }));
	CHECK_EQ(query("body.S", "pear"), (std::vector<Xapian::docid>{}));
	CHECK_EQ(run(db, to_xapian(Query::wildcard("title.S", "ap"))), (std::vector<Xapian::docid>{ 1, 5 }));
	CHECK_EQ(run(db, to_xapian(Query::field("body."))), (std::vector<Xapian::docid>{ 2, 4 }));
	CHECK_EQ(run(db, to_xapian(Query::match_all())).size(), 5u);
}

// Text is indexed into the field, and its stems into the field `Z` + name: the document is the one the
// generator builds with the prefix, for every stem strategy and language.
static void test_text_fields() {
	const std::vector<std::string> texts = {
		"The quick brown fox jumps over the lazy dog. Running runners ran quickly; Zebras zig-zag.",
		"Zzz ZZ Z z zealous zones",
		"Le chat mange la souris; el nino come manzanas; Der Hund laeuft schnell",
		"cafe naive resume strasse Istanbul",
		"",
	};
	const std::vector<std::pair<std::string, std::string>> languages = { { "", "" }, { "", "english" }, { "", "french" } };
	for (const auto& prefix : std::vector<std::string>{ "name.S", "Zname.S", "ZZ.S", "<a>.T" }) {
		for (const auto stem : { analysis::stem_strategy::none, analysis::stem_strategy::some, analysis::stem_strategy::all, analysis::stem_strategy::all_z }) {
			for (const auto& [language, stem_language] : languages) {
				for (const bool positions : { true, false }) {
					for (const auto& text : texts) {
						engine::Document doc;
						GenerateTerms::text(doc, prefix, text, positions, 2, false, false, language, stem_language, analysis::stop_strategy::none, stem);

						// The oracle: the generator with the prefix, terms joined by the generator itself.
						Xapian::Document xdoc;
						analysis::term_generator generator;
						std::optional<snowball::stemmer> stemmer;
						if (!stem_language.empty()) {
							stemmer.emplace(stem_language);
							generator.stemmer = &*stemmer;
							generator.stem = stem;
						}
						generator.index_text(text, [&](std::string_view term, std::uint32_t wdf, std::uint32_t pos) {
							if (pos != 0) {
								xdoc.add_posting(std::string(term), pos, wdf);
							} else {
								xdoc.add_term(std::string(term), wdf);
							}
						}, {.wdf_inc = 2, .prefix = prefix, .positions = positions});
						CHECK_EQ(doc.serialise(), engine::xapian::from_xapian(xdoc).serialise());
					}
				}
			}
		}
	}
}

// The same for Xapian's own generator, which splits words by the Unicode word-break rules.
static void test_word_break_fields() {
#ifdef USE_ICU
	const std::vector<std::string> texts = { "The quick brown fox jumps; running runners ran", "Zzz ZZ zealous", "" };
	for (const auto stem : { analysis::stem_strategy::none, analysis::stem_strategy::some, analysis::stem_strategy::all, analysis::stem_strategy::all_z }) {
		for (const bool positions : { true, false }) {
			for (const auto& text : texts) {
				const std::string prefix = "body.S";
				engine::Document doc;
				GenerateTerms::text(doc, prefix, text, positions, 1, false, true, "", "english", analysis::stop_strategy::none, stem);

				Xapian::Document xdoc;
				Xapian::TermGenerator generator;
				generator.set_flags(Xapian::TermGenerator::FLAG_WORD_BREAKS);
				generator.set_document(xdoc);
				generator.set_stemmer(Xapian::Stem("english"));
				generator.set_stemming_strategy(stem == analysis::stem_strategy::none ? Xapian::TermGenerator::STEM_NONE
					: stem == analysis::stem_strategy::some ? Xapian::TermGenerator::STEM_SOME
					: stem == analysis::stem_strategy::all ? Xapian::TermGenerator::STEM_ALL : Xapian::TermGenerator::STEM_ALL_Z);
				if (positions) {
					generator.index_text(text, 1, prefix);
				} else {
					generator.index_text_without_positions(text, 1, prefix);
				}
				CHECK_EQ(doc.serialise(), engine::xapian::from_xapian(xdoc).serialise());
			}
		}
	}
#endif
}

// The strata engine's interim mapping holds the same joined terms, with the frequencies and positions.
static void test_strata_interim() {
	engine::Document doc(engine::Kind::strata);
	doc.set_key("QKid");
	const auto title = doc.field("title.S", FieldKind::text);
	const auto marker = doc.field("title.");
	doc.add_term(title, "bob", 2);
	doc.add_posting(title, "smith", 4, 1);
	doc.add_posting(title, "smith", 2, 1);
	doc.add_boolean_term(marker, std::string());
	doc.add_term(doc.field("title.S"), "bob", 3);
	auto& native = engine::strata::strata_document(doc);
	native.normalise();
	CHECK_EQ(native.key(), "QKid");
	CHECK_EQ(native.terms().size(), 4u);
	CHECK_EQ(native.terms().at("title.Sbob").wdf, 5u);
	CHECK_EQ(native.terms().at("title.Ssmith").wdf, 2u);
	CHECK_EQ(native.terms().at("title.Ssmith").positions, (std::vector<engine::termpos>{ 2, 4 }));
	CHECK_EQ(native.terms().at("title.").wdf, 0u);
	CHECK_EQ(native.terms().at("QKid").wdf, 0u);

	// The log line carries the joined terms: it reads back to the same document.
	auto back = engine::Document::unserialise(engine::Kind::strata, doc.serialise());
	CHECK_EQ(back.serialise(), doc.serialise());
}

TEST_MAIN(test_field_refs, test_xapian_terms_are_joined, test_marker_and_joined_ambiguity, test_query_nodes, test_field_selects_terms, test_text_fields, test_word_break_fields, test_strata_interim)
