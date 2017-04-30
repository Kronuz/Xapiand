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

#include <ctime>         // for time_t
#include <functional>    // for ref
#include <iostream>      // for cerr
#include <regex>         // for regex_replace, regex
#include <stdarg.h>      // for va_list, va_end, va_start
#include <stdexcept>     // for out_of_range
#include <stdio.h>       // for fileno, vsnprintf, stderr
#include <system_error>  // for system_error
#include <unistd.h>      // for isatty

#include "datetime.h"    // for to_string
#include "exception.h"   // for traceback
#include "utils.h"       // for get_thread_name

#define BUFFER_SIZE (500 * 1024)
#define STACKED_INDENT "<indent>"


std::atomic<uint64_t> logger_info_hook;

const std::regex filter_re("\033\\[[;\\d]*m", std::regex::optimize);
std::mutex Logging::stack_mtx;
std::unordered_map<std::thread::id, unsigned> Logging::stack_levels;
int Logging::log_level = DEFAULT_LOG_LEVEL;
std::vector<std::unique_ptr<Logger>> Logging::handlers;


const char*
ansi_color(float red, float green, float blue, float alpha, bool bold)
{
	static enum class Coloring : uint8_t {
		Unknown,
		TrueColor,
		Palette,
		Standard256,
		Standard16,
		None,
	} coloring = Coloring::Unknown;
	static std::unordered_map<size_t, const std::string> colors;
	uint8_t r = static_cast<uint8_t>(red * alpha + 0.5);
	uint8_t g = static_cast<uint8_t>(green * alpha + 0.5);
	uint8_t b = static_cast<uint8_t>(blue * alpha + 0.5);
	size_t hash = r << 17 | g << 9 | b << 1 | bold;
	auto it = colors.find(hash);
	if (it == colors.end()) {
		if (coloring == Coloring::Unknown) {
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
				coloring = Coloring::TrueColor;
			} else if (term.find("256color") != std::string::npos) {
				coloring = Coloring::Standard256;
			} else if (term.find("ansi") != std::string::npos || term.find("16color") != std::string::npos) {
				coloring = Coloring::Standard16;
			} else {
				coloring = Coloring::Standard16;
			}
		}
		char buffer[30] = "";
		switch (coloring) {
			case Coloring::TrueColor:
				sprintf(buffer, "\033[%d;38;2;%d;%d;%dm", bold, r, g, b);
				break;
			case Coloring::Palette:
			case Coloring::Standard256: {
				uint8_t color;
				if (r == g == b) {
					if (r < 8) {
						color = 16;
					} else if (r > 248) {
						color = 231;
					} else {
						color = 232 + (((r - 8) / 247) * 24);
					}
				} else {
					r = r / 255.0 * 5.0 + 0.5;
					g = g / 255.0 * 5.0 + 0.5;
					b = b / 255.0 * 5.0 + 0.5;
					color = 16 + 36 * r + 6 * g + b;
				}
				sprintf(buffer, "\033[%d;38;5;%dm", bold, color);
				break;
			}
			case Coloring::Standard16: {
				uint8_t max = std::max({r, g, b});
				uint8_t min = std::min({r, g, b});
				uint8_t color;
				if (max < 32) {
					color = 0;
				} else {
					r = static_cast<uint8_t>((r - min) * 255.0 / (max - min) + 0.5) > 128 ? 1 : 0;
					g = static_cast<uint8_t>((g - min) * 255.0 / (max - min) + 0.5) > 128 ? 1 : 0;
					b = static_cast<uint8_t>((b - min) * 255.0 / (max - min) + 0.5) > 128 ? 1 : 0;
					color = (b << 2) | (g << 1) | r;
					if (max > 192) {
						color += 8;
					}
				}
				sprintf(buffer, "\033[%d;38;5;%dm", bold, color);
				break;
			}
			case Coloring::None:
			default:
				break;
		}
		it = colors.insert(std::make_pair(hash, buffer)).first;
	}
	return it->second.c_str();
}


