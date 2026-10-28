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

// Unit tests for the engine shard on both engines: what they share (versioned writes by id and by
// key, errors by kind, get by id, scan, metadata, stats, transactions, persistence, replay through
// the serialised document), then what is the strata engine's own (the key lead byte, the shared
// handle, the document codec, the engine chosen per shard).

#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "engine/error.h"
#include "engine/kind.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/strata/document.h"
#include "engine/sync.h"

namespace {

using engine::Kind;

std::string scratch(const char* name, Kind kind) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_strata_test_") + name + "_" + engine::to_string(kind) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(Kind kind, const std::string& key, const std::string& data) {
	engine::Document doc(kind);
	doc.set_key(key);
	doc.add_term("body", 2);
	doc.add_posting("word", 1);
	doc.add_posting("word", 3);
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

engine::Opened open_shard(Kind kind, const std::string& path, bool writable = true, bool create = true) {
	return engine::sync_wait(engine::open_local(kind, path, writable, create));
}

// What the two engines share.
void shared(Kind kind) {
	const auto path = scratch("shared", kind);
	auto opened = open_shard(kind, path);
	CHECK_TRUE(opened.created);
	auto& shard = *opened.shard;
	CHECK_TRUE(shard.kind() == kind);
	CHECK_FALSE(shard.uuid().empty());

	// A new document gets id 1 and version 1; the hook sees both before the write and may extend the document.
	engine::WriteArgs args;
	engine::docid hooked_did = 0;
	engine::rev hooked_version = 0;
	args.finalise = [&](engine::Document& doc, engine::WriteResult& result) {
		hooked_did = result.did;
		hooked_version = result.version;
		doc.add_term("stamped");
	};
	auto doc = make(kind, "Qa", "one");
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
	auto doc2 = make(kind, "Qa", "two");
	CHECK_EQ(engine::sync_wait(shard.replace(1, doc2, plain)).version, 2u);
	engine::WriteArgs stale;
	stale.expected_version = 1;
	auto doc3 = make(kind, "Qa", "three");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.replace(1, doc3, stale)); }) == engine::errc::version_conflict);
	engine::WriteArgs current;
	current.expected_version = 2;
	CHECK_EQ(engine::sync_wait(shard.replace(1, doc3, current)).version, 3u);
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "three");

	// By key: a new key adds, an existing key replaces and bumps; a new document refuses a version.
	auto doc4 = make(kind, "Qb", "b1");
	auto by_key = engine::sync_wait(shard.replace_by_key("Qb", doc4, plain));
	CHECK_EQ(by_key.did, 2u);
	CHECK_EQ(by_key.version, 1u);
	CHECK_EQ(by_key.term, "Qb");
	auto doc5 = make(kind, "Qb", "b2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qb", doc5, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b2");
	engine::WriteArgs must_be_new;
	must_be_new.expected_version = 0;
	auto doc6 = make(kind, "Qc", "c1");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qc", doc6, must_be_new)).did, 3u);
	auto doc7 = make(kind, "Qc", "c2");
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
	auto doc8 = make(kind, "Qd", "d1");
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
	auto doc9 = make(kind, "Qe", "e1");
	CHECK_EQ(engine::sync_wait(shard.add(doc9, plain)).did, 5u);
	engine::sync_wait(shard.commit_transaction());
	CHECK_EQ(engine::sync_wait(shard.find_key("Qe")), 5u);
	engine::sync_wait(shard.begin_transaction(true));
	auto doc10 = make(kind, "Qf", "f1");
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
	auto again = open_shard(kind, path, true, false);
	CHECK_FALSE(again.created);
	CHECK_EQ(again.shard->uuid(), uuid);
	CHECK_TRUE(again.shard->revision() >= revision);
	CHECK_EQ(engine::sync_wait(again.shard->get(4))->data(), "d1");
	CHECK_EQ(engine::sync_wait(again.shard->get(5))->data(), "e1");
	CHECK_EQ(engine::sync_wait(again.shard->metadata("k")), "v");
	CHECK_EQ(engine::sync_wait(again.shard->stats()).doc_count, 3u);

	// Opening what is missing fails with the not-found kind.
	CHECK_TRUE(error_kind([&] { open_shard(kind, path + ".missing", true, false); }) == engine::errc::not_found);

	again.shard.reset();
	std::filesystem::remove_all(path);
}

