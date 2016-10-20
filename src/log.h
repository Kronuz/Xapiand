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

#include "exception.h"
#include "dllist.h"

#include <syslog.h>

#include <exception>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <condition_variable>
#include <unordered_map>

#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)

#define DEFAULT_LOG_LEVEL LOG_WARNING  // The default log_level (higher than this are filtered out)
#define LOCATION_LOG_LEVEL LOG_DEBUG  // The minimum log_level that prints file:line
#define ASYNC_LOG_LEVEL LOG_CRIT  // The minimum log_level that is asynchronous


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


class LogWrapper;
class LogThread;

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

#define _(args...)

#define _L(level, stacked, color, args...) Log::log(false, stacked, 0ms, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)

#define _LOG_DEBUG(args...) _L(LOG_DEBUG, false, DEBUG_COL, args)
#define _LOG_INFO(args...) _L(LOG_INFO, false, INFO_COL, args)
#define _LOG_NOTICE(args...) _L(LOG_NOTICE, false, NOTICE_COL, args)
#define _LOG_WARNING(args...) _L(LOG_WARNING, false, WARNING_COL, args)
#define _LOG_ERR(args...) _L(LOG_ERR, false, ERR_COL, args)
#define _LOG_CRIT(args...) _L(-LOG_CRIT, false, CRIT_COL, args)
#define _LOG_ALERT(args...) _L(-LOG_ALERT, false, ALERT_COL, args)
#define _LOG_EMERG(args...) _L(-LOG_EMERG, false, EMERG_COL, args)
#define _LOG_EXC(args...) Log::log(false, false, 0ms, LOG_CRIT, &exc, __FILE__, __LINE__, NO_COL, ERR_COL, args)

#define _LOG(args...) _L(LOG_DEBUG, false, NO_COL, args)
#define _LOG_LOG(args...) _L(LOG_DEBUG, false, LOG_COL, args)
#define _LOG_BLACK(args...) _L(LOG_DEBUG, false, BLACK, args)
#define _LOG_GREY(args...) _L(LOG_DEBUG, false, GREY, args)
#define _LOG_RED(args...) _L(LOG_DEBUG, false, RED, args)
#define _LOG_GREEN(args...) _L(LOG_DEBUG, false, GREEN, args)
#define _LOG_YELLOW(args...) _L(LOG_DEBUG, false, YELLOW, args)
#define _LOG_BLUE(args...) _L(LOG_DEBUG, false, BLUE, args)
#define _LOG_MAGENTA(args...) _L(LOG_DEBUG, false, MAGENTA, args)
#define _LOG_CYAN(args...) _L(LOG_DEBUG, false, CYAN, args)
#define _LOG_DARK_GREY(args...) _L(LOG_DEBUG, false, DARK_GREY, args)
#define _LOG_LIGHT_RED(args...) _L(LOG_DEBUG, false, LIGHT_RED, args)
#define _LOG_LIGHT_GREEN(args...) _L(LOG_DEBUG, false, LIGHT_GREEN, args)
#define _LOG_LIGHT_YELLOW(args...) _L(LOG_DEBUG, false, LIGHT_YELLOW, args)
#define _LOG_LIGHT_BLUE(args...) _L(LOG_DEBUG, false, LIGHT_BLUE, args)
#define _LOG_LIGHT_MAGENTA(args...) _L(LOG_DEBUG, false, LIGHT_MAGENTA, args)
#define _LOG_LIGHT_CYAN(args...) _L(LOG_DEBUG, false, LIGHT_CYAN, args)
#define _LOG_WHITE(args...) _L(LOG_DEBUG, false, WHITE, args)

