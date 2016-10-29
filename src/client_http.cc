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

#include "client_http.h"

#include "xxh64.hpp"
#include "io_utils.h"
#include "length.h"
#include "multivalue/aggregation.h"
#include "serialise.h"
#include "utils.h"

#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <regex>
#include <unistd.h>

#include <arpa/inet.h>
#include <sysexits.h>
#include <sys/socket.h>

#define RESPONSE_MESSAGE "_message"
#define RESPONSE_STATUS  "_status"

#define MAX_BODY_SIZE (250 * 1024 * 1024)
#define MAX_BODY_MEM (5 * 1024 * 1024)

#define QUERY_FIELD_COMMIT (1 << 0)
#define QUERY_FIELD_SEARCH (1 << 1)
#define QUERY_FIELD_ID     (1 << 2)
#define QUERY_FIELD_TIME   (1 << 3)


type_t content_type_pair(const std::string& ct_type) {
	std::size_t found = ct_type.find_last_of("/");
	if (found == std::string::npos) {
		return  make_pair(std::string(), std::string());
	}
	const char* content_type_str = ct_type.c_str();
	return make_pair(std::string(content_type_str, found), std::string(content_type_str, found + 1, ct_type.size()));
}


static const auto any_type     = content_type_pair(ANY_CONTENT_TYPE);
static const auto json_type    = content_type_pair(JSON_CONTENT_TYPE);
static const auto msgpack_type = content_type_pair(MSGPACK_CONTENT_TYPE);
static const auto html_type    = content_type_pair(HTML_CONTENT_TYPE);
static const auto text_type    = content_type_pair(TEXT_CONTENT_TYPE);
static const auto msgpack_serializers = std::vector<type_t>({json_type, msgpack_type, html_type, text_type});


static const std::regex header_accept_re("([-a-z+]+|\\*)/([-a-z+]+|\\*)(?:[^,]*;q=(\\d+(?:\\.\\d+)?))?");


static const char* http_status[6][14] = {
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
		"Precondition Failed",      // 412
		"Request Entity Too Large"  // 413
	},
	{
		"Internal Server Error",    // 500
		"Not Implemented",          // 501
		"Bad Gateway"               // 502
	}
};


GuidGenerator HttpClient::generator;

AcceptLRU HttpClient::accept_sets;