// Replay: the log keeps the serialised document (after the write, so it carries what the write
// stamped) and replays it by id. On strata a line the index already holds changes nothing: the same
// version, data and values; an older line does not roll a newer document back. On Xapian the shard
// versions every replace and the log is protected by the revision check in `DatabaseWAL::execute_line`.
void replay(Kind kind) {
	const auto path = scratch("replay", kind);
	auto opened = open_shard(kind, path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	// Replayed as a replace by id of an id that does not exist yet: it is created with that id.
	auto doc = make(kind, "Qr", "first");
	doc.add_value(7, "seven");
	CHECK_EQ(engine::sync_wait(shard.replace(5, doc, plain)).did, 5u);
	CHECK_EQ(engine::sync_wait(shard.get(5))->version(), 1u);
	const auto line1 = doc.serialise();
	auto replayed = engine::Document::unserialise(kind, line1);
	CHECK_EQ(replayed.get_data(), "first");
	CHECK_EQ(replayed.get_value(7), "seven");
	CHECK_EQ(replayed.serialise(), line1);
	CHECK_EQ(engine::sync_wait(shard.find_key("Qr")), 5u);
	CHECK_EQ(engine::sync_wait(shard.stats()).lastdocid, 5u);

	if (kind == Kind::strata) {
		// The same line, again: nothing changes, however many times it comes.
		for (int i = 0; i < 3; ++i) {
			auto again = engine::Document::unserialise(kind, line1);
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
		auto second = make(kind, "Qr", "second");
		CHECK_EQ(engine::sync_wait(shard.replace(5, second, plain)).version, 2u);
		const auto line2 = second.serialise();
		for (const auto& line : {line1, line2, line1, line2}) {
			auto again = engine::Document::unserialise(kind, line);
			CHECK_EQ(engine::sync_wait(shard.replace(5, again, plain)).version, 2u);
			auto view = engine::sync_wait(shard.get(5));
			CHECK_EQ(view->version(), 2u);
			CHECK_EQ(view->data(), "second");
		}

		// The same holds by key, and a line that is ahead of the index is applied (the index is behind).
		auto by_key = engine::Document::unserialise(kind, line2);
		CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qr", by_key, plain)).version, 2u);
		CHECK_EQ(engine::sync_wait(shard.get(5))->version(), 2u);
		auto third = make(kind, "Qr", "third");
		CHECK_EQ(engine::sync_wait(shard.replace(5, third, plain)).version, 3u);
		CHECK_EQ(engine::sync_wait(shard.get(5))->data(), "third");

		// A document the shard stamped is a fresh write when written again, not a replay.
		auto fresh = make(kind, "Qs", "s");
		CHECK_EQ(engine::sync_wait(shard.add(fresh, plain)).version, 1u);
		CHECK_EQ(engine::sync_wait(shard.replace(6, fresh, plain)).version, 2u);
	} else {
		auto again = engine::Document::unserialise(kind, line1);
		CHECK_EQ(engine::sync_wait(shard.replace(5, again, plain)).version, 2u);
		CHECK_EQ(engine::sync_wait(shard.get(5))->data(), "first");
	}
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, kind == Kind::strata ? 2u : 1u);

	// A line that is not a document is a syntax error, not a crash.
	if (kind == Kind::strata) {
		CHECK_TRUE(error_kind([&] { engine::Document::unserialise(kind, std::string("\xff\xff\xff", 3)); }) == engine::errc::syntax);
	}

	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// The strata engine's own behavior.
void strata_only() {
	const auto kind = Kind::strata;
	const auto path = scratch("own", kind);
	auto opened = open_shard(kind, path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	// A term that starts like an id term ("Q...") is an ordinary term, not the key: a field called
	// "Quote" must index.
	auto doc = make(kind, "QKone", "d");
	doc.add_term("Quote.word");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("QKone", doc, plain)).did, 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("QKone")), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("Quote.word")), 0u);

	// A document without a key is refused: its version would have no key to belong to.
	engine::Document keyless(kind);
	keyless.add_term("x");
	keyless.set_data("k");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add(keyless, plain)); }) == engine::errc::invalid);
	auto other = make(kind, "QKtwo", "t");
	CHECK_EQ(engine::sync_wait(shard.add(other, plain)).did, 2u);

	// A key that belongs to another document cannot be replaced by id.
	auto clash = make(kind, "QKone", "c");
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
	CHECK_EQ(engine::sync_wait(shard.find_key("QKone")), 1u);
	CHECK_EQ(engine::sync_wait(shard.find_key("QKtwo")), 2u);
	CHECK_EQ(engine::sync_wait(shard.stats()).doc_count, 2u);

	// A document built for the other engine is refused.
	engine::Document foreign(Kind::xapian);
	foreign.add_term("x");
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add(foreign, plain)); }) == engine::errc::invalid);

	// The readable handle of a shard in this process shares the writable one's index and sees its
	// writes before any commit; it cannot write.
	auto readable = open_shard(kind, path, false, false);
	CHECK_EQ(engine::sync_wait(readable.shard->get(1))->data(), "d");
	CHECK_EQ(engine::sync_wait(readable.shard->find_key("QKone")), 1u);
	CHECK_TRUE(error_kind([&] {
		auto other = make(kind, "Qz", "z");
		engine::sync_wait(readable.shard->add(other, plain));
	}) == engine::errc::invalid);
	readable.shard.reset();
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "d");  // the writer is unaffected by the reader's close

	// Spelling is not kept.
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.add_spelling("word", 1)); }) == engine::errc::not_available);

	// Term lists wait for prospect's term-list plugin (P2): reading a document's terms says so.
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1))->term_details([](const engine::TermDetail&) { }); }) == engine::errc::not_available);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(1))->terms("", [](std::string_view, engine::termcount) { }); }) == engine::errc::not_available);
	CHECK_TRUE(error_kind([&] { (void)engine::sync_wait(shard.get(1))->serialise(); }) == engine::errc::not_available);

	// Search waits for S2: a strata shard says so, for a term and for match-all.
	for (const auto& query : {engine::Query::term("x"), engine::Query::match_all()}) {
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		engine::Shard* shards[] = {&shard};
		CHECK_TRUE(error_kind([&] { engine::sync_wait(engine::search(shards, request)); }) == engine::errc::not_available);
	}

	// Dropping the writable shard without a commit keeps its work, as a Xapian writer does.
	opened.shard.reset();
	auto reopened = open_shard(kind, path, true, false);
	CHECK_EQ(engine::sync_wait(reopened.shard->get(1))->data(), "d");
	reopened.shard.reset();

	// The engine is the one whose files are there.
	CHECK_TRUE(engine::kind_of(path) == Kind::strata);
	CHECK_TRUE(engine::kind_of(".xapiand/indexes") == Kind::xapian);

	std::filesystem::remove_all(path);
}