#define _LOG_INDENTED(args...) _L(LOG_DEBUG, true, NO_COL, args)
#define _LOG_INDENTED_LOG(args...) _L(LOG_DEBUG, true, LOG_COL, args)
#define _LOG_INDENTED_BLACK(args...) _L(LOG_DEBUG, true, BLACK, args)
#define _LOG_INDENTED_GREY(args...) _L(LOG_DEBUG, true, GREY, args)
#define _LOG_INDENTED_RED(args...) _L(LOG_DEBUG, true, RED, args)
#define _LOG_INDENTED_GREEN(args...) _L(LOG_DEBUG, true, GREEN, args)
#define _LOG_INDENTED_YELLOW(args...) _L(LOG_DEBUG, true, YELLOW, args)
#define _LOG_INDENTED_BLUE(args...) _L(LOG_DEBUG, true, BLUE, args)
#define _LOG_INDENTED_MAGENTA(args...) _L(LOG_DEBUG, true, MAGENTA, args)
#define _LOG_INDENTED_CYAN(args...) _L(LOG_DEBUG, true, CYAN, args)
#define _LOG_INDENTED_DARK_GREY(args...) _L(LOG_DEBUG, true, DARK_GREY, args)
#define _LOG_INDENTED_LIGHT_RED(args...) _L(LOG_DEBUG, true, LIGHT_RED, args)
#define _LOG_INDENTED_LIGHT_GREEN(args...) _L(LOG_DEBUG, true, LIGHT_GREEN, args)
#define _LOG_INDENTED_LIGHT_YELLOW(args...) _L(LOG_DEBUG, true, LIGHT_YELLOW, args)
#define _LOG_INDENTED_LIGHT_BLUE(args...) _L(LOG_DEBUG, true, LIGHT_BLUE, args)
#define _LOG_INDENTED_LIGHT_MAGENTA(args...) _L(LOG_DEBUG, true, LIGHT_MAGENTA, args)
#define _LOG_INDENTED_LIGHT_CYAN(args...) _L(LOG_DEBUG, true, LIGHT_CYAN, args)
#define _LOG_INDENTED_WHITE(args...) _L(LOG_DEBUG, true, WHITE, args)

#define _LOG_STACKED(args...) auto UNIQUE_NAME = _LOG_INDENTED(args)
#define _LOG_STACKED_LOG(args...) auto UNIQUE_NAME = _LOG_INDENTED_LOG(args)
#define _LOG_STACKED_BLACK(args...) auto UNIQUE_NAME = _LOG_INDENTED_BLACK(args)
#define _LOG_STACKED_GREY(args...) auto UNIQUE_NAME = _LOG_INDENTED_GREY(args)
#define _LOG_STACKED_RED(args...) auto UNIQUE_NAME = _LOG_INDENTED_RED(args)
#define _LOG_STACKED_GREEN(args...) auto UNIQUE_NAME = _LOG_INDENTED_GREEN(args)
#define _LOG_STACKED_YELLOW(args...) auto UNIQUE_NAME = _LOG_INDENTED_YELLOW(args)
#define _LOG_STACKED_BLUE(args...) auto UNIQUE_NAME = _LOG_INDENTED_BLUE(args)
#define _LOG_STACKED_MAGENTA(args...) auto UNIQUE_NAME = _LOG_INDENTED_MAGENTA(args)
#define _LOG_STACKED_CYAN(args...) auto UNIQUE_NAME = _LOG_INDENTED_CYAN(args)
#define _LOG_STACKED_DARK_GREY(args...) auto UNIQUE_NAME = _LOG_INDENTED_DARK_GREY(args)
#define _LOG_STACKED_LIGHT_RED(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_RED(args)
#define _LOG_STACKED_LIGHT_GREEN(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_GREEN(args)
#define _LOG_STACKED_LIGHT_YELLOW(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_YELLOW(args)
#define _LOG_STACKED_LIGHT_BLUE(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_BLUE(args)
#define _LOG_STACKED_LIGHT_MAGENTA(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_MAGENTA(args)
#define _LOG_STACKED_LIGHT_CYAN(args...) auto UNIQUE_NAME = _LOG_INDENTED_LIGHT_CYAN(args)
#define _LOG_STACKED_WHITE(args...) auto UNIQUE_NAME = _LOG_INDENTED_WHITE(args)

