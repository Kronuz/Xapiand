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

#pragma once

#include <atomic>             // for atomic_uulong
#include <chrono>             // for system_clock, time_point, duration, millise...
#include <cstdarg>            // for va_list, va_end
#include <stdlib.h>           // for getenv
#include <syslog.h>           // for LOG_DEBUG, LOG_WARNING, LOG_CRIT, LOG_ALERT
#include <unordered_map>      // for unordered_map

#include "exception.h"
#include "utils.h"
#include "xxh64.hpp"          // for xxh64


#define ASYNC_LOG_LEVEL LOG_ERR  // The minimum log_level that is asynchronous


extern std::atomic<uint64_t> logger_info_hook;


using namespace std::chrono_literals;


class Log;
using LogType = std::shared_ptr<Log>;


class LogWrapper {
	LogType log;

	LogWrapper(const LogWrapper&) = delete;
	LogWrapper& operator=(const LogWrapper&) = delete;

public:
	LogWrapper(LogWrapper&& o);
	LogWrapper& operator=(LogWrapper&& o);

	LogWrapper(LogType log_);
	~LogWrapper();

	bool unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);

	bool unlog(int priority, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...) {
		va_list argptr;
		va_start(argptr, format);
		auto ret = unlog(priority, file, line, suffix, prefix, obj, format, argptr);
		va_end(argptr);

		return ret;
	}

	bool clear();
	long double age();
	LogType release();
};


void println(bool with_endl, const char *format, va_list argptr, const void* obj=nullptr, bool info=false);
inline void print(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	println(true, format, argptr);
	va_end(argptr);
}


inline void println(bool with_endl, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	println(with_endl, format, argptr);
	va_end(argptr);
}


inline void log(const void* obj, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	println(true, format, argptr, obj, true);
	va_end(argptr);
}


LogWrapper _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const std::string& exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, va_list argptr);


template <typename T, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
inline LogWrapper _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const T* exc, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	auto ret = _log(cleanup, stacked, wakeup, async, priority, std::string(exc->get_traceback()), file, line, suffix, prefix, obj, format, argptr);
	va_end(argptr);
	return ret;
}


LogWrapper _log(bool cleanup, bool stacked, std::chrono::time_point<std::chrono::system_clock> wakeup, bool async, int priority, const void*, const char *file, int line, const char *suffix, const char *prefix, const void *obj, const char *format, ...);


template <typename T, typename R, typename... Args>
inline LogWrapper _log(bool cleanup, bool stacked, std::chrono::duration<T, R> timeout, bool async, int priority, Args&&... args) {
	return _log(cleanup, stacked, std::chrono::system_clock::now() + timeout, async, priority, std::forward<Args>(args)...);
}


template <typename... Args>
inline LogWrapper _log(bool cleanup, bool stacked, int timeout, bool async, int priority, Args&&... args) {
	return _log(cleanup, stacked, std::chrono::milliseconds(timeout), async, priority, std::forward<Args>(args)...);
}


#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(__unique, a)
#define UNIQUE_NAME LABEL_(__LINE__)


#define NO_COL "\033[0m"
#define BLACK "\033[0;30m"
#define GREY "\033[0;37m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define DARK_GREY "\033[1;30m"
#define LIGHT_RED "\033[1;31m"
#define LIGHT_GREEN "\033[1;32m"
#define LIGHT_YELLOW "\033[1;33m"
#define LIGHT_BLUE "\033[1;34m"
#define LIGHT_MAGENTA "\033[1;35m"
#define LIGHT_CYAN "\033[1;36m"
#define WHITE "\033[1;37m"


inline const char*
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


#define rgb(r, g, b)      ansi_color(r, g, b, 1, false)
#define rgba(r, g, b, a)  ansi_color(r, g, b, a, false)
#define brgb(r, g, b)     ansi_color(r, g, b, 1, true)
#define brgba(r, g, b, a) ansi_color(r, g, b, a, true)


#define LOG_COL WHITE
#define DEBUG_COL NO_COL
#define INFO_COL CYAN
#define NOTICE_COL LIGHT_CYAN
#define WARNING_COL LIGHT_YELLOW
#define ERR_COL RED
#define CRIT_COL LIGHT_RED
#define ALERT_COL LIGHT_RED
#define EMERG_COL LIGHT_RED


#define L_DELAYED(cleanup, delay, priority, color, args...) _log(cleanup, false, delay, true, priority, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_UNLOG(priority, color, args...) unlog(priority, __FILE__, __LINE__, NO_COL, color, args)
#define L_DELAYED_CLEAR() clear()

