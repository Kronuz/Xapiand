/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "logger.h"

#include <cerrno>             // for errno
#include <cstdio>             // for fileno, vsnprintf, stderr
#include <cstdlib>            // for getenv
#include <ctime>              // for time_t
#include <functional>         // for ref
#include <regex>              // for regex_replace, regex
#include <stdexcept>          // for out_of_range
#include <system_error>       // for std::system_error
#include <thread>             // for std::this_thread
#include <unistd.h>           // for isatty
#include <unordered_map>      // for unordered_map
#include <utility>
#include <vector>             // for vector

#include "base_x.hh"          // for Base64
#include "bloom_filter.hh"    // for BloomFilter
#include "datetime.h"         // for to_string
#include "exception.h"        // for traceback
#include "ignore_unused.h"    // for ignore_unused
#include "io.hh"              // for io::write
#include "opts.h"             // for opts
#include "string.hh"          // for  string::format
#include "thread.hh"          // for get_thread_name
#include "time_point.hh"      // for time_point_to_ullong


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


#define MAX_PRIORITY (LOG_DEBUG + 1)
static const std::string priorities[MAX_PRIORITY + 1] = {
	EMERG_COL   + "█" + CLEAR_COLOR, // LOG_EMERG    0 = System is unusable
	ALERT_COL   + "▉" + CLEAR_COLOR, // LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL    + "▊" + CLEAR_COLOR, // LOG_CRIT     2 = Critical conditions
	ERR_COL     + "▋" + CLEAR_COLOR, // LOG_ERR      3 = Error conditions
	WARNING_COL + "▌" + CLEAR_COLOR, // LOG_WARNING  4 = Warning conditions
	NOTICE_COL  + "▍" + CLEAR_COLOR, // LOG_NOTICE   5 = Normal but significant condition
	INFO_COL    + "▎" + CLEAR_COLOR, // LOG_INFO     6 = Informational
	DEBUG_COL   + "▏" + CLEAR_COLOR, // LOG_DEBUG    7 = Debug-level messages
	NO_COLOR,                        // VERBOSE    > 7 = Verbose messages
};


const std::regex coloring_re("(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)(" ESC "\\[[;\\d]*m)", std::regex::optimize);


static inline bool
is_tty()
{
	static const bool is_tty = ::isatty(STDERR_FILENO) != 0;
	return is_tty;
}


static inline const std::string&
detectColoring()
{
	const char* no_color = getenv("NO_COLOR");
	if (no_color != nullptr) {
		static const std::string _;
		return _;
	}
	std::string colorterm;
	const char* env_colorterm = getenv("COLORTERM");
	if (env_colorterm != nullptr) {
		colorterm = env_colorterm;
	}
	std::string term;
	const char* env_term = getenv("TERM");
	if (env_term != nullptr) {
		term = env_term;
	}

	if (colorterm.find("truecolor") != std::string::npos || term.find("24bit") != std::string::npos) {
		static const std::string _ = "$1";
		return _;
	}
	if (term.find("256color") != std::string::npos) {
		static const std::string _ = "$2";
		return _;
	}
	// if (term.find("ansi") != std::string::npos || term.find("16color") != std::string::npos) {
	// 	static const std::string _ = "$3";
	// 	return _;
	// }
	static const std::string _ = "$3";
	return _;
}


static inline int
validated_priority(int priority)
{
	if (priority < 0) {
		priority = -priority;
	}
	if (priority > MAX_PRIORITY) {
		priority = MAX_PRIORITY;
	}
	return priority;
}


void
vprintln(bool collect, bool with_endl, std::string_view format, fmt::printf_args args)
{
	Logging::do_println(collect, with_endl, format, args);
}


Log
vlog(bool cleanup, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, bool once, int priority, const BaseException* exc, const char* function, const char* filename, int line, std::string_view format, fmt::printf_args args)
{
	return Logging::do_log(cleanup, wakeup, async, info, stacked, once, priority, exc, function, filename, line, format, args);
}


Log::Log(Log&& o)
	: log(std::move(o.log))
{
}


Log::Log(LogType log)
	: log(std::move(log))
{
}


Log::~Log()
{
	if (log) { log->cleanup(); }
}


Log&
Log::operator=(Log&& o)
{
	log = std::move(o.log);
	o.log.reset();
	return *this;
}


bool
Log::vunlog(int _priority, const char* _function, const char* _filename, int _line, std::string_view format, fmt::printf_args args)
{
	return log ? log->vunlog(_priority, _function, _filename, _line, format, args) : false;
}


bool
Log::clear()
{
	return log ? log->clear() : false;
}


long double
Log::age()
{
	return log ? log->age() : 0;
}


LogType
Log::release()
{
	auto ret = log;
	log.reset();
	return ret;
}

StreamLogger::StreamLogger(const char* filename)
	: fdout(io::open(filename, O_WRONLY | O_CREAT | O_APPEND))
{
	if (fdout == -1) {
		throw std::system_error(errno, std::generic_category());
	}
}


