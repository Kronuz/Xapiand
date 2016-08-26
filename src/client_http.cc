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

#include "io_utils.h"
#include "length.h"
#include "multivalue/matchspy.h"
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

#define DEF_36e6 2176782336

// Xapian http client
#define METHOD_DELETE  0
#define METHOD_HEAD    2
#define METHOD_GET     1
#define METHOD_POST    3
#define METHOD_PUT     4
#define METHOD_OPTIONS 6
#define METHOD_PATCH   24

#define QUERY_FIELD_COMMIT (1 << 0)
#define QUERY_FIELD_SEARCH (1 << 1)
#define QUERY_FIELD_RANGE  (1 << 2)
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

		if (mode & HTTP_OPTIONS) {
			response += "Allow: GET,HEAD,POST,PUT,PATCH,OPTIONS" + eol;
		}

		if (mode & HTTP_TOTAL_COUNT) {
			response += "Total-Count: " + std::to_string(total_count) + eol;
		}

		if (mode & HTTP_MATCHES_ESTIMATED) {
			response += "Matches-Estimated: " + std::to_string(matches_estimated) + eol;
		}

		if (mode & HTTP_CONTENT_TYPE) {
			response += "Content-Type: " + ct_type + eol;
		}

		if (mode & HTTP_CONTENT_ENCODING) {
			response += "Content-Encoding: " + ct_encoding + eol;
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


HttpClient::HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_)
	: BaseClient(std::move(server_), ev_loop_, ev_flags_, sock_),
	  pretty(false),
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
		write_http_response(err_response, error_code, false);
		L_HTTP_PROTO(this, HTTP_PARSER_ERRNO(&parser) != HPE_OK ? message : "incomplete request");
		destroy();  // Handle error. Just close the connection.
		detach();
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
	L_CALL(this, "HttpClient::run()");

	L_OBJ_BEGIN(this, "HttpClient::run:BEGIN");
	response_begins = std::chrono::system_clock::now();

	std::string error;
	int error_code = 0;

	try {
		if (path == "/quit") {
			time_t now = epoch::now<>();
			XapiandManager::manager->shutdown_asap.store(now);
			XapiandManager::manager->shutdown_sig(0);
			L_OBJ_END(this, "HttpClient::run:END");
			return;
		}

		int cmd = url_resolve();

		switch (parser.method) {
			case METHOD_DELETE:
				_delete(cmd);
				break;
			case METHOD_GET:
				_get(cmd);
				break;
			case METHOD_POST:
				_post(cmd);
				break;
			case METHOD_HEAD:
				_head(cmd);
				break;
			case METHOD_PUT:
				_put(cmd);
				break;
			case METHOD_OPTIONS:
				_options(cmd);
				break;
			case METHOD_PATCH:
				_patch(cmd);
			default:
				write(http_response(501, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
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
		if (db_handler.get_database()) {
			db_handler.checkin();
		}
		if (written) {
			destroy();
			detach();
		} else {
			MsgPack err_response = {
				{ RESPONSE_STATUS, error_code },
				{ RESPONSE_MESSAGE, error }
			};

			write_http_response(err_response, error_code, pretty);
		}
	}

	L_OBJ_END(this, "HttpClient::run:END");
}


void
HttpClient::_options(int cmd)
{
	L_CALL(this, "HttpClient::_options(%d)", cmd); (void)cmd;

	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_OPTIONS, parser.http_major, parser.http_minor));
}


