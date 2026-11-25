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

#include "completion/posix.h"   // for kronuz::journal::PosixIOOptions

#include "log.h"                // for L_WARNING
#include "opts.h"               // for opts

namespace engine::strata {

// Logged once per volume (completion's rule) where the ordering barrier does not flush: macOS on storage
// other than an internal Apple SSD.
inline void log_barrier_warning(void*, const kronuz::journal::BarrierWarning& warning) {
	L_WARNING("Ordering barrier on {} ({}) is not guaranteed to flush: commits stay durable, but their order after a crash is not. Pass --full-flush-ordering to flush in full.", warning.volume, warning.source);
}

// The options of every directory IO Xapiand builds (shard acquire, the pool's resolve, the journal host):
// plain flushes by default, full ones under --full-flush-ordering (D177).
inline kronuz::journal::PosixIOOptions posix_io_options() {
	using kronuz::journal::FlushMode;
	kronuz::journal::PosixIOOptions options{opts.full_flush_ordering ? FlushMode::full : FlushMode::plain, true, true};
	options.barrier_warning = &log_barrier_warning;
	return options;
}

}  // namespace engine::strata
