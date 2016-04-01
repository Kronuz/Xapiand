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

#include "client_http.h"

#include "multivalue.h"
#include "utils.h"
#include "serialise.h"
#include "length.h"
#include "io_utils.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include <unistd.h>
#include <regex>

#include <sysexits.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define RESPONSE_MESSAGE "message"
#define RESPONSE_STATUS  "status"

#define MAX_BODY_SIZE (250 * 1024 * 1024)
#define MAX_BODY_MEM (5 * 1024 * 1024)

#define def_36e6 2176782336

// Xapian http client
#define METHOD_DELETE  0
#define METHOD_HEAD    2
#define METHOD_GET     1
#define METHOD_POST    3
#define METHOD_PUT     4
#define METHOD_OPTIONS 6
#define METHOD_PATCH   24


std::pair<std::string, std::string>
content_type_pair(const std::string& ct_type)
{
	std::size_t found = ct_type.find_last_of("/");
	if (found == std::string::npos) {
		return  make_pair(std::string(), std::string());
	}
	const char* content_type_str = ct_type.c_str();
	return make_pair(std::string(content_type_str, found), std::string(content_type_str, found + 1, ct_type.size()));
}

static const auto any_type     = content_type_pair(ANY_TYPE);
static const auto json_type    = content_type_pair(JSON_TYPE);
static const auto msgpack_type = content_type_pair(MSGPACK_TYPE);
static const auto html_type    = content_type_pair(HTML_TYPE);
static const auto text_type    = content_type_pair(TEXT_TYPE);
static const auto msgpack_serializers = std::vector<std::pair<std::string, std::string>>({json_type, msgpack_type, html_type, text_type});

static const std::regex header_accept_re("([-a-z+]+|\\*)/([-a-z+]+|\\*)(?:[^,]*;q=(\\d+(?:\\.\\d+)?))?");


static const char* status_code[6][14] = {
	{},
	{
		"Continue"                  // 100
	},
	{
		"OK",                       // 200
		"Created"                   // 201
	},
	{},
	{
		"Bad Request",              // 400
		nullptr,                    // 401
		nullptr,                    // 402
		nullptr,                    // 403
		"Not Found",                // 404
		nullptr,                    // 405
		"Not Acceptable",           // 406
		nullptr,                    // 407
		nullptr,                    // 408
		"Conflict",                 // 409
		nullptr,                    // 410
		nullptr,                    // 411
		nullptr,                    // 412
		"Request Entity Too Large"  // 413
	},
	{
		"Internal Server Error",    // 500
		"Not Implemented",          // 501
		"Bad Gateway"               // 502
	}
};


std::string
HttpClient::http_response(int status, int mode, unsigned short http_major, unsigned short http_minor, int matched_count, const std::string& body, const std::string& ct_type, const std::string& ct_encoding) {
	L_CALL(this, "HttpClient::http_response()");

	char buffer[20];
	std::string response;
	const std::string eol("\r\n");


	if (mode & HTTP_STATUS) {
		snprintf(buffer, sizeof(buffer), "HTTP/%d.%d %d ", http_major, http_minor, status);
		response += buffer;
		response += status_code[status / 100][status % 100] + eol;
		if (!(mode & HTTP_HEADER)) {
			response += eol;
		}
	}

	if (mode & HTTP_HEADER) {

		response += "Server: " + std::string(PACKAGE_NAME) + "/" + std::string(VERSION) + eol;

		response_ends = std::chrono::system_clock::now();
		response += "Response-Time: " + delta_string(request_begins, response_ends) + eol;
		if (operation_ends >= operation_begins) {
			response += "Operation-Time: " + delta_string(operation_begins, operation_ends) + eol;
		}

		if (mode & HTTP_CONTENT_TYPE) {
			response += "Content-Type: " + ct_type + eol;
		}

		if (mode & HTTP_CONTENT_ENCODING) {
			response += "Content-Encoding: " + ct_encoding + eol;
		}

		if (mode & HTTP_OPTIONS) {
			response += "Allow: GET,HEAD,POST,PUT,PATCH,OPTIONS" + eol;
		}

		if (mode & HTTP_MATCHED_COUNT) {
			response += "X-Matched-count: " + std::to_string(matched_count) + eol;
		}

		if (mode & HTTP_CHUNKED) {
			response += "Transfer-Encoding: chunked" + eol;
		} else {
			response += "Content-Length: ";
			snprintf(buffer, sizeof(buffer), "%lu", body.size());
			response += buffer + eol;
		}
		response += eol;
	}

	if (mode & HTTP_BODY) {
		if (mode & HTTP_CHUNKED) {
			snprintf(buffer, sizeof(buffer), "%lx", body.size());
			response += buffer + eol;
			response += body + eol;
		} else {
			response += body;
		}
	}

	if (!(mode & HTTP_CHUNKED) && !(mode & HTTP_EXPECTED100)) {
		clean_http_request();
	}

	return response;
}


