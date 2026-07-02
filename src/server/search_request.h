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
#include "compressor_deflate.h"             // for DeflateCompressData
#include "database/data.h"                  // for ct_type_t, accept_set_t
#include "endpoint.h"                       // for Endpoints
#include "hashes.hh"                        // for hhl
#include "http_parser.h"                    // for http_parser, http_parser_settings
#include "http_message.h"                   // for http::Request, http::RequestExtension (Leg 2 stage 3d)
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
// The endpoint views are now file-scope free functions (Leg 2 stage 3c-3), so a
// request's selected view is a plain function pointer, not a member pointer.
using view_function = void(*)(Request&);

// The generic HTTP library's output seam (Kronuz/http). Forward-declared so a
// Request can carry a pointer to the ResponseWriter it is served through by the
// http::HttpConnection that runs it (Leg 2 stage 3c).
namespace http { class ResponseWriter; }


ENUM_CLASS(RequestMode, int,
	FULL,
	STREAM,
	STREAM_NDJSON,
	STREAM_MSGPACK
)


// Xapiand's per-request working state -- now the typed EXTENSION of the library's
// http::Request (http::RequestExtension), not a parallel request object: the
// http::HttpConnection builds one via SearchApplication::create_extension() and it
// lives on the http::Request for the request's life. It carries only search-side
// state (the decoded body, resolved endpoints, tokenized URL, content negotiation,
// timings); the HTTP facts (method string, path, query, headers, body, version,
// keep-alive) belong to the http::Request it points at via `http_req`. (Leg 2 3d.)
class Request : public http::RequestExtension {
	MsgPack _decoded_body;

	MsgPack decode(std::string_view body);

public:
	using Mode = RequestMode;

	// The library request this state extends (set in SearchApplication::handle); the
	// source of truth for every HTTP fact, so nothing here duplicates it.
	const http::Request* http_req = nullptr;

	Mode mode;

	// The response being built (formerly a separate Response object; flattened here so
	// there is no parallel response class -- it is emitted through the library's
	// ResponseWriter). `response_blob` is the serialized body, `response_ct_type` its
	// content-type, and `response_status` the status once emitted (0 = not yet).
	// Content-Encoding is applied by the library, not here.
	ct_type_t response_ct_type;
	std::string response_blob;
	std::atomic<http_status> response_status;
	size_t response_size;

	// The ResponseWriter this request is served through (set by SearchApplication::handle
	// from the http::HttpConnection). The response path emits through it. (Leg 2 stage 3c.)
	http::ResponseWriter* response_writer = nullptr;
	view_function view;

	std::string _header_name;

	accept_set_t accept_set;

	enum http_method method;
	std::string path;

	// HTTP facts derived from the library request (formerly read off an embedded
	// http_parser). keep_alive already folds in the version + Connection header
	// (http_should_keep_alive); has_content_length / content_length drive the body
	// mode + buffer reserve.
	bool keep_alive = true;
	int http_major = 1;
	int http_minor = 1;
	bool has_content_length = false;
	size_t content_length = 0;

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

	// The request body, read straight from the library request (no copy). For a FULL
	// request that IS the body; a streamed request accumulates into `raw` as scratch,
	// so fall back to it when there is no http_req (defensive; it is always set in the
	// handler path).
	std::string_view body_view() const {
		return http_req != nullptr ? std::string_view(http_req->body) : std::string_view(raw);
	}
};
