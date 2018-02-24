/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <atomic>               // for atomic_bool
#include <chrono>               // for system_clock, time_point, duration
#include <memory>               // for shared_ptr, unique_ptr
#include <mutex>                // for mutex, lock_guard
#include <ratio>                // for milli
#include <set>                  // for set
#include <stdio.h>              // for size_t, snprintf
#include <string>               // for string, operator==
#include <sys/types.h>          // for ssize_t
#include <tuple>                // for get, tuple
#include <utility>              // for pair
#include <vector>               // for vector

#include "atomic_shared_ptr.h"  // for atomic_shared_ptr
#include "client_base.h"        // for BaseClient
#include "cuuid/uuid.h"         // for UUIDGenerator
#include "database_handler.h"   // for DatabaseHandler
#include "database_utils.h"     // for query_field_t (ptr only)
#include "deflate_compressor.h" // for DeflateCompressData
#include "http_parser.h"        // for http_parser, http_parser_settings
#include "lru.h"                // for LRU
#include "msgpack.h"            // for MsgPack
#include "url_parser.h"         // for PathParser, QueryParser
#include "hashes.hh"            // for fnv1ah32


class UUIDGenerator;
class HttpServer;
class Logging;
class Worker;


#define HTTP_STATUS_RESPONSE            (1 << 0)
#define HTTP_HEADER_RESPONSE            (1 << 1)
#define HTTP_ACCEPT_RESPONSE            (1 << 2)
#define HTTP_BODY_RESPONSE              (1 << 3)
#define HTTP_CONTENT_TYPE_RESPONSE      (1 << 4)
#define HTTP_CONTENT_ENCODING_RESPONSE  (1 << 5)
#define HTTP_CHUNKED_RESPONSE           (1 << 6)
#define HTTP_OPTIONS_RESPONSE           (1 << 7)
#define HTTP_TOTAL_COUNT_RESPONSE       (1 << 8)
#define HTTP_MATCHES_ESTIMATED_RESPONSE (1 << 9)
#define HTTP_EXPECTED_CONTINUE_RESPONSE (1 << 10)


template <typename T>
struct accept_preference_comp {
	constexpr bool operator()(const std::tuple<double, int, T, unsigned>& l, const std::tuple<double, int, T, unsigned>& r) const noexcept {
		return (std::get<0>(l) == std::get<0>(r)) ? std::get<1>(l) < std::get<1>(r) : std::get<0>(l) > std::get<0>(r);
	}
};


using accept_set_t = std::set<std::tuple<double, int, ct_type_t, int>, accept_preference_comp<ct_type_t>>;


class AcceptLRU : private lru::LRU<std::string, accept_set_t> {
	std::mutex qmtx;

public:
	AcceptLRU()
		: LRU<std::string, accept_set_t>(100) { }

	auto at(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::at(key);
	}

	auto emplace(std::string key, accept_set_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::emplace(key, set);
	}
};


using accept_encoding_t = std::set<std::tuple<double, int, std::string, unsigned>, accept_preference_comp<std::string>>;


class AcceptEncodingLRU : private lru::LRU<std::string, accept_encoding_t> {
	std::mutex qmtx;

public:
	AcceptEncodingLRU()
	: LRU<std::string, accept_encoding_t>(100) { }

	auto at(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::at(key);
	}

	auto emplace(std::string key, accept_encoding_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::emplace(key, set);
	}
};


enum class Encoding {
	none,
	gzip,
	deflate,
	identity,
	unknown,
};


// Available commands

constexpr const char COMMAND_COMMIT[]      = COMMAND_PREFIX "commit";
constexpr const char COMMAND_INFO[]        = COMMAND_PREFIX "info";
constexpr const char COMMAND_METADATA[]    = COMMAND_PREFIX "metadata";
constexpr const char COMMAND_NODES[]       = COMMAND_PREFIX "nodes";
constexpr const char COMMAND_QUIT[]        = COMMAND_PREFIX "quit";
constexpr const char COMMAND_SCHEMA[]      = COMMAND_PREFIX "schema";
constexpr const char COMMAND_SEARCH[]      = COMMAND_PREFIX "search";
constexpr const char COMMAND_STATS[]       = COMMAND_PREFIX "stats";
constexpr const char COMMAND_TOUCH[]       = COMMAND_PREFIX "touch";
constexpr const char COMMAND_WAL[]         = COMMAND_PREFIX "wal";