// Values are the seam's opaque bytes, held by the document until prospect's values plugin (P1) stores
// them. This pins the gap: when P1 lands the shard returns them and this check flips.
void values_gap() {
	const auto path = scratch("values", Kind::strata);
	auto opened = open_shard(Kind::strata, path);
	engine::WriteArgs plain;
	auto doc = make(Kind::strata, "Qv", "d");
	doc.add_value(9, "nine");
	CHECK_EQ(doc.get_value(9), "nine");  // the document holds it
	engine::sync_wait(opened.shard->add(doc, plain));
	CHECK_EQ(engine::sync_wait(opened.shard->get(1))->value(9), "");  // TODO(P1): "nine"
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// The strata document's codec: round trip, and no crash or runaway allocation on damaged input.
void codec() {
	using engine::strata::StrataDocument;
	engine::Document doc(Kind::strata);
	doc.set_key("Qk");
	doc.add_term("a", 3);
	doc.add_posting("b", 9);
	doc.add_posting("b", 2);
	doc.add_posting("b", 9);
	doc.add_value(1, "x");
	doc.add_value(0xfffffffe, std::string(300, 'y'));
	doc.set_data(std::string("bin\0data", 8));
	const auto bytes = doc.serialise();
	auto back = engine::Document::unserialise(Kind::strata, bytes);
	CHECK_EQ(back.serialise(), bytes);
	CHECK_EQ(back.get_data(), std::string("bin\0data", 8));
	const auto& terms = engine::strata::strata_document(back).terms();
	CHECK_EQ(terms.at("b").positions.size(), 2u);  // sorted and unique in the serialised form
	CHECK_EQ(terms.at("a").wdf, 3u);

	// The version the shard stamped is what the log keeps and replay reads; a fresh document has none.
	CHECK_EQ(engine::strata::strata_document(back).version(), 0u);
	CHECK_FALSE(engine::strata::strata_document(back).replayed());
	engine::strata::strata_document(back).stamp(77);
	auto stamped = engine::Document::unserialise(Kind::strata, back.serialise());
	CHECK_EQ(engine::strata::strata_document(stamped).version(), 77u);
	CHECK_TRUE(engine::strata::strata_document(stamped).replayed());
	CHECK_EQ(stamped.serialise(), back.serialise());
	engine::strata::strata_document(back).stamp(0);

	// Every truncation fails cleanly.
	for (std::size_t n = 0; n < bytes.size(); ++n) {
		CHECK_TRUE(error_kind([&] { engine::Document::unserialise(Kind::strata, std::string_view(bytes).substr(0, n)); }) == engine::errc::syntax);
	}
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(Kind::strata, bytes + "x"); }) == engine::errc::syntax);
	// A count larger than the input could hold is refused before anything is allocated for it.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(Kind::strata, std::string("\x01\x00\x00\x00\xff\xff\xff\xff\x0f", 9)); }) == engine::errc::syntax);
	// A key that is not one of the terms is refused.
	CHECK_TRUE(error_kind([&] { engine::Document::unserialise(Kind::strata, std::string("\x01\x00\x01Q\x00\x00\x00", 7)); }) == engine::errc::syntax);

	// Random single-byte damage never crashes: it either decodes or raises a syntax error.
	std::mt19937 random(42);
	for (int i = 0; i < 20000; ++i) {
		auto damaged = bytes;
		damaged[random() % damaged.size()] = static_cast<char>(random());
		if (i % 3 == 0) {
			damaged.resize(random() % (damaged.size() + 1));
		}
		try {
			auto result = engine::Document::unserialise(Kind::strata, damaged);
			(void)result.serialise();
		} catch (const engine::Error& error) {
			CHECK_TRUE(error.kind == engine::errc::syntax);
		}
	}
}

