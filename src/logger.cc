/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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
#include <thread>             // for std::this_thread
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
	EMERG_COL   + "█" + CLEAR_COLOR, // LOG_EMERG    0 = System is unusable
	ALERT_COL   + "▉" + CLEAR_COLOR, // LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL    + "▊" + CLEAR_COLOR, // LOG_CRIT     2 = Critical conditions
	ERR_COL     + "▋" + CLEAR_COLOR, // LOG_ERR      3 = Error conditions
	WARNING_COL + "▌" + CLEAR_COLOR, // LOG_WARNING  4 = Warning conditions
	NOTICE_COL  + "▍" + CLEAR_COLOR, // LOG_NOTICE   5 = Normal but significant condition
	INFO_COL    + "▎" + CLEAR_COLOR, // LOG_INFO     6 = Informational
	DEBUG_COL   + "▏" + CLEAR_COLOR, // LOG_DEBUG    7 = Debug-level messages
	CLEAR_COLOR,                     // VERBOSE    > 7 = Verbose messages
};


const std::regex coloring_re("(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)", std::regex::optimize);


static inline const std::string&
detectColoring()
{
	const char* no_color = getenv("NO_COLOR");
	if (no_color) {
		static const std::string _ = "";
		return _;
	}
	std::string colorterm;
	const char* env_colorterm = getenv("COLORTERM");
	if (env_colorterm) {
		colorterm = env_colorterm;
	}
	std::string term;
	const char* env_term = getenv("TERM");
	if (env_term) {
		term = env_term;
	}
	if (colorterm.find("truecolor") != std::string::npos || term.find("24bit") != std::string::npos) {
		static const std::string _ = "$1";
		return _;
	} else if (term.find("256color") != std::string::npos) {
		static const std::string _ = "$2";
		return _;
	} else if (term.find("ansi") != std::string::npos || term.find("16color") != std::string::npos) {
		static const std::string _ = "$3";
		return _;
	} else {
		static const std::string _ = "$3";
		return _;
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
vprintln(bool collect, bool with_endl, string_view format, va_list argptr)
{
	Logging::do_println(collect, with_endl, format, argptr);
}


void
_println(bool collect, bool with_endl, string_view format, int n, ...)
{
	va_list argptr;
	va_start(argptr, n);
	vprintln(collect, with_endl, format, argptr);
	va_end(argptr);
}


Log
vlog(bool cleanup, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const BaseException* exc, const char* file, int line, string_view suffix, string_view prefix, string_view format, va_list argptr)
{
	return Logging::do_log(cleanup, info, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, format, argptr);
}


Log
_log(bool cleanup, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const BaseException* exc, const char* file, int line, string_view suffix, string_view prefix, string_view format, int n, ...)
{
	va_list argptr;
	va_start(argptr, n);
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
Log::vunlog(int priority, const char* file, int line, string_view suffix, string_view prefix, string_view format, va_list argptr)
{
	return log->vunlog(priority, file, line, suffix, prefix, format, argptr);
}


bool
Log::_unlog(int priority, const char* file, int line, string_view suffix, string_view prefix, string_view format, int n, ...)
{
	va_list argptr;
	va_start(argptr, n);
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
StreamLogger::log(int priority, string_view str, bool with_priority, bool with_endl)
{
	bool colorized = Logging::colors && !Logging::no_colors;
	ofs << Logging::colorized(with_priority ? priorities[priority] : "", colorized);
	ofs << Logging::colorized(str, colorized);
	if (with_endl) {
		ofs << std::endl;
	}
}


void
StderrLogger::log(int priority, string_view str, bool with_priority, bool with_endl)
{
	static const bool is_tty = isatty(fileno(stderr));
	bool colorized = (is_tty || Logging::colors) && !Logging::no_colors;
	std::cerr << Logging::colorized(with_priority ? priorities[priority] : "", colorized);
	std::cerr << Logging::colorized(str, colorized);
	if (with_endl) {
		std::cerr << std::endl;
	}
}


SysLog::SysLog(const char* ident, int option, int facility)
{
	openlog(ident, option, facility);
}


SysLog::~SysLog()
{
	closelog();
}


void
SysLog::log(int priority, string_view str, bool with_priority, bool)
{
	bool colorized = Logging::colors && !Logging::no_colors;
	auto a = Logging::colorized(with_priority ? priorities[priority] : "", colorized);
	auto b = Logging::colorized(str, colorized);
	syslog(priority, "%s%s", a.c_str(), b.c_str());
}


Logging::Logging(string_view str, const BaseException* exc, bool clean_, bool stacked_, bool async_, int priority_, std::chrono::time_point<std::chrono::system_clock> created_at_)
	: ScheduledTask(created_at_),
	  stack_level(0),
	  stacked(stacked_),
	  clean(clean_),
	  str_start(str.data(), str.size()),
	  exception(exc),
	  async(async_),
	  priority(priority_),
	  cleaned(false)
{
	if (stacked) {
		std::lock_guard<std::mutex> lk(stack_mtx);
		thread_id = std::this_thread::get_id();
		auto it = stack_levels.find(thread_id);
		if (it == stack_levels.end()) {
			stack_levels[thread_id] = 0;
		} else {
			stack_level = ++it->second;
		}
	}
}


Logging::~Logging()
{
	cleanup();
}


std::string
Logging::colorized(string_view str, bool try_coloring)
{
	static const auto coloring_group = detectColoring();
	static const std::string empty_group;
	const auto& group = try_coloring ? coloring_group : empty_group;

	std::string result;
	std::regex_replace(std::back_inserter(result), str.begin(), str.end(), coloring_re, group);
	return result;
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

	if (!exception.empty()) {
		msg += DEBUG_COL.c_str();
		msg += exception.get_traceback();
		msg += CLEAR_COLOR.c_str();
	}

	log(priority, msg, stack_level * 2);
}


std::string
Logging::format_string(bool info, bool stacked, int priority, const char* file, int line, string_view suffix, string_view prefix, string_view format, va_list argptr)
{
	auto msg = vformat_string(format, argptr);

	std::string result;
	if (info && priority <= LOG_DEBUG) {
		auto iso8601 = "[" + Datetime::iso8601(std::chrono::system_clock::now(), false, ' ') + "]";
		auto tid = " (" + get_thread_name() + ")";
		result = DEBUG_COL.c_str() + iso8601 + tid;
		result.push_back(' ');

#ifdef LOG_LOCATION
		result += std::string(file) + ":" + std::to_string(line) + ": ";
#else
		ignore_unused(file);
		ignore_unused(line);
#endif
		result.append(CLEAR_COLOR.c_str());
	}

	if (stacked) {
		result.append(STACKED_INDENT);
	}

	result.append(prefix.data(), prefix.size());
	result.append(msg.data(), msg.size());
	result.append(suffix.data(), suffix.size());

	return result;
}


bool
Logging::vunlog(int _priority, const char* file, int line, string_view suffix, string_view prefix, string_view format, va_list argptr)
{
	if (!clear()) {
		if (_priority <= log_level) {
			_priority = validated_priority(_priority);
			auto str = format_string(true, stacked, _priority, file, line, suffix, prefix, format, argptr);
			add(str, nullptr, false, stacked, std::chrono::system_clock::now(), async, _priority, time_point_from_ullong(created_at));
			return true;
		}
	}
	return false;
}


bool
Logging::_unlog(int _priority, const char* file, int line, string_view suffix, string_view prefix, string_view format, int n, ...)
{
	va_list argptr;
	va_start(argptr, n);
	auto ret = vunlog(_priority, file, line, suffix, prefix, format, argptr);
	va_end(argptr);

	return ret;
}


void
Logging::do_println(bool collect, bool with_endl, string_view format, va_list argptr)
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
Logging::do_log(bool clean, bool info, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int _priority, const BaseException* exc, const char* file, int line, string_view suffix, string_view prefix, string_view format, va_list argptr)
{
	if (_priority <= log_level) {
		_priority = validated_priority(_priority);
		auto str = format_string(info, stacked, _priority, file, line, suffix, prefix, format, argptr);  // TODO: Slow!
		return add(str, exc, clean, stacked, wakeup, async, _priority);
	}

	_priority = validated_priority(_priority);
	return Log(std::make_shared<Logging>("", exc, clean, stacked, async, _priority, std::chrono::system_clock::now()));
}


Log
Logging::add(string_view str, const BaseException* exc, bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, std::chrono::time_point<std::chrono::system_clock> created_at)
{
	auto l_ptr = std::make_shared<Logging>(str, exc, clean, stacked, async, priority, created_at);

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
