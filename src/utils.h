/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include <xapian.h>
#include <string>
#include <vector>
#include <locale>
#include <algorithm>

#include "md5.h"
#include <sstream>
#include <pcre.h>
#include <sys/time.h>

#define NUMERIC_TYPE 'n'
#define STRING_TYPE 's'
#define DATE_TYPE 'd'
#define GEO_TYPE 'g'
#define BOOLEAN_TYPE 'b'
#define TEXT_TYPE 't'

#define CMD_NUMBER 0
#define CMD_SEARCH 1
#define CMD_FACETS 2
#define CMD_STATS 3

static time_t init_time;

void log(void *obj, const char *fmt, ...);

std::string repr(const char *p, size_t size);
std::string repr(const std::string &string);

typedef struct times_row_s {
	unsigned short ind[1440];
	unsigned short ind_sec[60];
	unsigned short src[1440];
	unsigned short src_sec[60];
	unsigned short del[1440];
	unsigned short del_sec[60];
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

typedef struct parser_query_s {
	size_t length;
	const char *offset;
} parser_query_t;


typedef struct parser_url_path_s {
	size_t length;
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
std::string get_prefix(const std::string &name, const std::string &prefix);
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
std::string serialise(char field_type, const std::string &field_name, const std::string &field_value);

int identify_cmd(std::string &commad);
bool is_digits(const std::string &str);
int get_minutes(std::string &hour, std::string &minute);


#define INFO(...) log(__VA_ARGS__)

#define LOG(...) log(__VA_ARGS__)
#define LOG_ERR(...) log(__VA_ARGS__)

#define LOG_CONN(...)
#define LOG_OBJ(...)
#define LOG_DATABASE(...)
#define LOG_HTTP_PROTO_PARSER(...)

#define LOG_EV(...)
#define LOG_CONN_WIRE(...)
#define LOG_HTTP_PROTO(...)
#define LOG_BINARY_PROTO(...)

#define LOG_DATABASE_WRAP(...) log(__VA_ARGS__)

#endif /* XAPIAND_INCLUDED_UTILS_H */