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

#include "log.h"

#include "date.h"
#include "utils.h"

#define BUFFER_SIZE (10 * 1024)

const char *priorities[] = {
	"🔥",	// LOG_EMERG    0 = System is unusable
	"⭐",	// LOG_ALERT    1 = Action must be taken immediately
	"⛔️",	// LOG_CRIT     2 = Critical conditions
	"🔴",	// LOG_ERR      3 = Error conditions
	"🔶",	// LOG_WARNING  4 = Warning conditions
	"⚪",	// LOG_NOTICE   5 = Normal but significant condition
	"🔵",	// LOG_INFO     6 = Informational
	"⚫",	// LOG_DEBUG    7 = Debug-level messages
};


void StreamLogger::log(int priority, const std::string& str) {
	ofs << priorities[priority] << "  " << str << std::endl;
}


void StderrLogger::log(int priority, const std::string& str) {
	std::cerr << priorities[priority] << "  " << str << std::endl;
}


SysLog::SysLog(const char *ident, int option, int facility) {
	openlog(ident, option, facility);
}


SysLog::~SysLog() {
	closelog();
}


void SysLog::log(int priority, const std::string& str) {
	syslog(priority, "%s", str.c_str());
}


std::vector<std::unique_ptr<Logger>> Log::handlers;


Log::Log(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup_, int priority_)
	: wakeup(wakeup_),
	  str_start(str),
	  priority(priority_),
	  finished(false) { }


std::string
Log::str_format(const char *file, int line, const char *suffix, const char *prefix, void *, const char *format, va_list argptr)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	auto iso8601 = "[" + date::to_string(std::chrono::system_clock::now()) + "]";
	auto tid = "(" + get_thread_name() + ")";
	auto location = std::string(file) + ":" + std::to_string(line);
	std::string result = iso8601 + " " + tid + + " " + location + ": " + prefix + buffer + suffix;
	delete []buffer;
	return result;
}


std::shared_ptr<Log>
Log::log(std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	std::string str(str_format(file, line, suffix, prefix, obj, format, argptr));
	va_end(argptr);

	return print(str, wakeup, priority);
}


void
Log::clear()
{
	finished.store(true);
}


void
Log::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...)
{
	if (finished.exchange(true)) {
		va_list argptr;
		va_start(argptr, format);
		std::string str(str_format(file, line, suffix, prefix, obj, format, argptr));
		va_end(argptr);

		print(str, 0, priority);
	}
}


std::shared_ptr<Log>
Log::add(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	static LogThread thread;

	// std::make_shared only can call a public constructor, for this reason
	// it is neccesary wrap the constructor in a struct.
	struct enable_make_shared : Log {
		enable_make_shared(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
			: Log(str, wakeup, priority) { }
	};

	auto l_ptr = std::make_shared<enable_make_shared>(str, wakeup, priority);
	thread.log_list.push_front(l_ptr->shared_from_this());

	if (thread.wakeup.load() > l_ptr->wakeup) {
		thread.wakeup_signal.notify_one();
	}

	return l_ptr;
}


std::shared_ptr<Log>
Log::print(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	if (!Log::handlers.size()) {
		Log::handlers.push_back(std::make_unique<StderrLogger>());
	}
	if (wakeup > std::chrono::system_clock::now()) {
		return add(str, wakeup, priority);
	} else {
		static std::mutex log_mutex;
		std::lock_guard<std::mutex> lk(log_mutex);
		for (auto& handler : Log::handlers) {
			handler->log(priority, str);
		}
		return std::shared_ptr<Log>();
	}
}


LogThread::LogThread()
	: running(true),
	  inner_thread(&LogThread::thread_function, this) { }


LogThread::~LogThread()
{
	running.store(false);
	wakeup_signal.notify_one();
	try {
		inner_thread.join();
	} catch (const std::system_error&) {
	}
}


void
LogThread::thread_function()
{
	std::mutex mtx;
	std::unique_lock<std::mutex> lk(mtx);
	while (running.load()) {
		auto now = std::chrono::system_clock::now();
		auto next_wakeup = now + 3s;
		for (auto it = log_list.begin(); it != log_list.end(); ++it) {
			if (it->use_count() == 1 || (*it)->finished.load()) {
				log_list.erase(it);
			} else if ((*it)->wakeup <= now) {
				(*it)->finished.store(true);
				Log::print((*it)->str_start, 0, (*it)->priority);
				log_list.erase(it);
			} else if (next_wakeup > (*it)->wakeup) {
				next_wakeup = (*it)->wakeup;
			}
		}
		if (next_wakeup < now + 100ms) {
			next_wakeup = now + 100ms;
		}
		wakeup.store(next_wakeup);
		wakeup_signal.wait_until(lk, next_wakeup);
	}
}