std::string
HttpClient::http_response(int status, int mode, unsigned short http_major, unsigned short http_minor, int total_count, int matches_estimated, const std::string& body, const std::string& ct_type, const std::string& ct_encoding) {
	L_CALL(this, "HttpClient::http_response()");

	char buffer[20];
	std::string headers;
	std::string response;
	const std::string eol("\r\n");

	if (mode & HTTP_STATUS) {
		response_status = status;

		snprintf(buffer, sizeof(buffer), "HTTP/%d.%d %d ", http_major, http_minor, status);
		headers += buffer;
		headers += http_status[status / 100][status % 100] + eol;
		if (!(mode & HTTP_HEADER)) {
			headers += eol;
		}
	}

	if (mode & HTTP_HEADER) {
		headers += "Server: " + std::string(PACKAGE_NAME) + "/" + std::string(VERSION) + eol;

		response_ends = std::chrono::system_clock::now();
		headers += "Response-Time: " + delta_string(request_begins, response_ends) + eol;
		if (operation_ends >= operation_begins) {
			headers += "Operation-Time: " + delta_string(operation_begins, operation_ends) + eol;
		}

		if (mode & HTTP_OPTIONS) {
			headers += "Allow: GET,HEAD,POST,PUT,PATCH,OPTIONS" + eol;
		}

		if (mode & HTTP_TOTAL_COUNT) {
			headers += "Total-Count: " + std::to_string(total_count) + eol;
		}

		if (mode & HTTP_MATCHES_ESTIMATED) {
			headers += "Matches-Estimated: " + std::to_string(matches_estimated) + eol;
		}

		if (mode & HTTP_CONTENT_TYPE) {
			headers += "Content-Type: " + ct_type + eol;
		}

		if (mode & HTTP_CONTENT_ENCODING) {
			headers += "Content-Encoding: " + ct_encoding + eol;
		}

		if (mode & HTTP_CHUNKED) {
			headers += "Transfer-Encoding: chunked" + eol;
		} else {
			headers += "Content-Length: ";
			snprintf(buffer, sizeof(buffer), "%lu", body.size());
			headers += buffer + eol;
		}
		headers += eol;
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

	response_size += response.size();

	if (!(mode & HTTP_CHUNKED) && !(mode & HTTP_EXPECTED100)) {
		clean_http_request();
	}

	return headers + response;
}


HttpClient::HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_)
	: BaseClient(std::move(server_), ev_loop_, ev_flags_, sock_),
	  pretty(false),
	  response_size(0),
	  body_size(0),
	  body_descriptor(0),
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

	L_CONN(this, "New Http Client {sock:%d}, %d client(s) of a total of %d connected.", sock.load(), http_clients, total_clients);

	response_log = LOG_DELAYED(true, 300s, LOG_WARNING, MAGENTA, this, "Client idle for too long...").release();
	idle = true;

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

	if (XapiandManager::manager->shutdown_asap.load()) {
		if (http_clients <= 0) {
			XapiandManager::manager->shutdown_sig(0);
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

	if (!response_log.load()->LOG_DELAYED_CLEAR()) {
		LOG(LOG_NOTICE, LIGHT_RED, this, "Client killed!");
	}

	L_OBJ(this, "DELETED HTTP CLIENT! (%d clients left)", http_clients);
}


void
HttpClient::on_read(const char* buf, ssize_t received)
{
	L_CALL(this, "HttpClient::on_read(<buf>, %zd)", received);

	unsigned init_state = parser.state;

	if (received <= 0) {
		response_log.load()->LOG_DELAYED_CLEAR();
		if (received < 0 || init_state != 18 || !write_queue.empty()) {
			LOG(LOG_ERR, LIGHT_RED, this, "Client unexpectedly closed the other end! [%d]", init_state);
			destroy();  // Handle error. Just close the connection.
			detach();
		}
		return;
	}

	if (request_begining) {
		idle = false;
		request_begining = false;
		request_begins = std::chrono::system_clock::now();
		response_log.load()->LOG_DELAYED_CLEAR();
		response_log = LOG_DELAYED(true, 10s, LOG_WARNING, MAGENTA, this, "Request taking too long...").release();
		response_logged = false;
	}

	L_HTTP_WIRE(this, "HttpClient::on_read: %zd bytes", received);
	ssize_t parsed = http_parser_execute(&parser, &settings, buf, received);
	if (parsed == received) {
		unsigned final_state = parser.state;
		if (final_state == init_state) {
			if (received == 1 and buf[0] == '\n') {  // ignore '\n' request
				request_begining = true;
				response_log.load()->LOG_DELAYED_CLEAR();
				response_log = LOG_DELAYED(true, 300s, LOG_WARNING, MAGENTA, this, "Client idle for too long...").release();
				idle = true;
				return;
			}
		}
		if (final_state == 1 || final_state == 18) {  // dead or message_complete
			L_EV(this, "Disable read event {sock:%d}", sock.load());
			io_read.stop();
			written = 0;
			if (!closed) {
				XapiandManager::manager->thread_pool.enqueue(share_this<HttpClient>());
			}
		}
	} else {
		int error_code = 400;
		std::string message(http_errno_description(HTTP_PARSER_ERRNO(&parser)));
		MsgPack err_response = {
			{ RESPONSE_STATUS,  error_code },
			{ RESPONSE_MESSAGE, message }
		};
		write_http_response(error_code, err_response);
		L_HTTP_PROTO(this, HTTP_PARSER_ERRNO(&parser) != HPE_OK ? message.c_str() : "incomplete request");
		destroy();  // Handle error. Just close the connection.
		detach();
	}
}


void
HttpClient::on_read_file(const char*, ssize_t received)
{
	L_CALL(this, "HttpClient::on_read_file(<buf>, %zd)", received);

	L_ERR(this, "Not Implemented: HttpClient::on_read_file: %zd bytes", received);
}


void
HttpClient::on_read_file_done()
{
	L_CALL(this, "HttpClient::on_read_file_done()");

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

	L_CALL(self, "HttpClient::on_info(...)");

	int state = p->state;

	L_HTTP_PROTO_PARSER(self, "%4d - (INFO)", state);

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

	L_CALL(self, "HttpClient::on_data(...)");

	int state = p->state;

	L_HTTP_PROTO_PARSER(self, "%4d - %s", state, repr(at, length).c_str());

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
				try {
					self->accept_set = accept_sets.at(value);
				} catch (std::range_error) {
					std::sregex_iterator next(value.begin(), value.end(), header_accept_re, std::regex_constants::match_any);
					std::sregex_iterator end;
					int i = 0;
					while (next != end) {
						next->length(3) != 0 ? self->accept_set.insert(std::make_tuple(std::stod(next->str(3)), i, std::make_pair(next->str(1), next->str(2))))
							: self->accept_set.insert(std::make_tuple(1, i, std::make_pair(next->str(1), next->str(2))));
						++next;
						++i;
					}
					accept_sets.insert(std::make_pair(value, self->accept_set));
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
	try {
		_run();
	} catch (...) {
		cleanup();
		throw;
	}
	cleanup();
}


void
HttpClient::_run()
{
	L_CALL(this, "HttpClient::_run()");

	L_CONN(this, "Start running in worker {sock:%d}.", sock.load());

	L_OBJ_BEGIN(this, "HttpClient::run:BEGIN");
	response_begins = std::chrono::system_clock::now();
	response_log.load()->LOG_DELAYED_CLEAR();
	response_log = LOG_DELAYED(true, 1s, LOG_WARNING, MAGENTA, this, "Response taking too long...").release();

	std::string error;
	int error_code = 0;

	try {
		switch (static_cast<HttpMethod>(parser.method)) {
			case HttpMethod::DELETE:
				_delete();
				break;
			case HttpMethod::GET:
				_get();
				break;
			case HttpMethod::POST:
				_post();
				break;
			case HttpMethod::HEAD:
				_head();
				break;
			case HttpMethod::PUT:
				_put();
				break;
			case HttpMethod::OPTIONS:
				_options();
				break;
			case HttpMethod::PATCH:
				_patch();
			default:
				write_http_response(501);
				break;
		}
	} catch (const DocNotFoundError&) {
		error_code = 404;
		error.assign("Document not found");
	} catch (const MissingTypeError& exc) {
		error_code = 412;
		error.assign(exc.what());
	} catch (const ClientError& exc) {
		error_code = 400;
		error.assign(exc.what());
	} catch (const CheckoutError& exc) {
		error_code = 502;
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
		if (written) {
			destroy();
			detach();
		} else {
			MsgPack err_response = {
				{ RESPONSE_STATUS, error_code },
				{ RESPONSE_MESSAGE, error }
			};

			write_http_response(error_code, err_response);
		}
	}

	L_OBJ_END(this, "HttpClient::run:END");
}


void
HttpClient::_options()
{
	L_CALL(this, "HttpClient::_options()");

	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_OPTIONS | HTTP_BODY, parser.http_major, parser.http_minor));
}


