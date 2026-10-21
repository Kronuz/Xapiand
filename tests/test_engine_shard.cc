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

// Unit tests for the Xapian implementation of the engine shard: versioned writes, errors by kind,
// transactions and reopen, each with the blocking calls on the calling thread and on an offload pool.

#include "test_harness.h"

#include <filesystem>
#include <string>
#include <thread>

#include "engine/xapian/convert.h"
#include "engine/xapian/errors.h"
#include "engine/xapian/shard.h"
#include "task.hh"

namespace {

engine::Document make(const std::string& key, const std::string& data) {
	engine::Document doc;
	doc.add_boolean_term(key);
	doc.add_term("body", 2);
	doc.set_data(data);
	return doc;
}

template <typename F>
engine::errc kind_of(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

void run(bool offload) {
	const auto path = (std::filesystem::current_path() / (offload ? "engine_shard_test_offload.db" : "engine_shard_test.db")).string();
	std::filesystem::remove_all(path);

	engine::xapian::Offload pool(2);
	if (offload) {
		engine::xapian::set_offload(&pool);
	}

	auto opened = task::sync_wait(engine::xapian::open_local(path, true, true));
	CHECK_TRUE(opened.created);
	auto& shard = *opened.shard;

	// A new document gets id 1 and version 1; the hook sees both before the write.
	engine::WriteArgs args;
	engine::docid hooked_did = 0;
	engine::rev hooked_version = 0;
	std::thread::id writer;
	args.finalise = [&](engine::Document& doc, engine::WriteResult& result) {
		writer = std::this_thread::get_id();
		hooked_did = result.did;
		hooked_version = result.version;
		doc.add_term("stamped");
	};
	auto doc = make("Qa", "one");
	auto added = task::sync_wait(shard.add(doc, args));
	CHECK_EQ(added.did, 1u);
	CHECK_EQ(added.version, 1u);
	CHECK_EQ(hooked_did, 1u);
	CHECK_EQ(hooked_version, 1u);
	// The blocking call runs on the pool when there is one, and on the caller's thread when not.
	CHECK_TRUE((writer != std::this_thread::get_id()) == offload);
	CHECK_TRUE(doc.terms().count("stamped") == 1);

	// The version is recorded: the document carries it as a value and a term.
	auto view = engine::xapian::make_view(shard.database()->get_document(1));
	CHECK_EQ(view->version(), 1u);
	CHECK_EQ(view->data(), "one");

	// Replacing by id bumps the version; a stale expectation conflicts.
	engine::WriteArgs plain;
	auto doc2 = make("Qa", "two");
	auto replaced = task::sync_wait(shard.replace(1, doc2, plain));
	CHECK_EQ(replaced.version, 2u);
	engine::WriteArgs stale;
	stale.expected_version = 1;
	auto doc3 = make("Qa", "three");
	CHECK_TRUE(kind_of([&] { task::sync_wait(shard.replace(1, doc3, stale)); }) == engine::errc::version_conflict);
	engine::WriteArgs current;
	current.expected_version = 2;
	auto replaced3 = task::sync_wait(shard.replace(1, doc3, current));
	CHECK_EQ(replaced3.version, 3u);

	// The conflict is also the Xapian exception it stands for.
	bool as_xapian = false;
	try {
		task::sync_wait(shard.replace(1, doc3, stale));
	} catch (const Xapian::DocVersionConflictError&) {
		as_xapian = true;
	}
	CHECK_TRUE(as_xapian);

	// By key: a new key adds, an existing key replaces and bumps; a new document refuses a version.
	auto doc4 = make("Qb", "b1");
	auto by_key = task::sync_wait(shard.replace_by_key("Qb", doc4, plain));
	CHECK_EQ(by_key.did, 2u);
	CHECK_EQ(by_key.version, 1u);
	CHECK_EQ(by_key.term, "Qb");
	auto doc5 = make("Qb", "b2");
	CHECK_EQ(task::sync_wait(shard.replace_by_key("Qb", doc5, plain)).version, 2u);
	engine::WriteArgs must_be_new;
	must_be_new.expected_version = 0;
	auto doc6 = make("Qc", "c1");
	CHECK_EQ(task::sync_wait(shard.replace_by_key("Qc", doc6, must_be_new)).did, 3u);
	auto doc7 = make("Qc", "c2");
	CHECK_TRUE(kind_of([&] { task::sync_wait(shard.replace_by_key("Qc", doc7, must_be_new)); }) == engine::errc::version_conflict);

	// Erasing: by id, and by key (which reports the id, and fails for a missing key).
	task::sync_wait(shard.erase(1, true, std::nullopt));
	CHECK_EQ(task::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)), 2u);
	CHECK_TRUE(kind_of([&] { task::sync_wait(shard.erase_by_key("Qb", true, std::nullopt)); }) == engine::errc::doc_not_found);

	// Commit, transactions, metadata and reopen.
	const auto before = shard.revision();
	task::sync_wait(shard.commit(engine::Transaction::none));
	CHECK_EQ(shard.revision(), before + 1);
	task::sync_wait(shard.begin_transaction(true));
	task::sync_wait(shard.set_metadata("k", "v"));
	task::sync_wait(shard.commit(engine::Transaction::flushed));
	task::sync_wait(shard.commit_transaction());
	CHECK_EQ(shard.database()->get_metadata("k"), "v");
	CHECK_FALSE(task::sync_wait(shard.reopen()));
	CHECK_FALSE(shard.uuid().empty());

	// Opening what is missing fails with the not-found kind.
	CHECK_TRUE(kind_of([&] { task::sync_wait(engine::xapian::open_local(path + ".missing", true, false)); }) == engine::errc::not_found);

	opened.shard.reset();
	engine::xapian::set_offload(nullptr);
	std::filesystem::remove_all(path);
}

}  // namespace

static void test_inline() { run(false); }
static void test_offload() { run(true); }

TEST_MAIN(test_inline, test_offload)