// A read by id sees every write of this handle at once: the index is refreshed when a write left it
// dirty, and only then.
void fresh_reads() {
	const auto kind = Kind::strata;
	const auto path = scratch("fresh", kind);
	auto opened = open_shard(kind, path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	auto one = make(kind, "Qa", "one");
	CHECK_EQ(engine::sync_wait(shard.add(one, plain)).did, 1u);
	CHECK_EQ(engine::sync_wait(shard.get(1))->data(), "one");  // flushes the write; the index is clean now

	// A replace after a clean read is seen, with its version.
	auto two = make(kind, "Qa", "two");
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
	auto b = make(kind, "Qb", "b");
	CHECK_EQ(engine::sync_wait(shard.add(b, plain)).did, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(2))->data(), "b");
	CHECK_EQ(engine::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)), 2u);
	CHECK_TRUE(error_kind([&] { engine::sync_wait(shard.get(2)); }) == engine::errc::doc_not_found);
	CHECK_EQ(shard.doc_count(), 0u);

	// A replace by key after a clean read.
	auto c1 = make(kind, "Qc", "c1");
	CHECK_EQ(engine::sync_wait(shard.add(c1, plain)).did, 3u);
	CHECK_EQ(engine::sync_wait(shard.get(3))->data(), "c1");
	auto c2 = make(kind, "Qc", "c2");
	CHECK_EQ(engine::sync_wait(shard.replace_by_key("Qc", c2, plain)).version, 2u);
	CHECK_EQ(engine::sync_wait(shard.get(3))->data(), "c2");
	std::vector<std::string> scanned;
	engine::sync_wait(shard.scan(0, [&](const engine::DocView& view) { scanned.push_back(view.data()); }));
	CHECK_TRUE(scanned == (std::vector<std::string>{"c2"}));

	opened.shard.reset();
	std::filesystem::remove_all(path);
}