void
StreamLogger::log(int priority, std::string_view str, bool with_priority, bool with_endl)
{
	std::string buf;
	if (with_priority) {
		buf += priorities[priority];
	}
	buf += str;
	if (with_endl) {
		buf += "\n";
	}

	bool colorized = Logging::colors && !Logging::no_colors;
	buf = Logging::colorized(buf, colorized);

	io::write(fdout, buf.data(), buf.size());
}


void
StderrLogger::log(int priority, std::string_view str, bool with_priority, bool with_endl)
{
	std::string buf;
	if (with_priority) {
		buf += priorities[priority];
	}
	buf += str;
	if (with_endl) {
		buf += "\n";
	}

	bool colorized = (is_tty() || Logging::colors) && !Logging::no_colors;
	buf = Logging::colorized(buf, colorized);

	io::write(STDERR_FILENO, buf.data(), buf.size());
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
SysLog::log(int priority, std::string_view str, bool with_priority, bool /*with_endl*/)
{
	std::string buf;
	if (with_priority) {
		buf += priorities[priority];
	}
	buf += str;

	bool colorized = Logging::colors && !Logging::no_colors;
	buf = Logging::colorized(buf, colorized);

	syslog(priority, "%s", buf.c_str());
}


Logging::Logging(const char *function, const char *filename, int line, std::string  str, const BaseException* exc, bool clean, bool async, bool info, bool stacked, bool once, int priority, const std::chrono::time_point<std::chrono::system_clock>& created_at)
	: ScheduledTask(created_at),
	  thread_id(std::this_thread::get_id()),
	  function(function),
	  filename(filename),
	  line(line),
	  stack_level(0),
	  clean(clean),
	  str(std::move(str)),
	  exception(exc),
	  async(async),
	  info(info),
	  stacked(stacked),
	  once(once),
	  priority(priority),
	  cleaned(false)
{
	if (stacked) {
		std::lock_guard<std::mutex> lk(stack_mtx);
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
Logging::colorized(std::string_view str, bool try_coloring)
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
	if (clean && cleared_at > created_at) {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			time_point_from_ullong(cleared_at) - time_point_from_ullong(created_at)
		).count();
	}
	return 0;
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


bool
Logging::finish(int wait)
{
	if (!scheduler().finish(wait)) {
		return false;
	}
	dump_collected();
	reset();
	return true;
}


void
Logging::set_mark()
{
	if (is_tty()) {
		auto buf = std::string("\033]1337;SetMark\a");
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}


void
Logging::tab_rgb(int red, int green, int blue)
{
	if (is_tty()) {
		std::string buf;
		buf += string::format("\033]6;1;bg;red;brightness;%d\a", red);
		buf += string::format("\033]6;1;bg;green;brightness;%d\a", green);
		buf += string::format("\033]6;1;bg;blue;brightness;%d\a", blue);
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}

void
Logging::tab_title(std::string_view title)
{
	if (is_tty()) {
		auto buf = string::format("\033]0;%s\a", title);
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}


void
Logging::badge(std::string_view badge)
{
	if (is_tty()) {
		auto buf = string::format("\033]1337;SetBadgeFormat=%s\a", Base64::rfc4648().encode(badge));
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}


void
Logging::growl(std::string_view text)
{
	if (is_tty()) {
		auto buf = string::format("\033]9;%s\a", text);
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}


void
Logging::reset()
{
	if (is_tty()) {
		std::string buf;
		buf += std::string("\033]1337;SetBadgeFormat=\a");
		buf += std::string("\033]6;1;bg;*;default\a");
		io::write(STDERR_FILENO, buf.data(), buf.size());
	}
}


void
Logging::run()
{
	L_DEBUG_HOOK("Logging::run", "Logging::run()");

	if (once) {
		static BloomFilter<> bloom;
		if (bloom.contains(str.data(), str.size())) {
			return;
		}
		bloom.add(str.data(), str.size());
	}

	std::string msg;

	if (info && priority <= LOG_DEBUG) {
		auto timestamp = Datetime::timestamp(time_point_from_ullong(created_at));

		if (opts.log_epoch) {
			auto epoch = static_cast<int>(timestamp);
			msg.append(std::string_view(rgb(94, 94, 94)));
			msg.append(string::format("%010d", epoch));
			if (opts.log_plainseconds) {
					// Use plain seconds only
			} else if (opts.log_milliseconds) {
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.append(string::format("%.3f", timestamp - epoch).erase(0, 1));
			} else if (opts.log_microseconds) {
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.append(string::format("%.6f", timestamp - epoch).erase(0, 1));
			}
			msg.push_back(' ');
		} else {
			auto tm = Datetime::to_tm_t(timestamp);
			if (opts.log_iso8601) {
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%04d", tm.year));
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.push_back('-');
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.mon));
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.push_back('-');
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.day));
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.push_back(' ');
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.hour));
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.push_back(':');
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.min));
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.push_back(':');
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.sec));
				if (opts.log_plainseconds) {
					// Use plain seconds only
				} else if (opts.log_milliseconds) {
					msg.append(std::string_view(rgb(60, 60, 60)));
					msg.append(string::format("%.3f", tm.fsec).erase(0, 1));
				} else if (opts.log_microseconds) {
					msg.append(std::string_view(rgb(60, 60, 60)));
					msg.append(string::format("%.6f", tm.fsec).erase(0, 1));
				}
				msg.push_back(' ');
			} else if (opts.log_timeless) {
				// No timestamp
			} else {
				msg.append(std::string_view(rgb(60, 60, 60)));
				msg.append(string::format("%04d", tm.year));
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.mon));
				msg.append(std::string_view(rgb(162, 162, 162)));
				msg.append(string::format("%02d", tm.day));
				msg.append(std::string_view(rgb(230, 230, 230)));
				msg.append(string::format("%02d", tm.hour));
				msg.append(std::string_view(rgb(162, 162, 162)));
				msg.append(string::format("%02d", tm.min));
				msg.append(std::string_view(rgb(94, 94, 94)));
				msg.append(string::format("%02d", tm.sec));
				if (opts.log_plainseconds) {
					// Use plain seconds only
				} else if (opts.log_milliseconds) {
					msg.append(std::string_view(rgb(60, 60, 60)));
					msg.append(string::format("%.3f", tm.fsec).erase(0, 1));
				} else if (opts.log_microseconds) {
					msg.append(std::string_view(rgb(60, 60, 60)));
					msg.append(string::format("%.6f", tm.fsec).erase(0, 1));
				}
				msg.push_back(' ');
			}
		}

		if (opts.log_threads) {
			msg.push_back('(');
			msg.append(get_thread_name(thread_id));
			msg.append(") ");
		}

