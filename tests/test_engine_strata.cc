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

// Unit tests for the strata engine shard: versioned writes by id and by key, errors by kind, get by
// id, scan, metadata, stats, transactions, persistence, replay through the serialised document, the
// key lead byte, the shared handle and the document codec.

#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "engine/error.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/strata/document.h"
#include "engine/strata/shard.h"
#include "engine/sync.h"
#include "sortable_serialise.h"

namespace {

std::string scratch(const char* name) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_strata_test_") + name + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), "body", 2);
	doc.add_posting(doc.field("f"), "word", 1);
	doc.add_posting(doc.field("f"), "word", 3);
	doc.set_data(data);
	return doc;
}

template <typename F>
engine::errc error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

engine::Opened open_shard(const std::string& path, bool writable = true, bool create = true) {
	return engine::sync_wait(engine::open_local(path, writable, create));
}

// Versioned writes, reads, errors, transactions and persistence.
void shared() {
	const auto path = scratch("shared");
	auto opened = open_shard(path);
	CHECK_TRUE(opened.created);
	auto& shard = *opened.shard;
	CHECK_FALSE(shard.uuid().empty());

	// A new document gets id 1 and version 1; the hook sees both before the write and may extend the document.
	engine::WriteArgs args;
	engine::docid hooked_did = 0;
	engine::rev hooked_version = 0;
	args.finalise = [&](engine::Document& doc, engine::WriteResult& result) {
		hooked_did = result.did;
		hooked_version = result.version;
		doc.add_term(doc.field("f"), "stamped");
	};
	auto doc = make("Qa", "one");
	auto added = engine::sync_wait(shard.add(doc, args));
	CHECK_EQ(added.did, 1u);
	CHECK_EQ(added.version, 1u);
	CHECK_EQ(hooked_did, 1u);
	CHECK_EQ(hooked_version, 1u);

	// It reads back by id and by key.
	{
		auto view = engine::sync_wait(shard.get(1));
		CHECK_EQ(view->did(), 1u);
		CHECK_EQ(view->data(), "one");
		CHECK_EQ(view->version(), 1u);
	}
	CHECK_EQ(engine::sync_wait(shard.find_key("Qa")), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Qmissing")), 0u);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(7)); }) == engine::errc::doc_not_found);

	// Replacing by id bumps the version; a stale expectation conflicts.
	engine::WriteArgs plain;
	auto doc2 = make("Qa", "two");
	CHECK_EQ(engine::sync_wait(shard.replace(1, doc2, plain)).version, 2u);
	engine::WriteArgs stale;
	stale.expected_version = 1;
	auto doc3 = make("Qa", "three");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.replace(1, doc3, stale)); }) == engine::errc::version_conflict);
	engine::WriteArgs current;
	current.expected_version = 2;
	CHECK_EQ(engine::sync_wait(shard.replace(1, doc3, current)).version, 3u);
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "three");

	// By key: a new key adds, an existing key replaces and bumps; a new document refuses a version.
	auto doc4 = make("Qb", "b1");
	auto by_key = engine::sync_wait(shard.replace_by_key("Qb", doc4, plain));
	CHECK_EQ(by_key.did, 2u);
	CHECK_EQ(by_key.version, 1u);
	CHECK_EQ(by_key.term, "Qb");
	auto doc5 = make("Qb", "b2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qb", doc5, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b2");
	engine::WriteArgs must_be_new;
	must_be_new.expected_version = 0;
	auto doc6 = make("Qc", "c1");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qc", doc6, must_be_new)).did, 3u);
	auto doc7 = make("Qc", "c2");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.replace_by_key("Qc", doc7, must_be_new)); }) == engine::errc::version_conflict);

	// Stats and scan see the writes before any commit.
	{
		auto stats = engine::sync_wait(shard.stats());
		CHECK_EQ(stats.doc_count, 3u);
		CHECK_EQ(stats.lastdocid, 3u);
		CHECK_EQ(shard.doc_count(), 3u);
		std::vector<engine::docid> seen;
		engine::sync_wait(shard.scan(2, [&](const engine::DocView& view) { seen.push_back(view.did()); }));
		CHECK_EQ(seen.size(), 2u);
		CHECK_TRUE(seen.size() == 2 && seen[0] == 2 && seen[1] == 3);
	}

	// Erasing: by id, and by key (which reports the id, and fails for a missing key or id).
	engine::sync_wait(shard.erase(1, true, std::nullopt));
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase(1, true, std::nullopt)); }) == engine::errc::doc_not_found);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase_by_key("Qb", true, 99)); }) == engine::errc::version_conflict);
	CHECK_EQ(engine::sync_wait(shard.erase_by_key("Qb", true, 2)), 2u);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)); }) == engine::errc::doc_not_found);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 1u);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::doc_not_found);

	// Ids are never reused.
	auto doc8 = make("Qd", "d1");
	CHECK_EQ(engine::sync_wait(shard.add(doc8, plain)).did, 4u);

	// Metadata: set, read, list, clear.
	engine::sync_wait(shard.set_metadata("k", "v"));
	engine::sync_wait(shard.set_metadata("k2", "v2"));
	CHECK_EQ(engine::sync_wait(shard.metadata("k")), "v");
	CHECK_EQ(engine::sync_wait(shard.metadata("absent")), "");
	{
		auto keys = engine::sync_wait(shard.metadata_keys());
		std::sort(keys.begin(), keys.end());
		CHECK_TRUE(keys == (std::vector<std::string>{"k", "k2"}));
	}
	engine::sync_wait(shard.set_metadata("k2", ""));
	CHECK_EQ(engine::sync_wait(shard.metadata_keys()).size(), 1u);

	// A commit that changed something bumps the revision; one that changed nothing does not.
	const auto before = shard.revision();
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);

	// A flushed transaction: its commit keeps the writes; its cancel drops them.
	engine::sync_wait(shard.begin_transaction(true));
	auto doc9 = make("Qe", "e1");
	CHECK_EQ(engine::sync_wait(shard.add(doc9, plain)).did, 5u);
	engine::sync_wait(shard.commit_transaction());
	CHECK_EQ(engine::sync_wait(shard.find_key("Qe")), 5u);
	engine::sync_wait(shard.begin_transaction(true));
	auto doc10 = make("Qf", "f1");
	engine::sync_wait(shard.add(doc10, plain));
	engine::sync_wait(shard.set_metadata("t", "x"));
	CHECK_EQ(engine::sync_wait(shard.find_key("Qf")), 6u);
	engine::sync_wait(shard.cancel_transaction());
	CHECK_EQ(engine::sync_wait(shard.find_key("Qf")), 0u);
	CHECK_EQ(engine::sync_wait(shard.metadata("t")), "");
	CHECK_EQ(engine::sync_wait(shard.find_key("Qe")), 5u);

	CHECK_FALSE(engine::sync_wait(shard.reopen()));

	// What was committed survives closing and opening again; the uuid is the shard's.
	const auto uuid = shard.uuid();
	const auto revision = shard.revision();
	engine::sync_wait(shard.commit(engine::Transaction::none));
	opened.shard.reset();
	auto again = open_shard(path, true, false);
	CHECK_FALSE(again.created);
	CHECK_EQ(again.shard->uuid(), uuid);
	CHECK_TRUE(again.shard->revision() >= revision);
	CHECK_EQ(engine::sync_wait(again.shard->get(4))->data(), "d1");
	CHECK_EQ(engine::sync_wait(again.shard->get(5))->data(), "e1");
	CHECK_EQ(engine::sync_wait(again.shard->metadata("k")), "v");
	CHECK_EQ(engine::sync_wait(again.shard->stats()).doc_count, 3u);

	// Opening what is missing fails with the not-found kind.
	CHECK_TRUE(error_kind([&] { open_shard(path + ".missing", true, false); }) == engine::errc::not_found);

	again.shard.reset();
	std::filesystem::remove_all(path);
}

