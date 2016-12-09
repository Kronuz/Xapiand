/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <ctype.h>      // for tolower, toupper
#include <dirent.h>     // for DIR
#include <math.h>       // for log10, floor, pow
#include <stdio.h>      // for size_t, snprintf
#include <sys/errno.h>  // for EAGAIN, ECONNRESET, EHOSTDOWN, EHOSTUNREACH
#include <sys/types.h>  // for uint64_t, uint16_t, uint8_t, int32_t, uint32_t
#include <unistd.h>     // for usleep
#include <chrono>       // for system_clock, time_point, duration_cast, seconds
#include <regex>        // for regex
#include <string>       // for string, allocator
#include <type_traits>  // for forward, underlying_type_t
#include <vector>       // for vector

#include "ev/ev++.h"    // for ::EV_ASYNC, ::EV_CHECK, ::EV_CHILD, ::EV_EMBED
#include "exception.h"  // for InvalidArgument, OutOfRange


#define RESERVED_FDS  50 /* Better approach? */


#define stox(func, s) \
	[](const std::string& str) { \
		std::size_t sz; \
		try { \
			auto ret = (func)(str, &sz); \
			if (sz != str.length()) { \
				THROW(InvalidArgument, "Cannot convert value: %s", str.c_str()); \
			} \
			return ret; \
		} catch (const std::out_of_range&) { \
			THROW(OutOfRange, "Out of range value: %s", str.c_str()); \
		} \
	}(s)


constexpr uint16_t SLOT_TIME_MINUTE = 1440;
constexpr uint8_t SLOT_TIME_SECOND = 60;


struct cont_time_t {
	uint32_t min[SLOT_TIME_MINUTE];
	uint32_t sec[SLOT_TIME_SECOND];
	uint64_t tm_min[SLOT_TIME_MINUTE];
	uint64_t tm_sec[SLOT_TIME_SECOND];
};


struct times_row_t {
	cont_time_t index;
	cont_time_t search;
	cont_time_t del;
	cont_time_t patch;
};


struct pos_time_t {
	uint16_t minute;
	uint8_t second;
};


struct File_ptr {
	struct dirent *ent;

	File_ptr()
		: ent(nullptr) { }
};

extern const std::regex numeric_re;

// Varibles used by server stats.
extern pos_time_t b_time;
extern std::chrono::time_point<std::chrono::system_clock> init_time;
extern times_row_t stats_cnt;

// It'll return the enum's underlying type.
template<typename E>
inline constexpr auto toUType(E enumerator) noexcept {
	return static_cast<std::underlying_type_t<E>>(enumerator);
}

template<typename T, std::size_t N>
inline constexpr std::size_t arraySize(T (&)[N]) noexcept {
	return N;
}

double random_real(double initial, double last);
uint64_t random_int(uint64_t initial, uint64_t last);

void set_thread_name(const std::string& name);
std::string get_thread_name();

std::string repr(const void* p, size_t size, bool friendly=true, bool quote=true, size_t max_size=0);
std::string repr(const std::string& string, bool friendly=true, bool quote=true, size_t max_size=0);


inline bool ignored_errorno(int e, bool tcp, bool udp) {
	switch(e) {
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			return true;  //  Ignore error
		case EINTR:
		case EPIPE:
		case EINPROGRESS:
			return tcp;  //  Ignore error

		case ENETDOWN:
		case EPROTO:
		case ENOPROTOOPT:
		case EHOSTDOWN:
#ifdef ENONET  // Linux-specific
		case ENONET:
#endif
		case EHOSTUNREACH:
		case EOPNOTSUPP:
		case ENETUNREACH:
		case ECONNRESET:
			return udp;  //  Ignore error on UDP sockets

		default:
			return false;  // Do not ignore error
	}
}


std::string name_generator();
int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets);


namespace std {
	template<typename T, int N>
	inline std::string to_string(const T (&s)[N])
	{
		return std::string(s, N - 1);
	}

	inline auto& to_string(std::string& str) {
		return str;
	}

	inline const auto& to_string(const std::string& str) {
		return str;
	}
}


template<typename... Args>
inline std::string format_string(const std::string& fmt, Args&&... args) {
	char buf[4096];
	snprintf(buf, sizeof(buf), fmt.c_str(), std::forward<Args>(args)...);
	return buf;
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter, const std::string& last_delimiter)
{
	std::string result;
	auto values_size = values.size();
	for (typename std::vector<T>::size_type idx = 0; idx < values_size; ++idx) {
		if (idx) {
			if (idx == values_size - 1) {
				result += last_delimiter;
			} else {
				result += delimiter;
			}
		}
		result += std::to_string(values[idx]);
	}
	return result;
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter) {
	return join_string(values, delimiter, delimiter);
}


inline std::string indent_string(const std::string& str, char sep, int level, bool indent_first=true) {
	std::string ret = str;

	std::string indentation(level, sep);
	if (indent_first) ret.insert(0, indentation);

	std::string::size_type pos = ret.find('\n');
	while (pos != std::string::npos) {
		ret.insert(pos + 1, indentation);
		pos = ret.find('\n', pos + level + 1);
	}

	return ret;
}


inline std::string center_string(const std::string& str, int width) {
	std::string result;
	for (auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2); idx > 0; --idx) {
		result += " ";
	}
	result += str;
	return result;
}

inline std::string right_string(const std::string& str, int width) {
	std::string result;
	for (auto idx = int(width - str.size()); idx > 0; --idx) {
		result += " ";
	}
	result += str;
	return result;
}

