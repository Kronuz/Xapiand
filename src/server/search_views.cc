/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "config.h"                         // for XAPIAND_LUA, XAPIAND_DATABASE_WAL

#include <cassert>                          // for assert
#include <charconv>                         // for std::from_chars (indent param)
#include <errno.h>                          // for errno
#include <exception>                        // for std::exception
#include <functional>                       // for std::function
#include <optional>                         // for std::optional
#include <signal.h>                         // for SIGTERM
#include <sysexits.h>                       // for EX_SOFTWARE
#include <syslog.h>                         // for LOG_WARNING, LOG_ERR, LOG...
#include <utility>                          // for std::move

#ifdef XAPIAND_LUA
#include <lua.h>                              // for LUA_VERSION_MAJOR, LUA_VERSION_MINOR
#endif
#include "cppcodec/base64_rfc4648.hpp"      // for cppcodec::base64_rfc4648
#include "database/handler.h"               // for DatabaseHandler, DocIndexer
#include "engine/host_asio.h"               // for engine::AsioHost
#include "engine/census.h"                  // for ENGINE_CENSUS_REQUEST
#include "engine/error.h"                   // for engine::Error
#include "database/utils.h"                 // for query_field_t
#include "database/pool.h"                  // for DatabasePool
#include "database/schema.h"                // for Schema
#include "endpoint.h"                       // for Endpoints, Endpoint
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "exception_engine.h"               // for Exception, SerialisationError
#include "field_parser.h"                   // for FieldParser, FieldParserError
#include "hashes.hh"                        // for hhl
#include "http_utils.h"                     // for catch_http_errors
#include "http_accept.h"                    // for http::AcceptCache (content negotiation)
#include "http_asio.h"                      // for http::HttpAsioService (the reactors' admission gates)
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
#include "engine/query.h"                   // for engine::Query
#include "aggregations/aggregations.h"      // for AggregationMatchSpy
#include "node.h"                           // for Node::local_node
#include "index_pattern.h"                   // for index_pattern::expand
#include "walk_gate.h"                       // for index_pattern::WalkGate
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
#include "p1_probe.h"           // P1 diagnostic probes


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
static task::task<MsgPack> retrieve_database(Request& request, const query_field_t& query_field, std::string_view selector);
static task::task<std::vector<std::string>> expand_paths(Request& request, bool patterns);
static task::task<size_t> resolve_index_endpoints(Request& request, const query_field_t& query_field, const MsgPack* settings = nullptr);

// The response/transport helpers lifted off HttpClient `this` (Leg 2 stage 3b). The
// response is formatted from `request` state (content negotiation, serialization) and
// emitted through the library's ResponseWriter, which frames the bytes and applies the
// negotiated Content-Encoding. Forward-declared here since prepare()/views call them above.
static void write_status_response(Request& request, enum http_status status, const std::string& message = "", std::optional<engine::rev> current_version = std::nullopt);
static void write_http_response(Request& request, enum http_status status, const MsgPack& obj = MsgPack(), const std::string& location = "", const ct_type_t& ct_type = no_type);

// The ~23 endpoint views lifted off HttpClient `this` (Leg 2 stage 3c-3). Their
// bodies use only their `request` argument + the file-scope helpers above +
// the manager/DB singletons -- no HttpClient/connection state -- so they are
// free functions, dispatched through a plain function pointer (Request::view is
// now `void(*)(Request&)`). This is what SearchService::handle() will call.
// Forward-declared here because prepare() takes their address before their defs.
static task::task<void> metrics_view(Request& request);
static task::task<void> info_view(Request& request);
static task::task<void> retrieve_metadata_view(Request& request);
static task::task<void> write_metadata_view(Request& request);
static task::task<void> update_metadata_view(Request& request);
static task::task<void> delete_metadata_view(Request& request);
static task::task<void> document_exists_view(Request& request);
static task::task<void> retrieve_document_view(Request& request);
static task::task<void> write_document_view(Request& request);
static task::task<void> update_document_view(Request& request);
static task::task<void> delete_document_view(Request& request);
static task::task<void> dump_document_view(Request& request);
static task::task<void> database_exists_view(Request& request);
static task::task<void> retrieve_database_view(Request& request);
static task::task<void> write_database_view(Request& request);
static task::task<void> delete_database_view(Request& request);
static task::task<void> dump_database_view(Request& request);
static task::task<void> restore_database_view(Request& request);
static task::task<void> check_database_view(Request& request);
static task::task<void> commit_database_view(Request& request);
static task::task<void> search_view(Request& request);
static task::task<void> count_view(Request& request);
static task::task<void> flush_view(Request& request);
static task::task<void> quit_view(Request& request);
#if XAPIAND_DATABASE_WAL
static task::task<void> wal_view(Request& request);
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








// The verbs the route table answers, for OPTIONS' Allow header.
#if XAPIAND_DATABASE_WAL
#define ALLOWED_WAL ", WAL"
#else
#define ALLOWED_WAL ""
#endif
static constexpr const char* ALLOWED_METHODS = "GET, POST, PUT, PATCH, UPDATE, UPSERT, DELETE, HEAD, OPTIONS, SEARCH, COUNT, INFO, COMMIT, DUMP, RESTORE, CHECK, FLUSH, QUIT" ALLOWED_WAL;


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
		writer.set_header("Allow", ALLOWED_METHODS);
	}
	if (request.retry_after != 0) {
		writer.set_header("Retry-After", std::to_string(request.retry_after));
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

	if (request.method == HTTP_HEAD) {
		// A HEAD response has no body (RFC 9110 9.3.2): it names the length the GET would send.
		writer.set_header("Content-Length", std::to_string(body.size()));
		writer.end();
		return;
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









// The longest a request runs before it yields its reactor (an estimate; docs/design/phase7c.md D2).
static constexpr std::chrono::microseconds REQUEST_SLICE{200};

// The slices a request spends light, ahead of the heavy lane, before it turns heavy (least attained service,
// docs/design/loop-scheduling.md D1), and the slices each body chunk of a streamed RESTORE gets light again (D82).
static constexpr unsigned REQUEST_LIGHT_SLICES = 8;
static constexpr unsigned REQUEST_STREAM_SLICES = 4;

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

	// Trivial verbs answered inline (no database work): OPTIONS
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
	// (request.http_req->body_stream set): that body is pulled in bounded chunks, so
	// reserving its full (possibly multi-gigabyte) length would defeat the point.
	bool streaming = request.http_req != nullptr && request.http_req->body_stream != nullptr;
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

	auto local_node = Node::get_local_node();

	MsgPack obj({
		{ "name", local_node->name() },
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
#ifdef XAPIAND_LUA
			{ "Lua", LUA_VERSION_MAJOR "." LUA_VERSION_MINOR },
#endif
		} },
		{ "options", {
			{ "verbosity", opts.verbosity },
			{ "processors", opts.processors },
			{ "limits", {
				// { "max_clients", opts.max_clients },
			} },
			{ "cache", {
				{ "database_pool_size", opts.database_pool_size },
				{ "schema_pool_size", opts.schema_pool_size },
				{ "scripts_cache_size", opts.scripts_cache_size },
			} },
			{ "thread_pools", {
				{ "num_shards", opts.num_shards },
				{ "num_replicas", opts.num_replicas },
				{ "num_http_servers", opts.num_http_servers },
				{ "num_async_wal_writers", opts.num_async_wal_writers },
				{ "num_fsynchers", opts.num_fsynchers },
			} },
		} },
	});

	return obj;
}