// Replay: the log keeps the serialised document (after the write, so it carries what the write
// stamped) and replays it by id. A line the index already holds changes nothing: the same
// version, data and values; an older line does not roll a newer document back.
void replay() {
	const auto path = scratch("replay");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	// Replayed as a replace by id of an id that does not exist yet: it is created with that id.
	auto doc = make("Qr", "first");
	doc.add_value(7, "seven");
	CHECK_EQ(engine::sync_wait(shard.replace(5, doc, plain)).did, 5u);
	CHECK_EQ(engine::sync_wait(shard.get(5))->version(), 1u);
	const auto line1 = doc.serialise();
	auto replayed = engine::Document::unserialise(line1);
	CHECK_EQ(replayed.get_data(), "first");
	CHECK_EQ(replayed.get_value(7), "seven");
	CHECK_EQ(replayed.serialise(), line1);
	CHECK_EQ(engine::sync_wait(shard.find_key("Qr")), 5u);
	CHECK_EQ(engine::sync_wait(shard.stats()).lastdocid, 5u);

	// The same line, again: nothing changes, however many times it comes.
	for (int i = 0; i < 3; ++i) {
		auto again = engine::Document::unserialise(line1);
		const auto result = engine::sync_wait(shard.replace(5, again, plain));
		CHECK_EQ(result.did, 5u);
		CHECK_EQ(result.version, 1u);
		auto view = engine::sync_wait(shard.get(5));
		CHECK_EQ(view->version(), 1u);
		CHECK_EQ(view->data(), "first");
		CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 1u);
		CHECK_EQ(engine::sync_wait(shard.stats()).lastdocid, 5u);
	}

	// A newer write is not rolled back by replaying the older line; replaying the newer one is a no-op too.
	auto second = make("Qr", "second");
	CHECK_EQ(engine::sync_wait(shard.replace(5, second, plain)).version, 2u);
	const auto line2 = second.serialise();
	for (const auto& line : {line1, line2, line1, line2}) {
		auto again = engine::Document::unserialise(line);
		CHECK_EQ(engine::sync_wait(shard.replace(5, again, plain)).version, 2u);
		auto view = engine::sync_wait(shard.get(5));
		CHECK_EQ(view->version(), 2u);
		CHECK_EQ(view->data(), "second");
	}

	// The same holds by key, and a line that is ahead of the index is applied (the index is behind).
	auto by_key = engine::Document::unserialise(line2);
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qr", by_key, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(5))->version(), 2u);
	auto third = make("Qr", "third");
	CHECK_EQ(engine::sync_wait(shard.replace(5, third, plain)).version, 3u);
	CHECK_EQ(engine::sync_wait(shard.get(5))->data(), "third");

	// A document the shard stamped is a fresh write when written again, not a replay.
	auto fresh = make("Qs", "s");
	CHECK_EQ(engine::sync_wait(shard.add(fresh, plain)).version, 1u);
	CHECK_EQ(engine::sync_wait(shard.replace(6, fresh, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 2u);

	// A line that is not a document is a syntax error, not a crash.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(std::string("\xff\xff\xff", 3)); }) == engine::errc::syntax);

	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// The field table a strata shard keeps: a field is declared by the first document that names it, with the
// kind that document asked for (what its postings hold), the key field once; a later document of another
// kind reuses the declaration.
void declared() {
	const auto path = scratch("declared");
	auto opened = open_shard(path);
	auto& shard = dynamic_cast<engine::strata::Shard&>(*opened.shard);
	CHECK_EQ(shard.declared_fields().size(), 0u);

	using engine::FieldKind;
	engine::WriteArgs plain;
	engine::Document one;
	one.set_key("Kone");
	one.add_posting(one.field("a.S", FieldKind::text), "x", 1);
	one.add_term(one.field("b.N", FieldKind::terms), "y", 2);
	one.add_boolean_term(one.field("c.", FieldKind::boolean), "");
	engine::sync_wait(shard.add(one, plain));

	const auto fields = shard.declared_fields();
	CHECK_EQ(fields.size(), 4u);
	const auto find = [&](const std::string& name) -> const engine::strata::DeclaredField* {
		for (const auto& field : fields) {
			if (field.name == name) { return &field; }
		}
		return nullptr;
	};
	CHECK_TRUE(find("a.S") && find("a.S")->kind == FieldKind::text && !find("a.S")->key);
	CHECK_TRUE(find("b.N") && find("b.N")->kind == FieldKind::terms && !find("b.N")->key);
	CHECK_TRUE(find("c.") && find("c.")->kind == FieldKind::boolean && !find("c.")->key);
	CHECK_TRUE(find(std::string(engine::KEY_FIELD)) && find(std::string(engine::KEY_FIELD))->key);

	// A later document: the known fields keep their declarations whatever kind it asks for, a new field is declared
	// after them, and the key field is not declared again.
	engine::Document two;
	two.set_key("Ktwo");
	two.add_term(two.field("a.S", FieldKind::boolean), "z", 1);
	two.add_posting(two.field("d.S", FieldKind::text), "w", 1);
	engine::sync_wait(shard.add(two, plain));
	const auto later = shard.declared_fields();
	CHECK_EQ(later.size(), 5u);
	for (std::size_t i = 0; i < fields.size(); ++i) {
		CHECK_EQ(later[i].name, fields[i].name);
		CHECK_TRUE(later[i].kind == fields[i].kind);
	}
	CHECK_EQ(later[4].name, "d.S");
	CHECK_TRUE(later[4].kind == FieldKind::text);

	// The declarations survive a commit and a reopen, and a replayed log line re-declares nothing.
	engine::sync_wait(shard.commit(engine::Transaction::none));
	opened.shard.reset();
	auto again = open_shard(path, true, false);
	auto& reopened = dynamic_cast<engine::strata::Shard&>(*again.shard);
	const auto kept = reopened.declared_fields();
	CHECK_EQ(kept.size(), 5u);
	for (std::size_t i = 0; i < kept.size(); ++i) {
		CHECK_EQ(kept[i].name, later[i].name);
		CHECK_TRUE(kept[i].kind == later[i].kind);
		CHECK_EQ(kept[i].key, later[i].key);
	}
	auto line = engine::Document::unserialise(one.serialise());
	engine::sync_wait(reopened.replace_by_key("Kone", line, plain));
	CHECK_EQ(reopened.declared_fields().size(), 5u);
	again.shard.reset();
	std::filesystem::remove_all(path);
}

// The strata engine's own behavior.
void strata_only() {
	const auto path = scratch("own");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	// A field or a term that looks like a key is an ordinary one, and a key is not a term: a field called
	// "Quote" must index, and its terms are not keys.
	auto doc = make("Kone", "d");
	doc.add_term(doc.field("Quote."), "word");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Kone", doc, plain)).did, 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Kone")), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Quote.word")), 0u);
	CHECK_EQ(engine::sync_wait(shard.find_key("word")), 0u);

	// A document without a key is refused: its version would have no key to belong to.
	engine::Document keyless;
	keyless.add_term(keyless.field("f"), "x");
	keyless.set_data("k");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add(keyless, plain)); }) == engine::errc::invalid);
	auto other = make("Ktwo", "t");
	CHECK_EQ(engine::sync_wait(shard.add(other, plain)).did, 2u);

	// A key that belongs to another document cannot be replaced by id.
	auto clash = make("Kone", "c");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.replace(2, clash, plain)); }) == engine::errc::invalid);
	{
		// The shard says why itself; the index would refuse the write too, with its own words.
		std::string why;
		try {
			engine::sync_wait(shard.replace(2, clash, plain));
		} catch (const engine::Error& error) {
			why = error.description();
		}
		CHECK_TRUE(why.find("key belongs to another document") != std::string::npos);
	}
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "d");  // neither document changed
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "t");
	CHECK_EQ(engine::sync_wait(shard.get(1))->version(), 1u);
	CHECK_EQ(engine::sync_wait(shard.get(2))->version(), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Kone")), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Ktwo")), 2u);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 2u);

	// The readable handle of a shard in this process shares the writable one's index and sees its
	// writes before any commit; it cannot write.
	auto readable = open_shard(path, false, false);
	CHECK_EQ(engine::sync_wait(readable.shard->get(1))->data(), "d");
	CHECK_EQ(engine::sync_wait(readable.shard->find_key("Kone")), 1u);
	CHECK_TRUE(error_kind([&] {
		auto other = make("Qz", "z");
		engine::sync_wait(readable.shard->add(other, plain));
	}) == engine::errc::invalid);
	readable.shard.reset();
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "d");  // the writer is unaffected by the reader's close

	// Spelling is not kept.
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add_spelling("word", 1)); }) == engine::errc::not_available);

	// The strata engine keeps no term lists: a stored document does not serialise.
	CHECK_TRUE(error_kind([&] { (void)engine::sync_wait(shard.get(1))->serialise(); }) == engine::errc::syntax);

	// Search is S2's: match-all finds every document.
	{
		auto prepared = engine::prepare(engine::Query::match_all());
		engine::SearchRequest request;
		request.query = prepared.get();
		engine::Shard* shards[] = {&shard};
		CHECK_EQ(engine::sync_wait(engine::search(shards, request)).hits.size(), static_cast<std::size_t>(shard.doc_count()));
	}

	// Dropping the writable shard without a commit keeps its work.
	opened.shard.reset();
	auto reopened = open_shard(path, true, false);
	CHECK_EQ(engine::sync_wait(reopened.shard->get(1))->data(), "d");
	reopened.shard.reset();

	std::filesystem::remove_all(path);
}