#define L_DELAYED_200(args...) auto __log_timed = L_DELAYED(true, 200ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_600(args...) auto __log_timed = L_DELAYED(true, 600ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_1000(args...) auto __log_timed = L_DELAYED(true, 1000ms, LOG_WARNING, LIGHT_MAGENTA, args)
#define L_DELAYED_N_UNLOG(args...) __log_timed.L_DELAYED_UNLOG(LOG_WARNING, MAGENTA, args)
#define L_DELAYED_N_CLEAR() __log_timed.L_DELAYED_CLEAR()

#define L_NOTHING(args...)

#define PRINT(stacked, level, color, args...) _log(false, stacked, 0ms, false, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)
#define LOG(stacked, level, color, args...) _log(false, stacked, 0ms, level >= ASYNC_LOG_LEVEL, level, nullptr, __FILE__, __LINE__, NO_COL, color, args)

#define L_INFO(args...) LOG(true, LOG_INFO, INFO_COL, args)
#define L_NOTICE(args...) LOG(true, LOG_NOTICE, NOTICE_COL, args)
#define L_WARNING(args...) LOG(true, LOG_WARNING, WARNING_COL, args)
#define L_ERR(args...) LOG(true, LOG_ERR, ERR_COL, args)
#define L_CRIT(args...) LOG(true, LOG_CRIT, CRIT_COL, args)
#define L_ALERT(args...) LOG(true, -LOG_ALERT, ALERT_COL, args)
#define L_EMERG(args...) LOG(true, -LOG_EMERG, EMERG_COL, args)
#define L_EXC(args...) _log(false, true, 0ms, true, -LOG_CRIT, &exc, __FILE__, __LINE__, NO_COL, ERR_COL, args)

#define L_UNINDENTED(level, color, args...) LOG(false, level, color, args)
#define L_UNINDENTED_LOG(args...) L_UNINDENTED(LOG_DEBUG, LOG_COL, args)
#define L_UNINDENTED_BLACK(args...) L_UNINDENTED(LOG_DEBUG, BLACK, args)
#define L_UNINDENTED_GREY(args...) L_UNINDENTED(LOG_DEBUG, GREY, args)
#define L_UNINDENTED_RED(args...) L_UNINDENTED(LOG_DEBUG, RED, args)
#define L_UNINDENTED_GREEN(args...) L_UNINDENTED(LOG_DEBUG, GREEN, args)
#define L_UNINDENTED_YELLOW(args...) L_UNINDENTED(LOG_DEBUG, YELLOW, args)
#define L_UNINDENTED_BLUE(args...) L_UNINDENTED(LOG_DEBUG, BLUE, args)
#define L_UNINDENTED_MAGENTA(args...) L_UNINDENTED(LOG_DEBUG, MAGENTA, args)
#define L_UNINDENTED_CYAN(args...) L_UNINDENTED(LOG_DEBUG, CYAN, args)
#define L_UNINDENTED_DARK_GREY(args...) L_UNINDENTED(LOG_DEBUG, DARK_GREY, args)
#define L_UNINDENTED_LIGHT_RED(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_RED, args)
#define L_UNINDENTED_LIGHT_GREEN(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_GREEN, args)
#define L_UNINDENTED_LIGHT_YELLOW(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_UNINDENTED_LIGHT_BLUE(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_BLUE, args)
#define L_UNINDENTED_LIGHT_MAGENTA(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_UNINDENTED_LIGHT_CYAN(args...) L_UNINDENTED(LOG_DEBUG, LIGHT_CYAN, args)
#define L_UNINDENTED_WHITE(args...) L_UNINDENTED(LOG_DEBUG, WHITE, args)

#define P(level, color, args...) PRINT(true, level, color, args)

#define L(level, color, args...) LOG(true, level, color, args)
#define L_LOG(args...) L(LOG_DEBUG, LOG_COL, args)
#define L_BLACK(args...) L(LOG_DEBUG, BLACK, args)
#define L_GREY(args...) L(LOG_DEBUG, GREY, args)
#define L_RED(args...) L(LOG_DEBUG, RED, args)
#define L_GREEN(args...) L(LOG_DEBUG, GREEN, args)
#define L_YELLOW(args...) L(LOG_DEBUG, YELLOW, args)
#define L_BLUE(args...) L(LOG_DEBUG, BLUE, args)
#define L_MAGENTA(args...) L(LOG_DEBUG, MAGENTA, args)
#define L_CYAN(args...) L(LOG_DEBUG, CYAN, args)
#define L_DARK_GREY(args...) L(LOG_DEBUG, DARK_GREY, args)
#define L_LIGHT_RED(args...) L(LOG_DEBUG, LIGHT_RED, args)
#define L_LIGHT_GREEN(args...) L(LOG_DEBUG, LIGHT_GREEN, args)
#define L_LIGHT_YELLOW(args...) L(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_LIGHT_BLUE(args...) L(LOG_DEBUG, LIGHT_BLUE, args)
#define L_LIGHT_MAGENTA(args...) L(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_LIGHT_CYAN(args...) L(LOG_DEBUG, LIGHT_CYAN, args)
#define L_WHITE(args...) L(LOG_DEBUG, WHITE, args)

