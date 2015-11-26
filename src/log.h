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
	Log(const char *file, int line, int timeout, void *obj, const char *format, va_list argptr);

public:
	std::string str_start;
	long long epoch_end;
	std::atomic_bool finished;

	static std::shared_ptr<Log> timed(const char *file, int line, int timeout, void *obj, const char *format, ...);
	static void end(std::shared_ptr<Log>&& l, const char *file, int line, void *obj, const char *format, ...);
	static void log(const char *file, int line, void *obj, const char *format, ...);
};


extern std::mutex log_mutex;
extern std::atomic_bool log_runner;
extern slist<std::shared_ptr<Log>> log_list;


class ThreadLog {
	std::thread t;

public:
	template <class F, class... Args>
	ThreadLog(F&& f, Args&&... args)
		: t(std::forward<F>(f), std::forward<Args>(args)...) { }

	~ThreadLog() {
		log_runner.store(false);
		t.detach();
	}
};


extern std::unique_ptr<ThreadLog> log_thread;


#define _(...)
#define _LOG_ENABLED(...) Log::log(__FILE__, __LINE__, __VA_ARGS__)
#define _LOG_TIMED_100(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 100, __VA_ARGS__)
#define _LOG_TIMED_500(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 500, __VA_ARGS__)
#define _LOG_TIMED_1000(...) auto __timed_log = Log::timed(__FILE__, __LINE__, 1000, __VA_ARGS__)
#define _LOG_TIMED_CLEAR(...) Log::end(std::move(__timed_log), __FILE__, __LINE__, __VA_ARGS__)


#define INFO _LOG_ENABLED

#define LOG _LOG_ENABLED

#define LOG_ERR _LOG_ENABLED

#define LOG_DEBUG _

#define LOG_CONN _LOG_ENABLED
#define LOG_DISCOVERY _LOG_ENABLED
#define LOG_RAFT _LOG_ENABLED
#define LOG_OBJ _LOG_ENABLED
#define LOG_OBJ_BEGIN _LOG_TIMED_100
#define LOG_OBJ_END _LOG_TIMED_CLEAR
#define LOG_DATABASE _
#define LOG_DATABASE_BEGIN _LOG_TIMED_100
#define LOG_DATABASE_END _LOG_TIMED_CLEAR
#define LOG_HTTP _LOG_ENABLED
#define LOG_BINARY _LOG_ENABLED
#define LOG_HTTP_PROTO_PARSER _

#define LOG_EV _LOG_ENABLED
#define LOG_EV_BEGIN _LOG_TIMED_500
#define LOG_EV_END _LOG_TIMED_CLEAR
#define LOG_CONN_WIRE _LOG_ENABLED
#define LOG_UDP_WIRE _
#define LOG_HTTP_PROTO _
#define LOG_BINARY_PROTO _

#define LOG_DATABASE_WRAP _LOG_ENABLED

#define LOG_REPLICATION _LOG_ENABLED
