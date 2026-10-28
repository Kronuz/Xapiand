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

#pragma once

#include <memory>                  // for std::unique_ptr
#include <cstddef>                 // for std::size_t
#include <string>                  // for std::string
#include <vector>                  // for std::vector

#include "engine/shard.h"          // for Shard
#include "xapian.h"                // for Xapian::Database


namespace engine::xapian {

// The Xapian implementation of the engine shard, over a Xapian database it owns.
class Shard : public engine::Shard {
public:
	// Each call hops onto `on` before calling Xapian; empty (the default) runs it on the thread
	// that awaits it. A caller that sync_waits must not be one of on's threads.
	explicit Shard(std::unique_ptr<Xapian::Database> database, task::scheduler_ref on = {}) : database_(std::move(database)), on_(on) { }

	// The database itself, for the code that has not moved to the seam yet (reads, X4).
	Xapian::Database* database() const noexcept {
		return database_.get();
	}

	task::task<WriteResult> add(Document& doc, WriteArgs args) override;
	task::task<WriteResult> replace(docid did, Document& doc, WriteArgs args) override;
	task::task<WriteResult> replace_by_key(const std::string& key, Document& doc, WriteArgs args) override;
	task::task<void> erase(docid did, bool local, std::optional<rev> expected_version) override;
	task::task<docid> erase_by_key(const std::string& key, bool local, std::optional<rev> expected_version) override;
	task::task<void> commit(Transaction transaction) override;
	task::task<void> begin_transaction(bool flushed) override;
	task::task<void> commit_transaction() override;
	task::task<void> cancel_transaction() override;
	task::task<void> set_metadata(const std::string& key, const std::string& value) override;
	task::task<void> add_spelling(const std::string& word, termcount increment) override;
	task::task<termcount> remove_spelling(const std::string& word, termcount decrement) override;
	task::task<bool> reopen() override;
	task::task<Stats> stats() override;
	task::task<docid> find_key(const std::string& key) override;
	task::task<std::unique_ptr<DocView>> get(docid did) override;
	task::task<void> scan(docid from, const ScanSink& sink) override;
	task::task<std::string> metadata(const std::string& key) override;
	task::task<std::vector<std::string>> metadata_keys() override;
	rev revision() const override;
	doccount doc_count() const override;
	std::string uuid() const override;
	unsigned refs() const override;

private:
	Xapian::WritableDatabase& writable() const;
	std::optional<rev> stored_version(docid did) const;
	void check_version(const std::optional<rev>& expected, const std::optional<rev>& stored) const;

	std::unique_ptr<Xapian::Database> database_;
	task::scheduler_ref on_;
};

// Integrity check of the shard at a path. `errors` counts what the check found; `failure` holds the
// description of the exception when the check itself could not run.
struct CheckResult {
	std::size_t errors = 0;
	std::string failure;
};
task::task<CheckResult> check(const std::string& path, task::scheduler_ref on = {});

struct Opened {
	std::unique_ptr<Shard> shard;
	bool created = false;
};

// The openers hop onto `on` as Shard's calls do, and the shard they open keeps it.

// Opens (or, when create_missing, creates) a local shard.
task::task<Opened> open_local(const std::string& path, bool writable, bool create_missing, task::scheduler_ref on = {});

// Opens a shard served by another node.
task::task<std::unique_ptr<Shard>> open_remote(const std::string& host, unsigned port, int flags, const std::string& path, bool writable, task::scheduler_ref on = {});

// Opens the local shard at path when it has the same uuid as the remote one; null when it differs.
task::task<std::unique_ptr<Shard>> open_local_if_same(const Shard& remote, const std::string& path, task::scheduler_ref on = {});

}  // namespace engine::xapian