// Values are the seam's opaque bytes, stored in the values plugin's columns: a document reads them back
// by slot and in slot order, an empty value is no value, the write buffer is flushed first, and a
// replacement drops the old ones.
void values_stored() {
	const auto path = scratch("values");
	auto opened = open_shard(path);
	engine::WriteArgs plain;
	auto doc = make("Qv", "d");
	doc.add_value(9, "nine");
	doc.add_value(2, std::string("t\0wo", 4));
	doc.add_value(5, "");
	CHECK_EQ(doc.get_value(9), "nine");
	engine::sync_wait(opened.shard->add(doc, plain));
	auto view = engine::sync_wait(opened.shard->get(1));
	CHECK_EQ(view->value(9), "nine");
	CHECK_EQ(view->value(2), std::string("t\0wo", 4));
	CHECK_EQ(view->value(5), "");
	CHECK_EQ(view->value(7), "");
	std::vector<std::pair<engine::valueno, std::string>> seen;
	view->values([&](engine::valueno slot, std::string_view bytes) { seen.emplace_back(slot, std::string(bytes)); });
	// The version slot (1) is stamped by the shard.
	CHECK_EQ(seen.size(), 3u);
	CHECK_EQ(seen[0].first, 1u);
	CHECK_EQ(seen[1].first, 2u);
	CHECK_EQ(seen[2].first, 9u);

	// A replacement carries its own values only.
	auto again = make("Qv", "e");
	again.add_value(3, "three");
	engine::sync_wait(opened.shard->replace_by_key("Qv", again, plain));
	engine::sync_wait(opened.shard->commit(engine::Transaction::none));
	auto replaced = engine::sync_wait(opened.shard->find_key("Qv"));
	view = engine::sync_wait(opened.shard->get(replaced));
	CHECK_EQ(view->value(3), "three");
	CHECK_EQ(view->value(9), "");
	CHECK_EQ(view->data(), "e");

	// The reserved slot is refused.
	auto reserved = make("Qr", "r");
	reserved.add_value(0xffffffffu, "x");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(opened.shard->add(reserved, plain)); }) == engine::errc::invalid);
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// The strata document's codec: round trip, and no crash or runaway allocation on damaged input.
void codec() {
	using engine::strata::StrataDocument;
	engine::Document doc;
	doc.set_key("Qk");
	doc.add_term(doc.field("f"), "a", 3);
	doc.add_posting(doc.field("f"), "b", 9);
	doc.add_posting(doc.field("f"), "b", 2);
	doc.add_posting(doc.field("f"), "b", 9);
	doc.add_posting(doc.field("g.S", engine::FieldKind::text), "c", 4);
	doc.add_term(doc.field("g.S"), "", 0);
	doc.add_term(doc.field(engine::KEY_FIELD), "Kk", 2);
	doc.add_value(1, "x");
	doc.add_value(0xfffffffe, std::string(300, 'y'));
	doc.set_data(std::string("bin\0data", 8));
	const auto bytes = doc.serialise();
	auto back = engine::Document::unserialise(bytes);
	CHECK_EQ(back.serialise(), bytes);
	CHECK_EQ(back.get_data(), std::string("bin\0data", 8));
	const auto& restored = engine::strata::strata_document(back);
	CHECK_EQ(restored.positions_of(*restored.find_term("f", "b")), (std::vector<engine::termpos>{ 2, 9 }));  // sorted and unique
	CHECK_EQ(restored.find_term("f", "a")->wdf, 3u);
	const auto& g = *restored.field("g.S");
	CHECK_TRUE(g.kind == engine::FieldKind::text);
	CHECK_EQ(g.term_count, 2u);
	CHECK_EQ(restored.find_term("g.S", "")->wdf, 0u);  // the empty term is a term
	CHECK_EQ(engine::strata::strata_document(back).key_wdf(), 2u);  // the key field's frequency, kept apart from the terms
	CHECK_EQ(engine::strata::strata_document(back).fields().size(), 2u);

	// The version the shard stamped is what the log keeps and replay reads; a fresh document has none.
	CHECK_EQ(engine::strata::strata_document(back).version(), 0u);
	CHECK_FALSE(engine::strata::strata_document(back).replayed());
	engine::strata::strata_document(back).stamp(77);
	auto stamped = engine::Document::unserialise(back.serialise());
	CHECK_EQ(engine::strata::strata_document(stamped).version(), 77u);
	CHECK_TRUE(engine::strata::strata_document(stamped).replayed());
	CHECK_EQ(stamped.serialise(), back.serialise());
	engine::strata::strata_document(back).stamp(0);

	// Every truncation fails cleanly.
	for (std::size_t n = 0; n < bytes.size(); ++n) {
		CHECK_TRUE(error_kind([&] { engine::Document::unserialise(std::string_view(bytes).substr(0, n)); }) == engine::errc::syntax);
	}
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(bytes + "x"); }) == engine::errc::syntax);
	// A count larger than the input could hold is refused before anything is allocated for it.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(std::string("\x03\x00\x00\x00\x00\xff\xff\xff\xff\x0f", 10)); }) == engine::errc::syntax);
	// A field named like the key field holds no terms of its own: it is refused.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(std::string("\x03\x00\x00\x00\x00\x00\x01\x03_id\x00\x00", 13)); }) == engine::errc::syntax);
	// Formats 1 and 2 are named and refused.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(std::string("\x02\x00\x00\x00\x00\x00", 6)); }) == engine::errc::syntax);

	// Random single-byte damage never crashes: it either decodes or raises a syntax error.
	std::mt19937 random(42);
	for (int i = 0; i < 20000; ++i) {
		auto damaged = bytes;
		damaged[random() % damaged.size()] = static_cast<char>(random());
		if (i % 3 == 0) {
			damaged.resize(random() % (damaged.size() + 1));
		}
		try {
			auto result = engine::Document::unserialise(damaged);
			(void)result.serialise();
		} catch (const engine::Error& error) {
			CHECK_TRUE(error.kind == engine::errc::syntax);
		}
	}
}