void
HttpClient::_head(int cmd)
{
	L_CALL(this, "HttpClient::_head(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			if (path_parser.off_id) {
				document_info_view();
			} else {
				bad_request_view();
			}
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::_get(int cmd)
{
	L_CALL(this, "HttpClient::_get(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			if (path_parser.off_id) {
				search_view();
			} else {
				home_view();
			}
			break;
		case CMD_SEARCH:
			path_parser.off_id = nullptr;
			search_view();
			break;
		case CMD_FACETS:
			facets_view();
			break;
		case CMD_SCHEMA:
			schema_view();
			break;
		case CMD_STATS:
			stats_view();
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::_put(int cmd)
{
	L_CALL(this, "HttpClient::_put(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			if (path_parser.off_id) {
				index_document_view(false);
			} else {
				bad_request_view();
			}
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::_post(int cmd)
{
	L_CALL(this, "HttpClient::_post(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			index_document_view(true);
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::_patch(int cmd)
{
	L_CALL(this, "HttpClient::_patch(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			if (path_parser.off_id) {
				update_document_view();
			} else {
				bad_request_view();
			}
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::_delete(int cmd)
{
	L_CALL(this, "HttpClient::_delete(%d)", cmd);

	switch (cmd) {
		case CMD_NO_CMD:
			if (path_parser.off_id) {
				delete_document_view();
			} else {
				bad_request_view();
			}
			break;
		default:
			bad_request_view();
			break;
	}
}


void
HttpClient::home_view()
{
	L_CALL(this, "HttpClient::home_view()");

	endpoints.clear();
	endpoints.add(Endpoint("."));

	db_handler.reset(endpoints, DB_SPAWN);

	auto local_node_ = local_node.load();
	auto document = db_handler.get_document(std::to_string(local_node_->id));

	auto obj_data = get_MsgPack(document);
	try {
		obj_data = obj_data.at(RESERVED_DATA);
	} catch (const std::out_of_range&) {
		obj_data[RESERVED_ID] = db_handler.get_value(document, RESERVED_ID);
	}

#ifdef XAPIAND_CLUSTERING
	obj_data["_cluster_name"] = XapiandManager::manager->cluster_name;
#endif
	obj_data["_version"] = {
		{ "_mastery", PACKAGE_VERSION },
		{ "_number", PACKAGE_VERSION  },
		{ "_xapian", Xapian::version_string() }
	};

	write_http_response(obj_data, 200, pretty);
}


void
HttpClient::document_info_view()
{
	L_CALL(this, "HttpClient::document_info_view()");

	endpoints_maker(1s);

	db_handler.reset(endpoints, DB_SPAWN);

	MsgPack response;
	response["doc_id"] = db_handler.get_docid(path_parser.get_id());

	write_http_response(response, 200, pretty);
}


void
HttpClient::delete_document_view()
{
	L_CALL(this, "HttpClient::delete_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	std::string doc_id(path_parser.get_id());

	int status_code;
	MsgPack response;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN);

	if (endpoints.size() == 1) {
		operation_begins = std::chrono::system_clock::now();
		db_handler.delete_document(doc_id, query_field->commit);
		operation_ends = std::chrono::system_clock::now();
		status_code = 200;

		response["_delete"] = {
			{ RESERVED_ID, doc_id },
			{ "_commit",  query_field->commit }
		};
	} else {
		operation_begins = std::chrono::system_clock::now();
		endpoints_error_list err_list = db_handler.multi_db_delete_document(doc_id, query_field->commit);
		operation_ends = std::chrono::system_clock::now();

		if (err_list.empty()) {
			status_code = 200;
			response["_delete"] = {
				{ RESERVED_ID, doc_id },
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


	write_http_response(response, status_code, pretty);
}


void
HttpClient::index_document_view(bool gen_id)
{
	L_CALL(this, "HttpClient::index_document_view()");

	std::string doc_id;
	int status_code;

	if (gen_id) {
		path_parser.off_id = nullptr;
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

	if (content_type.empty()) {
		content_type = JSON_CONTENT_TYPE;
	}

	endpoints_error_list err_list;
	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF);
	db_handler.index(body, doc_id, query_field->commit, content_type, content_length, &err_list);

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
			{ RESERVED_ID, doc_id },
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

	write_http_response(response, status_code, pretty);
}


void
HttpClient::update_document_view()
{
	L_CALL(this, "HttpClient::update_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	operation_begins = std::chrono::system_clock::now();

	std::string doc_id(path_parser.get_id());

	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN);
	db_handler.patch(body, doc_id, query_field->commit, content_type, content_length);

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
	response["_update"] = {
		{ RESERVED_ID, doc_id },
		{ "_commit", query_field->commit }
	};

	write_http_response(response, 200, pretty);
}


void
HttpClient::stats_view()
{
	L_CALL(this, "HttpClient::stats_view()");

	MsgPack response;
	bool res_stats = false;

	if (!path_parser.off_id) {
		query_field_maker(QUERY_FIELD_TIME);
		XapiandManager::manager->server_status(response["_server_status"]);
		XapiandManager::manager->get_stats_time(response["_stats_time"], query_field->time);
		res_stats = true;
	} else {
		endpoints_maker(1s);

		db_handler.reset(endpoints, DB_OPEN);
		try {
			db_handler.get_stats_doc(response["_document_status"], path_parser.get_id());
		} catch (const CheckoutError&) {
			path_parser.off_id = nullptr;
			response.erase("_document_status");
		}

		path_parser.rewind();
		endpoints_maker(1s);

		db_handler.reset(endpoints, DB_OPEN);
		db_handler.get_stats_database(response["_database_status"]);
		res_stats = true;
	}

	int response_status = 200;
	if (res_stats) {
		write_http_response(response, response_status, pretty);
	} else {
		response_status = 404;
		response[RESPONSE_STATUS] = response_status;
		response[RESPONSE_MESSAGE] = "Not found";
		write_http_response(response, 404, pretty);
	}
}


void
HttpClient::schema_view()
{
	L_CALL(this, "HttpClient::schema_view()");

	path_parser.off_id = nullptr;
	endpoints_maker(1s);

	db_handler.reset(endpoints, DB_SPAWN);
	write_http_response(db_handler.get_schema()->get_readable(), 200, pretty);
	return;
}


void
HttpClient::facets_view()
{
	L_CALL(this, "HttpClient::facets_view()");

	path_parser.off_id = nullptr;
	endpoints_maker(1s);
	query_field_maker(QUERY_FIELD_SEARCH | QUERY_FIELD_RANGE);

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	SpiesVector spies;

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_SPAWN);
	db_handler.get_mset(*query_field, mset, spies, suggestions);

	L_DEBUG(this, "Suggested queries:\n%s", [&suggestions]() {
		std::string res;
		for (const auto& suggestion : suggestions) {
			res += "\t+ " + suggestion + "\n";
		}
		return res;
	}().c_str());

	MsgPack response;
	if (spies.empty()) {
		response[RESPONSE_MESSAGE] = "Not found documents tallied";
	} else {
		for (const auto& spy : spies) {
			auto name_result = spy.first;
			MsgPack array;
			const auto facet_e = spy.second->values_end();
			for (auto facet = spy.second->values_begin(); facet != facet_e; ++facet) {
				auto field_t = db_handler.get_schema()->get_slot_field(spy.first);
				MsgPack value = {
					{ RESERVED_VALUE, Unserialise::MsgPack(field_t.type, *facet) },
					{ "_termfreq", facet.get_termfreq() }
				};
				array.push_back(std::move(value));
			}
			response[name_result] = std::move(array);
		}
	}
	operation_ends = std::chrono::system_clock::now();
	write_http_response(response, 200, pretty);

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

	L_DEBUG(this, "FINISH SEARCH");
}


void
HttpClient::search_view()
{
	L_CALL(this, "HttpClient::search_view()");

	bool chunked = !path_parser.off_id || isRange(path_parser.get_id());

	int query_field_flags = 0;

	if (!path_parser.off_id) {
		query_field_flags |= QUERY_FIELD_SEARCH;
	}

	if (chunked) {
		query_field_flags |= QUERY_FIELD_RANGE;
	}

	endpoints_maker(1s);
	query_field_maker(query_field_flags);

	if (path_parser.off_id) {
		query_field->query.push_back(std::string(RESERVED_ID)  + ":" +  path_parser.get_id());
	}

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	SpiesVector spies;

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_OPEN);
	try {
		db_handler.get_mset(*query_field, mset, spies, suggestions);
	} catch (CheckoutError) {
		/* At the moment when the endpoint it not exist and it is chunck it will return 200 response
		 * and zero matches this behavior may change in the future for instance ( return 404 )
		 * if is not chunk return 404
		 */
	}

	L_SEARCH(this, "Suggested queries:\n%s", [&suggestions]() {
		std::string res;
		for (const auto& suggestion : suggestions) {
			res += "\t+ " + suggestion + "\n";
		}
		return res;
	}().c_str());

	int rc = 0;
	if (!chunked && mset.empty()) {
		int error_code = 404;
		MsgPack err_response = {
			{ RESPONSE_STATUS, error_code },
			{ RESPONSE_MESSAGE, "No document found" }
		};
		write_http_response(err_response, error_code, pretty);
	} else {
		for (auto m = mset.begin(); m != mset.end(); ++rc, ++m) {
			auto document = db_handler.get_document(*m);
			operation_ends = std::chrono::system_clock::now();

			auto ct_type_str = document.get_value(DB_SLOT_TYPE);
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
				write_http_response(err_response, error_code, pretty);
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
				write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_BODY, parser.http_major, parser.http_minor, 0, 0, blob_data, ct_type_str));
				return;
			}

			ct_type = *accepted_ct_type;
			ct_type_str = ct_type.first + "/" + ct_type.second;

			try {
				obj_data = obj_data.at(RESERVED_DATA);
			} catch (const std::out_of_range&) {
				obj_data[RESERVED_ID] = db_handler.get_value(document, RESERVED_ID);
				// Detailed info about the document:
				obj_data[RESERVED_RANK] = m.get_rank();
				obj_data[RESERVED_WEIGHT] = m.get_weight();
				obj_data[RESERVED_PERCENT] = m.get_percent();
				int subdatabase = (document.get_docid() - 1) % endpoints.size();
				auto endpoint = endpoints[subdatabase];
				obj_data[RESERVED_ENDPOINT] = endpoint.as_string();
			}

			auto result = serialize_response(obj_data, ct_type, pretty);
			if (chunked) {
				if (rc == 0) {
					write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_CHUNKED | HTTP_TOTAL_COUNT | HTTP_MATCHES_ESTIMATED, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type_str));
				}
				if (!write(http_response(200, HTTP_CHUNKED | HTTP_BODY, 0, 0, 0, 0, result.first + "\n\n"))) {
					break;
				}
			} else if (!write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second))) {
				break;
			}
		}

		if (chunked) {
			if (rc == 0) {
				write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CHUNKED | HTTP_TOTAL_COUNT | HTTP_MATCHES_ESTIMATED, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated()));
			}
			write(http_response(0, HTTP_BODY, 0, 0, 0, 0, "0\r\n\r\n"));
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

	L_DEBUG(this, "FINISH SEARCH");
}


