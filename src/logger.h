/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <stdarg.h>       // for va_list
#include <syslog.h>       // for LOG_DEBUG, LOG_WARNING, LOG_CRIT, LOG_ALERT
#include <time.h>         // for time_t
#include <algorithm>      // for move
#include <atomic>         // for atomic_bool, atomic, atomic_int
#include <chrono>         // for system_clock, time_point, duration, millise...
#include <fstream>        // for ofstream
#include <memory>         // for shared_ptr, enable_shared_from_this, unique...
#include <mutex>          // for condition_variable, mutex
#include <string>         // for string, basic_string
#include <thread>         // for thread, thread::id
#include <type_traits>    // for forward, decay_t, enable_if_t, is_base_of
#include <unordered_map>  // for unordered_map

#include "dllist.h"       // for DLList
#include "exception.h"

class Exception;

#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)

#define DEFAULT_LOG_LEVEL LOG_WARNING  // The default log_level (higher than this are filtered out)
#define LOCATION_LOG_LEVEL LOG_DEBUG  // The minimum log_level that prints file:line
#define ASYNC_LOG_LEVEL LOG_ERR  // The minimum log_level that is asynchronous


using namespace std::chrono_literals;


class Logger {
public:
	virtual void log(int priority, const std::string& str) = 0;
};


class StreamLogger : public Logger {
	std::ofstream ofs;

public:
	StreamLogger(const char* filename)
		: ofs(std::ofstream(filename, std::ofstream::out)) { }

	void log(int priority, const std::string& str);
};


class StderrLogger : public Logger {
public:
	void log(int priority, const std::string& str);
};


class SysLog : public Logger {
public:
	SysLog(const char *ident="xapiand", int option=LOG_PID|LOG_CONS, int facility=LOG_USER);
	~SysLog();

	void log(int priority, const std::string& str);
};


class LogThread;
class LogWrapper;

class Log : public std::enable_shared_from_this<Log> {
	friend class LogWrapper;
	friend class LogThread;

	static LogThread& _thread();

	static std::string str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);
	static LogWrapper add(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());
	static void log(int priority, const std::string& str);

	static std::mutex stack_mtx;
	static std::unordered_map<std::thread::id, unsigned> stack_levels;
	std::thread::id thread_id;
	unsigned stack_level;
	bool stacked;

	bool clean;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	std::chrono::time_point<std::chrono::system_clock> wakeup;
	std::string str_start;
	int priority;
	std::atomic_bool finished;
	std::atomic_bool cleaned;

	Log(Log&&) = delete;
	Log(const Log&) = delete;
	Log& operator=(Log&&) = delete;
	Log& operator=(const Log&) = delete;

public:
	static int& _log_level();
	static DLList<const std::unique_ptr<Logger>>& _handlers();

	Log(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_=std::chrono::system_clock::now());
	~Log();

	template <typename T, typename R, typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority, Args&&... args);

	template <typename T, typename R>
	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	template <typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, int timeout, int priority, Args&&... args);

	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, int timeout=0, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Exception, std::decay_t<T>>::value>>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	template <typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	bool unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);
	bool clear();
	void cleanup();

	static void finish(int wait=10);

	long double age();
};


class LogWrapper {
	std::shared_ptr<Log> log;

	LogWrapper(const LogWrapper&) = delete;
	LogWrapper& operator=(const LogWrapper&) = delete;

public:
	LogWrapper(LogWrapper&& o) : log(std::move(o.log)) { o.log.reset(); }
	LogWrapper& operator=(LogWrapper&& o) { log = std::move(o.log); o.log.reset(); return *this; }

	LogWrapper(std::shared_ptr<Log> log_) : log(log_) { }
	~LogWrapper() {
		if (log) {
			log->cleanup();
		}
		log.reset();
	}

	template <typename... Args>
	bool unlog(Args&&... args) {
		return log->unlog(std::forward<Args>(args)...);
	}

	bool clear() {
		return log->clear();
	}

	long double age() {
		return log->age();
	}

	std::shared_ptr<Log> release() {
		auto ret = log;
		log.reset();
		return ret;
	}
};

class LogThread {
	std::condition_variable wakeup_signal;
	std::atomic<std::time_t> wakeup;

	DLList<const std::shared_ptr<Log>> log_list;
	std::atomic_int running;
	std::thread inner_thread;

	void thread_function(DLList<const std::shared_ptr<Log>>& log_list);

public:
	LogThread();
	~LogThread();

	void finish(int wait=10);
	void add(const std::shared_ptr<Log>& l_ptr);
};


template <typename T, typename R, typename... Args>
inline LogWrapper Log::log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::system_clock::now() + timeout, priority, std::forward<Args>(args)...);
}

template <typename T, typename R>
inline LogWrapper Log::print(const std::string& str, bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority, std::chrono::time_point<std::chrono::system_clock> created_at) {
	return print(str, cleanup, stacked, std::chrono::system_clock::now() + timeout, priority, created_at);
}

template <typename... Args>
inline LogWrapper Log::log(bool cleanup, bool stacked, int timeout, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::milliseconds(timeout), priority, std::forward<Args>(args)...);
}