// A read by id sees every write of this handle at once: the index is refreshed when a write left it
// dirty, and only then.
void fresh_reads() {
	const auto path = scratch("fresh");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	auto one = make("Qa", "one");
	CHECK_EQ(engine::sync_wait(shard.add(one, plain)).did, 1u);
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "one");  // flushes the write; the index is clean now

	// A replace after a clean read is seen, with its version.
	auto two = make("Qa", "two");
	CHECK_EQ(engine::sync_wait(shard.replace(1, two, plain)).version, 2u);
	{
		auto view = engine::sync_wait(shard.get(1));
		CHECK_EQ(view->data(), "two");
		CHECK_EQ(view->version(), 2u);
	}

	// An erase after a clean read is seen, by id and by key.
	engine::sync_wait(shard.erase(1, true, std::nullopt));
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::doc_not_found);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 0u);
	auto b = make("Qb", "b");
	CHECK_EQ(engine::sync_wait(shard.add(b, plain)).did, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b");
	CHECK_EQ(engine::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)), 2u);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(2)); }) == engine::errc::doc_not_found);
	CHECK_EQ(shard.doc_count(), 0u);

	// A replace by key after a clean read.
	auto c1 = make("Qc", "c1");
	CHECK_EQ(engine::sync_wait(shard.add(c1, plain)).did, 3u);
	CHECK_EQ(engine::sync_wait(shard.get(3))->data(), "c1");
	auto c2 = make("Qc", "c2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qc", c2, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(3))->data(), "c2");
	std::vector<std::string> scanned;
	engine::sync_wait(shard.scan(0, [&](const engine::DocView& view) { scanned.push_back(view.data()); }));
	CHECK_TRUE(scanned == (std::vector<std::string>{"c2"}));

	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// Closing the last handle commits what is pending: documents, erases and
// metadata survive; a handle that wrote nothing leaves the revision alone.
void commit_on_drop() {
	const auto path = scratch("drop");
	engine::WriteArgs plain;
	{
		auto opened = open_shard(path);
		auto& shard = *opened.shard;
		auto a = make("Qa", "a");
		auto b = make("Qb", "b");
		engine::sync_wait(shard.add(a, plain));
		engine::sync_wait(shard.add(b, plain));
		engine::sync_wait(shard.commit(engine::Transaction::none));
		engine::sync_wait(shard.erase(1, true, std::nullopt));
		engine::sync_wait(shard.set_metadata("m", "v"));
		auto c = make("Qc", "c");
		engine::sync_wait(shard.add(c, plain));
	}
	engine::rev revision = 0;
	{
		auto reopened = open_shard(path, true, false);
		auto& shard = *reopened.shard;
		CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::doc_not_found);
		CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b");
		CHECK_EQ(engine::sync_wait(shard.get(3))->data(), "c");
		CHECK_EQ(engine::sync_wait(shard.metadata("m")), "v");
		CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 2u);
		revision = shard.revision();
		CHECK_GE(revision, 2u);
	}
	{
		auto reopened = open_shard(path, true, false);
		CHECK_EQ(reopened.shard->revision(), revision);  // nothing was written, so nothing committed
	}
	std::filesystem::remove_all(path);
}

