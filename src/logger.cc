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

#include "logger.h"

#include <stdarg.h>      // for va_list, va_end, va_start
#include <stdio.h>       // for fileno, vsnprintf, stderr
#include <unistd.h>      // for isatty
#include <ctime>         // for time_t
#include <functional>    // for ref
#include <iostream>      // for cerr
#include <regex>         // for regex_replace, regex
#include <stdexcept>     // for out_of_range
#include <system_error>  // for system_error

#include "datetime.h"    // for to_string
#include "exception.h"   // for traceback
#include "utils.h"       // for get_thread_name

#define BUFFER_SIZE (10 * 1024)
#define STACKED_INDENT "<indent>"


std::atomic<uint64_t> logger_info_hook;

const std::regex filter_re("\033\\[[;\\d]*m");
std::mutex Log::stack_mtx;
std::unordered_map<std::thread::id, unsigned> Log::stack_levels;
int Log::log_level = DEFAULT_LOG_LEVEL;
std::vector<std::unique_ptr<Logger>> Log::handlers;


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
println(bool with_endl, const char *format, va_list argptr)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	std::string msg(buffer);
	Log::log(LOG_CRIT, msg, 0, false, with_endl);
}


LogWrapper
log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = log(cleanup, stacked, wakeup, async, priority, std::string(), file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);
	return ret;
}


LogWrapper
log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	return Log::log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, obj, format, argptr);
}


LogWrapper::LogWrapper(LogWrapper&& o)
	: log(std::move(o.log))
{
	o.log.reset();
}


LogWrapper&
LogWrapper::operator=(LogWrapper&& o)
{
	log = std::move(o.log);
	o.log.reset();
	return *this;
}

LogWrapper::LogWrapper(LogType log_)
	: log(log_) { }


LogWrapper::~LogWrapper()
{
	if (log) {
		log->cleanup();
	}
	log.reset();
}


bool
LogWrapper::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	return log->unlog(priority, file, line, suffix, prefix, obj, format, argptr);
}


bool
LogWrapper::clear()
{
	return log->clear();
}


long double
LogWrapper::age()
{
	return log->age();
}


LogType
LogWrapper::release()
{
	auto ret = log;
	log.reset();
	return ret;
}


void
StreamLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	ofs << std::regex_replace((with_priority ? priorities[priority < 0 ? -priority : priority] : "") + str, filter_re, "");
	if (with_endl) {
		ofs << std::endl;
	}
}


