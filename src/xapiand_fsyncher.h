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
#include "strings.hh"            // for strings::from_delta
#include "thread.hh"             // for ThreadPolicyType::fsynchers


inline auto& fsyncher(bool create = true) {
	static auto fsyncher = create ? make_unique_debouncer<int, ThreadPolicyType::fsynchers>("FS--", "FS{:02}", opts.num_fsynchers, [] (int fd, bool full_fsync) {
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
	}, std::chrono::milliseconds(opts.fsyncher_throttle_time), std::chrono::milliseconds(opts.fsyncher_debounce_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_busy_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_min_force_timeout), std::chrono::milliseconds(opts.fsyncher_debounce_max_force_timeout)) : nullptr;
	assert(!create || fsyncher);
	return fsyncher;
}
