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

#pragma once

#include "xapiand.h"
#include "md5.h"
#include "wkt_parser.h"
#include "datetime.h"
#include "cJSON.h"
#include "multivalue.h"
#include "geospatialrange.h"
#include "htm.h"
#include <limits.h>

#include <xapian.h>
#include <string>
#include <vector>
#include <locale>
#include <algorithm>
#include <memory>
#include <sstream>
#include <random>
#include <sys/time.h>
#include <chrono>
#include <regex>
#include <thread>


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
};

struct pos_time_t {
	uint16_t minute;
	uint8_t second;
};

struct parser_query_t {
	size_t length;
	const char *offset;
};

struct parser_url_path_t {
	const char *offset;
	size_t len_path;
	const char *off_path;
	size_t len_host;
	const char *off_host;
	size_t len_namespace;
	const char *off_namespace;
	size_t len_command;
	const char *off_command;
	size_t len_upload;
	const char *off_upload;
};

extern std::regex numeric_re;
extern std::regex find_range_re;

extern std::mutex log_mutex;

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

void set_thread_name(const std::string &name);
std::string get_thread_name();

std::string repr(const void *p, size_t size, bool friendly=true);
std::string repr(const std::string &string, bool friendly=true);


class Log {
	std::string log_start;
	std::atomic_bool log_runner;
	std::thread log_thread;
	std::chrono::time_point<std::chrono::system_clock> log_tp_end;

	std::string log(const char *file, int line, void *, const char *format, va_list ap);

public:
	Log(const char *file, int line, int timeout, void *obj, const char *format, ...);
	Log(const char *file, int line, void *obj, const char *fmt, ...);
	~Log();

	void end(const char *file, int line, void *obj, const char *format, ...);
};


inline bool ignored_errorno(int e, bool udp) {
	switch(e) {
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
		case EPIPE:
		case EINPROGRESS:
			return true;  //  Ignore error

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
		case EINTR:
		case ECONNRESET:
			return udp;  //  Ignore error on UDP sockets

		default:
			return false;  // Do not ignore error
	}
}


std::string name_generator();
int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets);

struct char_ptr_deleter {
	void operator()(char *c) const {
		free(c);
	}
};

struct TRANSFORM_UPPER {
	char operator() (char c) { return  toupper(c);}
};

struct TRANSFORM_LOWER {
	char operator() (char c) { return  tolower(c);}
};

// Mapped [0-9] -> [A-J] and  [A-F] -> [R-W]
struct TRANSFORM_MAP {
	char operator() (char c) { return  c + 17;}
};

typedef std::unique_ptr<cJSON, void(*)(cJSON*)> unique_cJSON;
typedef std::unique_ptr<char, char_ptr_deleter> unique_char_ptr;

int url_path(const char* n1, size_t size, parser_url_path_t *par);
int url_qs(const char *, const char *, size_t, parser_query_t *);
std::string urldecode(const char *, size_t);
std::string stringtolower(const std::string &str);
std::string stringtoupper(const std::string &str);
// String tokenizer with the delimiter.
void stringTokenizer(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens);

long strtol(const std::string &str, int base=10);
unsigned long strtoul(const std::string &str, int base=10);
long long strtoll(const std::string &str, int base=10);
unsigned long long strtoull(const std::string &str, int base=10);
double strtod(const std::string &str);

unsigned int get_slot(const std::string &name);
std::string prefixed(const std::string &term, const std::string &prefixO);
std::string get_prefix(const std::string &name, const std::string &prefix, char type);
std::string get_slot_hex(const std::string &name);
bool strhasupper(const std::string &str);
bool isRange(const std::string &str);
bool isNumeric(const std::string &str);
bool startswith(const std::string &text, const std::string &token);
void delete_files(const std::string &path);
void move_files(const std::string &src, const std::string &dst);
inline bool exist(const std::string& name);
bool buid_path_index(const std::string& path);

void update_pos_time();
void fill_zeros_stats_min(uint16_t start, uint16_t end);
void fill_zeros_stats_sec(uint8_t start, uint8_t end);
void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t> &cnt, std::vector<double> &tm_cnt, times_row_t &stats_cnt_cpy);
void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t> &cnt, std::vector<double> &tm_cnt, times_row_t &stats_cnt_cpy);

// Levenshtein distance is a string metric for measuring the difference between two
// sequences (known as edit distance).
unsigned int levenshtein_distance(const std::string &str1, const std::string &str2);

namespace epoch {
	auto now = []() noexcept { return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); };
}

#define _(...)
#define _LOG_ENABLED(...) Log(__FILE__, __LINE__, __VA_ARGS__)
#define _LOG_TIMED_100(...) Log __timed(__FILE__, __LINE__, 100, __VA_ARGS__)
#define _LOG_TIMED_500(...) Log __timed(__FILE__, __LINE__, 500, __VA_ARGS__)
#define _LOG_TIMED_1000(...) Log __timed(__FILE__, __LINE__, 1000, __VA_ARGS__)
#define _LOG_TIMED_CLEAR(...) __timed.end(__FILE__, __LINE__, __VA_ARGS__)


#define INFO _LOG_ENABLED

#define LOG _LOG_ENABLED

#define LOG_ERR _LOG_ENABLED

#define LOG_DEBUG _

#define LOG_CONN _LOG_ENABLED
#define LOG_DISCOVERY _LOG_ENABLED
#define LOG_RAFT _LOG_ENABLED
#define LOG_OBJ _LOG_ENABLED
#define LOG_OBJ_BEGIN _LOG_TIMED_100
#define LOG_OBJ_END _LOG_TIMED_CLEAR
#define LOG_DATABASE _
#define LOG_DATABASE_BEGIN _LOG_TIMED_100
#define LOG_DATABASE_END _LOG_TIMED_CLEAR
#define LOG_HTTP _LOG_ENABLED
#define LOG_BINARY _LOG_ENABLED
#define LOG_HTTP_PROTO_PARSER _

#define LOG_EV _LOG_ENABLED
#define LOG_EV_BEGIN _LOG_TIMED_500
#define LOG_EV_END _LOG_TIMED_CLEAR
#define LOG_CONN_WIRE _LOG_ENABLED
#define LOG_UDP_WIRE _
#define LOG_HTTP_PROTO _
#define LOG_BINARY_PROTO _

#define LOG_DATABASE_WRAP _LOG_ENABLED