// Writers on one shard that check and set: a write with a stale expectation conflicts, the ones that win
// get each version once, no update is lost, and a key that must be new is created by exactly one.
void concurrent_cas() {
	const auto path = scratch("cas");
	// The threads are the host of this shard: its writes take turns, and each is refreshed before the turn ends;
	// reads take no turn.
	engine::OpenOptions host;
	host.host = true;
	auto opened = engine::sync_wait(engine::open_local(path, true, true, host));
	auto& shard = *opened.shard;
	std::mutex turn;
	const auto written = [&](auto&& write) {
		std::lock_guard<std::mutex> lock(turn);
		auto result = write();
		engine::sync_wait(shard.refresh());
		return result;
	};
	engine::WriteArgs plain;

	constexpr int keys = 3;
	constexpr int threads = 8;
	constexpr int wins_wanted = 150;  // successful updates per thread
	for (int k = 0; k < keys; ++k) {
		auto doc = make("Qk" + std::to_string(k), "seed");
		CHECK_EQ(written([&] { return engine::sync_wait(shard.add(doc, plain)); }).did, static_cast<engine::docid>(k + 1));
	}

	struct Win {
		int key;
		engine::rev version;
		std::string data;
	};
	std::vector<std::vector<Win>> wins(threads);
	std::vector<long> conflicts(threads, 0);
	std::vector<std::string> unexpected(threads);
	std::atomic<bool> go{false};
	std::vector<std::thread> workers;
	for (int t = 0; t < threads; ++t) {
		workers.emplace_back([&, t] {
			while (!go.load()) { }
			std::mt19937 random(t * 7919 + 1);
			try {
				while (static_cast<int>(wins[t].size()) < wins_wanted) {
					const int k = static_cast<int>(random() % keys);
					const auto did = static_cast<engine::docid>(k + 1);
					const auto seen = engine::sync_wait(shard.get(did))->version();
					const auto data = "t" + std::to_string(t) + "n" + std::to_string(wins[t].size());
					auto doc = make("Qk" + std::to_string(k), data);
					engine::WriteArgs args;
					args.expected_version = seen;
					try {
						const auto result = written([&] { return engine::sync_wait(shard.replace(did, doc, args)); });
						wins[t].push_back({k, result.version, data});
					} catch (const engine::Error& error) {
						if (error.kind != engine::errc::version_conflict) {
							throw;
						}
						++conflicts[t];
					}
				}
			} catch (const std::exception& exc) {
				unexpected[t] = exc.what();
			}
		});
	}
	go = true;
	for (auto& worker : workers) {
		worker.join();
	}

	long total_conflicts = 0;
	for (int t = 0; t < threads; ++t) {
		CHECK_TRUE(unexpected[t].empty());
		CHECK_EQ(static_cast<int>(wins[t].size()), wins_wanted);
		total_conflicts += conflicts[t];
	}
	CHECK_GT(total_conflicts, 0);  // the writers did contend
	for (int k = 0; k < keys; ++k) {
		std::map<engine::rev, std::string> by_version;
		for (const auto& list : wins) {
			for (const auto& win : list) {
				if (win.key == k) {
					CHECK_TRUE(by_version.emplace(win.version, win.data).second);  // each version won once
				}
			}
		}
		// the versions that won are exactly 2..final: nothing lost, nothing skipped
		const auto final_version = static_cast<engine::rev>(by_version.size()) + 1;
		CHECK_TRUE(!by_version.empty() && by_version.begin()->first == 2 && by_version.rbegin()->first == final_version);
		auto view = engine::sync_wait(shard.get(static_cast<engine::docid>(k + 1)));
		CHECK_EQ(view->version(), final_version);
		CHECK_EQ(view->data(), by_version.rbegin()->second);
	}

	// A key that must be new: exactly one of the racing creators wins, the rest conflict.
	for (int round = 0; round < 20; ++round) {
		const auto key = "Qnew" + std::to_string(round);
		std::atomic<int> created{0}, refused{0}, other{0};
		std::atomic<bool> start{false};
		std::vector<std::thread> racers;
		for (int t = 0; t < threads; ++t) {
			racers.emplace_back([&, t] {
				while (!start.load()) { }
				auto doc = make(key, "by" + std::to_string(t));
				engine::WriteArgs args;
				args.expected_version = 0;
				try {
					written([&] { return engine::sync_wait(shard.replace_by_key(key, doc, args)); });
					++created;
				} catch (const engine::Error& error) {
					++(error.kind == engine::errc::version_conflict ? refused : other);
				}
			});
		}
		start = true;
		for (auto& racer : racers) {
			racer.join();
		}
		CHECK_EQ(created.load(), 1);
		CHECK_EQ(refused.load(), threads - 1);
		CHECK_EQ(other.load(), 0);
	}

	// Erasing at a version: of the racers that name the current one, exactly one removes the document.
	{
		const auto current = engine::sync_wait(shard.get(1))->version();
		std::atomic<int> erased{0}, conflicted{0}, missing{0};
		std::atomic<bool> start{false};
		std::vector<std::thread> racers;
		for (int t = 0; t < threads; ++t) {
			racers.emplace_back([&] {
				while (!start.load()) { }
				try {
					written([&] { engine::sync_wait(shard.erase_by_key("Qk0", true, current)); return 0; });
					++erased;
				} catch (const engine::Error& error) {
					++(error.kind == engine::errc::version_conflict ? conflicted : missing);
				}
			});
		}
		start = true;
		for (auto& racer : racers) {
			racer.join();
		}
		CHECK_EQ(erased.load(), 1);
		CHECK_EQ(erased.load() + conflicted.load() + missing.load(), threads);
		CHECK_EQ(engine::sync_wait(shard.find_key("Qk0")), 0u);
	}

	opened.shard.reset();
	std::filesystem::remove_all(path);
}


// A writer dropped in the middle of a transaction discards it, on both engines: only the commit before it
// is on disk when the shard is opened again.
void drop_mid_transaction() {
	const auto path = scratch("txdrop");
	engine::WriteArgs plain;
	{
		auto opened = open_shard(path);
		auto a = make("Qa", "committed");
		engine::sync_wait(opened.shard->add(a, plain));
		engine::sync_wait(opened.shard->commit(engine::Transaction::none));
		engine::sync_wait(opened.shard->begin_transaction(true));
		auto b = make("Qb", "in the transaction");
		engine::sync_wait(opened.shard->add(b, plain));
	}
	auto reopened = open_shard(path, true, false);
	CHECK_EQ(engine::sync_wait(reopened.shard->stats()).doc_count, 1u);
	CHECK_EQ(engine::sync_wait(reopened.shard->find_key("Qb")), 0u);
	// A cancel then has nothing of the half transaction to come back to.
	engine::sync_wait(reopened.shard->begin_transaction(true));
	engine::sync_wait(reopened.shard->cancel_transaction());
	CHECK_EQ(engine::sync_wait(reopened.shard->stats()).doc_count, 1u);
	reopened.shard.reset();
	std::filesystem::remove_all(path);
}

bool can_deny_access() { return ::geteuid() != 0; }