HttpClient::HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* loop_, int sock_)
	: BaseClient(std::move(server_), loop_, sock_),
	  database(nullptr),
	  body_size(0),
	  body_descriptor(0),
	  post_id(0),
	  request_begining(true)
{
	parser.data = this;
	http_parser_init(&parser, HTTP_REQUEST);

	int http_clients = ++XapiandServer::http_clients;
	if (http_clients > XapiandServer::max_http_clients) {
		XapiandServer::max_http_clients = http_clients;
	}
	int total_clients = XapiandServer::total_clients;
	if (http_clients > total_clients) {
		L_CRIT(this, "Inconsistency in number of http clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_CONN(this, "New Http Client (sock=%d), %d client(s) of a total of %d connected.", sock, http_clients, total_clients);

	L_OBJ(this, "CREATED HTTP CLIENT! (%d clients)", http_clients);
}


HttpClient::~HttpClient()
{
	int http_clients = --XapiandServer::http_clients;
	int total_clients = XapiandServer::total_clients;
	if (http_clients < 0 || http_clients > total_clients) {
		L_CRIT(this, "Inconsistency in number of http clients");
		sig_exit(-EX_SOFTWARE);
	}

	if (manager()->shutdown_asap.load()) {
		if (http_clients <= 0) {
			manager()->shutdown_sig(0);
		}
	}

	if (body_descriptor) {
		if (io::close(body_descriptor) < 0) {
			L_ERR(this, "ERROR: Cannot close temporary file '%s': %s", body_path, strerror(errno));
		}

		if (io::unlink(body_path) < 0) {
			L_ERR(this, "ERROR: Cannot delete temporary file '%s': %s", body_path, strerror(errno));
		}
	}

	L_OBJ(this, "DELETED HTTP CLIENT! (%d clients left)", http_clients);
}


void
HttpClient::on_read(const char* buf, size_t received)
{
	if (request_begining) {
		request_begining = false;
		request_begins = std::chrono::system_clock::now();
	}
	L_CONN_WIRE(this, "HttpClient::on_read: %zu bytes", received);
	unsigned init_state = parser.state;
	size_t parsed = http_parser_execute(&parser, &settings, buf, received);
	if (parsed == received) {
		unsigned final_state = parser.state;
		if (final_state == init_state and received == 1 and (strncmp(buf, "\n", received) == 0)) { //ignore '\n' request
			return;
		}
		if (final_state == 1 || final_state == 18) { // dead or message_complete
			L_EV(this, "Disable read event (sock=%d)", sock);
			io_read.stop();
			written = 0;
			if (!closed) {
				manager()->thread_pool.enqueue(share_this<HttpClient>());
			}
		}
	} else {
		int error_code = 400;
		std::string message(http_errno_description(HTTP_PARSER_ERRNO(&parser)));
		MsgPack err_response;
		err_response[RESPONSE_STATUS] = error_code;
		err_response[RESPONSE_MESSAGE] = message;
		write_http_response(err_response, error_code, false);
		L_HTTP_PROTO(this, HTTP_PARSER_ERRNO(&parser) != HPE_OK ? message : "incomplete request");
		destroy();  // Handle error. Just close the connection.
	}
}


void
HttpClient::on_read_file(const char*, size_t received)
{
	L_ERR(this, "Not Implemented: HttpClient::on_read_file: %zu bytes", received);
}


void
HttpClient::on_read_file_done()
{
	L_ERR(this, "Not Implemented: HttpClient::on_read_file_done");
}


// HTTP parser callbacks.
const http_parser_settings HttpClient::settings = {
	.on_message_begin = HttpClient::on_info,
	.on_url = HttpClient::on_data,
	.on_status = HttpClient::on_data,
	.on_header_field = HttpClient::on_data,
	.on_header_value = HttpClient::on_data,
	.on_headers_complete = HttpClient::on_info,
	.on_body = HttpClient::on_data,
	.on_message_complete = HttpClient::on_info
};


int
HttpClient::on_info(http_parser* p)
{
	HttpClient *self = static_cast<HttpClient *>(p->data);

	int state = p->state;

	L_HTTP_PROTO_PARSER(self, "%3d. (INFO)", state);

	switch (state) {
		case 18:  // message_complete
			break;
		case 19:  // message_begin
			self->path.clear();
			self->body.clear();
			self->body_size = 0;
			self->header_name.clear();
			self->header_value.clear();
			if (self->body_descriptor && io::close(self->body_descriptor) < 0) {
				L_ERR(self, "ERROR: Cannot close temporary file '%s': %s", self->body_path, strerror(errno));
			} else {
				self->body_descriptor = 0;
			}
			break;
		case 50:  // headers done
			if (self->expect_100) {
				// Return 100 if client is expecting it
				self->write(self->http_response(100, HTTP_STATUS | HTTP_EXPECTED100, p->http_major, p->http_minor));
			}
	}

	return 0;
}


int
HttpClient::on_data(http_parser* p, const char* at, size_t length)
{
	HttpClient *self = static_cast<HttpClient *>(p->data);

	int state = p->state;

	L_HTTP_PROTO_PARSER(self, "%3d. %s", state, repr(at, length).c_str());

	if (state > 26 && state <= 32) {
		// s_req_path  ->  s_req_http_start
		self->path.append(at, length);
	} else if (state >= 43 && state <= 44) {
		// s_header_field  ->  s_header_value_discard_ws
		self->header_name.append(at, length);
	} else if (state >= 45 && state <= 50) {
		// s_header_value_discard_ws_almost_done  ->  s_header_almost_done
		self->header_value.append(at, length);
		if (state == 50) {
			std::string name = lower_string(self->header_name);
			std::string value = lower_string(self->header_value);

			if (name.compare("host") == 0) {
				self->host = self->header_value;
			} else if (name.compare("expect") == 0 && value.compare("100-continue") == 0) {
				if (p->content_length > MAX_BODY_SIZE) {
					self->write(self->http_response(413, HTTP_STATUS, p->http_major, p->http_minor));
					self->close();
					return 0;
				}
				// Respond with HTTP/1.1 100 Continue
				self->expect_100 = true;
			} else if (name.compare("content-type") == 0) {
				self->content_type = value;
			} else if (name.compare("content-length") == 0) {
				self->content_length = value;
			} else if (name.compare("accept") == 0) {
				std::sregex_iterator next(value.begin(), value.end(), header_accept_re, std::regex_constants::match_any);
				std::sregex_iterator end;
				int i = 0;
				while (next != end) {
					next->length(3) != 0 ? self->accept_set.insert(std::make_tuple(std::stod(next->str(3)), i, std::make_pair(next->str(1), next->str(2))))
						: self->accept_set.insert(std::make_tuple(1, i, std::make_pair(next->str(1), next->str(2))));
					++next;
					++i;
				}
			}
			self->header_name.clear();
			self->header_value.clear();
		}
	} else if (state >= 60 && state <= 62) { // s_body_identity  ->  s_message_done
		self->body_size += length;
		if (self->body_size > MAX_BODY_SIZE || p->content_length > MAX_BODY_SIZE) {
			self->write(self->http_response(413, HTTP_STATUS, p->http_major, p->http_minor));
			self->close();
			return 0;
		} else if (self->body_descriptor || self->body_size > MAX_BODY_MEM) {

			// The next two lines are switching off the write body in to a file option when the body is too large
			// (for avoid have it in memory) but this feature is not available yet
			self->write(self->http_response(413, HTTP_STATUS, p->http_major, p->http_minor)); // <-- remove leater!
			self->close(); // <-- remove leater!

			if (!self->body_descriptor) {
				strcpy(self->body_path, "/tmp/xapiand_upload.XXXXXX");
				self->body_descriptor = mkstemp(self->body_path);
				if (self->body_descriptor < 0) {
					L_ERR(self, "Cannot write to %s (1)", self->body_path);
					return 0;
				}
				io::write(self->body_descriptor, self->body.data(), self->body.size());
				self->body.clear();
			}
			io::write(self->body_descriptor, at, length);
			if (state == 62) {
				if (self->body_descriptor && io::close(self->body_descriptor) < 0) {
					L_ERR(self, "ERROR: Cannot close temporary file '%s': %s", self->body_path, strerror(errno));
				} else {
					self->body_descriptor = 0;
				}
			}
		} else {
			self->body.append(at, length);
		}
	}

	return 0;
}


void
HttpClient::run()
{
	L_CALL(this, "HttpClient::run()");

	L_OBJ_BEGIN(this, "HttpClient::run:BEGIN");
	response_begins = std::chrono::system_clock::now();

	MsgPack err_response;
	std::string error;
	int error_code = 0;

	try {
		if (path == "/quit") {
			time_t now = epoch::now<>();
			manager()->shutdown_asap.store(now);
			manager()->shutdown_sig(0);
			L_OBJ_END(this, "HttpClient::run:END");
			return;
		}

		switch (parser.method) {
			case METHOD_DELETE:
				_delete();
				break;
			case METHOD_GET:
				_get();
				break;
			case METHOD_POST:
				_post();
				break;
			case METHOD_HEAD:
				_head();
				break;
			case METHOD_PUT:
				_put();
				break;
			case METHOD_OPTIONS:
				_options();
				break;
			case METHOD_PATCH:
				_patch();
			default:
				write(http_response(501, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
				break;
		}
	} catch (const ClientError& exc) {
		error_code = 400;
		error.assign(exc.what());
	} catch (const Exception& exc) {
		error_code = 500;
		error.assign(*exc.what() ? exc.what() : "Unkown Exception!");
		L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
	} catch (const Xapian::Error& exc) {
		error_code = 500;
		auto exc_msg = exc.get_msg().c_str();
		error.assign(*exc_msg ? exc_msg : "Unkown Xapian::Error!");
		L_EXC(this, "ERROR: %s", error.c_str());
	} catch (const std::exception& exc) {
		error_code = 500;
		error.assign(*exc.what() ? exc.what() : "Unkown std::exception!");
		L_EXC(this, "ERROR: %s", error.c_str());
	} catch (...) {
		error_code = 500;
		error.assign("Unknown exception!");
		std::exception exc;
		L_EXC(this, "ERROR: %s", error.c_str());
	}

	if (error_code) {
		if (database) {
			manager()->database_pool.checkin(database);
		}
		if (written) {
			destroy();
		} else {
			err_response[RESPONSE_STATUS] = error_code;
			err_response[RESPONSE_MESSAGE] = error;
			write_http_response(err_response, error_code, false);
		}
	}

	L_OBJ_END(this, "HttpClient::run:END");
}


void
HttpClient::_options()
{
	L_CALL(this, "HttpClient::_options()");

	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_OPTIONS, parser.http_major, parser.http_minor));
}


void
HttpClient::_head()
{
	L_CALL(this, "HttpClient::_head()");

	query_field_t e;
	int cmd = url_resolve(e, false);

	switch (cmd) {
		case CMD_ID:
			document_info_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::_get()
{
	L_CALL(this, "HttpClient::_get()");

	query_field_t e;
	int cmd = url_resolve(e, false);
	int mode = identify_mode(HttpClient::mode);

	if (mode != CMD_UNKNOWN) {
		cmd = mode; /* Left the cmd as the mode */
	}

	switch (cmd) {
		case CMD_HOME:
			home_view(e);
			break;
		case CMD_ID:
			e.query.push_back(std::string(RESERVED_ID)  + ":" +  command);
			search_view(e, false, false);
			break;
		case CMD_SEARCH:
			e.check_at_least = 0;
			search_view(e, false, false);
			break;
		case CMD_FACETS:
			search_view(e, true, false);
			break;
		case CMD_STATS:
			stats_view(e, mode);
			break;
		case CMD_SCHEMA:
			search_view(e, false, true);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::_put()
{
	L_CALL(this, "HttpClient::_put()");

	query_field_t e;
	int cmd = url_resolve(e, true);

	switch (cmd) {
		case CMD_ID:
			index_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::_post()
{
	L_CALL(this, "HttpClient::_post()");

	query_field_t e;
	int cmd = url_resolve(e, false);

	switch (cmd) {
		case CMD_ID: /* by default will be set as a command id */
			index_document_view(e, true);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::_patch()
{
	L_CALL(this, "HttpClient::_patch()");

	query_field_t e;
	int cmd = url_resolve(e, true);

	switch (cmd) {
		case CMD_ID:
			update_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::_delete()
{
	L_CALL(this, "HttpClient::_delete()");

	query_field_t e;
	int cmd = url_resolve(e, true);

	switch (cmd) {
		case CMD_ID:
			delete_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void
HttpClient::document_info_view(const query_field_t& e)
{
	L_CALL(this, "HttpClient::document_info_view()");

	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(command.at(0))) {
		prefix += ":";
	}

	Xapian::QueryParser queryparser;
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	Xapian::Query query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + command);
	Xapian::Enquire enquire(*database->db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);

	MsgPack response;
	int status_code = 200;
	if (mset.empty()) {
		status_code = 404;
		response[RESPONSE_STATUS] = status_code;
		response[RESPONSE_MESSAGE] = "Document not found";
	} else {
		for (int t = DB_RETRIES; t >= 0; --t) {
			try {
				response["doc_id"] = *mset.begin();
				break;
			}  catch (const Xapian::DatabaseModifiedError& exc) {
				if (t) {
					database->reopen();
				} else {
					throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
				}
			} catch (const Xapian::NetworkError& exc) {
				if (t) {
					database->reopen();
				} else {
					throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
				}
			} catch (const Xapian::Error& exc) {
				throw MSG_Error(exc.get_msg().c_str());
			}
		}
	}

	manager()->database_pool.checkin(database);
	write_http_response(response, status_code, e.pretty);
}


void
HttpClient::delete_document_view(const query_field_t& e)
{
	L_CALL(this, "HttpClient::delete_document_view()");

	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	operation_begins = std::chrono::system_clock::now();

	database->delete_document(command, e.commit);

	operation_ends = std::chrono::system_clock::now();

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		++stats_cnt.del.min[b_time.minute];
		++stats_cnt.del.sec[b_time.second];
		stats_cnt.del.tm_min[b_time.minute] += _time;
		stats_cnt.del.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Deletion took %s", delta_string(operation_begins, operation_ends).c_str());

	manager()->database_pool.checkin(database);

	MsgPack response;
	auto data = response["delete"];
	data[RESERVED_ID] = command;
	data["commit"] = e.commit;

	write_http_response(response, 200, e.pretty);
}


void
HttpClient::index_document_view(const query_field_t& e, bool gen_id)
{
	L_CALL(this, "HttpClient::index_document_view()");

	build_path_index(index_path);

	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	if (gen_id) {
		unsigned long mangled = std::fmod(++post_id * 1679979167, def_36e6);
		command = baseN(mangled, 36);
	}

	if (content_type.empty()) {
		content_type = JSON_TYPE;
	}

	operation_begins = std::chrono::system_clock::now();

	database->index(body, command, e.commit, content_type, content_length);

	operation_ends = std::chrono::system_clock::now();

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		++stats_cnt.index.min[b_time.minute];
		++stats_cnt.index.sec[b_time.second];
		stats_cnt.index.tm_min[b_time.minute] += _time;
		stats_cnt.index.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Indexing took %s", delta_string(operation_begins, operation_ends).c_str());

	manager()->database_pool.checkin(database);
	MsgPack response;
	auto data = response["index"];
	data[RESERVED_ID] = command;
	write_http_response(response, 200, e.pretty);
}


void
HttpClient::update_document_view(const query_field_t& e)
{
	L_CALL(this, "HttpClient::update_document_view()");

	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE | DB_SPAWN)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	operation_begins = std::chrono::system_clock::now();

	database->patch(body, command, e.commit, content_type, content_length);

	operation_ends = std::chrono::system_clock::now();

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		++stats_cnt.patch.min[b_time.minute];
		++stats_cnt.patch.sec[b_time.second];
		stats_cnt.patch.tm_min[b_time.minute] += _time;
		stats_cnt.patch.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Updating took %s", delta_string(operation_begins, operation_ends).c_str());

	manager()->database_pool.checkin(database);
	MsgPack response;
	auto data = response["update"];
	data[RESERVED_ID] = command;
	write_http_response(response, 200, e.pretty);
}


void
HttpClient::stats_view(const query_field_t& e, int mode)
{
	L_CALL(this, "HttpClient::stats_view()");

	MsgPack response;
	int response_status = 200;
	bool res_stats = false;

	if (endpoints.size() == 0) {	/* Server stats */
		manager()->server_status(response["server_status"]);
		res_stats = true;
	} else if (endpoints.size() == 1) {
		 if (mode == CMD_UNKNOWN) {		/* Database stats */
			 if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
				 L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
				 write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
				 return;
			 }
			 database->get_stats_database(response["database_status"]);
			 manager()->database_pool.checkin(database);
			 res_stats = true;
		 } else if (mode == CMD_STATS) {	/* Document stats */
			 if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
				 L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
				 write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
				 return;
			 }
			 database->get_stats_doc(response["document_status"], command);
			 manager()->database_pool.checkin(database);
			 res_stats = true;
		 } else {
			//It is not expected to enter here
			 assert(false);
		 }
	} else {
		response_status = 400;
		response[RESPONSE_STATUS] = response_status;
		response[RESPONSE_MESSAGE] = "Expecting exactly one database for stats operation";
		write_http_response(response, 400, e.pretty);
	}

	if (!res_stats) {
		response[RESPONSE_MESSAGE] = "Empty statistics";
	}

	write_http_response(response, response_status, e.pretty);
}


void
HttpClient::bad_request_view(const query_field_t& e, int cmd)
{
	L_CALL(this, "HttpClient::bad_request_view()");

	MsgPack err_response;
	err_response[RESPONSE_STATUS] = 400;
	switch (cmd) {
		case CMD_UNKNOWN_HOST:
			err_response[RESPONSE_MESSAGE] = "Unknown host " + host;
			break;
		default:
			err_response[RESPONSE_MESSAGE] = "BAD QUERY";
	}
	write_http_response(err_response, 400, e.pretty);
}


void
HttpClient::home_view(const query_field_t& e)
{
	L_CALL(this, "HttpClient::home_view()");

	MsgPack response;
	int response_status = 200;

	response["name"] = local_node.name;
	// response["address"] = local_node.addr;
#ifdef XAPIAND_CLUSTERING
	response["cluster_name"] = manager()->cluster_name;
#endif
	MsgPack version;
	version["number"] = PACKAGE_VERSION;
	// build_hash
	// build_timestamp
	// build_snapshot
	version["xapian_version"] = Xapian::version_string();
	response["version"] = version;
	response["tagline"] = XAPIAND_TAGLINE;
	response["_id"] = local_node.id;

	write_http_response(response, response_status, e.pretty);
}


void
HttpClient::upload_view(const query_field_t&)
{
	L_CALL(this, "HttpClient::upload_view()");

	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	L_DEBUG(this, "Uploaded %s (%zu)", body_path, body_size);
	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));

	manager()->database_pool.checkin(database);
}


void
HttpClient::search_view(const query_field_t& e, bool facets, bool schema)
{
	L_CALL(this, "HttpClient::search_view()");

	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		L_WARNING(this, "Cannot checkout database: %s", endpoints.as_string().c_str());
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	if (schema) {
		write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, database->schema.to_json_string(e.pretty)));
		manager()->database_pool.checkin(database);
		return;
	}

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;

	operation_begins = std::chrono::system_clock::now();
	database->get_mset(e, mset, spies, suggestions);

	L_DEBUG(this, "Suggested queries:\n%s", [&suggestions]() {
		std::string res;
		for (const auto& suggestion : suggestions) {
			res += "\t+ " + suggestion + "\n";
		}
		return res;
	}().c_str());

	if (facets) {
		MsgPack response;
		if (spies.empty()) {
			response[RESPONSE_MESSAGE] = "Not found documents tallied";
		} else {
			for (const auto& spy : spies) {
				std::string name_result = spy.first;
				MsgPack array;
				const auto facet_e = spy.second->values_end();
				for (auto facet = spy.second->values_begin(); facet != facet_e; ++facet) {
					MsgPack value;
					data_field_t field_t = database->get_slot_field(spy.first);
					auto _val = value["value"];
					Unserialise::unserialise(field_t.type, *facet, _val);
					value["termfreq"] = facet.get_termfreq();
					array.add_item_to_array(value);
				}
				response[name_result] = array;
			}
		}
		operation_ends = std::chrono::system_clock::now();
		write_http_response(response, 200, e.pretty);
	} else {
		int rc = 0;

		if (mset.empty()) {
			MsgPack err_response;
			int error_code = 404;
			err_response[RESPONSE_STATUS] = error_code;
			if (e.unique_doc) {
				err_response[RESPONSE_MESSAGE] = "No document found";
			} else {
				err_response[RESPONSE_MESSAGE] = "No match found";
			}
			write_http_response(err_response, error_code, e.pretty);
		} else {
			bool chunked = e.unique_doc && mset.size() == 1 ? false : true;

			for (auto m = mset.begin(); m != mset.end(); ++rc, ++m) {
				Xapian::Document document;
				if (!database->get_document(m, document)) {
					database->reopen();
					database->get_mset(e, mset, spies, suggestions, rc);
					m = mset.begin();
					continue;
				}

				operation_ends = std::chrono::system_clock::now();

				auto ct_type_str = document.get_value(DB_SLOT_TYPE);
				if (ct_type_str == JSON_TYPE || ct_type_str == MSGPACK_TYPE) {
					if (is_acceptable_type(get_acceptable_type(json_type), json_type)) {
						ct_type_str = JSON_TYPE;
					} else if (is_acceptable_type(get_acceptable_type(msgpack_type), msgpack_type)) {
						ct_type_str = MSGPACK_TYPE;
					}
				}
				auto ct_type = content_type_pair(ct_type_str);

				std::vector<std::pair<std::string, std::string>> ct_types;
				if (ct_type == json_type || ct_type == msgpack_type) {
					ct_types = msgpack_serializers;
				} else {
					ct_types.push_back(ct_type);
				}

				const auto& accepted_type = get_acceptable_type(ct_types);
				const auto accepted_ct_type = is_acceptable_type(accepted_type, ct_types);
				if (!accepted_ct_type) {
					MsgPack err_response;
					int error_code = 406;
					err_response[RESPONSE_STATUS] = error_code;
					err_response[RESPONSE_MESSAGE] = std::string("Response type " + ct_type.first + "/" + ct_type.second + " not provided in the accept header");
					write_http_response(err_response, error_code, e.pretty);
					manager()->database_pool.checkin(database);
					L_DEBUG(this, "ABORTED SEARCH");
					return;
				}

				MsgPack obj_data;
				if (is_acceptable_type(json_type, ct_type)) {
					obj_data = get_MsgPack(document);
				} else if (is_acceptable_type(msgpack_type, ct_type)) {
					obj_data = get_MsgPack(document);
				} else {
					// Returns blob_data in case that type is unkown
					auto blob_data = get_blob(document);
					write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_BODY, parser.http_major, parser.http_minor, 0, blob_data, ct_type_str));
					manager()->database_pool.checkin(database);
					return;
				}

				ct_type = *accepted_ct_type;
				ct_type_str = ct_type.first + "/" + ct_type.second;

				if (rc == 0 && chunked) {
					write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_CHUNKED | HTTP_MATCHED_COUNT, parser.http_major, parser.http_minor, mset.size(), "", ct_type_str));
				}

				try {
					obj_data = obj_data.at(RESERVED_DATA);
				} catch (const std::out_of_range&) {
					clean_reserved(obj_data);
					obj_data[RESERVED_ID] = document.get_value(DB_SLOT_ID);
				}

				auto result = serialize_response(obj_data, ct_type, e.pretty);
				if (chunked) {
					if (!write(http_response(200, HTTP_BODY | HTTP_CHUNKED, parser.http_major, parser.http_minor, 0, result.first + "\n\n"))) {
						break;
					}
				} else if (!write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result.first, result.second))) {
					break;
				}
			}

			if (chunked) {
				write(http_response(0, HTTP_BODY, 0, 0, 0, "0\r\n\r\n"));
			}
		}
	}

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		++stats_cnt.search.min[b_time.minute];
		++stats_cnt.search.sec[b_time.second];
		stats_cnt.search.tm_min[b_time.minute] += _time;
		stats_cnt.search.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Searching took %s", delta_string(operation_begins, operation_ends).c_str());

	manager()->database_pool.checkin(database);
	L_DEBUG(this, "FINISH SEARCH");
}


