/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "config.h"                         // for XAPIAND_DATABASE_WAL

#include <chrono>                           // for std::chrono, std::chrono::steady_clock, std::chrono::time_point
#include <deque>                            // for std::deque
#include <memory>                           // for shared_ptr
#include <mutex>                            // for std::mutex, std::lock_guard
#include <set>                              // for std::set
#include <stdio.h>                          // for size_t
#include <string>                           // for std::string
#include <sys/types.h>                      // for ssize_t
#include <utility>                          // for std::pair
#include <vector>                           // for std::vector

#include "atomic_shared_ptr.h"              // for atomic_shared_ptr
#include "base_client.h"                    // for BaseClient
#include "compressor_deflate.h"             // for DeflateCompressData
#include "database/data.h"                  // for ct_type_t, accept_set_t
#include "endpoint.h"                       // for Endpoints
#include "hashes.hh"                        // for hhl
#include "http_parser.h"                    // for http_parser, http_parser_settings
#include "lru.h"                            // for lru::lru
#include "msgpack.h"                        // for MsgPack
#include "phf.hh"                           // for phf::make_phf
#include "url_parser.h"                     // for PathParser, QueryParser


class DocIndexer;
class UUIDGenerator;
class Logging;
class Worker;
struct query_field_t;


#define HTTP_STATUS_RESPONSE            (1 << 0)
#define HTTP_HEADER_RESPONSE            (1 << 1)
#define HTTP_BODY_RESPONSE              (1 << 2)
#define HTTP_CONTENT_TYPE_RESPONSE      (1 << 3)
#define HTTP_CONTENT_ENCODING_RESPONSE  (1 << 4)
#define HTTP_CONTENT_LENGTH_RESPONSE    (1 << 5)
#define HTTP_OPTIONS_RESPONSE           (1 << 6)


class AcceptLRU : private lru::lru<std::string, accept_set_t> {
	std::mutex qmtx;

public:
	AcceptLRU()
		: lru::lru<std::string, accept_set_t>(100) { }

	std::pair<bool, accept_set_t> lookup(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		auto it = lru::lru::find(key);
		if (it == lru::lru::end()) {
			return std::make_pair(false, accept_set_t{});
		}
		return std::make_pair(true, it->second);
	}

	auto emplace(std::string key, accept_set_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return lru::lru::emplace(key, set);
	}
};


struct AcceptEncoding {
	int position;
	double priority;

	std::string encoding;

	AcceptEncoding(int position, double priority, std::string encoding) : position(position), priority(priority), encoding(encoding) { }
};
using accept_encoding_set_t = std::set<AcceptEncoding, accept_preference_comp<AcceptEncoding>>;


class AcceptEncodingLRU : private lru::lru<std::string, accept_encoding_set_t> {
	std::mutex qmtx;

public:
	AcceptEncodingLRU()
	: lru::lru<std::string, accept_encoding_set_t>(100) { }

	std::pair<bool, accept_encoding_set_t> lookup(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		auto it = lru::lru::find(key);
		if (it == lru::lru::end()) {
			return std::make_pair(false, accept_encoding_set_t{});
		}
		return std::make_pair(true, it->second);
	}

	auto emplace(std::string key, accept_encoding_set_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return lru::lru::emplace(key, set);
	}
};


ENUM_CLASS(Encoding, int,
	none,
	gzip,
	deflate,
	identity,
	unknown
)


class Request;
class Response;
// The endpoint views are now file-scope free functions (Leg 2 stage 3c-3), so a
// request's selected view is a plain function pointer, not a member pointer.
using view_function = void(*)(Request&);

// The generic HTTP library's output seam (Kronuz/http). Forward-declared so a
// Request can carry a pointer to the ResponseWriter it is served through by the
// http::HttpConnection that runs it (Leg 2 stage 3c).
namespace http { class ResponseWriter; }


class Response {
public:
	std::string head;
	std::string headers;
	std::string text;  // The text representation of the body (for logging purposes mostly) goes here

	ct_type_t ct_type;
	std::string blob;

	std::atomic<http_status> status;
	size_t size;

	DeflateCompressData encoding_compressor;
	DeflateCompressData::iterator it_compressor;

	Response();

	Response(const Response&) = delete;
	Response(Response&&) = delete;
	Response& operator=(const Response&) = delete;
	Response& operator=(Response&&) = delete;

	std::string to_text(bool decode);
};


ENUM_CLASS(RequestMode, int,
	FULL,
	STREAM,
	STREAM_NDJSON,
	STREAM_MSGPACK
)


class Request {
	MsgPack _decoded_body;

	MsgPack decode(std::string_view body);

public:
	using Mode = RequestMode;

	Mode mode;

	Response response;

	// The ResponseWriter this request is served through (set by SearchApplication::handle
	// from the http::HttpConnection). The response path emits through it. (Leg 2 stage 3c.)
	http::ResponseWriter* response_writer = nullptr;
	view_function view;

	Encoding type_encoding;

	std::string _header_name;

	accept_set_t accept_set;
	accept_encoding_set_t accept_encoding_set;

	enum http_method method;
	std::string path;
	struct http_parser parser;

	std::string headers;
	std::string text;  // The text representation of the body (for logging purposes mostly) goes here

	bool begining;
	bool ending;

	std::atomic_bool atom_ending;  // ending requests have received all body
	std::atomic_bool atom_ended;

	std::condition_variable pending;
	std::mutex pending_mtx;
	bool has_pending;

	std::string raw;
	size_t raw_peek;
	size_t raw_offset;

	std::mutex objects_mtx;
	std::deque<MsgPack> objects;
	msgpack::unpacker unpacker;  // msgpack unpacker

	ct_type_t ct_type;

	// Resolved index endpoints for this request. Was an HttpClient member, but it
	// is per-request state (and a connection-level member is wrong under request
	// pipelining), so it lives on the Request now. (Leg 2 stage 1.)
	Endpoints endpoints;

	size_t size;

	bool echo;
	bool human;
	bool comments;
	int indented;
	bool expect_100;
	bool closing;

	PathParser path_parser;
	QueryParser query_parser;

	std::shared_ptr<Logging> log;

	std::chrono::steady_clock::time_point begins;
	std::chrono::steady_clock::time_point received;
	std::chrono::steady_clock::time_point processing;
	std::chrono::steady_clock::time_point ready;
	std::chrono::steady_clock::time_point ends;

	atomic_shared_ptr<DocIndexer> indexer;

	Request();
	~Request() noexcept;

	Request(const Request&) = delete;
	Request(Request&&) = delete;
	Request& operator=(const Request&) = delete;
	Request& operator=(Request&&) = delete;

	bool append(const char* at, size_t length);

	bool next_object(MsgPack& obj);

	MsgPack& decoded_body();

	std::string head();

	std::string to_text(bool decode);
};
