/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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
#include "forward_list.h"

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


#define DEFAULT_LOG_LEVEL LOG_WARNING  // The default log_level (higher than this are filtered out)
#define LOCATION_LOG_LEVEL LOG_DEBUG  // The minimum log_level that prints file:line
#define ASYNC_LOG_LEVEL LOG_INFO  // The minimum log_level that is asynchronous


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


class Log : public std::enable_shared_from_this<Log> {
	friend class LogThread;

	static LogThread& _thread();

	static std::string str_format(int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);
	static std::shared_ptr<Log> add(const std::string& str, bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority);
	static void log(int priority, const std::string& str);

	bool cleanup;
	std::chrono::time_point<std::chrono::system_clock> wakeup;
	std::string str_start;
	int priority;
	std::atomic_bool finished;

public:
	static int log_level;
	static std::vector<std::unique_ptr<Logger>> handlers;

	Log(const std::string& str, bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup_, int priority_);
	~Log();

	template <typename T, typename R, typename... Args>
	inline static std::shared_ptr<Log> log(bool cleanup, std::chrono::duration<T, R> timeout, int priority, Args&&... args) {
		return log(cleanup, std::chrono::system_clock::now() + timeout, priority, std::forward<Args>(args)...);
	}

	template <typename T, typename R>
	inline static std::shared_ptr<Log> print(const std::string& str, bool cleanup, std::chrono::duration<T, R> timeout, int priority=LOG_DEBUG) {
		return print(str, cleanup, std::chrono::system_clock::now() + timeout, priority);
	}

	template <typename... Args>
	inline static std::shared_ptr<Log> log(bool cleanup, int timeout, int priority, Args&&... args) {
		return log(cleanup, std::chrono::milliseconds(timeout), priority, std::forward<Args>(args)...);
	}

	inline static std::shared_ptr<Log> print(const std::string& str, bool cleanup, int timeout=0, int priority=LOG_DEBUG) {
		return print(str, cleanup, std::chrono::milliseconds(timeout), priority);
	}

