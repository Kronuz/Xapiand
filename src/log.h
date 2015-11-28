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

#include "slist.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

using namespace std::literals;


class Log : public std::enable_shared_from_this<Log> {
	friend class LogThread;

	static std::string str_format(const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, va_list argptr);
	static std::shared_ptr<Log> add(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup);

	std::chrono::time_point<std::chrono::system_clock> wakeup;
	std::string str_start;
	std::atomic_bool finished;

protected:
	Log(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup_);

public:
	template <typename T, typename R, typename... Args>
	static std::shared_ptr<Log> log(std::chrono::duration<T, R> timeout, Args&&... args) {
		return log(std::chrono::system_clock::now() + timeout, std::forward<Args>(args)...);
	}

	template <typename T, typename R>
	static std::shared_ptr<Log> print(const std::string& str, std::chrono::duration<T, R> timeout) {
		return print(str, std::chrono::system_clock::now() + timeout);
	}

	template <typename... Args>
	static std::shared_ptr<Log> log(int timeout, Args&&... args) {
		return log(std::chrono::system_clock::now() + std::chrono::milliseconds(timeout), std::forward<Args>(args)...);
	}

	static std::shared_ptr<Log> print(const std::string& str, int timeout=0) {
		return print(str, std::chrono::system_clock::now() + std::chrono::milliseconds(timeout));
	}

	static std::shared_ptr<Log> log(std::chrono::time_point<std::chrono::system_clock> wakeup, const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...);
	static std::shared_ptr<Log> print(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup);
	void unlog(const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...);
	void clear();
};


class LogThread {
	friend Log;

	std::condition_variable wakeup_signal;
	std::atomic<std::chrono::time_point<std::chrono::system_clock>> wakeup;

	std::atomic_bool running;
	std::thread inner_thread;
	slist<std::shared_ptr<Log>> log_list;

	void thread_function();

	LogThread();
	~LogThread();
};


#define NOCOL "\033[0m"
#define BLACK "\033[0;30m"
#define GREY "\033[0;37m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
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

#define DEBUG_COL DARK_GREY
#define INFO_COL BRIGHT_CYAN
#define ERR_COL RED
#define FATAL_COL BRIGHT_RED
#define WARN_COL BRIGHT_YELLOW

#define _(...)
#define _LOG_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, __VA_ARGS__)
#define _LOG_LOG_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, DEBUG_COL, __VA_ARGS__)
#define _LOG_DEBUG_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, DEBUG_COL, __VA_ARGS__)
#define _LOG_INFO_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, INFO_COL, __VA_ARGS__)
#define _LOG_ERR_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, ERR_COL, __VA_ARGS__)
#define _LOG_FATAL_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, FATAL_COL, __VA_ARGS__)
#define _LOG_WARN_ENABLED(...) Log::log(0ms, __FILE__, __LINE__, NOCOL, WARN_COL, __VA_ARGS__)
#define _LOG_TIMED_100(...) auto __timed_log = Log::log(100ms, __FILE__, __LINE__, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_500(...) auto __timed_log = Log::log(500ms, __FILE__, __LINE__, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_1000(...) auto __timed_log = Log::log(1s, __FILE__, __LINE__, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_CLEAR(...) __timed_log->unlog(__FILE__, __LINE__, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)

#define LOG _LOG_LOG_ENABLED
#define CLOG _LOG_ENABLED
#define INFO _LOG_INFO_ENABLED
#define ERR _LOG_ERR_ENABLED
#define FATAL _LOG_FATAL_ENABLED
#define WARN _LOG_WARN_ENABLED
#define DBG _LOG_DEBUG_ENABLED

#define LOG_ERR _LOG_ERR_ENABLED
#define LOG_WARN _LOG_WARN_ENABLED

#define LOG_DEBUG _

#define LOG_CONN _LOG_LOG_ENABLED
#define LOG_DISCOVERY _LOG_LOG_ENABLED
#define LOG_RAFT _LOG_LOG_ENABLED
#define LOG_OBJ _LOG_LOG_ENABLED
#define LOG_OBJ_BEGIN _LOG_TIMED_100
#define LOG_OBJ_END _LOG_TIMED_CLEAR
#define LOG_DATABASE _
#define LOG_DATABASE_BEGIN _LOG_TIMED_100
#define LOG_DATABASE_END _LOG_TIMED_CLEAR
#define LOG_HTTP _LOG_LOG_ENABLED
#define LOG_BINARY _LOG_LOG_ENABLED
#define LOG_HTTP_PROTO_PARSER _

#define LOG_EV _LOG_LOG_ENABLED
#define LOG_EV_BEGIN _LOG_TIMED_500
#define LOG_EV_END _LOG_TIMED_CLEAR
#define LOG_CONN_WIRE _LOG_LOG_ENABLED
#define LOG_UDP_WIRE _
#define LOG_HTTP_PROTO _
#define LOG_BINARY_PROTO _

#define LOG_DATABASE_WRAP _LOG_LOG_ENABLED

#define LOG_REPLICATION _LOG_LOG_ENABLED
