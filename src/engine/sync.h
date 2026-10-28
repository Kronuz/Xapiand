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

#include <cstdint>                 // for std::uint64_t
#include <utility>                 // for std::move

#include "task.hh"                 // for task::task, task::sync_wait


namespace engine {

// Checks that the calling thread may block on database work: it is not inside a reactor loop's
// run() and not one of a task scheduler's threads. A violation aborts when asserts are on;
// otherwise it is counted and logged once per site.
void check_may_block(const char* site) noexcept;

// The violations counted so far.
std::uint64_t blocking_violations() noexcept;

// The one bridge from a blocking caller to the engine: waits for t on this thread.
template <typename T>
T sync_wait(task::task<T> t) {
	check_may_block("engine::sync_wait");
	return task::sync_wait(std::move(t));
}

}  // namespace engine