	static std::shared_ptr<Log> log(bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Exception, std::decay_t<T>>::value>>
	inline static std::shared_ptr<Log> log(bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
		return log(cleanup, wakeup, priority, std::string(exc->get_traceback()), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline static std::shared_ptr<Log> log(bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
		return log(cleanup, wakeup, priority, std::string(), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
	}

	static std::shared_ptr<Log> print(const std::string& str, bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority);

	void unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);
	void clear();

	static void finish(bool wait=false);
};


class LogThread {
	std::condition_variable wakeup_signal;
	std::atomic<std::time_t> wakeup;

	std::atomic_bool running;
	std::thread inner_thread;
	ForwardList<std::shared_ptr<Log>> log_list;

	void thread_function();

public:
	LogThread();
	~LogThread();

	void finish(bool wait=false);
	void add(const std::shared_ptr<Log>& l_ptr);
};


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
#define BRIGHT_RED "\033[1;31m"
#define BRIGHT_GREEN "\033[1;32m"
#define BRIGHT_YELLOW "\033[1;33m"
#define BRIGHT_BLUE "\033[1;34m"
#define BRIGHT_MAGENTA "\033[1;35m"
#define BRIGHT_CYAN "\033[1;36m"
#define WHITE "\033[1;37m"

#define LOG_COL DARK_GREY
#define DEBUG_COL NO_COL
#define INFO_COL CYAN
#define NOTICE_COL BRIGHT_CYAN
#define WARNING_COL BRIGHT_YELLOW
#define ERR_COL RED
#define CRIT_COL BRIGHT_RED
#define ALERT_COL BRIGHT_RED
#define EMERG_COL BRIGHT_RED

#define _(args...)
#define _LOG_ENABLED(args...) Log::log(false, 0ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, NO_COL, args)
#define _LOG_TIMED(t, args...) auto __log_timed = Log::log(true, t, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, NO_COL, args)
#define _LOG_TIMED_CLEAR(args...) __log_timed->unlog(LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, NO_COL, args)

#define _LOG_LOG_ENABLED(args...) Log::log(false, 0ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, LOG_COL, args)
#define _LOG_DEBUG_ENABLED(args...) Log::log(false, 0ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, DEBUG_COL, args)
#define _LOG_INFO_ENABLED(args...) Log::log(false, 0ms, LOG_INFO, nullptr, __FILE__, __LINE__, NO_COL, INFO_COL, args)
#define _LOG_NOTICE_ENABLED(args...) Log::log(false, 0ms, LOG_NOTICE, nullptr, __FILE__, __LINE__, NO_COL, NOTICE_COL, args)
#define _LOG_WARNING_ENABLED(args...) Log::log(false, 0ms, LOG_WARNING, nullptr, __FILE__, __LINE__, NO_COL, WARNING_COL, args)
#define _LOG_ERR_ENABLED(args...) Log::log(false, 0ms, LOG_ERR, nullptr, __FILE__, __LINE__, NO_COL, ERR_COL, args)
#define _LOG_CRIT_ENABLED(args...) Log::log(false, 0ms, -LOG_CRIT, nullptr, __FILE__, __LINE__, NO_COL, CRIT_COL, args)
#define _LOG_ALERT_ENABLED(args...) Log::log(false, 0ms, -LOG_ALERT, nullptr, __FILE__, __LINE__, NO_COL, ALERT_COL, args)
#define _LOG_EMERG_ENABLED(args...) Log::log(false, 0ms, -LOG_EMERG, nullptr, __FILE__, __LINE__, NO_COL, EMERG_COL, args)
#define _LOG_EXC_ENABLED(args...) Log::log(false, 0ms, -LOG_ERR, &exc, __FILE__, __LINE__, NO_COL, ERR_COL, args)

#define _LOG_MARKED_ENABLED(args...) Log::log(false, 0ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, "🔥  " DEBUG_COL, args)

#define _LOG_TIMED_100(args...) auto __log_timed = Log::log(true, 100ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, BRIGHT_MAGENTA, args)
#define _LOG_TIMED_500(args...) auto __log_timed = Log::log(true, 500ms, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, BRIGHT_MAGENTA, args)
#define _LOG_TIMED_1000(args...) auto __log_timed = Log::log(true, 1s, LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, BRIGHT_MAGENTA, args)
#define _LOG_TIMED_N_CLEAR(args...) __log_timed->unlog(LOG_DEBUG, nullptr, __FILE__, __LINE__, NO_COL, BRIGHT_MAGENTA, args)

#define L _LOG_ENABLED
#define L_LOG _LOG_LOG_ENABLED
#define L_MARK _LOG_MARKED_ENABLED

#define L_BEGIN _LOG_TIMED
#define L_END _LOG_TIMED_CLEAR

#define L_DEBUG _LOG_DEBUG_ENABLED
#define L_INFO _LOG_INFO_ENABLED
#define L_NOTICE _LOG_NOTICE_ENABLED
#define L_WARNING _LOG_WARNING_ENABLED
#define L_ERR _LOG_ERR_ENABLED
#define L_CRIT _LOG_CRIT_ENABLED
#define L_ALERT _LOG_ALERT_ENABLED
#define L_EMERG _LOG_EMERG_ENABLED
#define L_EXC _LOG_EXC_ENABLED

// Enable the following, when needed, using _LOG_LOG_ENABLED:

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
#define L_DATABASE_WRAP _

// #define L_BEGIN_END
#ifdef L_BEGIN_END
#define L_OBJ_BEGIN _LOG_TIMED_1000
#define L_OBJ_END _LOG_TIMED_N_CLEAR
#define L_DATABASE_BEGIN _LOG_TIMED_100
#define L_DATABASE_END _LOG_TIMED_N_CLEAR
#define L_EV_BEGIN _LOG_TIMED_500
#define L_EV_END _LOG_TIMED_N_CLEAR
#else
#define L_OBJ_BEGIN _
#define L_OBJ_END _
#define L_DATABASE_BEGIN _
#define L_DATABASE_END _
#define L_EV_BEGIN _
#define L_EV_END _
#endif
