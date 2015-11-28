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


class Log : public std::enable_shared_from_this<Log> {
protected:
	Log(const char *file, int line, int timeout, const char *suffix, const char *prefix, void *obj, const char *format, va_list argptr);

public:
	std::string str_start;
	long long epoch_end;
	std::atomic_bool finished;

	static std::shared_ptr<Log> timed(const char *file, int line, int timeout, const char *suffix, const char *prefix, void *obj, const char *format, ...);
	static void end(std::shared_ptr<Log>&& l, const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...);
	static void log(const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...);
};


class ThreadLog {
public:
	std::thread inner_thread;
	std::atomic_bool running;

	void thread_function() const;


	ThreadLog()
		: running(true) { }

	ThreadLog(const ThreadLog&) = delete;
	ThreadLog& operator=(const ThreadLog&) = delete;

	~ThreadLog() {
		printf("Deleting\n");
		if (inner_thread.joinable()) {
			printf("joinable\n");
			inner_thread.detach();
			printf("detach\n");
			running.store(false);
		}
	}

	inline void start() {
		printf("++ It is Joinable: %s\n", inner_thread.joinable() ? "true" : "false");
		inner_thread = std::thread(&ThreadLog::thread_function, this);
		printf("-- It is Joinable: %s\n", inner_thread.joinable() ? "true" : "false");
	}

	static inline std::unique_ptr<ThreadLog> create() {
		auto t = std::make_unique<ThreadLog>();
		t->start();
		printf("t -> is  join: %d\n", t->inner_thread.joinable());
		return t;
	}
};


// extern std::mutex log_mutex;
// extern slist<std::shared_ptr<Log>> log_list;
// extern std::unique_ptr<ThreadLog> log_thread;

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
#define ERR_COL BRIGHT_RED
#define WARN_COL BRIGHT_YELLOW

#define _(...)
#define _LOG_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, __VA_ARGS__)
#define _LOG_LOG_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, DEBUG_COL, __VA_ARGS__)
#define _LOG_DEBUG_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, DEBUG_COL, __VA_ARGS__)
#define _LOG_INFO_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, INFO_COL, __VA_ARGS__)
#define _LOG_ERR_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, ERR_COL, __VA_ARGS__)
#define _LOG_WARN_ENABLED(...) Log::log(__FILE__, __LINE__, NOCOL, WARN_COL, __VA_ARGS__)
#define _LOG_TIMED_100(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 100, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_500(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 500, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_1000(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 1000, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)
#define _LOG_TIMED_CLEAR(...) Log::end(std::move(__timed_log), __FILE__, __LINE__, NOCOL, BRIGHT_MAGENTA, __VA_ARGS__)

#define LOG _LOG_LOG_ENABLED
#define CLOG _LOG_ENABLED
#define INFO _LOG_INFO_ENABLED
#define ERR _LOG_ERR_ENABLED
#define WARN _LOG_WARN_ENABLED
#define DEBUG _LOG_DEBUG_ENABLED

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
