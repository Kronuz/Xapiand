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

#include "http_client.h"

#include "config.h"                         // for XAPIAND_CLUSTERING, XAPIAND_CHAISCRIPT, XAPIAND_DATABASE_WAL

#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <exception>                        // for std::exception
#include <functional>                       // for std::function
#include <regex>                            // for std::regex, std::regex_constants
#include <signal.h>                         // for SIGTERM
#include <sysexits.h>                       // for EX_SOFTWARE
#include <syslog.h>                         // for LOG_WARNING, LOG_ERR, LOG...
#include <utility>                          // for std::move

#ifdef USE_ICU
#include <unicode/uvernum.h>
#endif

#ifdef XAPIAND_CHAISCRIPT
#include "chaiscript/chaiscript_defines.hpp"  // for chaiscript::Build_Info
#endif
#include "cppcodec/base64_rfc4648.hpp"      // for cppcodec::base64_rfc4648
#include "database/handler.h"               // for DatabaseHandler, DocIndexer
#include "database/utils.h"                 // for query_field_t
#include "database/pool.h"                  // for DatabasePool
#include "database/schema.h"                // for Schema
#include "endpoint.h"                       // for Endpoints, Endpoint
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "ev/ev++.h"                        // for async, io, loop_ref (ptr ...
#include "exception_xapian.h"               // for Exception, SerialisationError
#include "field_parser.h"                   // for FieldParser, FieldParserError
#include "hashes.hh"                        // for hhl
#include "http_utils.h"                     // for catch_http_errors
#include "io.hh"                            // for close, write, unlink
#include "log.h"                            // for L_CALL, L_ERR, LOG_DEBUG
#include "logger.h"                         // for Logging
#include "manager.h"                        // for XapiandManager
#include "metrics.h"                        // for Metrics::metrics
#include "mime_types.hh"                    // for mime_type
#include "msgpack.h"                        // for MsgPack, msgpack::object
#include "aggregations/aggregations.h"      // for AggregationMatchSpy
#include "node.h"                           // for Node::local_node, Node::leader_node
#include "opts.h"                           // for opts::*
#include "package.h"                        // for Package::*
#include "phf.hh"                           // for phf::*
#include "rapidjson/document.h"             // for Document
#include "reserved/aggregations.h"          // for RESERVED_AGGS_*
#include "reserved/fields.h"                // for RESERVED_*
#include "reserved/query_dsl.h"             // for RESERVED_QUERYDSL_*
#include "reserved/schema.h"                // for RESERVED_SCHEMA
#include "response.h"                       // for RESPONSE_*
#include "serialise.h"                      // for Serialise::boolean
#include "strings.hh"                       // for strings::from_delta
#include "system.hh"                        // for check_compiler, check_OS, check_architecture
#include "xapian.h"                         // for Xapian::major_version, Xapian::minor_version


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_HTTP
// #define L_HTTP L_RED
// #undef L_HTTP_WIRE
// #define L_HTTP_WIRE L_ORANGE
// #undef L_HTTP_PROTO
// #define L_HTTP_PROTO L_TEAL


#define QUERY_FIELD_PRIMARY    (1 << 0)
#define QUERY_FIELD_WRITABLE   (1 << 1)
#define QUERY_FIELD_COMMIT     (1 << 2)
#define QUERY_FIELD_SEARCH     (1 << 3)
#define QUERY_FIELD_ID         (1 << 4)
#define QUERY_FIELD_TIME       (1 << 5)
#define QUERY_FIELD_PERIOD     (1 << 6)
#define QUERY_FIELD_VOLATILE   (1 << 7)
#define QUERY_FIELD_OFFSET     (1 << 8)

#define DEFAULT_INDENTATION 2


static const std::regex header_params_re(R"(\s*;\s*([a-z]+)=(\d+(?:\.\d+)?))", std::regex::optimize);
static const std::regex header_accept_re(R"(([-a-z+]+|\*)/([-a-z+]+|\*)((?:\s*;\s*[a-z]+=\d+(?:\.\d+)?)*))", std::regex::optimize);
static const std::regex header_accept_encoding_re(R"(([-a-z+]+|\*)((?:\s*;\s*[a-z]+=\d+(?:\.\d+)?)*))", std::regex::optimize);

static const std::string eol("\r\n");


// Available commands

#define METHODS_OPTIONS() \
	OPTION(DELETE,   "delete") \
	OPTION(GET,      "get") \
	OPTION(HEAD,     "head") \
	OPTION(POST,     "post") \
	OPTION(PUT,      "put") \
	OPTION(CONNECT,  "connect") \
	OPTION(OPTIONS,  "options") \
	OPTION(TRACE,    "trace") \
	OPTION(PATCH,    "patch") \
	OPTION(PURGE,    "purge") \
	OPTION(LINK,     "link") \
	OPTION(UNLINK,   "unlink") \
	OPTION(CHECK,    "check") \
	OPTION(CLOSE,    "close") \
	OPTION(COMMIT,   "commit") \
	OPTION(COPY,     "copy") \
	OPTION(COUNT,    "count") \
	OPTION(DUMP,     "dump") \
	OPTION(FLUSH,    "flush") \
	OPTION(INFO,     "info") \
	OPTION(LOCK,     "lock") \
	OPTION(MOVE,     "move") \
	OPTION(OPEN,     "open") \
	OPTION(QUIT,     "quit") \
	OPTION(RESTORE,  "restore") \
	OPTION(SEARCH,   "search") \
	OPTION(UNLOCK,   "unlock") \
	OPTION(UPDATE,   "update") \
	OPTION(UPSERT,   "upsert") \
	OPTION(WAL,      "wal")


constexpr static auto http_methods = phf::make_phf({
	#define OPTION(name, str) hhl(str),
	METHODS_OPTIONS()
	#undef OPTION
});


bool is_range(std::string_view str) {
	try {
		FieldParser fieldparser(str);
		fieldparser.parse();
		return fieldparser.is_range();
	} catch (const FieldParserError&) {
		return false;
	}
}


bool can_preview(const ct_type_t& ct_type) {
	#define CONTENT_TYPE_OPTIONS() \
		OPTION("application/eps") \
		OPTION("application/pdf") \
		OPTION("application/postscript") \
		OPTION("application/x-bzpdf") \
		OPTION("application/x-eps") \
		OPTION("application/x-gzpdf") \
		OPTION("application/x-pdf") \
		OPTION("application/x-photoshop") \
		OPTION("application/photoshop") \
		OPTION("application/psd")

	constexpr static auto _ = phf::make_phf({
		#define OPTION(ct) hhl(ct),
		CONTENT_TYPE_OPTIONS()
		#undef OPTION
	});
	switch (_.fhhl(ct_type.to_string())) {
		#define OPTION(ct) case _.fhhl(ct):
		CONTENT_TYPE_OPTIONS()
		#undef OPTION
			return true;
		default:
			return ct_type.first == "image";
	}
}


/*
 *  _   _ _   _
 * | | | | |_| |_ _ __
 * | |_| | __| __| '_ \
 * |  _  | |_| |_| |_) |
 * |_| |_|\__|\__| .__/
 *               |_|
 */


HttpClient::HttpClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: BaseClient<HttpClient>(std::move(parent_), ev_loop_, ev_flags_),
	  new_request(std::make_shared<Request>(this))
{
	auto manager = XapiandManager::manager();
	if (manager) {
		++manager->http_clients;
	}

	Metrics::metrics()
		.xapiand_http_connections
		.Increment();

	// Initialize new_request->begins as soon as possible (for correctly timing disconnecting clients)
	new_request->begins = std::chrono::steady_clock::now();

	L_CONN("New Http Client, {} client(s) of a total of {} connected.", manager ? manager->http_clients.load() : 0, manager ? manager->total_clients.load() : 0);
}


HttpClient::~HttpClient() noexcept
{
	try {
		if (auto manager = XapiandManager::manager()) {
			if (manager->http_clients.fetch_sub(1) == 0) {
				L_CRIT("Inconsistency in number of http clients");
				sig_exit(-EX_SOFTWARE);
			}
		}

		if (is_shutting_down() && !is_idle()) {
			L_INFO("HTTP client killed!");
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


std::string
HttpClient::http_response(Request& request, enum http_status status, int mode, const std::string& body, const std::string& location, const std::string& ct_type, const std::string& ct_encoding, size_t content_length) {
	L_CALL("HttpClient::http_response()");

	std::string head;
	std::string headers;
	std::string head_sep;
	std::string headers_sep;
	std::string response_body;

	if ((mode & HTTP_STATUS_RESPONSE) != 0) {
		assert(request.response.status == static_cast<http_status>(0));
		request.response.status = status;
		auto http_major = request.parser.http_major;
		auto http_minor = request.parser.http_minor;
		if (http_major == 0 && http_minor == 0) {
			http_major = 1;
		}
		head += strings::format("HTTP/{}.{} {} ", http_major, http_minor, status);
		head += http_status_str(status);
		head_sep += eol;
		if ((mode & HTTP_HEADER_RESPONSE) == 0) {
			headers_sep += eol;
		}
	}

	assert(request.response.status != static_cast<http_status>(0));

	if ((mode & HTTP_HEADER_RESPONSE) != 0) {
		headers += "Server: " + Package::STRING + eol;

		// if (!endpoints.empty()) {
		// 	headers += "Database: " + endpoints.to_string() + eol;
		// }

		request.ends = std::chrono::steady_clock::now();

		if (request.human) {
			headers += strings::format("Response-Time: {}", strings::from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count())) + eol;
			if (request.ready >= request.processing) {
				headers += strings::format("Operation-Time: {}", strings::from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count())) + eol;
			}
		} else {
			headers += strings::format("Response-Time: {}", std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count() / 1e9) + eol;
			if (request.ready >= request.processing) {
				headers += strings::format("Operation-Time: {}", std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count() / 1e9) + eol;
			}
		}

		if ((mode & HTTP_OPTIONS_RESPONSE) != 0) {
			headers += "Allow: GET, POST, PUT, PATCH, UPDATE, STORE, DELETE, HEAD, OPTIONS" + eol;
		}

		if (!location.empty()) {
			headers += "Location: " + location + eol;
		}

		if ((mode & HTTP_CONTENT_TYPE_RESPONSE) != 0 && !ct_type.empty()) {
			headers += "Content-Type: " + ct_type + eol;
		}

		if ((mode & HTTP_CONTENT_ENCODING_RESPONSE) != 0 && !ct_encoding.empty()) {
			headers += "Content-Encoding: " + ct_encoding + eol;
		}

		if ((mode & HTTP_CONTENT_LENGTH_RESPONSE) != 0) {
			headers += strings::format("Content-Length: {}", content_length) + eol;
		} else {
			headers += strings::format("Content-Length: {}", body.size()) + eol;
		}
		headers_sep += eol;
	}

	if ((mode & HTTP_BODY_RESPONSE) != 0) {
		response_body += body;
	}

	auto this_response_size = response_body.size();
	request.response.size += this_response_size;

	if (Logging::log_level >= LOG_DEBUG) {
		request.response.head += head;
		request.response.headers += headers;
	}

	return head + head_sep + headers + headers_sep + response_body;
}


template <typename Func>
int
HttpClient::handled_errors(Request& request, Func&& func)
{
	L_CALL("HttpClient::handled_errors()");

	auto http_errors = catch_http_errors(std::forward<Func>(func));

	if (http_errors.error_code != HTTP_STATUS_OK) {
		if (request.response.status != static_cast<http_status>(0)) {
			// There was an error, but request already had written stuff...
			// disconnect client!
			detach();
		} else {
			write_status_response(request, http_errors.error_code, http_errors.error);
		}
		request.atom_ending = true;
	}

	return http_errors.ret;
}


size_t
HttpClient::pending_requests() const
{
	std::lock_guard<std::mutex> lk(runner_mutex);
	auto requests_size = requests.size();
	if (requests_size && requests.front()->response.status != static_cast<http_status>(0)) {
		--requests_size;
	}
	return requests_size;
}


bool
HttpClient::is_idle() const
{
	L_CALL("HttpClient::is_idle() {{is_waiting:{}, is_running:{}, write_queue_empty:{}, pending_requests:{}}}", is_waiting(), is_running(), write_queue.empty(), pending_requests());

	return !is_waiting() && !is_running() && write_queue.empty() && !pending_requests();
}


void
HttpClient::shutdown_impl(long long asap, long long now)
{
	L_CALL("HttpClient::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		shutting_down = true;
		auto manager = XapiandManager::manager();
		if (now != 0 || !manager || manager->ready_to_end_http() || is_idle()) {
			stop(false);
			destroy(false);
			detach();
		}
	} else {
		if (is_idle()) {
			stop(false);
			destroy(false);
			detach();
		}
	}
}


void
HttpClient::destroy_impl()
{
	L_CALL("HttpClient::destroy_impl()");

	BaseClient<HttpClient>::destroy_impl();

	// HttpClient could be using indexer (which would block)
	// if destroying is received, finish indexer:
	std::unique_lock<std::mutex> lk(runner_mutex);
	if (!requests.empty()) {
		auto& request = *requests.front();
		auto indexer = request.indexer.load();
		if (indexer) {
			indexer->finish();
			request.indexer.store(nullptr);
		}
	}
}


ssize_t
HttpClient::on_read(const char* buf, ssize_t received)
{
	L_CALL("HttpClient::on_read(<buf>, {})", received);

	if (received <= 0) {
		std::string reason;

		if (received < 0) {
			reason = strings::format("{} ({}): {}", error::name(errno), errno, error::description(errno));
			if (errno != ENOTCONN && errno != ECONNRESET && errno != ESPIPE) {
				L_NOTICE("HTTP client connection closed unexpectedly after {}: {}", strings::from_delta(new_request->begins, std::chrono::steady_clock::now()), reason);
				close();
				return received;
			}
		} else {
			reason = "EOF";
		}

		auto state = HTTP_PARSER_STATE(&new_request->parser);
		if (state != s_start_req) {
			L_NOTICE("HTTP client closed unexpectedly after {}: Not in final HTTP state ({}): {}", strings::from_delta(new_request->begins, std::chrono::steady_clock::now()), state, reason);
			close();
			return received;
		}

		if (is_waiting()) {
			L_NOTICE("HTTP client closed unexpectedly after {}: There was still a request in progress: {}", strings::from_delta(new_request->begins, std::chrono::steady_clock::now()), reason);
			close();
			return received;
		}

		if (!write_queue.empty()) {
			L_NOTICE("HTTP client closed unexpectedly after {}: There is still pending data: {}", strings::from_delta(new_request->begins, std::chrono::steady_clock::now()), reason);
			close();
			return received;
		}

		if (pending_requests()) {
			L_NOTICE("HTTP client closed unexpectedly after {}: There are still pending requests: {}", strings::from_delta(new_request->begins, std::chrono::steady_clock::now()), reason);
			close();
			return received;
		}

		// HTTP client normally closed connection.
		close();
		return received;
	}

	L_HTTP_WIRE("HttpClient::on_read: {} bytes", received);
	ssize_t parsed = http_parser_execute(&new_request->parser, &parser_settings, buf, received);
	if (parsed != received) {
		http_errno err = HTTP_PARSER_ERRNO(&new_request->parser);
		if (err == HPE_INVALID_METHOD) {
			if (new_request->response.status == static_cast<http_status>(0)) {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
				end_http_request(*new_request);
			}
		} else {
			std::string message(http_errno_description(err));
			L_DEBUG("HTTP parser error: {}", HTTP_PARSER_ERRNO(&new_request->parser) != HPE_OK ? message : "incomplete request");
			if (new_request->response.status == static_cast<http_status>(0)) {
				write_status_response(*new_request, HTTP_STATUS_BAD_REQUEST, message);
				end_http_request(*new_request);
			}
		}
		detach();
	}

	return received;
}


void
HttpClient::on_read_file(const char* /*buf*/, ssize_t received)
{
	L_CALL("HttpClient::on_read_file(<buf>, {})", received);

	L_ERR("Not Implemented: HttpClient::on_read_file: {} bytes", received);
}


void
HttpClient::on_read_file_done()
{
	L_CALL("HttpClient::on_read_file_done()");

	L_ERR("Not Implemented: HttpClient::on_read_file_done");
}


// HTTP parser callbacks.
const http_parser_settings HttpClient::parser_settings = {
	HttpClient::message_begin_cb,
	HttpClient::url_cb,
	HttpClient::status_cb,
	HttpClient::header_field_cb,
	HttpClient::header_value_cb,
	HttpClient::headers_complete_cb,
	HttpClient::body_cb,
	HttpClient::message_complete_cb,
	HttpClient::chunk_header_cb,
	HttpClient::chunk_complete_cb,
};


inline std::string readable_http_parser_flags(http_parser* parser) {
	std::vector<std::string> values;
	if ((parser->flags & F_CHUNKED) == F_CHUNKED) values.push_back("F_CHUNKED");
	if ((parser->flags & F_CONNECTION_KEEP_ALIVE) == F_CONNECTION_KEEP_ALIVE) values.push_back("F_CONNECTION_KEEP_ALIVE");
	if ((parser->flags & F_CONNECTION_CLOSE) == F_CONNECTION_CLOSE) values.push_back("F_CONNECTION_CLOSE");
	if ((parser->flags & F_CONNECTION_UPGRADE) == F_CONNECTION_UPGRADE) values.push_back("F_CONNECTION_UPGRADE");
	if ((parser->flags & F_TRAILING) == F_TRAILING) values.push_back("F_TRAILING");
	if ((parser->flags & F_UPGRADE) == F_UPGRADE) values.push_back("F_UPGRADE");
	if ((parser->flags & F_SKIPBODY) == F_SKIPBODY) values.push_back("F_SKIPBODY");
	if ((parser->flags & F_CONTENTLENGTH) == F_CONTENTLENGTH) values.push_back("F_CONTENTLENGTH");
	return strings::join(values, "|");
}


int
HttpClient::message_begin_cb(http_parser* parser)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_message_begin(parser);
	});
}

