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

#ifndef XAPIAND_INCLUDED_CLIENT_HTTP_H
#define XAPIAND_INCLUDED_CLIENT_HTTP_H

#include "xapiand.h"

#include "client_base.h"

#include "http_parser.h"


#define HTTP_HEADER  0x01
#define HTTP_CONTENT 0x02
#define HTTP_JSON    0x04
#define HTTP_CHUNKED 0x08
#define HTTP_OPTIONS 0x10
#define HTTP_MATCHED_COUNT 0x20

#define CMD_ID     0
#define CMD_SEARCH 1
#define CMD_FACETS 2
#define CMD_STATS  3
#define CMD_SCHEMA 4
#define CMD_UNKNOWN_HOST     5
#define CMD_UNKNOWN_ENDPOINT 6
#define CMD_UNKNOWN   -1
#define CMD_BAD_QUERY -2
#define CMD_BAD_ENDPS -3

#define HTTP_SEARCH "_search"
#define HTTP_FACETS "_facets"
#define HTTP_STATS  "_stats"
#define HTTP_SCHEMA "_schema"


// A single instance of a non-blocking Xapiand HTTP protocol handler.
class HttpClient : public BaseClient {
	struct http_parser parser;
	Database *database;

	void on_read(const char *buf, ssize_t received);

	static const http_parser_settings settings;

	std::string path;
	std::string body;
	std::string host;
	std::string command; //command or ID

	bool ishost = false;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

public:
	HttpClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	~HttpClient();

	void run();
	void _delete();
	void _index();
	void _search();
	void _patch();
	void _head();
	void _stats(query_t &e);
	int _endpointgen(query_t &e, bool writable);
	std::string http_response(int status, int mode, int matched_count = 0, std::string content=std::string(""));
	static int identify_cmd(const std::string &commad);
};

#endif /* XAPIAND_INCLUDED_CLIENT_HTTP_H */
