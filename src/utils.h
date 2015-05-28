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

#include <xapian.h>

#include <string>
#include <vector>
#include <locale>
#include <algorithm>

#include <sstream>
#include <pcre.h>
#include <sys/time.h>

#define NUMERIC_TYPE 'n'
#define STRING_TYPE 's'
#define DATE_TYPE 'd'
#define GEO_TYPE 'g'
#define BOOLEAN_TYPE 'b'
#define ARRAY_TYPE 'a'
#define OBJECT_TYPE 'o'
#define NO_TYPE ' '

#define CMD_NUMBER 0
#define CMD_SEARCH 1
#define CMD_FACETS 2
#define CMD_STATS 3
#define CMD_SCHEMA 4
#define CMD_UNKNOWN_HOST 5
#define CMD_UNKNOWN_ENDPOINT 6
#define CMD_UNKNOWN -1
#define CMD_BAD_QUERY -2
#define CMD_BAD_ENDPS -3

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

extern pcre *compiled_date_re;
extern pcre *compiled_date_math_re;
extern pcre *compiled_coords_re;
extern pcre *compiled_coords_dist_re;
extern pcre *compiled_numeric_re;
extern pcre *compiled_find_range_re;
extern pos_time_t b_time;
extern time_t init_time;
extern times_row_t stats_cnt;

typedef struct similar_s {
	int n_rset;
	int n_eset;
	std::vector <std::string> field;
	std::vector <std::string> type;
} similar_t;

typedef struct query_s {
	int offset;
	int limit;
	int check_at_least;
	bool spelling;
	bool synonyms;
	bool pretty;
	bool commit;
	bool server;
	bool database;
	int document;
	bool unique_doc;
	bool is_fuzzy;
	bool is_nearest;
	std::string stats;
	std::vector <std::string> language;
	std::vector <std::string> query;
	std::vector <std::string> partial;
	std::vector <std::string> terms;
	std::vector <std::string> order;
	std::vector <std::string> facets;
	similar_t fuzzy;
	similar_t nearest;
} query_t;

typedef struct search_s {
	Xapian::Query query;
	std::vector<std::string> suggested_query;
} search_t;

typedef struct data_field_s {
	unsigned int slot;
	std::string prefix;
	char type;
} data_field_t;

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

int url_path(const char* n1, size_t size, parser_url_path_t *par);
int url_qs(const char *, const char *, size_t, parser_query_t *);
std::string urldecode(const char *, size_t);
int look_cmd(const char *);

std::string serialise_numeric(const std::string &field_value);
std::string serialise_date(const std::string &field_value);
std::string unserialise_date(const std::string &serialise_val);
std::string serialise_geo(const std::string &field_value);
std::string unserialise_geo(const std::string &serialise_val);
std::string serialise_bool(const std::string &field_value);
bool lat_lon(const std::string &str, int *grv, int size, int offset);
std::string stringtolower(const std::string &str);
std::string stringtoupper(const std::string &str);
unsigned int get_slot(const std::string &name);
std::string prefixed(const std::string &term, const std::string &prefixO);
unsigned int hex2int(const std::string &input);
int strtoint(const std::string &str);
double strtodouble(const std::string &str);
std::string timestamp_date(const std::string &str);
std::string get_prefix(const std::string &name, const std::string &prefix, char type);
std::string get_slot_hex(const std::string &name);
bool strhasupper(const std::string &str);
int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, group_t **groups);
int get_coords(const std::string &str, double *coords);
bool isRange(const std::string &str);
bool isLatLongDistance(const std::string &str);
void get_order(const std::string &str, query_t &e);
bool isNumeric(const std::string &str);
bool StartsWith(const std::string &text, const std::string &token);
int number_days(int year, int month);
bool validate_date(int n[]);
void calculate_date(int n[], const std::string &op, const std::string &units);
std::string unserialise(char field_type, const std::string &field_name, const std::string &serialise_val);
std::string serialise(char field_type, const std::string &field_value);

int identify_cmd(std::string &commad);
bool is_digits(const std::string &str);
int get_minutes(std::string &hour, std::string &minute);
bool Is_id_range(std::string &ids);
std::string to_type(std::string type);

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