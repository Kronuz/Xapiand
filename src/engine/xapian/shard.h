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

#pragma once

#include <atomic>                  // for std::atomic
#include <condition_variable>      // for std::condition_variable
#include <coroutine>               // for std::coroutine_handle
#include <cstdint>                 // for std::uint64_t
#include <deque>                   // for std::deque
#include <memory>                  // for std::unique_ptr
#include <mutex>                   // for std::mutex
#include <string>                  // for std::string
#include <thread>                  // for std::thread
#include <vector>                  // for std::vector

#include "engine/shard.h"          // for Shard
#include "xapian.h"                // for Xapian::Database


namespace engine::xapian {

// A pool of threads for the blocking Xapian calls. When one is set, each task hops onto it before
// calling Xapian, so a caller that is a coroutine never blocks its own thread. A caller that
// waits with sync_wait must not be one of its threads. It models task::scheduler.
class Offload {
public:
	explicit Offload(unsigned threads);
	Offload(const Offload&) = delete;
	Offload& operator=(const Offload&) = delete;
	~Offload();

	void post(std::coroutine_handle<> handle);
	std::uint64_t now() const noexcept;

private:
	void run();

	std::mutex mutex_;
	std::condition_variable wake_;
	std::deque<std::coroutine_handle<>> queue_;
	std::vector<std::thread> threads_;
	bool stopping_ = false;
};

// Sets the pool the blocking calls hop onto; null (the default) runs each call on the thread that
// awaits it. The pool must outlive every shard operation.
void set_offload(Offload* pool) noexcept;

// The Xapian implementation of the engine shard, over a Xapian database it owns.
class Shard final : public engine::Shard {
public:
	explicit Shard(std::unique_ptr<Xapian::Database> database) : database_(std::move(database)) { }

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
	rev revision() const override;
	std::string uuid() const override;
	unsigned refs() const override;

private:
	Xapian::WritableDatabase& writable() const;
	std::optional<rev> stored_version(docid did) const;
	void check_version(const std::optional<rev>& expected, const std::optional<rev>& stored) const;

	std::unique_ptr<Xapian::Database> database_;
};

struct Opened {
	std::unique_ptr<Shard> shard;
	bool created = false;
};

// Opens (or, when create_missing, creates) a local shard.
task::task<Opened> open_local(const std::string& path, bool writable, bool create_missing);

// Opens a shard served by another node.
task::task<std::unique_ptr<Shard>> open_remote(const std::string& host, unsigned port, int flags, const std::string& path, bool writable);

// Opens the local shard at path when it has the same uuid as the remote one; null when it differs.
task::task<std::unique_ptr<Shard>> open_local_if_same(const Shard& remote, const std::string& path);

}  // namespace engine::xapian
