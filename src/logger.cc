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

#include "logger.h"

#include <ctime>              // for time_t
#include <functional>         // for ref
#include <iostream>           // for cerr
#include <regex>              // for regex_replace, regex
#include <stdarg.h>           // for va_list, va_end, va_start
#include <stdexcept>          // for out_of_range
#include <stdio.h>            // for fileno, vsnprintf, stderr
#include <stdlib.h>           // for getenv
#include <system_error>       // for system_error
#include <unistd.h>           // for isatty
#include <unordered_map>      // for unordered_map

#include "datetime.h"         // for to_string
#include "exception.h"        // for traceback
#include "ignore_unused.h"    // for ignore_unused
#include "utils.h"            // for get_thread_name


#define BUFFER_SIZE (500 * 1024)
#define STACKED_INDENT "<indent>"


std::atomic<uint64_t> logger_info_hook;

const std::regex filter_re("\033\\[[;\\d]*m", std::regex::optimize);
std::mutex Logging::stack_mtx;
std::unordered_map<std::thread::id, unsigned> Logging::stack_levels;
bool Logging::colors = false;
bool Logging::no_colors = false;
int Logging::log_level = DEFAULT_LOG_LEVEL;
std::vector<std::unique_ptr<Logger>> Logging::handlers;



static const std::string priorities[] = {
	EMERG_COL   + "█" + NO_COL, // LOG_EMERG    0 = System is unusable
	ALERT_COL   + "▉" + NO_COL, // LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL    + "▊" + NO_COL, // LOG_CRIT     2 = Critical conditions
	ERR_COL     + "▋" + NO_COL, // LOG_ERR      3 = Error conditions
	WARNING_COL + "▌" + NO_COL, // LOG_WARNING  4 = Warning conditions
	NOTICE_COL  + "▍" + NO_COL, // LOG_NOTICE   5 = Normal but significant condition
	INFO_COL    + "▎" + NO_COL, // LOG_INFO     6 = Informational
	DEBUG_COL   + "▏" + NO_COL, // LOG_DEBUG    7 = Debug-level messages
	NO_COL,                     // VERBOSE    > 7 = Verbose messages
};


static inline constexpr int
validated_priority(int priority)
{
	if (priority < 0) {
		priority = -priority;
	}
	if (priority > LOG_DEBUG + 1) {
		priority = LOG_DEBUG + 1;
	}
	return priority;
}


void
_println(bool with_endl, const char *format, va_list argptr, bool info)
{
	std::string str(Logging::_str_format(false, LOG_DEBUG, "", "", 0, "", "", format, argptr, info));
	Logging::log(LOG_DEBUG, str, 0, info, with_endl);
}


Log
_log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = _log(cleanup, stacked, wakeup, async, priority, std::string(), file, line, suffix, prefix, format, argptr);
	va_end(argptr);
	return ret;
}


Log
_log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	return Logging::log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, format, argptr);
}


Log::Log(Log&& o)
	: log(std::move(o.log))
{
	o.log.reset();
}


Log::Log(LogType log_)
	: log(log_) { }


Log::~Log()
{
	if (log) {
		log->cleanup();
	}
	log.reset();
}


Log&
Log::operator=(Log&& o)
{
	log = std::move(o.log);
	o.log.reset();
	return *this;
}


bool
Log::_unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	return log->_unlog(priority, file, line, suffix, prefix, format, argptr);
}


bool
Log::clear()
{
	return log->clear();
}


long double
Log::age()
{
	return log->age();
}


LogType
Log::release()
{
	auto ret = log;
	log.reset();
	return ret;
}

void
StreamLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	if (Logging::colors && !Logging::no_colors) {
		ofs << (with_priority ? priorities[validated_priority(priority)] : "") + str;
	} else {
		ofs << Logging::decolorize((with_priority ? priorities[validated_priority(priority)] : "") + str);
	}
	if (with_endl) {
		ofs << std::endl;
	}
}


void
StderrLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	if ((isatty(fileno(stderr)) || Logging::colors) && !Logging::no_colors) {
		std::cerr << (with_priority ? priorities[validated_priority(priority)] : "") + str;
	} else {
		std::cerr << Logging::decolorize((with_priority ? priorities[validated_priority(priority)] : "") + str);
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
	if (Logging::colors && !Logging::no_colors) {
		syslog(priority, "%s", ((with_priority ? priorities[validated_priority(priority)] : "") + str).c_str());
	} else {
		syslog(priority, "%s", Logging::decolorize((with_priority ? priorities[validated_priority(priority)] : "") + str).c_str());
	}
}


