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

#include "utils.h"
#include "datetime.h"

#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFER_SIZE (10 * 1024)

const std::regex filter_re("\033\\[[;\\d]*m");

const char *priorities[] = {
	EMERG_COL "█" NO_COL,	// LOG_EMERG    0 = System is unusable
	ALERT_COL "▉" NO_COL,	// LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL "▊" NO_COL,	// LOG_CRIT     2 = Critical conditions
	ERR_COL "▋" NO_COL,	// LOG_ERR      3 = Error conditions
	WARNING_COL "▌" NO_COL,	// LOG_WARNING  4 = Warning conditions
	NOTICE_COL "▍" NO_COL,	// LOG_NOTICE   5 = Normal but significant condition
	INFO_COL "▎" NO_COL,	// LOG_INFO     6 = Informational
	DEBUG_COL "▏" NO_COL,	// LOG_DEBUG    7 = Debug-level messages
};


void StreamLogger::log(int priority, const std::string& str) {
	ofs << std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "") << std::endl;
}


void StderrLogger::log(int priority, const std::string& str) {
	if (isatty(fileno(stderr))) {
		std::cerr << priorities[priority < 0 ? -priority : priority] + str << std::endl;
	} else {
		std::cerr << std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "") << std::endl;
	}
}


SysLog::SysLog(const char *ident, int option, int facility) {
	openlog(ident, option, facility);
}


SysLog::~SysLog() {
	closelog();
}


void SysLog::log(int priority, const std::string& str) {
	syslog(priority, "%s", std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "").c_str());
}


int Log::log_level = DEFAULT_LOG_LEVEL;
std::vector<std::unique_ptr<Logger>> Log::handlers;


Log::Log(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup_, int priority_)
	: wakeup(wakeup_),
	  str_start(str),
	  priority(priority_),
	  finished(false) { }


std::string
Log::str_format(int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void*, const char *format, va_list argptr)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	auto iso8601 = "[" + Datetime::to_string(std::chrono::system_clock::now()) + "]";
	auto tid = " (" + get_thread_name() + ")";
	auto location = (priority >= LOCATION_LOG_LEVEL) ? " " + std::string(file) + ":" + std::to_string(line) : std::string();
	std::string result = iso8601 + tid + location + ": " + prefix + buffer + suffix;
	delete []buffer;
	if (priority < 0) {
		if (exc.empty()) {
			result += traceback(file, line);
		} else {
			result += exc;
		}
	}
	return result;
}


std::shared_ptr<Log>
Log::log(std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	std::string str(str_format(priority, exc, file, line, suffix, prefix, obj, format, argptr));
	va_end(argptr);

	return print(str, wakeup, priority);
}


void
Log::clear()
{
	finished.store(true);
}


void
Log::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	if (finished.exchange(true)) {
		va_list argptr;
		va_start(argptr, format);
		std::string str(str_format(priority, nullptr, file, line, suffix, prefix, obj, format, argptr));
		va_end(argptr);

		print(str, 0, priority);
	}
}


std::shared_ptr<Log>
Log::add(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	static LogThread thread;

	auto l_ptr = std::make_shared<Log>(str, wakeup, priority);
	thread.log_list.push_front(l_ptr->shared_from_this());

	if (std::chrono::system_clock::from_time_t(thread.wakeup.load()) > l_ptr->wakeup) {
		thread.wakeup_signal.notify_one();
	}

	return l_ptr;
}


std::shared_ptr<Log>
Log::print(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	if (priority > Log::log_level) {
		return std::make_shared<Log>(str, wakeup, priority);
	}

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
		return std::make_shared<Log>(str, wakeup, priority);
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
		for (auto it = log_list.begin(); it != log_list.end(); ) {
			auto l_ptr = it->lock();
			if (!l_ptr || l_ptr->finished.load()) {
				it = log_list.erase(it);
			} else if (l_ptr->wakeup <= now) {
				l_ptr->finished.store(true);
				Log::print(l_ptr->str_start, 0, l_ptr->priority);
				it = log_list.erase(it);
			} else if (next_wakeup > l_ptr->wakeup) {
				next_wakeup = l_ptr->wakeup;
				++it;
			}
		}
		if (next_wakeup < now + 100ms) {
			next_wakeup = now + 100ms;
		}
		wakeup.store(std::chrono::system_clock::to_time_t(next_wakeup));
		wakeup_signal.wait_until(lk, next_wakeup);
	}
}
