/*
 * Copyright (c) 2015-2026 Dubalu LLC / Germán Méndez Bravo (Kronuz)
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

// Xapiand adapter over the extracted logger (github.com/Kronuz/logger).
//
// The extracted logger provides the types (Logging, Log, sinks, LogConfig,
// LogHooks) and the colorless macro surface (L_INFO..L_EMERG, L_*_ONCE, L_EXC,
// L_DELAYED_*, L_TIMED, L_STACKED, L_PRINT), color resolved from priority by the
// sink. This header re-adds the few things Xapiand and its dependency libraries
// expect: the trace no-op stubs the extracted stash/scheduler/threadpool use
// internally (defined BEFORE <logger.h>, since it pulls in scheduler.h), the
// color-taking L / L_UNINDENTED / L_STACKED forms, the info-hook bitmask, and a
// couple of deferred variants.

#pragma once

#include <atomic>            // for std::atomic
#include <cstdint>           // for uint64_t

// (1) Trace no-op stubs, BEFORE <logger.h>. The extracted scheduler/stash/
// threadpool trace through these (their xapiand_*_trace.h headers would otherwise
// recurse into log.h while it is mid-include). Object-like "= L_NOTHING" matches
// log.h's own stub style, so log.h's later identical redefinitions don't clash.
#define L_NOTHING(...)
#ifndef L_EXC
#define L_EXC L_NOTHING       // becomes the real async-exception log below (extracted #undefs it)
#endif
#ifndef L_DEBUG_HOOK
#define L_DEBUG_HOOK L_NOTHING
#endif
#ifndef L_CALL
#define L_CALL L_NOTHING
#endif
// (L_SCHEDULER / L_STASH are self-guarded and cleaned up by scheduler.h / stash.h.)

// (2) ThreadPolicyType, before <logger.h>. The extracted Logging is a
// ScheduledTask<..., ThreadPolicyType::logging>, and some entry orders
// (manager.h -> debouncer.h -> scheduler.h -> trace header -> log.h) reach the
// Logging class before thread.hh finishes; pulling it here makes the type always
// available. (The trace stubs above let thread.hh's own trace lines resolve.)
#include "thread.hh"

// (3) The extracted logger: types + the real colorless L_* macros (it #undefs the
// L_EXC stub above and defines the real one).
#include <logger.h>

// Xapiand's runtime info-hook bitmask (read/cleared in main.cc; gates HOOK_LOG).
extern std::atomic<uint64_t> logger_info_hook;

// Xapiand's `print(...)` free function (the extracted logger names it print_msg).
template <typename... Args>
inline void print(std::string_view fmt, Args&&... args) {
	::print_msg(fmt, std::forward<Args>(args)...);
}

// (3) Color-aware wrappers over the extracted logger. The color argument is
// accepted and ignored (the sink colorizes by priority); this is the surface
// colors.h builds its L_RED / L_STACKED_* / L_UNINDENTED_* shortcuts on. The
// extracted header defines colorless L / L_STACKED, so undefine them first.
#undef L
#undef L_STACKED
#define L(priority, color, ...)            LOG(0, (priority), __VA_ARGS__)
#define L_UNINDENTED(priority, color, ...) LOG(0, (priority), __VA_ARGS__)
#define L_STACKED(priority, color, ...)    LOG(0, (priority), __VA_ARGS__); ::LogIndent L_UNIQUE_NAME

// (4) Deferred variants Xapiand uses that the core does not ship. Color dropped.
#define L_DELAYED_600(...) auto __log_delayed = L_DELAYED(true, std::chrono::milliseconds(600), LOG_WARNING, __VA_ARGS__)
#define L_DELAYED_BACKTRACE(clears, delay, priority, color, ...) \
	auto __log_delayed = L_DELAYED((clears), (delay), (priority), __VA_ARGS__)

// (5) Timing macros Xapiand keeps disabled by default (as the in-tree logger did).
// The core ships a real L_TIMED; override it to a no-op to preserve behavior.
#undef L_TIMED
#define L_TIMED(...) L_NOTHING()
#define L_TIMED_VAR(...) L_NOTHING()
#define L_DEBUG_NOW(name)
