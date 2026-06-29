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

// (3) Color-aware wrappers over the extracted logger. Xapiand colors a whole line
// by prepending the macro's color to the format (the sink keeps embedded color);
// the extracted core instead colors only the priority marker by a generic palette.
// To preserve Xapiand's look, prepend the color here. LOG_C routes through the
// core's LOG (once, priority) with `(color) + (format)` as the message. This is
// also the surface colors.h builds its L_RED / L_STACKED_* shortcuts on.
#define LOG_C(once, priority, color, format, ...) \
	LOG((once), (priority), ((color) + (format)), ##__VA_ARGS__)

#undef L
#undef L_UNINDENTED
#undef L_STACKED
#define L(priority, color, ...)            LOG_C(0, (priority), color, __VA_ARGS__)
#define L_UNINDENTED(priority, color, ...) LOG_C(0, (priority), color, __VA_ARGS__)
#define L_STACKED(priority, color, ...)    LOG_C(0, (priority), color, __VA_ARGS__); ::LogIndent L_UNIQUE_NAME

// The severity macros prepend Xapiand's per-level color (INFO_COL, ERR_COL, ...)
// from log.h, restoring the colored lines the in-tree logger produced.
#undef L_INFO
#undef L_NOTICE
#undef L_WARNING
#undef L_ERR
#undef L_CRIT
#undef L_ALERT
#undef L_EMERG
#define L_INFO(...)    LOG_C(0, LOG_INFO,    INFO_COL,    __VA_ARGS__)
#define L_NOTICE(...)  LOG_C(0, LOG_NOTICE,  NOTICE_COL,  __VA_ARGS__)
#define L_WARNING(...) LOG_C(0, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR(...)     LOG_C(0, LOG_ERR,     ERR_COL,     __VA_ARGS__)
#define L_CRIT(...)    LOG_C(0, LOG_CRIT,    CRIT_COL,    __VA_ARGS__)
#define L_ALERT(...)   LOG_C(0, LOG_ALERT,   ALERT_COL,   __VA_ARGS__)
#define L_EMERG(...)   LOG_C(0, LOG_EMERG,   EMERG_COL,   __VA_ARGS__)

#undef L_INFO_ONCE
#undef L_NOTICE_ONCE
#undef L_WARNING_ONCE
#undef L_ERR_ONCE
#define L_INFO_ONCE(...)    LOG_C(1, LOG_INFO,    INFO_COL,    __VA_ARGS__)
#define L_NOTICE_ONCE(...)  LOG_C(1, LOG_NOTICE,  NOTICE_COL,  __VA_ARGS__)
#define L_WARNING_ONCE(...) LOG_C(1, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR_ONCE(...)     LOG_C(1, LOG_ERR,     ERR_COL,     __VA_ARGS__)

#undef L_WARNING_ONCE_PER_MINUTE
#undef L_ERR_ONCE_PER_MINUTE
#define L_WARNING_ONCE_PER_MINUTE(...) LOG_C(L_ONCE_PER_MINUTE_TOKEN, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR_ONCE_PER_MINUTE(...)     LOG_C(L_ONCE_PER_MINUTE_TOKEN, LOG_ERR,     ERR_COL,     __VA_ARGS__)

// L_EXC: in-flight exception at CRIT, async, with the error color prepended.
#undef L_EXC
#define L_EXC(format, ...) \
	L_LOG_BASE(false, std::chrono::milliseconds(0), true, true, 0, LOG_CRIT, std::current_exception(), ((ERR_COL) + (format)), ##__VA_ARGS__)

// (4) Deferred variants. Xapiand's call sites carry a `color` argument (prepended
// to the message, like the severity macros) and use these as expressions, so they
// override the core's colorless versions. The deferred lines are LIGHT_PURPLE.
#undef L_DELAYED
#define L_DELAYED(clears, delay, priority, color, format, ...) \
	L_LOG_BASE((clears), (delay), true, true, 0, (priority), std::exception_ptr{}, ((color) + (format)), ##__VA_ARGS__)
#undef L_DELAYED_100
#undef L_DELAYED_200
#undef L_DELAYED_1000
#undef L_DELAYED_N
#define L_DELAYED_100(...)  auto __log_delayed = L_DELAYED(true, std::chrono::milliseconds(100),  LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_200(...)  auto __log_delayed = L_DELAYED(true, std::chrono::milliseconds(200),  LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_600(...)  auto __log_delayed = L_DELAYED(true, std::chrono::milliseconds(600),  LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_1000(...) auto __log_delayed = L_DELAYED(true, std::chrono::milliseconds(1000), LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_N(delay, ...) auto __log_delayed = L_DELAYED(true, (delay), LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)

// An expression yielding a Log handle (e.g. `auto x = L_DELAYED_BACKTRACE(...)`).
// The core has no callstack-carrying variant; this is a plain colored deferred line.
#define L_DELAYED_BACKTRACE(clears, delay, priority, color, ...) \
	L_DELAYED((clears), (delay), (priority), color, __VA_ARGS__)

// Swap/cancel a pending deferred line. Xapiand passes (priority, color, fmt, ...).
#undef L_DELAYED_UNLOG
#define L_DELAYED_UNLOG(priority, color, format, ...) unlog((priority), ::format_msg(((color) + (format)), ##__VA_ARGS__))
#undef L_DELAYED_N_UNLOG
#define L_DELAYED_N_UNLOG(...) __log_delayed.L_DELAYED_UNLOG(LOG_WARNING, PURPLE, __VA_ARGS__)

// (5) Timing macros Xapiand keeps disabled by default (as the in-tree logger did).
// The core ships a real L_TIMED; override it to a no-op to preserve behavior.
#undef L_TIMED
#define L_TIMED(...) L_NOTHING()
#define L_TIMED_VAR(...) L_NOTHING()
#define L_DEBUG_NOW(name)
