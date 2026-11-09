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

// A host index's owed answers (x-pool D9): under defer_io, apply answers flush_owed, drain_owed or map_owed
// with nothing applied; the host settles the debt in a chore (refresh, reclaim_for_space or promote) and
// applies the same transaction again.

#include "engine/error.h"          // for Error, errc
#include "strata/core/index.hh"    // for strata::verdict
#include "task.hh"                 // for task::task

namespace engine::strata {

// The owed answers one transaction may meet. strata answers each kind at most once per transaction
// (CT-R22), so two cover a flush then a map; a third is a strata bug, raised rather than looped on.
inline constexpr int max_owed_answers = 2;

inline bool is_owed(::strata::verdict::outcome result) noexcept {
	return result == ::strata::verdict::flush_owed || result == ::strata::verdict::drain_owed || result == ::strata::verdict::map_owed;
}

// Applies through `apply()` (a verdict) until it answers anything but an owed answer, awaiting
// `settle(owed)` (a task) for each owed one. Past `max_owed_answers` it raises errc::internal.
template <typename Apply, typename Settle>
task::task<::strata::verdict> apply_settling(Apply apply, Settle settle) {
	for (int owed = 0;; ++owed) {
		auto verdict = apply();
		if (!is_owed(verdict.result)) {
			co_return verdict;
		}
		if (owed == max_owed_answers) {
			throw Error(errc::internal, "The index kept owing I/O before it took a transaction");
		}
		co_await settle(verdict.result);
	}
}

}  // namespace engine::strata
