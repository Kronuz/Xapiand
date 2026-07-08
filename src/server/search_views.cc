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

#include "search_views.h"

#include "config.h"                         // for XAPIAND_CLUSTERING, XAPIAND_LUA, XAPIAND_DATABASE_WAL

#include <cassert>                          // for assert
#include <charconv>                         // for std::from_chars (indent param)
#include <errno.h>                          // for errno
#include <exception>                        // for std::exception
#include <functional>                       // for std::function
#include <signal.h>                         // for SIGTERM
#include <sysexits.h>                       // for EX_SOFTWARE
#include <syslog.h>                         // for LOG_WARNING, LOG_ERR, LOG...
#include <utility>                          // for std::move

#ifdef USE_ICU
#include <unicode/uvernum.h>
#endif

#ifdef XAPIAND_LUA
#include <lua.h>                              // for LUA_VERSION_MAJOR, LUA_VERSION_MINOR
#endif
#include "cppcodec/base64_rfc4648.hpp"      // for cppcodec::base64_rfc4648
#include "database/handler.h"               // for DatabaseHandler, DocIndexer
#include "database/utils.h"                 // for query_field_t
#include "database/pool.h"                  // for DatabasePool
#include "database/schema.h"                // for Schema
#include "endpoint.h"                       // for Endpoints, Endpoint
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "exception_xapian.h"               // for Exception, SerialisationError
#include "field_parser.h"                   // for FieldParser, FieldParserError
#include "hashes.hh"                        // for hhl
#include "http_utils.h"                     // for catch_http_errors
#include "http_accept.h"                    // for http::AcceptCache (content negotiation)
#include "http_handler.h"                   // for http::ResponseWriter (Leg 2 stage 3c)
#include "http_router.h"                    // for http::MethodRouter (Leg 2 stage 3c-6)
#include "search_service.h"             // for SearchService (Leg 2 stage 3c)
#include "io.hh"                            // for close, write, unlink
#include "log.h"                            // for L_CALL, L_ERR, LOG_DEBUG
#include "logger.h"                         // for Logging
#include "manager.h"                        // for XapiandManager
#include "metrics.h"                        // for Metrics::metrics
#include "mime_types.h"                    // for mime_type
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
#include "xapiand_strings.hh"                       // for strings::from_delta
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


static const std::string eol("\r\n");


// Content negotiation helpers, lifted off HttpClient `this` so the forthcoming
// SearchService can negotiate without an HttpClient (Leg 2 stage 2a). They use
// only `request` (content negotiation caches live in the framework now), so they are file-scope free
// functions; forward-declared here because the views below call them before their
// definitions (and they call one another out of order).
static const ct_type_t& resolve_ct_type(Request& request, const std::vector<const ct_type_t*>& ct_types);
static const ct_type_t& resolve_ct_type(Request& request, const ct_type_t& ct_type = no_type);
static const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t* ct_type);
static const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<const ct_type_t*>& ct_types);
template <typename T>
static const ct_type_t& get_acceptable_type(Request& request, const T& ct);

// More request-only helpers lifted off HttpClient `this` (Leg 2 stage 2b/2c): URL +
// query parsing. They read/write only their `request` argument, so they are file-scope
// free functions too; forward-declared here because they are used above their
// definitions (e.g. prepare(), write_http_response).
static void url_resolve(Request& request);
static query_field_t query_field_maker(Request& request, int flags);

// The DB-bound prep helpers lifted off HttpClient `this` (Leg 2 stage 3a). They
// touch only their `request` argument plus the manager/DB singletons -- never any
// HttpClient/connection state -- so they are file-scope free functions, the seam
// the forthcoming SearchService shares. Forward-declared here because the views
// above call them before their definitions.
static MsgPack node_obj();
static MsgPack retrieve_database(Request& request, const query_field_t& query_field, bool is_root, std::string_view selector);
static std::vector<std::string> expand_paths(Request& request);
static size_t resolve_index_endpoints(Request& request, const query_field_t& query_field, const MsgPack* settings = nullptr);

// The response/transport helpers lifted off HttpClient `this` (Leg 2 stage 3b). The
// response is formatted from `request` state (content negotiation, serialization) and
// emitted through the library's ResponseWriter, which frames the bytes and applies the
// negotiated Content-Encoding. Forward-declared here since prepare()/views call them above.
static void write_status_response(Request& request, enum http_status status, const std::string& message = "");
static void write_http_response(Request& request, enum http_status status, const MsgPack& obj = MsgPack(), const std::string& location = "", const ct_type_t& ct_type = no_type);

// The ~23 endpoint views lifted off HttpClient `this` (Leg 2 stage 3c-3). Their
// bodies use only their `request` argument + the file-scope helpers above +
// the manager/DB singletons -- no HttpClient/connection state -- so they are
// free functions, dispatched through a plain function pointer (Request::view is
// now `void(*)(Request&)`). This is what SearchService::handle() will call.
// Forward-declared here because prepare() takes their address before their defs.
static void metrics_view(Request& request);
static void info_view(Request& request);
static void retrieve_metadata_view(Request& request);
static void write_metadata_view(Request& request);
static void update_metadata_view(Request& request);
static void delete_metadata_view(Request& request);
static void document_exists_view(Request& request);
static void retrieve_document_view(Request& request);
static void write_document_view(Request& request);
static void update_document_view(Request& request);
static void delete_document_view(Request& request);
static void dump_document_view(Request& request);
static void database_exists_view(Request& request);
static void retrieve_database_view(Request& request);
static void write_database_view(Request& request);
static void delete_database_view(Request& request);
static void dump_database_view(Request& request);
static void restore_database_view(Request& request);
static void check_database_view(Request& request);
static void commit_database_view(Request& request);
static void search_view(Request& request);
static void count_view(Request& request);
static void flush_view(Request& request);
static void quit_view(Request& request);
#if XAPIAND_DATABASE_WAL
static void wal_view(Request& request);
#endif



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








