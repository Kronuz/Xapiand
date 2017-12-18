/*
 * Copyright (C) 2016,2017 deipi.com LLC and contributors. All rights reserved.
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
#include <syslog.h>           // for LOG_DEBUG, LOG_WARNING, LOG_CRIT, LOG_ALERT

#include "exception.h"        // for BaseException
#include "utils.h"
#include "xxh64.hpp"          // for xxh64
#include "ansi_color.hh"      // for ansi_color

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
	Log(Log&& o);
	Log& operator=(Log&& o);

	explicit Log(LogType log_);
	~Log();

	bool _unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr);

	bool _unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);
		auto ret = _unlog(priority, file, line, suffix, prefix, format, argptr);
		va_end(argptr);

		return ret;
	}

	template <typename... Args>
	bool unlog(int priority, const char *file, int line, const std::string& suffix, const std::string& prefix, const std::string& fmt, Args&&... args) {
		return _unlog(priority, file, line, suffix.c_str(), prefix.c_str(), fmt.c_str(), std::forward<Args>(args)...);
	}

	bool clear();
	long double age();
	LogType release();
};


void _println(bool with_endl, const char *format, va_list argptr, bool info=false);


inline void _print(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	_println(true, format, argptr);
	va_end(argptr);
}


template <typename... Args>
inline void print(const std::string& fmt, Args&&... args) {
	_print(fmt.c_str(), std::forward<Args>(args)...);
}


inline void _println(bool with_endl, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	_println(with_endl, format, argptr);
	va_end(argptr);
}


template <typename... Args>
inline void println(bool with_endl, const std::string& fmt, Args&&... args) {
	_println(with_endl, fmt.c_str(), std::forward<Args>(args)...);
}


inline void _log(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	_println(true, format, argptr, true);
	va_end(argptr);
}


template <typename... Args>
inline void log(const std::string& fmt, Args&&... args) {
	_log(fmt.c_str(), std::forward<Args>(args)...);
}


Log _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr);


template <typename T, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
inline Log _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	auto ret = _log(cleanup, stacked, wakeup, async, priority, std::string(exc->get_traceback()), file, line, suffix, prefix, format, argptr);
	va_end(argptr);
	return ret;
}


template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
inline Log log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const T* exc, const char *file, int line, const std::string& suffix, const std::string& prefix, const std::string& fmt, Args&&... args) {
	return _log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix.c_str(), prefix.c_str(), fmt.c_str(), std::forward<Args>(args)...);
}


Log _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...);


template <typename... Args>
inline Log log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void* exc, const char *file, int line, const std::string& suffix, const std::string& prefix, const std::string& fmt, Args&&... args) {
	return _log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix.c_str(), prefix.c_str(), fmt.c_str(), std::forward<Args>(args)...);
}


template <typename T, typename R, typename... Args>
inline Log log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::system_clock::now() + timeout, async, priority, std::forward<Args>(args)...);
}


template <typename... Args>
inline Log log(bool cleanup, bool stacked, int timeout, bool async, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::milliseconds(timeout), async, priority, std::forward<Args>(args)...);
}

const std::string NO_COL("\033[0m");

#define BLACK rgb(0, 0, 0)
#define RED rgb(131, 19, 11)
#define GREEN rgb(11, 131, 61)
#define YELLOW rgb(131, 87, 11)
#define BLUE rgb(11, 69, 131)
#define MAGENTA rgb(131, 11, 103)
#define CYAN rgb(11, 103, 131)
#define GREY rgb(192, 192, 192)
#define DARK_GREY rgb(96, 96, 96)
#define LIGHT_RED rgb(232, 25, 10)
#define LIGHT_GREEN rgb(10, 232, 103)
#define LIGHT_YELLOW rgb(232, 195, 10)
#define LIGHT_BLUE rgb(10, 114, 232)
#define LIGHT_MAGENTA rgb(232, 10, 180)
#define LIGHT_CYAN rgb(6, 220, 255)
#define WHITE rgb(255, 255, 255)


#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)


#define LOG_COL WHITE
#define DEBUG_COL NO_COL
#define INFO_COL CYAN
#define NOTICE_COL LIGHT_CYAN
#define WARNING_COL LIGHT_YELLOW
#define ERR_COL RED
#define CRIT_COL LIGHT_RED
#define ALERT_COL LIGHT_RED
#define EMERG_COL LIGHT_RED


#define L_DELAYED(cleanup, delay, priority, color, args...) log(cleanup, false, delay, true, priority, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_UNLOG(priority, color, args...) unlog(priority, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_CLEAR() clear()

#define L_DELAYED_200(args...) auto __log_timed = L_DELAYED(true, 200ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_600(args...) auto __log_timed = L_DELAYED(true, 600ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_1000(args...) auto __log_timed = L_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_N_UNLOG(args...) __log_timed.L_DELAYED_UNLOG(LOG_WARNING, MAGENTA, args)
#define L_DELAYED_N_CLEAR() __log_timed.L_DELAYED_CLEAR()

#define L_NOTHING(args...)

#define PRINT(stacked, level, color, args...) log(false, stacked, 0ms, false, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define LOG(stacked, level, color, args...) log(false, stacked, 0ms, level >= ASYNC_LOG_LEVEL, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)

#define L_INFO(args...) LOG(true, LOG_INFO, INFO_COL, args)
#define L_NOTICE(args...) LOG(true, LOG_NOTICE, NOTICE_COL, args)
#define L_WARNING(args...) LOG(true, LOG_WARNING, WARNING_COL, args)
#define L_ERR(args...) LOG(true, LOG_ERR, ERR_COL, args)
#define L_CRIT(args...) LOG(true, LOG_CRIT, CRIT_COL, args)
#define L_ALERT(args...) LOG(true, -LOG_ALERT, ALERT_COL, args)
#define L_EMERG(args...) LOG(true, -LOG_EMERG, EMERG_COL, args)
#define L_EXC(args...) log(false, true, 0ms, true, -LOG_CRIT, &exc, __FILE__, __LINE__, NO_COL, ERR_COL, args)

#define L_UNINDENTED(level, color, args...) LOG(false, level, color, args)
#define L_UNINDENTED_LOG(args...) L_UNINDENTED(LOG_DEBUG, LOG_COL, args)
#define L_UNINDENTED_BLACK(args...) L_UNINDENTED(LOG_DEBUG, BLACK, args)
#define L_UNINDENTED_GREY(args...) L_UNINDENTED(LOG_DEBUG, GREY, args)
#define L_UNINDENTED_RED(args...) L_UNINDENTED(LOG_DEBUG, RED, args)
#define L_UNINDENTED_GREEN(args...) L_UNINDENTED(LOG_DEBUG, GREEN, args)
#define L_UNINDENTED_YELLOW(args...) L_UNINDENTED(LOG_DEBUG, YELLOW, args)
#define L_UNINDENTED_BLUE(args...) L_UNINDENTED(LOG_DEBUG, BLUE, args)
#define L_UNINDENTED_MAGENTA(args...) L_UNINDENTED(LOG_DEBUG, MAGENTA, args)
#define L_UNINDENTED_CYAN(args...) L_UNINDENTED(LOG_DEBUG, CYAN, args)
#define L_UNINDENTED_DARK_GREY(args...) L_UNINDENTED(LOG_DEBUG, DARK_GREY, args)
#define L_UNINDENTED_LIGHT_RED(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_RED, args)
#define L_UNINDENTED_LIGHT_GREEN(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_GREEN, args)
#define L_UNINDENTED_LIGHT_YELLOW(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_UNINDENTED_LIGHT_BLUE(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_BLUE, args)
#define L_UNINDENTED_LIGHT_MAGENTA(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_UNINDENTED_LIGHT_CYAN(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_CYAN, args)
#define L_UNINDENTED_WHITE(args...) L_UNINDENTED(LOG_DEBUG, WHITE, args)

#define P(level, color, args...) PRINT(true, level, color, args)

#define L(level, color, args...) LOG(true, level, color, args)
#define L_LOG(args...) L(LOG_DEBUG, LOG_COL, args)
#define L_BLACK(args...) L(LOG_DEBUG, BLACK, args)
#define L_GREY(args...) L(LOG_DEBUG, GREY, args)
#define L_RED(args...) L(LOG_DEBUG, RED, args)
#define L_GREEN(args...) L(LOG_DEBUG, GREEN, args)
#define L_YELLOW(args...) L(LOG_DEBUG, YELLOW, args)
#define L_BLUE(args...) L(LOG_DEBUG, BLUE, args)
#define L_MAGENTA(args...) L(LOG_DEBUG, MAGENTA, args)
#define L_CYAN(args...) L(LOG_DEBUG, CYAN, args)
#define L_DARK_GREY(args...) L(LOG_DEBUG, DARK_GREY, args)
#define L_LIGHT_RED(args...) L(LOG_DEBUG, LIGHT_RED, args)
#define L_LIGHT_GREEN(args...) L(LOG_DEBUG, LIGHT_GREEN, args)
#define L_LIGHT_YELLOW(args...) L(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_LIGHT_BLUE(args...) L(LOG_DEBUG, LIGHT_BLUE, args)
#define L_LIGHT_MAGENTA(args...) L(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_LIGHT_CYAN(args...) L(LOG_DEBUG, LIGHT_CYAN, args)
#define L_WHITE(args...) L(LOG_DEBUG, WHITE, args)

#define L_STACKED(args...) auto UNIQUE_NAME = L(args)
#define L_STACKED_LOG(args...) L_STACKED(LOG_DEBUG, LOG_COL, args)
#define L_STACKED_BLACK(args...) L_STACKED(LOG_DEBUG, BLACK, args)
#define L_STACKED_GREY(args...) L_STACKED(LOG_DEBUG, GREY, args)
#define L_STACKED_RED(args...) L_STACKED(LOG_DEBUG, RED, args)
#define L_STACKED_GREEN(args...) L_STACKED(LOG_DEBUG, GREEN, args)
#define L_STACKED_YELLOW(args...) L_STACKED(LOG_DEBUG, YELLOW, args)
#define L_STACKED_BLUE(args...) L_STACKED(LOG_DEBUG, BLUE, args)
#define L_STACKED_MAGENTA(args...) L_STACKED(LOG_DEBUG, MAGENTA, args)
#define L_STACKED_CYAN(args...) L_STACKED(LOG_DEBUG, CYAN, args)
#define L_STACKED_DARK_GREY(args...) L_STACKED(LOG_DEBUG, DARK_GREY, args)
#define L_STACKED_LIGHT_RED(args...) L_STACKED(LOG_DEBUG, LIGHT_RED, args)
#define L_STACKED_LIGHT_GREEN(args...) L_STACKED(LOG_DEBUG, LIGHT_GREEN, args)
#define L_STACKED_LIGHT_YELLOW(args...) L_STACKED(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_STACKED_LIGHT_BLUE(args...) L_STACKED(LOG_DEBUG, LIGHT_BLUE, args)
#define L_STACKED_LIGHT_MAGENTA(args...) L_STACKED(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_STACKED_LIGHT_CYAN(args...) L_STACKED(LOG_DEBUG, LIGHT_CYAN, args)
#define L_STACKED_WHITE(args...) L_STACKED(LOG_DEBUG, WHITE, args)

#ifdef NDEBUG
#define L_INFO_HOOK L_NOTHING
#else
#define L_INFO_HOOK(hook, args...) if ((logger_info_hook.load() & xxh64::hash(hook)) == xxh64::hash(hook)) { P(args); }
#endif
#define L_INFO_HOOK_LOG(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LOG_COL, args)
#define L_INFO_HOOK_BLACK(hook, args...) L_INFO_HOOK(hook, LOG_INFO, BLACK, args)
#define L_INFO_HOOK_GREY(hook, args...) L_INFO_HOOK(hook, LOG_INFO, GREY, args)
#define L_INFO_HOOK_RED(hook, args...) L_INFO_HOOK(hook, LOG_INFO, RED, args)
#define L_INFO_HOOK_GREEN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, GREEN, args)
#define L_INFO_HOOK_YELLOW(hook, args...) L_INFO_HOOK(hook, LOG_INFO, YELLOW, args)
#define L_INFO_HOOK_BLUE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, BLUE, args)
#define L_INFO_HOOK_MAGENTA(hook, args...) L_INFO_HOOK(hook, LOG_INFO, MAGENTA, args)
#define L_INFO_HOOK_CYAN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, CYAN, args)
#define L_INFO_HOOK_DARK_GREY(hook, args...) L_INFO_HOOK(hook, LOG_INFO, DARK_GREY, args)
#define L_INFO_HOOK_LIGHT_RED(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_RED, args)
#define L_INFO_HOOK_LIGHT_GREEN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_GREEN, args)
#define L_INFO_HOOK_LIGHT_YELLOW(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_YELLOW, args)
#define L_INFO_HOOK_LIGHT_BLUE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_BLUE, args)
#define L_INFO_HOOK_LIGHT_MAGENTA(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_MAGENTA, args)
#define L_INFO_HOOK_LIGHT_CYAN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_CYAN, args)
#define L_INFO_HOOK_WHITE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, WHITE, args)

#define L_TEST L_NOTHING

#ifdef NDEBUG
#define L_DEBUG L_NOTHING
#else
#define L_DEBUG(args...) L(LOG_DEBUG, DEBUG_COL, args)
#endif
