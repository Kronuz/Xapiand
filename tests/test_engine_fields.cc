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

// Unit tests for the seam's fields (fields-and-keys X6): a term is a field and the bytes under it,
// and the engine keeps them apart.

#include "test_harness.h"

#include <csignal>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "analysis.hh"
#include "config.h"
#include "database/utils.h"
#include "hashes.hh"
#include "engine/document.h"
#include "engine/query.h"
#include "engine/text.h"
#include "engine/strata/document.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "multivalue/generate_terms.h"

namespace {

using engine::FieldKind;
using engine::Op;
using engine::Query;
using engine::strata::strata_document;

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

// The unique ids of the documents a query finds in a shard.
std::set<engine::docid> run(engine::Shard& shard, const Query& query) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100;
	request.check_at_least = 100;
	engine::Shard* shards[] = {&shard};
	std::set<engine::docid> dids;
	for (const auto& hit : engine::sync_wait(engine::search(shards, request)).hits) {
		dids.insert(hit.did);
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
	CHECK_EQ(strata_document(moved).find_term("f7", "x")->wdf, 1u);
	CHECK_TRUE(strata_document(moved).find_term("f8", "x") == nullptr);
}

// A document built through fields holds each (field, term) once, with the sum of the frequencies and the
// set of the positions given for it, and its serialised form reads back the same.
static void test_field_terms_match_model() {
	struct Term {
		engine::termcount wdf = 0;
		std::set<engine::termpos> positions;
	};
	Lcg rng{ 7 };
	for (int round = 0; round < 200; ++round) {
		engine::Document doc;
		std::map<std::pair<std::string, std::string>, Term> model;
		bool used_empty_name = false;
		for (int i = 0; i < 40; ++i) {
			const auto& name = field_names[rng.next(static_cast<std::uint32_t>(field_names.size()))];
			// Empty terms are the field's marker.
			const std::string term = rng.next(5) == 0 ? std::string() : "t" + std::to_string(rng.next(6));
			used_empty_name = used_empty_name || name.empty();
			const auto field = doc.field(name, static_cast<FieldKind>(rng.next(3)));
			auto& entry = model[{name, term}];
			switch (rng.next(3)) {
				case 0: {
					const auto wdf = rng.next(4);
					doc.add_term(field, term, wdf);
					entry.wdf += wdf;
					break;
				}
				case 1:
					doc.add_boolean_term(field, term);
					break;
				default: {
					const auto pos = 1 + rng.next(5);
					const auto wdf = rng.next(3);
					doc.add_posting(field, term, pos, wdf);
					entry.wdf += wdf;
					entry.positions.insert(pos);
					break;
				}
			}
		}
		const auto check = [&](const engine::Document& d) {
			const auto& native = strata_document(d);
			CHECK_EQ(native.terms().size(), model.size());
			for (const auto& [key, term] : model) {
				const auto* found = native.find_term(key.first, key.second);
				CHECK_TRUE(found != nullptr);
				if (found) {
					CHECK_EQ(found->wdf, term.wdf);
					const auto positions = native.positions_of(*found);
					CHECK_TRUE((std::set<engine::termpos>(positions.begin(), positions.end()) == term.positions));
				}
			}
		};
		check(doc);
		// The log line names every field, so the empty name does not read back.
		if (!used_empty_name) {
			const auto bytes = doc.serialise();
			const auto back = engine::Document::unserialise(bytes);
			check(back);
			CHECK_EQ(back.serialise(), bytes);
		}
	}
}

// The marker term of a null value is the field's name with an empty term; ("ab", "c") and ("a", "bc")
// are different terms of different fields.
static void test_marker_and_field_boundary() {
	engine::Document doc;
	doc.add_boolean_term(doc.field("name."), std::string());
	const auto& native = strata_document(doc);
	CHECK_TRUE(native.find_term("name.", "") != nullptr);
	CHECK_EQ(native.find_term("name.", "")->wdf, 0u);
	CHECK_TRUE(native.find_term("name", ".") == nullptr);

	engine::Document joined;
	joined.add_term(joined.field("ab"), "c", 1);
	joined.add_term(joined.field("a"), "bc", 2);
	const auto& two = strata_document(joined);
	CHECK_EQ(two.terms().size(), 2u);
	CHECK_EQ(two.find_term("ab", "c")->wdf, 1u);
	CHECK_EQ(two.find_term("a", "bc")->wdf, 2u);
	CHECK_TRUE(two.find_term("abc", "") == nullptr);
	CHECK_TRUE(two.find_term("", "abc") == nullptr);
}

// A query node names its field apart from its term.
static void test_query_nodes() {
	const std::string field = "name.firstName.S";
	const auto term = Query::term(field, "bob", 2, 3);
	CHECK_TRUE(term.node() != nullptr && term.node()->kind == engine::QueryKind::term);
	CHECK_EQ(term.node()->field, field);
	CHECK_EQ(term.node()->str, "bob");
	CHECK_EQ(term.node()->wqf, 2u);
	CHECK_EQ(term.node()->pos, 3u);
	const auto marker = Query::term(field, "");  // the field's marker, not match-all
	CHECK_EQ(marker.node()->field, field);
	CHECK_TRUE(marker.node()->str.empty());
	CHECK_FALSE(marker.empty());
	const auto all = Query::term("", "");
	CHECK_TRUE(all.node()->field.empty() && all.node()->str.empty());

	const auto wildcard = Query::wildcard(field, "b?*", 7, engine::Expansion::first, engine::PATTERN_SINGLE | engine::PATTERN_MULTI, Op::OR);
	CHECK_TRUE(wildcard.node()->kind == engine::QueryKind::wildcard);
	CHECK_EQ(wildcard.node()->field, field);
	CHECK_EQ(wildcard.node()->str, "b?*");
	CHECK_EQ(wildcard.node()->parameter, 7u);
	CHECK_TRUE(wildcard.node()->limit == engine::Expansion::first);
	CHECK_EQ(wildcard.node()->pattern_flags, engine::PATTERN_SINGLE | engine::PATTERN_MULTI);
	CHECK_TRUE(wildcard.node()->op == Op::OR);

	// The edit distance's prefix length counts from the term, not from the field.
	for (std::size_t prefix_length : { 0u, 1u, 3u }) {
		const auto fuzzy = Query::edit_distance(field, "bobby", 5, engine::Expansion::error, Op::OR, 2, prefix_length);
		CHECK_TRUE(fuzzy.node()->kind == engine::QueryKind::edit_distance);
		CHECK_EQ(fuzzy.node()->field, field);
		CHECK_EQ(fuzzy.node()->str, "bobby");
		CHECK_EQ(fuzzy.node()->distance, 2u);
		CHECK_EQ(fuzzy.node()->prefix_length, prefix_length);
	}

	engine::TextConfig config;
	config.field = field;
	const auto parsed = engine::parse_text("bob", config);
	CHECK_TRUE(parsed.node() != nullptr && parsed.node()->kind == engine::QueryKind::term);
	CHECK_EQ(parsed.node()->field, field);
	CHECK_EQ(parsed.node()->str, "bob");
}

// The same word under two fields is two terms: a query on a field finds the documents with that field's
// term only. A field mapped to the wrong prefix, on either side, breaks this.
static void test_field_selects_terms() {
	const auto path = (std::filesystem::current_path() / "engine_fields_select.db").string();
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::open_local(path, true, true));
	auto& shard = *opened.shard;
	const std::vector<std::pair<std::string, std::string>> docs = {
		{ "title.S", "apple" }, { "body.S", "apple" }, { "title.S", "pear" }, { "body.S", "apple" }, { "title.S", "apple" },
	};
	int n = 0;
	for (const auto& [name, word] : docs) {
		engine::Document doc;
		doc.set_key("K" + std::to_string(++n));
		doc.add_term(doc.field(name), word, 1);
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));
	const auto query = [&](const std::string& field, const std::string& word) { return run(shard, Query::term(field, word)); };
	CHECK_TRUE((query("title.S", "apple") == std::set<engine::docid>{ 1, 5 }));
	CHECK_TRUE((query("body.S", "apple") == std::set<engine::docid>{ 2, 4 }));
	CHECK_TRUE((query("title.S", "pear") == std::set<engine::docid>{ 3 }));
	CHECK_TRUE(query("body.S", "pear").empty());
	CHECK_TRUE((run(shard, Query::wildcard("title.S", "ap")) == std::set<engine::docid>{ 1, 5 }));
	CHECK_TRUE((run(shard, Query::field("body.S")) == std::set<engine::docid>{ 2, 4 }));
	CHECK_EQ(run(shard, Query::match_all()).size(), 5u);
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// Text is indexed into the field, and its stems into the field `Z` + name: the document is the one the
// generator builds with the prefix, split into the word field and the stem field, for every stem
// strategy and language.
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
						GenerateTerms::text(doc, prefix, text, positions, 2, false, language, stem_language, analysis::stop_strategy::none, stem);

						// The oracle: the generator with the prefix gives the joined terms; a term that leads with
						// 'Z' + prefix is a stem, under the field of that name, and any other is under the prefix.
						engine::Document expected;
						analysis::term_generator generator;
						std::optional<snowball::stemmer> stemmer;
						if (!stem_language.empty()) {
							stemmer.emplace(stem_language);
							generator.stemmer = &*stemmer;
							generator.stem = stem;
						}
						const auto kind = positions ? FieldKind::text : FieldKind::terms;
						generator.index_text(text, [&](std::string_view term, std::uint32_t wdf, std::uint32_t pos) {
							const std::string stems = "Z" + prefix;
							const bool is_stem = term.substr(0, stems.size()) == stems;
							const auto field = expected.field(is_stem ? stems : prefix, kind);
							term.remove_prefix(is_stem ? stems.size() : prefix.size());
							if (pos != 0) {
								expected.add_posting(field, term, pos, wdf);
							} else {
								expected.add_term(field, term, wdf);
							}
						}, {.wdf_inc = 2, .prefix = prefix, .positions = positions});
						CHECK_EQ(doc.serialise(), expected.serialise());
					}
				}
			}
		}
	}
}

