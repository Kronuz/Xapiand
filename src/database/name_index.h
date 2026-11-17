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

#include <atomic>                  // for std::atomic
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <functional>              // for std::less
#include <mutex>                   // for std::mutex
#include <set>                     // for std::set
#include <shared_mutex>            // for std::shared_mutex
#include <span>                    // for std::span
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <thread>                  // for std::thread
#include <vector>                  // for std::vector

#include "index_pattern.h"         // for index_pattern::Bounds, index_pattern::Checkpoint


// The names of the node's indexes as the pattern walk lists them, in byte order, so a pattern is a few range scans
// instead of a walk of the data directory (docs/design/name-index.md). It is built by one walk and kept current by
// the catalog's resolve; nothing of it is written, so every start builds it again.
class NameIndex {
public:
	NameIndex() = default;
	~NameIndex();   // stops a running build and waits for it

	NameIndex(const NameIndex&) = delete;
	NameIndex& operator=(const NameIndex&) = delete;

	// The names a directory at `key` (an index path under the data directory, its shard `.__N` included) makes the
	// walk list: each prefix of `key/strata` followed by `strata` or a `.__N`, when the walk can reach it (D3).
	static std::vector<std::string> names_of(std::string_view key);

	// Adds the names of an index directory that a resolve found or made (D3).
	void opened(std::string_view key);

	// Starts the build over `root` on a thread of its own, unless one runs, one ended well, or the index stopped (D5).
	void start(const std::string& root);
	// Builds over `root` on this thread: one unbounded walk, merged with what resolve added meanwhile. False when
	// the index was stopped first; a failure of the walk is thrown.
	bool build(const std::string& root);
	// Asks a running build to stop at its next directory entry; no build starts after it. It never waits.
	void request_stop() noexcept;
	// Stops a running build and waits for its thread.
	void stop();

	// Whether a build ended well: from then on every name the walk lists is here.
	bool complete() const noexcept { return complete_.load(std::memory_order_acquire); }

	// The names `items` stand for, as index_pattern::expand gives them over the tree the index holds (D6), under the
	// same bounds: each name read and each seek is a visit (D7).
	std::vector<std::string> expand(std::span<const std::string> items, const index_pattern::Bounds& bounds, const index_pattern::Checkpoint& checkpoint = {}) const;

	// The names it holds, and the heap they take.
	std::size_t size() const;
	std::size_t bytes() const;

private:
	// The names in order, packed: name i is bytes[ends[i - 1], ends[i]) (D4).
	struct Base {
		std::string bytes;
		std::vector<std::uint64_t> ends;

		std::size_t size() const noexcept { return ends.size(); }
		std::string_view at(std::size_t i) const noexcept;
		std::size_t lower_bound(std::string_view key) const noexcept;
		void push_back(std::string_view name);
	};
	using Delta = std::set<std::string, std::less<>>;
	class Cursor;

	bool contains(std::string_view name) const;
	void merge_delta();   // under the exclusive lock

	mutable std::shared_mutex mtx_;
	Base base_;     // disjoint from delta_
	Delta delta_;
	std::atomic<bool> complete_{false};

	std::mutex build_mtx_;
	std::thread builder_;          // under build_mtx_
	std::atomic<bool> building_{false};
	std::atomic<bool> stopped_{false};
};
