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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"

#include <algorithm>          // for move
#include <atomic>             // for atomic_bool, atomic, atomic_int
#include <chrono>             // for system_clock, time_point, duration, millise...
#include <condition_variable> // for condition_variable
#include <fstream>            // for ofstream
#include <memory>             // for shared_ptr, enable_shared_from_this, unique...
#include <mutex>              // for condition_variable, mutex
#include <stdarg.h>           // for va_list
#include <string>             // for string, basic_string
#include <thread>             // for thread, thread::id
#include <time.h>             // for time_t
#include <type_traits>        // for forward, decay_t, enable_if_t, is_base_of
#include <unordered_map>      // for unordered_map
#include <vector>             // for vector

#include "logger_fwd.h"
#include "scheduler.h"
#include "xapiand.h"


#define DEFAULT_LOG_LEVEL LOG_WARNING  // The default log_level (higher than this are filtered out)


class Logger {
public:
	virtual void log(int priority, const std::string& str, bool with_priority, bool with_endl) = 0;

	virtual ~Logger() = default;
};


class StreamLogger : public Logger {
	std::ofstream ofs;

public:
	explicit StreamLogger(const char* filename)
		: ofs(filename, std::ofstream::out) { }

	void log(int priority, const std::string& str, bool with_priority, bool with_endl) override;
};


class StderrLogger : public Logger {
public:
	void log(int priority, const std::string& str, bool with_priority, bool with_endl) override;
};


class SysLog : public Logger {
public:
	SysLog(const char *ident="xapiand", int option=LOG_PID|LOG_CONS, int facility=LOG_USER);
	~SysLog();

	void log(int priority, const std::string& str, bool with_priority, bool with_endl) override;
};


class Log;


class Logging : public ScheduledTask {
	friend class Log;

	static Scheduler& scheduler();

	static Log add(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	static std::mutex stack_mtx;
	static std::unordered_map<std::thread::id, unsigned> stack_levels;
	std::thread::id thread_id;
	unsigned stack_level;
	bool stacked;

	bool clean;
	std::string str_start;
	bool async;
	int priority;
	std::atomic_bool cleaned;

	Logging(Logging&&) = delete;
	Logging(const Logging&) = delete;
	Logging& operator=(Logging&&) = delete;
	Logging& operator=(const Logging&) = delete;

public:
	static bool colors;
	static bool no_colors;

	static int log_level;
	static std::vector<std::unique_ptr<Logger>> handlers;

	Logging(const std::string& str, bool cleanup, bool stacked, bool async_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_=std::chrono::system_clock::now());
	~Logging();

	static std::string _str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr, bool info);

	static std::string decolorize(const std::string& str);

	static void finish(int wait=10);
	static void join();
	static void add(const TaskType& task, std::chrono::time_point<std::chrono::system_clock> wakeup);

	static void log(int priority, std::string str, int indent=0, bool with_priority=true, bool with_endl=true);

	template <typename T, typename R, typename... Args>
	static Log log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority, Args&&... args);

	template <typename T, typename R>
	static Log print(const std::string& str, bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	template <typename... Args>
	static Log log(bool cleanup, bool stacked, int timeout, bool async, int priority, Args&&... args);

	static Log print(const std::string& str, bool cleanup, bool stacked, int timeout=0, bool async=true, int priority=LOG_DEBUG, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	static Log _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);

	static Log _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	template <typename... Args>
	static Log log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const std::string& suffix, const std::string& prefix, const void *obj, const std::string& fmt, Args&&... args) {
		return _log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix.c_str(), prefix.c_str(), obj, fmt.c_str(), std::forward<Args>(args)...);
	}

	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
	static Log log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	template <typename... Args>
	static Log log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args);

	static Log print(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());

	bool _unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);

	bool _unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);

	template <typename... Args>
	bool unlog(int _priority, const char *file, int line, const std::string& suffix, const std::string& prefix, const void *obj, const std::string& fmt, Args&&... args) {
		return _unlog(_priority, file, line, suffix.c_str(), prefix.c_str(), obj, fmt.c_str(), std::forward<Args>(args)...);
	}

	void cleanup();

	long double age();

	void run() override;

	std::string __repr__() const override {
		return ScheduledTask::__repr__("Logging");
	}
};


template <typename T, typename R, typename... Args>
inline Log Logging::log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::system_clock::now() + timeout, async, priority, std::forward<Args>(args)...);
}


template <typename T, typename R>
inline Log Logging::print(const std::string& str, bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at) {
	return print(str, cleanup, stacked, std::chrono::system_clock::now() + timeout, async, priority, created_at);
}


template <typename... Args>
inline Log Logging::log(bool cleanup, bool stacked, int timeout, bool async, int priority, Args&&... args) {
	return log(cleanup, stacked, std::chrono::milliseconds(timeout), async, priority, std::forward<Args>(args)...);
}


inline Log Logging::print(const std::string& str, bool cleanup, bool stacked, int timeout, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at) {
	return print(str, cleanup, stacked, std::chrono::milliseconds(timeout), async, priority, created_at);
}


template <typename T, typename... Args, typename>
inline Log Logging::log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
	return log(cleanup, stacked, wakeup, async, priority, std::string(exc->get_traceback()), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
}


template <typename... Args>
inline Log Logging::log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, Args&&... args) {
	return log(cleanup, stacked, wakeup, async, priority, std::string(), file, line, suffix, prefix, obj, format, std::forward<Args>(args)...);
}

#pragma GCC diagnostic pop