void
HttpClient::_head()
{
	L_CALL(this, "HttpClient::_head()");

	switch (url_resolve()) {
		case Command::NO_CMD_NO_ID:
			write_http_response(200);
			break;
		case Command::NO_CMD_ID:
			document_info_view(HttpMethod::HEAD);
			break;
		default:
			status_view(400);
			break;
	}
}


void
HttpClient::_get()
{
	L_CALL(this, "HttpClient::_get()");

	switch (url_resolve()) {
		case Command::NO_CMD_NO_ID:
			home_view(HttpMethod::GET);
			break;
		case Command::NO_CMD_ID:
			search_view(HttpMethod::GET);
			break;
		case Command::CMD_SEARCH:
			path_parser.off_id = nullptr;  // Command has no ID
			search_view(HttpMethod::GET);
			break;
		case Command::CMD_SCHEMA:
			path_parser.off_id = nullptr;  // Command has no ID
			schema_view(HttpMethod::GET);
			break;
		case Command::CMD_INFO:
			info_view(HttpMethod::GET);
			break;
		case Command::CMD_NODES:
			nodes_view(HttpMethod::GET);
			break;
		default:
			status_view(400);
			break;
	}
}


void
HttpClient::_put()
{
	L_CALL(this, "HttpClient::_put()");

	switch (url_resolve()) {
		case Command::NO_CMD_ID:
			index_document_view(HttpMethod::PUT);
			break;
		case Command::CMD_SCHEMA:
			path_parser.off_id = nullptr;  // Command has no ID
			write_schema_view(HttpMethod::PUT);
			break;
		default:
			status_view(400);
			break;
	}
}


void
HttpClient::_post()
{
	L_CALL(this, "HttpClient::_post()");

	switch (url_resolve()) {
		case Command::NO_CMD_ID:
			path_parser.off_id = nullptr;  // Command has no ID
			index_document_view(HttpMethod::POST);
			break;
		case Command::CMD_SCHEMA:
			path_parser.off_id = nullptr;  // Command has no ID
			write_schema_view(HttpMethod::POST);
			break;
		case Command::CMD_SEARCH:
			path_parser.off_id = nullptr;  // Command has no ID
			search_view(HttpMethod::POST);
			break;
#ifndef NDEBUG
		case Command::CMD_QUIT:
			XapiandManager::manager->shutdown_asap.store(epoch::now<>());
			XapiandManager::manager->shutdown_sig(0);
			break;
#endif
		default:
			status_view(400);
			break;
	}
}


void
HttpClient::_patch()
{
	L_CALL(this, "HttpClient::_patch()");

	switch (url_resolve()) {
		case Command::NO_CMD_ID:
			update_document_view(HttpMethod::PATCH);
			break;
		default:
			status_view(400);
			break;
	}
}


void
HttpClient::_delete()
{
	L_CALL(this, "HttpClient::_delete()");

	switch (url_resolve()) {
		case Command::NO_CMD_ID:
			delete_document_view(HttpMethod::DELETE);
			break;
		default:
			status_view(400);
			break;
	}
}


std::pair<std::string, MsgPack>
HttpClient::get_body()
{
	// Create MsgPack object for the body
	auto ct_type = content_type;

	if (ct_type.empty()) {
		ct_type = FORM_URLENCODED_CONTENT_TYPE;
	}
	MsgPack msgpack;
	rapidjson::Document rdoc;
	switch (xxh64::hash(ct_type)) {
		case xxh64::hash(FORM_URLENCODED_CONTENT_TYPE):
			try {
				json_load(rdoc, body);
				msgpack = MsgPack(rdoc);
				ct_type = JSON_CONTENT_TYPE;
			} catch (const std::exception&) {
				msgpack = MsgPack(body);
			}
			break;
		case xxh64::hash(JSON_CONTENT_TYPE):
			json_load(rdoc, body);
			msgpack = MsgPack(rdoc);
			break;
		case xxh64::hash(MSGPACK_CONTENT_TYPE):
			msgpack = MsgPack::unserialise(body);
			break;
		default:
			msgpack = MsgPack(body);
			break;
	}

	return std::make_pair(ct_type, msgpack);
}


void
HttpClient::home_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::home_view()");

	endpoints.clear();
	endpoints.add(Endpoint("."));

	db_handler.reset(endpoints, DB_SPAWN, method);

	auto local_node_ = local_node.load();
	auto document = db_handler.get_document("." + serialise_node_id(local_node_->id));

	auto obj_data = document.get_obj();
	obj_data[ID_FIELD_NAME] = document.get_value(ID_FIELD_NAME);

#ifdef XAPIAND_CLUSTERING
	obj_data["_cluster_name"] = XapiandManager::manager->cluster_name;
#endif
	obj_data["_version"] = {
		{ "_mastery", PACKAGE_VERSION },
		{ "_number", PACKAGE_VERSION  },
		{ "_xapian", Xapian::version_string() }
	};

	write_http_response(200, obj_data);
}


void
HttpClient::document_info_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::document_info_view()");

	endpoints_maker(1s);

	db_handler.reset(endpoints, DB_SPAWN, method);

	MsgPack response;
	response["doc_id"] = db_handler.get_docid(path_parser.get_id());

	write_http_response(200, response);
}


