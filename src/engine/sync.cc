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


#include "engine/sync.h"

#include <atomic>                  // for std::atomic
#include <cstdio>                  // for std::fprintf
#include <cstdlib>                 // for std::abort
#include <mutex>                   // for std::mutex, std::lock_guard
#include <set>                     // for std::set

#include <execinfo.h>              // for backtrace, backtrace_symbols_fd

#include "log.h"                   // for L_ERR
#include "reactor.h"               // for reactor::inside_run


namespace engine {

namespace {

std::atomic<std::uint64_t> violations{0};

}  // namespace


void
check_may_block(const char* site) noexcept
{
	const bool on_loop = reactor::inside_run();
	if (!on_loop && !task::current()) {
		return;
	}
	violations.fetch_add(1, std::memory_order_relaxed);
	const char* where = on_loop ? "a reactor loop" : "a task scheduler's thread";
#ifndef NDEBUG
	std::fprintf(stderr, "%s blocks on database work on %s\n", site, where);
	void* frames[64];
	backtrace_symbols_fd(frames, backtrace(frames, 64), 2);
	std::abort();
#else
	static std::mutex mutex;
	static std::set<const char*> logged;
	try {
		std::lock_guard lock(mutex);
		if (logged.insert(site).second) {
			L_ERR("{} blocks on database work on {}", site, where);
		}
	} catch (...) { }
#endif
}


std::uint64_t
blocking_violations() noexcept
{
	return violations.load(std::memory_order_relaxed);
}

}  // namespace engine