int
HttpClient::url_resolve(query_field_t& e, bool writable)
{
	L_CALL(this, "HttpClient::url_resolve()");

	bool solo_command = false;
	struct http_parser_url u;
	std::string b = repr(path);

	L_HTTP_PROTO_PARSER(this, "URL: %s", b.c_str());
	if (http_parser_parse_url(b.c_str(), b.size(), 0, &u) == 0) {
		L_HTTP_PROTO_PARSER(this, "HTTP parsing done!");

		if (u.field_set & (1 << UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			std::string path_buf(b.c_str() + u.field_data[3].off, u.field_data[3].len);
			auto unique_path_buf = std::make_unique<char[]>(path_buf.size() + 1);
			char* path_buf_str = unique_path_buf.get();
			normalize_path(path_buf.c_str(), path_buf_str);

			if (*path_buf_str == '/' && *(path_buf_str + 1) == '\0') {
				command.clear();
			} else {
				endpoints.clear();

				parser_url_path_t p;
				memset(&p, 0, sizeof(p));

				bool find_id = parser.method == METHOD_POST ? false : true;
				int state = url_path(path_buf_str, path_size, &p, find_id);

				if (state < 0) {
					return CMD_BAD_QUERY;
				}

				if (state == 10 /*STATE_UNIQUE_CMD_STAT*/) { /* Solo command case (without index part) */
					solo_command = true;
					command = lower_string(urldecode(p.off_command, p.len_command));
				} else {
					while (state == 0) {
						int endp_err = endpoint_maker(p, writable, find_id);
						if (endp_err != 0) {
							return endp_err;
						}
						state = url_path(path_buf_str, path_size, &p);
					}
				}

				if (p.len_parameter) {
					mode = lower_string(urldecode(p.off_parameter, p.len_parameter));
				}
			}
		}

		if ((parser.method == METHOD_PUT || parser.method == METHOD_PATCH) && endpoints.size() > 1) {
			return CMD_BAD_ENDPS;
		}

		int cmd = identify_cmd(command);

		if (u.field_set & (1 <<  UF_QUERY)) {
			size_t query_size = u.field_data[4].len;
			const char *query_str = b.data() + u.field_data[4].off;

			parser_query_t q;
			query_maker(query_str, query_size, cmd, e, q);
		} else {
			//Especial case (search ID and empty query in the url)
			if (cmd == CMD_ID) {

				if (solo_command) {
					return CMD_BAD_QUERY;
				}

				if (isRange(command)) {
					e.offset = 0;
					e.check_at_least = 0;
					e.limit = 10;
					e.sort.push_back(RESERVED_ID);
				} else {
					e.limit = 1;
					e.unique_doc = true;
					e.offset = 0;
					e.check_at_least = 0;
				}
			}
		}
		return cmd;
	} else {
		L_CONN_WIRE(this, "Parsing not done");
		// Bad query
		return CMD_BAD_QUERY;
	}
}



int
HttpClient::endpoint_maker(parser_url_path_t& p, bool writable, bool require_id)
{
	bool has_node_name = false;

	if (p.len_command and require_id) {
		command = lower_string(urldecode(p.off_command, p.len_command));
		if (command.empty()) {
			return CMD_BAD_QUERY;
		}
	}

	std::string ns;
	if (p.len_namespace) {
		ns = urldecode(p.off_namespace, p.len_namespace) + "/";
	}

	std::string path;
	if (p.len_path) {
		path = urldecode(p.off_path, p.len_path);
	}

	index_path = ns + path;
	std::string node_name;
	Endpoint asked_node("xapian://" + index_path);

	std::vector<Endpoint> asked_nodes;

	if (p.len_host) {
		node_name = urldecode(p.off_host, p.len_host);
		has_node_name = true;
	} else {
		duration<double, std::milli> timeout;
		size_t num_endps = 1;
		if (writable) {
			timeout = 2s;
		} else {
			timeout = 1s;
		}

		if (manager()->is_single_node()) {
			has_node_name = true;
			node_name = local_node.name;
		} else {
			if (!manager()->resolve_index_endpoint(asked_node.path, asked_nodes, num_endps, timeout)) {
				has_node_name = true;
				node_name = local_node.name;
			}
		}
	}

	if (has_node_name) {
#ifdef XAPIAND_CLUSTERING
		Endpoint index("xapian://" + node_name + "/" + index_path);
		int node_port = (index.port == XAPIAND_BINARY_SERVERPORT) ? 0 : index.port;
		node_name = index.host.empty() ? node_name : index.host;

		// Convert node to endpoint:
		char node_ip[INET_ADDRSTRLEN];
		const Node *node = nullptr;
		if (!manager()->touch_node(node_name, UNKNOWN_REGION, &node)) {
			L_DEBUG(this, "Node %s not found", node_name.c_str());
			host = node_name;
			return CMD_UNKNOWN_HOST;
		}
		if (!node_port) {
			node_port = node->binary_port;
		}
		inet_ntop(AF_INET, &(node->addr.sin_addr), node_ip, INET_ADDRSTRLEN);
		Endpoint endpoint("xapian://" + std::string(node_ip) + ":" + std::to_string(node_port) + "/" + index_path, nullptr, -1, node_name);
#else
		Endpoint endpoint(index_path);
#endif
		endpoints.add(endpoint);
	} else {
		for (const auto& asked_node : asked_nodes) {
			endpoints.add(asked_node);
		}
	}
	L_CONN_WIRE(this, "Endpoint: -> %s", endpoints.as_string().c_str());

	p.len_host = 0; //Clean the host, so you not stay with the previous host in case doesn't come new one
	return 0;
}


void
HttpClient::query_maker(const char* query_str, size_t query_size, int cmd, query_field_t& e, parser_query_t& q)
{
	q.offset = nullptr;
	if (url_qs("pretty", query_str, query_size, &q) != -1) {
		e.pretty = true;
		if (q.length) {
			try {
				e.pretty = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
			} catch (const Exception&) { }
		}
	}

	switch (cmd) {
		case CMD_SEARCH:
		case CMD_FACETS:
			q.offset = nullptr;
			if (url_qs("offset", query_str, query_size, &q) != -1) {
				e.offset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
			}

			q.offset = nullptr;
			if (url_qs("check_at_least", query_str, query_size, &q) != -1) {
				e.check_at_least = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
			}

			q.offset = nullptr;
			if (url_qs("limit", query_str, query_size, &q) != -1) {
				e.limit = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
			}

			q.offset = nullptr;
			if (url_qs("collapse_max", query_str, query_size, &q) != -1) {
				e.collapse_max = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
			}

			q.offset = nullptr;
			if (url_qs("spelling", query_str, query_size, &q) != -1) {
				e.spelling = true;
				if (q.length) {
					try {
						e.spelling = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
					} catch (const Exception&) { }
				}
			}

			q.offset = nullptr;
			if (url_qs("synonyms", query_str, query_size, &q) != -1) {
				e.synonyms = true;
				if (q.length) {
					try {
						e.synonyms = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
					} catch (const Exception&) { }
				}
			}

			q.offset = nullptr;
			L_DEBUG(this, "Buffer: %s", query_str);
			while (url_qs("query", query_str, query_size, &q) != -1) {
				L_DEBUG(this, "%s", urldecode(q.offset, q.length).c_str());
				e.query.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("q", query_str, query_size, &q) != -1) {
				L_DEBUG(this, "%s", urldecode(q.offset, q.length).c_str());
				e.query.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("partial", query_str, query_size, &q) != -1) {
				e.partial.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("terms", query_str, query_size, &q) != -1) {
				e.terms.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("sort", query_str, query_size, &q) != -1) {
				e.sort.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("facets", query_str, query_size, &q) != -1) {
				e.facets.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			while (url_qs("language", query_str, query_size, &q) != -1) {
				e.language.push_back(urldecode(q.offset, q.length));
			}

			q.offset = nullptr;
			if (url_qs("collapse", query_str, query_size, &q) != -1) {
				e.collapse = urldecode(q.offset, q.length);
			}

			q.offset = nullptr;
			if (url_qs("fuzzy", query_str, query_size, &q) != -1) {
				e.is_fuzzy = true;
				if (q.length) {
					try {
						e.is_fuzzy = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
					} catch (const Exception&) { }
				}
			}

			if (e.is_fuzzy) {
				q.offset = nullptr;
				if (url_qs("fuzzy.n_rset", query_str, query_size, &q) != -1){
					e.fuzzy.n_rset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("fuzzy.n_eset", query_str, query_size, &q) != -1){
					e.fuzzy.n_eset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("fuzzy.n_term", query_str, query_size, &q) != -1){
					e.fuzzy.n_term = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				while (url_qs("fuzzy.field", query_str, query_size, &q) != -1){
					e.fuzzy.field.push_back(urldecode(q.offset, q.length));
				}

				q.offset = nullptr;
				while (url_qs("fuzzy.type", query_str, query_size, &q) != -1){
					e.fuzzy.type.push_back(urldecode(q.offset, q.length));
				}
			}

			q.offset = nullptr;
			if (url_qs("nearest", query_str, query_size, &q) != -1) {
				e.is_nearest = true;
				if (q.length) {
					try {
						e.is_nearest = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
					} catch (const Exception&) { }
				}
			}

			if (e.is_nearest) {
				q.offset = nullptr;
				if (url_qs("nearest.n_rset", query_str, query_size, &q) != -1){
					e.nearest.n_rset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				} else {
					e.nearest.n_rset = 5;
				}

				q.offset = nullptr;
				if (url_qs("nearest.n_eset", query_str, query_size, &q) != -1){
					e.nearest.n_eset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("nearest.n_term", query_str, query_size, &q) != -1){
					e.nearest.n_term = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				while (url_qs("nearest.field", query_str, query_size, &q) != -1){
					e.nearest.field.push_back(urldecode(q.offset, q.length));
				}

				q.offset = nullptr;
				while (url_qs("nearest.type", query_str, query_size, &q) != -1){
					e.nearest.type.push_back(urldecode(q.offset, q.length));
				}
			}
			break;

		case CMD_ID:
			q.offset = nullptr;
			if (url_qs("commit", query_str, query_size, &q) != -1) {
				e.commit = true;
				if (q.length) {
					try {
						e.commit = Serialise::boolean(urldecode(q.offset, q.length)) == "t";
					} catch (const Exception&) { }
				}
			}

			if (isRange(command)) {
				q.offset = nullptr;
				if (url_qs("offset", query_str, query_size, &q) != -1) {
					e.offset = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("check_at_least", query_str, query_size, &q) != -1) {
					e.check_at_least = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("limit", query_str, query_size, &q) != -1) {
					e.limit = static_cast<unsigned>(std::stoul(urldecode(q.offset, q.length)));
				}

				q.offset = nullptr;
				if (url_qs("sort", query_str, query_size, &q) != -1) {
					e.sort.push_back(urldecode(q.offset, q.length));
				} else {
					e.sort.push_back(RESERVED_ID);
				}
			} else {
				e.limit = 1;
				e.unique_doc = true;
				e.offset = 0;
				e.check_at_least = 0;
			}
			break;

		case CMD_STATS:
			break;

		case CMD_UPLOAD:
			break;
	}
}


int
HttpClient::identify_cmd(const std::string& command)
{
	if (command.empty()) {
		return CMD_HOME;
	}

	if (command.compare(HTTP_SEARCH) == 0) {
		return CMD_SEARCH;
	}

	if (command.compare(HTTP_FACETS) == 0) {
		return CMD_FACETS;
	}

	if (command.compare(HTTP_STATS) == 0) {
		return CMD_STATS;
	}

	if (command.compare(HTTP_SCHEMA) == 0) {
		return CMD_SCHEMA;
	}

	if (command.compare(HTTP_UPLOAD) == 0) {
		return CMD_UPLOAD;
	}

	return CMD_ID;
}


int
HttpClient::identify_mode(const std::string& mode)
{
	if (mode.compare(HTTP_STATS) == 0) {
		return CMD_STATS;
	}

	if (mode.compare(HTTP_UPLOAD) == 0) {
		return CMD_UPLOAD;
	}
	return CMD_UNKNOWN;
}


void
HttpClient::clean_http_request()
{
	L_CALL(this, "HttpClient::clean_http_request()");

	path.clear();
	body.clear();
	header_name.clear();
	header_value.clear();
	content_type.clear();
	content_length.clear();
	host.clear();
	command.clear();

	response_ends = std::chrono::system_clock::now();
	request_begining = true;
	L_TIME(this, "Full request took %s, response took %s", delta_string(request_begins, response_ends).c_str(), delta_string(response_begins, response_ends).c_str());

	async_read.send();
	http_parser_init(&parser, HTTP_REQUEST);
}

const std::pair<std::string, std::string>*
HttpClient::is_acceptable_type(const std::pair<std::string, std::string>& ct_type_pattern, const std::pair<std::string, std::string>& ct_type)
{
	L_CALL(this, "HttpClient::is_acceptable_type()");

	bool type_ok = false, subtype_ok = false;
	if (ct_type_pattern.first == "*") {
		type_ok = true;
	} else {
		type_ok = ct_type_pattern.first == ct_type.first;
	}
	if (ct_type_pattern.second == "*") {
		subtype_ok = true;
	} else {
		subtype_ok = ct_type_pattern.second == ct_type.second;
	}
	if (type_ok && subtype_ok) {
		return &ct_type;
	}
	return nullptr;
}

const std::pair<std::string, std::string>*
HttpClient::is_acceptable_type(const std::pair<std::string, std::string>& ct_type_pattern, const std::vector<std::pair<std::string, std::string>>& ct_types)
{
	for (auto& ct_type : ct_types) {
		if (is_acceptable_type(ct_type_pattern, ct_type)) {
			return &ct_type;
		}
	}
	return nullptr;
}

template <typename T>
const std::pair<std::string, std::string>&
HttpClient::get_acceptable_type(const T& ct)
{
	L_CALL(this, "HttpClient::get_acceptable_type()");

	if (accept_set.empty()) {
		if (!content_type.empty()) accept_set.insert(std::tuple<double, int, std::pair<std::string, std::string>>(1, 0, content_type_pair(content_type)));
		accept_set.insert(std::make_tuple(1, 1, std::make_pair(std::string("*"), std::string("*"))));
	}
	for (const auto& accept : accept_set) {
		if (is_acceptable_type(std::get<2>(accept), ct)) {
			return std::get<2>(accept);
		}
	}
	return std::get<2>(*accept_set.begin());
}


std::pair<std::string, std::string>
HttpClient::serialize_response(const MsgPack& obj, const std::pair<std::string, std::string>& ct_type, bool pretty, bool serialize_error)
{
	L_CALL(this, "HttpClient::serialize_response()");

	if (is_acceptable_type(ct_type, json_type)) {
		return std::make_pair(obj.to_json_string(pretty), json_type.first + "/" + json_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, msgpack_type)) {
		return std::make_pair(obj.to_string(), msgpack_type.first + "/" + msgpack_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, html_type)) {
		std::function<std::string(const msgpack::object&)> html_serialize = serialize_error ? msgpack_to_html_error : msgpack_to_html;
		return std::make_pair(html_serialize(*(obj.body->obj)), html_type.first + "/" + html_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, text_type)) {
		/*
		 error:
			{{ RESPONSE_STATUS }} - {{ RESPONSE_MESSAGE }}

		 obj:
			{{ key1 }}: {{ val1 }}
			{{ key2 }}: {{ val2 }}
			...

		 array:
			{{ val1 }}, {{ val2 }}, ...
		 */
	}
	throw MSG_SerialisationError("Type is not serializable");
}


void
HttpClient::write_http_response(const MsgPack& response,  int status_code, bool pretty)
{
	L_CALL(this, "HttpClient::write_http_response()");

	auto ct_type = content_type_pair(content_type);
	std::vector<std::pair<std::string, std::string>> ct_types;
	if (ct_type == json_type || ct_type == msgpack_type || content_type.empty()) {
		ct_types = msgpack_serializers;
	} else {
		ct_types.push_back(ct_type);
	}

	const auto& accepted_type = get_acceptable_type(ct_types);
	try {
		auto result = serialize_response(response, accepted_type, pretty,  status_code >= 400);
		write(http_response(status_code, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result.first, result.second));
	} catch (const SerialisationError& exc) {
		status_code = 406;
		MsgPack response_err;
		response_err[RESPONSE_STATUS] = status_code;
		response_err[RESPONSE_MESSAGE] = std::string("Response type " + accepted_type.first + "/" + accepted_type.second + " " + exc.what());
		auto response_str = response_err.to_json_string();
		write(http_response(status_code, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor, 0, response_str));
		return;
	}
}
