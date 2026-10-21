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

#include <cstdint>                 // for std::uint8_t
#include <functional>              // for std::function
#include <optional>                // for std::optional
#include <string>                  // for std::string

#include "engine/document.h"       // for Document
#include "engine/types.h"          // for docid, rev, termcount, WriteResult
#include "task.hh"                 // for task::task


namespace engine {

// How a commit treats an open transaction.
enum class Transaction : std::uint8_t {
	none,
	flushed,
	unflushed,
};

// What a document write needs beyond the document.
struct WriteArgs {
	// True when the shard is the local copy; false when it is a remote proxy, which versions and
	// numbers documents itself.
	bool local = true;

	// The version the stored document must have; 0 means the document must not exist yet.
	// Empty accepts any.
	std::optional<rev> expected_version;

	// Called once the target document and its new version are known and before the document is
	// written: it may complete the document (embed the id or the version in its data) and may set
	// the result's term. The engine adds its own version bookkeeping after it returns.
	std::function<void(Document&, WriteResult&)> finalise;
};

// One shard of an index: the write and lifecycle slice. Every operation that can block on I/O
// returns a task. The referenced arguments must outlive the task. A shard is used by one task at
// a time.
class Shard {
public:
	virtual ~Shard() = default;

	// Appends a document under a new id.
	virtual task::task<WriteResult> add(Document& doc, WriteArgs args) = 0;

	// Replaces the document with that id, creating it if absent.
	virtual task::task<WriteResult> replace(docid did, Document& doc, WriteArgs args) = 0;

	// Replaces the document holding the key term, creating it if absent.
	virtual task::task<WriteResult> replace_by_key(const std::string& key, Document& doc, WriteArgs args) = 0;

	// Removes a document; the version check is skipped when expected_version is empty.
	virtual task::task<void> erase(docid did, bool local, std::optional<rev> expected_version) = 0;

	// Removes the document holding the key term and returns its id (0 for a remote proxy).
	virtual task::task<docid> erase_by_key(const std::string& key, bool local, std::optional<rev> expected_version) = 0;

	virtual task::task<void> commit(Transaction transaction) = 0;
	virtual task::task<void> begin_transaction(bool flushed) = 0;
	virtual task::task<void> commit_transaction() = 0;
	virtual task::task<void> cancel_transaction() = 0;

	virtual task::task<void> set_metadata(const std::string& key, const std::string& value) = 0;
	virtual task::task<void> add_spelling(const std::string& word, termcount increment) = 0;
	virtual task::task<termcount> remove_spelling(const std::string& word, termcount decrement) = 0;

	// Reopens at the latest revision; false when it was already the latest.
	virtual task::task<bool> reopen() = 0;

	// Cheap state of a local shard, read from memory.
	virtual rev revision() const = 0;
	virtual std::string uuid() const = 0;
	virtual unsigned refs() const = 0;
};

}  // namespace engine