// A cancel whose reopen fails leaves a shard that raises a database error on every call, never one that
// reads freed state; once the directory is readable again a reopen brings back the last commit.
void cancel_reopen_fails() {
	if (!can_deny_access()) {
		return;
	}
	const auto path = scratch("cancelfail");
	engine::WriteArgs plain;
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	auto a = make("Qa", "committed");
	engine::sync_wait(shard.add(a, plain));
	engine::sync_wait(shard.commit(engine::Transaction::none));
	engine::sync_wait(shard.begin_transaction(true));
	auto b = make("Qb", "in the transaction");
	engine::sync_wait(shard.add(b, plain));

	const auto dir = path + "/strata";
	::chmod(dir.c_str(), 0000);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.cancel_transaction()); }) == engine::errc::database);
	::chmod(dir.c_str(), 0700);

	auto c = make("Qc", "after the failure");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.stats()); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.find_key("Qa")); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1)); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add(c, plain)); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.commit(engine::Transaction::none)); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.metadata("m")); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { (void)shard.doc_count(); }) == engine::errc::database);
	(void)shard.revision();  // the owner's last published figure: it is read without the index

	CHECK_TRUE(engine::sync_wait(shard.reopen()));
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Qb")), 0u);
	CHECK_EQ(engine::sync_wait(shard.add(c, plain)).version, 1u);
	engine::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 2u);
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// One transient I/O error fences the index; the next writable handle, or a reopen, gets a working one from
// the last commit, whatever handles are still open.
void fenced_recovers(bool through_reopen) {
	if (!can_deny_access()) {
		return;
	}
	const auto path = scratch(through_reopen ? "fenceb" : "fencea");
	engine::WriteArgs plain;
	auto writer = open_shard(path);
	auto reader = open_shard(path, false, false);
	auto a = make("Qa", "committed");
	engine::sync_wait(writer.shard->add(a, plain));
	engine::sync_wait(writer.shard->commit(engine::Transaction::none));
	auto b = make("Qb", "lost with the error");
	engine::sync_wait(writer.shard->add(b, plain));

	const auto dir = path + "/strata";
	::chmod(dir.c_str(), 0500);  // no new files
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->commit(engine::Transaction::none)); }) == engine::errc::database);
	::chmod(dir.c_str(), 0700);
	auto c = make("Qc", "refused while fenced");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->add(c, plain)); }) == engine::errc::database);

	std::unique_ptr<engine::Shard> next;
	if (through_reopen) {
		// A writer's unsaved writes went with the fenced index: reopen() tells it to close and open again (the log replays).
		CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->reopen()); }) == engine::errc::reopen);
		writer.shard.reset();
		next = std::move(open_shard(path, true, false).shard);
	} else {
		next = std::move(open_shard(path, true, false).shard);  // the reader and the fenced writer still live
	}
	auto d = make("Qd", "after the recovery");
	CHECK_EQ(engine::sync_wait(next->add(d, plain)).version, 1u);
	engine::sync_wait(next->commit(engine::Transaction::none));
	CHECK_EQ(engine::sync_wait(next->find_key("Qb")), 0u);  // what the fenced index held is gone (the WAL replays it on a node)
	CHECK_EQ(engine::sync_wait(next->stats()).doc_count, 2u);
	CHECK_EQ(engine::sync_wait(reader.shard->stats()).doc_count, 2u);
	next.reset();
	writer.shard.reset();
	reader.shard.reset();
	std::filesystem::remove_all(path);
}

// NF1: a handle that is not the writer replaces a fenced index while the writer holds unsaved writes. The writer
// must hear of it (its writes are gone, its log lines are not): every call raises, reopen() asks to close and open
// again, and the writable open that follows works on the last commit.
void fenced_writer_notified() {
	if (!can_deny_access()) {
		return;
	}
	const auto path = scratch("fencec");
	engine::WriteArgs plain;
	auto writer = open_shard(path);
	auto reader = open_shard(path, false, false);
	auto a = make("Qa", "committed");
	engine::sync_wait(writer.shard->add(a, plain));
	engine::sync_wait(writer.shard->commit(engine::Transaction::none));
	auto b = make("Qb", "acknowledged, not committed");
	engine::sync_wait(writer.shard->add(b, plain));

	const auto dir = path + "/strata";
	::chmod(dir.c_str(), 0500);  // the reader's flush of the writer's buffer cannot write
	CHECK_TRUE(error_kind([&] { engine::sync_wait(reader.shard->stats()); }) == engine::errc::database);
	::chmod(dir.c_str(), 0700);
	auto joined = open_shard(path, false, false);  // replaces the fenced index

	auto c = make("Qc", "after the replacement");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->find_key("Qb")); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->add(c, plain)); }) == engine::errc::database);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->commit(engine::Transaction::none)); }) == engine::errc::database);
	(void)writer.shard->revision();
	CHECK_TRUE(error_kind([&] { engine::sync_wait(writer.shard->reopen()); }) == engine::errc::reopen);
	CHECK_EQ(engine::sync_wait(joined.shard->stats()).doc_count, 1u);  // readers keep working

	writer.shard.reset();
	auto next = open_shard(path, true, false);
	CHECK_EQ(engine::sync_wait(next.shard->add(c, plain)).version, 1u);
	engine::sync_wait(next.shard->commit(engine::Transaction::none));
	CHECK_EQ(engine::sync_wait(next.shard->stats()).doc_count, 2u);
	CHECK_EQ(engine::sync_wait(next.shard->find_key("Qb")), 0u);  // the log replays it on a node
	next.shard.reset();
	joined.shard.reset();
	reader.shard.reset();
	std::filesystem::remove_all(path);
}

// A scripted log on the strata engine: each write keeps the line the WAL would hold.
struct Logged {
	enum { put, erase } type;
	engine::docid did;
	std::string line;
};

struct Primary {
	engine::Opened opened;
	std::vector<Logged> log;
	engine::WriteArgs plain;

	explicit Primary(const std::string& path) : opened(open_shard(path)) { }

	void write(const std::string& key, const std::string& data) {
		auto doc = make(key, data);
		const auto result = engine::sync_wait(opened.shard->replace_by_key(key, doc, plain));
		log.push_back({Logged::put, result.did, doc.serialise()});
	}
	void drop(engine::docid did) {
		engine::sync_wait(opened.shard->erase(did, true, std::nullopt));
		log.push_back({Logged::erase, did, {}});
	}
};

std::string shard_state(engine::Shard& shard) {
	std::string state;
	const auto last = engine::sync_wait(shard.stats()).lastdocid;
	for (engine::docid did = 1; did <= last; ++did) {
		try {
			auto view = engine::sync_wait(shard.get(did));
			state += std::to_string(did) + "=v" + std::to_string(view->version()) + ":" + view->data() + " ";
		} catch (const engine::Error& error) {
			CHECK_TRUE(error.kind == engine::errc::doc_not_found);
		}
	}
	return state + "last=" + std::to_string(last);
}

// What the WAL does with a line: a replace by id from the document, an erase that tolerates a document that is already gone.
void replay_line(engine::Shard& shard, const Logged& line) {
	engine::WriteArgs plain;
	if (line.type == Logged::put) {
		auto doc = engine::Document::unserialise(line.line);
		engine::sync_wait(shard.replace(line.did, doc, plain));
	} else {
		try {
			engine::sync_wait(shard.erase(line.did, false, std::nullopt));
		} catch (const engine::Error& error) {
			if (error.kind != engine::errc::doc_not_found) {
				throw;
			}
		}
	}
}

// The log applied again over the state it produced changes nothing, including a key that was erased and
// added again (it lives at a new id by then).
void double_replay(const char* name, const std::function<void(Primary&)>& script) {
	Primary primary(scratch(name));
	script(primary);
	const auto want = shard_state(*primary.opened.shard);
	for (int pass = 0; pass < 2; ++pass) {
		for (const auto& line : primary.log) {
			replay_line(*primary.opened.shard, line);
		}
		CHECK_EQ(shard_state(*primary.opened.shard), want);
	}
}