int
HttpClient::url_cb(http_parser* parser, const char* at, size_t length)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_url(parser, at, length);
	});
}

int
HttpClient::status_cb(http_parser* parser, const char* at, size_t length)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_status(parser, at, length);
	});
}

int
HttpClient::header_field_cb(http_parser* parser, const char* at, size_t length)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_header_field(parser, at, length);
	});
}

int
HttpClient::header_value_cb(http_parser* parser, const char* at, size_t length)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_header_value(parser, at, length);
	});
}

int
HttpClient::headers_complete_cb(http_parser* parser)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_headers_complete(parser);
	});
}

int
HttpClient::body_cb(http_parser* parser, const char* at, size_t length)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_body(parser, at, length);
	});
}

int
HttpClient::message_complete_cb(http_parser* parser)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_message_complete(parser);
	});
}

int
HttpClient::chunk_header_cb(http_parser* parser)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_chunk_header(parser);
	});
}

int
HttpClient::chunk_complete_cb(http_parser* parser)
{
	auto http_client = static_cast<HttpClient *>(parser->data);
	return http_client->handled_errors(*http_client->new_request, [&]{
		return http_client->on_chunk_complete(parser);
	});
}


int
HttpClient::on_message_begin([[maybe_unused]] http_parser* parser)
{
	L_CALL("HttpClient::on_message_begin(<parser>)");

	L_HTTP_PROTO("on_message_begin {{state:{}, header_state:{}}}", enum_name(HTTP_PARSER_STATE(parser)), enum_name(HTTP_PARSER_HEADER_STATE(parser)));

	waiting = true;
	new_request->begins = std::chrono::steady_clock::now();
	L_TIMED_VAR(new_request->log, 10s,
		"Request taking too long...",
		"Request took too long!");

	return 0;
}

int
HttpClient::on_url(http_parser* parser, const char* at, size_t length)
{
	L_CALL("HttpClient::on_url(<parser>, <at>, <length>)");

	L_HTTP_PROTO("on_url {{state:{}, header_state:{}}}: {}", enum_name(HTTP_PARSER_STATE(parser)), enum_name(HTTP_PARSER_HEADER_STATE(parser)), repr(at, length));

	new_request->method = HTTP_PARSER_METHOD(parser);

	new_request->path.append(at, length);

	return 0;
}

int
HttpClient::on_status([[maybe_unused]] http_parser* parser, [[maybe_unused]] const char* at, [[maybe_unused]] size_t length)
{
	L_CALL("HttpClient::on_status(<parser>, <at>, <length>)");

	L_HTTP_PROTO("on_status {{state:{}, header_state:{}}}: {}", enum_name(HTTP_PARSER_STATE(parser)), enum_name(HTTP_PARSER_HEADER_STATE(parser)), repr(at, length));

	return 0;
}

int
HttpClient::on_header_field([[maybe_unused]] http_parser* parser, const char* at, size_t length)
{
	L_CALL("HttpClient::on_header_field(<parser>, <at>, <length>)");

	L_HTTP_PROTO("on_header_field {{state:{}, header_state:{}}}: {}", enum_name(HTTP_PARSER_STATE(parser)), enum_name(HTTP_PARSER_HEADER_STATE(parser)), repr(at, length));

	new_request->_header_name = std::string(at, length);

	return 0;
}

int
HttpClient::on_header_value([[maybe_unused]] http_parser* parser, const char* at, size_t length)
{
	L_CALL("HttpClient::on_header_value(<parser>, <at>, <length>)");

	L_HTTP_PROTO("on_header_value {{state:{}, header_state:{}}}: {}", enum_name(HTTP_PARSER_STATE(parser)), enum_name(HTTP_PARSER_HEADER_STATE(parser)), repr(at, length));

	auto _header_value = std::string_view(at, length);
	if (Logging::log_level >= LOG_DEBUG) {
		new_request->headers.append(new_request->_header_name);
		new_request->headers.append(": ");
		new_request->headers.append(_header_value);
		new_request->headers.append(eol);
	}

	constexpr static auto _ = phf::make_phf({
		hhl("expect"),
		hhl("100-continue"),
		hhl("content-type"),
		hhl("accept"),
		hhl("accept-encoding"),
		hhl("http-method-override"),
		hhl("x-http-method-override"),
	});

	switch (_.fhhl(new_request->_header_name)) {
		case _.fhhl("expect"):
		case _.fhhl("100-continue"):
			// Respond with HTTP/1.1 100 Continue
			new_request->expect_100 = true;
			break;

		case _.fhhl("content-type"):
			new_request->ct_type = ct_type_t(_header_value);
			break;

		case _.fhhl("accept"): {
			static AcceptLRU accept_sets;
			auto value = strings::lower(_header_value);
			auto lookup = accept_sets.lookup(value);
			if (!lookup.first) {
				std::sregex_iterator next(value.begin(), value.end(), header_accept_re, std::regex_constants::match_any);
				std::sregex_iterator end;
				int i = 0;
				while (next != end) {
					int indent = -1;
					double q = 1.0;
					if (next->length(3) != 0) {
						auto param = next->str(3);
						std::sregex_iterator next_param(param.begin(), param.end(), header_params_re, std::regex_constants::match_any);
						while (next_param != end) {
							if (next_param->str(1) == "q") {
								q = strict_stod(next_param->str(2));
							} else if (next_param->str(1) == "indent") {
								indent = strict_stoi(next_param->str(2));
								if (indent < 0) { indent = 0;
								} else if (indent > 16) { indent = 16; }
							}
							++next_param;
						}
					}
					lookup.second.emplace(i, q, ct_type_t(next->str(1), next->str(2)), indent);
					++next;
					++i;
				}
				accept_sets.emplace(value, lookup.second);
			}
			new_request->accept_set = std::move(lookup.second);
			break;
		}

		case _.fhhl("accept-encoding"): {
			static AcceptEncodingLRU accept_encoding_sets;
			auto value = strings::lower(_header_value);
			auto lookup = accept_encoding_sets.lookup(value);
			if (!lookup.first) {
				std::sregex_iterator next(value.begin(), value.end(), header_accept_encoding_re, std::regex_constants::match_any);
				std::sregex_iterator end;
				int i = 0;
				while (next != end) {
					double q = 1.0;
					if (next->length(2) != 0) {
						auto param = next->str(2);
						std::sregex_iterator next_param(param.begin(), param.end(), header_params_re, std::regex_constants::match_any);
						while (next_param != end) {
							if (next_param->str(1) == "q") {
								q = strict_stod(next_param->str(2));
							}
							++next_param;
						}
					} else {
					}
					lookup.second.emplace(i, q, next->str(1));
					++next;
					++i;
				}
				accept_encoding_sets.emplace(value, lookup.second);
			}
			new_request->accept_encoding_set = std::move(lookup.second);
			break;
		}

		case _.fhhl("x-http-method-override"):
		case _.fhhl("http-method-override"): {
			switch (http_methods.fhhl(_header_value)) {
				#define OPTION(name, str) \
				case http_methods.fhhl(str): \
					if ( \
						new_request->method != HTTP_POST && \
						new_request->method != HTTP_GET && \
						new_request->method != HTTP_##name \
					) { \
						THROW(ClientError, "{} header must use the POST method", repr(new_request->_header_name)); \
					} \
					new_request->method = HTTP_##name; \
					break;
				METHODS_OPTIONS()
				#undef OPTION
				default:
					L_HTTP_PROTO("Invalid HTTP method override: {}", repr(_header_value));
					parser->http_errno = HPE_INVALID_METHOD;
					break;
			}
			break;
		}
	}

	return 0;
}

int
HttpClient::on_headers_complete([[maybe_unused]] http_parser* parser)
{
	L_CALL("HttpClient::on_headers_complete(<parser>)");

	L_HTTP_PROTO("on_headers_complete {{state:{}, header_state:{}, flags:[{}]}}",
		enum_name(HTTP_PARSER_STATE(parser)),
		enum_name(HTTP_PARSER_HEADER_STATE(parser)),
		readable_http_parser_flags(parser));

	// Prepare the request view
	if (int err = prepare()) {
		end_http_request(*new_request);
		return err;
	}

	if likely(!closed) {
		if likely(!new_request->atom_ending) {
			if likely(new_request->view) {
				if (new_request->mode != Request::Mode::FULL) {
					std::lock_guard<std::mutex> lk(runner_mutex);
					if (requests.empty() || new_request != requests.front()) {
						requests.push_back(new_request);  // Enqueue streamed request
					}
				}
			}
		}
	}

	return 0;
}

int
HttpClient::on_body([[maybe_unused]] http_parser* parser, const char* at, size_t length)
{
	L_CALL("HttpClient::on_body(<parser>, <at>, {})", length);

	L_HTTP_PROTO("on_body {{state:{}, header_state:{}, flags:[{}]}}: {}",
		enum_name(HTTP_PARSER_STATE(parser)),
		enum_name(HTTP_PARSER_HEADER_STATE(parser)),
		readable_http_parser_flags(parser),
		repr(at, length));

	new_request->size += length;

	if likely(!closed) {
		if likely(!new_request->atom_ending) {
			bool signal_pending = false;
			if (Logging::log_level >= LOG_DEBUG || new_request->view) {
				signal_pending = new_request->append(at, length);
			}
			if likely(new_request->view) {
				if (signal_pending) {
					{
						std::lock_guard<std::mutex> pending_lk(new_request->pending_mtx);
						new_request->has_pending = true;
						new_request->pending.notify_one();
					}

					std::lock_guard<std::mutex> lk(runner_mutex);
					if (!running) {  // Start a runner if not running
						running = true;
						XapiandManager::manager(true)->http_client_pool->enqueue(share_this<HttpClient>());
					}
				}
			}
		}
	}

	return 0;
}

int
HttpClient::on_message_complete([[maybe_unused]] http_parser* parser)
{
	L_CALL("HttpClient::on_message_complete(<parser>)");

	L_HTTP_PROTO("on_message_complete {{state:{}, header_state:{}, flags:[{}]}}",
		enum_name(HTTP_PARSER_STATE(parser)),
		enum_name(HTTP_PARSER_HEADER_STATE(parser)),
		readable_http_parser_flags(parser));

	if (Logging::log_level > LOG_DEBUG) {
		log_request(*new_request);
	}

	if likely(!closed) {
		std::shared_ptr<Request> request = std::make_shared<Request>(this);
		std::swap(new_request, request);

		if likely(!request->atom_ending) {
			request->atom_ending = true;

			bool signal_pending = false;
			if (Logging::log_level >= LOG_DEBUG || request->view) {
				request->append(nullptr, 0);  // flush pending stuff
				signal_pending = true;  // always signal pending
			}
			if likely(request->view) {
				if (signal_pending) {
					{
						// always signal, so view continues ending
						std::lock_guard<std::mutex> pending_lk(request->pending_mtx);
						request->has_pending = true;
						request->pending.notify_one();
					}

					std::lock_guard<std::mutex> lk(runner_mutex);
					if (requests.empty() || request != requests.front()) {
						requests.push_back(std::move(request));  // Enqueue request
					}
					if (!running) {  // Start a runner if not running
						running = true;
						XapiandManager::manager(true)->http_client_pool->enqueue(share_this<HttpClient>());
					}
				}
			} else {
				end_http_request(*request);
			}
		}
	}

	waiting = false;

	return 0;
}