// The host's id key: the type character and the id's bytes, no 'Q'; the id field's terms are the key field's;
// placement hashes the 'Q' spelling, so a shard keeps its documents.
static void test_host_keys() {
	CHECK_EQ(document_key("abc", 'K'), "Kabc");
	CHECK_EQ(document_key(std::string("\x81\x40", 2), 'N'), std::string("N\x81\x40", 3));
	CHECK_EQ(key_placement_hash("Kabc"), fnv1ah64::hash("QKabc"));
	CHECK_EQ(key_placement_hash("N\x80"), fnv1ah64::hash(std::string("QN\x80")));
	const auto id = field_term("Q", 'K', "abc");
	CHECK_EQ(id.field, std::string(engine::KEY_FIELD));
	CHECK_EQ(id.term, "Kabc");
	const auto other = field_term("name.", 'S', "abc");
	CHECK_EQ(other.field, "name.S");
	CHECK_EQ(other.term, "abc");
	CHECK_EQ(std::string(query_field("Q")), std::string(engine::KEY_FIELD));
	CHECK_EQ(std::string(query_field("name.")), "name.");
	// The accuracy terms of a numeric id lead with 'A', not the id prefix: they are an ordinary field.
	CHECK_EQ(std::string(query_field(acc_term_prefix("Q"))), "A");
}

