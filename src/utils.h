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

#include <string>

void log(void *obj, const char *fmt, ...);

std::string repr(const char *p, size_t size);
std::string repr(const std::string &string);

enum commands_value
{
    command_search = 1
    ,command_count
    ,command_facets
    ,command_similar
    ,identifier
};


typedef struct parser_query_t {
    const char *offset;
    size_t length;
} parser_query;



typedef struct parser_url_path_t {
    const char *off_path;
    const char *off_host;
    const char *off_namespace;
    const char *off_command;
    size_t len_path;
    size_t len_host;
    size_t len_namespace;
    size_t len_command;
} parser_url_path;

int url_path(const char**, parser_url_path *);
int url_qs(const char *, const char *, parser_query *);
std::string urldecode(const char *, size_t);
int look_cmd(const char *);

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