int
HttpClient::on_chunk_header([[maybe_unused]] http_parser* parser)
{
	L_CALL("HttpClient::on_chunk_header(<parser>)");

	L_HTTP_PROTO("on_chunk_header {{state:{}, header_state:{}, flags:[{}]}}",
		enum_name(HTTP_PARSER_STATE(parser)),
		enum_name(HTTP_PARSER_HEADER_STATE(parser)),
		readable_http_parser_flags(parser));

	return 0;
}

int
HttpClient::on_chunk_complete([[maybe_unused]] http_parser* parser)
{
	L_CALL("HttpClient::on_chunk_complete(<parser>)");

	L_HTTP_PROTO("on_chunk_complete {{state:{}, header_state:{}, flags:[{}]}}",
		enum_name(HTTP_PARSER_STATE(parser)),
		enum_name(HTTP_PARSER_HEADER_STATE(parser)),
		readable_http_parser_flags(parser));

	return 0;
}


int
HttpClient::prepare()
{
	L_CALL("HttpClient::prepare()");

	L_TIMED_VAR(request.log, 1s,
		"Response taking too long: {}",
		"Response took too long: {}",
		request.head());

	new_request->received = std::chrono::steady_clock::now();

	if (new_request->parser.http_major == 0 || (new_request->parser.http_major == 1 && new_request->parser.http_minor == 0)) {
		new_request->closing = true;
	}
	if ((new_request->parser.flags & F_CONNECTION_KEEP_ALIVE) == F_CONNECTION_KEEP_ALIVE) {
		new_request->closing = false;
	}
	if ((new_request->parser.flags & F_CONNECTION_CLOSE) == F_CONNECTION_CLOSE) {
		new_request->closing = true;
	}

	if (new_request->accept_set.empty()) {
		if (!new_request->ct_type.empty()) {
			new_request->accept_set.emplace(0, 1.0, new_request->ct_type, 0);
		}
		new_request->accept_set.emplace(1, 1.0, any_type, 0);
	}

	new_request->type_encoding = resolve_encoding(*new_request);
	if (new_request->type_encoding == Encoding::unknown) {
		write_status_response(*new_request, HTTP_STATUS_NOT_ACCEPTABLE, "Response encoding gzip, deflate or identity not provided in the Accept-Encoding header");
		return 1;
	}

	url_resolve(*new_request);

	auto id = new_request->path_parser.get_id();
	auto has_pth = new_request->path_parser.has_pth();
	auto cmd = new_request->path_parser.get_cmd();

	if (!cmd.empty()) {
		auto mapping = cmd;
		mapping.remove_prefix(1);
		switch (http_methods.fhhl(mapping)) {
			#define OPTION(name, str) \
			case http_methods.fhhl(str): \
				if ( \
					new_request->method != HTTP_POST && \
					new_request->method != HTTP_GET && \
					new_request->method != HTTP_##name \
				) { \
					THROW(ClientError, "HTTP Mappings must use GET or POST method"); \
				} \
				new_request->method = HTTP_##name; \
				cmd = ""; \
				break;
			METHODS_OPTIONS()
			#undef OPTION
		}
	}

	switch (new_request->method) {
		case HTTP_SEARCH:
			if (id.empty()) {
				new_request->view = &HttpClient::search_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_COUNT:
			if (id.empty()) {
				new_request->view = &HttpClient::count_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_INFO:
			if (id.empty()) {
				new_request->view = &HttpClient::info_view;
			} else {
				new_request->view = &HttpClient::info_view;
			}
			break;

		case HTTP_HEAD:
			if (id.empty()) {
				new_request->view = &HttpClient::database_exists_view;
			} else {
				new_request->view = &HttpClient::document_exists_view;
			}
			break;

		case HTTP_GET:
			if (!cmd.empty() && id.empty()) {
				if (!has_pth && cmd == ":metrics") {
					new_request->view = &HttpClient::metrics_view;
				} else {
					new_request->view = &HttpClient::retrieve_metadata_view;
				}
			} else if (!id.empty()) {
				if (is_range(id)) {
					new_request->view = &HttpClient::search_view;
				} else {
					new_request->view = &HttpClient::retrieve_document_view;
				}
			} else {
				new_request->view = &HttpClient::retrieve_database_view;
			}
			break;

		case HTTP_POST:
			if (!cmd.empty() && id.empty()) {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			} else if (!id.empty()) {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			} else {
				new_request->view = &HttpClient::write_document_view;
			}
			break;

		case HTTP_PUT:
			if (!cmd.empty() && id.empty()) {
				new_request->view = &HttpClient::write_metadata_view;
			} else if (!id.empty()) {
				new_request->view = &HttpClient::write_document_view;
			} else {
				new_request->view = &HttpClient::write_database_view;
			}
			break;

		case HTTP_PATCH:
		case HTTP_UPDATE:
		case HTTP_UPSERT:
			if (!cmd.empty() && id.empty()) {
				new_request->view = &HttpClient::update_metadata_view;
			} else if (!id.empty()) {
				new_request->view = &HttpClient::update_document_view;
			} else {
				new_request->view = &HttpClient::write_database_view;
			}
			break;

		case HTTP_DELETE:
			if (!cmd.empty() && id.empty()) {
				new_request->view = &HttpClient::delete_metadata_view;
			} else if (!id.empty()) {
				new_request->view = &HttpClient::delete_document_view;
			} else if (has_pth) {
				new_request->view = &HttpClient::delete_database_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_COMMIT:
			if (id.empty()) {
				new_request->view = &HttpClient::commit_database_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_DUMP:
			if (id.empty()) {
				new_request->view = &HttpClient::dump_database_view;
			} else {
				new_request->view = &HttpClient::dump_document_view;
			}
			break;

		case HTTP_RESTORE:
			if (id.empty()) {
				if ((new_request->parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH) {
					if (new_request->ct_type == ndjson_type || new_request->ct_type == x_ndjson_type) {
						new_request->mode = Request::Mode::STREAM_NDJSON;
					} else if (new_request->ct_type == msgpack_type || new_request->ct_type == x_msgpack_type) {
						new_request->mode = Request::Mode::STREAM_MSGPACK;
					}
				}
				new_request->view = &HttpClient::restore_database_view;
			} else {
				new_request->view = &HttpClient::write_document_view;
			}
			break;

		case HTTP_CHECK:
			if (id.empty()) {
				new_request->view = &HttpClient::check_database_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_FLUSH:
			if (opts.admin_commands && id.empty() && !has_pth) {
				// Flush both databases and clients by default (unless one is specified)
				new_request->query_parser.rewind();
				int flush_databases = new_request->query_parser.next("databases");
				new_request->query_parser.rewind();
				int flush_clients = new_request->query_parser.next("clients");
				if (flush_databases != -1 || flush_clients == -1) {
					XapiandManager::manager(true)->database_pool->cleanup(true, false);
				}
				if (flush_clients != -1 || flush_databases == -1) {
					XapiandManager::manager(true)->shutdown(0, 0);
				}
				write_http_response(*new_request, HTTP_STATUS_OK);
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

		case HTTP_OPTIONS:
			write(http_response(*new_request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_OPTIONS_RESPONSE | HTTP_BODY_RESPONSE));
			break;

		case HTTP_QUIT:
			if (opts.admin_commands && !has_pth && id.empty()) {
				XapiandManager::try_shutdown(true);
				write_http_response(*new_request, HTTP_STATUS_OK);
				destroy();
				detach();
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;

#if XAPIAND_DATABASE_WAL
		case HTTP_WAL:
			if (id.empty()) {
				new_request->view = &HttpClient::wal_view;
			} else {
				write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			}
			break;
#endif

		case HTTP_OPEN:
		case HTTP_CLOSE:
			write_status_response(*new_request, HTTP_STATUS_NOT_IMPLEMENTED);
			break;

		default:
			L_HTTP_PROTO("Invalid HTTP method: {}", enum_name(new_request->method));
			write_status_response(*new_request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			new_request->parser.http_errno = HPE_INVALID_METHOD;
			return 1;
	}

	if (!new_request->view && Logging::log_level < LOG_DEBUG) {
		return 1;
	}

	if (new_request->expect_100) {
		// Return "100 Continue" if client sent "Expect: 100-continue"
		write(http_response(*new_request, HTTP_STATUS_CONTINUE, HTTP_STATUS_RESPONSE));
		// Go back to unknown response state:
		new_request->response.head.clear();
		new_request->response.status = static_cast<http_status>(0);
	}

	if ((new_request->parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH && new_request->parser.content_length) {
		if (new_request->mode == Request::Mode::STREAM_MSGPACK) {
			new_request->unpacker.reserve_buffer(new_request->parser.content_length);
		} else {
			new_request->raw.reserve(new_request->parser.content_length);
		}
	}

	return 0;
}


void
HttpClient::process(Request& request)
{
	L_CALL("HttpClient::process()");

	L_OBJ_BEGIN("HttpClient::process:BEGIN");
	L_OBJ_END("HttpClient::process:END");

	handled_errors(request, [&]{
		(this->*request.view)(request);
		return 0;
	});
}


void
HttpClient::operator()()
{
	L_CALL("HttpClient::operator()()");

	L_CONN("Start running in worker...");

	std::unique_lock<std::mutex> lk(runner_mutex);

	while (!requests.empty() && !closed) {
		Request& request = *requests.front();
		if (request.atom_ended) {
			requests.pop_front();
			continue;
		}
		request.ending = request.atom_ending.load();
		lk.unlock();

		// wait for the request to be ready
		if (!request.ending && !request.wait()) {
			lk.lock();
			continue;
		}

		try {
			assert(request.view);
			process(request);
			request.begining = false;
		} catch (...) {
			request.begining = false;
			end_http_request(request);
			lk.lock();
			requests.pop_front();
			running = false;
			L_CONN("Running in worker ended with an exception.");
			lk.unlock();
			L_EXC("ERROR: HTTP client ended with an unhandled exception");
			detach();
			throw;
		}
		if (request.ending) {
			end_http_request(request);
			auto closing = request.closing;
			lk.lock();
			requests.pop_front();
			if (closing) {
				running = false;
				L_CONN("Running in worker ended after request closing.");
				lk.unlock();
				destroy();
				detach();
				return;
			}
		} else {
			lk.lock();
		}
	}

	running = false;
	L_CONN("Running in replication worker ended. {{requests_empty:{}, closed:{}, is_shutting_down:{}}}", requests.empty(), closed.load(), is_shutting_down());
	lk.unlock();

	if (is_shutting_down() && is_idle()) {
		detach();
		return;
	}

	redetach();  // try re-detaching if already flagged as detaching
}


MsgPack
HttpClient::node_obj()
{
	L_CALL("HttpClient::node_obj()");

	endpoints.clear();
	auto leader_node = Node::get_leader_node();
	endpoints.add(Endpoint{".xapiand/nodes", leader_node});

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN);

	auto local_node = Node::get_local_node();
	auto document = db_handler.get_document(local_node->lower_name());

	auto obj = document.get_obj();

	obj.update(MsgPack({
#ifdef XAPIAND_CLUSTERING
		{ "cluster_name", opts.cluster_name },
#endif
		{ "server", {
			{ "name", Package::NAME },
			{ "url", Package::URL },
			{ "issues", Package::BUGREPORT },
			{ "version", Package::VERSION },
			{ "revision", Package::REVISION },
			{ "hash", Package::HASH },
			{ "compiler", check_compiler() },
			{ "os", check_OS() },
			{ "arch", check_architecture() },
		} },
		{ "versions", {
			{ "Xapiand", Package::REVISION.empty() ? Package::VERSION : strings::format("{}_{}", Package::VERSION, Package::REVISION) },
			{ "Xapian", strings::format("{}.{}.{}", Xapian::major_version(), Xapian::minor_version(), Xapian::revision()) },
#ifdef XAPIAND_CHAISCRIPT
			{ "ChaiScript", strings::format("{}.{}", chaiscript::Build_Info::version_major(), chaiscript::Build_Info::version_minor()) },
#endif
#ifdef USE_ICU
			{ "ICU", strings::format("{}.{}", U_ICU_VERSION_MAJOR_NUM, U_ICU_VERSION_MINOR_NUM) },
#endif
		} },
		{ "options", {
			{ "verbosity", opts.verbosity },
			{ "processors", opts.processors },
			{ "limits", {
				// { "max_clients", opts.max_clients },
				{ "max_database_readers", opts.max_database_readers },
			} },
			{ "cache", {
				{ "database_pool_size", opts.database_pool_size },
				{ "schema_pool_size", opts.schema_pool_size },
				{ "scripts_cache_size", opts.scripts_cache_size },
#ifdef XAPIAND_CLUSTERING
				{ "resolver_cache_size", opts.resolver_cache_size },
#endif
			} },
			{ "thread_pools", {
				{ "num_shards", opts.num_shards },
				{ "num_replicas", opts.num_replicas },
				{ "num_http_servers", opts.num_http_servers },
				{ "num_http_clients", opts.num_http_clients },
#ifdef XAPIAND_CLUSTERING
				{ "num_remote_servers", opts.num_remote_servers },
				{ "num_remote_clients", opts.num_remote_clients },
				{ "num_replication_servers", opts.num_replication_servers },
				{ "num_replication_clients", opts.num_replication_clients },
#endif
				{ "num_async_wal_writers", opts.num_async_wal_writers },
				{ "num_doc_matchers", opts.num_doc_matchers },
				{ "num_doc_preparers", opts.num_doc_preparers },
				{ "num_doc_indexers", opts.num_doc_indexers },
				{ "num_committers", opts.num_committers },
				{ "num_fsynchers", opts.num_fsynchers },
#ifdef XAPIAND_CLUSTERING
				{ "num_replicators", opts.num_replicators },
				{ "num_discoverers", opts.num_discoverers },
#endif
			} },
		} },
	}));

	return obj;
}


void
HttpClient::metrics_view(Request& request)
{
	L_CALL("HttpClient::metrics_view()");

	auto query_field = query_field_maker(request, 0);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	auto server_info =  XapiandManager::server_metrics();
	write(http_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_LENGTH_RESPONSE | HTTP_BODY_RESPONSE, server_info, "", "text/plain", "", server_info.size()));
}


void
HttpClient::document_exists_view(Request& request)
{
	L_CALL("HttpClient::document_exists_view()");

	auto query_field = query_field_maker(request, 0);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN);

	db_handler.get_document(request.path_parser.get_id()).validate();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


void
HttpClient::delete_document_view(Request& request)
{
	L_CALL("HttpClient::delete_document_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	std::string document_id(request.path_parser.get_id());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	db_handler.delete_document(document_id, query_field.commit);
	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_NO_CONTENT);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Deletion took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "delete"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::write_document_view(Request& request)
{
	L_CALL("HttpClient::write_document_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	auto document_id = request.path_parser.get_id();

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
	bool stored = !request.ct_type.empty() && request.ct_type != json_type && request.ct_type != x_json_type && request.ct_type != yaml_type && request.ct_type != x_yaml_type && request.ct_type != msgpack_type && request.ct_type != x_msgpack_type;
	auto indexed = db_handler.index(document_id, query_field.version, stored, decoded_body, query_field.commit, request.ct_type.empty() ? mime_type(selector) : request.ct_type);

	request.ready = std::chrono::steady_clock::now();

	std::string location;

	auto info = indexed.first;
	auto& response_obj = indexed.second;

	Document document(info.did, &db_handler);

	if (request.echo) {
		auto it = response_obj.find(ID_FIELD_NAME);
		if (document_id.empty()) {
			if (it == response_obj.end()) {
				auto document_id_obj = db_handler.unserialise_term_id(info.term);
				location = strings::format("/{}/{}", unsharded_path(endpoints[0].path).first, document_id_obj.as_str());
				response_obj[ID_FIELD_NAME] = std::move(document_id_obj);
			} else {
				location = strings::format("/{}/{}", unsharded_path(endpoints[0].path).first, it.value().as_str());
			}
		} else {
			if (it == response_obj.end()) {
				response_obj[ID_FIELD_NAME] = db_handler.unserialise_term_id(info.term);
			}
		}

		response_obj[VERSION_FIELD_NAME] = info.version;

		if (request.comments) {
			response_obj[RESPONSE_xDOCID] = info.did;

			size_t n_shards = endpoints.size();
			size_t shard_num = (info.did - 1) % n_shards;
			response_obj[RESPONSE_xSHARD] = shard_num + 1;
			// response_obj[RESPONSE_xENDPOINT] = endpoints[shard_num].to_string();
		}

		if (!selector.empty()) {
			response_obj = response_obj.select(selector);
		}

		write_http_response(request, HTTP_STATUS_OK, response_obj, location);
	} else {
		if (document_id.empty()) {
			auto it = response_obj.find(ID_FIELD_NAME);
			if (it == response_obj.end()) {
				auto document_id_obj = db_handler.unserialise_term_id(info.term);
				location = strings::format("/{}/{}", unsharded_path(endpoints[0].path).first, document_id_obj.as_str());
			} else {
				location = strings::format("/{}/{}", unsharded_path(endpoints[0].path).first, it.value().as_str());
			}
		}

		write_http_response(request, document_id.empty() ? HTTP_STATUS_CREATED : HTTP_STATUS_NO_CONTENT, MsgPack(), location);
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Indexing took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "index"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::update_document_view(Request& request)
{
	L_CALL("HttpClient::update_document_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	auto document_id = request.path_parser.get_id();
	assert(!document_id.empty());

	request.processing = std::chrono::steady_clock::now();

	std::string operation;
	DocumentInfo indexed;
	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
	if (request.method == HTTP_PATCH) {
		operation = "patch";
		indexed = db_handler.patch(document_id, query_field.version, false, decoded_body, query_field.commit);
	} else if (request.method == HTTP_UPSERT) {
		operation = "update";
		bool stored = !request.ct_type.empty() && request.ct_type != json_type && request.ct_type != x_json_type && request.ct_type != yaml_type && request.ct_type != x_yaml_type && request.ct_type != msgpack_type && request.ct_type != x_msgpack_type;
		indexed = db_handler.update(document_id, query_field.version, stored, true, decoded_body, query_field.commit, request.ct_type.empty() ? mime_type(selector) : request.ct_type);
	} else {
		operation = "update";
		bool stored = !request.ct_type.empty() && request.ct_type != json_type && request.ct_type != x_json_type && request.ct_type != yaml_type && request.ct_type != x_yaml_type && request.ct_type != msgpack_type && request.ct_type != x_msgpack_type;
		indexed = db_handler.update(document_id, query_field.version, stored, false, decoded_body, query_field.commit, request.ct_type.empty() ? mime_type(selector) : request.ct_type);
	}

	request.ready = std::chrono::steady_clock::now();

	if (request.echo) {
		auto info = indexed.first;
		auto& response_obj = indexed.second;

		Document document(info.did, &db_handler);

		if (response_obj.find(ID_FIELD_NAME) == response_obj.end()) {
			response_obj[ID_FIELD_NAME] = db_handler.unserialise_term_id(info.term);
		}

		response_obj[VERSION_FIELD_NAME] = info.version;

		if (request.comments) {
			response_obj[RESPONSE_xDOCID] = info.did;

			size_t n_shards = endpoints.size();
			size_t shard_num = (info.did - 1) % n_shards;
			response_obj[RESPONSE_xSHARD] = shard_num + 1;
			// response_obj[RESPONSE_xENDPOINT] = endpoints[shard_num].to_string();
		}

		if (!selector.empty()) {
			response_obj = response_obj.select(selector);
		}

		write_http_response(request, HTTP_STATUS_OK, response_obj);
	} else {
		write_http_response(request, HTTP_STATUS_NO_CONTENT);
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Updating took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", operation},
		})
		.Observe(took / 1e9);
}


void
HttpClient::retrieve_metadata_view(Request& request)
{
	L_CALL("HttpClient::retrieve_metadata_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	MsgPack response_obj;

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(endpoints, DB_OPEN);
	}

	auto key = request.path_parser.get_cmd();
	assert(!key.empty());
	key.remove_prefix(1);

	if (key.empty()) {
		response_obj = MsgPack::MAP();
		for (auto& _key : db_handler.get_metadata_keys()) {
			auto metadata = db_handler.get_metadata(_key);
			if (!metadata.empty()) {
				response_obj[_key] = MsgPack::unserialise(metadata);
			}
		}
	} else {
		auto metadata = db_handler.get_metadata(key);
		if (metadata.empty()) {
			throw Xapian::DocNotFoundError("Metadata not found");
		} else {
			response_obj = MsgPack::unserialise(metadata);
		}
	}

	request.ready = std::chrono::steady_clock::now();

	if (!selector.empty()) {
		response_obj = response_obj.select(selector);
	}

	write_http_response(request, HTTP_STATUS_OK, response_obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Get metadata took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "get_metadata"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::write_metadata_view(Request& request)
{
	L_CALL("HttpClient::write_metadata_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(endpoints, DB_OPEN);
	}

	auto key = request.path_parser.get_cmd();
	assert(!key.empty());
	key.remove_prefix(1);

	if (key.empty() || key == "schema" || key == "wal" || key == "nodes" || key == "metrics") {
		THROW(ClientError, "Metadata {} is read-only", repr(request.path_parser.get_cmd()));
	}

	db_handler.set_metadata(key, decoded_body.serialise());

	request.ready = std::chrono::steady_clock::now();

	if (request.echo) {
		write_http_response(request, HTTP_STATUS_OK, selector.empty() ? decoded_body : decoded_body.select(selector));
	} else {
		write_http_response(request, HTTP_STATUS_NO_CONTENT);
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Set metadata took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "set_metadata"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::update_metadata_view(Request& request)
{
	L_CALL("HttpClient::update_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


void
HttpClient::delete_metadata_view(Request& request)
{
	L_CALL("HttpClient::delete_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


void
HttpClient::info_view(Request& request)
{
	L_CALL("HttpClient::info_view()");

	MsgPack response_obj;

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(endpoints, DB_OPEN);
	}

	// Info about a specific document was requested
	if (request.path_parser.off_id != nullptr) {
		auto id = request.path_parser.get_id();

		request.query_parser.rewind();
		bool raw = request.query_parser.next("raw") != -1;

		response_obj = db_handler.get_document_info(id, raw, request.human);
	} else {
		response_obj = db_handler.get_database_info();
	}

	request.ready = std::chrono::steady_clock::now();

	if (!selector.empty()) {
		response_obj = response_obj.select(selector);
	}

	write_http_response(request, HTTP_STATUS_OK, response_obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Info took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "info"}
		})
		.Observe(took / 1e9);
}


void
HttpClient::database_exists_view(Request& request)
{
	L_CALL("HttpClient::database_exists_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_OPEN);

	db_handler.reopen();  // Ensure it can be opened.

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


MsgPack
HttpClient::retrieve_database(const query_field_t& query_field, bool is_root, std::string_view selector)
{
	L_CALL("HttpClient::retrieve_database()");

	if (is_root) {
		if (selector == "state") {
			auto manager = XapiandManager::manager();
			if (manager) {
				return enum_name(manager->state.load());
			}
		}
	}

#ifdef XAPIAND_CLUSTERING
	auto nodes = MsgPack::ARRAY();
	if (is_root) {
		for (auto& node : Node::nodes()) {
			auto node_obj = MsgPack::MAP();
			node_obj["name"] = node->name();
			if (node->is_active()) {
				node_obj["active"] = true;
				if (!opts.solo) {
					node_obj["leader"] = node->is_leader();
					node_obj["local"] = node->is_local();
				}
				node_obj["host"] = node->host();
				node_obj["http_port"] = node->http_port;
				node_obj["remote_port"] = node->remote_port;
				node_obj["replication_port"] = node->replication_port;
			} else {
				node_obj["active"] = false;
			}
			nodes.push_back(node_obj);
		}

		if (selector == "nodes") {
			return nodes;
		}
	}
#endif

	auto obj = MsgPack::MAP();

	// Get active schema
	MsgPack schema;
	try {
		DatabaseHandler db_handler;
		if (query_field.writable || query_field.primary) {
			db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(endpoints, DB_OPEN);
		}

		// Retrieve full schema
		schema = db_handler.get_schema()->get_full(true);
	} catch (const Xapian::DocNotFoundError&) {
		if (!is_root) {
			throw;
		}
	} catch (const Xapian::DatabaseNotFoundError&) {
		if (!is_root) {
			throw;
		}
	}

	// Get index settings (from .xapiand/indices)
	MsgPack settings;
	auto id = std::string(endpoints.size() == 1 ? endpoints[0].path : unsharded_path(endpoints[0].path).first);
	endpoints = XapiandManager::resolve_index_endpoints(
		Endpoint{".xapiand/indices"},
		query_field.writable,
		query_field.primary);

	try {
		DatabaseHandler db_handler;
		if (query_field.writable || query_field.primary) {
			db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(endpoints, DB_OPEN);
		}

		// Retrive document ID
		auto did = db_handler.get_docid(id);

		// Retrive document data
		auto document = db_handler.get_document(did);
		settings = document.get_obj();

		// Remove schema, ID and version from document:
		auto it_e = settings.end();
		auto it = settings.find(SCHEMA_FIELD_NAME);
		if (it != it_e) {
			settings.erase(it);
		}
		it = settings.find(ID_FIELD_NAME);
		if (it != it_e) {
			settings.erase(it);
		}
		it = settings.find(VERSION_FIELD_NAME);
		if (it != it_e) {
			settings.erase(it);
		}
	} catch (const Xapian::DocNotFoundError&) {
		if (!is_root) {
			throw;
		}
	} catch (const Xapian::DatabaseNotFoundError&) {
		if (!is_root) {
			throw;
		}
	}

	// Add node information for '/':
	if (is_root) {
		obj.update(node_obj());
#ifdef XAPIAND_CLUSTERING
		obj["nodes"] = nodes;
#endif
		auto manager = XapiandManager::manager();
		if (manager) {
			obj["state"] = enum_name(manager->state.load());
		}
	}

	if (!settings.empty()) {
		obj[RESERVED_SETTINGS].update(settings);
	}

	if (!schema.empty()) {
		obj[RESERVED_SCHEMA].update(schema);
	}

	if (!selector.empty()) {
		obj = obj.select(selector);
	}

	return obj;
}


void
HttpClient::retrieve_database_view(Request& request)
{
	L_CALL("HttpClient::retrieve_database_view()");

	assert(request.path_parser.get_id().empty());

	auto is_root = !request.path_parser.has_pth();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	auto obj = retrieve_database(query_field, is_root, selector);

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Retrieving database took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "retrieve_database"},
		})
		.Observe(took / 1e9);

	L_SEARCH("FINISH RETRIEVE DATABASE");
}


void
HttpClient::write_database_view(Request& request)
{
	L_CALL("HttpClient::write_database_view()");

	assert(request.path_parser.get_id().empty());

	auto is_root = !request.path_parser.has_pth();

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE);
	if (resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	if (decoded_body.is_map()) {
		auto schema_it = decoded_body.find(RESERVED_SCHEMA);
		if (schema_it != decoded_body.end()) {
			auto& schema = schema_it.value();
			if (request.method == HTTP_UPDATE) {
				db_handler.update_schema(schema);
			} else {
				db_handler.write_schema(schema);
			}
		}
	}

	db_handler.reopen();  // Ensure touch.

	request.ready = std::chrono::steady_clock::now();

	if (request.echo) {
		auto obj = retrieve_database(query_field, is_root, selector);

		write_http_response(request, HTTP_STATUS_OK, obj);
	} else {
		write_http_response(request, HTTP_STATUS_NO_CONTENT);
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Updating database took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "update_database"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::delete_database_view(Request& request)
{
	L_CALL("HttpClient::delete_database_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


void
HttpClient::commit_database_view(Request& request)
{
	L_CALL("HttpClient::commit_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	db_handler.commit();  // Ensure touch.

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Commit took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "commit"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::dump_document_view(Request& request)
{
	L_CALL("HttpClient::dump_document_view()");

	auto query_field = query_field_maker(request, 0);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto document_id = request.path_parser.get_id();
	assert(!document_id.empty());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_OPEN);

	auto obj = db_handler.dump_document(document_id);

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Dump took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "dump"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::dump_database_view(Request& request)
{
	L_CALL("HttpClient::dump_database_view()");

	auto query_field = query_field_maker(request, 0);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints, DB_OPEN);

	auto& ct_type = resolve_ct_type(request);
	if (ct_type.empty()) {
		// No content type could be resolved, return NOT ACCEPTABLE.
		write_status_response(request, HTTP_STATUS_NOT_ACCEPTABLE, "Response type not accepted by the Accept header");
		return;
	}

	auto docs = db_handler.dump_documents();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, docs);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Dump took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "dump"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::restore_database_view(Request& request)
{
	L_CALL("HttpClient::restore_database_view()");

	auto indexer = request.indexer.load();

	if (request.mode == Request::Mode::STREAM_MSGPACK || request.mode == Request::Mode::STREAM_NDJSON) {
		MsgPack obj;
		while (request.next_object(obj)) {
			if (!indexer) {
				auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT | QUERY_FIELD_OFFSET);
				if (resolve_index_endpoints(request, query_field, &obj) > 1) {
					THROW(ClientError, "Method can only be used with single indexes");
				}

				request.processing = std::chrono::steady_clock::now();

				indexer = DocIndexer::make_shared(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, request.echo, request.comments, query_field);
				request.indexer.store(indexer);
			}
			indexer->prepare(std::move(obj));
		}
	} else {
		auto& docs = request.decoded_body();
		if (!docs.is_array()) {
			THROW(ClientError, "Invalid request body");
		}
		for (auto& obj : docs) {
			if (!indexer) {
				auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT | QUERY_FIELD_OFFSET);
				if (resolve_index_endpoints(request, query_field, &obj) > 1) {
					THROW(ClientError, "Method can only be used with single indexes");
				}

				request.processing = std::chrono::steady_clock::now();

				indexer = DocIndexer::make_shared(endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, request.echo, request.comments, query_field);
				request.indexer.store(indexer);
			}
			indexer->prepare(std::move(obj));
		}
	}

	if (request.ending) {
		if (indexer && !indexer->wait()) {
			indexer.reset();
		}

		request.ready = std::chrono::steady_clock::now();
		auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();

		MsgPack response_obj = {
			// { RESPONSE_ENDPOINT, endpoints.to_string() },
			{ RESPONSE_PREPARED, indexer ? indexer->prepared() : 0 },
			{ RESPONSE_PROCESSED, indexer ? indexer->processed() : 0 },
			{ RESPONSE_INDEXED, indexer ? indexer->indexed() : 0 },
			{ RESPONSE_TOTAL, indexer ? indexer->total() : 0 },
			{ RESPONSE_ITEMS, indexer ? indexer->results() : MsgPack::ARRAY() },
		};

		if (request.human) {
			response_obj[RESPONSE_TOOK] = strings::from_delta(took);
		} else {
			response_obj[RESPONSE_TOOK] = took / 1e9;
		}

		write_http_response(request, HTTP_STATUS_OK, response_obj);

		L_TIME("Restore took {}", strings::from_delta(took));

		Metrics::metrics()
			.xapiand_operations_summary
			.Add({
				{"operation", "restore"},
			})
			.Observe(took / 1e9);
	}
}


#if XAPIAND_DATABASE_WAL
void
HttpClient::wal_view(Request& request)
{
	L_CALL("HttpClient::wal_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints);

	request.query_parser.rewind();
	bool unserialised = request.query_parser.next("raw") == -1;
	auto obj = db_handler.repr_wal(0, std::numeric_limits<Xapian::rev>::max(), unserialised);

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("WAL took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "wal"},
		})
		.Observe(took / 1e9);
}
#endif


void
HttpClient::check_database_view(Request& request)
{
	L_CALL("HttpClient::check_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(endpoints);

	auto status = db_handler.check();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, status);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Database check took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "db_check"},
		})
		.Observe(took / 1e9);
}


void
HttpClient::retrieve_document_view(Request& request)
{
	L_CALL("HttpClient::retrieve_document_view()");

	auto id = request.path_parser.get_id();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_ID);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(endpoints, DB_OPEN);
	}

	// Retrive document ID
	Xapian::docid did;
	did = db_handler.get_docid(id);

	// Retrive document data
	auto document = db_handler.get_document(did);
	auto document_data = document.get_data();
	const Data data(document_data.empty() ? std::string(DATABASE_DATA_MAP) : std::move(document_data));
	auto selector_mime_type = mime_type(selector);
	auto accepted = data.get_accepted(request.accept_set, selector_mime_type);
	if (accepted.first == nullptr) {
		// No content type could be resolved, return NOT ACCEPTABLE.
		write_status_response(request, HTTP_STATUS_NOT_ACCEPTABLE, "Response type not accepted by the Accept header");
		return;
	}

	auto& locator = *accepted.first;
	if (locator.ct_type.empty()) {
		if (selector_mime_type == json_type || selector_mime_type == x_json_type ||
			selector_mime_type == yaml_type || selector_mime_type == x_yaml_type ||
			selector_mime_type == msgpack_type || selector_mime_type == x_msgpack_type) {
			selector = "";
		}

		// Locator doesn't have a content type, serialize and return as document
		auto obj = MsgPack::unserialise(locator.data());

		// Detailed info about the document:
		if (obj.find(ID_FIELD_NAME) == obj.end()) {
			obj[ID_FIELD_NAME] = document.get_value(ID_FIELD_NAME);
		}

		if (obj.find(VERSION_FIELD_NAME) == obj.end()) {
			auto version = document.get_value(DB_SLOT_VERSION);
			if (!version.empty()) {
				obj[VERSION_FIELD_NAME] = static_cast<Xapian::rev>(sortable_unserialise(version));
			}
		}

		if (request.comments) {
			obj[RESPONSE_xDOCID] = did;

			size_t n_shards = endpoints.size();
			size_t shard_num = (did - 1) % n_shards;
			obj[RESPONSE_xSHARD] = shard_num + 1;
			// obj[RESPONSE_xENDPOINT] = endpoints[shard_num].to_string();
		}

		if (!selector.empty()) {
			obj = obj.select(selector);
		}

		request.ready = std::chrono::steady_clock::now();

		write_http_response(request, HTTP_STATUS_OK, obj, "", resolve_ct_type(request, selector_mime_type));
	} else {
		// Locator has content type, return as a blob (an image for instance)
		auto ct_type = locator.ct_type;
		request.response.blob = locator.data();
#ifdef XAPIAND_DATA_STORAGE
		if (locator.type == Locator::Type::stored || locator.type == Locator::Type::compressed_stored) {
			if (request.response.blob.empty()) {
				auto stored = db_handler.storage_get_stored(locator, did);
				request.response.blob = unserialise_string_at(STORED_BLOB, stored);
			}
		}
#endif

		request.ready = std::chrono::steady_clock::now();

		request.response.ct_type = ct_type;
		if (request.type_encoding != Encoding::none) {
			auto encoded = encoding_http_response(request.response, request.type_encoding, request.response.blob, false, true, true);
			if (!encoded.empty() && encoded.size() <= request.response.blob.size()) {
				write(http_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_BODY_RESPONSE, encoded, "", ct_type.to_string(), readable_encoding(request.type_encoding)));
			} else {
				write(http_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_BODY_RESPONSE, request.response.blob, "", ct_type.to_string(), readable_encoding(Encoding::identity)));
			}
		} else {
			write(http_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_BODY_RESPONSE, request.response.blob, "", ct_type.to_string()));
		}
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Retrieving took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "retrieve"},
		})
		.Observe(took / 1e9);

	L_SEARCH("FINISH RETRIEVE");
}


void
HttpClient::search_view(Request& request)
{
	L_CALL("HttpClient::search_view()");

	std::string selector_string_holder;

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_SEARCH);
	resolve_index_endpoints(request, query_field);

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	std::tuple<Xapian::MSet, MsgPack, Xapian::Query> mset_tuple;
	MsgPack aggregations;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	try {
		if (query_field.primary) {
			db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(endpoints, DB_OPEN);
		}

		if (request.raw.empty()) {
			mset_tuple = db_handler.get_mset(query_field, nullptr, nullptr);
		} else {
			auto& decoded_body = request.decoded_body();
			if (!decoded_body.is_map()) {
				THROW(ClientError, "Invalid request body");
			}

			AggregationMatchSpy aggs(decoded_body, db_handler.get_schema());

			if (decoded_body.find(RESERVED_QUERYDSL_SELECTOR) != decoded_body.end()) {
				auto selector_obj = decoded_body.at(RESERVED_QUERYDSL_SELECTOR);
				if (selector_obj.is_string()) {
					selector_string_holder = selector_obj.as_str();
					selector = selector_string_holder;
				} else {
					THROW(ClientError, "The {} must be a string", RESERVED_QUERYDSL_SELECTOR);
				}
			}

			mset_tuple = db_handler.get_mset(query_field, &decoded_body, &aggs);
			aggregations = aggs.get_aggregation().at(RESERVED_AGGS_AGGREGATIONS);
		}
	} catch (const Xapian::DatabaseNotFoundError&) {
		/* At the moment when the endpoint does not exist and it is chunck it will return 200 response
		 * with zero matches this behavior may change in the future for instance ( return 404 ) */
	}
	auto& mset = std::get<0>(mset_tuple);
	auto& qdsl = std::get<1>(mset_tuple);
	auto& query = std::get<2>(mset_tuple);

	MsgPack obj;
	if (aggregations) {
		obj[RESPONSE_AGGREGATIONS] = aggregations;
	}
	obj[RESPONSE_HITS] = MsgPack::ARRAY();
	if (request.comments && opts.verbosity >= 3) {
		obj["#query"] = qdsl;
		obj["#xapian_query"] = query.get_description();
	}
	obj[RESPONSE_COUNT] = mset.size();
	obj[RESPONSE_TOTAL] = mset.get_matches_estimated();

	auto& hits = obj[RESPONSE_HITS];

	const auto m_e = mset.end();
	for (auto m = mset.begin(); m != m_e; ++m) {
		auto did = *m;

		// Retrive document data
		auto document = db_handler.get_document(did);
		auto document_data = document.get_data();
		const auto data = Data(document_data.empty() ? std::string(DATABASE_DATA_MAP) : std::move(document_data));

		auto hit_obj = MsgPack::MAP();
		auto main_locator = data.get("");
		if (main_locator != nullptr) {
			auto locator_data = main_locator->data();
			if (!locator_data.empty()) {
				hit_obj = MsgPack::unserialise(locator_data);
			}
		}

		// Detailed info about the document:
		if (hit_obj.find(ID_FIELD_NAME) == hit_obj.end()) {
			hit_obj[ID_FIELD_NAME] = document.get_value(ID_FIELD_NAME);
		}

		if (hit_obj.find(VERSION_FIELD_NAME) == hit_obj.end()) {
			auto version = document.get_value(DB_SLOT_VERSION);
			if (!version.empty()) {
				hit_obj[VERSION_FIELD_NAME] = static_cast<Xapian::rev>(sortable_unserialise(version));
			}
		}

		if (request.comments) {
			hit_obj[RESPONSE_xDOCID] = did;

			size_t n_shards = endpoints.size();
			size_t shard_num = (did - 1) % n_shards;
			hit_obj[RESPONSE_xSHARD] = shard_num + 1;
			// hit_obj[RESPONSE_xENDPOINT] = endpoints[shard_num].to_string();

			hit_obj[RESPONSE_xRANK] = m.get_rank();
			hit_obj[RESPONSE_xWEIGHT] = m.get_weight();
			hit_obj[RESPONSE_xPERCENT] = m.get_percent();
		}

		if (!selector.empty()) {
			hit_obj = hit_obj.select(selector);
		}

		hits.append(hit_obj);
	}

	request.ready = std::chrono::steady_clock::now();
	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Searching took {}", strings::from_delta(took));

	if (request.human) {
		obj[RESPONSE_TOOK] = strings::from_delta(took);
	} else {
		obj[RESPONSE_TOOK] = took / 1e9;
	}

	write_http_response(request, HTTP_STATUS_OK, obj);

	if (aggregations) {
		Metrics::metrics()
			.xapiand_operations_summary
			.Add({
				{"operation", "aggregation"},
			})
			.Observe(took / 1e9);
	} else {
		Metrics::metrics()
			.xapiand_operations_summary
			.Add({
				{"operation", "search"},
			})
			.Observe(took / 1e9);
	}

	L_SEARCH("FINISH SEARCH");
}


void
HttpClient::count_view(Request& request)
{
	L_CALL("HttpClient::count_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_SEARCH);
	resolve_index_endpoints(request, query_field);

	std::tuple<Xapian::MSet, MsgPack, Xapian::Query> mset_tuple;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	try {
		if (query_field.primary) {
			db_handler.reset(endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(endpoints, DB_OPEN);
		}

		if (request.raw.empty()) {
			mset_tuple = db_handler.get_mset(query_field, nullptr, nullptr);
		} else {
			auto& decoded_body = request.decoded_body();
			if (!decoded_body.is_map()) {
				THROW(ClientError, "Invalid request body");
			}
			mset_tuple = db_handler.get_mset(query_field, &decoded_body, nullptr);
		}
	} catch (const Xapian::DatabaseNotFoundError&) {
		/* At the moment when the endpoint does not exist and it is chunck it will return 200 response
		 * with zero matches this behavior may change in the future for instance ( return 404 ) */
	}

	auto& mset = std::get<0>(mset_tuple);
	auto& qdsl = std::get<1>(mset_tuple);
	auto& query = std::get<2>(mset_tuple);

	MsgPack obj;
	if (request.comments && opts.verbosity >= 3) {
		obj["#query"] = qdsl;
		obj["#xapian_query"] = query.get_description();
	}
	obj[RESPONSE_TOTAL] = mset.get_matches_estimated();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);
}


void
HttpClient::write_status_response(Request& request, enum http_status status, const std::string& message)
{
	L_CALL("HttpClient::write_status_response()");

	MsgPack response({
		{ RESPONSE_STATUS, static_cast<unsigned>(status) },
		{ RESPONSE_TYPE, http_status_str(status) },
	});
	if (!message.empty()) {
		response[RESPONSE_MESSAGE] = message;
	}
	write_http_response(request, status, response);
}


void
HttpClient::url_resolve(Request& request)
{
	L_CALL("HttpClient::url_resolve(request)");

	struct http_parser_url u;
	std::string b = repr(request.path, true, 0);

	L_HTTP("URL: {}", b);

	if (http_parser_parse_url(request.path.data(), request.path.size(), 0, &u) != 0) {
		L_HTTP_PROTO("Parsing not done");
		THROW(ClientError, "Invalid HTTP");
	}

	L_HTTP_PROTO("HTTP parsing done!");

	if ((u.field_set & (1 << UF_PATH )) != 0) {
		if (request.path_parser.init(std::string_view(request.path.data() + u.field_data[3].off, u.field_data[3].len)) >= PathParser::State::END) {
			THROW(ClientError, "Invalid path");
		}
	}

	if ((u.field_set & (1 <<  UF_QUERY)) != 0) {
		if (request.query_parser.init(std::string_view(b.data() + u.field_data[4].off, u.field_data[4].len)) < 0) {
			THROW(ClientError, "Invalid query");
		}
	}

	bool pretty = !opts.no_pretty && (opts.pretty || opts.verbosity >= 4);
	request.query_parser.rewind();
	if (request.query_parser.next("pretty") != -1) {
		if (request.query_parser.len != 0u) {
			try {
				pretty = Serialise::boolean(request.query_parser.get()) == "t";
				request.indented = pretty ? DEFAULT_INDENTATION : -1;
			} catch (const Exception&) { }
		} else {
			if (request.indented == -1) {
				request.indented = DEFAULT_INDENTATION;
			}
		}
	} else {
		if (pretty && request.indented == -1) {
			request.indented = DEFAULT_INDENTATION;
		}
	}

	bool human = !opts.no_human && (opts.human || opts.verbosity >= 4 || pretty);
	request.query_parser.rewind();
	if (request.query_parser.next("human") != -1) {
		if (request.query_parser.len != 0u) {
			try {
				request.human = Serialise::boolean(request.query_parser.get()) == "t" ? true : false;
			} catch (const Exception&) { }
		} else {
			request.human = true;
		}
	} else {
		request.human = human;
	}

	bool echo = !opts.no_echo && (opts.echo || opts.verbosity >= 4);
	request.query_parser.rewind();
	if (request.query_parser.next("echo") != -1) {
		if (request.query_parser.len != 0u) {
			try {
				request.echo = Serialise::boolean(request.query_parser.get()) == "t" ? true : false;
			} catch (const Exception&) { }
		} else {
			request.echo = true;
		}
	} else {
		request.echo = echo;
	}

	bool comments = !opts.no_comments && (opts.comments || opts.verbosity >= 4);
	request.query_parser.rewind();
	if (request.query_parser.next("comments") != -1) {
		if (request.query_parser.len != 0u) {
			try {
				request.comments = Serialise::boolean(request.query_parser.get()) == "t" ? true : false;
			} catch (const Exception&) { }
		} else {
			request.comments = true;
		}
	} else {
		request.comments = comments;
	}
}


size_t
HttpClient::resolve_index_endpoints(Request& request, const query_field_t& query_field, const MsgPack* settings)
{
	L_CALL("HttpClient::resolve_index_endpoints(<request>, <query_field>, <settings>)");

	auto paths = expand_paths(request);

	endpoints.clear();
	for (const auto& path : paths) {
		auto index_endpoints = XapiandManager::resolve_index_endpoints(
			Endpoint(path),
			query_field.writable,
			query_field.primary,
			settings);
		if (index_endpoints.empty()) {
			throw Xapian::NetworkError("Endpoint node not available");
		}
		for (auto& endpoint : index_endpoints) {
			endpoints.add(endpoint);
		}
	}
	L_HTTP("Endpoint: -> {}", endpoints.to_string());

	return paths.size();
}


std::vector<std::string>
HttpClient::expand_paths(Request& request)
{
	L_CALL("HttpClient::expand_paths(<request>)");

	std::vector<std::string> paths;

	request.path_parser.rewind();

	PathParser::State state;
	while ((state = request.path_parser.next()) < PathParser::State::END) {
		std::string index_path;

		auto pth = request.path_parser.get_pth();
		if (strings::startswith(pth, '/')) {
			pth.remove_prefix(1);
		}
		index_path.append(pth);


#ifdef XAPIAND_CLUSTERING
		Xapian::MSet mset;
		if (strings::endswith(index_path, '*')) {
			index_path.pop_back();
			auto stripped_index_path = index_path;
			if (strings::endswith(stripped_index_path, '/')) {
				stripped_index_path.pop_back();
			}
			Endpoints index_endpoints;
			for (auto& node : Node::nodes()) {
				index_endpoints.add(Endpoint(strings::format(".xapiand/nodes/{}", node->lower_name())));
			}
			DatabaseHandler db_handler;
			db_handler.reset(index_endpoints);
			if (stripped_index_path.empty()) {
				mset = db_handler.get_mset(Xapian::Query(std::string()), 0, 100);
			} else {
				auto query = Xapian::Query(Xapian::Query::OP_AND_NOT,
					Xapian::Query(Xapian::Query::OP_OR,
							Xapian::Query(Xapian::Query::OP_WILDCARD, Xapian::Query(prefixed(index_path, DOCUMENT_ID_TERM_PREFIX, KEYWORD_CHAR))),
							Xapian::Query(prefixed(stripped_index_path, DOCUMENT_ID_TERM_PREFIX, KEYWORD_CHAR))),
					Xapian::Query(Xapian::Query::OP_WILDCARD, Xapian::Query(prefixed(index_path + "/.", DOCUMENT_ID_TERM_PREFIX, KEYWORD_CHAR)))
				);
				mset = db_handler.get_mset(query, 0, 100);
			}
			const auto m_e = mset.end();
			for (auto m = mset.begin(); m != m_e; ++m) {
				auto document = db_handler.get_document(*m);
				index_path = document.get_value(DB_SLOT_ID);
				paths.push_back(std::move(index_path));
			}
		} else {
#endif
			paths.push_back(std::move(index_path));
#ifdef XAPIAND_CLUSTERING
		}
#endif
	}

	return paths;
}


query_field_t
HttpClient::query_field_maker(Request& request, int flags)
{
	L_CALL("HttpClient::query_field_maker(<request>, <flags>)");

	query_field_t query_field;

	if ((flags & QUERY_FIELD_WRITABLE) != 0) {
		query_field.writable = true;
	}

	if ((flags & QUERY_FIELD_PRIMARY) != 0) {
		query_field.primary = true;
	}

	if ((flags & QUERY_FIELD_COMMIT) != 0) {
		request.query_parser.rewind();
		if (request.query_parser.next("commit") != -1) {
			query_field.commit = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.commit = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}

		request.query_parser.rewind();
		if (request.query_parser.next("version") != -1) {
			query_field.version = strict_stou(nullptr, request.query_parser.get());
		}
	}

	if ((flags & QUERY_FIELD_VOLATILE) != 0) {
		request.query_parser.rewind();
		if (request.query_parser.next("volatile") != -1) {
			query_field.primary = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.primary = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
	}

	if (((flags & QUERY_FIELD_ID) != 0) || ((flags & QUERY_FIELD_SEARCH) != 0)) {
		request.query_parser.rewind();
		if (request.query_parser.next("check_at_least") != -1) {
			query_field.check_at_least = strict_stou(nullptr, request.query_parser.get());
		}
	}

	if (((flags & QUERY_FIELD_ID) != 0) || ((flags & QUERY_FIELD_SEARCH) != 0) || ((flags & QUERY_FIELD_OFFSET) != 0)) {
		request.query_parser.rewind();
		if (request.query_parser.next("offset") != -1) {
			query_field.offset = strict_stou(nullptr, request.query_parser.get());
		}

		request.query_parser.rewind();
		if (request.query_parser.next("limit") != -1) {
			query_field.limit = strict_stou(nullptr, request.query_parser.get());
		} else if ((flags & QUERY_FIELD_OFFSET) == 0) {
			query_field.limit = 10;  // Default is to limit to 10 items
		}
	}

	if ((flags & QUERY_FIELD_SEARCH) != 0) {
		request.query_parser.rewind();
		if (request.query_parser.next("spelling") != -1) {
			query_field.spelling = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.spelling = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}

		request.query_parser.rewind();
		if (request.query_parser.next("synonyms") != -1) {
			query_field.synonyms = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.synonyms = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}

		request.query_parser.rewind();
		while (request.query_parser.next("query") != -1) {
			L_SEARCH("query={}", request.query_parser.get());
			query_field.query.emplace_back(request.query_parser.get());
		}

		request.query_parser.rewind();
		while (request.query_parser.next("q") != -1) {
			L_SEARCH("query={}", request.query_parser.get());
			query_field.query.emplace_back(request.query_parser.get());
		}

		request.query_parser.rewind();
		while (request.query_parser.next("sort") != -1) {
			query_field.sort.emplace_back(request.query_parser.get());
		}

		request.query_parser.rewind();
		if (request.query_parser.next("metric") != -1) {
			query_field.metric = request.query_parser.get();
		}

		request.query_parser.rewind();
		if (request.query_parser.next("icase") != -1) {
			query_field.icase = Serialise::boolean(request.query_parser.get()) == "t";
		}

		request.query_parser.rewind();
		if (request.query_parser.next("collapse_max") != -1) {
			query_field.collapse_max = strict_stou(nullptr, request.query_parser.get());
		}

		request.query_parser.rewind();
		if (request.query_parser.next("collapse") != -1) {
			query_field.collapse = request.query_parser.get();
		}

		request.query_parser.rewind();
		if (request.query_parser.next("fuzzy") != -1) {
			query_field.is_fuzzy = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.is_fuzzy = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}

		if (query_field.is_fuzzy) {
			request.query_parser.rewind();
			if (request.query_parser.next("fuzzy.n_rset") != -1) {
				query_field.fuzzy.n_rset = strict_stou(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			if (request.query_parser.next("fuzzy.n_eset") != -1) {
				query_field.fuzzy.n_eset = strict_stou(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			if (request.query_parser.next("fuzzy.n_term") != -1) {
				query_field.fuzzy.n_term = strict_stou(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			while (request.query_parser.next("fuzzy.field") != -1) {
				query_field.fuzzy.field.emplace_back(request.query_parser.get());
			}

			request.query_parser.rewind();
			while (request.query_parser.next("fuzzy.type") != -1) {
				query_field.fuzzy.type.emplace_back(request.query_parser.get());
			}
		}

		request.query_parser.rewind();
		if (request.query_parser.next("nearest") != -1) {
			query_field.is_nearest = true;
			if (request.query_parser.len != 0u) {
				try {
					query_field.is_nearest = Serialise::boolean(request.query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}

		if (query_field.is_nearest) {
			query_field.nearest.n_rset = 5;
			request.query_parser.rewind();
			if (request.query_parser.next("nearest.n_rset") != -1) {
				query_field.nearest.n_rset = strict_stoul(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			if (request.query_parser.next("nearest.n_eset") != -1) {
				query_field.nearest.n_eset = strict_stoul(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			if (request.query_parser.next("nearest.n_term") != -1) {
				query_field.nearest.n_term = strict_stoul(nullptr, request.query_parser.get());
			}

			request.query_parser.rewind();
			while (request.query_parser.next("nearest.field") != -1) {
				query_field.nearest.field.emplace_back(request.query_parser.get());
			}

			request.query_parser.rewind();
			while (request.query_parser.next("nearest.type") != -1) {
				query_field.nearest.type.emplace_back(request.query_parser.get());
			}
		}
	}

	if ((flags & QUERY_FIELD_TIME) != 0) {
		request.query_parser.rewind();
		if (request.query_parser.next("time") != -1) {
			query_field.time = request.query_parser.get();
		} else {
			query_field.time = "1h";
		}
	}

	if ((flags & QUERY_FIELD_PERIOD) != 0) {
		request.query_parser.rewind();
		if (request.query_parser.next("period") != -1) {
			query_field.period = request.query_parser.get();
		} else {
			query_field.period = "1m";
		}
	}

	request.query_parser.rewind();
	if (request.query_parser.next("selector") != -1) {
		query_field.selector = request.query_parser.get();
	}

	return query_field;
}


void
HttpClient::log_request(Request& request)
{
	L_CALL("HttpClient::log_request()");

	std::string request_prefix = " 🌎  ";
	int priority = LOG_DEBUG;
	if ((int)request.response.status >= 400 && (int)request.response.status <= 499) {
		priority = LOG_INFO;
	} else if ((int)request.response.status >= 500 && (int)request.response.status <= 599) {
		priority = LOG_NOTICE;
	}
	auto request_text = request.to_text(true);
	L(priority, NO_COLOR, "{}{}", request_prefix, strings::indent(request_text, ' ', 4, false));
}


void
HttpClient::log_response(Response& response)
{
	L_CALL("HttpClient::log_response()");

	std::string response_prefix = " 💊  ";
	int priority = LOG_DEBUG;
	if ((int)response.status >= 300 && (int)response.status <= 399) {
		response_prefix = " 💫  ";
	} else if ((int)response.status == 404) {
		response_prefix = " 🕸  ";
	} else if ((int)response.status >= 400 && (int)response.status <= 499) {
		response_prefix = " 💥  ";
		priority = LOG_INFO;
	} else if ((int)response.status >= 500 && (int)response.status <= 599) {
		response_prefix = " 🔥  ";
		priority = LOG_NOTICE;
	}
	auto response_text = response.to_text(true);
	L(priority, NO_COLOR, "{}{}", response_prefix, strings::indent(response_text, ' ', 4, false));
}


void
HttpClient::end_http_request(Request& request)
{
	L_CALL("HttpClient::end_http_request()");

	request.ends = std::chrono::steady_clock::now();
	request.atom_ending = true;
	request.atom_ended = true;
	waiting = false;

	auto indexer = request.indexer.load();
	if (indexer) {
		indexer->finish();
		request.indexer.store(nullptr);
	}

	if (request.log) {
		request.log->clear();
		request.log.reset();
	}

	if (request.parser.http_errno != 0u) {
		L(LOG_ERR, LIGHT_RED, "HTTP parsing error ({}): {}", http_errno_name(HTTP_PARSER_ERRNO(&request.parser)), http_errno_description(HTTP_PARSER_ERRNO(&request.parser)));
	} else {
		static constexpr auto fmt_defaut = RED + "\"{}\" {} {} {}";
		auto fmt = fmt_defaut.c_str();
		int priority = LOG_DEBUG;

		if ((int)request.response.status >= 200 && (int)request.response.status <= 299) {
			static constexpr auto fmt_2xx = LIGHT_GREY + "\"{}\" {} {} {}";
			fmt = fmt_2xx.c_str();
		} else if ((int)request.response.status >= 300 && (int)request.response.status <= 399) {
			static constexpr auto fmt_3xx = STEEL_BLUE + "\"{}\" {} {} {}";
			fmt = fmt_3xx.c_str();
		} else if ((int)request.response.status >= 400 && (int)request.response.status <= 499) {
			static constexpr auto fmt_4xx = SADDLE_BROWN + "\"{}\" {} {} {}";
			fmt = fmt_4xx.c_str();
			if ((int)request.response.status != 404) {
				priority = LOG_INFO;
			}
		} else if ((int)request.response.status >= 500 && (int)request.response.status <= 599) {
			static constexpr auto fmt_5xx = LIGHT_PURPLE + "\"{}\" {} {} {}";
			fmt = fmt_5xx.c_str();
			priority = LOG_NOTICE;
		}
		if (Logging::log_level > LOG_DEBUG) {
			log_response(request.response);
		} else if (Logging::log_level == LOG_DEBUG) {
			if ((int)request.response.status >= 400 && (int)request.response.status != 404) {
				log_request(request);
				log_response(request.response);
			}
		}

		auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count();
		Metrics::metrics()
			.xapiand_http_requests_summary
			.Add({
				{"method", http_method_str(request.method)},
				{"status", strings::format("{}", request.response.status)},
			})
			.Observe(took / 1e9);

		L(priority, NO_COLOR, fmt, request.head(), (int)request.response.status, strings::from_bytes(request.response.size), strings::from_delta(request.begins, request.ends));
	}

	L_TIME("Full request took {}, response took {}", strings::from_delta(request.begins, request.ends), strings::from_delta(request.received, request.ends));

	auto sent = total_sent_bytes.exchange(0);
	Metrics::metrics()
		.xapiand_http_sent_bytes
		.Increment(sent);

	auto received = total_received_bytes.exchange(0);
	Metrics::metrics()
		.xapiand_http_received_bytes
		.Increment(received);
}


const ct_type_t&
HttpClient::resolve_ct_type(Request& request, const std::vector<const ct_type_t*>& ct_types)
{
	L_CALL("HttpClient::resolve_ct_type(<request>, <ct_types>)");

	const auto& acceptable_type = get_acceptable_type(request, ct_types);
	auto accepted_ct_type = is_acceptable_type(acceptable_type, ct_types);
	if (accepted_ct_type == nullptr) {
		accepted_ct_type = &no_type;
	}

	return *accepted_ct_type;
}


const ct_type_t&
HttpClient::resolve_ct_type(Request& request, const ct_type_t& ct_type)
{
	L_CALL("HttpClient::resolve_ct_type(<request>, {})", repr(ct_type.to_string()));

	std::vector<const ct_type_t*> ct_types;
	if (!ct_type.empty()) {
		if (ct_type == json_type || ct_type == x_json_type) {
			ct_types.push_back(&json_type);
			ct_types.push_back(&x_json_type);
		} else if (ct_type == yaml_type || ct_type == x_yaml_type) {
			ct_types.push_back(&yaml_type);
			ct_types.push_back(&x_yaml_type);
		} else if (ct_type == msgpack_type || ct_type == x_msgpack_type) {
			ct_types.push_back(&msgpack_type);
			ct_types.push_back(&x_msgpack_type);
		} else if (ct_type == ndjson_type || ct_type == x_ndjson_type) {
			ct_types.push_back(&ndjson_type);
			ct_types.push_back(&x_ndjson_type);
		} else {
			ct_types.push_back(&ct_type);
		}
	} else {
		ct_types = msgpack_serializers;
	}

	return resolve_ct_type(request, ct_types);
}


const ct_type_t*
HttpClient::is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t* ct_type)
{
	L_CALL("HttpClient::is_acceptable_type({}, {})", repr(ct_type_pattern.to_string()), repr(ct_type->to_string()));

	bool type_ok = false, subtype_ok = false;
	if (ct_type_pattern.first == "*") {
		type_ok = true;
	} else {
		type_ok = ct_type_pattern.first == ct_type->first;
	}
	if (ct_type_pattern.second == "*") {
		subtype_ok = true;
	} else {
		subtype_ok = ct_type_pattern.second == ct_type->second;
	}
	if (type_ok && subtype_ok) {
		return ct_type;
	}
	return nullptr;
}


const ct_type_t*
HttpClient::is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<const ct_type_t*>& ct_types)
{
	L_CALL("HttpClient::is_acceptable_type(({}, <ct_types>)", repr(ct_type_pattern.to_string()));

	for (auto ct_type : ct_types) {
		if (is_acceptable_type(ct_type_pattern, ct_type) != nullptr) {
			return ct_type;
		}
	}
	return nullptr;
}


template <typename T>
const ct_type_t&
HttpClient::get_acceptable_type(Request& request, const T& ct)
{
	L_CALL("HttpClient::get_acceptable_type()");

	if (request.accept_set.empty()) {
		return no_type;
	}
	for (const auto& accept : request.accept_set) {
		if (is_acceptable_type(accept.ct_type, ct)) {
			return accept.ct_type;
		}
	}
	const auto& accept = *request.accept_set.begin();
	auto indent = accept.indent;
	if (indent != -1) {
		request.indented = indent;
	}
	return accept.ct_type;
}


std::pair<std::string, std::string>
HttpClient::serialize_response(const MsgPack& obj, const ct_type_t& ct_type, int indent, bool serialize_error)
{
	L_CALL("HttpClient::serialize_response({}, {}, {}, {})", repr(obj.to_string(), true, '\'', 200), repr(ct_type.to_string()), indent, serialize_error);

	if (ct_type == no_type) {
		return std::make_pair("", "");
	}
	if (ct_type == json_type) {
		return std::make_pair(obj.to_string(indent), json_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == x_json_type) {
		return std::make_pair(obj.to_string(indent), x_json_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == yaml_type) {
		return std::make_pair(obj.to_string(indent), yaml_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == x_yaml_type) {
		return std::make_pair(obj.to_string(indent), x_yaml_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == msgpack_type) {
		return std::make_pair(obj.serialise(), msgpack_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == x_msgpack_type) {
		return std::make_pair(obj.serialise(), x_msgpack_type.to_string() + "; charset=utf-8");
	}
	if (ct_type == html_type) {
		std::function<std::string(const msgpack::object&)> html_serialize = serialize_error ? msgpack_to_html_error : msgpack_to_html;
		return std::make_pair(obj.external(html_serialize), html_type.to_string() + "; charset=utf-8");
	}
	/*if (ct_type == text_type)) {
		error:
			{{ ERROR_CODE }} - {{ MESSAGE }}

		obj:
			{{ key1 }}: {{ val1 }}
			{{ key2 }}: {{ val2 }}
			...

		array:
			{{ val1 }}, {{ val2 }}, ...
	}*/
	THROW(SerialisationError, "Type is not serializable");
}


void
HttpClient::write_http_response(Request& request, enum http_status status, const MsgPack& obj, const std::string& location, const ct_type_t& ct_type)
{
	L_CALL("HttpClient::write_http_response()");

	if (obj.is_undefined()) {
		write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location));
		return;
	}

	auto& resolved_ct_type = ct_type.empty() ? resolve_ct_type(request) : ct_type;

	if (status == HTTP_STATUS_NOT_ACCEPTABLE) {
		if (resolved_ct_type.empty()) {
			write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location));
			return;
		}
	}

	try {
		auto result = serialize_response(obj, resolved_ct_type, request.indented, (int)status >= 400);
		if (Logging::log_level >= LOG_DEBUG && request.response.size <= 1024 * 10) {
			if (resolved_ct_type == json_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == x_json_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == yaml_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == x_yaml_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == msgpack_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == x_msgpack_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == html_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (resolved_ct_type == text_type) {
				request.response.text.append(obj.to_string(DEFAULT_INDENTATION));
			} else if (!obj.empty()) {
				request.response.text.append("...");
			}
		}
		if (request.type_encoding != Encoding::none) {
			auto encoded = encoding_http_response(request.response, request.type_encoding, result.first, false, true, true);
			if (!encoded.empty() && encoded.size() <= result.first.size()) {
				write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, encoded, location, result.second, readable_encoding(request.type_encoding)));
			} else {
				write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, result.first, location, result.second, readable_encoding(Encoding::identity)));
			}
		} else {
			write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE, result.first, location, result.second));
		}
	} catch (const SerialisationError& exc) {
		if (status == HTTP_STATUS_NOT_ACCEPTABLE) {
			write(http_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location));
		} else {
			write_http_response(request, HTTP_STATUS_NOT_ACCEPTABLE, MsgPack({
				{ RESPONSE_STATUS, static_cast<unsigned>(HTTP_STATUS_NOT_ACCEPTABLE) },
				{ RESPONSE_TYPE, http_status_str(HTTP_STATUS_NOT_ACCEPTABLE) },
				{ RESPONSE_MESSAGE, { MsgPack({ exc.what() }) } }
			}), location);
		}
		return;
	}
}


Encoding
HttpClient::resolve_encoding(Request& request)
{
	L_CALL("HttpClient::resolve_encoding()");

	if (request.accept_encoding_set.empty()) {
		return Encoding::none;
	}

	constexpr static auto _ = phf::make_phf({
		hhl("gzip"),
		hhl("deflate"),
		hhl("identity"),
		hhl("*"),
	});
	for (const auto& encoding : request.accept_encoding_set) {
		switch (_.fhhl(encoding.encoding)) {
			case _.fhhl("gzip"):
				return Encoding::gzip;
			case _.fhhl("deflate"):
				return Encoding::deflate;
			case _.fhhl("identity"):
				return Encoding::identity;
			case _.fhhl("*"):
				return Encoding::identity;
			default:
				continue;
		}
	}
	return Encoding::unknown;
}


std::string
HttpClient::readable_encoding(Encoding e)
{
	L_CALL("Request::readable_encoding()");

	switch (e) {
		case Encoding::none:
			return "none";
		case Encoding::gzip:
			return "gzip";
		case Encoding::deflate:
			return "deflate";
		case Encoding::identity:
			return "identity";
		default:
			return "Encoding:UNKNOWN";
	}
}


std::string
HttpClient::encoding_http_response(Response& response, Encoding e, const std::string& response_obj, bool chunk, bool start, bool end)
{
	L_CALL("HttpClient::encoding_http_response({})", repr(response_obj));

	bool gzip = false;
	switch (e) {
		case Encoding::gzip:
			gzip = true;
			[[fallthrough]];
		case Encoding::deflate: {
			if (chunk) {
				if (start) {
					response.encoding_compressor.reset(nullptr, 0, gzip);
					response.encoding_compressor.begin();
				}
				if (end) {
					auto ret = response.encoding_compressor.next(response_obj.data(), response_obj.size(), DeflateCompressData::FINISH_COMPRESS);
					return ret;
				}
				auto ret = response.encoding_compressor.next(response_obj.data(), response_obj.size());
				return ret;
			}

			response.encoding_compressor.reset(response_obj.data(), response_obj.size(), gzip);
			response.it_compressor = response.encoding_compressor.begin();
			std::string encoding_respose;
			while (response.it_compressor) {
				encoding_respose.append(*response.it_compressor);
				++response.it_compressor;
			}
			return encoding_respose;
		}

		case Encoding::identity:
			return response_obj;

		default:
			return std::string();
	}
}


std::string
HttpClient::__repr__() const
{
	return strings::format(STEEL_BLUE + "<HttpClient {{cnt:{}, sock:{}}}{}{}{}{}{}{}{}{}>",
		use_count(),
		sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "",
		is_idle() ? " " + DARK_STEEL_BLUE + "(idle)" + STEEL_BLUE : "",
		is_waiting() ? " " + LIGHT_STEEL_BLUE + "(waiting)" + STEEL_BLUE : "",
		is_running() ? " " + DARK_ORANGE + "(running)" + STEEL_BLUE : "",
		is_shutting_down() ? " " + ORANGE + "(shutting down)" + STEEL_BLUE : "",
		is_closed() ? " " + ORANGE + "(closed)" + STEEL_BLUE : "");
}


Request::Request(HttpClient* client) :
	mode(Mode::FULL),
	client(client),
	view(nullptr),
	type_encoding(Encoding::none),
	begining(true),
	ending(false),
	atom_ending(false),
	atom_ended(false),
	raw_peek(0),
	raw_offset(0),
	size(0),
	echo(false),
	human(false),
	comments(true),
	indented(-1),
	expect_100(false),
	closing(false),
	begins(std::chrono::steady_clock::now())
{
	parser.data = client;
	http_parser_init(&parser, HTTP_REQUEST);
}


Request::~Request() noexcept
{
	try {
		auto indexer_ = indexer.load();
		if (indexer_) {
			indexer_->finish();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}

	try {
		if (log) {
			log->clear();
			log.reset();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


MsgPack
Request::decode(std::string_view body)
{
	L_CALL("Request::decode({})", repr(body));

	std::string ct_type_str = ct_type.to_string();
	if (ct_type_str.empty()) {
		ct_type_str = JSON_CONTENT_TYPE;
	}

	MsgPack decoded;
	rapidjson::Document rdoc;

	constexpr static auto _ = phf::make_phf({
		hhl(JSON_CONTENT_TYPE),
		hhl(X_JSON_CONTENT_TYPE),
		hhl(YAML_CONTENT_TYPE),
		hhl(X_YAML_CONTENT_TYPE),
		hhl(MSGPACK_CONTENT_TYPE),
		hhl(X_MSGPACK_CONTENT_TYPE),
		hhl(NDJSON_CONTENT_TYPE),
		hhl(X_NDJSON_CONTENT_TYPE),
		hhl(FORM_URLENCODED_CONTENT_TYPE),
		hhl(X_FORM_URLENCODED_CONTENT_TYPE),
	});
	switch (_.fhhl(ct_type_str)) {
		case _.fhhl(NDJSON_CONTENT_TYPE):
		case _.fhhl(X_NDJSON_CONTENT_TYPE):
			decoded = MsgPack::ARRAY();
			for (auto json : Split<std::string_view>(body, '\n')) {
				json_load(rdoc, json);
				decoded.append(rdoc);
			}
			ct_type = json_type;
			return decoded;
		case _.fhhl(JSON_CONTENT_TYPE):
		case _.fhhl(X_JSON_CONTENT_TYPE):
			json_load(rdoc, body);
			decoded = MsgPack(rdoc);
			ct_type = json_type;
			return decoded;
		case _.fhhl(YAML_CONTENT_TYPE):
		case _.fhhl(X_YAML_CONTENT_TYPE):
			yaml_load(rdoc, body);
			decoded = MsgPack(rdoc);
			ct_type = yaml_type;
			return decoded;
		case _.fhhl(MSGPACK_CONTENT_TYPE):
		case _.fhhl(X_MSGPACK_CONTENT_TYPE):
			decoded = MsgPack::unserialise(body);
			ct_type = msgpack_type;
			return decoded;
		case _.fhhl(FORM_URLENCODED_CONTENT_TYPE):
		case _.fhhl(X_FORM_URLENCODED_CONTENT_TYPE):
			try {
				json_load(rdoc, body);
				decoded = MsgPack(rdoc);
				ct_type = json_type;
			} catch (const std::exception&) {
				decoded = MsgPack(body);
				ct_type = msgpack_type;
			}
			return decoded;
		default:
			decoded = MsgPack(body);
			return decoded;
	}
}


MsgPack&
Request::decoded_body()
{
	L_CALL("Request::decoded_body()");

	if (_decoded_body.is_undefined()) {
		if (!raw.empty()) {
			_decoded_body = decode(raw);
		}
	}
	return _decoded_body;
}


bool
Request::append(const char* at, size_t length)
{
	L_CALL("Request::append(<at>, <length>)");

	bool signal_pending = false;

	switch (mode) {
		case Mode::FULL:
			raw.append(std::string_view(at, length));
			break;

		case Mode::STREAM:
			assert((parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH);

			raw.append(std::string_view(at, length));
			signal_pending = true;
			break;

		case Mode::STREAM_NDJSON:
			assert((parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH);

			if (length) {
				raw.append(std::string_view(at, length));

				auto new_raw_offset = raw.find_first_of('\n', raw_peek);
				while (new_raw_offset != std::string::npos) {
					auto json = std::string_view(raw).substr(raw_offset, new_raw_offset - raw_offset);
					raw_offset = raw_peek = new_raw_offset + 1;
					new_raw_offset = raw.find_first_of('\n', raw_peek);
					if (!json.empty()) {
						try {
							rapidjson::Document rdoc;
							json_load(rdoc, json);
							signal_pending = true;
							std::lock_guard<std::mutex> lk(objects_mtx);
							objects.emplace_back(rdoc);
						} catch (const std::exception&) {
							L_EXC("Cannot load object");
						}
					}
				}
			}

			if (!length) {
				auto json = std::string_view(raw).substr(raw_offset);
				raw_offset = raw_peek = raw.size();
				if (!json.empty()) {
					try {
						rapidjson::Document rdoc;
						json_load(rdoc, json);
						signal_pending = true;
						std::lock_guard<std::mutex> lk(objects_mtx);
						objects.emplace_back(rdoc);
					} catch (const std::exception&) {
						L_EXC("Cannot load object");
					}
				}
			}

			break;

		case Mode::STREAM_MSGPACK:
			assert((parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH);

			if (length) {
				unpacker.reserve_buffer(length);
				memcpy(unpacker.buffer(), at, length);
				unpacker.buffer_consumed(length);

				try {
					msgpack::object_handle result;
					while (unpacker.next(result)) {
						signal_pending = true;
						std::lock_guard<std::mutex> lk(objects_mtx);
						objects.emplace_back(result.get());
					}
				} catch (const std::exception&) {
					L_EXC("Cannot load object");
				}
			}

			break;
	}

	return signal_pending;
}

bool
Request::wait()
{
	if (mode != Request::Mode::FULL) {
		// Wait for a pending raw body for one second and clear pending
		// flag before processing the request, otherwise retry
		// checking for empty/ended requests or closed connections.
		std::unique_lock<std::mutex> pending_lk(pending_mtx);
		while (!pending.wait_for(pending_lk, 1s, [this]() {
			return has_pending || (client && client->is_closed());
		})) {}
		has_pending = false;
	}
	return true;
}

bool
Request::next(std::string_view& str_view)
{
	L_CALL("Request::next(<&str_view>)");

	assert((parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH);
	assert(mode == Mode::STREAM);

	if (raw_offset == raw.size()) {
		return false;
	}
	str_view = std::string_view(raw).substr(raw_offset);
	raw_offset = raw.size();
	return true;
}


bool
Request::next_object(MsgPack& obj)
{
	L_CALL("Request::next_object(<&obj>)");

	assert((parser.flags & F_CONTENTLENGTH) == F_CONTENTLENGTH);
	assert(mode == Mode::STREAM_MSGPACK || mode == Mode::STREAM_NDJSON);

	std::lock_guard<std::mutex> lk(objects_mtx);
	if (objects.empty()) {
		return false;
	}
	obj = std::move(objects.front());
	objects.pop_front();
	return true;
}


std::string
Request::head()
{
	L_CALL("Request::head()");

	auto parser_method = HTTP_PARSER_METHOD(&parser);

	if (parser_method != method) {
		return strings::format("{} <- {} {} HTTP/{}.{}",
			http_method_str(method),
			http_method_str(parser_method),
			path, parser.http_major,
			parser.http_minor);
	}

	return strings::format("{} {} HTTP/{}.{}",
		http_method_str(parser_method),
		path, parser.http_major,
		parser.http_minor);
}


std::string
Request::to_text(bool decode)
{
	L_CALL("Request::to_text({})", decode);

	static constexpr auto no_col = NO_COLOR;
	auto request_headers_color = no_col.c_str();
	auto request_head_color = no_col.c_str();
	auto request_text_color = no_col.c_str();

	switch (method) {
		case HTTP_OPTIONS:
		case HTTP_INFO:
		case HTTP_HEAD:
		{
			// rgb(144, 18, 254)
			static constexpr auto _request_headers_color = rgba(100, 64, 131, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(100, 64, 131);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(100, 64, 131);
			request_text_color = _request_text_color.c_str();
			break;
		}
		case HTTP_GET:
		case HTTP_SEARCH:
		case HTTP_COUNT:
		case HTTP_DUMP:
		{
			// rgb(101, 177, 251)
			static constexpr auto _request_headers_color = rgba(34, 113, 191, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(34, 113, 191);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(34, 113, 191);
			request_text_color = _request_text_color.c_str();
			break;
		}
		case HTTP_POST:
		case HTTP_RESTORE:
		case HTTP_PATCH:
		case HTTP_UPDATE:
		case HTTP_UPSERT:
		case HTTP_PUT:
		{
			// rgb(250, 153, 63)
			static constexpr auto _request_headers_color = rgba(158, 90, 28, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(158, 90, 28);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(158, 90, 28);
			request_text_color = _request_text_color.c_str();
			break;
		}
		case HTTP_DELETE:
		{
			// rgb(250, 104, 63)
			static constexpr auto _request_headers_color = rgba(158, 56, 28, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(158, 56, 28);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(158, 56, 28);
			request_text_color = _request_text_color.c_str();
			break;
		}
		case HTTP_COMMIT:
		{
			// rgb(88, 226, 194)
			static constexpr auto _request_headers_color = rgba(51, 136, 116, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(51, 136, 116);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(51, 136, 116);
			request_text_color = _request_text_color.c_str();
			break;
		}
		case HTTP_OPEN:
		case HTTP_CLOSE:
		{
			// rgb(250, 63, 125)
			static constexpr auto _request_headers_color = rgba(158, 28, 71, 0.6);
			request_headers_color = _request_headers_color.c_str();
			static constexpr auto _request_head_color = brgb(158, 28, 71);
			request_head_color = _request_head_color.c_str();
			static constexpr auto _request_text_color = rgb(158, 28, 71);
			request_text_color = _request_text_color.c_str();
			break;
		}
		default:
			break;
	};

	auto request_text = request_head_color + head() + "\n" + request_headers_color + headers + request_text_color;
	if (!raw.empty()) {
		if (!decode) {
			if (raw.size() > 1024 * 10) {
				request_text += "<body " + strings::from_bytes(raw.size()) + ">";
			} else {
				request_text += "<body " + repr(raw, true, true, 500) + ">";
			}
		} else if (Logging::log_level > LOG_DEBUG + 1 && can_preview(ct_type)) {
			// From [https://www.iterm2.com/documentation-images.html]
			std::string b64_name = cppcodec::base64_rfc4648::encode("");
			std::string b64_data = cppcodec::base64_rfc4648::encode(raw);
			request_text += strings::format("\033]1337;File=name={};inline=1;size={};width=20%:",
				b64_name,
				b64_data.size());
			request_text += b64_data;
			request_text += '\a';
		} else {
			if (raw.size() > 1024 * 10) {
				request_text += "<body " + strings::from_bytes(raw.size()) + ">";
			} else {
				try {
					auto& decoded = decoded_body();
					if (
						ct_type == json_type ||
						ct_type == x_json_type ||
						ct_type == yaml_type ||
						ct_type == x_yaml_type ||
						ct_type == msgpack_type ||
						ct_type == x_msgpack_type
					) {
						request_text += decoded.to_string(DEFAULT_INDENTATION);
					} else {
						request_text += "<body " + repr(raw, true, true, 500) + ">";
					}
				} catch (...) {
					request_text += "<body " + repr(raw, true, true, 500) + ">";
				}
			}
		}
	} else if (!text.empty()) {
		if (!decode) {
			if (text.size() > 1024 * 10) {
				request_text += "<body " + strings::from_bytes(text.size()) + ">";
			} else {
				request_text += "<body " + repr(text, true, true, 500) + ">";
			}
		} else if (text.size() > 1024 * 10) {
			request_text += "<body " + strings::from_bytes(text.size()) + ">";
		} else {
			request_text += text;
		}
	} else if (size) {
		request_text += "<body " + strings::from_bytes(size) + ">";
	}

	return request_text;
}


Response::Response() :
	status(static_cast<http_status>(0)),
	size(0)
{
}


std::string
Response::to_text(bool decode)
{
	L_CALL("Response::to_text({})", decode);

	static constexpr auto no_col = NO_COLOR;
	auto response_headers_color = no_col.c_str();
	auto response_head_color = no_col.c_str();
	auto response_text_color = no_col.c_str();

	if ((int)status >= 200 && (int)status <= 299) {
		static constexpr auto _response_headers_color = rgba(68, 136, 68, 0.6);
		response_headers_color = _response_headers_color.c_str();
		static constexpr auto _response_head_color = brgb(68, 136, 68);
		response_head_color = _response_head_color.c_str();
		static constexpr auto _response_text_color = rgb(68, 136, 68);
		response_text_color = _response_text_color.c_str();
	} else if ((int)status >= 300 && (int)status <= 399) {
		static constexpr auto _response_headers_color = rgba(68, 136, 120, 0.6);
		response_headers_color = _response_headers_color.c_str();
		static constexpr auto _response_head_color = brgb(68, 136, 120);
		response_head_color = _response_head_color.c_str();
		static constexpr auto _response_text_color = rgb(68, 136, 120);
		response_text_color = _response_text_color.c_str();
	} else if ((int)status == 404) {
		static constexpr auto _response_headers_color = rgba(116, 100, 77, 0.6);
		response_headers_color = _response_headers_color.c_str();
		static constexpr auto _response_head_color = brgb(116, 100, 77);
		response_head_color = _response_head_color.c_str();
		static constexpr auto _response_text_color = rgb(116, 100, 77);
		response_text_color = _response_text_color.c_str();
	} else if ((int)status >= 400 && (int)status <= 499) {
		static constexpr auto _response_headers_color = rgba(183, 70, 17, 0.6);
		response_headers_color = _response_headers_color.c_str();
		static constexpr auto _response_head_color = brgb(183, 70, 17);
		response_head_color = _response_head_color.c_str();
		static constexpr auto _response_text_color = rgb(183, 70, 17);
		response_text_color = _response_text_color.c_str();
	} else if ((int)status >= 500 && (int)status <= 599) {
		static constexpr auto _response_headers_color = rgba(190, 30, 10, 0.6);
		response_headers_color = _response_headers_color.c_str();
		static constexpr auto _response_head_color = brgb(190, 30, 10);
		response_head_color = _response_head_color.c_str();
		static constexpr auto _response_text_color = rgb(190, 30, 10);
		response_text_color = _response_text_color.c_str();
	}

	auto response_text = response_head_color + head + "\n" + response_headers_color + headers + response_text_color;
	if (!blob.empty()) {
		if (!decode) {
			if (blob.size() > 1024 * 10) {
				response_text += "<blob " + strings::from_bytes(blob.size()) + ">";
			} else {
				response_text += "<blob " + repr(blob, true, true, 500) + ">";
			}
		} else if (Logging::log_level > LOG_DEBUG + 1 && can_preview(ct_type)) {
			// From [https://www.iterm2.com/documentation-images.html]
			std::string b64_name = cppcodec::base64_rfc4648::encode("");
			std::string b64_data = cppcodec::base64_rfc4648::encode(blob);
			response_text += strings::format("\033]1337;File=name={};inline=1;size={};width=20%:",
				b64_name,
				b64_data.size());
			response_text += b64_data;
			response_text += '\a';
		} else {
			if (blob.size() > 1024 * 10) {
				response_text += "<blob " + strings::from_bytes(blob.size()) + ">";
			} else {
				response_text += "<blob " + strings::from_bytes(blob.size()) + ">";
			}
		}
	} else if (!text.empty()) {
		if (!decode) {
			if (size > 1024 * 10) {
				response_text += "<body " + strings::from_bytes(size) + ">";
			} else {
				response_text += "<body " + repr(text, true, true, 500) + ">";
			}
		} else if (size > 1024 * 10) {
			response_text += "<body " + strings::from_bytes(size) + ">";
		} else {
			response_text += text;
		}
	} else if (size) {
		response_text += "<body " + strings::from_bytes(size) + ">";
	}

	return response_text;
}