// The key field's terms are the key's: they count in the document's length, they are not terms of it.
static void test_key_field() {
	engine::Document doc;
	doc.set_key("Kid");
	doc.add_term(doc.field(engine::KEY_FIELD, FieldKind::terms), "Kother", 2);
	doc.add_term(doc.field("QK"), "plain", 1);  // a field that merely looks like the id term's lead
	const auto& native = strata_document(doc);
	CHECK_EQ(native.key(), "Kid");
	CHECK_EQ(native.key_wdf(), 2u);
	CHECK_TRUE(native.field(engine::KEY_FIELD) == nullptr);
	CHECK_TRUE(native.find_term(engine::KEY_FIELD, "Kother") == nullptr);
	CHECK_EQ(native.terms().size(), 1u);
	CHECK_EQ(native.find_term("QK", "plain")->wdf, 1u);
}

// A document holds its terms by field, with the frequencies and positions; the key and
// the key field's frequency stay apart from the terms.
static void test_strata_fields() {
	engine::Document doc;
	doc.set_key("Kid");
	doc.add_term(doc.field(engine::KEY_FIELD, FieldKind::boolean), "Kid", 1);
	const auto title = doc.field("title.S", FieldKind::text);
	const auto marker = doc.field("title.");
	doc.add_term(title, "bob", 2);
	doc.add_posting(title, "smith", 4, 1);
	doc.add_posting(title, "smith", 2, 1);
	doc.add_boolean_term(marker, std::string());
	doc.add_term(doc.field("title.S"), "bob", 3);
	auto& native = strata_document(doc);
	CHECK_EQ(native.key(), "Kid");
	CHECK_EQ(native.key_wdf(), 1u);
	CHECK_EQ(native.fields().size(), 2u);
	const auto& text = *native.field("title.S");
	CHECK_TRUE(text.kind == FieldKind::text);
	CHECK_EQ(text.term_count, 2u);
	CHECK_EQ(native.find_term("title.S", "bob")->wdf, 5u);
	CHECK_EQ(native.find_term("title.S", "smith")->wdf, 2u);
	CHECK_EQ(native.positions_of(*native.find_term("title.S", "smith")), (std::vector<engine::termpos>{ 2, 4 }));
	CHECK_TRUE(native.find_term("title.S", "jones") == nullptr);
	// The same bytes under another field are another term; the empty term is a term.
	CHECK_TRUE(native.field("title.")->kind == FieldKind::terms);
	CHECK_EQ(native.find_term("title.", "")->wdf, 0u);

	// The log line carries the fields: it reads back to the same document.
	auto back = engine::Document::unserialise(doc.serialise());
	CHECK_EQ(back.serialise(), doc.serialise());
}

