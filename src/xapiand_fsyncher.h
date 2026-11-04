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

/*
 * Xapiand's batched async-fsync debouncer.
 *
 * This used to live in src/storage.h. The extracted `storage` library no longer
 * carries a thread pool / debouncer / opts dependency: it exposes an injectable
 * StorageFsyncFn instead. Xapiand keeps its original debounced, thread-pooled
 * fsyncher here and feeds it to the WAL through that hook (see database/wal.cc),
 * while manager.cc still drives it for startup/shutdown and metrics.
 *
 * fsyncher() is an inline function with a function-static singleton, so every
 * translation unit shares the one debouncer instance.
 */

#pragma once

#include <cassert>               // for assert
#include <cerrno>                // for errno, EBADF, EINVAL, ENOTSUP
#include <chrono>                // for std::chrono

#include "debouncer.h"           // for make_unique_debouncer
#include "error.hh"              // for error::name, error::description
#include "io.hh"                 // for io::unchecked_fsync, io::unchecked_full_fsync
#include "logger.h"              // for L_DEBUG, L_WARNING
#include "opts.h"                // for opts::*
#include "xapiand_strings.hh"            // for strings::from_delta


// The fsync itself: run by the debouncer, or directly when it refuses.
inline void fsyncher_fsync(int fd, bool full_fsync) {
	auto start = std::chrono::steady_clock::now();

	int err = full_fsync
		? io::unchecked_full_fsync(fd)
		: io::unchecked_fsync(fd);

	auto end = std::chrono::steady_clock::now();

	if (err == -1) {
		if (errno == EBADF || errno == EINVAL || errno == ENOTSUP) {
			L_DEBUG("Async {} falied after {}: {} ({}): {}", full_fsync ? "Full Fsync" : "Fsync", strings::from_delta(start, end), error::name(errno), errno, error::description(errno));
		} else {
			L_WARNING("Async {} falied after {}: {} ({}): {}", full_fsync ? "Full Fsync" : "Fsync", strings::from_delta(start, end), error::name(errno), errno, error::description(errno));
		}
	} else {
		L_DEBUG("Async {} succeeded after {}", full_fsync ? "Full Fsync" : "Fsync", strings::from_delta(start, end));
	}
}


inline auto& fsyncher(bool create = true) {
	static auto fsyncher = create ? make_unique_debouncer<int>("FS--", "FS{:02}", opts.num_fsynchers, fsyncher_fsync, std::chrono::milliseconds(opts.fsyncher_throttle_time), std::chrono::milliseconds(opts.fsyncher_debounce_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_busy_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_min_force_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_max_force_timeout)) : nullptr;
	assert(!create || fsyncher);
	return fsyncher;
}
