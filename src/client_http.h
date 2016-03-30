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

#include "servers/server_http.h"
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
#define HTTP_EXPECTED100    (1 << 8)

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

	void on_read(const char* buf, size_t received) override;
	void on_read_file(const char* buf, size_t received) override;
	void on_read_file_done() override;

	static const http_parser_settings settings;

	struct accept_preference_comp {
		constexpr bool operator()(const std::tuple<double, int, std::pair<std::string, std::string>>& l, const std::tuple<double, int, std::pair<std::string, std::string>>& r) const noexcept {
			return (std::get<0>(l) == std::get<0>(r)) ? std::get<1>(l) < std::get<1>(r) : std::get<0>(l) > std::get<0>(r);
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
	std::set<std::tuple<double, int, std::pair<std::string, std::string>>, accept_preference_comp> accept_set;
	bool expect_100 = false;

	std::string host;
	std::string command;  //command or ID
	std::string mode; //parameter optional in url

	unsigned long post_id; /* only usend for method POST to generate id */

	bool request_begining;
	std::chrono::time_point<std::chrono::system_clock> request_begins;
	std::chrono::time_point<std::chrono::system_clock> response_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_ends;
	std::chrono::time_point<std::chrono::system_clock> response_ends;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char* at, size_t length);

	void stats_view(const query_field_t& e, int mode);
	void delete_document_view(const query_field_t& e);
	void index_document_view(const query_field_t& e, bool gen_id=false);
	void document_info_view(const query_field_t& e);
	void update_document_view(const query_field_t& e);
	void upload_view(const query_field_t& e);
	void search_view(const query_field_t& e, bool facets, bool schema);
	void bad_request_view(const query_field_t& e, int cmd);

	void _options();
	void _head();
	void _get();
	void _put();
	void _post();
	void _patch();
	void _delete();

	int url_resolve(query_field_t& e, bool writable);
	int endpoint_maker(parser_url_path_t& p, bool writable, bool require);
	void query_maker(const char* query_str, size_t query_size, int cmd, query_field_t& e, parser_query_t& q);
	static int identify_cmd(const std::string& commad);
	int identify_mode(const std::string &mode);

	std::string http_response(int status, int mode, unsigned short http_major=0, unsigned short http_minor=9, int matched_count=0, std::string body=std::string(""), std::string ct_type=std::string("application/json; charset=UTF-8"));
	void clean_http_request();
	std::string serialize_response(const MsgPack& obj, const std::pair<std::string, std::string>& ct_type, bool pretty);
	const std::pair<std::string, std::string>& get_acceptable_type(const std::pair<std::string, std::string>& ct_type);
	bool is_acceptable_type(const std::pair<std::string, std::string>& ct_type_pattern, const std::pair<std::string, std::string>& ct_type);
	std::pair<std::string, std::string> content_type_pair(const std::string& ct_type);
	void write_http_response(const MsgPack& response,  int st_code, bool pretty);

	friend Worker;

public:
	HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref *loop_, int sock_);

	~HttpClient();

	void run() override;
};
