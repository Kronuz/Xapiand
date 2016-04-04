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

#define HTTP_STATUS           (1 << 0)
#define HTTP_HEADER           (1 << 1)
#define HTTP_ACCEPT           (1 << 2)
#define HTTP_BODY             (1 << 3)
#define HTTP_CONTENT_TYPE     (1 << 4)
#define HTTP_CONTENT_ENCODING (1 << 5)
#define HTTP_CHUNKED          (1 << 6)
#define HTTP_OPTIONS          (1 << 7)
#define HTTP_MATCHED_COUNT    (1 << 8)
#define HTTP_EXPECTED100      (1 << 9)


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

	PathParser path_parser;
	QueryParser query_parser;

	bool pretty;
	std::unique_ptr<query_field_t> query_field;

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

	unsigned long post_id; /* only usend for method POST to generate id */

	bool request_begining;
	std::chrono::time_point<std::chrono::system_clock> request_begins;
	std::chrono::time_point<std::chrono::system_clock> response_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_ends;
	std::chrono::time_point<std::chrono::system_clock> response_ends;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char* at, size_t length);

	void home_view();
	void stats_view();
	void delete_document_view();
	void index_document_view(bool gen_id);
	void document_info_view();
	void update_document_view();
	void search_view();
	void schema_view();
	void facets_view();
	void bad_request_view();

	void _options(int cmd);
	void _head(int cmd);
	void _get(int cmd);
	void _put(int cmd);
	void _post(int cmd);
	void _patch(int cmd);
	void _delete(int cmd);

	int identify_cmd();
	int url_resolve();
	int _endpoint_maker(duration<double, std::milli> timeout);
	int endpoints_maker(duration<double, std::milli> timeout);
	void query_field_maker(int flags);

	std::string http_response(int status, int mode, unsigned short http_major=0, unsigned short http_minor=9, int matched_count=0, const std::string& body="", const std::string& ct_type="application/json; charset=UTF-8", const std::string& ct_encoding="");
	void clean_http_request();
	std::pair<std::string, std::string> serialize_response(const MsgPack& obj, const std::pair<std::string, std::string>& ct_type, bool pretty, bool serialize_error=false);
	template <typename T>
	const std::pair<std::string, std::string>& get_acceptable_type(const T& ct_types);
	const std::pair<std::string, std::string>* is_acceptable_type(const std::pair<std::string, std::string>& ct_type_pattern, const std::pair<std::string, std::string>& ct_type);
	const std::pair<std::string, std::string>* is_acceptable_type(const std::pair<std::string, std::string>& ct_type_pattern, const std::vector<std::pair<std::string, std::string>>& ct_types);
	void write_http_response(const MsgPack& response,  int st_code, bool pretty);

	friend Worker;

public:
	HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref *loop_, int sock_);

	~HttpClient();

	void run() override;
};