// Emit a response through the generic HTTP library's ResponseWriter (Leg 2 stage
// 3c). Mirrors the header set http_response() produces on the legacy path, minus
// Content-Length + Connection, which the library's Writer frames itself (and the
// status line/reason). Sets the same request.response bookkeeping so the error
// handling (handled_errors' "already wrote something" check) behaves identically.
static void
emit_via_writer(Request& request, enum http_status status, const std::string& body, const std::string& location, const std::string& ct_type, const std::string& ct_encoding, bool options)
{
	auto& writer = *request.response_writer;

	assert(request.response_status == static_cast<http_status>(0));
	request.response_status = status;
	request.ends = std::chrono::steady_clock::now();

	// Per-request HTTP latency summary, keyed by method + status. The legacy
	// HttpClient recorded this on every response; the reactor/http migration dropped
	// the observation (the family stayed declared but unfed), which upstream
	// prometheus-cpp then surfaced as a missing metric family. Restore it here, the
	// single per-request emission point.
	Metrics::metrics()
		.xapiand_http_requests_summary
		.Add({
			{"method", request.http_req != nullptr ? request.http_req->method : std::string()},
			{"status", std::to_string(static_cast<int>(status))},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count() / 1e9);

	writer.status(static_cast<int>(status));

	writer.set_header("Server", Package::STRING);

	if (request.human) {
		writer.set_header("Response-Time", strings::from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count()));
		if (request.ready >= request.processing) {
			writer.set_header("Operation-Time", strings::from_delta(std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count()));
		}
	} else {
		writer.set_header("Response-Time", strings::format("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(request.ends - request.begins).count() / 1e9));
		if (request.ready >= request.processing) {
			writer.set_header("Operation-Time", strings::format("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count() / 1e9));
		}
	}

	if (options) {
		writer.set_header("Allow", "GET, POST, PUT, PATCH, UPDATE, STORE, DELETE, HEAD, OPTIONS");
	}
	if (!location.empty()) {
		writer.set_header("Location", location);
	}
	if (!ct_type.empty()) {
		writer.set_header("Content-Type", ct_type);
	}
	if (!ct_encoding.empty()) {
		writer.set_header("Content-Encoding", ct_encoding);
	}

	writer.write(body);
	writer.end();
	request.response_size += body.size();
}


// The single response emit point (Leg 2 stage 3c): the request is served through an
// http::HttpConnection, so the response is framed by the library's ResponseWriter.
// The `mode` flags carry which response parts apply (Content-Type / Content-Encoding /
// Allow-for-OPTIONS); the Writer owns Content-Length + Connection + the status line.
static void
emit_response(Request& request, enum http_status status, int mode, const std::string& body = "", const std::string& location = "", const std::string& ct_type = "application/json; charset=UTF-8", const std::string& ct_encoding = "", size_t content_length = 0)
{
	(void)content_length;  // the Writer derives Content-Length from the body it frames
	emit_via_writer(request, status, body, location, (mode & HTTP_CONTENT_TYPE_RESPONSE) != 0 ? ct_type : std::string(), (mode & HTTP_CONTENT_ENCODING_RESPONSE) != 0 ? ct_encoding : std::string(), (mode & HTTP_OPTIONS_RESPONSE) != 0);
}


















// HTTP parser callbacks.

















// The per-header processing lifted out of on_header_value (Leg 2 stage 3c): fills
// request state (accept / accept-encoding via the LRU caches, content-type, the
// Expect flag, and the method-override verbs) from one header name/value. A free
// function so SearchService::handle() can replay it over http::Request.headers
// exactly as the parser callback does. An invalid method override sets
// request.parser.http_errno (the same object on_header_value already writes).
static void
process_header(Request& request, std::string_view header_name, std::string_view header_value)
{
		constexpr static auto _ = phf::make_phf({
			hhl("expect"),
			hhl("100-continue"),
			hhl("content-type"),
			hhl("accept"),
			hhl("http-method-override"),
			hhl("x-http-method-override"),
		});

		switch (_.fhhl(header_name)) {
			case _.fhhl("expect"):
			case _.fhhl("100-continue"):
				// Respond with HTTP/1.1 100 Continue
				request.expect_100 = true;
				break;

			case _.fhhl("content-type"):
				request.ct_type = ct_type_t(header_value);
				break;

			case _.fhhl("accept"): {
				// Parse + cache the Accept header with the framework negotiator
				// (http::AcceptCache does the generic RFC 7231 parsing + LRU caching we
				// used to hand-roll). Build Xapiand's ct_type_t accept_set from the parsed
				// items, reading the custom ";indent" rendering hint off each item's
				// parameters. Only the ct_type_t + indent mapping is Xapiand's now.
				static http::AcceptCache accept_cache;
				auto parsed = accept_cache.get(strings::lower(header_value));
				accept_set_t set;
				for (const auto& item : parsed->items()) {
					// Content-type ranges are always "type/subtype"; skip a token-only
					// item (e.g. a bare "*", which is malformed for Accept) so the set
					// matches what the old type/subtype parser produced. get_accepted's
					// wildcard handling expects "*/*", not a lone "*".
					if (item.subtype.empty()) { continue; }
					int indent = -1;
					auto iv = item.param("indent");
					if (!iv.empty()) {
						int n = 0;
						auto res = std::from_chars(iv.data(), iv.data() + iv.size(), n);
						if (res.ec == std::errc()) { indent = n < 0 ? 0 : (n > 16 ? 16 : n); }
					}
					set.emplace(item.order, item.q, ct_type_t(item.type, item.subtype), indent);
				}
				request.accept_set = std::move(set);
				break;
			}

			case _.fhhl("x-http-method-override"):
			case _.fhhl("http-method-override"): {
				switch (http_methods.fhhl(header_value)) {
					#define OPTION(name, str) \
					case http_methods.fhhl(str): \
						if ( \
							request.method != HTTP_POST && \
							request.method != HTTP_GET && \
							request.method != HTTP_##name \
						) { \
							THROW(ClientError, "{} header must use the POST method", repr(header_name)); \
						} \
						request.method = HTTP_##name; \
						break;
					METHODS_OPTIONS()
					#undef OPTION
					default:
						L_HTTP_PROTO("Invalid HTTP method override: {}", repr(header_value));
						break;
				}
				break;
			}
		}
}









// The URL "kinds" Xapiand's grammar classifies into: a small, fixed set of route
// keys the declarative table is registered under. They are internal routing tokens
// (never user-facing paths), so they are zero-copy constants and cost no allocation
// per request.
static constexpr std::string_view K_DB      = "/db";        // a database / collection (no id, no command)
static constexpr std::string_view K_DOC     = "/doc";       // a document (a trailing id)
static constexpr std::string_view K_META    = "/meta";      // a metadata command (":command", no id)
static constexpr std::string_view K_SEARCH  = "/search";    // GET of a range id -> a search shortcut
static constexpr std::string_view K_METRICS = "/metrics";   // GET ":metrics" at the root


// Map an HTTP method enum to the lowercase token the route table is keyed by (the
// same names the METHODS_OPTIONS command table uses). Returns empty for a method not
// in that table -- the lookup then misses and the caller answers 405.
static std::string_view
method_token(enum http_method method)
{
	switch (method) {
		#define OPTION(name, str) \
		case HTTP_##name: return str;
		METHODS_OPTIONS()
		#undef OPTION
		default:
			return {};
	}
}


// Classify a request's URL shape into a route key. This is the whole of Xapiand's
// URL grammar the router needs: PathParser has already tokenized the ":command" and
// ".selector" sub-syntax (which the views read directly), leaving three structural
// cases -- a document (trailing id), a command (":command" with no id), or a database
// (neither) -- plus GET's two sub-forms (a range id is a search; ":metrics" at the
// root is the metrics endpoint).
static std::string_view
route_key(enum http_method method, std::string_view id, std::string_view cmd, bool has_pth)
{
	if (!id.empty()) {
		if (method == HTTP_GET && is_range(id)) { return K_SEARCH; }
		return K_DOC;
	}
	if (!cmd.empty()) {
		if (method == HTTP_GET && !has_pth && cmd == ":metrics") { return K_METRICS; }
		return K_META;
	}
	return K_DB;
}


// The declarative route table: (method, URL-kind) -> view, backed by the generic
// radix router in Kronuz/http (http::MethodRouter). Built once, on first use. This is
// what prepare()'s hand-rolled method + URL switch collapses into.
//
// A verb that ignores the metadata axis (search/count/info/head/commit/dump/restore/
// check/flush/quit/wal all key only on whether an id is present) registers its no-id
// view under K_META as well as K_DB, so a stray ":command" folds to the same view the
// old switch produced -- exact behavioral parity. A (method, kind) pair left
// unregistered is answered 405, the same status the old switch's fall-through gave.
static const http::MethodRouter<view_function>&
search_routes()
{
	static const http::MethodRouter<view_function> router = [] {
		http::MethodRouter<view_function> r;
		auto add = [&r](std::string_view method, std::initializer_list<std::string_view> kinds, view_function view) {
			for (auto kind : kinds) { r.add(method, kind, view); }
		};

		// Query verbs (a command folds to the no-id view).
		add("search", {K_DB, K_META}, &search_view);
		add("count",  {K_DB, K_META}, &count_view);
		add("info",   {K_DB, K_DOC, K_META}, &info_view);
		add("head",   {K_DB, K_META}, &database_exists_view);
		add("head",   {K_DOC}, &document_exists_view);

		// Reads (GET carries the metadata / metrics / range sub-forms).
		add("get", {K_DB}, &retrieve_database_view);
		add("get", {K_DOC}, &retrieve_document_view);
		add("get", {K_SEARCH}, &search_view);
		add("get", {K_META}, &retrieve_metadata_view);
		add("get", {K_METRICS}, &metrics_view);

		// Writes.
		add("post", {K_DB}, &write_document_view);
		add("put",  {K_DB}, &write_database_view);
		add("put",  {K_DOC}, &write_document_view);
		add("put",  {K_META}, &write_metadata_view);
		for (std::string_view m : {std::string_view("patch"), std::string_view("update"), std::string_view("upsert")}) {
			add(m, {K_DB}, &write_database_view);
			add(m, {K_DOC}, &update_document_view);
			add(m, {K_META}, &update_metadata_view);
		}
		add("delete", {K_DB}, &delete_database_view);
		add("delete", {K_DOC}, &delete_document_view);
		add("delete", {K_META}, &delete_metadata_view);

		// Maintenance / admin verbs (a command folds to the no-id view).
		add("commit",  {K_DB, K_META}, &commit_database_view);
		add("dump",    {K_DB, K_META}, &dump_database_view);
		add("dump",    {K_DOC}, &dump_document_view);
		add("restore", {K_DB, K_META}, &restore_database_view);
		add("restore", {K_DOC}, &write_document_view);
		add("check",   {K_DB, K_META}, &check_database_view);
		add("flush",   {K_DB, K_META}, &flush_view);
		add("quit",    {K_DB, K_META}, &quit_view);
#if XAPIAND_DATABASE_WAL
		add("wal",     {K_DB, K_META}, &wal_view);
#endif
		return r;
	}();
	return router;
}


// The request dispatch for the http::HttpConnection path (Leg 2 stage 3c): the setup
// prepare() does (keep-alive/close, content negotiation, encoding), then a
// declarative route-table lookup that selects the view (replacing prepare()'s method +
// URL switch, stage 3c-6). A few trivial verbs (OPTIONS, OPEN/CLOSE) are answered
// inline; Expect: 100-continue is left to the transport. Returns non-zero when there
// is no view to run (a terminal response was already emitted).
static int
dispatch_request(Request& request)
{


	request.received = std::chrono::steady_clock::now();

	// keep_alive already folds in the HTTP version + the Connection header
	// (http_should_keep_alive), so it is the single source for whether to close.
	request.closing = !request.keep_alive;

	if (request.accept_set.empty()) {
		if (!request.ct_type.empty()) {
			request.accept_set.emplace(0, 1.0, request.ct_type, 0);
		}
		request.accept_set.emplace(1, 1.0, any_type, 0);
	}

	url_resolve(request);

	auto id = request.path_parser.get_id();
	auto has_pth = request.path_parser.has_pth();
	auto cmd = request.path_parser.get_cmd();

	if (!cmd.empty()) {
		auto mapping = cmd;
		mapping.remove_prefix(1);
		switch (http_methods.fhhl(mapping)) {
			#define OPTION(name, str) \
			case http_methods.fhhl(str): \
				if ( \
					request.method != HTTP_POST && \
					request.method != HTTP_GET && \
					request.method != HTTP_##name \
				) { \
					THROW(ClientError, "HTTP Mappings must use GET or POST method"); \
				} \
				request.method = HTTP_##name; \
				cmd = ""; \
				break;
			METHODS_OPTIONS()
			#undef OPTION
		}
	}

	// Trivial verbs answered inline (no database work, so never offloaded): OPTIONS
	// advertises the allowed methods, OPEN/CLOSE are unimplemented, and a method not in
	// the command table is rejected the way prepare()'s switch default did.
	switch (request.method) {
		case HTTP_OPTIONS:
			emit_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_OPTIONS_RESPONSE | HTTP_BODY_RESPONSE);
			return 1;
		case HTTP_OPEN:
		case HTTP_CLOSE:
			write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
			return 1;
		default:
			break;
	}
	if (method_token(request.method).empty()) {
		L_HTTP_PROTO("Invalid HTTP method: {}", enum_name(request.method));
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		return 1;
	}

	// A body-carrying RESTORE of a whole database streams its objects (NDJSON / MsgPack)
	// rather than buffering; pick the stream mode before the body is fed. A single
	// document RESTORE (with an id) buffers like any other write.
	if (request.method == HTTP_RESTORE && id.empty() && request.has_content_length) {
		if (request.ct_type == ndjson_type || request.ct_type == x_ndjson_type) {
			request.mode = Request::Mode::STREAM_NDJSON;
		} else if (request.ct_type == msgpack_type || request.ct_type == x_msgpack_type) {
			request.mode = Request::Mode::STREAM_MSGPACK;
		}
	}

	// The declarative route-table lookup (method + URL-kind -> view). A miss is a 405 --
	// the same status prepare()'s switch produced for every unmatched method/URL shape.
	{
		http::Params params;
		auto key = route_key(request.method, id, cmd, has_pth);
		if (const view_function* view = search_routes().find(method_token(request.method), key, params)) {
			request.view = *view;
		} else {
			write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
			return 1;
		}
	}


	// (Expect: 100-continue is a transport concern handled by http::HttpConnection.)

	// Reserve the body buffer up front from Content-Length -- but NOT when streaming
	// (request.http_req->body_reader set): that body is pulled in bounded chunks, so
	// reserving its full (possibly multi-gigabyte) length would defeat the point.
	bool streaming = request.http_req != nullptr && request.http_req->body_reader != nullptr;
	if (request.has_content_length && request.content_length && !streaming) {
		if (request.mode == Request::Mode::STREAM_MSGPACK) {
			request.unpacker.reserve_buffer(request.content_length);
		} else {
			request.raw.reserve(request.content_length);
		}
	}

	return 0;
}








static MsgPack
node_obj()
{
	L_CALL("node_obj()");

	Endpoints endpoints;  // local scratch for the nodes lookup (was the HttpClient member)
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
#ifdef XAPIAND_LUA
			{ "Lua", LUA_VERSION_MAJOR "." LUA_VERSION_MINOR },
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


static void
metrics_view(Request& request)
{
	L_CALL("metrics_view()");

	auto query_field = query_field_maker(request, 0);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	auto server_info =  XapiandManager::server_metrics();
	emit_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_LENGTH_RESPONSE | HTTP_BODY_RESPONSE, server_info, "", "text/plain", "", server_info.size());
}


static void
document_exists_view(Request& request)
{
	L_CALL("document_exists_view()");

	auto query_field = query_field_maker(request, 0);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN);

	db_handler.get_document(request.path_parser.get_id()).validate();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


static void
delete_document_view(Request& request)
{
	L_CALL("delete_document_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	std::string document_id(request.path_parser.get_id());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	db_handler.delete_document(document_id, query_field.commit);
	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_NO_CONTENT);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Deletion took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "delete"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
write_document_view(Request& request)
{
	L_CALL("write_document_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	auto document_id = request.path_parser.get_id();

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
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
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, document_id_obj.as_str());
				response_obj[ID_FIELD_NAME] = std::move(document_id_obj);
			} else {
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, it.value().as_str());
			}
		} else {
			if (it == response_obj.end()) {
				response_obj[ID_FIELD_NAME] = db_handler.unserialise_term_id(info.term);
			}
		}

		response_obj[VERSION_FIELD_NAME] = info.version;

		if (request.comments) {
			response_obj[RESPONSE_xDOCID] = info.did;

			size_t n_shards = request.endpoints.size();
			size_t shard_num = (info.did - 1) % n_shards;
			response_obj[RESPONSE_xSHARD] = shard_num + 1;
			// response_obj[RESPONSE_xENDPOINT] = request.endpoints[shard_num].to_string();
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
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, document_id_obj.as_str());
			} else {
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, it.value().as_str());
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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
update_document_view(Request& request)
{
	L_CALL("update_document_view()");

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
	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
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

			size_t n_shards = request.endpoints.size();
			size_t shard_num = (info.did - 1) % n_shards;
			response_obj[RESPONSE_xSHARD] = shard_num + 1;
			// response_obj[RESPONSE_xENDPOINT] = request.endpoints[shard_num].to_string();
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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
retrieve_metadata_view(Request& request)
{
	L_CALL("retrieve_metadata_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	MsgPack response_obj;

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
write_metadata_view(Request& request)
{
	L_CALL("write_metadata_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
update_metadata_view(Request& request)
{
	L_CALL("update_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


static void
delete_metadata_view(Request& request)
{
	L_CALL("delete_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


static void
info_view(Request& request)
{
	L_CALL("info_view()");

	MsgPack response_obj;

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
database_exists_view(Request& request)
{
	L_CALL("database_exists_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

	db_handler.reopen();  // Ensure it can be opened.

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


static MsgPack
retrieve_database(Request& request, const query_field_t& query_field, bool is_root, std::string_view selector)
{
	L_CALL("retrieve_database()");

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
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
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
	auto id = std::string(request.endpoints.size() == 1 ? request.endpoints[0].path : unsharded_path(request.endpoints[0].path).first);
	request.endpoints = XapiandManager::resolve_index_endpoints(
		Endpoint{".xapiand/indices"},
		query_field.writable,
		query_field.primary);

	try {
		DatabaseHandler db_handler;
		if (query_field.writable || query_field.primary) {
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
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


static void
retrieve_database_view(Request& request)
{
	L_CALL("retrieve_database_view()");

	assert(request.path_parser.get_id().empty());

	auto is_root = !request.path_parser.has_pth();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	auto obj = retrieve_database(request, query_field, is_root, selector);

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Retrieving database took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "retrieve_database"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);

	L_SEARCH("FINISH RETRIEVE DATABASE");
}


static void
write_database_view(Request& request)
{
	L_CALL("write_database_view()");

	assert(request.path_parser.get_id().empty());

	auto is_root = !request.path_parser.has_pth();

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE);
	if (resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

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
		auto obj = retrieve_database(request, query_field, is_root, selector);

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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
delete_database_view(Request& request)
{
	L_CALL("delete_database_view()");

	// Deleting a database needs a path to name it; without one there is nothing to
	// address (prepare() answered this 405 before ever selecting the view).
	if (!request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		return;
	}

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


// Administrative verbs. These do no per-database query work and (like the OPTIONS /
// OPEN / CLOSE verbs) run inline on the reactor rather than the worker pool -- see
// SearchService::should_offload. Their admin_commands + root-URL preconditions,
// which prepare()'s switch enforced before dispatch, are checked here at the view.
static void
flush_view(Request& request)
{
	L_CALL("flush_view()");

	if (!opts.admin_commands || request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		return;
	}

	// Flush both databases and clients by default (unless one is specified).
	request.query_parser.rewind();
	int flush_databases = request.query_parser.next("databases");
	request.query_parser.rewind();
	int flush_clients = request.query_parser.next("clients");
	if (flush_databases != -1 || flush_clients == -1) {
		XapiandManager::manager(true)->database_pool->cleanup(true, false);
	}
	if (flush_clients != -1 || flush_databases == -1) {
		XapiandManager::manager(true)->shutdown(0, 0);
	}

	write_http_response(request, HTTP_STATUS_OK);
}


static void
quit_view(Request& request)
{
	L_CALL("quit_view()");

	if (!opts.admin_commands || request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		return;
	}

	XapiandManager::try_shutdown(true);
	write_http_response(request, HTTP_STATUS_OK);
	request.closing = true;
}


static void
commit_database_view(Request& request)
{
	L_CALL("commit_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	db_handler.commit();  // Ensure touch.

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Commit took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "commit"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
dump_document_view(Request& request)
{
	L_CALL("dump_document_view()");

	auto query_field = query_field_maker(request, 0);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto document_id = request.path_parser.get_id();
	assert(!document_id.empty());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

	auto obj = db_handler.dump_document(document_id);

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Dump took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "dump"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
dump_database_view(Request& request)
{
	L_CALL("dump_database_view()");

	auto query_field = query_field_maker(request, 0);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
restore_database_view(Request& request)
{
	L_CALL("restore_database_view()");

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

				indexer = DocIndexer::make_shared(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, request.echo, request.comments, query_field);
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

				indexer = DocIndexer::make_shared(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, request.echo, request.comments, query_field);
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
			// { RESPONSE_ENDPOINT, request.endpoints.to_string() },
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
			}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
			.Observe(took / 1e9);
	}
}


#if XAPIAND_DATABASE_WAL
static void
wal_view(Request& request)
{
	L_CALL("wal_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints);

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
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}
#endif


static void
check_database_view(Request& request)
{
	L_CALL("check_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints);

	auto status = db_handler.check();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, status);

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Database check took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "db_check"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);
}


static void
retrieve_document_view(Request& request)
{
	L_CALL("retrieve_document_view()");

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
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
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

			size_t n_shards = request.endpoints.size();
			size_t shard_num = (did - 1) % n_shards;
			obj[RESPONSE_xSHARD] = shard_num + 1;
			// obj[RESPONSE_xENDPOINT] = request.endpoints[shard_num].to_string();
		}

		if (!selector.empty()) {
			obj = obj.select(selector);
		}

		request.ready = std::chrono::steady_clock::now();

		write_http_response(request, HTTP_STATUS_OK, obj, "", resolve_ct_type(request, selector_mime_type));
	} else {
		// Locator has content type, return as a blob (an image for instance)
		auto ct_type = locator.ct_type;
		request.response_blob = locator.data();
#ifdef XAPIAND_DATA_STORAGE
		if (locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored) {
			if (request.response_blob.empty()) {
				auto stored = db_handler.storage_get_stored(locator, did);
				request.response_blob = unserialise_string_at(STORED_BLOB, stored);
			}
		}
#endif

		request.ready = std::chrono::steady_clock::now();

		request.response_ct_type = ct_type;
		emit_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_BODY_RESPONSE, request.response_blob, "", ct_type.to_string());
	}

	auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();
	L_TIME("Retrieving took {}", strings::from_delta(took));

	Metrics::metrics()
		.xapiand_operations_summary
		.Add({
			{"operation", "retrieve"},
		}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
		.Observe(took / 1e9);

	L_SEARCH("FINISH RETRIEVE");
}


static void
search_view(Request& request)
{
	L_CALL("search_view()");

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
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
		}

		if (request.body_view().empty()) {
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

			size_t n_shards = request.endpoints.size();
			size_t shard_num = (did - 1) % n_shards;
			hit_obj[RESPONSE_xSHARD] = shard_num + 1;
			// hit_obj[RESPONSE_xENDPOINT] = request.endpoints[shard_num].to_string();

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
			}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
			.Observe(took / 1e9);
	} else {
		Metrics::metrics()
			.xapiand_operations_summary
			.Add({
				{"operation", "search"},
			}, Metrics::summary_quantiles(), std::chrono::minutes(10), 5)
			.Observe(took / 1e9);
	}

	L_SEARCH("FINISH SEARCH");
}


static void
count_view(Request& request)
{
	L_CALL("count_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_SEARCH);
	resolve_index_endpoints(request, query_field);

	std::tuple<Xapian::MSet, MsgPack, Xapian::Query> mset_tuple;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	try {
		if (query_field.primary) {
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
		}

		if (request.body_view().empty()) {
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


static void
write_status_response(Request& request, enum http_status status, const std::string& message)
{
	L_CALL("write_status_response()");

	MsgPack response({
		{ RESPONSE_STATUS, static_cast<unsigned>(status) },
		{ RESPONSE_TYPE, http_status_str(status) },
	});
	if (!message.empty()) {
		response[RESPONSE_MESSAGE] = message;
	}
	write_http_response(request, status, response);
}


static void
url_resolve(Request& request)
{
	L_CALL("url_resolve(request)");

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


static size_t
resolve_index_endpoints(Request& request, const query_field_t& query_field, const MsgPack* settings)
{
	L_CALL("resolve_index_endpoints(<request>, <query_field>, <settings>)");

	auto paths = expand_paths(request);

	request.endpoints.clear();
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
			request.endpoints.add(endpoint);
		}
	}
	L_HTTP("Endpoint: -> {}", request.endpoints.to_string());

	return paths.size();
}


static std::vector<std::string>
expand_paths(Request& request)
{
	L_CALL("expand_paths(<request>)");

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


static query_field_t
query_field_maker(Request& request, int flags)
{
	L_CALL("query_field_maker(<request>, <flags>)");

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








static const ct_type_t&
resolve_ct_type(Request& request, const std::vector<const ct_type_t*>& ct_types)
{
	L_CALL("resolve_ct_type(<request>, <ct_types>)");

	const auto& acceptable_type = get_acceptable_type(request, ct_types);
	auto accepted_ct_type = is_acceptable_type(acceptable_type, ct_types);
	if (accepted_ct_type == nullptr) {
		accepted_ct_type = &no_type;
	}

	return *accepted_ct_type;
}


static const ct_type_t&
resolve_ct_type(Request& request, const ct_type_t& ct_type)
{
	L_CALL("resolve_ct_type(<request>, {})", repr(ct_type.to_string()));

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


static const ct_type_t*
is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t* ct_type)
{
	L_CALL("is_acceptable_type({}, {})", repr(ct_type_pattern.to_string()), repr(ct_type->to_string()));

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


static const ct_type_t*
is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<const ct_type_t*>& ct_types)
{
	L_CALL("is_acceptable_type(({}, <ct_types>)", repr(ct_type_pattern.to_string()));

	for (auto ct_type : ct_types) {
		if (is_acceptable_type(ct_type_pattern, ct_type) != nullptr) {
			return ct_type;
		}
	}
	return nullptr;
}


template <typename T>
static const ct_type_t&
get_acceptable_type(Request& request, const T& ct)
{
	L_CALL("get_acceptable_type()");

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


// Content negotiation: render a MsgPack object as the bytes + content-type for a
// negotiated ct_type. Pure (no client state) -- a file-scope free function so the
// forthcoming SearchService can format responses without an HttpClient. (Leg 2
// stage 1: extracting the shared response helpers off HttpClient `this`.)
static std::pair<std::string, std::string>
serialize_response(const MsgPack& obj, const ct_type_t& ct_type, int indent, bool serialize_error)
{

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


static void
write_http_response(Request& request, enum http_status status, const MsgPack& obj, const std::string& location, const ct_type_t& ct_type)
{
	L_CALL("write_http_response()");

	if (obj.is_undefined()) {
		emit_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location);
		return;
	}

	auto& resolved_ct_type = ct_type.empty() ? resolve_ct_type(request) : ct_type;

	if (status == HTTP_STATUS_NOT_ACCEPTABLE) {
		if (resolved_ct_type.empty()) {
			emit_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location);
			return;
		}
	}

	try {
		auto result = serialize_response(obj, resolved_ct_type, request.indented, (int)status >= 400);
		emit_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE, result.first, location, result.second);
	} catch (const SerialisationError& exc) {
		if (status == HTTP_STATUS_NOT_ACCEPTABLE) {
			emit_response(request, status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, "", location);
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






Request::Request() :
	mode(Mode::FULL),
	view(nullptr),
	begining(true),
	ending(false),
	atom_ending(false),
	atom_ended(false),
	raw_peek(0),
	raw_offset(0),
	size(0),
	response_status(static_cast<http_status>(0)),
	response_size(0),
	echo(false),
	human(false),
	comments(true),
	indented(-1),
	expect_100(false),
	closing(false),
	begins(std::chrono::steady_clock::now())
{
	// Count this as an in-flight HTTP request so a graceful shutdown waits for it
	// to finish (ready_to_end_http() == !http_clients). Mirrors remote_clients /
	// replication_clients; the HTTP counter was left unwired by the reactor
	// migration, so a single SIGINT/SIGTERM no longer drained active requests.
	if (auto manager = XapiandManager::manager()) {
		++manager->http_clients;
	}

	// In-flight watchdog (recovered v0.4.0 behavior via L_TIMED_VAR): a scheduled
	// WARNING that surfaces a slow or hung request *while it is still running* (before
	// it completes), replaced by a "done" line if it fires. The handle lives on the
	// request (log) so it spans the whole lifetime -- parse, queue, and handling -- and
	// is cleared/completed in ~Request.
	L_TIMED_VAR(log, std::chrono::seconds(10),
		"Request taking too long...",
		"Request took too long!");
}


Request::~Request() noexcept
{
	// Balance the in-flight count taken in the constructor (see there).
	if (auto manager = XapiandManager::manager()) {
		manager->http_clients.fetch_sub(1);
	}

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
			// clean() (not clear()) completes the L_TIMED_VAR watchdog: it cancels the
			// scheduled "taking too long" line silently if the request finished in time,
			// or emits the "done" line ("Request took too long!") if the warning already
			// fired. An external clear() would instead drop the "done" and cancel.
			log->clean();
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
		auto body = body_view();
		if (!body.empty()) {
			_decoded_body = decode(body);
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
			assert(has_content_length);

			raw.append(std::string_view(at, length));
			signal_pending = true;
			break;

		case Mode::STREAM_NDJSON:
			assert(has_content_length);

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

				// Drop the parsed prefix so `raw` holds only the unparsed remainder (a
				// partial line) -- keeps memory O(one line + one chunk) while streaming a
				// multi-gigabyte NDJSON body, instead of retaining the whole body.
				if (raw_offset != 0) {
					raw.erase(0, raw_offset);
					raw_peek -= raw_offset;
					raw_offset = 0;
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
			assert(has_content_length);

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
Request::next_object(MsgPack& obj)
{
	L_CALL("Request::next_object(<&obj>)");

	assert(has_content_length);
	assert(mode == Mode::STREAM_MSGPACK || mode == Mode::STREAM_NDJSON);

	for (;;) {
		{
			std::lock_guard<std::mutex> lk(objects_mtx);
			if (!objects.empty()) {
				obj = std::move(objects.front());
				objects.pop_front();
				return true;
			}
		}
		// The deque is drained. On the streaming path, pull the next raw body chunk from
		// the framework's flow-controlled BodyReader and parse it (append() fills the
		// deque); on the buffered path (no reader) the body was already fully appended, so
		// an empty deque means done. Bounded memory: one chunk + a partial-line remainder.
		if (http_req == nullptr || !http_req->body_reader) {
			return false;
		}
		std::string chunk;
		if (http_req->body_reader->read(chunk)) {
			append(chunk.data(), chunk.size());
		} else {
			append(nullptr, 0);   // flush any trailing streamed object
			std::lock_guard<std::mutex> lk(objects_mtx);
			if (objects.empty()) {
				return false;
			}
		}
	}
}




// ---------------------------------------------------------------------------
// SearchService (Leg 2 stage 3c): Xapiand's search engine as one
// http::HttpHandler. Defined here (not in search_service.cc) because it
// drives the file-scope request pipeline above -- process_header, url_resolve,
// dispatch_request, the *_view functions, emit_response -- all internal to this
// translation unit. When http::HttpConnection serves a request it calls handle();
// the legacy HttpClient path is unchanged and retires once the swap is proven.
// ---------------------------------------------------------------------------

// Map an HTTP method name (as http-parser parsed it into http::Request.method,
// e.g. "COUNT") back to the enum the dispatch switches on. Covers Xapiand's custom
// verbs via the same METHODS_OPTIONS table used for the URL-command / override maps.
static enum http_method
method_from_string(std::string_view m)
{
	auto lower = strings::lower(m);
	switch (http_methods.fhhl(lower)) {
		#define OPTION(name, str) \
		case http_methods.fhhl(str): \
			return HTTP_##name;
		METHODS_OPTIONS()
		#undef OPTION
		default:
			return static_cast<enum http_method>(0xff);  // unknown -> dispatch default -> 405
	}
}


std::unique_ptr<http::RequestExtension>
SearchService::create_extension(const http::Request& /*hreq*/)
{
	// Allocate Xapiand's per-request state as the http::Request's extension. It is
	// populated in handle() (which runs guarded by the connection's error backstop),
	// because that setup -- process_header()'s Accept parsing + method override -- can
	// throw, and create_extension() runs unguarded in the connection's parse path.
	return std::make_unique<Request>();
}


bool
SearchService::wants_body_stream(const http::Request& hreq)
{
	// Stream a whole-database RESTORE with a bulk body (NDJSON / MsgPack, no doc id): the
	// library runs handle() on a worker and feeds the body through Request::body_reader,
	// which restore_database_view pulls object-by-object -- O(buffer) memory for a dump of
	// any size. A single-document RESTORE (id present) and every other request buffer.
	if (method_from_string(hreq.method) != HTTP_RESTORE) { return false; }
	if (hreq.header("Content-Length").empty()) { return false; }
	ct_type_t ct(hreq.content_type());
	if (ct != ndjson_type && ct != x_ndjson_type && ct != msgpack_type && ct != x_msgpack_type) { return false; }
	PathParser pp;
	if (pp.init(hreq.path) >= PathParser::State::END) { return false; }   // invalid path
	return pp.get_id().empty();                                           // no id -> whole database
}


void
SearchService::handle(const http::Request& hreq, http::ResponseWriter& response)
{
	Request& request = hreq.ext<Request>();   // the extension create_extension() built
	request.http_req = &hreq;                 // the source of truth for every HTTP fact
	request.response_writer = &response;       // the response path emits through the library writer

	// HTTP facts from the library request (keep-alive/close, version).
	request.http_major = hreq.http_major;
	request.http_minor = hreq.http_minor;
	request.keep_alive = hreq.keep_alive;

	request.method = method_from_string(hreq.method);
	request.path = hreq.target;          // url_resolve() splits path + query out of this

	// A request carrying a body advertises it with a Content-Length and/or a
	// Content-Type; mirror that so dispatch_request() picks the same body mode the
	// legacy parser would (a RESTORE/bulk with an NDJSON/MsgPack content-type streams,
	// and streams gracefully even when the body is empty). content_length feeds the
	// buffer reserve.
	if (!hreq.header("Content-Length").empty() || !hreq.content_type().empty() || !hreq.body.empty()) {
		request.has_content_length = true;
	}
	{
		auto cl = hreq.header("Content-Length");
		if (!cl.empty()) {
			request.content_length = static_cast<size_t>(std::strtoull(std::string(cl).c_str(), nullptr, 10));
		} else {
			request.content_length = hreq.body.size();
		}
	}

	// Replay the header processing the parser callback does (accept / accept-encoding /
	// content-type / expect / method-override), so content negotiation matches exactly.
	for (const auto& kv : hreq.headers) {
		process_header(request, kv.first, kv.second);
	}

	request.atom_ending = true;
	request.atom_ended = true;
	request.ending = true;
	request.received = std::chrono::steady_clock::now();

	// Dispatch, then feed the body (if not streamed) and run the selected view.
	// Xapian/ClientError exceptions propagate to the connection, which calls
	// SearchService::on_error() -- the app's error-to-status mapping.
	if (dispatch_request(request) == 0 && request.view != nullptr) {
		// A FULL request reads its body straight from http_req (via body_view() /
		// decoded_body()) -- no copy. A buffered streamed request (a whole-DB RESTORE
		// under the buffer path) feeds append() once, which parses the chunks into the
		// objects deque. When the body is streamed concurrently (body_reader set), the
		// view's next_object() pulls + append()s each chunk itself, so nothing is fed
		// here -- the body flows in bounded chunks while the view indexes them.
		if (request.mode != Request::Mode::FULL && hreq.body_reader == nullptr) {
			if (!hreq.body.empty()) {
				request.append(hreq.body.data(), hreq.body.size());
			}
			request.append(nullptr, 0);  // flush any trailing streamed object
		}
		request.view(request);
	}

	if (request.closing) {
		response.set_close();
	}
}


// The application's error-to-status mapping, invoked by the http::HttpConnection when
// handle() throws without having answered (the library owns only the generic 500
// fallback). Rethrow to inspect the type; catch_http_errors maps Xapian / ClientError /
// Datetime / Cartesian to the right status, then write_status_response emits it -- the
// same mapping handled_errors did on the legacy path.
void
SearchService::on_error(std::exception_ptr error, const http::Request& hreq, http::ResponseWriter& response)
{
	Request& request = hreq.ext<Request>();

	auto http_errors = catch_http_errors([&]{ std::rethrow_exception(error); return 0; });
	if (request.response_status == static_cast<http_status>(0)) {
		write_status_response(request, http_errors.error_code, http_errors.error);
	} else {
		// A response was already emitted before the error; the writer is one-shot,
		// so just close the connection (mirrors handled_errors' detach()).
		response.set_close();
	}

	if (request.closing) {
		response.set_close();
	}
}


// The options for the http_log::AccessLog middleware wrapping SearchService (see
// search_service.h). The body prettifier decodes a JSON / YAML / MsgPack body back to
// a MsgPack and renders it with DEFAULT_INDENTATION, reproducing what Request::to_text
// / Response::to_text did before the Response class was removed. The previewable-type
// set and per-status levels use http-log's Xapiand-faithful defaults.
http_log::Options
make_http_log_options()
{
	http_log::Options options;
	options.prettify = [](std::string_view ct, std::string_view body) -> std::optional<std::string> {
		try {
			MsgPack decoded;
			rapidjson::Document rdoc;
			if (ct.find("json") != std::string_view::npos) {
				json_load(rdoc, body);
				decoded = MsgPack(rdoc);
			} else if (ct.find("yaml") != std::string_view::npos) {
				yaml_load(rdoc, body);
				decoded = MsgPack(rdoc);
			} else if (ct.find("msgpack") != std::string_view::npos) {
				decoded = MsgPack::unserialise(body);
			} else {
				return std::nullopt;
			}
			return decoded.to_string(DEFAULT_INDENTATION);
		} catch (...) {
			return std::nullopt;
		}
	};
	return options;
}