Logging::Logging(const std::string& str, bool clean_, bool stacked_, bool async_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_)
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


Logging::~Logging()
{
	cleanup();
}


std::string
Logging::decolorize(const std::string& str)
{
	return std::regex_replace(str, filter_re, "");
}


void
Logging::cleanup()
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
Logging::age()
{
	auto now = (cleared_at > created_at) ? time_point_from_ullong(cleared_at) : std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::nanoseconds>(now - time_point_from_ullong(created_at)).count();
}


/*
 * https://isocpp.org/wiki/faq/ctors#static-init-order
 * Avoid the "static initialization order fiasco"
 */

Scheduler&
Logging::scheduler()
{
	static Scheduler scheduler("LOG");
	return scheduler;
}


void
Logging::finish(int wait)
{
	scheduler().finish(wait);
}


void
Logging::join()
{
	scheduler().join();
}


void
Logging::run()
{
	L_INFO_HOOK_LOG("Logging::run", "Logging::run()");

	auto msg = str_start;
	auto log_age = age();
	if (log_age > 2e8) {
		msg += " ~" + delta_string(log_age, true);
	}
	Logging::log(priority, msg, stack_level * 2);
}


std::string
Logging::_str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr, bool info)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	std::string msg(buffer);
	std::string result;
	if (info && validated_priority(priority) <= LOG_DEBUG) {
		auto iso8601 = "[" + Datetime::iso8601(std::chrono::system_clock::now(), false, ' ') + "]";
		auto tid = " (" + get_thread_name() + ")";
		result = DARK_GREY + iso8601 + tid;
		result += " ";

#ifdef LOG_LOCATION
		result += std::string(file) + ":" + std::to_string(line) + ": ";
#endif
		result += NO_COL;
	}

	if (stacked) {
		result += STACKED_INDENT;
	}
	result += prefix + msg + suffix;
	delete []buffer;
	if (priority < 0) {
		if (exc.empty()) {
#ifdef XAPIAND_TRACEBACKS
			result += DARK_GREY + traceback(file, line) + NO_COL;
#endif
		} else {
			result += NO_COL + exc + NO_COL;
		}
	}
	return result;
}


Log
Logging::_log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	if (priority > log_level) {
		return Log(std::make_shared<Logging>("", clean, stacked, async, priority, std::chrono::system_clock::now()));
	}

	std::string str(_str_format(stacked, priority, exc, file, line, suffix, prefix, format, argptr, true)); // TODO: Slow!

	return print(str, clean, stacked, wakeup, async, priority);
}


Log
Logging::_log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = _log(clean, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, format, argptr);
	va_end(argptr);

	return ret;
}


bool
Logging::_unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	if (!clear()) {
		if (_priority > log_level) {
			return false;
		}

		std::string str(_str_format(stacked, _priority, std::string(), file, line, suffix, prefix, format, argptr, true));

		print(str, false, stacked, 0, async, _priority, time_point_from_ullong(created_at));

		return true;
	}
	return false;
}


bool
Logging::_unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = _unlog(_priority, file, line, suffix, prefix, format, argptr);
	va_end(argptr);

	return ret;
}


Log
Logging::add(const std::string& str, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	auto l_ptr = std::make_shared<Logging>(str, clean, stacked, async, priority, created_at);

	scheduler().add(l_ptr, wakeup);

	return Log(l_ptr);
}


void
Logging::log(int priority, std::string str, int indent, bool with_priority, bool with_endl)
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


Log
Logging::print(const std::string& str, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	if (priority > log_level) {
		return Log(std::make_shared<Logging>(str, clean, stacked, async, priority, created_at));
	}

	if (async || wakeup > std::chrono::system_clock::now()) {
		return add(str, clean, stacked, wakeup, async, priority, created_at);
	} else {
		auto l_ptr = std::make_shared<Logging>(str, clean, stacked, async, priority, created_at);
		log(priority, str, l_ptr->stack_level * 2);
		return Log(l_ptr);
	}
}