void replay_idempotent() {
	double_replay("dr_plain", [](Primary& p) { p.write("Qa", "a1"); p.write("Qa", "a2"); p.write("Qb", "b1"); });
	double_replay("dr_erase", [](Primary& p) { p.write("Qa", "a1"); p.write("Qb", "b1"); p.drop(1); });
	double_replay("dr_readd", [](Primary& p) { p.write("Qa", "a1"); p.drop(1); p.write("Qa", "a2"); });
	double_replay("dr_readd_replace", [](Primary& p) { p.write("Qa", "a1"); p.drop(1); p.write("Qa", "a2"); p.write("Qa", "a3"); });
	double_replay("dr_swap", [](Primary& p) { p.write("Qa", "a1"); p.write("Qb", "b1"); p.drop(1); p.write("Qa", "a2"); p.drop(2); });

	// A line that is not the next version of the document is refused, not applied as the next one: the
	// replica below never saw version 2.
	Primary primary(scratch("gap_primary"));
	primary.write("Qg", "v1");
	primary.write("Qg", "v2");
	primary.write("Qg", "v3");
	auto replica = open_shard(scratch("gap_replica"));
	replay_line(*replica.shard, primary.log[0]);
	CHECK_TRUE(error_kind([&] { replay_line(*replica.shard, primary.log[2]); }) == engine::errc::corrupt);
	CHECK_EQ(shard_state(*replica.shard), "1=v1:v1 last=1");
	replay_line(*replica.shard, primary.log[1]);
	replay_line(*replica.shard, primary.log[2]);
	replay_line(*replica.shard, primary.log[0]);  // older lines are no-ops
	CHECK_EQ(shard_state(*replica.shard), "1=v3:v3 last=1");
	// A document that is gone takes a line only at version 1.
	auto fresh = open_shard(scratch("gap_fresh"));
	CHECK_TRUE(error_kind([&] { replay_line(*fresh.shard, primary.log[1]); }) == engine::errc::corrupt);
	CHECK_EQ(engine::sync_wait(fresh.shard->stats()).doc_count, 0u);
}

// A log line from before fields (formats 1 and 2) is named as such; the current format is 3.
void old_format() {
	const auto old_line = std::string("\x01\x01Qk\x01x\x00\x00", 8);
	for (const auto& line : {old_line, std::string("\x02\x00\x00\x00\x00\x00", 6)}) {
		try {
			engine::Document::unserialise(line);
			CHECK_TRUE(false);
		} catch (const engine::Error& error) {
			CHECK_TRUE(error.kind == engine::errc::syntax);
			CHECK_TRUE(error.description().find("predate fields") != std::string::npos);
		}
	}
	try {
		engine::Document::unserialise(std::string("\x09", 1));
		CHECK_TRUE(false);
	} catch (const engine::Error& error) {
		CHECK_TRUE(error.description().find("unknown version") != std::string::npos);
	}
	engine::Document doc;
	doc.set_key("Qk");
	CHECK_EQ(static_cast<unsigned char>(doc.serialise()[0]), 3u);
}

// The version slot the host reads back to check a write holds the version the write landed at, whatever
// the caller put there to ask for a version (R1: a stale slot made every later versioned write conflict).
void version_slot() {
	const auto path = scratch("version_slot");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	const auto slot = [&](engine::docid did) { return engine::sync_wait(shard.get(did))->value(1); };
	engine::WriteArgs plain;
	auto doc = make("Qa", "one");
	doc.add_value(1, sortable_serialise(0.0));   // what the host asks for on a new document
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qa", doc, plain)).version, 1u);
	CHECK_EQ(slot(1), sortable_serialise(1.0));
	for (engine::rev v = 1; v < 4; ++v) {
		engine::WriteArgs expect;
		expect.expected_version = v;
		auto next = make("Qa", "again");
		next.add_value(1, sortable_serialise(static_cast<double>(v)));
		CHECK_EQ(engine::sync_wait(shard.replace(1, next, expect)).version, v + 1);
		CHECK_EQ(slot(1), sortable_serialise(static_cast<double>(v + 1)));
	}
	auto added = make("Qb", "b");
	CHECK_EQ(engine::sync_wait(shard.add(added, plain)).version, 1u);
	CHECK_EQ(slot(2), sortable_serialise(1.0));
}

// The stats report the shortest and longest document length of the shard (D3).
void length_bounds() {
	const auto path = scratch("length_bounds");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	CHECK_EQ(engine::sync_wait(shard.stats()).doclen_upper, 0u);
	engine::WriteArgs plain;
	for (const auto& [key, wdf] : {std::pair{"Qa", 3u}, {"Qb", 9u}, {"Qc", 5u}}) {
		engine::Document doc;
		doc.set_key(key);
		doc.add_term(doc.field("f"), "t", wdf);
		engine::sync_wait(shard.add(doc, plain));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));
	const auto stats = engine::sync_wait(shard.stats());
	CHECK_EQ(stats.doclen_lower, 3u);
	CHECK_EQ(stats.doclen_upper, 9u);
	CHECK_EQ(stats.total_length, 17u);
}

// The integrity check passes on a good shard, and a flipped byte fails it.
void check_shard() {
	const auto path = scratch("check");
	{
		auto opened = open_shard(path);
		engine::WriteArgs plain;
		auto doc = make("Qa", std::string(4000, 'x'));
		engine::sync_wait(opened.shard->add(doc, plain));
		engine::sync_wait(opened.shard->commit(engine::Transaction::none));
	}
	const auto good = engine::sync_wait(engine::check(path));
	CHECK_EQ(good.errors, 0u);
	CHECK_EQ(good.failure, "");
	// Flips one byte in the largest file under the shard.
	namespace fs = std::filesystem;
	fs::path biggest;
	std::uintmax_t size = 0;
	for (const auto& entry : fs::recursive_directory_iterator(path)) {
		if (entry.is_regular_file() && entry.file_size() > size && entry.path().filename() != "LOCK") {
			biggest = entry.path();
			size = entry.file_size();
		}
	}
	CHECK_TRUE(size > 64);
	{
		std::fstream f(biggest, std::ios::in | std::ios::out | std::ios::binary);
		f.seekg(static_cast<std::streamoff>(size / 2));
		char c = 0;
		f.get(c);
		f.seekp(static_cast<std::streamoff>(size / 2));
		f.put(static_cast<char>(c ^ 0x5a));
	}
	const auto bad = engine::sync_wait(engine::check(path));
	CHECK_TRUE(bad.errors != 0 || !bad.failure.empty());
}

// The current version a version conflict reports (nullopt: no conflict; 999: a conflict without one).
template <typename F>
std::optional<engine::rev> conflict_version(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		if (error.kind == engine::errc::version_conflict) {
			return error.current_version.value_or(999);
		}
		throw;
	}
	return std::nullopt;
}

