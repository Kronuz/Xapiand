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

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <string>                  // for std::string

#include "engine/chores.h"         // for Chores
#include "engine/shard.h"          // for Shard
#include "task.hh"                 // for task::task, task::scheduler_ref


namespace strata {
class executor;
}

namespace engine {

struct Opened {
	std::unique_ptr<Shard> shard;
	bool created = false;
};

// The records readers claim to hold an index's published view (docs/design/phase7c.md D6, C9). One registry is
// shared by every index of a host: a record is held for as long as a search reads a view, so its size bounds
// the searches in flight, and a read that finds none left is refused with errc::busy.
class ReaderRegistry {
public:
	virtual ~ReaderRegistry() = default;
	virtual std::size_t capacity() const noexcept = 0;
	// The memory the records take.
	virtual std::size_t bytes() const noexcept = 0;
};

std::shared_ptr<ReaderRegistry> make_reader_registry(std::size_t records);

// How a host runs the shard. With none of it, every call does its device work inline.
struct OpenOptions {
	// Where the shard's device work runs (docs/design/phase7c.md D4); null runs it inline.
	Chores* chores = nullptr;

	// Host mode (D6): a write does no I/O of its own. When the index owes a flush or a drain the shard
	// runs it as a chore and applies again, and a read sees what a write applied only after refresh().
	bool host = false;

	// Runs the index's background merges and deletes (a strata::executor); null runs them inline.
	::strata::executor* background = nullptr;

	// The documents the write buffer holds before the next write owes a flush; 0 keeps the index's own bound.
	std::uint64_t buffered_docs_cap = 0;

	// The registry readers of this shard claim their records in; null gives the shard one of its own.
	std::shared_ptr<ReaderRegistry> readers;
};

// Opens (or, when create_missing, creates) the local shard at `path`.
task::task<Opened> open_local(const std::string& path, bool writable, bool create_missing, OpenOptions options = {});

// The engine's own integrity check of the shard at `path`, for a path no host has open.
task::task<CheckResult> check(const std::string& path, OpenOptions options = {});

}  // namespace engine
