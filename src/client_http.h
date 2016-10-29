/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "database_handler.h"
#include "http_parser.h"
#include "guid/guid.h"
#include "url_parser.h"
#include "servers/server_http.h"
#include "atomic_shared_ptr.h"

#include <memory>

#define HTTP_STATUS            (1 << 0)
#define HTTP_HEADER            (1 << 1)
#define HTTP_ACCEPT            (1 << 2)
#define HTTP_BODY              (1 << 3)
#define HTTP_CONTENT_TYPE      (1 << 4)
#define HTTP_CONTENT_ENCODING  (1 << 5)
#define HTTP_CHUNKED           (1 << 6)
#define HTTP_OPTIONS           (1 << 7)
#define HTTP_TOTAL_COUNT       (1 << 8)
#define HTTP_MATCHES_ESTIMATED (1 << 9)
#define HTTP_EXPECTED100       (1 << 10)


using type_t = std::pair<std::string, std::string>;


struct accept_preference_comp {
	constexpr bool operator()(const std::tuple<double, int, type_t>& l, const std::tuple<double, int, type_t>& r) const noexcept {
		return (std::get<0>(l) == std::get<0>(r)) ? std::get<1>(l) < std::get<1>(r) : std::get<0>(l) > std::get<0>(r);
	}
};


using accept_set_t = std::set<std::tuple<double, int, type_t>, accept_preference_comp>;


class AcceptLRU : private lru::LRU<std::string, accept_set_t> {
	std::mutex qmtx;

public:
	AcceptLRU()
		: LRU<std::string, accept_set_t>(100) { }

	accept_set_t& at(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::at(key);
	}

	accept_set_t& insert(std::pair<std::string, accept_set_t> pair) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::insert(pair);
	}
};


// A single instance of a non-blocking Xapiand HTTP protocol handler.
class HttpClient : public BaseClient {
	enum class Command : uint64_t {
		NO_CMD_NO_ID,
		NO_CMD_ID,
		BAD_QUERY,
		CMD_SEARCH    = xxh64::hash("_search"),
		CMD_INFO      = xxh64::hash("_info"),
		CMD_SCHEMA    = xxh64::hash("_schema"),
		CMD_NODES     = xxh64::hash("_nodes"),
		CMD_QUIT      = xxh64::hash("_quit"),
	};

	struct http_parser parser;
	DatabaseHandler db_handler;

	void on_read(const char* buf, ssize_t received) override;
	void on_read_file(const char* buf, ssize_t received) override;
	void on_read_file_done() override;

	static const http_parser_settings settings;

	static GuidGenerator generator;

	static AcceptLRU accept_sets;
	accept_set_t accept_set;

	PathParser path_parser;
	QueryParser query_parser;

	bool pretty;
	std::unique_ptr<query_field_t> query_field;

	int response_status;
	size_t response_size;
	atomic_shared_ptr<Log> response_log;
	std::atomic_bool response_logged;

	std::string path;
	std::string body;
	std::string header_name;
	std::string header_value;

	size_t body_size;
	int body_descriptor;
	char body_path[PATH_MAX];
	std::vector<std::string> index_paths;

	std::string content_type;
	std::string content_length;
	bool expect_100 = false;

	std::string host;

	bool request_begining;
	std::chrono::time_point<std::chrono::system_clock> request_begins;
	std::chrono::time_point<std::chrono::system_clock> response_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_ends;
	std::chrono::time_point<std::chrono::system_clock> response_ends;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char* at, size_t length);

	std::pair<std::string, MsgPack> get_body();

	void home_view(HttpMethod method);
	void info_view(HttpMethod method);
	void delete_document_view(HttpMethod method);
	void index_document_view(HttpMethod method);
	void write_schema_view(HttpMethod method);
	void document_info_view(HttpMethod method);
	void update_document_view(HttpMethod method);
	void search_view(HttpMethod method);
	void schema_view(HttpMethod method);
	void nodes_view(HttpMethod method);
	void status_view(int status_code, const std::string& message="");

	void _options();
	void _head();
	void _get();
	void _put();
	void _post();
	void _patch();
	void _delete();

	Command url_resolve();
	void _endpoint_maker(duration<double, std::milli> timeout);
	void endpoints_maker(duration<double, std::milli> timeout);
	void query_field_maker(int flags);

	std::string http_response(int status, int mode, unsigned short http_major=0, unsigned short http_minor=9, int total_count=0, int matches_estimated=0, const std::string& body="", const std::string& ct_type="application/json; charset=UTF-8", const std::string& ct_encoding="");
	void clean_http_request();
	type_t serialize_response(const MsgPack& obj, const type_t& ct_type, bool pretty, bool serialize_error=false);

	type_t resolve_ct_type(std::string ct_type_str);
	template <typename T>
	const type_t& get_acceptable_type(const T& ct_types);
	const type_t* is_acceptable_type(const type_t& ct_type_pattern, const type_t& ct_type);
	const type_t* is_acceptable_type(const type_t& ct_type_pattern, const std::vector<type_t>& ct_types);
	void write_http_response(int st_code, const MsgPack& response=MsgPack());

	friend Worker;

public:
	std::string __repr__() const override {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "<HttpClient at %p>", this);
		return buffer;
	}

	HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_);

	~HttpClient();

	void run() override;
	void _run();
};