void
StderrLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	if (isatty(fileno(stderr))) {
		std::cerr << (with_priority ? priorities[priority < 0 ? -priority : priority] : "") + str;
	} else {
		std::cerr << std::regex_replace((with_priority ? priorities[priority < 0 ? -priority : priority] : "") + str, filter_re, "");
	}
	if (with_endl) {
		std::cerr << std::endl;
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
SysLog::log(int priority, const std::string& str, bool with_priority, bool)
{
	syslog(priority, "%s", std::regex_replace((with_priority ? priorities[priority < 0 ? -priority : priority] : "") + str, filter_re, "").c_str());
}


Log::Log(const std::string& str, bool clean_, bool stacked_, bool async_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_)
	: ScheduledTask(created_at_),
	  stack_level(0),
	  stacked(stacked_),
	  clean(clean_),
	  str_start(str),
	  async(async_),
	  priority(priority_),
	  cleaned(false)
{

	if (stacked) {
		std::lock_guard<std::mutex> lk(stack_mtx);
		thread_id = std::this_thread::get_id();
		try {
			stack_level = ++stack_levels.at(thread_id);
		} catch (const std::out_of_range&) {
			stack_levels[thread_id] = 0;
		}
	}
}


Log::~Log()
{
	cleanup();
}


void
Log::cleanup()
{
	unsigned long long c = 0;
	cleared_at.compare_exchange_strong(c, clean ? time_point_to_ullong(std::chrono::system_clock::now()) : 0);

	if (!cleaned.exchange(true)) {
		if (stacked) {
			std::lock_guard<std::mutex> lk(stack_mtx);
			if (stack_levels.at(thread_id)-- == 0) {
				stack_levels.erase(thread_id);
			}
		}
	}
}


long double
Log::age()
{
	auto now = (cleared_at > created_at) ? time_point_from_ullong(cleared_at) : std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::nanoseconds>(now - time_point_from_ullong(created_at)).count();
}


/*
 * https://isocpp.org/wiki/faq/ctors#static-init-order
 * Avoid the "static initialization order fiasco"
 */

Scheduler&
Log::scheduler()
{
	static Scheduler scheduler("LOG");
	return scheduler;
}


void
Log::finish(int wait)
{
	scheduler().finish(wait);
}


void
Log::join()
{
	scheduler().join();
}


void
Log::run()
{
	L_INFO_HOOK_LOG("Log::run", this, "Log::run()");

	auto msg = str_start;
	auto log_age = age();
	if (log_age > 2e8) {
		msg += " ~" + delta_string(log_age, true);
	}
	Log::log(priority, msg, stack_level * 2);
}


std::string
Log::str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void* obj, const char *format, va_list argptr)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	std::string msg(buffer);
	auto iso8601 = "[" + Datetime::to_string(std::chrono::system_clock::now()) + "]";
	auto tid = " (" + get_thread_name() + ")";
	std::string result = iso8601 + tid;
#ifdef LOG_OBJ_ADDRESS
	if (obj) {
		snprintf(buffer, BUFFER_SIZE, " [%p]", obj);
		result += buffer;
	}
#endif
#ifdef TRACEBACK
	auto location = (priority >= LOCATION_LOG_LEVEL) ? " " + std::string(file) + ":" + std::to_string(line) : std::string();
	result += location + ": ";
#else
	result += " ";
	(void)obj;
#endif
	if (stacked) {
		result += STACKED_INDENT;
	}
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


LogWrapper
Log::log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	std::string str(str_format(stacked, priority, exc, file, line, suffix, prefix, obj, format, argptr));

	return print(str, clean, stacked, wakeup, async, priority);
}


LogWrapper
Log::log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = log(clean, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);

	return ret;
}


bool
Log::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	if (!clear()) {
		std::string str(str_format(stacked, priority, std::string(), file, line, suffix, prefix, obj, format, argptr));

		print(str, false, stacked, 0, async, priority, time_point_from_ullong(created_at));

		return true;
	}
	return false;
}


bool
Log::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = unlog(priority, file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);

	return ret;
}


LogWrapper
Log::add(const std::string& str, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	auto l_ptr = std::make_shared<Log>(str, clean, stacked, async, priority, created_at);

	scheduler().add(l_ptr, wakeup);

	return LogWrapper(l_ptr);
}


void
Log::log(int priority, std::string str, int indent, bool with_priority, bool with_endl)
{
	static std::mutex log_mutex;
	std::lock_guard<std::mutex> lk(log_mutex);
	auto needle = str.find(STACKED_INDENT);
	if (needle != std::string::npos) {
		str.replace(needle, sizeof(STACKED_INDENT) - 1, std::string(indent, ' '));
	}
	for (auto& handler : handlers) {
		handler->log(priority, str, with_priority, with_endl);
	}
}


LogWrapper
Log::print(const std::string& str, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	if (priority > log_level) {
		return LogWrapper(std::make_shared<Log>(str, clean, stacked, async, priority, created_at));
	}

	if (async || wakeup > std::chrono::system_clock::now()) {
		return add(str, clean, stacked, wakeup, async, priority, created_at);
	} else {
		auto l_ptr = std::make_shared<Log>(str, clean, stacked, async, priority, created_at);
		log(priority, str, l_ptr->stack_level * 2);
		return LogWrapper(l_ptr);
	}
}
