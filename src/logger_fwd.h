/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include <atomic>             // for std::atomic
#include <chrono>             // for system_clock, time_point, duration, millise...
#include <cstdarg>            // for va_list, va_end
#include <string_view>        // for std::string_view
#include <syslog.h>           // for LOG_DEBUG, LOG_WARNING, LOG_CRIT, LOG_ALERT

#include "exception.h"        // for BaseException
#include "hashes.hh"          // for fnv1ah32

#define ASYNC_LOG_LEVEL LOG_ERR  // The minimum log_level that is asynchronous

extern std::atomic<uint64_t> logger_info_hook;


using namespace std::chrono_literals;


class Logging;
using LogType = std::shared_ptr<Logging>;


class Log {
	LogType log;

	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	bool _unlog(int priority, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, int n, ...);

public:
	Log(Log&& o);
	Log& operator=(Log&& o);

	explicit Log(LogType log_);
	~Log();

	template <typename... Args>
	bool unlog(int priority, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, Args&&... args) {
		return _unlog(priority, function, filename, line, suffix, prefix, format, 0, std::forward<Args>(args)...);
	}
	bool vunlog(int priority, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, va_list argptr);

	bool clear();
	long double age();
	LogType release();
};


void vprintln(bool collect, bool with_endl, std::string_view format, va_list argptr);
void _println(bool collect, bool with_endl, std::string_view format, int n, ...);


template <typename... Args>
static void print(std::string_view format, Args&&... args) {
	return _println(false, true, format, 0, std::forward<Args>(args)...);
}


template <typename... Args>
static void collect(std::string_view format, Args&&... args) {
	return _println(true, true, format, 0, std::forward<Args>(args)...);
}


Log vlog(bool cleanup, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, int priority, const BaseException* exc, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, va_list argptr);
Log _log(bool cleanup, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, int priority, const BaseException* exc, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, int n, ...);


template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
inline Log log(bool cleanup, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, int priority, const T* exc, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, Args&&... args) {
	return _log(cleanup, wakeup, async, info, stacked, priority, exc, function, filename, line, suffix, prefix, format, 0, std::forward<Args>(args)...);
}


template <typename... Args>
inline Log log(bool cleanup, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, int priority, const void*, const char* function, const char* filename, int line, const char* suffix, const char* prefix, std::string_view format, Args&&... args) {
	return _log(cleanup, wakeup, async, info, stacked, priority, nullptr, function, filename, line, suffix, prefix, format, 0, std::forward<Args>(args)...);
}


template <typename T, typename R, typename... Args>
inline Log log(bool cleanup, std::chrono::duration<T, R> timeout, bool async, bool info, bool stacked, int priority, Args&&... args) {
	return log(cleanup, std::chrono::system_clock::now() + timeout, async, info, stacked, priority, std::forward<Args>(args)...);
}


template <typename... Args>
inline Log log(bool cleanup, int timeout, bool async, bool info, bool stacked, int priority, Args&&... args) {
	return log(cleanup, std::chrono::milliseconds(timeout), async, info, stacked, priority, std::forward<Args>(args)...);
}

#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)

#define L_DELAYED(cleanup, delay, priority, color, format, ...) ::log(cleanup, delay, true, true, false, priority, nullptr, __func__, __FILE__, __LINE__, "", "", (color + format + CLEAR_COLOR), ##__VA_ARGS__)
#define L_DELAYED_UNLOG(priority, color, format, ...) unlog(priority, __func__, __FILE__, __LINE__, "", "", format, ##__VA_ARGS__)
#define L_DELAYED_CLEAR() clear()

#define L_DELAYED_200(...) auto __log_timed = L_DELAYED(true, 200ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_600(...) auto __log_timed = L_DELAYED(true, 600ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_1000(...) auto __log_timed = L_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_PURPLE, __VA_ARGS__)
#define L_DELAYED_N_UNLOG(...) __log_timed.L_DELAYED_UNLOG(LOG_WARNING, PURPLE, __VA_ARGS__)
#define L_DELAYED_N_CLEAR() __log_timed.L_DELAYED_CLEAR()

#define L_NOTHING(...)

#define LOG(stacked, level, color, format, ...) ::log(false, 0ms, level >= ASYNC_LOG_LEVEL, true, stacked, level, nullptr, __func__, __FILE__, __LINE__, "", "", (color + format + CLEAR_COLOR), ##__VA_ARGS__)

#define HOOK_LOG(hook, stacked, level, color, format, ...) if ((logger_info_hook.load() & fnv1ah32::hash(hook)) == fnv1ah32::hash(hook)) { ::log(false, 0ms, true, true, stacked, level, nullptr, __func__, __FILE__, __LINE__, "", "", (color + format + CLEAR_COLOR), ##__VA_ARGS__); }

#define L_INFO(...) LOG(true, LOG_INFO, INFO_COL, __VA_ARGS__)
#define L_NOTICE(...) LOG(true, LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_WARNING(...) LOG(true, LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR(...) LOG(true, LOG_ERR, ERR_COL, __VA_ARGS__)
#define L_CRIT(...) LOG(true, LOG_CRIT, CRIT_COL, __VA_ARGS__)
#define L_ALERT(...) LOG(true, LOG_ALERT, ALERT_COL, __VA_ARGS__)
#define L_EMERG(...) LOG(true, LOG_EMERG, EMERG_COL, __VA_ARGS__)
#define L_EXC(format, ...) ::log(false, 0ms, true, true, true, LOG_CRIT, &exc, __func__, __FILE__, __LINE__, "", "", (ERR_COL + format + CLEAR_COLOR), ##__VA_ARGS__)

#define L_INFO_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_INFO, INFO_COL, __VA_ARGS__)
#define L_NOTICE_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_NOTICE, NOTICE_COL, __VA_ARGS__)
#define L_WARNING_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_WARNING, WARNING_COL, __VA_ARGS__)
#define L_ERR_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_ERR, ERR_COL, __VA_ARGS__)

#define L_UNINDENTED(level, color, ...) LOG(false, level, color, __VA_ARGS__)
#define L_UNINDENTED_LOG(...) L_UNINDENTED(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L(level, color, ...) LOG(true, level, color, __VA_ARGS__)
#define L_LOG(...) L(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L_STACKED(...) auto UNIQUE_NAME = L(args)
#define L_STACKED_LOG(...) L_STACKED(LOG_DEBUG, LOG_COL, __VA_ARGS__)

#define L_COLLECT(...) ::collect(args)

#define L_PRINT(...) ::print(args)

#ifdef NDEBUG
#define L_DEBUG L_NOTHING
#define L_DEBUG_HOOK L_NOTHING
#else
#define L_DEBUG(...) L(LOG_DEBUG, DEBUG_COL, __VA_ARGS__)
#define L_DEBUG_HOOK(hook, ...) HOOK_LOG(hook, true, -LOG_DEBUG, DEBUG_COL, __VA_ARGS__)
#endif
