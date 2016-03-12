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

#include "datetime.h"
#include "utils.h"

#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFER_SIZE (10 * 1024)

const std::regex filter_re("\033\\[[;\\d]*m");

const char *priorities[] = {
	EMERG_COL "█" NO_COL,   // LOG_EMERG    0 = System is unusable
	ALERT_COL "▉" NO_COL,   // LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL "▊" NO_COL,    // LOG_CRIT     2 = Critical conditions
	ERR_COL "▋" NO_COL,     // LOG_ERR      3 = Error conditions
	WARNING_COL "▌" NO_COL, // LOG_WARNING  4 = Warning conditions
	NOTICE_COL "▍" NO_COL,  // LOG_NOTICE   5 = Normal but significant condition
	INFO_COL "▎" NO_COL,    // LOG_INFO     6 = Informational
	DEBUG_COL "▏" NO_COL,   // LOG_DEBUG    7 = Debug-level messages
};


void
StreamLogger::log(int priority, const std::string& str)
{
	ofs << std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "") << std::endl;
}


void
StderrLogger::log(int priority, const std::string& str)
{
	if (isatty(fileno(stderr))) {
		std::cerr << priorities[priority < 0 ? -priority : priority] + str << std::endl;
	} else {
		std::cerr << std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "") << std::endl;
	}
}


SysLog::SysLog(const char *ident, int option, int facility)
{
	openlog(ident, option, facility);
}


SysLog::~SysLog()
{
	closelog();
}


void
SysLog::log(int priority, const std::string& str)
{
	syslog(priority, "%s", std::regex_replace(priorities[priority < 0 ? -priority : priority] + str, filter_re, "").c_str());
}


int Log::log_level = DEFAULT_LOG_LEVEL;
std::vector<std::unique_ptr<Logger>> Log::handlers;


Log::Log(const std::string& str, bool cleanup_, std::chrono::time_point<std::chrono::system_clock> wakeup_, int priority_)
	: cleanup(cleanup_),
	  wakeup(wakeup_),
	  str_start(str),
	  priority(priority_),
	  finished(false) { }

Log::~Log()
{
	bool f = false;
	finished.compare_exchange_strong(f, cleanup);
}


/*
 * https://isocpp.org/wiki/faq/ctors#static-init-order
 * Avoid the "static initialization order fiasco"
 */
LogThread&
Log::thread()
{
	static LogThread* thread_ = new LogThread();
	return *thread_;
}


std::string
Log::str_format(int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void* obj, const char *format, va_list argptr)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	std::string msg(buffer);
	auto iso8601 = "[" + Datetime::to_string(std::chrono::system_clock::now()) + "]";
	auto tid = " (" + get_thread_name() + ")";
	std::string result = iso8601 + tid;
#ifdef TRACEBACK
	if (obj) {
		snprintf(buffer, BUFFER_SIZE, " [%p]", obj);
		result += buffer;
	}
	auto location = (priority >= LOCATION_LOG_LEVEL) ? " " + std::string(file) + ":" + std::to_string(line) : std::string();
	result += location + ": ";
#endif
	result += prefix + msg + suffix;
	delete []buffer;
	if (priority < 0) {
		if (exc.empty()) {
			result += DARK_GREY + traceback(file, line) + NO_COL;
		} else {
			result += NO_COL + exc + NO_COL;
		}
	}
	return result;
}


std::shared_ptr<Log>
Log::log(bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	std::string str(str_format(priority, exc, file, line, suffix, prefix, obj, format, argptr));
	va_end(argptr);

	return print(str, cleanup, wakeup, priority);
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

		print(str, false, 0, priority);
	}
}


std::shared_ptr<Log>
Log::add(const std::string& str, bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	auto l_ptr = std::make_shared<Log>(str, cleanup, wakeup, priority);

	thread().add(l_ptr);

	return l_ptr;
}


void
Log::log(int priority, const std::string& str)
{
	static std::mutex log_mutex;
	std::lock_guard<std::mutex> lk(log_mutex);
	for (auto& handler : Log::handlers) {
		handler->log(priority, str);
	}
}

std::shared_ptr<Log>
Log::print(const std::string& str, bool cleanup, std::chrono::time_point<std::chrono::system_clock> wakeup, int priority)
{
	if (priority > Log::log_level) {
		return std::make_shared<Log>(str, cleanup, wakeup, priority);
	}

	if (!Log::handlers.size()) {
		Log::handlers.push_back(std::make_unique<StderrLogger>());
	}

	if (priority >= ASYNC_LOG_LEVEL || wakeup > std::chrono::system_clock::now()) {
		return add(str, cleanup, wakeup, priority);
	} else {
		log(priority, str);
		return std::make_shared<Log>(str, cleanup, wakeup, priority);
	}
}


void
Log::finish(bool wait)
{
	thread().finish(wait);
}


LogThread::LogThread()
	: running(true),
	  inner_thread(&LogThread::thread_function, this) { }


LogThread::~LogThread()
{
	finish(true);
}


void
LogThread::finish(bool wait)
{
	running.store(false);
	wakeup_signal.notify_all();
	if (wait) {
		try {
			inner_thread.join();
		} catch (const std::system_error&) { }
	}
}


void
LogThread::add(const std::shared_ptr<Log>& l_ptr)
{
	if (running.load()) {
		log_list.push_back(l_ptr->shared_from_this());

		if (std::chrono::system_clock::from_time_t(wakeup.load()) >= l_ptr->wakeup) {
			wakeup_signal.notify_all();
		}
	}
}


void
LogThread::thread_function()
{
	std::mutex mtx;
	std::unique_lock<std::mutex> lk(mtx);

	auto now = std::chrono::system_clock::now();
	auto next_wakeup = now + 3s;

	while (running.load()) {
		wakeup.store(std::chrono::system_clock::to_time_t(next_wakeup));
		wakeup_signal.wait_until(lk, next_wakeup);

		now = std::chrono::system_clock::now();
		next_wakeup = now + 3s;

		for (auto it = log_list.begin(); it != log_list.end(); ) {
			auto& l_ptr = *it;
			if (!l_ptr || l_ptr->finished.load()) {
				it = log_list.erase(it);
			} else if (l_ptr->wakeup <= now) {
				l_ptr->finished.store(true);
				Log::log(l_ptr->priority, l_ptr->str_start);
				it = log_list.erase(it);
			} else if (next_wakeup > l_ptr->wakeup) {
				next_wakeup = l_ptr->wakeup;
				++it;
			} else {
				++it;
			}
		}
		if (next_wakeup < now + 100ms) {
			next_wakeup = now + 100ms;
		}
	}
}