// A field with many terms finds each by its hash through the growing table, adds repeats to the same term,
// and survives interleaved fields; a Field built by hand with a repeated id still lands under its name.
static void test_strata_many_terms() {
	engine::Document doc;
	doc.set_key("Kmany");
	const auto a = doc.field("a.S", FieldKind::text);
	const auto b = doc.field("b");
	for (int round = 0; round < 3; ++round) {
		for (int i = 0; i < 300; ++i) {
			doc.add_posting(a, "term" + std::to_string(i), static_cast<engine::termpos>(round * 1000 + i), 1);
			doc.add_term(b, "x" + std::to_string(i % 40), 2);
		}
	}
	auto& native = strata_document(doc);
	CHECK_EQ(native.field("a.S")->term_count, 300u);
	CHECK_EQ(native.field("b")->term_count, 40u);
	for (int i = 0; i < 300; ++i) {
		const auto* term = native.find_term("a.S", "term" + std::to_string(i));
		CHECK_TRUE(term != nullptr);
		CHECK_EQ(term->wdf, 3u);
		CHECK_EQ(native.positions_of(*term).size(), 3u);
	}
	CHECK_EQ(native.find_term("b", "x7")->wdf, 2u * 3 * 8);  // 300 / 40 rounded: ids 7, 47, ... 287 is 8 hits per round
	CHECK_TRUE(native.find_term("b", "x40") == nullptr);
	auto back = engine::Document::unserialise(doc.serialise());
	CHECK_EQ(back.serialise(), doc.serialise());
	CHECK_EQ(strata_document(back).field("a.S")->term_count, 300u);

	// Two fields given the same id by hand are still two fields.
	engine::strata::StrataDocument raw;
	engine::Field one{"one", FieldKind::terms, 0};
	engine::Field two{"two", FieldKind::terms, 0};
	raw.add_term(one, "t", 1);
	raw.add_term(two, "t", 1);
	raw.add_term(one, "t", 1);
	CHECK_EQ(raw.fields().size(), 2u);
	CHECK_EQ(raw.find_term("one", "t")->wdf, 2u);
	CHECK_EQ(raw.find_term("two", "t")->wdf, 1u);
}

// A field ref used on a document that did not declare it trips the debug assert, for each add_*.
#ifndef NDEBUG
bool aborts(const std::function<void()>& call) {
	int fds[2];
	if (pipe(fds) != 0) { return false; }
	const pid_t pid = fork();
	if (pid == 0) {
		dup2(fds[1], STDERR_FILENO);
		close(fds[0]);
		call();
		_exit(0);
	}
	close(fds[1]);
	std::string err;
	char buffer[512];
	for (ssize_t n; (n = read(fds[0], buffer, sizeof buffer)) > 0;) { err.append(buffer, static_cast<std::size_t>(n)); }
	close(fds[0]);
	int status = 0;
	waitpid(pid, &status, 0);
	// The assert's own message, so a sanitizer's abort on the stray access does not pass for it.
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && err.find("field.id < fields_.size()") != std::string::npos;
}

void test_foreign_ref_asserts() {
	engine::Document big, empty;
	for (int i = 0; i < 10; ++i) { big.field("f" + std::to_string(i)); }
	const auto foreign = big.field("f9");
	CHECK_TRUE(aborts([&] { empty.add_term(foreign, "x"); }));
	CHECK_TRUE(aborts([&] { empty.add_boolean_term(foreign, "x"); }));
	CHECK_TRUE(aborts([&] { empty.add_posting(foreign, "x", 1); }));
	CHECK_TRUE(aborts([&] { empty.add_term(engine::FieldRef{}, "x"); }));   // a default ref on an empty document
	CHECK_FALSE(aborts([&] { big.add_term(foreign, "x"); }));               // the control: the ref's own document
}
#else
void test_foreign_ref_asserts() { }
#endif

TEST_MAIN(test_foreign_ref_asserts, test_field_refs, test_field_terms_match_model, test_marker_and_field_boundary, test_query_nodes, test_field_selects_terms, test_text_fields, test_key_field, test_host_keys, test_strata_fields, test_strata_many_terms)
