/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <atomic>             // for std::atomic
#include <chrono>             // for system_clock, time_point, duration, millise...
#include <exception>          // for std::exception_ptr, std::current_exception
#include "string_view.hh"     // for std::string_view
#include <syslog.h>           // for LOG_DEBUG, LOG_WARNING, LOG_CRIT, LOG_ALERT

#include "fmt/format.h"       // for fmt::format_args, fmt::vsformat, fmt::make_format_args
#include "hashes.hh"          // for fnv1ah32
#include "lazy.hh"            // for LAZY
#include "traceback.h"        // for TRACEBACK


#define ASYNC_LOG_LEVEL LOG_ERR  // The minimum log_level that is asynchronous

extern std::atomic<uint64_t> logger_info_hook;


using namespace std::chrono_literals;


class Logging;
using LogType = std::shared_ptr<Logging>;


class Log {
	LogType log;

	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

public:
	Log() { }

	Log(Log&& o);
	Log& operator=(Log&& o);

	explicit Log(LogType log);
	~Log() noexcept;

	template <typename... Args>
	void unlog(int priority, const char* function, const char* filename, int line, std::string_view format, Args&&... args) {
		vunlog(priority, function, filename, line, format, fmt::make_format_args(std::forward<Args>(args)...));
	}
	void vunlog(int _priority, const char* _function, const char* _filename, int _line, std::string_view format, fmt::format_args args);

	bool clear();
	long double age();
	LogType release();
};


void vprintln(bool collect, bool with_endl, std::string_view format, fmt::format_args args);


template <typename... Args>
static void print(std::string_view format, Args&&... args) {
	return vprintln(false, true, format, fmt::make_format_args(std::forward<Args>(args)...));
}


template <typename... Args>
static void collect(std::string_view format, Args&&... args) {
	return vprintln(true, true, format, fmt::make_format_args(std::forward<Args>(args)...));
}


Log vlog(bool clears, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, uint64_t once, int priority, std::exception_ptr&& eptr, const char* function, const char* filename, int line, std::string_view format, fmt::format_args args);


template <typename... Args>
inline Log log(bool clears, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, uint64_t once, int priority, std::exception_ptr&& eptr, const char* function, const char* filename, int line, std::string_view format, Args&&... args) {
	return vlog(clears, wakeup, async, info, stacked, once, priority, std::move(eptr), function, filename, line, format, fmt::make_format_args(std::forward<Args>(args)...));
}


template <typename T, typename R, typename... Args>
inline Log log(bool clears, std::chrono::duration<T, R> timeout, bool async, bool info, bool stacked, uint64_t once, int priority, Args&&... args) {
	return log(clears, std::chrono::system_clock::now() + timeout, async, info, stacked, once, priority, std::forward<Args>(args)...);
}