// A single instance of a non-blocking Xapiand HTTP protocol handler.
class HttpClient : public BaseClient {
	enum class Command : uint32_t {
		NO_CMD_NO_ID,
		NO_CMD_ID,
		BAD_QUERY,
		CMD_COMMIT    = fnv1ah32::hash(COMMAND_COMMIT),
		CMD_INFO      = fnv1ah32::hash(COMMAND_INFO),
		CMD_METADATA  = fnv1ah32::hash(COMMAND_METADATA),
		CMD_NODES     = fnv1ah32::hash(COMMAND_NODES),
#ifndef NDEBUG
		CMD_QUIT      = fnv1ah32::hash(COMMAND_QUIT),
#endif
		CMD_SCHEMA    = fnv1ah32::hash(COMMAND_SCHEMA),
		CMD_SEARCH    = fnv1ah32::hash(COMMAND_SEARCH),
		CMD_STATS     = fnv1ah32::hash(COMMAND_STATS),
		CMD_TOUCH     = fnv1ah32::hash(COMMAND_TOUCH),
#if XAPIAND_DATABASE_WAL
		CMD_WAL       = fnv1ah32::hash(COMMAND_WAL),
#endif
	};

	struct http_parser parser;
	DatabaseHandler db_handler;

	void on_read(const char* buf, ssize_t received) override;
	void on_read_file(const char* buf, ssize_t received) override;
	void on_read_file_done() override;

	static const http_parser_settings settings;

	accept_set_t accept_set;
	accept_encoding_t accept_encoding_set;

	PathParser path_parser;
	QueryParser query_parser;

	int indent;
	std::unique_ptr<query_field_t> query_field;

	enum http_status response_status;
	size_t response_size;
	atomic_shared_ptr<Logging> response_log;
	std::atomic_bool response_logged;

	std::string path;
	std::string body;
	std::string header_name;
	std::string header_value;

	std::string request_head;
	std::string request_headers;
	std::string request_body;
	std::string response_head;
	std::string response_headers;
	std::string response_body;

	size_t body_size;
	int body_descriptor;
	char body_path[PATH_MAX];
	std::vector<std::string> index_paths;

	std::string content_type;
	std::string content_length;
	bool expect_100 = false;

	DeflateCompressData encoding_compressor;
	DeflateCompressData::iterator it_compressor;

	std::string host;

	bool request_begining;
	std::chrono::time_point<std::chrono::system_clock> request_begins;
	std::chrono::time_point<std::chrono::system_clock> response_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_begins;
	std::chrono::time_point<std::chrono::system_clock> operation_ends;
	std::chrono::time_point<std::chrono::system_clock> response_ends;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char* at, size_t length);

	std::pair<ct_type_t, MsgPack> decoded_body;
	std::pair<ct_type_t, MsgPack>& get_decoded_body();

	void home_view(enum http_method method, Command cmd);
	void info_view(enum http_method method, Command cmd);
	void metadata_view(enum http_method method, Command cmd);
	void write_metadata_view(enum http_method method, Command cmd);
	void update_metadata_view(enum http_method method, Command cmd);
	void delete_metadata_view(enum http_method method, Command cmd);
	void delete_document_view(enum http_method method, Command cmd);
	void delete_schema_view(enum http_method method, Command cmd);
	void index_document_view(enum http_method method, Command cmd);
	void write_schema_view(enum http_method method, Command cmd);
	void document_info_view(enum http_method method, Command cmd);
	void update_document_view(enum http_method method, Command cmd);
	void search_view(enum http_method method, Command cmd);
	void touch_view(enum http_method method, Command cmd);
	void commit_view(enum http_method method, Command cmd);
	void schema_view(enum http_method method, Command cmd);
#if XAPIAND_DATABASE_WAL
	void wal_view(enum http_method method, Command cmd);
#endif
	void nodes_view(enum http_method method, Command cmd);
	void stats_view(enum http_method method, Command cmd);

	void _options(enum http_method method);
	void _head(enum http_method method);
	void _get(enum http_method method);
	void _merge(enum http_method method);
	void _put(enum http_method method);
	void _post(enum http_method method);
	void _patch(enum http_method method);
	void _delete(enum http_method method);

	Command url_resolve();
	void _endpoint_maker(std::chrono::duration<double, std::milli> timeout);
	void endpoints_maker(std::chrono::duration<double, std::milli> timeout);
	void query_field_maker(int flags);

	void log_request();
	void log_response();

	std::string http_response(enum http_status status, int mode, unsigned short http_major=0, unsigned short http_minor=9, int total_count=0, int matches_estimated=0, const std::string& body="", const std::string& ct_type="application/json; charset=UTF-8", const std::string& ct_encoding="");
	void clean_http_request();
	void set_idle();
	ct_type_t serialize_response(const MsgPack& obj, const ct_type_t& ct_type, int indent, bool serialize_error=false);

	ct_type_t resolve_ct_type(std::string ct_type_str);
	template <typename T>
	const ct_type_t& get_acceptable_type(const T& ct_types);
	const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t& ct_type);
	const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<ct_type_t>& ct_types);
	void write_status_response(enum http_status status, const std::string& message="");
	void write_http_response(enum http_status status, const MsgPack& response=MsgPack());
	Encoding resolve_encoding();
	std::string readable_encoding(Encoding e);
	std::string encoding_http_response(Encoding e, const std::string& response, bool chunk, bool start, bool end);

	friend Worker;

public:
	std::string __repr__() const override {
		return Worker::__repr__("HttpClient");
	}

	HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_);

	~HttpClient();

	void run() override;
};