void
HttpClient::delete_document_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::delete_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	std::string doc_id(path_parser.get_id());

	int status_code;
	MsgPack response;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN, method);

	if (endpoints.size() == 1) {
		operation_begins = std::chrono::system_clock::now();
		db_handler.delete_document(doc_id, query_field->commit);
		operation_ends = std::chrono::system_clock::now();
		status_code = 200;

		response["_delete"] = {
			{ ID_FIELD_NAME, doc_id },
			{ "_commit",  query_field->commit }
		};
	} else {
		operation_begins = std::chrono::system_clock::now();
		endpoints_error_list err_list = db_handler.multi_db_delete_document(doc_id, query_field->commit);
		operation_ends = std::chrono::system_clock::now();

		if (err_list.empty()) {
			status_code = 200;
			response["_delete"] = {
				{ ID_FIELD_NAME, doc_id },
				{ "_commit",  query_field->commit }
			};
		} else {
			status_code = 400;
			for (const auto& err : err_list) {
				MsgPack o;
				for (const auto& end : err.second) {
					o.push_back(end);
				}
				response["_delete"].insert(err.first, o);
			}
		}
	}

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		stats_cnt.del.min[b_time.minute] += endpoints.size();
		stats_cnt.del.sec[b_time.second] += endpoints.size();
		stats_cnt.del.tm_min[b_time.minute] += _time;
		stats_cnt.del.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Deletion took %s", delta_string(operation_begins, operation_ends).c_str());


	write_http_response(status_code, response);
}


void
HttpClient::index_document_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::index_document_view()");

	std::string doc_id;
	int status_code = 400;

	if (method == HttpMethod::POST) {
		auto g = generator.newGuid();
		doc_id = g.to_string();
	} else {
		doc_id = path_parser.get_id();
	}

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	for (auto& index : index_paths) {
		build_path_index(index);
	}

	operation_begins = std::chrono::system_clock::now();

	auto body_ = get_body();
	endpoints_error_list err_list;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	db_handler.index(doc_id, body_.second, query_field->commit, body_.first, &err_list);

	operation_ends = std::chrono::system_clock::now();

	auto _time = std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count();
	{
		std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
		update_pos_time();
		stats_cnt.index.min[b_time.minute] += endpoints.size();
		stats_cnt.index.sec[b_time.second] += endpoints.size();
		stats_cnt.index.tm_min[b_time.minute] += _time;
		stats_cnt.index.tm_sec[b_time.second] += _time;
	}
	L_TIME(this, "Indexing took %s", delta_string(operation_begins, operation_ends).c_str());

	MsgPack response;
	if (err_list.empty()) {
		status_code = 200;
		response["_index"] = {
			{ ID_FIELD_NAME, doc_id },
			{ "_commit", query_field->commit }
		};
	} else {
		for (const auto& err : err_list) {
			MsgPack o;
			for (const auto& end : err.second) {
				o.push_back(end);
			}
			response["_index"].insert(err.first, o);
		}
	}

	write_http_response(status_code, response);
}


void
HttpClient::write_schema_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::write_schema_view()");

	int status_code = 400;

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	for (auto& index : index_paths) {
		build_path_index(index);
	}

	endpoints_error_list err_list;
	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	db_handler.write_schema(body);

	operation_ends = std::chrono::system_clock::now();

	MsgPack response;
	if (err_list.empty()) {
		status_code = 200;
		response = db_handler.get_schema()->get_readable();
	} else {
		for (const auto& err : err_list) {
			MsgPack o;
			for (const auto& end : err.second) {
				o.push_back(end);
			}
			response["_schema"].insert(err.first, o);
		}
	}

	write_http_response(status_code, response);
}


void
HttpClient::update_document_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::update_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	std::string doc_id(path_parser.get_id());
	int status_code = 400;

	operation_begins = std::chrono::system_clock::now();

	auto body_ = get_body();
	endpoints_error_list err_list;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	db_handler.patch(doc_id, body_.second, query_field->commit, body_.first, &err_list);

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

	MsgPack response;
	if (err_list.empty()) {
		status_code = 200;
		response["_update"] = {
			{ ID_FIELD_NAME, doc_id },
			{ "_commit", query_field->commit }
		};
	} else {
		for (const auto& err : err_list) {
			MsgPack o;
			for (const auto& end : err.second) {
				o.push_back(end);
			}
			response["_update"].insert(err.first, o);
		}
	}

	write_http_response(status_code, response);
}


void
HttpClient::info_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::info_view()");

	MsgPack response;
	bool res_stats = false;

	if (!path_parser.off_id) {
		query_field_maker(QUERY_FIELD_TIME);
		XapiandManager::manager->server_status(response["_server_info"]);
		XapiandManager::manager->get_stats_time(response["_stats_time"], query_field->time);
		res_stats = true;
	} else {
		endpoints_maker(1s);

		db_handler.reset(endpoints, DB_OPEN, method);
		try {
			db_handler.get_document_info(response["_document_info"], path_parser.get_id());
		} catch (const CheckoutError&) {
			path_parser.off_id = nullptr;
			response.erase("_document_info");
		}

		path_parser.rewind();
		endpoints_maker(1s);

		db_handler.reset(endpoints, DB_OPEN, method);
		db_handler.get_database_info(response["_database_info"]);
		res_stats = true;
	}

	if (res_stats) {
		write_http_response(200, response);
	} else {
		status_view(404);
	}
}


