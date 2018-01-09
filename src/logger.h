/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
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

	static std::mutex collected_mtx;
	static std::vector<std::shared_ptr<Logging>> collected;

	static std::mutex stack_mtx;
	static std::unordered_map<std::thread::id, unsigned> stack_levels;
	std::thread::id thread_id;
	unsigned stack_level;
	bool stacked;

	bool clean;
	std::string str_start;
	int async;
	int priority;
	std::atomic_bool cleaned;

	Logging(Logging&&) = delete;
	Logging(const Logging&) = delete;
	Logging& operator=(Logging&&) = delete;
	Logging& operator=(const Logging&) = delete;

	bool _unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr);
	bool _unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...);

	static std::string str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr, bool info);
	static Log add(const std::string& str, bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at=std::chrono::system_clock::now());
	static void log(int priority, std::string str, int indent=0, bool with_priority=true, bool with_endl=true);

public:
	static bool colors;
	static bool no_colors;

	static int log_level;
	static std::vector<std::unique_ptr<Logger>> handlers;

	Logging(const std::string& str, bool cleanup, bool stacked, int async_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_=std::chrono::system_clock::now());
	~Logging();

	static std::string decolorize(const std::string& str);

	static void finish(int wait=10);
	static void join();
	static void dump_collected();

	static void _println(bool info, bool with_endl, const char *format, va_list argptr);
	static Log _log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, int async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr);

	template <typename S, typename P, typename F, typename... Args>
	bool unlog(int _priority, const char *file, int line, S&& suffix, P&& prefix, F&& format, Args&&... args) {
		return _unlog(_priority, file, line, cstr(std::forward<S>(suffix)), cstr(std::forward<P>(prefix)), cstr(std::forward<F>(format)), std::forward<Args>(args)...);
	}

	void cleanup();

	long double age();

	void run() override;

	std::string __repr__() const override {
		return ScheduledTask::__repr__("Logging");
	}
};

#pragma GCC diagnostic pop