#define _LOG_MARK(args...) _L(LOG_DEBUG, false, "🔥  " DEBUG_COL, args)

#define LOG(priority, color, args...) Log::log(false, false, 0ms, priority, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define LOG_DELAYED(cleanup, delay, priority, color, args...) Log::log(cleanup, false, delay, priority, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define LOG_DELAYED_UNLOG(priority, color, args...) unlog(priority, __FILE__, __LINE__, NO_COL, color, args)
#define LOG_DELAYED_CLEAR() clear()

#define _LOG_DELAYED_200(args...) auto __log_timed = LOG_DELAYED(true, 200ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define _LOG_DELAYED_600(args...) auto __log_timed = LOG_DELAYED(true, 600ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define _LOG_DELAYED_1000(args...) auto __log_timed = LOG_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define _LOG_DELAYED_N_UNLOG(args...) __log_timed.LOG_DELAYED_UNLOG(LOG_WARNING, MAGENTA, args)
#define _LOG_DELAYED_N_CLEAR() __log_timed.LOG_DELAYED_CLEAR()

#define _DBG_SET(name, value) auto name = value
#define _LOG_INIT() _DBG_SET(start, std::chrono::system_clock::now())

#define L _LOG
#define L_LOG _LOG_INDENTED_LOG
#define L_MARK _LOG_MARK

#define L_INFO _LOG_INFO
#define L_NOTICE _LOG_NOTICE
#define L_WARNING _LOG_WARNING
#define L_ERR _LOG_ERR
#define L_CRIT _LOG_CRIT
#define L_ALERT _LOG_ALERT
#define L_EMERG _LOG_EMERG
#define L_EXC _LOG_EXC

#ifdef NDEBUG
#define L_DEBUG _
#define L_OBJ_BEGIN _
#define L_OBJ_END _
#define L_DATABASE_BEGIN _
#define L_DATABASE_END _
#define L_EV_BEGIN _
#define L_EV_END _
#define DBG_SET _
#else
#define L_DEBUG _LOG_DEBUG
#define L_OBJ_BEGIN _LOG_DELAYED_1000
#define L_OBJ_END _LOG_DELAYED_N_UNLOG
#define L_DATABASE_BEGIN _LOG_DELAYED_200
#define L_DATABASE_END _LOG_DELAYED_N_UNLOG
#define L_EV_BEGIN _LOG_DELAYED_600
#define L_EV_END _LOG_DELAYED_N_UNLOG
#define DBG_SET _DBG_SET
#endif

////////////////////////////////////////////////////////////////////////////////
// Uncomment the folloging to different logging options:

// #define LOG_OBJ_ADDRESS 1


////////////////////////////////////////////////////////////////////////////////
// Enable the following when needed. Use _LOG_* _LOG_INDENTED_* or _LOG_STACKED_*
// ex. _LOG, _LOG_STACKED_DARK_GREY, _LOG_CYAN, _LOG_STACKED_LOG or _LOG_INDENTED_MAGENTA

#define L_TRACEBACK _
#define L_CALL _
#define L_TIME _
#define L_CONN _
#define L_RAFT _
#define L_RAFT_PROTO _
#define L_DISCOVERY _
#define L_DISCOVERY_PROTO _
#define L_REPLICATION _
#define L_OBJ _
#define L_THREADPOOL _
#define L_DATABASE _
#define L_DATABASE_WAL _
#define L_HTTP _
#define L_BINARY _
#define L_HTTP_PROTO_PARSER _
#define L_EV _
#define L_CONN_WIRE _
#define L_UDP_WIRE _
#define L_HTTP_PROTO _
#define L_BINARY_PROTO _
#define L_DATABASE_WRAP_INIT _
#define L_DATABASE_WRAP _
#define L_INDEX _
#define L_SEARCH _