// A version conflict reports the document's version, 0 when it does not exist; an expected version on a missing
// document conflicts before the miss (Elasticsearch's if_seq_no), and none or 0 leaves it a miss.
void conflicts() {
	const auto path = scratch("conflicts");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;
	auto a = make("Ka", "a");
	CHECK_EQ(engine::sync_wait(shard.add(a, plain)).did, 1u);
	auto a2 = make("Ka", "a2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Ka", a2, plain)).version, 2u);
	const auto expecting = [](engine::rev v) {
		engine::WriteArgs args;
		args.expected_version = v;
		return args;
	};

	auto stale = make("Ka", "stale");
	CHECK(conflict_version([&] { engine::sync_wait(shard.replace(1, stale, expecting(1))); }) == std::optional<engine::rev>(2));
	CHECK(conflict_version([&] { engine::sync_wait(shard.replace_by_key("Ka", stale, expecting(3))); }) == std::optional<engine::rev>(2));
	auto missing = make("Kmissing", "m");
	CHECK(conflict_version([&] { engine::sync_wait(shard.replace_by_key("Kmissing", missing, expecting(1))); }) == std::optional<engine::rev>(0));
	CHECK_EQ(engine::sync_wait(shard.find_key("Kmissing")), 0u);

	CHECK(conflict_version([&] { engine::sync_wait(shard.erase(1, true, 1)); }) == std::optional<engine::rev>(2));
	CHECK(conflict_version([&] { engine::sync_wait(shard.erase_by_key("Ka", true, 3)); }) == std::optional<engine::rev>(2));
	CHECK(conflict_version([&] { engine::sync_wait(shard.erase_by_key("Ka", true, 0)); }) == std::optional<engine::rev>(2));
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "a2");   // nothing changed
	CHECK(conflict_version([&] { engine::sync_wait(shard.erase(9, true, 1)); }) == std::optional<engine::rev>(0));
	CHECK(conflict_version([&] { engine::sync_wait(shard.erase_by_key("Kmissing", true, 4)); }) == std::optional<engine::rev>(0));
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase(9, true, 0)); }) == engine::errc::doc_not_found);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase_by_key("Kmissing", true, 0)); }) == engine::errc::doc_not_found);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.erase_by_key("Kmissing", true, std::nullopt)); }) == engine::errc::doc_not_found);

	CHECK_EQ(engine::sync_wait(shard.erase_by_key("Ka", true, 2)), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Ka")), 0u);
}

// A new key with a docid to take takes it when it is free, and goes after the last when another document holds it;
// a key that exists stays where it is. A replace by an id that holds another key's document refuses and leaves it.
void placement() {
	const auto path = scratch("placement");
	auto opened = open_shard(path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;
	const auto at = [](engine::docid did) {
		engine::WriteArgs args;
		args.new_did = did;
		return args;
	};
	auto b = make("Kb", "b");
	auto placed = engine::sync_wait(shard.replace_by_key("Kb", b, at(5)));
	CHECK_EQ(placed.did, 5u);
	CHECK_EQ(placed.version, 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Kb")), 5u);
	CHECK_EQ(engine::sync_wait(shard.stats()).lastdocid, 5u);

	auto c = make("Kc", "c");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Kc", c, at(5))).did, 6u);   // 5 is Kb's
	CHECK_EQ(engine::sync_wait(shard.get(5))->data(), "b");
	auto b2 = make("Kb", "b2");
	const auto again = engine::sync_wait(shard.replace_by_key("Kb", b2, at(7)));
	CHECK_EQ(again.did, 5u);
	CHECK_EQ(again.version, 2u);
	auto d = make("Kd", "d");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Kd", d, at(3))).did, 3u);   // a free id under the last
	CHECK_EQ(engine::sync_wait(shard.stats()).lastdocid, 6u);
	auto e = make("Ke", "e");
	CHECK_EQ(engine::sync_wait(shard.add(e, plain)).did, 7u);

	auto clash = make("Kz", "z");
	CHECK(conflict_version([&] { engine::sync_wait(shard.replace(5, clash, plain)); }) == std::optional<engine::rev>(0));
	CHECK_EQ(engine::sync_wait(shard.get(5))->data(), "b2");
	CHECK_EQ(engine::sync_wait(shard.get(5))->version(), 2u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Kz")), 0u);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 4u);
}

}  // namespace

// An engine error that escapes a test is a failure with its text, not an abort.
template <typename F>
static void guarded(const char* name, F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		std::fprintf(stderr, "%s: engine error (%s): %s\n", name, engine::to_string(error.kind), error.description().c_str());
		CHECK_TRUE(false);
	}
}

static void test_shared() { guarded("shared", [] { shared(); }); }
static void test_replay() { guarded("replay", [] { replay(); }); }
static void test_declared() { guarded("declared fields", [] { declared(); }); }
static void test_strata_only() { guarded("strata only", [] { strata_only(); }); }
static void test_values_stored() { guarded("values stored", [] { values_stored(); }); }
static void test_fresh_reads() { guarded("fresh reads", [] { fresh_reads(); }); }
static void test_commit_on_drop() { guarded("commit on drop", [] { commit_on_drop(); }); }
static void test_concurrent_cas() { guarded("concurrent cas", [] { concurrent_cas(); }); }
static void test_drop_mid_transaction() { guarded("drop mid transaction", [] { drop_mid_transaction(); }); }
static void test_cancel_reopen_fails() { guarded("cancel reopen fails", [] { cancel_reopen_fails(); }); }
static void test_fenced_new_handle() { guarded("fenced new handle", [] { fenced_recovers(false); }); }
static void test_fenced_reopen() { guarded("fenced reopen", [] { fenced_recovers(true); }); }
static void test_fenced_writer_notified() { guarded("fenced writer notified", [] { fenced_writer_notified(); }); }
static void test_replay_idempotent() { guarded("replay idempotent", [] { replay_idempotent(); }); }
static void test_old_format() { guarded("old format", [] { old_format(); }); }
static void test_check() { guarded("check", [] { check_shard(); }); }
static void test_version_slot() { guarded("version slot", [] { version_slot(); }); }
static void test_conflicts() { guarded("conflicts", [] { conflicts(); }); }
static void test_placement() { guarded("placement", [] { placement(); }); }
static void test_length_bounds() { guarded("length bounds", [] { length_bounds(); }); }
static void test_codec() { guarded("codec", [] { codec(); }); }

TEST_MAIN(test_shared, test_replay, test_declared, test_strata_only, test_values_stored, test_codec, test_fresh_reads, test_commit_on_drop, test_concurrent_cas, test_drop_mid_transaction, test_cancel_reopen_fails, test_fenced_new_handle, test_fenced_reopen, test_fenced_writer_notified, test_replay_idempotent, test_old_format, test_check, test_version_slot, test_conflicts, test_placement, test_length_bounds)
