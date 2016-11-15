/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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


#include <stdarg.h>           // for va_list
#include <time.h>             // for time_t
#include <algorithm>          // for move
#include <atomic>             // for atomic_bool, atomic, atomic_int
#include <chrono>             // for system_clock, time_point, duration, millise...
#include <fstream>            // for ofstream
#include <memory>             // for shared_ptr, enable_shared_from_this, unique...
#include <mutex>              // for condition_variable, mutex
#include <string>             // for string, basic_string
#include <thread>             // for thread, thread::id
#include <type_traits>        // for forward, decay_t, enable_if_t, is_base_of
#include <unordered_map>      // for unordered_map
#include <vector>             // for vector
#include <condition_variable> // for condition_variable

#include "xapiand.h"
#include "logger_fwd.h"
#include "scheduler.h"


#define DEFAULT_LOG_LEVEL LOG_WARNING  // The default log_level (higher than this are filtered out)
#define LOCATION_LOG_LEVEL LOG_DEBUG  // The minimum log_level that prints file:line
#define ASYNC_LOG_LEVEL LOG_ERR  // The minimum log_level that is asynchronous


class Logger {
public:
	virtual void log(int priority, const std::string& str) = 0;
};


class StreamLogger : public Logger {
	std::ofstream ofs;

public:
	StreamLogger(const char* filename)
		: ofs(std::ofstream(filename, std::ofstream::out)) { }

	void log(int priority, const std::string& str) override;
};


class StderrLogger : public Logger {
public:
	void log(int priority, const std::string& str) override;
};


class SysLog : public Logger {
public:
	SysLog(const char *ident="xapiand", int option=LOG_PID|LOG_CONS, int facility=LOG_USER);
	~SysLog();

	void log(int priority, const std::string& str) override;
};


class LogThread;
class LogWrapper;


class Log : public std::enable_shared_from_this<Log> {
	friend class LogWrapper;
	friend class LogThread;

	static LogThread& _thread();

	static std::string str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);
	static LogWrapper add(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());
	static void log(int priority, std::string str, int indent=0);

	static std::mutex stack_mtx;
	static std::unordered_map<std::thread::id, unsigned> stack_levels;
	std::thread::id thread_id;
	unsigned stack_level;
	bool stacked;

	bool clean;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	std::chrono::time_point<std::chrono::system_clock> cleared_at;
	unsigned long long wakeup_time;
	std::string str_start;
	int priority;
	std::atomic_bool cleared;
	std::atomic_bool cleaned;

	Log(Log&&) = delete;
	Log(const Log&) = delete;
	Log& operator=(Log&&) = delete;
	Log& operator=(const Log&) = delete;

public:
	static int log_level;
	static std::vector<std::unique_ptr<Logger>> handlers;

	Log(const std::string& str, bool cleanup, bool stacked, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_=std::chrono::system_clock::now());
	~Log();

	template <typename T, typename R, typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority, Args&&... args);

	template <typename T, typename R>
	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	template <typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, int timeout, int priority, Args&&... args);

	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, int timeout=0, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);

	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Exception, std::decay_t<T>>::value>>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	template <typename... Args>
	static LogWrapper log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	static LogWrapper print(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	bool unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	bool unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);

	bool clear();
	void cleanup();

	static void finish(int wait=10);

	long double age();
};


#define MUL 1000000ULL

class LogQueue {
public:
	template <typename T>
	static inline uint64_t time_point_to_key(std::chrono::time_point<T> n) {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(n.time_since_epoch()).count();
	}

	static inline uint64_t now() {
		return time_point_to_key(std::chrono::system_clock::now());
	}

private:
	using _logs =         StashValues<LogType,     10ULL>;
	using _50_1ms =       StashSlots<_logs,        10ULL,   &now, 1ULL * MUL / 2ULL, 1ULL * MUL,       50ULL,   false>;
	using _10_50ms =      StashSlots<_50_1ms,      10ULL,   &now, 0ULL,              50ULL * MUL,      10ULL,   false>;
	using _3600_500ms =   StashSlots<_10_50ms,     600ULL,  &now, 0ULL,              500ULL * MUL,     3600ULL, false>;
	using _48_1800s =     StashSlots<_3600_500ms,  48ULL,   &now, 0ULL,              1800000ULL * MUL, 48ULL,   true>;
	_48_1800s queue;

public:
	LogQueue();

	LogType& peep();
	LogType& next(bool final=true, uint64_t final_key=0, bool keep_going=true);
	void add(const LogType& l_ptr, uint64_t key=0);
};


class LogThread {
	std::condition_variable wakeup_signal;
	std::atomic_ullong next_wakeup_time;

	LogQueue log_queue;
	std::atomic_int running;
	std::thread inner_thread;

	void run_one(LogType& l_ptr);
	void run();

public:
	LogThread();
	~LogThread();

	void finish(int wait=10);
	void add(const LogType& l_ptr, std::chrono::time_point<std::chrono::system_clock> wakeup);
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
