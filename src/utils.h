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

#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <limits.h>
#include <locale>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#define RESERVED_FDS  50 /* Better approach? */

#define CMD_NO_CMD     0
#define CMD_SEARCH     1
#define CMD_FACETS     2
#define CMD_INFO       3
#define CMD_SCHEMA     4
#define CMD_UNKNOWN   -1
#define CMD_BAD_QUERY -2


#define strict(func, s) \
	[](const std::string& str) { \
		std::size_t sz; \
		auto ret = (func)(str, &sz); \
		if (sz != str.length()) { \
			throw std::invalid_argument("Cannot convert value: " + str); \
		} \
		return ret; \
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

std::string repr(const void* p, size_t size, bool friendly=true, size_t max_size=0);
std::string repr(const std::string& string, bool friendly=true, size_t max_size=0);


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
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter) {
	return join_string(values, delimiter, delimiter);
}


inline std::string indent_string(const std::string& str, char sep, int level) {
	std::string ret = str;

	std::string indentation(level, sep);
	ret.insert(0, indentation);

	std::string::size_type pos = ret.find('\n');
	while (pos != std::string::npos) {
		ret.insert(pos + 1, indentation);
		pos = ret.find('\n', pos + level + 1);
	}

	return ret;
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


inline std::string center_string(const std::string& str, int width) {
	std::string result;
	for (auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2); idx > 0; --idx) {
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

char* normalize_path(const char* src, const char* end, char* dst);
char* normalize_path(const std::string& src, char* dst);
std::string normalize_path(const std::string& src);
int url_qs(const char *, const char *, size_t);

// String tokenizer with the delimiter.
void stringTokenizer(const std::string& str, const std::string& delimiter, std::vector<std::string> &tokens);

bool strhasupper(const std::string& str);

bool isRange(const std::string& str);
bool isNumeric(const std::string& str);

bool startswith(const std::string& text, const std::string& token);
bool endswith(const std::string& text, const std::string& token);
void delete_files(const std::string& path);
void move_files(const std::string& src, const std::string& dst);
bool exist(const std::string& name);
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
