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

#ifndef XAPIAND_INCLUDED_UTILS_H
#define XAPIAND_INCLUDED_UTILS_H

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
#include <pcre.h>
#include <sys/time.h>

#define SLOT_TIME_MINUTE 1440
#define SLOT_TIME_SECOND 60


void log(const char *file, int line, void *obj, const char *fmt, ...);

std::string repr(const char *p, size_t size, bool friendly=true);
std::string repr(const std::string &string, bool friendly=true);


inline bool ignored_errorno(int e, bool udp) {
	switch(e) {
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
		case EPIPE:
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

int bind_tcp(const char *type, int &port, struct sockaddr_in &addr, int tries);
int bind_udp(const char *type, int &port, struct sockaddr_in &addr, int tries, const char *group);
int connect_tcp(const char *hostname, const char *servname);
int accept_tcp(int listener_sock);

std::string name_generator();
int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets);

typedef struct cont_time_s {
	unsigned short cnt[SLOT_TIME_MINUTE];
	unsigned short sec[SLOT_TIME_SECOND];
	double tm_cnt[SLOT_TIME_MINUTE];
	double tm_sec[SLOT_TIME_SECOND];
} cont_time_t;

typedef struct times_row_s {
	cont_time_t index;
	cont_time_t search;
	cont_time_t del;
} times_row_t;

typedef struct pos_time_s {
	unsigned short minute;
	unsigned short second;
} pos_time_t;

extern pcre *compiled_coords_re;
extern pcre *compiled_numeric_re;
extern pcre *compiled_find_range_re;

// Varibles used by server stats.
extern pos_time_t b_time;
extern time_t init_time;
extern times_row_t stats_cnt;

typedef struct similar_s {
	unsigned int n_rset;
	unsigned int n_eset;
	unsigned int n_term; //If the number of subqueries is less than this threshold, OP_ELITE_SET behaves identically to OP_OR
	std::vector <std::string> field;
	std::vector <std::string> type;
} similar_t;

typedef struct query_s {
	unsigned int offset;
	unsigned int limit;
	unsigned int check_at_least;
	bool spelling;
	bool synonyms;
	bool pretty;
	bool commit;
	bool server;
	bool database;
	std::string document;
	bool unique_doc;
	bool is_fuzzy;
	bool is_nearest;
	std::string stats;
	std::string collapse;
	unsigned int collapse_max;
	std::vector <std::string> language;
	std::vector <std::string> query;
	std::vector <std::string> partial;
	std::vector <std::string> terms;
	std::vector <std::string> sort;
	std::vector <std::string> facets;
	similar_t fuzzy;
	similar_t nearest;
} query_t;

typedef struct parser_query_s {
	size_t length;
	const char *offset;
} parser_query_t;

typedef struct parser_url_path_s {
	const char *offset;
	size_t len_path;
	const char *off_path;
	size_t len_host;
	const char *off_host;
	size_t len_namespace;
	const char *off_namespace;
	size_t len_command;
	const char *off_command;
} parser_url_path_t;

typedef struct group_s {
	int start;
	int end;
} group_t;

struct group_t_deleter {
	void operator()(group_t *p) const {
		free(p);
	}
};

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
typedef std::unique_ptr<group_t, group_t_deleter> unique_group;
typedef std::unique_ptr<char, char_ptr_deleter> unique_char_ptr;

int url_path(const char* n1, size_t size, parser_url_path_t *par);
int url_qs(const char *, const char *, size_t, parser_query_t *);
std::string urldecode(const char *, size_t);
std::string stringtolower(const std::string &str);
std::string stringtoupper(const std::string &str);
void stringTokenizer(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens);
int strtoint(const std::string &str);
unsigned int strtouint(const std::string &str);
long long int strtollong(const std::string &str);
uInt64 strtouInt64(const std::string &str);
double strtodouble(const std::string &str);
unsigned int get_slot(const std::string &name);
std::string prefixed(const std::string &term, const std::string &prefixO);
unsigned int hex2int(const std::string &input);
std::string get_prefix(const std::string &name, const std::string &prefix, char type);
std::string get_slot_hex(const std::string &name);
bool strhasupper(const std::string &str);
bool isRange(const std::string &str, unique_group &unique_gr);
bool isRange(const std::string &str);
bool isNumeric(const std::string &str);
bool startswith(const std::string &text, const std::string &token);
void delete_files(const std::string &path);
void move_files(const std::string &src, const std::string &dst);
int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, unique_group &unique_groups);
void update_pos_time();
void fill_zeros_stats_cnt(int start, int end);
void fill_zeros_stats_sec(int start, int end);

#define INFO(...) log(__FILE__, __LINE__, __VA_ARGS__)

#define LOG(...) log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERR(...) log(__FILE__, __LINE__, __VA_ARGS__)

#define LOG_DEBUG(...)

#define LOG_CONN(...)
#define LOG_DISCOVERY(...) log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_OBJ(...)
#define LOG_DATABASE(...)
#define LOG_HTTP(...)
#define LOG_BINARY(...)
#define LOG_HTTP_PROTO_PARSER(...)

#define LOG_EV(...)
#define LOG_CONN_WIRE(...)
#define LOG_DISCOVERY_WIRE(...)
#define LOG_HTTP_PROTO(...)
#define LOG_BINARY_PROTO(...)

#define LOG_DATABASE_WRAP(...) log(__FILE__, __LINE__, __VA_ARGS__)

#endif /* XAPIAND_INCLUDED_UTILS_H */