inline LogWrapper Log::print(const std::string& str, bool cleanup, bool stacked, int timeout, int priority, std::chrono::time_point<std::chrono::system_clock> created_at) {
	return print(str, cleanup, stacked, std::chrono::milliseconds(timeout), priority, created_at);
}

template <typename T, typename... Args, typename>
inline LogWrapper Log::log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
	return log(cleanup, stacked, wakeup, priority, std::string(exc->get_traceback()), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
}

template <typename... Args>
inline LogWrapper Log::log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
	return log(cleanup, stacked, wakeup, priority, std::string(), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
}


#define NO_COL "\033[0m"
#define BLACK "\033[0;30m"
#define GREY "\033[0;37m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define DARK_GREY "\033[1;30m"
#define LIGHT_RED "\033[1;31m"
#define LIGHT_GREEN "\033[1;32m"
#define LIGHT_YELLOW "\033[1;33m"
#define LIGHT_BLUE "\033[1;34m"
#define LIGHT_MAGENTA "\033[1;35m"
#define LIGHT_CYAN "\033[1;36m"
#define WHITE "\033[1;37m"


#define LOG_COL WHITE
#define DEBUG_COL NO_COL
#define INFO_COL CYAN
#define NOTICE_COL LIGHT_CYAN
#define WARNING_COL LIGHT_YELLOW
#define ERR_COL RED
#define CRIT_COL LIGHT_RED
#define ALERT_COL LIGHT_RED
#define EMERG_COL LIGHT_RED


#define L_DELAYED(cleanup, delay, priority, color, args...) Log::log(cleanup, false, delay, priority, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_UNLOG(priority, color, args...) unlog(priority, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_CLEAR() clear()

#define L_DELAYED_200(args...) auto __log_timed = L_DELAYED(true, 200ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_600(args...) auto __log_timed = L_DELAYED(true, 600ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_1000(args...) auto __log_timed = L_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_N_UNLOG(args...) __log_timed.L_DELAYED_UNLOG(LOG_WARNING, MAGENTA, args)
#define L_DELAYED_N_CLEAR() __log_timed.L_DELAYED_CLEAR()

#define L_NOTHING(args...)

#define LOG(stacked, level, color, args...) Log::log(false, stacked, 0ms, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)

#define L_INFO(args...) LOG(true, LOG_INFO, INFO_COL, args)
#define L_NOTICE(args...) LOG(true, LOG_NOTICE, NOTICE_COL, args)
#define L_WARNING(args...) LOG(true, LOG_WARNING, WARNING_COL, args)
#define L_ERR(args...) LOG(true, LOG_ERR, ERR_COL, args)
#define L_CRIT(args...) LOG(true, LOG_CRIT, CRIT_COL, args)
#define L_ALERT(args...) LOG(true, -LOG_ALERT, ALERT_COL, args)
#define L_EMERG(args...) LOG(true, -LOG_EMERG, EMERG_COL, args)
#define L_EXC(args...) Log::log(false, true, 0ms, -LOG_CRIT, &exc, __FILE__, __LINE__, NO_COL, ERR_COL, args)

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
#define L_STACKED_LOG(args...) auto UNIQUE_NAME = L_LOG(args)
#define L_STACKED_BLACK(args...) auto UNIQUE_NAME = L_BLACK(args)
#define L_STACKED_GREY(args...) auto UNIQUE_NAME = L_GREY(args)
#define L_STACKED_RED(args...) auto UNIQUE_NAME = L_RED(args)
#define L_STACKED_GREEN(args...) auto UNIQUE_NAME = L_GREEN(args)
#define L_STACKED_YELLOW(args...) auto UNIQUE_NAME = L_YELLOW(args)
#define L_STACKED_BLUE(args...) auto UNIQUE_NAME = L_BLUE(args)
#define L_STACKED_MAGENTA(args...) auto UNIQUE_NAME = L_MAGENTA(args)
#define L_STACKED_CYAN(args...) auto UNIQUE_NAME = L_CYAN(args)
#define L_STACKED_DARK_GREY(args...) auto UNIQUE_NAME = L_DARK_GREY(args)
#define L_STACKED_LIGHT_RED(args...) auto UNIQUE_NAME = L_LIGHT_RED(args)
#define L_STACKED_LIGHT_GREEN(args...) auto UNIQUE_NAME = L_LIGHT_GREEN(args)
#define L_STACKED_LIGHT_YELLOW(args...) auto UNIQUE_NAME = L_LIGHT_YELLOW(args)
#define L_STACKED_LIGHT_BLUE(args...) auto UNIQUE_NAME = L_LIGHT_BLUE(args)
#define L_STACKED_LIGHT_MAGENTA(args...) auto UNIQUE_NAME = L_LIGHT_MAGENTA(args)
#define L_STACKED_LIGHT_CYAN(args...) auto UNIQUE_NAME = L_LIGHT_CYAN(args)
#define L_STACKED_WHITE(args...) auto UNIQUE_NAME = L_WHITE(args)

#ifdef NDEBUG
#define L_DEBUG L_NOTHING
#else
#define L_DEBUG(args...) L(LOG_DEBUG, DEBUG_COL, args)
#endif
