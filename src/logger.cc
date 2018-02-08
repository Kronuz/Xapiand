/*
 * Copyright (C) 2015-2018 dubalu.com LLC and contributors
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

#include <cstdarg>            // for va_list, va_end, va_start
#include <cstdio>             // for fileno, vsnprintf, stderr
#include <cstdlib>            // for getenv
#include <ctime>              // for time_t
#include <functional>         // for ref
#include <iostream>           // for cerr
#include <regex>              // for regex_replace, regex
#include <stdexcept>          // for out_of_range
#include <system_error>       // for system_error
#include <unistd.h>           // for isatty
#include <unordered_map>      // for unordered_map
#include <vector>             // for vector

#include "datetime.h"         // for to_string
#include "exception.h"        // for traceback
#include "ignore_unused.h"    // for ignore_unused
#include "utils.h"            // for get_thread_name


#define STACKED_INDENT "<indent>"


std::atomic<uint64_t> logger_info_hook;

std::mutex Logging::collected_mtx;
std::vector<std::pair<std::string, bool>> Logging::collected;
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


const std::regex coloring_re("(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)", std::regex::optimize);


static inline std::string
detectColoring()
{
	const char *no_color = getenv("NO_COLOR");
	if (no_color) {
		return "";
	}
	std::string colorterm;
	const char *env_colorterm = getenv("COLORTERM");
	if (env_colorterm) {
		colorterm = env_colorterm;
	}
	std::string term;
	const char* env_term = getenv("TERM");
	if (env_term) {
		term = env_term;
	}
	if (colorterm.find("truecolor") != std::string::npos || term.find("24bit") != std::string::npos) {
		return "$1";
	} else if (term.find("256color") != std::string::npos) {
		return "$2";
	} else if (term.find("ansi") != std::string::npos || term.find("16color") != std::string::npos) {
		return "$3";
	} else {
		return "$3";
	}
}


static inline int
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
vprintln(bool collect, bool with_endl, const char *format, va_list argptr)
{
	Logging::do_println(collect, with_endl, format, argptr);
}


void
_println(bool collect, bool with_endl, const char* format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	vprintln(collect, with_endl, format, argptr);
	va_end(argptr);
}


Log
vlog(bool cleanup, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	return Logging::do_log(cleanup, info, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, format, argptr);
}


Log
_log(bool cleanup, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = vlog(cleanup, info, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, format, argptr);
	va_end(argptr);
	return ret;
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
Log::vunlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	return log->vunlog(priority, file, line, suffix, prefix, format, argptr);
}


bool
Log::_unlog(int priority, const char* file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = vunlog(priority, file, line, suffix, prefix, format, argptr);
	va_end(argptr);
	return ret;
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
	ofs << Logging::colorized((with_priority ? priorities[priority] : "") + str, Logging::colors && !Logging::no_colors);
	if (with_endl) {
		ofs << std::endl;
	}
}


void
StderrLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	std::cerr << Logging::colorized((with_priority ? priorities[priority] : "") + str, (isatty(fileno(stderr)) || Logging::colors) && !Logging::no_colors);
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
	syslog(priority, "%s", Logging::colorized((with_priority ? priorities[priority] : "") + str, Logging::colors && !Logging::no_colors).c_str());
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
Logging::colorized(const std::string& str, bool try_coloring)
{
	if (try_coloring) {
		static const auto coloring_group = detectColoring();
		return std::regex_replace(str, coloring_re, coloring_group);
	} else {
		return std::regex_replace(str, coloring_re, "");
	}
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
	dump_collected();
}


void
Logging::join()
{
	scheduler().join();
}


void
Logging::run()
{
	L_DEBUG_HOOK("Logging::run", "Logging::run()");

	auto msg = str_start;
	if (async) {
		auto log_age = age();
		if (log_age > 2e8) {
			msg += " ~" + delta_string(log_age, true);
		}
	}
	log(priority, msg, stack_level * 2);
}


std::string
Logging::format_string(bool info, bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	auto msg = vformat_string(format, argptr);

	std::string result;
	if (info && priority <= LOG_DEBUG) {
		auto iso8601 = "[" + Datetime::iso8601(std::chrono::system_clock::now(), false, ' ') + "]";
		auto tid = " (" + get_thread_name() + ")";
		result = DIM_GREY.c_str() + iso8601 + tid;
		result += " ";

#ifdef LOG_LOCATION
		result += std::string(file) + ":" + std::to_string(line) + ": ";
#else
		ignore_unused(file);
		ignore_unused(line);
#endif
		result += NO_COL.c_str();
	}

	if (stacked) {
		result += STACKED_INDENT;
	}

	result += prefix + msg + suffix;

	if (!exc.empty()) {
		result += DIM_GREY.c_str() + exc + NO_COL.c_str();
	}

	return result;
}


bool
Logging::vunlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	if (!clear()) {
		if (_priority <= log_level) {
			_priority = validated_priority(_priority);
			auto str = format_string(true, stacked, _priority, std::string(), file, line, suffix, prefix, format, argptr);
			add(str, false, stacked, std::chrono::system_clock::now(), async, _priority, time_point_from_ullong(created_at));
			return true;
		}
	}
	return false;
}


bool
Logging::_unlog(int _priority, const char *file, int line, const char *suffix, const char *prefix, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = vunlog(_priority, file, line, suffix, prefix, format, argptr);
	va_end(argptr);

	return ret;
}


void
Logging::do_println(bool collect, bool with_endl, const char *format, va_list argptr)
{
	auto str = vformat_string(format, argptr);
	if (collect) {
		std::lock_guard<std::mutex> lk(collected_mtx);
		collected.push_back(std::make_pair(std::move(str), with_endl));
	} else {
		log(0, str, 0, false, with_endl);
	}
}


Log
Logging::do_log(bool clean, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int _priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const char *format, va_list argptr)
{
	if (_priority <= log_level) {
		_priority = validated_priority(_priority);
		auto str = format_string(info, stacked, _priority, exc, file, line, suffix, prefix, format, argptr);  // TODO: Slow!
		return add(str, clean, stacked, wakeup, async, _priority);
	}

	_priority = validated_priority(_priority);
	return Log(std::make_shared<Logging>("", clean, stacked, async, _priority, std::chrono::system_clock::now()));
}


Log
Logging::add(const std::string& str, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	auto l_ptr = std::make_shared<Logging>(str, clean, stacked, async, priority, created_at);

	if (async || wakeup > std::chrono::system_clock::now()) {
		// asynchronous logs
		scheduler().add(l_ptr, wakeup);
	} else {
		// immediate logs
		l_ptr->run();
	}

	return Log(l_ptr);
}


void
Logging::log(int priority, std::string str, int indent, bool with_priority, bool with_endl)
{
	static std::mutex log_mtx;
	std::lock_guard<std::mutex> lk(log_mtx);
	auto needle = str.find(STACKED_INDENT);
	if (needle != std::string::npos) {
		str.replace(needle, sizeof(STACKED_INDENT) - 1, std::string(indent, ' '));
	}
	for (auto& handler : handlers) {
		handler->log(priority, str, with_priority, with_endl);
	}
}


void
Logging::dump_collected()
{
	std::lock_guard<std::mutex> lk(collected_mtx);
	for (const auto& s : collected) {
		log(0, s.first, 0, false, s.second);
	}
	collected.clear();
}
