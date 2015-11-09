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

#include "client_base.h"

#include "http_parser.h"

#include <memory>

#define HTTP_STATUS         (1 << 0)
#define HTTP_HEADER         (1 << 1)
#define HTTP_ACCEPT         (1 << 2)
#define HTTP_BODY           (1 << 3)
#define HTTP_CONTENT_TYPE   (1 << 4)
#define HTTP_CHUNKED        (1 << 5)
#define HTTP_OPTIONS        (1 << 6)
#define HTTP_MATCHED_COUNT  (1 << 7)

#define CMD_ID     0
#define CMD_SEARCH 1
#define CMD_FACETS 2
#define CMD_STATS  3
#define CMD_SCHEMA 4
#define CMD_UPLOAD 5
#define CMD_UNKNOWN_HOST     6
#define CMD_UNKNOWN_ENDPOINT 7
#define CMD_UNKNOWN   -1
#define CMD_BAD_QUERY -2
#define CMD_BAD_ENDPS -3

#define HTTP_SEARCH "_search"
#define HTTP_FACETS "_facets"
#define HTTP_STATS  "_stats"
#define HTTP_SCHEMA "_schema"
#define HTTP_UPLOAD "_upload"


// A single instance of a non-blocking Xapiand HTTP protocol handler.
class HttpClient : public BaseClient {
	struct http_parser parser;
	std::shared_ptr<Database> database;

	void on_read(const char *buf, size_t received) override;
	void on_read_file(const char *buf, size_t received) override;
	void on_read_file_done() override;

	static const http_parser_settings settings;

	struct accept_preference_comp {
		bool operator() (const std::pair<int, std::string> a, std::pair<int, std::string> b) const
		{
			if (b.first >= a.first)
				return true;
			else return false;
		}
	};

	std::string path;
	std::string body;
	std::string header_name;
	std::string header_value;

	size_t body_size;
	int body_descriptor;
	char body_path[PATH_MAX];
	std::string index_path;

	std::string content_type;
	std::string content_length;
	std::set<std::pair<double,std::string>, accept_preference_comp> accept_set;
	bool expect_100 = false;

	std::string host;
	std::string command;  //command or ID

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

	void stats_view(const query_field &e);
	void delete_document_view(const query_field &e);
	void index_document_view(const query_field &e);
	void document_info_view(const query_field &e);
	void update_document_view(const query_field &e);
	void upload_view(const query_field &e);
	void search_view(const query_field &e, bool facets, bool schema);
	void bad_request_view(const query_field &e, int cmd);

	void _options();
	void _head();
	void _get();
	void _put();
	void _post();
	void _patch();
	void _delete();

	int _endpointgen(query_field &e, bool writable);
	static int identify_cmd(const std::string &commad);

	friend Worker;

public:
	HttpClient(std::shared_ptr<XapiandServer> server_, ev::loop_ref *loop_, int sock_);

	~HttpClient();

	void run() override;
};