static constexpr const char * const priorities[] = {
	EMERG_COL "█" NO_COL,   // LOG_EMERG    0 = System is unusable
	ALERT_COL "▉" NO_COL,   // LOG_ALERT    1 = Action must be taken immediately
	CRIT_COL "▊" NO_COL,    // LOG_CRIT     2 = Critical conditions
	ERR_COL "▋" NO_COL,     // LOG_ERR      3 = Error conditions
	WARNING_COL "▌" NO_COL, // LOG_WARNING  4 = Warning conditions
	NOTICE_COL "▍" NO_COL,  // LOG_NOTICE   5 = Normal but significant condition
	INFO_COL "▎" NO_COL,    // LOG_INFO     6 = Informational
	DEBUG_COL "▏" NO_COL,   // LOG_DEBUG    7 = Debug-level messages
	NO_COL,                 // VERBOSE    > 7 = Verbose messages
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
println(bool with_endl, const char *format, va_list argptr, const void* obj, bool info)
{
	std::string str(Logging::str_format(false, LOG_DEBUG, "", "", 0, "", "", obj, format, argptr, info));
	Logging::log(LOG_DEBUG, str, 0, info, with_endl);
}


Log
log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = log(cleanup, stacked, wakeup, async, priority, std::string(), file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);
	return ret;
}


Log
log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	return Logging::log(cleanup, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, obj, format, argptr);
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
Log::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	return log->unlog(priority, file, line, suffix, prefix, obj, format, argptr);
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
	ofs << std::regex_replace((with_priority ? priorities[validated_priority(priority)] : "") + str, filter_re, "");
	if (with_endl) {
		ofs << std::endl;
	}
}


void
StderrLogger::log(int priority, const std::string& str, bool with_priority, bool with_endl)
{
	if (isatty(fileno(stderr))) {
		std::cerr << (with_priority ? priorities[validated_priority(priority)] : "") + str;
	} else {
		std::cerr << std::regex_replace((with_priority ? priorities[validated_priority(priority)] : "") + str, filter_re, "");
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
	syslog(priority, "%s", std::regex_replace((with_priority ? priorities[validated_priority(priority)] : "") + str, filter_re, "").c_str());
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
	L_INFO_HOOK_LOG("Logging::run", this, "Logging::run()");

	auto msg = str_start;
	auto log_age = age();
	if (log_age > 2e8) {
		msg += " ~" + delta_string(log_age, true);
	}
	Logging::log(priority, msg, stack_level * 2);
}


std::string
Logging::str_format(bool stacked, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void* obj, const char *format, va_list argptr, bool info)
{
	char* buffer = new char[BUFFER_SIZE];
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	std::string msg(buffer);
	std::string result;
	if (info && validated_priority(priority) <= LOG_DEBUG) {
		auto iso8601 = "[" + Datetime::to_string(std::chrono::system_clock::now()) + "]";
		auto tid = " (" + get_thread_name() + ")";
		result = iso8601 + tid;
#ifdef LOG_OBJ_ADDRESS
		if (obj) {
			snprintf(buffer, BUFFER_SIZE, " [%p]", obj);
			result += buffer;
		}
#else
		(void)obj;
#endif
		result += " ";

#ifdef LOG_LOCATION
		result += std::string(file) + ":" + std::to_string(line) + ": ";
#endif
	}

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


Log
Logging::log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	if (priority > log_level) {
		return Log(std::make_shared<Logging>("", clean, stacked, async, priority, std::chrono::system_clock::now()));
	}

	std::string str(str_format(stacked, priority, exc, file, line, suffix, prefix, obj, format, argptr, true)); // TODO: Slow!

	return print(str, clean, stacked, wakeup, async, priority);
}


Log
Logging::log(bool clean, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = log(clean, stacked, wakeup, async, priority, exc, file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);

	return ret;
}


bool
Logging::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr)
{
	if (!clear()) {
		if (priority > log_level) {
			return false;
		}

		std::string str(str_format(stacked, priority, std::string(), file, line, suffix, prefix, obj, format, argptr, true));

		print(str, false, stacked, 0, async, priority, time_point_from_ullong(created_at));

		return true;
	}
	return false;
}


bool
Logging::unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	auto ret = unlog(priority, file, line, suffix, prefix, obj, format, argptr);
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