void
HttpClient::nodes_view(HttpMethod)
{
	L_CALL(this, "HttpClient::nodes_view()");

	path_parser.off_id = nullptr;  // Command has no ID

	path_parser.next();
	if (path_parser.next() != PathParser::State::END) {
		status_view(404);
		return;
	}

	if (path_parser.len_pth || path_parser.len_pmt || path_parser.len_ppmt) {
		status_view(404);
		return;
	}

	MsgPack nodes(MsgPack::Type::MAP);

	// FIXME: Get all nodes from cluster database:
	auto local_node_ = local_node.load();
	nodes["." + serialise_node_id(local_node_->id)] = {
		{ "_name", local_node_->name },
	};

	write_http_response(200, {
		{ "_cluster_name", XapiandManager::manager->cluster_name },
		{ "_nodes", nodes },
	});
}


void
HttpClient::schema_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::schema_view()");

	endpoints_maker(1s);

	db_handler.reset(endpoints, DB_SPAWN, method);
	write_http_response(200, db_handler.get_schema()->get_readable());
}


void
HttpClient::search_view(HttpMethod method)
{
	L_CALL(this, "HttpClient::search_view()");

	bool chunked = !path_parser.off_id || isRange(path_parser.get_id());

	int query_field_flags = 0;

	if (!path_parser.off_id) {
		query_field_flags |= QUERY_FIELD_SEARCH;
	} else {
		query_field_flags |= QUERY_FIELD_ID;
	}

	endpoints_maker(1s);
	query_field_maker(query_field_flags);

	if (path_parser.off_id) {
		query_field->query.push_back(std::string(ID_FIELD_NAME)  + ":" +  path_parser.get_id());
	}

	MSet mset;
	std::vector<std::string> suggestions;

	int db_flags = DB_OPEN;

	if (query_field->volatile_) {
		db_flags |= DB_WRITABLE;
	}

	operation_begins = std::chrono::system_clock::now();

	MsgPack aggregations;
	try {
		db_handler.reset(endpoints, db_flags, method);
		if (!body.empty()) {
			rapidjson::Document json_aggs;
			json_load(json_aggs, body);
			MsgPack object(json_aggs);
			AggregationMatchSpy aggs(object, db_handler.get_schema());
			mset = db_handler.get_mset(*query_field, &aggs, &object, suggestions);
			aggregations = aggs.get_aggregation().at(AGGREGATION_AGGS);
		} else {
			mset = db_handler.get_mset(*query_field, nullptr, nullptr, suggestions);
		}
	} catch (const CheckoutError&) {
		/* At the moment when the endpoint it not exist and it is chunck it will return 200 response
		 * and zero matches this behavior may change in the future for instance ( return 404 )
		 * if is not chunk return 404
		 */
	}

	L_SEARCH(this, "Suggested queries: %s", [&suggestions]() {
		MsgPack res(MsgPack::Type::ARRAY);
		for (const auto& suggestion : suggestions) {
			res.push_back(suggestion);
		}
		return res;
	}().to_string().c_str());

	int rc = 0;
	if (!chunked && mset.empty()) {
		int error_code = 404;
		MsgPack err_response = {
			{ RESPONSE_STATUS, error_code },
			{ RESPONSE_MESSAGE, "No document found" }
		};
		write_http_response(error_code, err_response);
	} else {
		std::string first_chunk;
		std::string last_chunk;

		if (chunked) {
			if (pretty) {
				first_chunk.append("{");
				if (aggregations) {
					first_chunk.append("\n    \"_aggregations\": ").append(indent_string(aggregations.to_string(true),' ', 4, false)).append(",");
				}
				first_chunk.append("\n    \"_query\": {");
				first_chunk.append("\n        \"_total_count\": ").append(std::to_string(mset.size())).append(",");
				first_chunk.append("\n        \"_matches_estimated\": ").append(std::to_string(mset.get_matches_estimated())).append(",");
				first_chunk.append("\n        \"_hits\": [");
				first_chunk.append("\n");
				first_chunk.append("\n");
				last_chunk.append("        ]");
				last_chunk.append("\n    }");
				last_chunk.append("\n}");
			} else {
				first_chunk.append("{");
				if (aggregations) {
					first_chunk.append("\"_aggregations\":").append(aggregations.to_string()).append(",");
				}
				first_chunk.append("\"_query\": {");
				first_chunk.append("\"_total_count\":").append(std::to_string(mset.size())).append(",");
				first_chunk.append("\"_matches_estimated\":").append(std::to_string(mset.get_matches_estimated())).append(",");
				first_chunk.append("\"_hits\":[");
				first_chunk.append("\n");
				first_chunk.append("\n");
				last_chunk.append("]");
				last_chunk.append("}");
				last_chunk.append("}");
			}
		}
		std::string buffer;
		for (auto m = mset.begin(); m != mset.end(); ++rc, ++m) {
			auto document = db_handler.get_document(*m);

			std::string ct_type_str = chunked ? MSGPACK_CONTENT_TYPE : document.get_value(DB_SLOT_CONTENT_TYPE);

			if (ct_type_str == JSON_CONTENT_TYPE || ct_type_str == MSGPACK_CONTENT_TYPE) {
				if (is_acceptable_type(get_acceptable_type(json_type), json_type)) {
					ct_type_str = JSON_CONTENT_TYPE;
				} else if (is_acceptable_type(get_acceptable_type(msgpack_type), msgpack_type)) {
					ct_type_str = MSGPACK_CONTENT_TYPE;
				}
			}
			auto ct_type = content_type_pair(ct_type_str);

			std::vector<type_t> ct_types;
			if (ct_type == json_type || ct_type == msgpack_type) {
				ct_types = msgpack_serializers;
			} else {
				ct_types.push_back(ct_type);
			}

			const auto& accepted_type = get_acceptable_type(ct_types);
			const auto accepted_ct_type = is_acceptable_type(accepted_type, ct_types);
			if (!accepted_ct_type) {
				int error_code = 406;
				MsgPack err_response = {
					{ RESPONSE_STATUS, error_code },
					{ RESPONSE_MESSAGE, std::string("Response type " + ct_type.first + "/" + ct_type.second + " not provided in the accept header") }
				};
				write_http_response(error_code, err_response);
				L_SEARCH(this, "ABORTED SEARCH");
				return;
			}

			MsgPack obj_data;
			if (is_acceptable_type(json_type, ct_type)) {
				obj_data = document.get_obj();
			} else if (is_acceptable_type(msgpack_type, ct_type)) {
				obj_data = document.get_obj();
			} else {
				// Returns blob_data in case that type is unkown
				auto blob_data = document.get_blob();
				write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_BODY, parser.http_major, parser.http_minor, 0, 0, blob_data, ct_type_str));
				return;
			}

			ct_type = *accepted_ct_type;
			ct_type_str = ct_type.first + "/" + ct_type.second;

			obj_data[ID_FIELD_NAME] = document.get_value(ID_FIELD_NAME);
			// Detailed info about the document:
			obj_data[RESERVED_RANK] = m.get_rank();
			obj_data[RESERVED_WEIGHT] = m.get_weight();
			obj_data[RESERVED_PERCENT] = m.get_percent();
			int subdatabase = (document.get_docid() - 1) % endpoints.size();
			auto endpoint = endpoints[subdatabase];
			obj_data[RESERVED_ENDPOINT] = endpoint.to_string();

			auto result = serialize_response(obj_data, ct_type, pretty);
			if (chunked) {
				if (rc == 0) {
					write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_CHUNKED | HTTP_TOTAL_COUNT | HTTP_MATCHES_ESTIMATED, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type_str));
					write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, first_chunk));
				}

				if (!buffer.empty()) {
					if (!write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, (pretty ? indent_string(buffer, ' ', 3 * 4) : buffer) + ",\n\n"))) {
						// TODO: log eror?
						break;
					}
				}
				buffer = result.first;
			} else if (!write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second))) {
				// TODO: log eror?
				break;
			}
		}

		if (chunked) {
			if (rc == 0) {
				write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CHUNKED | HTTP_TOTAL_COUNT | HTTP_MATCHES_ESTIMATED, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated()));
				write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, first_chunk));
			}

			if (!buffer.empty()) {
				write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, (pretty ? indent_string(buffer, ' ', 3 * 4) : buffer) + "\n\n"));
			}

			write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, last_chunk));

			write(http_response(0, HTTP_BODY, 0, 0, 0, 0, "0\r\n\r\n"));
		}
	}

	operation_ends = std::chrono::system_clock::now();

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

	L_SEARCH(this, "FINISH SEARCH");
}