void
HttpClient::bad_request_view()
{
	L_CALL(this, "HttpClient::bad_request_view()");

	/*
	 REMOVE THIS
	 switch (cmd) {
		case CMD_UNKNOWN_HOST:
			err_response[RESPONSE_MESSAGE] = "Unknown host " + host;
			break;
		default:
			err_response[RESPONSE_MESSAGE] = "BAD QUERY";
	}*/

	MsgPack err_response = {
		{ RESPONSE_STATUS, 400 },
		{ RESPONSE_MESSAGE, "BAD QUERY" }
	};

	write_http_response(err_response, 400, pretty);
}


constexpr size_t const_hash(char const *input) {
	return *input ? static_cast<size_t>(*input) + 33 * const_hash(input + 1) : 5381;
}


static constexpr auto http_search = const_hash("_search");
static constexpr auto http_facets = const_hash("_facets");
static constexpr auto http_stats = const_hash("_stats");
static constexpr auto http_schema = const_hash("_schema");


int
HttpClient::identify_cmd()
{
	if (!path_parser.off_cmd) {
		return CMD_NO_CMD;
	} else {
		switch (const_hash(lower_string(path_parser.get_cmd()).c_str())) {
			case http_search:
				return CMD_SEARCH;
				break;
			case http_facets:
				return CMD_FACETS;
				break;
			case http_stats:
				return CMD_STATS;
				break;
			case http_schema:
				return CMD_SCHEMA;
				break;
			default:
				return CMD_UNKNOWN;
				break;
		}
	}
}


