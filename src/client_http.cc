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
#include "cJSON.h"
#include "io_utils.h"

#include <unistd.h>
#include <regex>

#include <cassert>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_BODY_SIZE (250 * 1024 * 1024)
#define MAX_BODY_MEM (5 * 1024 * 1024)

// Xapian http client
#define METHOD_DELETE  0
#define METHOD_HEAD    2
#define METHOD_GET     1
#define METHOD_POST    3
#define METHOD_PUT     4
#define METHOD_OPTIONS 6
#define METHOD_PATCH   24

std::regex header_accept_re("([a-z*+]*/[a-z*+]*)(?:;?(q=(?:\\d*\\.)?\\d+)?),?");

const char* status_code[6][14] = {
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
		NULL,                       // 401
		NULL,                       // 402
		NULL,                       // 403
		"Not Found",                // 404
		NULL,                       // 405
		"Not Acceptable",           // 406
		NULL,                       // 407
		NULL,                       // 408
		NULL,                       // 409
		NULL,                       // 410
		NULL,                       // 411
		NULL,                       // 412
		"Request Entity Too Large"  // 413
	},
	{
		"Internal Server Error",    // 500
		"Not Implemented",          // 501
		"Bad Gateway"               // 502
	}
};


std::string http_response(int status, int mode, unsigned short http_major=0, unsigned short http_minor=9, int matched_count=0, std::string body=std::string(""), std::string ct_type=std::string("application/json; charset=UTF-8"))
{
	char buffer[20];
	std::string response;
	std::string eol("\r\n");


	if (mode & HTTP_STATUS) {
		snprintf(buffer, sizeof(buffer), "HTTP/%d.%d %d ", http_major, http_minor, status);
		response += buffer;
		response += status_code[status / 100][status % 100] + eol;
		if (!(mode & HTTP_HEADER)) {
			response += eol;
		}
	}

	if (mode & HTTP_HEADER) {
		if (mode & HTTP_CONTENT_TYPE) {
			response += "Content-Type: "+ ct_type + eol;
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

	return response;
}


HttpClient::HttpClient(std::shared_ptr<XapiandServer> server_, ev::loop_ref *loop_, int sock_)
	: BaseClient(std::move(server_), loop_, sock_),
	  database(nullptr),
	  body_size(0),
	  body_descriptor(0),
	  body_path("")
{
	parser.data = this;
	http_parser_init(&parser, HTTP_REQUEST);

	int http_clients = ++XapiandServer::http_clients;
	int total_clients = XapiandServer::total_clients;
	assert(http_clients <= total_clients);

	LOG_CONN(this, "New Http Client (sock=%d), %d client(s) of a total of %d connected.\n", sock, http_clients, total_clients);

	LOG_OBJ(this, "CREATED HTTP CLIENT! (%d clients)\n", http_clients);
}


HttpClient::~HttpClient()
{
	int http_clients = --XapiandServer::http_clients;

	time_t shutdown_asap = manager()->shutdown_asap;

	if (shutdown_asap) {
		if (http_clients <= 0) {
			manager()->async_shutdown.send();
		}
	}

	if (body_descriptor && ::close(body_descriptor) < 0) {
		LOG_ERR(this, "ERROR: Cannot close temporary file '%s': %s\n", body_path, strerror(errno));
	}

	if (*body_path) {
		if (::unlink(body_path) < 0) {
			LOG_ERR(this, "ERROR: Cannot delete temporary file '%s': %s\n", body_path, strerror(errno));
		}
	}

	LOG_OBJ(this, "DELETED HTTP CLIENT! (%d clients left)\n", http_clients);
	assert(http_clients >= 0);
}


void HttpClient::on_read(const char *buf, size_t received)
{
	LOG_CONN_WIRE(this, "BinaryClient::on_read: %zu bytes\n", received);
	size_t parsed = http_parser_execute(&parser, &settings, buf, received);
	if (parsed == received) {
		if (parser.state == 1 || parser.state == 18) { // dead or message_complete
			io_read.stop();
			written = 0;
			manager()->thread_pool.enqueue(share_this<HttpClient>());
		}
	} else {
		LOG_HTTP_PROTO(this, HTTP_PARSER_ERRNO(&parser) != HPE_OK ? http_errno_description(HTTP_PARSER_ERRNO(&parser)) : "incomplete request");
		destroy();  // Handle error. Just close the connection.
	}
}


void HttpClient::on_read_file(const char *, size_t received)
{
	LOG_ERR(this, "Not Implemented: HttpClient::on_read_file: %zu bytes\n", received);
}


void HttpClient::on_read_file_done()
{
	LOG_ERR(this, "Not Implemented: HttpClient::on_read_file_done\n");
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


int HttpClient::on_info(http_parser* p) {
	HttpClient *self = static_cast<HttpClient *>(p->data);

	LOG_HTTP_PROTO_PARSER(self, "%3d. (INFO)\n", p->state);

	switch (p->state) {
		case 18:  // message_complete
			break;
		case 19:  // message_begin
			self->path.clear();
			self->body.clear();
			self->body_size = 0;
			self->header_name.clear();
			self->header_value.clear();
			if (self->body_descriptor && ::close(self->body_descriptor) < 0) {
				LOG_ERR(self, "ERROR: Cannot close temporary file '%s': %s\n", self->body_path, strerror(errno));
			} else {
				self->body_descriptor = 0;
			}
			break;
		case 50:  // headers done
			if (self->expect_100) {
				// Return 100 if client is expecting it
				self->write(http_response(100, HTTP_STATUS, p->http_major, p->http_minor));
			}
	}

	return 0;
}


int HttpClient::on_data(http_parser* p, const char *at, size_t length) {
	HttpClient *self = static_cast<HttpClient *>(p->data);

	LOG_HTTP_PROTO_PARSER(self, "%3d. %s\n", p->state, repr(at, length).c_str());

	int state = p->state;

	// s_req_path  ->  s_req_http_start
	if (state > 26 && state <= 32) {
		self->path.append(at, length);
	} else

	// s_header_field  ->  s_header_value_discard_ws
	if (state >= 43 && state <= 44) {
		self->header_name.append(at, length);
	} else

	// s_header_value_discard_ws_almost_done  ->  s_header_almost_done
	if (state >= 45 && state <= 50) {
		self->header_value.append(at, length);
		if (state == 50) {
			std::string name(self->header_name);
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);

			std::string value(self->header_value);
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);

			if (name.compare("host") == 0) {
				self->host = self->header_value;
			} else

			if (name.compare("expect") == 0 && value.compare("100-continue") == 0) {
				if (p->content_length > MAX_BODY_SIZE) {
					self->write(http_response(413, HTTP_STATUS, p->http_major, p->http_minor));
					self->close();
					return 0;
				}
				// Respond with HTTP/1.1 100 Continue
				self->expect_100 = true;
			} else

			if (name.compare("content-type") == 0) {
				self->content_type = value;
			} else

			if (name.compare("content-length") == 0) {
				self->content_length = value;
			} else

			if (name.compare("accept") == 0) {
				int submatches[] = {1, 2};
				std::regex_token_iterator<std::string::iterator> beg_re (value.begin(), value.end(), header_accept_re, submatches);
				std::regex_token_iterator<std::string::iterator> end_re;
				if (std::distance(beg_re, end_re)) {
					while (beg_re != end_re) {
						//Get the accept type part
						std::string type(*beg_re++);
						if(type.length() != 0) {
							if(beg_re != end_re) {
								//Get the accept prefereces
								std::string num(*beg_re++);
								if (num.length() != 0) self->accept_set.insert(std::make_pair(std::stod(std::string(num, 2)), type));
								else self->accept_set.insert(std::make_pair(1, type));
							}
						} else beg_re++;
					}
				}
			}
			self->header_name.clear();
			self->header_value.clear();
		}
	} else

	// s_body_identity  ->  s_message_done
	if (state >= 60 && state <= 62) {
		self->body_size += length;
		if (self->body_size > MAX_BODY_SIZE || p->content_length > MAX_BODY_SIZE) {
			self->write(http_response(413, HTTP_STATUS, p->http_major, p->http_minor));
			self->close();
			return 0;
		} else if (self->body_descriptor || self->body_size > MAX_BODY_MEM) {

			//The next two lines are switching off the write body in to a file option when the body is too large
			//(for avoid have it in memory) but this feature is not available yet
			self->write(http_response(413, HTTP_STATUS, p->http_major, p->http_minor)); // <-- remove leater!
			self->close(); // <-- remove leater!

			if (!self->body_descriptor) {
				strcpy(self->body_path, "/tmp/xapiand_upload.XXXXXX");
				self->body_descriptor = mkstemp(self->body_path);
				if (self->body_descriptor < 0) {
					LOG_ERR(self, "Cannot write to %s (1)\n", self->body_path);
					return 0;
				}
				::io_write(self->body_descriptor, self->body.data(), self->body.size());
				self->body.clear();
			}
			::io_write(self->body_descriptor, at, length);
			if (state == 62) {
				if (self->body_descriptor && ::close(self->body_descriptor) < 0) {
					LOG_ERR(self, "ERROR: Cannot close temporary file '%s': %s\n", self->body_path, strerror(errno));
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


void HttpClient::run()
{
	std::string error;
	const char *error_str;
	bool has_error = false;

	try {
		if (path == "/quit") {
			manager()->async_shutdown.send();
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
	} catch (const Xapian::Error &err) {
		has_error = true;
		error_str = err.get_error_string();
		if (error_str) {
			error.assign(error_str);
		} else {
			error.assign("Unkown Xapian error!");
		}
	} catch (const std::exception &err) {
		has_error = true;
		error_str = err.what();
		if (error_str) {
			error.assign(error_str);
		} else {
			error.assign("Unkown exception!");
		}
	} catch (...) {
		has_error = true;
		error.assign("Unkown error!");
	}
	if (has_error) {
		LOG_ERR(this, "ERROR: %s\n", error.c_str());
		if (database) {
			manager()->database_pool.checkin(database);
		}
		if (written) {
			destroy();
		} else {
			write(http_response(500, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		}
	}
	io_read.start();
}


void HttpClient::_options()
{
	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_OPTIONS, parser.http_major, parser.http_minor));
}


void HttpClient::_head()
{
	query_field e;
	int cmd = _endpointgen(e, false);

	switch (cmd) {
		case CMD_ID:
			document_info_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::_get()
{
	query_field e;
	int cmd = _endpointgen(e, false);

	switch (cmd) {
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
			stats_view(e);
			break;
		case CMD_SCHEMA:
			search_view(e, false, true);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::_put()
{
	query_field e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID:
			index_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::_post()
{
	query_field e;
	int cmd = _endpointgen(e, false);

	switch (cmd) {
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
			stats_view(e);
			break;
		case CMD_SCHEMA:
			search_view(e, false, true);
			break;
		case CMD_UPLOAD:
			upload_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::_patch()
{
	query_field e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID:
			update_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::_delete()
{
	query_field e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID:
			delete_document_view(e);
			break;
		default:
			bad_request_view(e, cmd);
			break;
	}
}


void HttpClient::document_info_view(const query_field &e)
{
	bool found = true;
	std::string result;
	Xapian::docid docid = 0;
	Xapian::QueryParser queryparser;

	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(command.at(0))) {
		prefix += ":";
	}
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	Xapian::Query query = queryparser.parse_query(std::string(std::string(RESERVED_ID) + ":" + command));
	Xapian::Enquire enquire(*database->db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	if (mset.size()) {
		Xapian::MSetIterator m = mset.begin();
		int t = 3;
		for ( ; t >= 0; --t) {
			try {
				docid = *m;
				break;
			} catch (const Xapian::Error &err) {
				database->reopen();
				m = mset.begin();
			}
		}
	} else {
		found = false;
	}

	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	if (found) {
		cJSON_AddNumberToObject(root.get(), RESERVED_ID, docid);
		if (e.pretty) {
			unique_char_ptr _cprint(cJSON_Print(root.get()));
			result.assign(_cprint.get());
		} else {
			unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
			result.assign(_cprint.get());
		}
		result += "\n";
		result = http_response(200,  HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
		write(result);
	} else {
		cJSON_AddStringToObject(root.get(), "Response empty", "Document not found");
		if (e.pretty) {
			unique_char_ptr _cprint(cJSON_Print(root.get()));
			result.assign(_cprint.get());
		} else {
			unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
			result.assign(_cprint.get());
		}
		result += "\n";
		write(http_response(404, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result));
	}

	manager()->database_pool.checkin(database);
}


void HttpClient::delete_document_view(const query_field &e)
{
	std::string result;
	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	auto tp_start = std::chrono::system_clock::now();
	if (!database->drop(command, e.commit)) {
		manager()->database_pool.checkin(database);
		write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp_start).count();
	LOG(this, "Time take for delete %d ms\n", time);
	std::unique_lock<std::mutex> lk(XapiandServer::static_mutex);
	update_pos_time();
	stats_cnt.del.min[b_time.minute]++;
	stats_cnt.del.sec[b_time.second]++;
	stats_cnt.del.tm_min[b_time.minute] += time;
	stats_cnt.del.tm_sec[b_time.second] += time;
	lk.unlock();

	manager()->database_pool.checkin(database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	if (e.commit) {
		cJSON_AddTrueToObject(data, "commit");
	} else {
		cJSON_AddFalseToObject(data, "commit");
	}
	cJSON_AddItemToObject(root.get(), "delete", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
	write(result);
}


void HttpClient::index_document_view(const query_field &e)
{
	std::string result;

	buid_path_index(index_path);
	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE|DB_SPAWN|DB_INIT_REF)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	auto tp_start = std::chrono::system_clock::now();

	if (!database->index(body, command, e.commit, content_type, content_length)) {
		manager()->database_pool.checkin(database);
		write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	// did = returned by index() call
	// filename = Termoprary file
	// if (manager()->binary->store(endpoints, did, filename, server())) {
	// 	INFO(this, "Storing %s...\n", filename.c_str());
	// }

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp_start).count();
	LOG(this, "Time take for index %d ms\n", time);
	std::unique_lock<std::mutex> lk(XapiandServer::static_mutex);
	update_pos_time();
	stats_cnt.index.min[b_time.minute]++;
	stats_cnt.index.sec[b_time.second]++;
	stats_cnt.index.tm_min[b_time.minute] += time;
	stats_cnt.index.tm_sec[b_time.second] += time;
	lk.unlock();

	manager()->database_pool.checkin(database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	if (e.commit) {
		cJSON_AddTrueToObject(data, "commit");
	} else {
		cJSON_AddFalseToObject(data, "commit");
	}
	cJSON_AddItemToObject(root.get(), "index", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
	write(result);
}


void HttpClient::update_document_view(const query_field &e)
{
	std::string result;

	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	unique_cJSON patches(cJSON_Parse(body.c_str()), cJSON_Delete);
	if (!patches) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		manager()->database_pool.checkin(database);
		write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	if (!database->patch(patches.get(), command, e.commit, content_type, content_length)) {
		manager()->database_pool.checkin(database);
		write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	manager()->database_pool.checkin(database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	if (e.commit) {
		cJSON_AddTrueToObject(data, "commit");
	} else {
		cJSON_AddFalseToObject(data, "commit");
	}
	cJSON_AddItemToObject(root.get(), "update", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
	write(result);
}


void HttpClient::stats_view(const query_field &e)
{
	std::string result;
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);

	if (e.server) {
		unique_cJSON server_stats = manager()->server_status();
		cJSON_AddItemToObject(root.get(), "Server status", server_stats.release());
	}
	if (e.database) {
		if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
			write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
			return;
		}
		unique_cJSON JSON_database = database->get_stats_database();
		cJSON_AddItemToObject(root.get(), "Database status", JSON_database.release());
		manager()->database_pool.checkin(database);
	}
	if (!e.document.empty()) {
		if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
			write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
			return;
		}
		unique_cJSON JSON_document = database->get_stats_docs(e.document);
		cJSON_AddItemToObject(root.get(), "Document status", JSON_document.release());
		manager()->database_pool.checkin(database);
	}
	if (!e.stats.empty()) {
		unique_cJSON server_stats_time = manager()->get_stats_time(e.stats);
		cJSON_AddItemToObject(root.get(), "Stats time", server_stats_time.release());
	}
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200,  HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
	write(result);
}


void HttpClient::bad_request_view(const query_field &e, int cmd)
{
	std::string result;

	unique_cJSON err_response(cJSON_CreateObject(), cJSON_Delete);
	switch (cmd) {
		case CMD_UNKNOWN_HOST:
			cJSON_AddStringToObject(err_response.get(), "Error message", std::string("Unknown host " + host).c_str());
			break;
		case CMD_UNKNOWN_ENDPOINT:
			cJSON_AddStringToObject(err_response.get(), "Error message", std::string("Unknown Endpoint - No one knows the index").c_str());
			break;
		default:
			cJSON_AddStringToObject(err_response.get(), "Error message", "BAD QUERY");
	}
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(err_response.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(err_response.get()));
		result.assign(_cprint.get());
	}
	result += "\n";

	write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result));
}


void HttpClient::upload_view(const query_field &)
{
	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	LOG(this, "Uploaded %s (%zu)\n", body_path, body_size);
	write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));

	manager()->database_pool.checkin(database);
}


void HttpClient::search_view(const query_field &e, bool facets, bool schema)
{
	std::string result;

	bool json_chunked = true;

	if (!manager()->database_pool.checkout(database, endpoints, DB_SPAWN)) {
		write(http_response(502, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		return;
	}

	if (schema) {
		std::string schema_;
		if (database->get_metadata(RESERVED_SCHEMA, schema_)) {
			unique_cJSON jschema(cJSON_Parse(schema_.c_str()), cJSON_Delete);
			readable_schema(jschema.get());
			unique_char_ptr _cprint(cJSON_Print(jschema.get()));
			schema_ = std::string(_cprint.get()) + "\n";
			write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, schema_));
			manager()->database_pool.checkin(database);
			return;
		} else {
			unique_cJSON err_response(cJSON_CreateObject(), cJSON_Delete);
			cJSON_AddStringToObject(err_response.get(), "Error message", "schema not found");
			if (e.pretty) {
				unique_char_ptr _cprint(cJSON_Print(err_response.get()));
				schema_.assign(_cprint.get());
			} else {
				unique_char_ptr _cprint(cJSON_PrintUnformatted(err_response.get()));
				schema_.assign(_cprint.get());
			}
			write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, schema_));
			manager()->database_pool.checkin(database);
			return;
		}
	}

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;
	auto tp_start = std::chrono::system_clock::now();
	int rmset = database->get_mset(e, mset, spies, suggestions);
	int cout_matched = mset.size();
	if (rmset == 1) {
		LOG(this, "get_mset return 1\n");
		write(http_response(400, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		manager()->database_pool.checkin(database);
		LOG(this, "ABORTED SEARCH\n");
		return;
	}
	if (rmset == 2) {
		LOG(this, "get_mset return 2\n");
		write(http_response(500, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
		manager()->database_pool.checkin(database);
		LOG(this, "ABORTED SEARCH\n");
		return;
	}


	LOG(this, "Suggered querys\n");
	std::vector<std::string>::const_iterator it_s(suggestions.begin());
	for ( ; it_s != suggestions.end(); it_s++) {
		LOG(this, "\t%s\n", (*it_s).c_str());
	}

	if (facets) {
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>::const_iterator spy(spies.begin());
		unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
		for (; spy != spies.end(); spy++) {
			std::string name_result = (*spy).first;
			cJSON *array_values = cJSON_CreateArray(); // It is managed by root.
			for (Xapian::TermIterator facet = (*spy).second->values_begin(); facet != (*spy).second->values_end(); ++facet) {
				cJSON *value = cJSON_CreateObject(); // It is managed by array_values.
				data_field_t field_t = database->get_data_field((*spy).first);
				cJSON_AddStringToObject(value, "value", Unserialise::unserialise(field_t.type, *facet).c_str());
				cJSON_AddNumberToObject(value, "termfreq", facet.get_termfreq());
				cJSON_AddItemToArray(array_values, value);
			}
			cJSON_AddItemToObject(root.get(), name_result.c_str(), array_values);
		}
		if (e.pretty) {
			unique_char_ptr _cprint(cJSON_Print(root.get()));
			result.assign(_cprint.get());
		} else {
			unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
			result.assign(_cprint.get());
		}
		result += "\n\n";
		result = http_response(200,  HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
		write(result);
	} else {
		int rc = 0;
		if (!mset.empty()) {
			for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); rc++, m++) {
				Xapian::docid docid = 0;
				std::string id;
				int rank = 0;
				double weight = 0, percent = 0;
				std::string data;

				int t = 3;
				for ( ; t >= 0; --t) {
					try {
						docid = *m;
						rank = m.get_rank();
						weight = m.get_weight();
						percent = m.get_percent();
						break;
					} catch (const Xapian::Error &err) {
						database->reopen();
						if (database->get_mset(e, mset, spies, suggestions, rc) == 0) {
							m = mset.begin();
						} else {
							t = -1;
						}
					}
				}

				Xapian::Document document;

				if (t >= 0) {
					// No errors, now try opening the document
					if (!database->get_document(docid, document)) {
						t = -1;  // flag as error
					}
				}

				if (t < 0) {
					// On errors, abort
					if (written) {
						write("0\r\n\r\n");
					} else {
						write(http_response(500, HTTP_STATUS | HTTP_HEADER | HTTP_BODY, parser.http_major, parser.http_minor));
					}
					manager()->database_pool.checkin(database);
					LOG(this, "ABORTED SEARCH\n");
					return;
				}

				data = document.get_data();
				const char *p = data.data();
				const char *p_end = p + data.size();
				size_t length = decode_length(&p, p_end, true);
				std::string ct_type = document.get_value(2);
				bool type_found = false;
				auto it = accept_set.begin();
				for (; it != accept_set.end(); it++){
					if (it->second == ct_type || it->second == "*/*") {
						if (it->second == "application/json" || ct_type == "application/json") {
							data = std::string(p, length);
							ct_type = "application/json";
							type_found = true;
							break;
						} else {
							p += length;
							data = std::string(p, p_end - p);
							type_found = true;
							break;
						}
					}
				}

				if (!type_found) {
					std::string response_err;
					unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
					cJSON_AddStringToObject(root.get(), "Error message", std::string("Response type " + ct_type + " not provided in the accept header").c_str());
					if (e.pretty) {
						unique_char_ptr _cprint(cJSON_Print(root.get()));
						response_err.assign(_cprint.get());
					} else {
						unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
						response_err.assign(_cprint.get());
					}
					response_err += "\n\n";
					write(http_response(406, HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, response_err));
					manager()->database_pool.checkin(database);
					LOG(this, "ABORTED SEARCH\n");
					return;
				}

				if (e.unique_doc && mset.size() == 1) {
					json_chunked = false;
				}

				id = document.get_value(0);

				/* Return data in case is not a json type */
				unique_cJSON object(cJSON_Parse(data.c_str()), cJSON_Delete);
				if (!object) {
					 write(http_response(200,  HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_BODY, parser.http_major, parser.http_minor, 0, data, ct_type));
					manager()->database_pool.checkin(database);
					return;
				}

				if (rc == 0 && json_chunked) {
					write(http_response(200, HTTP_STATUS | HTTP_HEADER | HTTP_CONTENT_TYPE | HTTP_CHUNKED | HTTP_MATCHED_COUNT, parser.http_major, parser.http_minor, cout_matched));
				}

				cJSON* object_data = cJSON_GetObjectItem(object.get(), RESERVED_DATA);
				if (object_data) {
					object_data = cJSON_Duplicate(object_data, 1);
					object.reset();
					object = unique_cJSON(object_data, cJSON_Delete);
				} else {
					clean_reserved(object.get());
					cJSON_AddStringToObject(object.get(), RESERVED_ID, id.c_str());
				}
				if (e.pretty) {
					unique_char_ptr _cprint(cJSON_Print(object.get()));
					result.assign(_cprint.get());
				} else {
					unique_char_ptr _cprint(cJSON_PrintUnformatted(object.get()));
					result.assign(_cprint.get());
				}
				result += "\n\n";
				if (json_chunked) {
					result = http_response(200,  HTTP_BODY | HTTP_CONTENT_TYPE | HTTP_CHUNKED, parser.http_major, parser.http_minor, 0, result);
				} else {
					result = http_response(200,  HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE, parser.http_major, parser.http_minor, 0, result);
				}

				if (!write(result)) {
					break;
				}
			}
			if (json_chunked) {
				write("0\r\n\r\n");
			}
		} else {
			int status_code = 200;
			unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);

			if (e.unique_doc) {
				cJSON_AddStringToObject(root.get(), "Response empty", "No document found");
				status_code = 404;
			} else {
				cJSON_AddStringToObject(root.get(), "Response empty", "No match found");
			}

			if (e.pretty) {
				unique_char_ptr _cprint(cJSON_Print(root.get()));
				result.assign(_cprint.get());
			} else {
				unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
				result.assign(_cprint.get());
			}
			result += "\n\n";
			result = http_response(status_code,  HTTP_STATUS | HTTP_HEADER | HTTP_BODY | HTTP_CONTENT_TYPE | HTTP_MATCHED_COUNT, parser.http_major, parser.http_minor, 0, result);
			write(result);
		}
	}

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tp_start).count();
	LOG(this, "Time take for search %d ms\n", time);
	std::unique_lock<std::mutex> lk(XapiandServer::static_mutex);
	update_pos_time();
	stats_cnt.search.min[b_time.minute]++;
	stats_cnt.search.sec[b_time.second]++;
	stats_cnt.search.tm_min[b_time.minute] += time;
	stats_cnt.search.tm_sec[b_time.second] += time;
	lk.unlock();

	manager()->database_pool.checkin(database);
	LOG(this, "FINISH SEARCH\n");
}


int HttpClient::_endpointgen(query_field &e, bool writable)
{
	int cmd, retval;
	bool has_node_name = false;
	struct http_parser_url u;
	std::string b = repr(path);

	LOG_HTTP_PROTO_PARSER(this, "URL: %s\n", b.c_str());
	if (http_parser_parse_url(b.c_str(), b.size(), 0, &u) == 0) {
		LOG_HTTP_PROTO_PARSER(this, "HTTP parsing done!\n");

		if (u.field_set & (1 <<  UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			std::string path_buf(b.c_str() + u.field_data[3].off, u.field_data[3].len);

			endpoints.clear();

			parser_url_path_t p;
			memset(&p, 0, sizeof(p));

			retval = url_path(path_buf.c_str(), path_size, &p);

			if (retval == -1) {
				return CMD_BAD_QUERY;
			}

			while (retval == 0) {
				command = urldecode(p.off_command, p.len_command);
				if (command.empty()) {
					return CMD_BAD_QUERY;
				}
				std::transform(command.begin(), command.end(), command.begin(), ::tolower);

				std::string ns;
				if (p.len_namespace) {
					ns = urldecode(p.off_namespace, p.len_namespace) + "/";
				} else {
					ns = "";
				}

				std::string path;
				if (p.len_path) {
					path = urldecode(p.off_path, p.len_path);
				} else {
					path = "";
				}

				index_path = ns + path;
				std::string node_name;
				Endpoint asked_node("xapian://" + node_name + index_path);
				std::vector<Endpoint> asked_nodes;

				if (p.len_host) {
					node_name = urldecode(p.off_host, p.len_host);
					has_node_name = true;
				} else {
					duration<double, std::milli> timeout;
					size_t num_endps = 1;
					if (writable) {
						timeout = 2s;
					} else timeout = 1s;

					if (!manager()->endp_r.resolve_index_endpoint(asked_node.path, manager(), asked_nodes, num_endps, timeout)) {
						has_node_name = true;
						node_name = local_node.name;
					}
				}

				if (has_node_name) {
					if(index_path.at(0) != '/') {
						index_path = '/' + index_path;
					}
					Endpoint index("xapian://" + node_name + index_path);
					int node_port = (index.port == XAPIAND_BINARY_SERVERPORT) ? 0 : index.port;
					node_name = index.host.empty() ? node_name : index.host;

					// Convert node to endpoint:
					char node_ip[INET_ADDRSTRLEN];
					const Node *node = nullptr;
					if (!manager()->touch_node(node_name, UNKNOWN_REGION, &node)) {
						LOG(this, "Node %s not found\n", node_name.c_str());
						host = node_name;
						return CMD_UNKNOWN_HOST;
					}
					if (!node_port) node_port = node->binary_port;
					inet_ntop(AF_INET, &(node->addr.sin_addr), node_ip, INET_ADDRSTRLEN);
					Endpoint endpoint("xapian://" + std::string(node_ip) + ":" + std::to_string(node_port) + index_path, NULL, -1, node_name);
					endpoints.insert(endpoint);
				} else {
					std::vector<Endpoint>::iterator it_endp = asked_nodes.begin();
					for ( ; it_endp != asked_nodes.end(); it_endp++) {
						endpoints.insert(*it_endp);
					}
				}
				LOG_CONN_WIRE(this, "Endpoint: -> %s\n", endpoints.as_string().c_str());

				p.len_host = 0; //Clean the host, so you not stay with the previous host in case doesn't come new one
				retval = url_path(path_buf.c_str(), path_size, &p);
			}
		}
		if ((parser.method == 4 || parser.method ==24) && endpoints.size() > 1) {
			return CMD_BAD_ENDPS;
		}

		cmd = identify_cmd(command);

		if (u.field_set & (1 <<  UF_QUERY)) {
			size_t query_size = u.field_data[4].len;
			const char *query_str = b.data() + u.field_data[4].off;

			parser_query_t q;

			q.offset = NULL;
			if (url_qs("pretty", query_str, query_size, &q) != -1) {
				std::string pretty = Serialise::boolean(urldecode(q.offset, q.length));
				(pretty.compare("f") == 0) ? e.pretty = false : e.pretty = true;
			}

			switch (cmd) {
				case CMD_SEARCH:
				case CMD_FACETS:

					q.offset = NULL;
					if (url_qs("offset", query_str, query_size, &q) != -1) {
						e.offset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
					}

					q.offset = NULL;
					if (url_qs("check_at_least", query_str, query_size, &q) != -1) {
						e.check_at_least = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
					}

					q.offset = NULL;
					if (url_qs("limit", query_str, query_size, &q) != -1) {
						e.limit = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
					}

					q.offset = NULL;
					if (url_qs("collapse_max", query_str, query_size, &q) != -1) {
						e.collapse_max = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
					}

					q.offset = NULL;
					if (url_qs("spelling", query_str, query_size, &q) != -1) {
						std::string spelling = Serialise::boolean(urldecode(q.offset, q.length));
						(spelling.compare("f") == 0) ? e.spelling = false : e.spelling = true;
					}

					q.offset = NULL;
					if (url_qs("synonyms", query_str, query_size, &q) != -1) {
						std::string synonyms = Serialise::boolean(urldecode(q.offset, q.length));
						(synonyms.compare("f") == 0) ? e.synonyms = false : e.synonyms = true;
					}

					q.offset = NULL;
					LOG(this, "Buffer: %s\n", query_str);
					while (url_qs("query", query_str, query_size, &q) != -1) {
						LOG(this, "%s\n", urldecode(q.offset, q.length).c_str());
						e.query.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("q", query_str, query_size, &q) != -1) {
						LOG(this, "%s\n", urldecode(q.offset, q.length).c_str());
						e.query.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("partial", query_str, query_size, &q) != -1) {
						e.partial.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("terms", query_str, query_size, &q) != -1) {
						e.terms.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("sort", query_str, query_size, &q) != -1) {
						e.sort.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("facets", query_str, query_size, &q) != -1) {
						e.facets.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					while (url_qs("language", query_str, query_size, &q) != -1) {
						e.language.push_back(urldecode(q.offset, q.length));
					}

					q.offset = NULL;
					if (url_qs("collapse", query_str, query_size, &q) != -1) {
						e.collapse = urldecode(q.offset, q.length);
					}

					q.offset = NULL;
					if (url_qs("fuzzy", query_str, query_size, &q) != -1) {
						std::string fuzzy = Serialise::boolean(urldecode(q.offset, q.length));
						(fuzzy.compare("f") == 0) ? e.is_fuzzy = false : e.is_fuzzy = true;
					}

					if(e.is_fuzzy) {
						q.offset = NULL;
						if (url_qs("fuzzy.n_rset", query_str, query_size, &q) != -1){
							e.fuzzy.n_rset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						if (url_qs("fuzzy.n_eset", query_str, query_size, &q) != -1){
							e.fuzzy.n_eset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						if (url_qs("fuzzy.n_term", query_str, query_size, &q) != -1){
							e.fuzzy.n_term = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						while (url_qs("fuzzy.field", query_str, query_size, &q) != -1){
							e.fuzzy.field.push_back(urldecode(q.offset, q.length));
						}

						q.offset = NULL;
						while (url_qs("fuzzy.type", query_str, query_size, &q) != -1){
							e.fuzzy.type.push_back(urldecode(q.offset, q.length));
						}
					}

					q.offset = NULL;
					if (url_qs("nearest", query_str, query_size, &q) != -1) {
						std::string nearest = Serialise::boolean(urldecode(q.offset, q.length));
						(nearest.compare("f") == 0) ? e.is_nearest = false : e.is_nearest = true;
					}

					if(e.is_nearest) {
						q.offset = NULL;
						if (url_qs("nearest.n_rset", query_str, query_size, &q) != -1){
							e.nearest.n_rset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						} else {
							e.nearest.n_rset = 5;
						}

						q.offset = NULL;
						if (url_qs("nearest.n_eset", query_str, query_size, &q) != -1){
							e.nearest.n_eset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						if (url_qs("nearest.n_term", query_str, query_size, &q) != -1){
							e.nearest.n_term = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						while (url_qs("nearest.field", query_str, query_size, &q) != -1){
							e.nearest.field.push_back(urldecode(q.offset, q.length));
						}

						q.offset = NULL;
						while (url_qs("nearest.type", query_str, query_size, &q) != -1){
							e.nearest.type.push_back(urldecode(q.offset, q.length));
						}
					}
					break;

				case CMD_ID:
					q.offset = NULL;
					if (url_qs("commit", query_str, query_size, &q) != -1) {
						std::string pretty = Serialise::boolean(urldecode(q.offset, q.length));
						(pretty.compare("f") == 0) ? e.commit = false : e.commit = true;
					}

					if (isRange(command)) {
						q.offset = NULL;
						if (url_qs("offset", query_str, query_size, &q) != -1) {
							e.offset = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						if (url_qs("check_at_least", query_str, query_size, &q) != -1) {
							e.check_at_least = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
						if (url_qs("limit", query_str, query_size, &q) != -1) {
							e.limit = static_cast<unsigned int>(strtoul(urldecode(q.offset, q.length)));
						}

						q.offset = NULL;
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
					q.offset = NULL;
					if (url_qs("server", query_str, query_size, &q) != -1) {
						std::string server = Serialise::boolean(urldecode(q.offset, q.length));
						(server.compare("f") == 0) ? e.server = false : e.server = true;
					}

					q.offset = NULL;
					if (url_qs("database", query_str, query_size, &q) != -1) {
						std::string _database = Serialise::boolean(urldecode(q.offset, q.length));
						(_database.compare("f") == 0) ? e.database = false : e.database = true;
					}

					q.offset = NULL;
					if (url_qs("document", query_str, query_size, &q) != -1) {
						e.document = urldecode(q.offset, q.length);
					}

					q.offset = NULL;
					if (url_qs("stats", query_str, query_size, &q) != -1) {
						e.stats = urldecode(q.offset, q.length);
					}
					break;

				case CMD_UPLOAD:
					break;
			}
		} else {
			//Especial case (search ID and empty query in the url)
			if (cmd == CMD_ID) {
				if (isRange(command)) {
					e.offset = 0;
					e.check_at_least = 0;
					e.limit = 10;
					e.sort.push_back(RESERVED_ID);
					}
				else {
					e.limit = 1;
					e.unique_doc = true;
					e.offset = 0;
					e.check_at_least = 0;
				}
			}
		}
	} else {
		LOG_CONN_WIRE(this,"Parsing not done\n");
		/* Bad query */
		return CMD_BAD_QUERY;
	}
	return cmd;
}


int
HttpClient::identify_cmd(const std::string &commad)
{
	if (commad.compare(HTTP_SEARCH) == 0) {
		return CMD_SEARCH;
	}

	if (commad.compare(HTTP_FACETS) == 0) {
		return CMD_FACETS;
	}

	if (commad.compare(HTTP_STATS) == 0) {
		return CMD_STATS;
	}

	if (commad.compare(HTTP_SCHEMA) == 0) {
		return CMD_SCHEMA;
	}

	if (commad.compare(HTTP_UPLOAD) == 0) {
		return CMD_UPLOAD;
	}

	return CMD_ID;
}