void
HttpClient::status_view(int status_code, const std::string& message)
{
	L_CALL(this, "HttpClient::status_view()");

	write_http_response(status_code, {
		{ RESPONSE_STATUS, status_code },
		{ RESPONSE_MESSAGE, message.empty() ? http_status[status_code / 100][status_code % 100] : message }
	});
}


HttpClient::Command
HttpClient::url_resolve()
{
	L_CALL(this, "HttpClient::url_resolve()");

	struct http_parser_url u;
	std::string b = repr(path, true, false);

	L_HTTP(this, "URL: %s", b.c_str());

	if (http_parser_parse_url(path.data(), path.size(), 0, &u) == 0) {
		L_HTTP_PROTO_PARSER(this, "HTTP parsing done!");

		if (u.field_set & (1 << UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			char path_buf_str[path_size + 1];
			const char* path_str = path.data() + u.field_data[3].off;
			normalize_path(path_str, path_str + path_size, path_buf_str);
			if (*path_buf_str != '/' || *(path_buf_str + 1) != '\0') {
				if (path_parser.init(path_buf_str) >= PathParser::State::END) {
					return Command::BAD_QUERY;
				}
			}
		}

		if (u.field_set & (1 <<  UF_QUERY)) {
			if (query_parser.init(std::string(b.data() + u.field_data[4].off, u.field_data[4].len)) < 0) {
				return Command::BAD_QUERY;
			}
		}

		if (query_parser.next("pretty") != -1) {
			pretty = true;
			if (query_parser.len) {
				try {
					pretty = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		if (!path_parser.off_cmd) {
			if (path_parser.off_id) {
				return Command::NO_CMD_ID;
			} else {
				return Command::NO_CMD_NO_ID;
			}
		} else {
			return static_cast<Command>(xxh64::hash(lower_string(path_parser.get_cmd())));
		}

	} else {
		L_HTTP_PROTO_PARSER(this, "Parsing not done");
		// Bad query
		return Command::BAD_QUERY;
	}
}


void
HttpClient::endpoints_maker(duration<double, std::milli> timeout)
{
	endpoints.clear();

	PathParser::State state;
	while ((state = path_parser.next()) < PathParser::State::END) {
		_endpoint_maker(timeout);
	}
}


void
HttpClient::_endpoint_maker(duration<double, std::milli> timeout)
{
	bool has_node_name = false;

	std::string ns;
	if (path_parser.off_nsp) {
		ns = path_parser.get_nsp() + "/";
		if (startswith(ns, "/")) { /* ns without slash */
			ns = ns.substr(1, std::string::npos);
		}
		if (startswith(ns, "_")) {
			throw MSG_ClientError("The index directory %s couldn't start with '_', it's reserved", ns.c_str());
		}
	}

	std::string path;
	if (path_parser.off_pth) {
		path = path_parser.get_pth();
		if (startswith(path, "/")) { /* path without slash */
			path = path.substr(1, std::string::npos);
		}
		if (startswith(path, "_")) {
			throw MSG_ClientError("The index directory %s couldn't start with '_', it's reserved", path.c_str());
		}
	}

	std::string index_path;
	if (ns.empty() && path.empty()) {
		index_path = ".";
	} else if (ns.empty()) {
		index_path = path;
	} else if (path.empty()) {
		index_path = ns;
	} else {
		index_path = ns + "/" + path;
	}
	index_paths.push_back(index_path);

	std::string node_name;
	std::vector<Endpoint> asked_nodes;
	if (path_parser.off_hst) {
		node_name = path_parser.get_hst();
		has_node_name = true;
	} else {
		auto local_node_ = local_node.load();
		size_t num_endps = 1;
		if (XapiandManager::manager->is_single_node()) {
			has_node_name = true;
			node_name = local_node_->name;
		} else {
			if (!XapiandManager::manager->resolve_index_endpoint(index_path, asked_nodes, num_endps, timeout)) {
				has_node_name = true;
				node_name = local_node_->name;
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
		auto node = XapiandManager::manager->touch_node(node_name, UNKNOWN_REGION);
		if (!node) {
			throw MSG_Error("Node %s not found", node_name.c_str());
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
	L_HTTP(this, "Endpoint: -> %s", endpoints.to_string().c_str());
}


void
HttpClient::query_field_maker(int flag)
{
	if (!query_field) query_field = std::make_unique<query_field_t>();

	if (flag & QUERY_FIELD_COMMIT) {
		if (query_parser.next("commit") != -1) {
			query_field->commit = true;
			if (query_parser.len) {
				try {
					query_field->commit = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();
	}

	if (flag & QUERY_FIELD_ID || flag & QUERY_FIELD_SEARCH) {
		if (query_parser.next("volatile") != -1) {
			query_field->volatile_ = true;
			if (query_parser.len) {
				try {
					query_field->volatile_ = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		if (query_parser.next("offset") != -1) {
			try {
				query_field->offset = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		if (query_parser.next("check_at_least") != -1) {
			try {
				query_field->check_at_least = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		if (query_parser.next("limit") != -1) {
			try {
				query_field->limit = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();
	}

	if (flag & QUERY_FIELD_SEARCH) {
		if (query_parser.next("spelling") != -1) {
			query_field->spelling = true;
			if (query_parser.len) {
				try {
					query_field->spelling = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		if (query_parser.next("synonyms") != -1) {
			query_field->synonyms = true;
			if (query_parser.len) {
				try {
					query_field->synonyms = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		while (query_parser.next("query") != -1) {
			L_SEARCH(this, "query=%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("q") != -1) {
			L_SEARCH(this, "query=%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("partial") != -1) {
			query_field->partial.push_back(query_parser.get());
		}
		query_parser.rewind();

		if (query_parser.next("sort") != -1) {
			query_field->sort.push_back(query_parser.get());
		} else {
			query_field->sort.push_back(ID_FIELD_NAME);
		}
		query_parser.rewind();

		if (query_parser.next("metric") != -1) {
			query_field->metric = query_parser.get();
		}
		query_parser.rewind();

		if (query_parser.next("icase") != -1) {
			query_field->icase = Serialise::boolean(query_parser.get()) == "t";
		}
		query_parser.rewind();

		if (query_parser.next("collapse_max") != -1) {
			try {
				query_field->collapse_max = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		if (query_parser.next("collapse") != -1) {
			query_field->collapse = query_parser.get();
		}
		query_parser.rewind();

		if (query_parser.next("fuzzy") != -1) {
			query_field->is_fuzzy = true;
			if (query_parser.len) {
				try {
					query_field->is_fuzzy = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		if (query_field->is_fuzzy) {
			if (query_parser.next("fuzzy.n_rset") != -1) {
				try {
					query_field->fuzzy.n_rset = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			if (query_parser.next("fuzzy.n_eset") != -1) {
				try {
					query_field->fuzzy.n_eset = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			if (query_parser.next("fuzzy.n_term") != -1) {
				try {
					query_field->fuzzy.n_term = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			while (query_parser.next("fuzzy.field") != -1) {
				query_field->fuzzy.field.push_back(query_parser.get());
			}
			query_parser.rewind();

			while (query_parser.next("fuzzy.type") != -1) {
				query_field->fuzzy.type.push_back(query_parser.get());
			}
			query_parser.rewind();
		}

		if (query_parser.next("nearest") != -1) {
			query_field->is_nearest = true;
			if (query_parser.len) {
				try {
					query_field->is_nearest = Serialise::boolean(query_parser.get()) == "t";
				} catch (const Exception&) { }
			}
		}
		query_parser.rewind();

		if (query_field->is_nearest) {
			query_field->nearest.n_rset = 5;
			if (query_parser.next("nearest.n_rset") != -1) {
				try {
					query_field->nearest.n_rset = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			if (query_parser.next("nearest.n_eset") != -1) {
				try {
					query_field->nearest.n_eset = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			if (query_parser.next("nearest.n_term") != -1) {
				try {
					query_field->nearest.n_term = static_cast<unsigned>(std::stoul(query_parser.get()));
				} catch (const std::invalid_argument&) { }
			}
			query_parser.rewind();

			while (query_parser.next("nearest.field") != -1) {
				query_field->nearest.field.push_back(query_parser.get());
			}
			query_parser.rewind();

			while (query_parser.next("nearest.type") != -1) {
				query_field->nearest.type.push_back(query_parser.get());
			}
			query_parser.rewind();
		}
	}

	if (flag & QUERY_FIELD_TIME) {
		if (query_parser.next("time") != -1) {
			query_field->time = query_parser.get();
		} else {
			query_field->time = "1h";
		}
		query_parser.rewind();
	}
}


void
HttpClient::clean_http_request()
{
	L_CALL(this, "HttpClient::clean_http_request()");

	response_ends = std::chrono::system_clock::now();
	auto request_delta = delta_string(request_begins, response_ends);
	auto response_delta = delta_string(response_begins, response_ends);

	response_log.load()->LOG_DELAYED_CLEAR();
	if (parser.http_errno) {
		if (!response_logged.exchange(true)) LOG(LOG_ERR, LIGHT_RED, this, "HTTP parsing error (%s): %s", http_errno_name(HTTP_PARSER_ERRNO(&parser)), http_errno_description(HTTP_PARSER_ERRNO(&parser)));
	} else {
		int priority = LOG_DEBUG;
		const char* color = WHITE;
		if (response_status >= 200 && response_status <= 299) {
			color = GREY;
		} else if (response_status >= 300 && response_status <= 399) {
			color = CYAN;
		} else if (response_status >= 400 && response_status <= 499) {
			color = YELLOW;
			priority = LOG_INFO;
		} else if (response_status >= 500 && response_status <= 599) {
			color = LIGHT_MAGENTA;
			priority = LOG_ERR;
		}
		if (!response_logged.exchange(true)) LOG(priority, color, this, "\"%s %s HTTP/%d.%d\" %d %s %s", http_method_str(HTTP_PARSER_METHOD(&parser)), path.c_str(), parser.http_major, parser.http_minor, response_status, bytes_string(response_size).c_str(), request_delta.c_str());
	}

	path.clear();
	body.clear();
	header_name.clear();
	header_value.clear();
	content_type.clear();
	content_length.clear();
	host.clear();
	response_status = 0;
	response_size = 0;

	pretty = false;
	query_field.reset();
	path_parser.clear();
	query_parser.clear();
	accept_set.clear();

	request_begining = true;
	L_TIME(this, "Full request took %s, response took %s", request_delta.c_str(), response_delta.c_str());
	idle = true;

	async_read.send();
	http_parser_init(&parser, HTTP_REQUEST);
}


const type_t*
HttpClient::is_acceptable_type(const type_t& ct_type_pattern, const type_t& ct_type)
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


const type_t*
HttpClient::is_acceptable_type(const type_t& ct_type_pattern, const std::vector<type_t>& ct_types)
{
	L_CALL(this, "HttpClient::is_acceptable_type(...)");

	for (auto& ct_type : ct_types) {
		if (is_acceptable_type(ct_type_pattern, ct_type)) {
			return &ct_type;
		}
	}
	return nullptr;
}


template <typename T>
const type_t&
HttpClient::get_acceptable_type(const T& ct)
{
	L_CALL(this, "HttpClient::get_acceptable_type()");

	if (accept_set.empty()) {
		if (!content_type.empty()) accept_set.insert(std::tuple<double, int, type_t>(1, 0, content_type_pair(content_type)));
		accept_set.insert(std::make_tuple(1, 1, std::make_pair(std::string("*"), std::string("*"))));
	}
	for (const auto& accept : accept_set) {
		if (is_acceptable_type(std::get<2>(accept), ct)) {
			return std::get<2>(accept);
		}
	}
	return std::get<2>(*accept_set.begin());
}


type_t
HttpClient::serialize_response(const MsgPack& obj, const type_t& ct_type, bool pretty, bool serialize_error)
{
	L_CALL(this, "HttpClient::serialize_response()");

	if (is_acceptable_type(ct_type, json_type)) {
		return std::make_pair(obj.to_string(pretty), json_type.first + "/" + json_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, msgpack_type)) {
		return std::make_pair(obj.to_string(), msgpack_type.first + "/" + msgpack_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, html_type)) {
		std::function<std::string(const msgpack::object&)> html_serialize = serialize_error ? msgpack_to_html_error : msgpack_to_html;
		return std::make_pair(obj.external(html_serialize), html_type.first + "/" + html_type.second + "; charset=utf-8");
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
HttpClient::write_http_response(int status_code, const MsgPack& response)
{
	L_CALL(this, "HttpClient::write_http_response()");

	if (response.is_undefined()) {
		write(http_response(status_code, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	auto ct_type = content_type_pair(content_type);
	std::vector<type_t> ct_types;
	if (ct_type == json_type || ct_type == msgpack_type || content_type.empty()) {
		ct_types = msgpack_serializers;
	} else {
		ct_types.push_back(ct_type);
	}
	const auto& accepted_type = get_acceptable_type(ct_types);
	try {
		auto result = serialize_response(response, accepted_type, pretty, status_code >= 400);
		write(http_response(status_code, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second));
	} catch (const SerialisationError& exc) {
		status_code = 406;
		MsgPack response_err = {
			{ RESPONSE_STATUS, status_code },
			{ RESPONSE_MESSAGE, std::string("Response type " + accepted_type.first + "/" + accepted_type.second + " " + exc.what()) }
		};
		auto response_str = response_err.to_string();
		write(http_response(status_code, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor, 0, 0, response_str));
		return;
	}
}