#define L_STACKED(args...) auto UNIQUE_NAME = L(args)
#define L_STACKED_LOG(args...) L_STACKED(LOG_DEBUG, LOG_COL, args)
#define L_STACKED_BLACK(args...) L_STACKED(LOG_DEBUG, BLACK, args)
#define L_STACKED_GREY(args...) L_STACKED(LOG_DEBUG, GREY, args)
#define L_STACKED_RED(args...) L_STACKED(LOG_DEBUG, RED, args)
#define L_STACKED_GREEN(args...) L_STACKED(LOG_DEBUG, GREEN, args)
#define L_STACKED_YELLOW(args...) L_STACKED(LOG_DEBUG, YELLOW, args)
#define L_STACKED_BLUE(args...) L_STACKED(LOG_DEBUG, BLUE, args)
#define L_STACKED_MAGENTA(args...) L_STACKED(LOG_DEBUG, MAGENTA, args)
#define L_STACKED_CYAN(args...) L_STACKED(LOG_DEBUG, CYAN, args)
#define L_STACKED_DARK_GREY(args...) L_STACKED(LOG_DEBUG, DARK_GREY, args)
#define L_STACKED_LIGHT_RED(args...) L_STACKED(LOG_DEBUG, LIGHT_RED, args)
#define L_STACKED_LIGHT_GREEN(args...) L_STACKED(LOG_DEBUG, LIGHT_GREEN, args)
#define L_STACKED_LIGHT_YELLOW(args...) L_STACKED(LOG_DEBUG, LIGHT_YELLOW, args)
#define L_STACKED_LIGHT_BLUE(args...) L_STACKED(LOG_DEBUG, LIGHT_BLUE, args)
#define L_STACKED_LIGHT_MAGENTA(args...) L_STACKED(LOG_DEBUG, LIGHT_MAGENTA, args)
#define L_STACKED_LIGHT_CYAN(args...) L_STACKED(LOG_DEBUG, LIGHT_CYAN, args)
#define L_STACKED_WHITE(args...) L_STACKED(LOG_DEBUG, WHITE, args)

#ifdef NDEBUG
#define L_INFO_HOOK L_NOTHING
#else
#define L_INFO_HOOK(hook, args...) if ((logger_info_hook.load() & xxh64::hash(hook)) == xxh64::hash(hook)) { P(args); }
#endif
#define L_INFO_HOOK_LOG(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LOG_COL, args)
#define L_INFO_HOOK_BLACK(hook, args...) L_INFO_HOOK(hook, LOG_INFO, BLACK, args)
#define L_INFO_HOOK_GREY(hook, args...) L_INFO_HOOK(hook, LOG_INFO, GREY, args)
#define L_INFO_HOOK_RED(hook, args...) L_INFO_HOOK(hook, LOG_INFO, RED, args)
#define L_INFO_HOOK_GREEN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, GREEN, args)
#define L_INFO_HOOK_YELLOW(hook, args...) L_INFO_HOOK(hook, LOG_INFO, YELLOW, args)
#define L_INFO_HOOK_BLUE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, BLUE, args)
#define L_INFO_HOOK_MAGENTA(hook, args...) L_INFO_HOOK(hook, LOG_INFO, MAGENTA, args)
#define L_INFO_HOOK_CYAN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, CYAN, args)
#define L_INFO_HOOK_DARK_GREY(hook, args...) L_INFO_HOOK(hook, LOG_INFO, DARK_GREY, args)
#define L_INFO_HOOK_LIGHT_RED(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_RED, args)
#define L_INFO_HOOK_LIGHT_GREEN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_GREEN, args)
#define L_INFO_HOOK_LIGHT_YELLOW(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_YELLOW, args)
#define L_INFO_HOOK_LIGHT_BLUE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_BLUE, args)
#define L_INFO_HOOK_LIGHT_MAGENTA(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_MAGENTA, args)
#define L_INFO_HOOK_LIGHT_CYAN(hook, args...) L_INFO_HOOK(hook, LOG_INFO, LIGHT_CYAN, args)
#define L_INFO_HOOK_WHITE(hook, args...) L_INFO_HOOK(hook, LOG_INFO, WHITE, args)

#define L_TEST L_NOTHING

#ifdef NDEBUG
#define L_DEBUG L_NOTHING
#else
#define L_DEBUG(args...) L(LOG_DEBUG, DEBUG_COL, args)
#endif