template<typename... Args>
inline std::string upper_string(Args&&... args) {
	std::string tmp(std::forward<Args>(args)...);
	for (auto& c : tmp) c = toupper(c);
	return tmp;
}

template<typename... Args>
inline std::string lower_string(Args&&... args) {
	std::string tmp(std::forward<Args>(args)...);
	for (auto& c : tmp) c = tolower(c);
	return tmp;
}

void to_upper(std::string& str);
void to_lower(std::string& str);

char* normalize_path(const char* src, const char* end, char* dst, bool slashed=false);
char* normalize_path(const std::string& src, char* dst, bool slashed=false);
std::string normalize_path(const std::string& src, bool slashed=false);
int url_qs(const char *, const char *, size_t);

// String tokenizer with the delimiter.
void stringTokenizer(const std::string& str, const std::string& delimiter, std::vector<std::string> &tokens);

bool strhasupper(const std::string& str);

bool isRange(const std::string& str);
bool isNumeric(const std::string& str);

bool startswith(const std::string& text, const std::string& token);
bool startswith(const std::string& text, char ch);
bool endswith(const std::string& text, const std::string& token);
bool endswith(const std::string& text, char ch);
void delete_files(const std::string& path);
void move_files(const std::string& src, const std::string& dst);
bool exists(const std::string& name);
bool build_path_index(const std::string& path);

void find_file_dir(DIR* dir, File_ptr& fptr, const std::string& pattern, bool pre_suf_fix);
DIR* opendir(const char* filename, bool create);
// Copy all directory if file_name and new_name are empty
int copy_file(const std::string& src, const std::string& dst, bool create=true, const std::string& file_name=std::string(), const std::string& new_name=std::string());

void update_pos_time();
void fill_zeros_stats_min(uint16_t start, uint16_t end);
void fill_zeros_stats_sec(uint8_t start, uint8_t end);
void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy);
void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy);

std::string bytes_string(size_t bytes, bool colored=false);
std::string delta_string(long double nanoseconds, bool colored=false);
std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored=false);

void _tcp_nopush(int sock, int optval);

void adjustOpenFilesLimit(size_t& max_clients);

inline void tcp_nopush(int sock) {
	_tcp_nopush(sock, 1);
}

inline void tcp_push(int sock) {
	_tcp_nopush(sock, 0);
}

namespace epoch {
	template<typename Period = std::chrono::seconds>
	inline auto now() noexcept {
		return std::chrono::duration_cast<Period>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}


struct Clk {
	unsigned long long mul;

	Clk() {
		auto a = std::chrono::system_clock::now();
		usleep(5000);
		auto b = std::chrono::system_clock::now();
		auto delta = *reinterpret_cast<unsigned long long*>(&b) - *reinterpret_cast<unsigned long long*>(&a);
		mul = 1000000 / static_cast<unsigned long long>(pow(10, floor(log10(delta))));
	}

	template <typename T>
	unsigned long long time_point_to_ullong(std::chrono::time_point<T> t) const {
		return *reinterpret_cast<unsigned long long*>(&t) * mul;
	}

	template <typename T=std::chrono::system_clock>
	std::chrono::time_point<T> time_point_from_ullong(unsigned long long t) const {
		t /= mul;
		return *reinterpret_cast<std::chrono::time_point<T>*>(&t);
	}

	static const Clk& clk() {
		static const Clk clk;
		return clk;
	}
};


template <typename T>
inline
unsigned long long
time_point_to_ullong(std::chrono::time_point<T> t) {
	return Clk::clk().time_point_to_ullong<T>(t);
}


template <typename T=std::chrono::system_clock>
inline
std::chrono::time_point<T>
time_point_from_ullong(unsigned long long t) {
	return Clk::clk().time_point_from_ullong<T>(t);
}


inline std::string readable_revents(int revents) {
	std::vector<std::string> values;
	if (revents == EV_NONE) values.push_back("EV_NONE");
	if ((revents & EV_READ) == EV_READ) values.push_back("EV_READ");
	if ((revents & EV_WRITE) == EV_WRITE) values.push_back("EV_WRITE");
	if ((revents & EV_TIMEOUT) == EV_TIMEOUT) values.push_back("EV_TIMEOUT");
	if ((revents & EV_TIMER) == EV_TIMER) values.push_back("EV_TIMER");
	if ((revents & EV_PERIODIC) == EV_PERIODIC) values.push_back("EV_PERIODIC");
	if ((revents & EV_SIGNAL) == EV_SIGNAL) values.push_back("EV_SIGNAL");
	if ((revents & EV_CHILD) == EV_CHILD) values.push_back("EV_CHILD");
	if ((revents & EV_STAT) == EV_STAT) values.push_back("EV_STAT");
	if ((revents & EV_IDLE) == EV_IDLE) values.push_back("EV_IDLE");
	if ((revents & EV_CHECK) == EV_CHECK) values.push_back("EV_CHECK");
	if ((revents & EV_PREPARE) == EV_PREPARE) values.push_back("EV_PREPARE");
	if ((revents & EV_FORK) == EV_FORK) values.push_back("EV_FORK");
	if ((revents & EV_ASYNC) == EV_ASYNC) values.push_back("EV_ASYNC");
	if ((revents & EV_EMBED) == EV_EMBED) values.push_back("EV_EMBED");
	if ((revents & EV_ERROR) == EV_ERROR) values.push_back("EV_ERROR");
	if ((revents & EV_UNDEF) == EV_UNDEF) values.push_back("EV_UNDEF");
	return join_string(values, " | ");
}