#ifndef NDEBUG
		if (opts.log_location) {
			msg.append(filename);
			msg.push_back(':');
			msg.append(string::Number(line));
			msg.append(" at ");
			msg.append(function);
			msg.append(": ");
		}
#endif

		msg.append(CLEAR_COLOR.c_str(), CLEAR_COLOR.size());
	}

	if (stacked) {
		msg.append(STACKED_INDENT);
	}

	msg.append(str);

	if (async) {
		auto log_age = age();
		if (log_age > 2e8) {
			msg += " " + string::from_delta(log_age, "+", true);
		}
	}

	if (!exception.empty()) {
		msg.append(DEBUG_COL.c_str(), DEBUG_COL.size());
		msg.append(exception.get_traceback());
		msg.append(CLEAR_COLOR.c_str(), CLEAR_COLOR.size());
	}

	if (priority >= -LOG_ERR && priority <= LOG_ERR) {
		Logging::growl(Logging::colorized(str, false));
		Logging::set_mark();
	}

	log(priority, msg, stack_level * 2);
}


bool
Logging::vunlog(int _priority, const char* _function, const char* _filename, int _line, std::string_view format, fmt::printf_args args)
{
	if (!clear()) {
		if (_priority <= log_level) {
			add(_function, _filename, _line, fmt::vsprintf(format, args), nullptr, false, std::chrono::system_clock::now(), async, true, stacked, once, _priority, time_point_from_ullong(created_at));
			return true;
		}
	}
	return false;
}


void
Logging::do_println(bool collect, bool with_endl, std::string_view format, fmt::printf_args args)
{
	auto str = fmt::vsprintf(format, args);
	if (collect) {
		std::lock_guard<std::mutex> lk(collected_mtx);
		collected.emplace_back(std::move(str), with_endl);
	} else {
		log(0, str, 0, false, with_endl);
	}
}


Log
Logging::do_log(bool clean, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, bool once, int priority, const BaseException* exc, const char* function, const char* filename, int line, std::string_view format, fmt::printf_args args)
{
	if (priority <= log_level) {
		auto str = fmt::vsprintf(format, args);
		return add(function, filename, line, str, exc, clean, wakeup, async, info, stacked, once, priority);
	}
	return Log();
}


Log
Logging::add(const char* function, const char* filename, int line, const std::string& str, const BaseException* exc, bool clean, const std::chrono::time_point<std::chrono::system_clock>& wakeup, bool async, bool info, bool stacked, bool once, int priority, const std::chrono::time_point<std::chrono::system_clock>& created_at)
{
	auto l_ptr = std::make_shared<Logging>(function, filename, line, str, exc, clean, async, info, stacked, once, priority, created_at);

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
	auto needle = str.find(STACKED_INDENT);
	if (needle != std::string::npos) {
		str.replace(needle, sizeof(STACKED_INDENT) - 1, std::string(indent, ' '));
	}
	priority = validated_priority(priority);
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
