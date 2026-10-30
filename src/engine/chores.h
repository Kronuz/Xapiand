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

#include <functional>              // for std::function
#include <utility>                 // for std::move

#include "task.hh"                 // for task::task


namespace engine {

// What a chore waits behind on the host's work pool (docs/design/phase7c.md D4).
enum class ChoreClass : unsigned char {
	query,   // a request waits for it: a read-your-writes refresh, an open, a blob read
	flush,   // throughput work: a commit, a close, a blob write
	merge,   // background work
};

// Where an engine shard's device work runs. The shard never touches a device on the thread that
// awaits it: it hands each such step to run(), which executes `fn` off that thread (or inline, for
// a host with no pool) and resumes the awaiting task through the scheduler it suspended on. What
// `fn` throws reaches the awaiter.
class Chores {
public:
	virtual ~Chores() = default;
	virtual task::task<void> run(ChoreClass cls, std::function<void()> fn) = 0;
};

// Runs a chore where it is awaited: tests, the CLI before a pool exists.
class InlineChores final : public Chores {
public:
	task::task<void> run(ChoreClass, std::function<void()> fn) override {
		fn();
		co_return;
	}
};

inline Chores& inline_chores() {
	static InlineChores chores;
	return chores;
}

}  // namespace engine