// Closing the last handle commits what is pending, as a Xapian writer does: documents, erases and
// metadata survive; a handle that wrote nothing leaves the revision alone.
void commit_on_drop() {
	const auto kind = Kind::strata;
	const auto path = scratch("drop", kind);
	engine::WriteArgs plain;
	{
		auto opened = open_shard(kind, path);
		auto& shard = *opened.shard;
		auto a = make(kind, "Qa", "a");
		auto b = make(kind, "Qb", "b");
		engine::sync_wait(shard.add(a, plain));
		engine::sync_wait(shard.add(b, plain));
		engine::sync_wait(shard.commit(engine::Transaction::none));
		engine::sync_wait(shard.erase(1, true, std::nullopt));
		engine::sync_wait(shard.set_metadata("m", "v"));
		auto c = make(kind, "Qc", "c");
		engine::sync_wait(shard.add(c, plain));
	}
	engine::rev revision = 0;
	{
		auto reopened = open_shard(kind, path, true, false);
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
		auto reopened = open_shard(kind, path, true, false);
		CHECK_EQ(reopened.shard->revision(), revision);  // nothing was written, so nothing committed
	}
	std::filesystem::remove_all(path);
}

// Writers on one shard that check and set: a write with a stale expectation conflicts, the ones that win
// get each version once, no update is lost, and a key that must be new is created by exactly one.
void concurrent_cas() {
	const auto kind = Kind::strata;
	const auto path = scratch("cas", kind);
	auto opened = open_shard(kind, path);
	auto& shard = *opened.shard;
	engine::WriteArgs plain;

	constexpr int keys = 3;
	constexpr int threads = 8;
	constexpr int wins_wanted = 150;  // successful updates per thread
	for (int k = 0; k < keys; ++k) {
		auto doc = make(kind, "Qk" + std::to_string(k), "seed");
		CHECK_EQ(engine::sync_wait(shard.add(doc, plain)).did, static_cast<engine::docid>(k + 1));
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
					auto doc = make(kind, "Qk" + std::to_string(k), data);
					engine::WriteArgs args;
					args.expected_version = seen;
					try {
						const auto result = engine::sync_wait(shard.replace(did, doc, args));
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
				auto doc = make(kind, key, "by" + std::to_string(t));
				engine::WriteArgs args;
				args.expected_version = 0;
				try {
					engine::sync_wait(shard.replace_by_key(key, doc, args));
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
					engine::sync_wait(shard.erase_by_key("Qk0", true, current));
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

static void test_shared_xapian() { guarded("shared xapian", [] { shared(Kind::xapian); }); }
static void test_shared_strata() { guarded("shared strata", [] { shared(Kind::strata); }); }
static void test_replay_xapian() { guarded("replay xapian", [] { replay(Kind::xapian); }); }
static void test_replay_strata() { guarded("replay strata", [] { replay(Kind::strata); }); }
static void test_strata_only() { guarded("strata only", [] { strata_only(); }); }
static void test_values_gap() { guarded("values gap", [] { values_gap(); }); }
static void test_fresh_reads() { guarded("fresh reads", [] { fresh_reads(); }); }
static void test_commit_on_drop() { guarded("commit on drop", [] { commit_on_drop(); }); }
static void test_concurrent_cas() { guarded("concurrent cas", [] { concurrent_cas(); }); }
static void test_codec() { guarded("codec", [] { codec(); }); }

TEST_MAIN(test_shared_xapian, test_shared_strata, test_replay_xapian, test_replay_strata, test_strata_only, test_values_gap, test_codec, test_fresh_reads, test_commit_on_drop, test_concurrent_cas)