template <typename... Args>
inline Log log(bool clears, int timeout, bool async, bool info, bool stacked, uint64_t once, int priority, Args&&... args) {
	return log(clears, std::chrono::milliseconds(timeout), async, info, stacked, once, priority, std::forward<Args>(args)...);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define LOG_ARGS_APPLY0(t, format) format
#define LOG_ARGS_APPLY1(t, format, a) format, t(a)
#define LOG_ARGS_APPLY2(t, format, a, b) format, t(a), t(b)
#define LOG_ARGS_APPLY3(t, format, a, b, c) format, t(a), t(b), t(c)
#define LOG_ARGS_APPLY4(t, format, a, b, c, d) format, t(a), t(b), t(c), t(d)
#define LOG_ARGS_APPLY5(t, format, a, b, c, d, e) format, t(a), t(b), t(c), t(d), t(e)
#define LOG_ARGS_APPLY6(t, format, a, b, c, d, e, f) format, t(a), t(b), t(c), t(d), t(e), t(f)
#define LOG_ARGS_APPLY7(t, format, a, b, c, d, e, f, g) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g)
#define LOG_ARGS_APPLY8(t, format, a, b, c, d, e, f, g, h) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h)
#define LOG_ARGS_APPLY9(t, format, a, b, c, d, e, f, g, h, i) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i)
#define LOG_ARGS_APPLY10(t, format, a, b, c, d, e, f, g, h, i, j) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j)
#define LOG_ARGS_APPLY11(t, format, a, b, c, d, e, f, g, h, i, j, k) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k)
#define LOG_ARGS_APPLY12(t, format, a, b, c, d, e, f, g, h, i, j, k, l) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k), t(l)
#define LOG_ARGS_APPLY13(t, format, a, b, c, d, e, f, g, h, i, j, k, l, m) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k), t(l), t(m)
#define LOG_ARGS_APPLY14(t, format, a, b, c, d, e, f, g, h, i, j, k, l, m, n) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k), t(l), t(m), t(n)
#define LOG_ARGS_APPLY15(t, format, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k), t(l), t(m), t(n), t(o)
#define LOG_ARGS_APPLY16(t, format, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) format, t(a), t(b), t(c), t(d), t(e), t(f), t(g), t(h), t(i), t(j), t(k), t(l), t(m), t(n), t(o), t(p)

#define LOG_ARGS_NUM_ARGS_H1(format, x16, x15, x14, x13, x12, x11, x10, x9, x8, x7, x6, x5, x4, x3, x2, x1, x0, ...) x0
#define LOG_ARGS_NUM_ARGS(...) LOG_ARGS_NUM_ARGS_H1(format, ##__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0)
#define LOG_ARGS_APPLY_ALL_H3(t, n, ...) LOG_ARGS_APPLY##n(t, __VA_ARGS__)
#define LOG_ARGS_APPLY_ALL_H2(t, n, ...) LOG_ARGS_APPLY_ALL_H3(t, n, __VA_ARGS__)
#define LOG_ARGS_APPLY_ALL(t, ...) LOG_ARGS_APPLY_ALL_H2(t, LOG_ARGS_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)

#define LAZY_LOG(clears, wakeup, async, info, stacked, once, priority, eptr, function, filename, line, ...) \
	::log(clears, wakeup, async, info, stacked, once, priority, eptr, function, filename, line, LOG_ARGS_APPLY_ALL(LAZY, __VA_ARGS__))

#define LAZY_UNLOG(priority, function, filename, line, ...) \
	unlog(priority, function, filename, line, LOG_ARGS_APPLY_ALL(LAZY, __VA_ARGS__))

#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)

#define L_DELAYED(clears, delay, priority, color, format, ...) LAZY_LOG(clears, delay, true, true, false, false, priority, std::exception_ptr{}, __func__, __FILE__, __LINE__, ((color) + (format)), ##__VA_ARGS__)
#define L_DELAYED_UNLOG(priority, color, format, ...) LAZY_UNLOG(priority, __func__, __FILE__, __LINE__, ((color) + (format)), ##__VA_ARGS__)
#define L_DELAYED_CLEAR() clear()

#define L_DELAYED_100(...) auto __log_delayed = L_DELAYED(true, 100ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_200(...) auto __log_delayed = L_DELAYED(true, 200ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_600(...) auto __log_delayed = L_DELAYED(true, 600ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_1000(...) auto __log_delayed = L_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_N(delay, ...) auto __log_delayed = L_DELAYED(true, delay, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_N_UNLOG(...) __log_delayed.L_DELAYED_UNLOG(LOG_WARNING, PURPLE, __VA_ARGS__)
#define L_DELAYED_N_CLEAR() __log_delayed.L_DELAYED_CLEAR()

#define _L_TIMED_VAR(var, delay, format_timeout, format_done, ...) { \
	if (var) { \
		var->clear(); \
	} \
	auto __log_timed = L_DELAYED(true, (delay), LOG_WARNING, LIGHT_PURPLE, (format_timeout), ##__VA_ARGS__); \
	__log_timed.L_DELAYED_UNLOG(LOG_WARNING, PURPLE, (format_done), ##__VA_ARGS__); \
	var = __log_timed.release(); \
}

#define _L_TIMED(delay, format_timeout, format_done, ...) \
	auto __log_timed = L_DELAYED(true, (delay), LOG_WARNING, LIGHT_PURPLE, (format_timeout), ##__VA_ARGS__); \
	__log_timed.L_DELAYED_UNLOG(LOG_WARNING, PURPLE, (format_done), ##__VA_ARGS__); \
}

#define L_NOTHING(...)

#define LOG(stacked, once, priority, color, format, ...) LAZY_LOG(false, 0ms, priority >= ASYNC_LOG_LEVEL, true, stacked, once, priority, std::exception_ptr{}, __func__, __FILE__, __LINE__, ((color) + (format)), ##__VA_ARGS__)

#define HOOK_LOG(hook, stacked, priority, color, format, ...) if ((logger_info_hook.load() & fnv1ah32::hash(hook)) == fnv1ah32::hash(hook)) { LAZY_LOG(false, 0ms, true, true, stacked, false, priority, std::exception_ptr{}, __func__, __FILE__, __LINE__, ((color) + (format)), ##__VA_ARGS__); }

#define L_INFO(...) LOG(true, 0, LOG_INFO, INFO_COL, __VA_ARGS__)
#define L_NOTICE(...) LOG(true, 0, LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_NOTICE_ONCE(...) LOG(true, 1, LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_NOTICE_ONCE_PER_MINUTE(...) LOG(true, std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count(), LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_WARNING(...) LOG(true, 0, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_WARNING_ONCE(...) LOG(true, 1, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_WARNING_ONCE_PER_MINUTE(...) LOG(true, std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count(), LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR(...) LOG(true, 0, LOG_ERR, ERR_COL, __VA_ARGS__)
#define L_ERR_ONCE(...) LOG(true, 1, LOG_ERR, ERR_COL, __VA_ARGS__)
#define L_ERR_ONCE_PER_MINUTE(...) LOG(true, std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count(), LOG_ERR, ERR_COL, __VA_ARGS__)
#define L_CRIT(...) LOG(true, 0, LOG_CRIT, CRIT_COL, __VA_ARGS__)
#define L_ALERT(...) LOG(true, 0, LOG_ALERT, ALERT_COL, __VA_ARGS__)
#define L_EMERG(...) LOG(true, 0, LOG_EMERG, EMERG_COL, __VA_ARGS__)
#define L_EXC(format, ...) LAZY_LOG(false, 0ms, true, true, true, false, LOG_CRIT, std::current_exception(), __func__, __FILE__, __LINE__, (ERR_COL + (format)), ##__VA_ARGS__)

#define L_TRACEBACK(...) LOG(true, 0, LOG_DEBUG, DEBUG_COL, DEBUG_COL + TRACEBACK() + "", __VA_ARGS__)

#define L_INFO_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_INFO, INFO_COL, __VA_ARGS__)
#define L_NOTICE_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_WARNING_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_ERR, ERR_COL, __VA_ARGS__)

#define L_UNINDENTED(priority, color, ...) LOG(false, 0, priority, color, __VA_ARGS__)
#define L_UNINDENTED_LOG(...) L_UNINDENTED(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L(priority, color, ...) LOG(true, 0, priority, color, __VA_ARGS__)
#define L_LOG(...) L(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L_STACKED(...) auto UNIQUE_NAME = L(__VA_ARGS__)
#define L_STACKED_LOG(...) L_STACKED(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L_COLLECT(...) ::collect(__VA_ARGS__)

#define L_PRINT(...) ::print(__VA_ARGS__)

#ifdef NDEBUG
#define L_DEBUG L_NOTHING
#define L_DEBUG_HOOK L_NOTHING
#define L_DEBUG_NOW(name)
#define L_TIMED L_NOTHING
#define L_TIMED_VAR L_NOTHING
#else
#define L_DEBUG(...) L(LOG_DEBUG, DEBUG_COL, __VA_ARGS__)
#define L_DEBUG_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_DEBUG, DEBUG_COL, __VA_ARGS__)
#define L_DEBUG_NOW(name) auto name = std::chrono::system_clock::now()
#define L_TIMED _L_TIMED
#define L_TIMED_VAR _L_TIMED_VAR
#endif

#pragma GCC diagnostic pop
