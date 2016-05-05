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

#include <limits.h>
#include <xapian.h>
#include <string>
#include <vector>
#include <locale>
#include <algorithm>
#include <dirent.h>
#include <memory>
#include <sstream>
#include <random>
#include <sys/time.h>
#include <chrono>
#include <regex>
#include <thread>
#include <unistd.h>


#define CMD_NO_CMD 0
#define CMD_SEARCH 1
#define CMD_FACETS 2
#define CMD_STATS  3
#define CMD_SCHEMA 4
#define CMD_UNKNOWN   -1
#define CMD_BAD_QUERY -2

#define strict(func, s) \
	[](const std::string& str) -> decltype(auto) { \
		std::size_t sz; \
		auto ret = (func)(str, &sz); \
		if (sz != str.size()) { \
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


struct QueryParser {
	std::string query;

public:
	size_t len;
	const char *off;

	QueryParser();

	void clear();
	void rewind();
	int init(const std::string& q);
	int next(const char *name);

	std::string get();
};


class PathParser {
	std::string path;
	const char *off;

public:
	enum State {
		start, pmt, cmd, id, nsp, pth, hst, end,
		INVALID_STATE,
		INVALID_NSP,
		INVALID_HST,
	};

	size_t len_pth;
	const char *off_pth;
	size_t len_hst;
	const char *off_hst;
	size_t len_nsp;
	const char *off_nsp;
	size_t len_pmt;
	const char *off_pmt;
	size_t len_cmd;
	const char *off_cmd;
	size_t len_id;
	const char *off_id;

	PathParser();

	void clear();
	void rewind();
	State init(const std::string& p);
	State next();

	std::string get_pth();
	std::string get_hst();
	std::string get_nsp();
	std::string get_pmt();
	std::string get_cmd();
	std::string get_id();
};


struct File_ptr {
	struct dirent *ent;

	File_ptr()
		: ent(nullptr) { }
};

extern const std::regex numeric_re;
extern const std::regex find_range_re;

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


inline bool ignored_errorno(int e, bool udp) {
	switch(e) {
		case EINTR:
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
		case ECONNRESET:
			return udp;  //  Ignore error on UDP sockets

		default:
			return false;  // Do not ignore error
	}
}


std::string name_generator();
int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets);

template<typename... Args>
std::string upper_string(Args&&... args) {
	std::string tmp(std::forward<Args>(args)...);
	for (auto& c : tmp) c = toupper(c);
	return tmp;
}

template<typename... Args>
std::string lower_string(Args&&... args) {
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
std::string urldecode(const char *, size_t);

// String tokenizer with the delimiter.
void stringTokenizer(const std::string& str, const std::string& delimiter, std::vector<std::string> &tokens);

unsigned get_slot(const std::string& name);
std::string prefixed(const std::string& term, const std::string& prefixO);
std::string get_prefix(const std::string& name, const std::string& prefix, char type);
std::string get_slot_hex(const std::string& name);
bool strhasupper(const std::string& str);

bool isRange(const std::string& str);
bool isNumeric(const std::string& str);

bool startswith(const std::string& text, const std::string& token);
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

// Levenshtein distance is a string metric for measuring the difference between two
// sequences (known as edit distance).
unsigned int levenshtein_distance(const std::string& str1, const std::string& str2);
/* convert the num to a string in a given numeric base */
std::string baseN(unsigned long num, int base, std::string symbols= std::string("0123456789abcdefghijklmnopqrstuvwxyz"));
/* strip a string from the left size */
std::string lstrip(const std::string& str, char c);

std::string delta_string(long double delta, bool colored=false);
std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored=false);

void _tcp_nopush(int sock, int optval);

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