bool
is_node_route(PathParser& path_parser)
{
	return !path_parser.has_pth() && path_parser.get_id().empty();
}


MsgPack
node_info(std::string_view selector)
{
	auto manager = XapiandManager::manager();
	if (selector == "state" && manager) {
		return enum_name(manager->state.load());
	}
	auto obj = MsgPack::MAP();
	obj.update(node_obj());
	if (manager) {
		obj["state"] = enum_name(manager->state.load());
	}
	if (!selector.empty()) {
		obj = obj.select(selector);
	}
	return obj;
}


static task::task<void>
metrics_view(Request& request)
{
	L_CALL("metrics_view()");

	request.processing = std::chrono::steady_clock::now();

	auto server_info =  XapiandManager::server_metrics();
	server_info += p1::text();
	emit_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_LENGTH_RESPONSE | HTTP_BODY_RESPONSE, server_info, "", "text/plain", "", server_info.size());
	co_return;
}


static task::task<void>
document_exists_view(Request& request)
{
	L_CALL("document_exists_view()");

	auto query_field = query_field_maker(request, 0);
	co_await resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN);

	co_await (co_await db_handler.get_document(request.path_parser.get_id())).validate();

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


static task::task<void>
delete_document_view(Request& request)
{
	L_CALL("delete_document_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	std::string document_id(request.path_parser.get_id());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	co_await db_handler.delete_document(document_id, query_field.version, query_field.commit);
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


// Whether a body of this content type is stored as a blob (not indexed as a JSON, YAML or MsgPack document).
static bool
is_stored_type(const ct_type_t& ct_type)
{
	return !ct_type.empty() && ct_type != json_type && ct_type != x_json_type && ct_type != yaml_type && ct_type != x_yaml_type && ct_type != msgpack_type && ct_type != x_msgpack_type;
}


// The body of a request, one chunk at a time, for an upload.
class RequestChunks final : public DatabaseHandler::BodyChunks {
public:
	explicit RequestChunks(Request& request) : request(request) { }
	task::task<bool> next(std::string& chunk) override {
		return request.read_chunk(chunk);
	}

private:
	Request& request;
};


// Indexes a document whose blob is the request's body, streamed to the data storage in chores of the upload's own
// group, a chunk at a time (docs/design/phase7c.md D9): no turn is held while the body arrives.
static task::task<DocumentInfo>
index_streamed_blob(Request& request, DatabaseHandler& db_handler, const MsgPack& document_id, engine::rev version, const ct_type_t& ct_type, bool commit)
{
	if (request.content_length > STORED_UPLOAD_LIMIT) {
		THROW(PayloadTooLarge, "A stored blob has a size limit of {}", strings::from_bytes(STORED_UPLOAD_LIMIT));
	}
	IndexChores chores(XapiandManager::manager(true)->database_pool->work_pool());
	RequestChunks body(request);
	auto blob = co_await db_handler.upload_stored(document_id, ct_type, request.content_length, chores, body);
	co_return co_await db_handler.index_uploaded(document_id, version, ct_type, blob, commit);
}


static task::task<void>
write_document_view(Request& request)
{
	L_CALL("write_document_view()");

	// A blob too big to buffer arrives as a stream and is written to the data storage as it comes (D9): the
	// document has no decoded body.
	const bool streamed = request.http_req != nullptr && request.http_req->body_stream != nullptr;
	MsgPack streamed_body;
	auto& decoded_body = streamed ? streamed_body : request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (co_await resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	auto document_id = request.path_parser.get_id();

	request.processing = std::chrono::steady_clock::now();
	const auto p1_cls = p1::cls_of(request.endpoints.empty() ? std::string_view{} : std::string_view(request.endpoints[0].path));
	p1::add(p1::put_resolve, p1_cls, request.begins, request.processing);

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
	task::budget script_budget(REQUEST_SLICE, {}, REQUEST_LIGHT_SLICES);   // a script yields its reactor between slices; an admitted write is not cancelled
	db_handler.budget = &script_budget;
	bool stored = is_stored_type(request.ct_type);
	auto ct_type = request.ct_type.empty() ? mime_type(selector) : request.ct_type;
	auto indexed = streamed
		? co_await index_streamed_blob(request, db_handler, document_id, query_field.version, ct_type, query_field.commit)
		: co_await db_handler.index(document_id, query_field.version, stored, decoded_body, query_field.commit, ct_type);

	request.ready = std::chrono::steady_clock::now();

	std::string location;

	auto info = indexed.first;
	auto& response_obj = indexed.second;

	Document document(info.did, &db_handler);

	if (request.echo) {
		auto it = response_obj.find(ID_FIELD_NAME);
		if (document_id.empty()) {
			if (it == response_obj.end()) {
				auto document_id_obj = co_await db_handler.unserialise_term_id(info.term);
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, document_id_obj.as_str());
				response_obj[ID_FIELD_NAME] = std::move(document_id_obj);
			} else {
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, it.value().as_str());
			}
		} else {
			if (it == response_obj.end()) {
				response_obj[ID_FIELD_NAME] = co_await db_handler.unserialise_term_id(info.term);
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
				auto document_id_obj = co_await db_handler.unserialise_term_id(info.term);
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, document_id_obj.as_str());
			} else {
				location = strings::format("/{}/{}", unsharded_path(request.endpoints[0].path).first, it.value().as_str());
			}
		}

		write_http_response(request, document_id.empty() ? HTTP_STATUS_CREATED : HTTP_STATUS_NO_CONTENT, MsgPack(), location);
	}

	p1::add(p1::put_index, p1_cls, request.processing, request.ready);
	if (request.ends != p1::clock::time_point{}) {
		p1::add(p1::put_tail, p1_cls, request.ready, request.ends);
		p1::add(p1::put_total, p1_cls, request.begins, request.ends);
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


static task::task<void>
update_document_view(Request& request)
{
	L_CALL("update_document_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT);
	if (co_await resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	auto document_id = request.path_parser.get_id();
	assert(!document_id.empty());

	request.processing = std::chrono::steady_clock::now();

	std::string operation;
	DocumentInfo indexed;
	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
	task::budget script_budget(REQUEST_SLICE, {}, REQUEST_LIGHT_SLICES);   // a script yields its reactor between slices; an admitted write is not cancelled
	db_handler.budget = &script_budget;
	if (request.method == HTTP_PATCH) {
		operation = "patch";
		indexed = co_await db_handler.patch(document_id, query_field.version, false, decoded_body, query_field.commit);
	} else if (request.method == HTTP_UPSERT) {
		operation = "update";
		bool stored = !request.ct_type.empty() && request.ct_type != json_type && request.ct_type != x_json_type && request.ct_type != yaml_type && request.ct_type != x_yaml_type && request.ct_type != msgpack_type && request.ct_type != x_msgpack_type;
		indexed = co_await db_handler.update(document_id, query_field.version, stored, true, decoded_body, query_field.commit, request.ct_type.empty() ? mime_type(selector) : request.ct_type);
	} else {
		operation = "update";
		bool stored = !request.ct_type.empty() && request.ct_type != json_type && request.ct_type != x_json_type && request.ct_type != yaml_type && request.ct_type != x_yaml_type && request.ct_type != msgpack_type && request.ct_type != x_msgpack_type;
		indexed = co_await db_handler.update(document_id, query_field.version, stored, false, decoded_body, query_field.commit, request.ct_type.empty() ? mime_type(selector) : request.ct_type);
	}

	request.ready = std::chrono::steady_clock::now();

	if (request.echo) {
		auto info = indexed.first;
		auto& response_obj = indexed.second;

		Document document(info.did, &db_handler);

		if (response_obj.find(ID_FIELD_NAME) == response_obj.end()) {
			response_obj[ID_FIELD_NAME] = co_await db_handler.unserialise_term_id(info.term);
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


static task::task<void>
retrieve_metadata_view(Request& request)
{
	L_CALL("retrieve_metadata_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
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
		for (auto& _key : co_await db_handler.get_metadata_keys()) {
			auto metadata = co_await db_handler.get_metadata(_key);
			if (!metadata.empty()) {
				response_obj[_key] = MsgPack::unserialise(metadata);
			}
		}
	} else {
		auto metadata = co_await db_handler.get_metadata(key);
		if (metadata.empty()) {
			throw engine::Error(engine::errc::doc_not_found, "Metadata not found");
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


static task::task<void>
write_metadata_view(Request& request)
{
	L_CALL("write_metadata_view()");

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
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

	if (key.empty() || key == "schema" || key == "wal" || key == "metrics" || key == ID_HIGH_METADATA) {
		THROW(ClientError, "Metadata {} is read-only", repr(request.path_parser.get_cmd()));
	}

	co_await db_handler.set_metadata(key, decoded_body.serialise());

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


static task::task<void>
update_metadata_view(Request& request)
{
	L_CALL("update_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
	co_return;
}


static task::task<void>
delete_metadata_view(Request& request)
{
	L_CALL("delete_metadata_view()");

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
	co_return;
}


static task::task<void>
info_view(Request& request)
{
	L_CALL("info_view()");

	MsgPack response_obj;

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (is_node_route(request.path_parser)) {
		auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;
		write_http_response(request, HTTP_STATUS_OK, node_info(selector));
		co_return;
	}
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
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

		response_obj = co_await db_handler.get_document_info(id, raw, request.human);
	} else {
		response_obj = co_await db_handler.get_database_info();
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


static task::task<void>
database_exists_view(Request& request)
{
	L_CALL("database_exists_view()");

	// HEAD / asks whether the node answers; it names no index.
	if (is_node_route(request.path_parser)) {
		write_http_response(request, HTTP_STATUS_OK);
		co_return;
	}

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

	co_await db_handler.reopen();  // Ensure it can be opened.

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK);
}


static task::task<MsgPack>
retrieve_database(Request& request, const query_field_t& query_field, std::string_view selector)
{
	L_CALL("retrieve_database()");

	auto obj = MsgPack::MAP();

	// Get active schema
	MsgPack schema;
	{
		DatabaseHandler db_handler;
		if (query_field.writable || query_field.primary) {
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
		}

		// Retrieve full schema
		schema = (co_await db_handler.get_schema())->get_full(true);
	}

	// Get index settings (from the reserved indexes database)
	MsgPack settings;
	auto id = std::string(request.endpoints.size() == 1 ? request.endpoints[0].path : unsharded_path(request.endpoints[0].path).first);
	request.endpoints = co_await XapiandManager::resolve_index_endpoints(
		Endpoint{".xapiand/indexes"},
		query_field.writable);

	DatabaseHandler db_handler;
	if (query_field.writable || query_field.primary) {
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
	}

	// Retrive document ID
	auto did = co_await db_handler.get_docid(id);

	// Retrive document data
	auto document = db_handler.get_document(did);
	settings = co_await document.get_obj();

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

	if (!settings.empty()) {
		obj[RESERVED_SETTINGS].update(settings);
	}

	if (!schema.empty()) {
		obj[RESERVED_SCHEMA].update(schema);
	}

	if (!selector.empty()) {
		obj = obj.select(selector);
	}

	co_return obj;
}


static task::task<void>
retrieve_database_view(Request& request)
{
	L_CALL("retrieve_database_view()");

	assert(request.path_parser.get_id().empty());

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE);
	if (is_node_route(request.path_parser)) {
		auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;
		write_http_response(request, HTTP_STATUS_OK, node_info(selector));
		co_return;
	}

	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	auto obj = co_await retrieve_database(request, query_field, selector);

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


static task::task<void>
write_database_view(Request& request)
{
	L_CALL("write_database_view()");

	assert(request.path_parser.get_id().empty());

	auto& decoded_body = request.decoded_body();

	auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE);
	if (co_await resolve_index_endpoints(request, query_field, &decoded_body) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);
	task::budget script_budget(REQUEST_SLICE, {}, REQUEST_LIGHT_SLICES);   // a script yields its reactor between slices; an admitted write is not cancelled
	db_handler.budget = &script_budget;

	if (decoded_body.is_map()) {
		auto schema_it = decoded_body.find(RESERVED_SCHEMA);
		if (schema_it != decoded_body.end()) {
			auto& schema = schema_it.value();
			if (request.method == HTTP_UPDATE) {
				co_await db_handler.update_schema(schema);
			} else {
				co_await db_handler.write_schema(schema);
			}
		}
	}

	co_await db_handler.reopen();  // Ensure touch.

	request.ready = std::chrono::steady_clock::now();

	if (request.echo) {
		auto obj = co_await retrieve_database(request, query_field, selector);

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


static task::task<void>
delete_database_view(Request& request)
{
	L_CALL("delete_database_view()");

	// Deleting a database needs a path to name it; without one there is nothing to
	// address (prepare() answered this 405 before ever selecting the view).
	if (!request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		co_return;
	}

	write_status_response(request, HTTP_STATUS_NOT_IMPLEMENTED);
}


// Administrative verbs. FLUSH closes and commits every open writable and awaits the closes; QUIT only
// signals. Their admin_commands + root-URL preconditions are checked here at the view.
static task::task<void>
flush_view(Request& request)
{
	L_CALL("flush_view()");

	if (!opts.admin_commands || request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		co_return;
	}

	// Flush both databases and clients by default (unless one is specified).
	request.query_parser.rewind();
	int flush_databases = request.query_parser.next("databases");
	request.query_parser.rewind();
	int flush_clients = request.query_parser.next("clients");
	if (flush_databases != -1 || flush_clients == -1) {
		co_await XapiandManager::manager(true)->database_pool->flush(request.http_req != nullptr ? request.http_req->cancel_token() : task::cancel_token{});
	}
	if (flush_clients != -1 || flush_databases == -1) {
		XapiandManager::manager(true)->shutdown(0, 0);
	}

	write_http_response(request, HTTP_STATUS_OK);
}


static task::task<void>
quit_view(Request& request)
{
	L_CALL("quit_view()");

	if (!opts.admin_commands || request.path_parser.has_pth()) {
		write_status_response(request, HTTP_STATUS_METHOD_NOT_ALLOWED);
		co_return;
	}

	XapiandManager::try_shutdown(true);
	write_http_response(request, HTTP_STATUS_OK);
	request.closing = true;
}


static task::task<void>
commit_database_view(Request& request)
{
	L_CALL("commit_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	co_await resolve_index_endpoints(request, query_field);

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE);

	co_await db_handler.commit();  // Ensure touch.

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


static task::task<void>
dump_document_view(Request& request)
{
	L_CALL("dump_document_view()");

	auto query_field = query_field_maker(request, 0);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto document_id = request.path_parser.get_id();
	assert(!document_id.empty());

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

	auto obj = co_await db_handler.dump_document(document_id);

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


static task::task<void>
dump_database_view(Request& request)
{
	L_CALL("dump_database_view()");

	auto query_field = query_field_maker(request, 0);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints, DB_OPEN);

	auto& ct_type = resolve_ct_type(request);
	if (ct_type.empty()) {
		// No content type could be resolved, return NOT ACCEPTABLE.
		write_status_response(request, HTTP_STATUS_NOT_ACCEPTABLE, "Response type not accepted by the Accept header");
		co_return;
	}

	auto docs = co_await db_handler.dump_documents();

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


static task::task<void>
restore_database_view(Request& request)
{
	L_CALL("restore_database_view()");

	// The restore's own group of chores: the parse of each chunk and the prepare of each batch run there, beside
	// the apply of the previous objects, and split the pool's service with the index's own group.
	IndexChores chores(XapiandManager::manager(true)->database_pool->work_pool());
	request.parse_chores = &chores;

	std::optional<DocIndexer> indexer;
	std::exception_ptr error;
	try {
		auto open_indexer = [&](MsgPack& obj) -> task::task<void> {
			auto query_field = query_field_maker(request, QUERY_FIELD_WRITABLE | QUERY_FIELD_COMMIT | QUERY_FIELD_OFFSET);
			if (co_await resolve_index_endpoints(request, query_field, &obj) > 1) {
				THROW(ClientError, "Method can only be used with single indexes");
			}

			request.processing = std::chrono::steady_clock::now();

			indexer.emplace(request.endpoints, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_RESTORE | DB_DISABLE_AUTOCOMMIT, request.echo, request.comments, query_field, chores, &*request.budget);
		};

		if (request.mode == Request::Mode::STREAM_MSGPACK || request.mode == Request::Mode::STREAM_NDJSON) {
			co_await request.feed_whole_body();
			std::vector<MsgPack> batch;
			bool more = co_await request.next_batch(batch);
			while (more) {
				if (!indexer) {
					co_await open_indexer(batch.front());
				}
				// The next chunk is read and parsed while this batch is prepared and applied. Both start light: the
				// rearm is here, before the children start, since the join goes on in its last child's lane.
				std::vector<MsgPack> next;
				bool next_more = false;
				request.budget->rearm_light(REQUEST_STREAM_SLICES);
				co_await task::when_all(indexer->index(batch), [&]() -> task::task<void> {
					next_more = co_await request.next_batch(next);
				}());
				batch = std::move(next);
				more = next_more;
			}
		} else {
			auto& docs = request.decoded_body();
			if (!docs.is_array()) {
				THROW(ClientError, "Invalid request body");
			}
			std::vector<MsgPack> batch;
			for (auto& obj : docs) {
				batch.push_back(std::move(obj));
			}
			if (!batch.empty()) {
				co_await open_indexer(batch.front());
				co_await indexer->index(batch);
			}
		}

		if (request.ending) {
			request.ready = std::chrono::steady_clock::now();
			auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(request.ready - request.processing).count();

			if (indexer) {
				co_await indexer->commit();
			}

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
	} catch (...) {
		error = std::current_exception();
	}

	// What was indexed before a failure stays: the closing commit runs on every exit.
	if (error && indexer) {
		try {
			co_await indexer->commit();
		} catch (...) {
			L_EXC("Unhandled exception committing the restore");
		}
	}
	request.parse_chores = nullptr;
	if (error) {
		std::rethrow_exception(error);
	}
}


#if XAPIAND_DATABASE_WAL
static task::task<void>
wal_view(Request& request)
{
	L_CALL("wal_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints);

	request.query_parser.rewind();
	bool unserialised = request.query_parser.next("raw") == -1;
	auto obj = co_await db_handler.repr_wal(0, std::numeric_limits<engine::rev>::max(), unserialised);

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


static task::task<void>
check_database_view(Request& request)
{
	L_CALL("check_database_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_PRIMARY);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	request.processing = std::chrono::steady_clock::now();

	DatabaseHandler db_handler(request.endpoints);

	auto status = co_await db_handler.check();

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


static task::task<void>
retrieve_document_view(Request& request)
{
	L_CALL("retrieve_document_view()");

	auto id = request.path_parser.get_id();

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_ID);
	if (co_await resolve_index_endpoints(request, query_field) > 1) {
		THROW(ClientError, "Method can only be used with single indexes");
	}

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	request.processing = std::chrono::steady_clock::now();
	const auto p1_cls = p1::cls_of(request.endpoints.empty() ? std::string_view{} : std::string_view(request.endpoints[0].path));
	p1::add(p1::get_resolve, p1_cls, request.begins, request.processing);

	// Open database
	DatabaseHandler db_handler;
	if (query_field.primary) {
		db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
	} else {
		db_handler.reset(request.endpoints, DB_OPEN);
	}

	// Retrive document ID
	engine::docid did;
	did = co_await db_handler.get_docid(id);
	const auto p1_docid = p1::clock::now();
	p1::add(p1::get_docid, p1_cls, request.processing, p1_docid);

	// Retrive document data
	auto document = db_handler.get_document(did);
	auto document_data = co_await document.get_data();
	const auto p1_data = p1::clock::now();
	p1::add(p1::get_data, p1_cls, p1_docid, p1_data);
	const Data data(document_data.empty() ? std::string(DATABASE_DATA_MAP) : std::move(document_data));
	auto selector_mime_type = mime_type(selector);
	auto accepted = data.get_accepted(request.accept_set, selector_mime_type);
	if (accepted.first == nullptr) {
		// No content type could be resolved, return NOT ACCEPTABLE.
		write_status_response(request, HTTP_STATUS_NOT_ACCEPTABLE, "Response type not accepted by the Accept header");
		co_return;
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
			obj[ID_FIELD_NAME] = co_await document.get_value(ID_FIELD_NAME);
		}

		if (obj.find(VERSION_FIELD_NAME) == obj.end()) {
			auto version = co_await document.get_value(DB_SLOT_VERSION);
			if (!version.empty()) {
				obj[VERSION_FIELD_NAME] = static_cast<engine::rev>(sortable_unserialise(version));
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
				auto stored = co_await db_handler.storage_get_stored(locator, did);
				request.response_blob = unserialise_string_at(STORED_BLOB, stored);
			}
		}
#endif

		request.ready = std::chrono::steady_clock::now();

		request.response_ct_type = ct_type;
		emit_response(request, HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_BODY_RESPONSE, request.response_blob, "", ct_type.to_string());
	}

	if (request.ends != p1::clock::time_point{}) {
		p1::add(p1::get_tail, p1_cls, p1_data, request.ends);
		p1::add(p1::get_total, p1_cls, request.begins, request.ends);
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


static task::task<void>
search_view(Request& request)
{
	L_CALL("search_view()");

	std::string selector_string_holder;

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_SEARCH);
	co_await resolve_index_endpoints(request, query_field);

	auto selector = query_field.selector.empty() ? request.path_parser.get_slc() : query_field.selector;

	std::tuple<engine::SearchResult, MsgPack, engine::Query> mset_tuple;
	MsgPack aggregations;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	db_handler.budget = &*request.budget;   // the search yields its reactor at window boundaries
	try {
		if (request.endpoints.empty()) {
			throw engine::Error(engine::errc::not_found, "No index matches the pattern");   // answered as a search that found nothing
		}
		if (query_field.primary) {
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
		}

		if (request.body_view().empty()) {
			mset_tuple = co_await db_handler.get_mset(query_field, nullptr, nullptr);
		} else {
			auto& decoded_body = request.decoded_body();
			if (!decoded_body.is_map()) {
				THROW(ClientError, "Invalid request body");
			}

			AggregationMatchSpy aggs(decoded_body, co_await db_handler.get_schema());

			if (decoded_body.find(RESERVED_QUERYDSL_SELECTOR) != decoded_body.end()) {
				auto selector_obj = decoded_body.at(RESERVED_QUERYDSL_SELECTOR);
				if (selector_obj.is_string()) {
					selector_string_holder = selector_obj.as_str();
					selector = selector_string_holder;
				} else {
					THROW(ClientError, "The {} must be a string", RESERVED_QUERYDSL_SELECTOR);
				}
			}

			mset_tuple = co_await db_handler.get_mset(query_field, &decoded_body, &aggs);
			aggregations = aggs.get_aggregation().at(RESERVED_AGGS_AGGREGATIONS);
		}
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::not_found) { throw; }
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
		obj["#engine_query"] = query.description();
	}
	obj[RESPONSE_COUNT] = mset.size();
	obj[RESPONSE_TOTAL] = mset.estimated;

	auto& hits = obj[RESPONSE_HITS];

	for (const auto& hit : mset.hits) {
		co_await request.budget->checkpoint();   // the hits yield the loop between slices, as the search's windows do
		auto did = hit.did;

		// Retrive document data
		auto document = db_handler.get_document(did);
		auto document_data = co_await document.get_data();
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
			hit_obj[ID_FIELD_NAME] = co_await document.get_value(ID_FIELD_NAME);
		}

		if (hit_obj.find(VERSION_FIELD_NAME) == hit_obj.end()) {
			auto version = co_await document.get_value(DB_SLOT_VERSION);
			if (!version.empty()) {
				hit_obj[VERSION_FIELD_NAME] = static_cast<engine::rev>(sortable_unserialise(version));
			}
		}

		if (request.comments) {
			hit_obj[RESPONSE_xDOCID] = did;

			size_t n_shards = request.endpoints.size();
			size_t shard_num = (did - 1) % n_shards;
			hit_obj[RESPONSE_xSHARD] = shard_num + 1;
			// hit_obj[RESPONSE_xENDPOINT] = request.endpoints[shard_num].to_string();

			hit_obj[RESPONSE_xRANK] = hit.rank;
			hit_obj[RESPONSE_xWEIGHT] = hit.weight;
			hit_obj[RESPONSE_xPERCENT] = hit.percent;
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


static task::task<void>
count_view(Request& request)
{
	L_CALL("count_view()");

	auto query_field = query_field_maker(request, QUERY_FIELD_VOLATILE | QUERY_FIELD_SEARCH);
	co_await resolve_index_endpoints(request, query_field);

	std::tuple<engine::SearchResult, MsgPack, engine::Query> mset_tuple;

	request.processing = std::chrono::steady_clock::now();

	// Open database
	DatabaseHandler db_handler;
	db_handler.budget = &*request.budget;   // the search yields its reactor at window boundaries
	try {
		if (request.endpoints.empty()) {
			throw engine::Error(engine::errc::not_found, "No index matches the pattern");   // answered as a search that found nothing
		}
		if (query_field.primary) {
			db_handler.reset(request.endpoints, DB_OPEN | DB_WRITABLE);
		} else {
			db_handler.reset(request.endpoints, DB_OPEN);
		}

		if (request.body_view().empty()) {
			mset_tuple = co_await db_handler.get_mset(query_field, nullptr, nullptr);
		} else {
			auto& decoded_body = request.decoded_body();
			if (!decoded_body.is_map()) {
				THROW(ClientError, "Invalid request body");
			}
			mset_tuple = co_await db_handler.get_mset(query_field, &decoded_body, nullptr);
		}
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::not_found) { throw; }
		/* At the moment when the endpoint does not exist and it is chunck it will return 200 response
		 * with zero matches this behavior may change in the future for instance ( return 404 ) */
	}

	auto& mset = std::get<0>(mset_tuple);
	auto& qdsl = std::get<1>(mset_tuple);
	auto& query = std::get<2>(mset_tuple);

	MsgPack obj;
	if (request.comments && opts.verbosity >= 3) {
		obj["#query"] = qdsl;
		obj["#engine_query"] = query.description();
	}
	obj[RESPONSE_TOTAL] = mset.estimated;

	request.ready = std::chrono::steady_clock::now();

	write_http_response(request, HTTP_STATUS_OK, obj);
}


static void
write_status_response(Request& request, enum http_status status, const std::string& message, std::optional<engine::rev> current_version)
{
	L_CALL("write_status_response()");

	MsgPack response({
		{ RESPONSE_STATUS, static_cast<unsigned>(status) },
		{ RESPONSE_TYPE, http_status_str(status) },
	});
	if (!message.empty()) {
		response[RESPONSE_MESSAGE] = message;
	}
	if (current_version) {
		response[VERSION_FIELD_NAME] = *current_version;  // a version conflict's: what to expect next, 0 for no document
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


static task::task<size_t>
resolve_index_endpoints(Request& request, const query_field_t& query_field, const MsgPack* settings)
{
	L_CALL("resolve_index_endpoints(<request>, <query_field>, <settings>)");

	auto paths = co_await expand_paths(request, query_field.patterns);

	// Sorted and without repeats, as Endpoints::add keeps them, in one sort: a wide search names thousands.
	std::vector<Endpoint> resolved;
	for (const auto& path : paths) {
		auto index_endpoints = co_await XapiandManager::resolve_index_endpoints(
			Endpoint(path),
			query_field.writable,
			settings);
		if (index_endpoints.empty()) {
			throw engine::Error(engine::errc::network, "Endpoint node not available");
		}
		for (auto& endpoint : index_endpoints) {
			resolved.push_back(std::move(endpoint));
		}
	}
	std::sort(resolved.begin(), resolved.end());
	resolved.erase(std::unique(resolved.begin(), resolved.end()), resolved.end());
	static_cast<std::vector<Endpoint>&>(request.endpoints) = std::move(resolved);
	L_HTTP("Endpoint: -> {}", request.endpoints.to_string());

	co_return paths.size();
}


// The index names of the request's path. A search or a count may name patterns (`_all`, `test*`, `-test3`):
// they are expanded against the indexes on the node's disk in a chore, so the reactor never reads the directory.
static task::task<std::vector<std::string>>
expand_paths(Request& request, bool patterns)
{
	L_CALL("expand_paths(<request>)");

	std::vector<std::string> paths;
	bool wildcards = false;

	request.path_parser.rewind();

	PathParser::State state;
	while ((state = request.path_parser.next()) < PathParser::State::END) {
		std::string index_path;

		auto pth = request.path_parser.get_pth();
		if (strings::startswith(pth, '/')) {
			pth.remove_prefix(1);
		}
		index_path.append(pth);
		while (index_path.size() > 1 && index_path.back() == '/') {
			index_path.pop_back();   // the last name of `/a,b/` carries the slash that ends the path
		}

		wildcards = wildcards || index_pattern::is_wildcard(index_path);
		paths.push_back(std::move(index_path));
	}

	if (wildcards && patterns) {
		std::vector<std::string> names;
		IndexChores chores(XapiandManager::manager(true)->database_pool->work_pool());
		// One walk at a time on the node: the rest wait their turn holding no thread, so the walks hold one worker of the
		// work pool whatever the clients send, and the writes' chores run beside them. The queue and the wait have bounds.
		static index_pattern::WalkGate walks(
			static_cast<std::size_t>(opts.search_walk_queue),
			std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(opts.search_walk_wait)));
		// The walk looks at the request between entries, so a client that goes away stops it.
		const task::cancel_token token = request.http_req != nullptr ? request.http_req->cancel_token() : task::cancel_token{};
		const auto turn = co_await walks.enter(token);
		const auto cap = static_cast<size_t>(opts.max_search_indexes);
		co_await chores.run(engine::ChoreClass::query, [&] {
			names = index_pattern::expand(Endpoint::cwd, paths, {cap, cap}, [&token] {
				if (token.requested()) {
					throw task::cancelled{};
				}
			});
		});
		co_return names;
	}

	co_return paths;
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
		query_field.patterns = true;

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
	// to finish (ready_to_end_http() == !http_clients). The counter must stay wired,
	// or a single SIGINT/SIGTERM drops active requests.
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
			decoded = msgpack_load(body);
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


task::task<void>
Request::decode_large_body()
{
	// A body past 64 KiB (an estimate) is decoded in a chore of its own group, so decoding up to the buffered cap
	// costs its reactor no more than the hand-off (docs/design/phase7c.md D12).
	constexpr size_t LARGE_BODY = 64 * 1024;
	if (mode != Mode::FULL || http_req == nullptr || http_req->body.size() <= LARGE_BODY || !_decoded_body.is_undefined()) {
		co_return;
	}
	IndexChores chores(XapiandManager::manager(true)->database_pool->work_pool());
	co_await chores.run(engine::ChoreClass::flush, [this] { decoded_body(); });
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

task::task<void>
Request::parse_ahead(std::string chunk, bool last)
{
	// The chunk is parsed in a chore of the request's group, so a body of any size costs its reactor no more than
	// the hand-off; the parser's state (raw, unpacker, objects) is touched by one chore at a time, and by this
	// task between them.
	const auto parse = [this, &chunk, last] {
		if (!last) {
			append(chunk.data(), chunk.size());
		} else {
			append(nullptr, 0);   // flush any trailing streamed object
		}
	};
	if (parse_chores != nullptr) {
		co_await parse_chores->run(engine::ChoreClass::flush, parse);
	} else {
		parse();
	}
}


task::task<void>
Request::feed_whole_body()
{
	// A streamed-mode body that arrived whole (it was under the buffered cap): parse it like the chunks of a
	// stream, in one chore, straight from the request's buffer.
	if (http_req == nullptr || http_req->body_stream != nullptr) {
		co_return;
	}
	const auto parse = [this] {
		if (!http_req->body.empty()) {
			append(http_req->body.data(), http_req->body.size());
		}
		append(nullptr, 0);   // flush any trailing streamed object
	};
	if (parse_chores != nullptr) {
		co_await parse_chores->run(engine::ChoreClass::flush, parse);
	} else {
		parse();
	}
}


task::task<bool>
Request::read_chunk(std::string& chunk)
{
	if (http_req == nullptr || !http_req->body_stream) {
		co_return false;
	}
	auto& stream = *http_req->body_stream;
	const asio::error_code ec = co_await stream.read(chunk, http_req->cancel_token());
	if (!ec) {
		co_return true;
	}
	if (ec == asio::error::eof) {
		co_return false;
	}
	if (ec == asio::error::operation_aborted) {
		throw task::cancelled{};   // the request's token: the client went away or the service is stopping
	}
	THROW(ClientError, "The request body ended early");
}


task::task<bool>
Request::next_batch(std::vector<MsgPack>& batch)
{
	L_CALL("Request::next_batch(<&batch>)");

	assert(has_content_length);
	assert(mode == Mode::STREAM_MSGPACK || mode == Mode::STREAM_NDJSON);

	batch.clear();
	for (;;) {
		if (!objects.empty()) {
			batch.assign(std::make_move_iterator(objects.begin()), std::make_move_iterator(objects.end()));
			objects.clear();
			co_return true;
		}
		// The objects are drained. On the streaming path, await the next raw body chunk from the
		// flow-controlled BodyStream and parse it (append() fills the deque); on the buffered path (no
		// stream) the body was already fed whole, so an empty deque means done.
		if (http_req == nullptr || !http_req->body_stream || body_ended) {
			co_return false;
		}
		std::string chunk;
		if (co_await read_chunk(chunk)) {
			co_await parse_ahead(std::move(chunk), false);
		} else {
			body_ended = true;
			co_await parse_ahead({}, true);
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


// The most handles a reactor lets queue in its heavy lane before it refuses new requests (docs/design/phase7c.md D2).
static constexpr std::size_t DEMOTED_ADMISSION_CAP = 256;


std::unique_ptr<http::RequestExtension>
SearchService::create_extension(const http::Request& /*hreq*/)
{
	// The second admission count (the first is the reactor's in-flight gate): resumptions of heavy
	// requests that wait in the heavy lane. Over its cap the loop is saturated, so the request is
	// refused before its body is read.
	if (auto* host = dynamic_cast<engine::AsioHost*>(engine::Host::current())) {
		const auto demoted = host->scheduler().demoted();
		auto& metrics = Metrics::metrics();
		Metrics::raise_peak(metrics.scheduler_demoted_peak, demoted);
		// The reactor's gate admitted this request just before: its count includes it.
		const auto& manager = XapiandManager::manager();
		for (std::size_t i = 0; manager && manager->http_service && i < manager->http_hosts.size(); ++i) {
			if (manager->http_hosts[i].get() == host) {
				Metrics::raise_peak(metrics.admission_inflight_peak, static_cast<std::size_t>(manager->http_service->reactor(i).gate().inflight.load(std::memory_order_relaxed)));
				break;
			}
		}
		if (demoted >= DEMOTED_ADMISSION_CAP) {
			throw http::RequestRejected(503, "The server is busy", http::RejectionContent::Text, 1);
		}
	}
	// Allocate Xapiand's per-request state as the http::Request's extension. It is
	// populated in handle() (which runs guarded by the connection's error backstop),
	// because that setup -- process_header()'s Accept parsing + method override -- can
	// throw, and create_extension() runs unguarded in the connection's parse path.
	return std::make_unique<Request>();
}


bool
SearchService::wants_body_stream(const http::Request& hreq)
{
	const auto method = method_from_string(hreq.method);

	// A blob past the buffered cap streams to the data storage as it arrives (D9): the request needs its length
	// up front. Which requests can take a stream is checked once the route is known (handle).
	if (method == HTTP_PUT || method == HTTP_POST) {
		const auto length = hreq.header("Content-Length");
		if (length.empty() || std::strtoull(std::string(length).c_str(), nullptr, 10) <= BUFFERED_BODY_CAP) { return false; }
		return is_stored_type(ct_type_t(hreq.content_type()));
	}

	// Stream a whole-database RESTORE with a bulk body (NDJSON / MsgPack, no doc id): handle()
	// runs while the body arrives and restore_database_view awaits it chunk by chunk --
	// O(buffer) memory for a dump of any size. A single-document RESTORE (id present) and every
	// other request buffer.
	if (method != HTTP_RESTORE) { return false; }
	if (hreq.header("Content-Length").empty()) { return false; }
	ct_type_t ct(hreq.content_type());
	if (ct != ndjson_type && ct != x_ndjson_type && ct != msgpack_type && ct != x_msgpack_type) { return false; }
	PathParser pp;
	if (pp.init(hreq.path) >= PathParser::State::END) { return false; }   // invalid path
	return pp.get_id().empty();                                           // no id -> whole database
}


task::task<void>
SearchService::handle(http::Request& hreq, http::ResponseWriter& response)
{
	Request& request = hreq.ext<Request>();   // the extension create_extension() built
	ENGINE_CENSUS_REQUEST(census_request, hreq.method, hreq.target);
	// A request starts light: on every exit, a throw included, the connection goes on light, so its next read and
	// request leave the heavy lane this one may have entered (docs/design/loop-scheduling.md D2 f).
	struct LightOnExit {
		~LightOnExit() { task::enter_light(); }
	} light_on_exit;
	request.http_req = &hreq;                 // the source of truth for every HTTP fact
	request.response_writer = &response;       // the response path emits through the library writer

	// A client that goes away, or a shutdown, stops the request's reads at their next checkpoint;
	// a write that was admitted completes and its response is attempted. The request's token is
	// http's (Request::cancel_token()); a read that waits for a streamed body ends with it too.
	const task::cancel_token token = hreq.cancel_token();
	request.budget.emplace(REQUEST_SLICE, token, REQUEST_LIGHT_SLICES);

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
	// Engine and ClientError exceptions propagate to the connection, which calls
	// SearchService::on_error() -- the app's error-to-status mapping.
	if (dispatch_request(request) == 0 && request.view != nullptr) {
		// A FULL request reads its body straight from http_req (via body_view() /
		// decoded_body()) -- no copy. A bulk body (RESTORE) is parsed by its view, in a chore of
		// its own group: next_batch() awaits and parses each chunk of a stream, and
		// feed_whole_body() parses one that arrived whole.

		// A streamed body is read by the one view that wants it; any other answers 413 without reading it.
		if (hreq.body_stream != nullptr && request.view != &restore_database_view && request.view != &write_document_view) {
			write_status_response(request, HTTP_STATUS_PAYLOAD_TOO_LARGE, strings::format("The body is past the {} limit of this request", strings::from_bytes(BUFFERED_BODY_CAP)));
			response.set_close();
			co_return;
		}
		// The view is a child task of the connection on this reactor's scheduler: it suspends for a
		// turn, a chore or a slice, and the loop serves other requests meanwhile.
		auto* host = dynamic_cast<engine::AsioHost*>(engine::Host::current());
		if (host == nullptr || &host->scheduler() != &hreq.scheduler()) {
			THROW(Error, "The request does not run on a reactor bound to its host");
		}
		try {
			co_await engine::counted(*host, request.decode_large_body());
			co_await engine::counted(*host, request.view(request));
		} catch (const index_pattern::WalkRefused& refused) {
			if (request.response_status == static_cast<http_status>(0)) {
				request.retry_after = refused.retry_after;
				write_status_response(request, static_cast<http_status>(refused.status), refused.reason);
			}
		} catch (const task::cancelled&) {
			Metrics::metrics().xapiand_http_requests_cancelled.Increment();
			// The reads stopped at a checkpoint; say so to a client that is still there.
			if (request.response_status == static_cast<http_status>(0)) {
				write_status_response(request, HTTP_STATUS_SERVICE_UNAVAILABLE, "The request was cancelled");
			}
			response.set_close();
		}
	}
	ENGINE_CENSUS_STATUS(census_request, request.response_status);

	if (request.closing) {
		response.set_close();
	}
}


// The application's error-to-status mapping, invoked by the http::HttpConnection when
// handle() throws without having answered (the library owns only the generic 500
// fallback). Rethrow to inspect the type; catch_http_errors maps engine / ClientError /
// Datetime / Cartesian to the right status, then write_status_response emits it -- the
// same mapping handled_errors did on the legacy path.
void
SearchService::on_error(std::exception_ptr error, const http::Request& hreq, http::ResponseWriter& response)
{
	Request& request = hreq.ext<Request>();

	auto http_errors = catch_http_errors([&]{ std::rethrow_exception(error); return 0; });
	std::optional<engine::rev> current_version;
	try {
		std::rethrow_exception(error);
	} catch (const engine::Error& exc) {
		current_version = exc.current_version;
	} catch (...) { }
	if (request.response_status == static_cast<http_status>(0)) {
		write_status_response(request, http_errors.error_code, http_errors.error, current_version);
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
				decoded = msgpack_load(body);
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