int
HttpClient::url_resolve()
{
	L_CALL(this, "HttpClient::url_resolve()");

	struct http_parser_url u;
	std::string b = repr(path);

	L_HTTP_PROTO_PARSER(this, "URL: %s", b.c_str());
	if (http_parser_parse_url(path.c_str(), path.size(), 0, &u) == 0) {
		L_HTTP_PROTO_PARSER(this, "HTTP parsing done!");

		if (u.field_set & (1 << UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			char path_buf_str[path_size + 1];
			const char* path_str = path.data() + u.field_data[3].off;
			normalize_path(path_str, path_str + path_size, path_buf_str);
			if (*path_buf_str != '/' || *(path_buf_str + 1) != '\0') {
				if (path_parser.init(path_buf_str) >= PathParser::end) {
					return CMD_BAD_QUERY;
				}
			}
		}

		if (u.field_set & (1 <<  UF_QUERY)) {
			if (query_parser.init(std::string(b.data() + u.field_data[4].off, u.field_data[4].len)) < 0) {
				return CMD_BAD_QUERY;
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

		return identify_cmd();
	} else {
		L_CONN_WIRE(this, "Parsing not done");
		// Bad query
		return CMD_BAD_QUERY;
	}
}


void
HttpClient::endpoints_maker(duration<double, std::milli> timeout)
{
	endpoints.clear();

	PathParser::State state;
	while ((state = path_parser.next()) < PathParser::end) {
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
		if (startswith(ns, "_") or startswith(ns, ".")) {
			throw MSG_ClientError("The index directory %s couldn't start with '_' or '.' are reserved", ns.c_str());
		}
	}

	std::string path;
	if (path_parser.off_pth) {
		path = path_parser.get_pth();
		if (startswith(path, "/")) { /* path without slash */
			path = path.substr(1, std::string::npos);
		}
		if (startswith(path, "_") or startswith(path, ".")) {
			throw MSG_ClientError("The index directory %s couldn't start with '_' or '.' are reserved", path.c_str());
		}
	}

	std::string index_path;
	if (!ns.empty()) {
		index_path = ns + "/" + path;
	} else {
		index_path = path;
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
	L_CONN_WIRE(this, "Endpoint: -> %s", endpoints.as_string().c_str());
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
			L_DEBUG(this, "%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("q") != -1) {
			L_DEBUG(this, "%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("partial") != -1) {
			query_field->partial.push_back(query_parser.get());
		}
		query_parser.rewind();
	}

	if (flag & QUERY_FIELD_RANGE) {
		if (!query_field) query_field = std::make_unique<query_field_t>();

		query_field->offset = 0;
		if (query_parser.next("offset") != -1) {
			try {
				query_field->offset = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		query_field->check_at_least = 0;
		if (query_parser.next("check_at_least") != -1) {
			try {
				query_field->check_at_least = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		query_field->limit = 10;
		if (query_parser.next("limit") != -1) {
			try {
				query_field->limit = static_cast<unsigned>(std::stoul(query_parser.get()));
			} catch (const std::invalid_argument&) { }
		}
		query_parser.rewind();

		if (query_parser.next("sort") != -1) {
			query_field->sort.push_back(query_parser.get());
		} else {
			query_field->sort.push_back(RESERVED_ID);
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

		query_field->collapse_max = 1;
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

		while (query_parser.next("facets") != -1) {
			query_field->facets.push_back(query_parser.get());
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

	path.clear();
	body.clear();
	header_name.clear();
	header_value.clear();
	content_type.clear();
	content_length.clear();
	host.clear();

	pretty = false;
	query_field.reset();
	path_parser.clear();
	query_parser.clear();
	accept_set.clear();

	response_ends = std::chrono::system_clock::now();
	request_begining = true;
	L_TIME(this, "Full request took %s, response took %s", delta_string(request_begins, response_ends).c_str(), delta_string(response_begins, response_ends).c_str());

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
HttpClient::write_http_response(const MsgPack& response,  int status_code, bool pretty)
{
	L_CALL(this, "HttpClient::write_http_response()");

	auto ct_type = content_type_pair(content_type);
	std::vector<type_t> ct_types;
	if (ct_type == json_type || ct_type == msgpack_type || content_type.empty()) {
		ct_types = msgpack_serializers;
	} else {
		ct_types.push_back(ct_type);
	}

	const auto& accepted_type = get_acceptable_type(ct_types);
	try {
		auto result = serialize_response(response, accepted_type, pretty,  status_code >= 400);
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
