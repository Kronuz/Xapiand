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

#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <string>                  // for std::string
#include <vector>                  // for std::vector

#include "engine/document.h"       // for FieldKind
#include "engine/shard.h"          // for engine::Shard


namespace engine::strata {

struct Core;

// Rethrows the exception being handled as an engine error of its kind, marking `core` (when given) fenced
// on an index_fenced. An exception of no known kind becomes an internal error, unless `callback_threw`: it came
// from the caller's own code (a spy, a key maker, a sink), so it leaves as thrown. Call only inside a catch block.
[[noreturn]] void rethrow_translated(Core* core = nullptr, bool callback_threw = false);

// Runs the caller's callbacks, remembering that one threw.
class CallbackGuard {
public:
	template <typename F>
	decltype(auto) operator()(F&& call) {
		try {
			return call();
		} catch (...) {
			threw_ = true;
			throw;
		}
	}
	bool threw() const noexcept { return threw_; }
private:
	bool threw_ = false;
};

struct Handle;
class Pinned;

// A field of the shard's field table: its name, the kind of the first document that named it (the table
// keeps what its postings hold), and whether it is the key field.
struct DeclaredField {
	std::string name;
	FieldKind kind = FieldKind::boolean;
	bool key = false;
};

// The strata implementation of the engine shard: an index in the shard's "strata" directory. The
// writable and the readable handle of one shard share one index (strata locks its directory), so the
// handles of a shard in this process share it through a Handle.
class Shard final : public engine::Shard {
public:
	// Each call hops onto `on` before it touches the index; empty runs it on the thread that awaits it.
	Shard(std::unique_ptr<Handle> handle, bool writable, task::scheduler_ref on = {});
	~Shard() override;


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

	// Flushes the write buffer and pins the shard's segments for a search (src/engine/strata/search.cc).
	// The pin holds the shard's lock; see identity() for taking several in order.
	Pinned pin();

	// Names the index behind the handle: handles of one shard share it, and shards are pinned in the
	// order of their identities.
	const void* identity() const noexcept;

	// The shard's field table, in id order, as of its last applied write.
	std::vector<DeclaredField> declared_fields() const;

private:
	std::unique_ptr<Handle> handle_;
	bool writable_;
	task::scheduler_ref on_;
};

struct Opened {
	std::unique_ptr<Shard> shard;
	bool created = false;
};

// Opens (or, when create_missing, creates) the strata shard at a path.
task::task<Opened> open_local(const std::string& path, bool writable, bool create_missing, task::scheduler_ref on = {});

// Whether the engine's own integrity check of the shard at `path` passes; the text says why not.
struct CheckResult {
	std::size_t errors = 0;
	std::string failure;
};
task::task<CheckResult> check(const std::string& path, task::scheduler_ref on = {});

}  // namespace engine::strata
