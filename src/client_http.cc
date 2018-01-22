/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
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

#include <algorithm>                        // for move
#include <exception>                        // for exception
#include <functional>                       // for __base, function
#include <regex>                            // for regex_iterator, match_res...
#include <stdexcept>                        // for invalid_argument, range_e...
#include <stdlib.h>                         // for mkstemp
#include <string.h>                         // for strerror, strcpy
#include <sys/errno.h>                      // for __error, errno
#include <sysexits.h>                       // for EX_SOFTWARE
#include <syslog.h>                         // for LOG_WARNING, LOG_ERR, LOG...
#include <type_traits>                      // for enable_if<>::type
#include <xapian.h>                         // for version_string, MSetIterator

#if defined(XAPIAND_V8)
#include <v8-version.h>                       // for V8_MAJOR_VERSION, V8_MINOR_VERSION
#endif

#if defined(XAPIAND_CHAISCRIPT)
#include <chaiscript/chaiscript_defines.hpp>  // for chaiscript::Build_Info
#endif

#include "cppcodec/base64_rfc4648.hpp"      // for cppcodec::base64_rfc4648
#include "endpoint.h"                       // for Endpoints, Node, Endpoint
#include "ev/ev++.h"                        // for async, io, loop_ref (ptr ...
#include "exception.h"                      // for Exception, SerialisationE...
#include "guid/guid.h"                      // for GuidGenerator, Guid
#include "io_utils.h"                       // for close, write, unlink
#include "log.h"                            // for L_CALL, L_ERR, LOG_D...
#include "manager.h"                        // for XapiandManager, XapiandMa...
#include "msgpack.h"                        // for MsgPack, object::object, ...
#include "multivalue/aggregation.h"         // for AggregationMatchSpy
#include "multivalue/aggregation_metric.h"  // for AGGREGATION_AGGS
#include "queue.h"                          // for Queue
#include "rapidjson/document.h"             // for Document
#include "schema.h"                         // for Schema
#include "serialise.h"                      // for boolean
#include "servers/server.h"                 // for XapiandServer, XapiandSer...
#include "servers/server_http.h"            // for HttpServer
#include "stats.h"                          // for Stats
#include "threadpool.h"                     // for ThreadPool
#include "utils.h"                          // for delta_string
#include "package.h"                        // for Package
#include "xxh64.hpp"                        // for xxh64


#define MAX_BODY_SIZE (250 * 1024 * 1024)
#define MAX_BODY_MEM (5 * 1024 * 1024)

#define QUERY_FIELD_COMMIT (1 << 0)
#define QUERY_FIELD_SEARCH (1 << 1)
#define QUERY_FIELD_ID     (1 << 2)
#define QUERY_FIELD_TIME   (1 << 3)
#define QUERY_FIELD_PERIOD (1 << 4)


// Reserved words only used in the responses to the user.
constexpr const char RESPONSE_ENDPOINT[]            = "#endpoint";
constexpr const char RESPONSE_RANK[]                = "#rank";
constexpr const char RESPONSE_WEIGHT[]              = "#weight";
constexpr const char RESPONSE_PERCENT[]             = "#percent";
constexpr const char RESPONSE_TOTAL_COUNT[]         = "#total_count";
constexpr const char RESPONSE_MATCHES_ESTIMATED[]   = "#matches_estimated";
constexpr const char RESPONSE_HITS[]                = "#hits";
constexpr const char RESPONSE_AGGREGATIONS[]        = "#aggregations";
constexpr const char RESPONSE_QUERY[]               = "#query";
constexpr const char RESPONSE_MESSAGE[]             = "#message";
constexpr const char RESPONSE_STATUS[]              = "#status";
constexpr const char RESPONSE_NAME[]                = "#name";
constexpr const char RESPONSE_NODES[]               = "#nodes";
constexpr const char RESPONSE_CLUSTER_NAME[]        = "#cluster_name";
constexpr const char RESPONSE_COMMIT[]              = "#commit";
constexpr const char RESPONSE_SERVER[]              = "#server";
constexpr const char RESPONSE_URL[]                 = "#url";
constexpr const char RESPONSE_VERSIONS[]            = "#versions";
constexpr const char RESPONSE_DELETE[]              = "#delete";
constexpr const char RESPONSE_DOCID[]               = "#docid";
constexpr const char RESPONSE_SERVER_INFO[]         = "#server_info";
constexpr const char RESPONSE_DOCUMENT_INFO[]       = "#document_info";
constexpr const char RESPONSE_DATABASE_INFO[]       = "#database_info";


static const std::regex header_params_re("\\s*;\\s*([a-z]+)=(\\d+(?:\\.\\d+)?)", std::regex::optimize);
static const std::regex header_accept_re("([-a-z+]+|\\*)/([-a-z+]+|\\*)((?:\\s*;\\s*[a-z]+=\\d+(?:\\.\\d+)?)*)", std::regex::optimize);
static const std::regex header_accept_encoding_re("([-a-z+]+|\\*)((?:\\s*;\\s*[a-z]+=\\d+(?:\\.\\d+)?)*)", std::regex::optimize);

static const std::string eol("\r\n");

GuidGenerator HttpClient::generator;

AcceptLRU HttpClient::accept_sets;

AcceptEncodingLRU HttpClient::accept_encoding_sets;


std::string
HttpClient::http_response(enum http_status status, int mode, unsigned short http_major, unsigned short http_minor, int total_count, int matches_estimated, const std::string& _body, const std::string& ct_type, const std::string& ct_encoding) {
	L_CALL("HttpClient::http_response()");

	char buffer[20];
	std::string head;
	std::string headers;
	std::string head_sep;
	std::string headers_sep;
	std::string response;

	if (mode & HTTP_STATUS_RESPONSE) {
		response_status = status;

		snprintf(buffer, sizeof(buffer), "HTTP/%d.%d %d ", http_major, http_minor, status);
		head += buffer;
		head += http_status_str(status);
		head_sep += eol;
		if (!(mode & HTTP_HEADER_RESPONSE)) {
			headers_sep += eol;
		}
	}

	if (mode & HTTP_HEADER_RESPONSE) {
		headers += "Server: " + Package::STRING + eol;

		if (!endpoints.empty()) {
			headers += "Database: " + endpoints.to_string() + eol;
		}

		response_ends = std::chrono::system_clock::now();
		headers += "Response-Time: " + delta_string(request_begins, response_ends) + eol;
		if (operation_ends >= operation_begins) {
			headers += "Operation-Time: " + delta_string(operation_begins, operation_ends) + eol;
		}

		if (mode & HTTP_OPTIONS_RESPONSE) {
			headers += "Allow: GET, POST, PUT, PATCH, MERGE, DELETE, HEAD, OPTIONS" + eol;
		}

		if (mode & HTTP_TOTAL_COUNT_RESPONSE) {
			headers += "Total-Count: " + std::to_string(total_count) + eol;
		}

		if (mode & HTTP_MATCHES_ESTIMATED_RESPONSE) {
			headers += "Matches-Estimated: " + std::to_string(matches_estimated) + eol;
		}

		if (mode & HTTP_CONTENT_TYPE_RESPONSE) {
			headers += "Content-Type: " + ct_type + eol;
		}

		if (mode & HTTP_CONTENT_ENCODING_RESPONSE) {
			headers += "Content-Encoding: " + ct_encoding + eol;
		}

		if (mode & HTTP_CHUNKED_RESPONSE) {
			headers += "Transfer-Encoding: chunked" + eol;
		} else {
			headers += "Content-Length: ";
			snprintf(buffer, sizeof(buffer), "%lu", _body.size());
			headers += buffer + eol;
		}
		headers_sep += eol;
	}

	if (mode & HTTP_BODY_RESPONSE) {
		if (mode & HTTP_CHUNKED_RESPONSE) {
			snprintf(buffer, sizeof(buffer), "%lx", _body.size());
			response += buffer + eol;
			response += _body + eol;
		} else {
			response += _body;
		}
	}

	auto this_response_size = response.size();
	response_size += this_response_size;

	if (Logging::log_level > LOG_DEBUG) {
		response_head += head;
		response_headers += headers;
	}

	return head + head_sep + headers + headers_sep + response;
}


HttpClient::HttpClient(std::shared_ptr<HttpServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_)
	: BaseClient(std::move(server_), ev_loop_, ev_flags_, sock_),
	  indent(-1),
	  response_size(0),
	  response_logged(false),
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
		L_CRIT("Inconsistency in number of http clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_CONN("New Http Client in socket %d, %d client(s) of a total of %d connected.", sock_, http_clients, total_clients);

	response_log = L_DELAYED(true, 300s, LOG_WARNING, PURPLE, "Client idle for too long...").release();
	idle = true;

	L_OBJ("CREATED HTTP CLIENT! (%d clients)", http_clients);
}


HttpClient::~HttpClient()
{
	int http_clients = --XapiandServer::http_clients;
	int total_clients = XapiandServer::total_clients;
	if (http_clients < 0 || http_clients > total_clients) {
		L_CRIT("Inconsistency in number of http clients");
		sig_exit(-EX_SOFTWARE);
	}

	if (XapiandManager::manager->shutdown_asap.load()) {
		if (http_clients <= 0) {
			XapiandManager::manager->shutdown_sig(0);
		}
	}

	if (body_descriptor) {
		if (io::close(body_descriptor) < 0) {
			L_ERR("ERROR: Cannot close temporary file '%s': %s", body_path, strerror(errno));
		}

		if (io::unlink(body_path) < 0) {
			L_ERR("ERROR: Cannot delete temporary file '%s': %s", body_path, strerror(errno));
		}
	}

	response_log.load()->clear();

	if (shutting_down || !(idle && write_queue.empty())) {
		L_WARNING("Client killed!");
	}

	L_OBJ("DELETED HTTP CLIENT! (%d clients left)", http_clients);
}


void
HttpClient::on_read(const char* buf, ssize_t received)
{
	L_CALL("HttpClient::on_read(<buf>, %zd)", received);

	unsigned init_state = parser.state;

	if (received <= 0) {
		response_log.load()->clear();
		if (received < 0 || init_state != 18 || !write_queue.empty()) {
			L_WARNING("Client unexpectedly closed the other end! [%d]", init_state);
			destroy();  // Handle error. Just close the connection.
			detach();
		}
		clean_http_request();
		return;
	}

	if (request_begining) {
		idle = false;
		request_begining = false;
		request_begins = std::chrono::system_clock::now();
		auto old_response_log = response_log.exchange(L_DELAYED(true, 10s, LOG_WARNING, PURPLE, "Request taking too long...").release());
		old_response_log->clear();
		response_logged = false;
	}

	L_HTTP_WIRE("HttpClient::on_read: %zd bytes", received);
	ssize_t parsed = http_parser_execute(&parser, &settings, buf, received);
	if (parsed == received) {
		unsigned final_state = parser.state;
		if (final_state == init_state) {
			if (received == 1 and buf[0] == '\n') {  // ignore '\n' request
				request_begining = true;
				set_idle();
				return;
			}
		}
		if (final_state == 1 || final_state == 18) {  // dead or message_complete
			L_EV("Disable read event");
			io_read.stop();
			written = 0;
			if (!closed) {
				XapiandManager::manager->thread_pool.enqueue(share_this<HttpClient>());
			}
		}
	} else {
		enum http_status error_code = HTTP_STATUS_BAD_REQUEST;
		http_errno err = HTTP_PARSER_ERRNO(&parser);
		if (err == HPE_INVALID_METHOD) {
			write_http_response(HTTP_STATUS_NOT_IMPLEMENTED);
		} else {
			std::string message(http_errno_description(err));
			MsgPack err_response = {
				{ RESPONSE_STATUS, (int)error_code },
				{ RESPONSE_MESSAGE, split_string(message, '\n') }
			};
			write_http_response(error_code, err_response);
			L_WARNING(HTTP_PARSER_ERRNO(&parser) != HPE_OK ? message.c_str() : "incomplete request");
		}
		destroy();  // Handle error. Just close the connection.
		detach();
		clean_http_request();
	}
}


void
HttpClient::on_read_file(const char*, ssize_t received)
{
	L_CALL("HttpClient::on_read_file(<buf>, %zd)", received);

	L_ERR("Not Implemented: HttpClient::on_read_file: %zd bytes", received);
}


void
HttpClient::on_read_file_done()
{
	L_CALL("HttpClient::on_read_file_done()");

	L_ERR("Not Implemented: HttpClient::on_read_file_done");
}


// HTTP parser callbacks.
const http_parser_settings HttpClient::settings = {
	HttpClient::on_info,  // on_message_begin
	HttpClient::on_data,  // on_url
	HttpClient::on_data,  // on_status
	HttpClient::on_data,  // on_header_field
	HttpClient::on_data,  // on_header_value
	HttpClient::on_info,  // on_headers_complete
	HttpClient::on_data,  // on_body
	HttpClient::on_info,  // on_message_complete
	HttpClient::on_info,  // on_chunk_header
	HttpClient::on_info   // on_chunk_complete
};


int
HttpClient::on_info(http_parser* p)
{
	HttpClient *self = static_cast<HttpClient *>(p->data);

	L_CALL("HttpClient::on_info(...)");

	int state = p->state;

	L_HTTP_PROTO_PARSER("%4d - (INFO)", state);

	switch (state) {
		case 18:  // message_complete
			break;
		case 19:  // message_begin
			if (Logging::log_level > LOG_DEBUG) {
				self->request_head.clear();
				self->request_headers.clear();
				self->request_body.clear();
				self->response_head.clear();
				self->response_headers.clear();
				self->response_body.clear();
			}
			self->path.clear();
			self->body.clear();
			self->decoded_body.first.clear();
			self->body_size = 0;
			self->header_name.clear();
			self->header_value.clear();
			if (self->body_descriptor && io::close(self->body_descriptor) < 0) {
				L_ERR("ERROR: Cannot close temporary file '%s': %s", self->body_path, strerror(errno));
			} else {
				self->body_descriptor = 0;
			}
			break;
		case 50:  // headers done
			self->request_head = format_string("%s %s HTTP/%d.%d", http_method_str(HTTP_PARSER_METHOD(p)), self->path.c_str(), p->http_major, p->http_minor);
			if (self->expect_100) {
				// Return 100 if client is expecting it
				self->write(self->http_response(HTTP_STATUS_CONTINUE, HTTP_STATUS_RESPONSE | HTTP_EXPECTED_CONTINUE_RESPONSE, p->http_major, p->http_minor));
			}
			break;
		case 57: //s_chunk_data begin chunk
			break;
	}

	return 0;
}


int
HttpClient::on_data(http_parser* p, const char* at, size_t length)
{
	HttpClient *self = static_cast<HttpClient *>(p->data);

	L_CALL("HttpClient::on_data(...)");

	int state = p->state;

	L_HTTP_PROTO_PARSER("%4d - %s", state, repr(at, length).c_str());

	if (state > 26 && state <= 32) {
		// s_req_path  ->  s_req_http_start
		self->path.append(at, length);
	} else if (state >= 43 && state <= 44) {
		// s_header_field  ->  s_header_value_discard_ws
		self->header_name.append(at, length);
	} else if (state >= 45 && state <= 50) {
		// s_header_value_discard_ws_almost_done  ->  s_header_almost_done
		self->header_value.append(at, length);
		if (Logging::log_level > LOG_DEBUG) {
			self->request_headers += self->header_name + ": " + self->header_value + eol;
		}
		if (state == 50) {
			std::string name = lower_string(self->header_name);

			switch (xxh64::hash(name)) {
				case xxh64::hash("host"):
					self->host = self->header_value;
					break;
				case xxh64::hash("expect"):
				case xxh64::hash("100-continue"):
					if (p->content_length > MAX_BODY_SIZE) {
						self->write(self->http_response(HTTP_STATUS_PAYLOAD_TOO_LARGE, HTTP_STATUS_RESPONSE, p->http_major, p->http_minor));
						self->close();
						return 0;
					}
					// Respond with HTTP/1.1 100 Continue
					self->expect_100 = true;
					break;

				case xxh64::hash("content-type"):
					self->content_type = lower_string(self->header_value);
					break;
				case xxh64::hash("content-length"):
					self->content_length = self->header_value;
					break;
				case xxh64::hash("accept"): {
					auto value = lower_string(self->header_value);
					try {
						self->accept_set = accept_sets.at(value);
					} catch (const std::out_of_range&) {
						std::sregex_iterator next(value.begin(), value.end(), header_accept_re, std::regex_constants::match_any);
						std::sregex_iterator end;
						int i = 0;
						while (next != end) {
							int indent = -1;
							double q = 1.0;
							if (next->length(3)) {
								auto param = next->str(3);
								std::sregex_iterator next_param(param.begin(), param.end(), header_params_re, std::regex_constants::match_any);
								while (next_param != end) {
									if (next_param->str(1) == "q") {
										q = strict_stod(next_param->str(2));
									} else if (next_param->str(1) == "indent") {
										indent = strict_stoi(next_param->str(2));
										if (indent < 0) indent = 0;
										else if (indent > 16) indent = 16;
									}
									++next_param;
								}
							}
							self->accept_set.insert(std::make_tuple(q, i, ct_type_t(next->str(1), next->str(2)), indent));
							++next;
							++i;
						}
						accept_sets.emplace(value, self->accept_set);
					}
					break;
				}

				case xxh64::hash("accept-encoding"): {
					auto value = lower_string(self->header_value);
					try {
						self->accept_encoding_set = accept_encoding_sets.at(value);
					} catch (const std::out_of_range&) {
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
							self->accept_encoding_set.insert(std::make_tuple(q, i, next->str(1), 0));
							++next;
							++i;
						}
						accept_encoding_sets.emplace(value, self->accept_encoding_set);
					}
					break;
				}

				case xxh64::hash("x-http-method-override"):
					switch (xxh64::hash(upper_string(self->header_value))) {
						case xxh64::hash("PUT"):
							p->method = HTTP_PUT;
							break;
						case xxh64::hash("PATCH"):
							p->method = HTTP_PATCH;
							break;
						case xxh64::hash("MERGE"):
							p->method = HTTP_MERGE;
							break;
						case xxh64::hash("DELETE"):
							p->method = HTTP_DELETE;
							break;
						case xxh64::hash("GET"):
							p->method = HTTP_GET;
							break;
						case xxh64::hash("POST"):
							p->method = HTTP_POST;
							break;
						default:
							p->http_errno = HPE_INVALID_METHOD;
							break;
					}
					break;
			}

			self->header_name.clear();
			self->header_value.clear();
		}
	} else if (state >= 59 && state <= 62) { // s_chunk_data_done, s_body_identity  ->  s_message_done
		self->body_size += length;
		if (self->body_size > MAX_BODY_SIZE || p->content_length > MAX_BODY_SIZE) {
			self->write(self->http_response(HTTP_STATUS_PAYLOAD_TOO_LARGE, HTTP_STATUS_RESPONSE, p->http_major, p->http_minor));
			self->close();
			return 0;
		} else if (self->body_descriptor || self->body_size > MAX_BODY_MEM) {

			// The next two lines are switching off the write body in to a file option when the body is too large
			// (for avoid have it in memory) but this feature is not available yet
			self->write(self->http_response(HTTP_STATUS_PAYLOAD_TOO_LARGE, HTTP_STATUS_RESPONSE, p->http_major, p->http_minor)); // <-- remove leater!
			self->close(); // <-- remove leater!

			if (!self->body_descriptor) {
				strcpy(self->body_path, "/tmp/xapiand_upload.XXXXXX");
				self->body_descriptor = mkstemp(self->body_path);
				if (self->body_descriptor < 0) {
					L_ERR("Cannot write to %s (1)", self->body_path);
					return 0;
				}
				io::write(self->body_descriptor, self->body.data(), self->body.size());
				self->body.clear();
			}
			io::write(self->body_descriptor, at, length);
			if (state == 62) {
				if (self->body_descriptor && io::close(self->body_descriptor) < 0) {
					L_ERR("ERROR: Cannot close temporary file '%s': %s", self->body_path, strerror(errno));
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
	L_CALL("HttpClient::run()");

	L_CONN("Start running in worker.");

	L_OBJ_BEGIN("HttpClient::run:BEGIN");
	response_begins = std::chrono::system_clock::now();
	auto old_response_log = response_log.exchange(L_DELAYED(true, 1s, LOG_WARNING, PURPLE, "Response taking too long...").release());
	old_response_log->clear();

	std::string error;
	enum http_status error_code = HTTP_STATUS_OK;

	try {
		if (Logging::log_level > LOG_DEBUG) {
			if (Logging::log_level > LOG_DEBUG + 1 && content_type.find("image/") != std::string::npos) {
				// From [https://www.iterm2.com/documentation-images.html]
				std::string b64_name = cppcodec::base64_rfc4648::encode("");
				std::string b64_request_body = cppcodec::base64_rfc4648::encode(body);
				request_body = format_string("\033]1337;File=name=%s;inline=1;size=%d;width=20%%:",
					b64_name.c_str(),
					b64_request_body.size());
				request_body += b64_request_body.c_str();
				request_body += '\a';
			} else {
				auto body_ = get_decoded_body();
				auto ct_type = body_.first;
				auto msgpack = body_.second;
				if (ct_type == json_type || ct_type == msgpack_type) {
					request_body = msgpack.to_string(4);
				} else if (!body.empty()) {
					request_body = "<blob " + bytes_string(body.size()) + ">";
				}
			}
			log_request();
		}

		auto method = HTTP_PARSER_METHOD(&parser);
		switch (method) {
			case HTTP_DELETE:
				_delete(method);
				break;
			case HTTP_GET:
				_get(method);
				break;
			case HTTP_POST:
				_post(method);
				break;
			case HTTP_HEAD:
				_head(method);
				break;
			case HTTP_MERGE:
				_merge(method);
				break;
			case HTTP_PUT:
				_put(method);
				break;
			case HTTP_OPTIONS:
				_options(method);
				break;
			case HTTP_PATCH:
				_patch(method);
				break;
			default:
				error_code = HTTP_STATUS_NOT_IMPLEMENTED;
				parser.http_errno = HPE_INVALID_METHOD;
				break;
		}
	} catch (const DocNotFoundError& exc) {
		error_code = HTTP_STATUS_NOT_FOUND;
		error.assign(http_status_str(error_code));
		// L_EXC("ERROR: %s", error.c_str());
	} catch (const MissingTypeError& exc) {
		error_code = HTTP_STATUS_PRECONDITION_FAILED;
		error.assign(exc.what());
		// L_EXC("ERROR: %s", error.c_str());
	} catch (const ClientError& exc) {
		error_code = HTTP_STATUS_BAD_REQUEST;
		error.assign(exc.what());
		// L_EXC("ERROR: %s", error.c_str());
	} catch (const CheckoutError& exc) {
		error_code = HTTP_STATUS_NOT_FOUND;
		error.assign(std::string(http_status_str(error_code)) + ": " + exc.what());
		// L_EXC("ERROR: %s", error.c_str());
	} catch (const TimeOutError& exc) {
		error_code = HTTP_STATUS_REQUEST_TIMEOUT;
		error.assign(std::string(http_status_str(error_code)) + ": " + exc.what());
		// L_EXC("ERROR: %s", error.c_str());
	} catch (const BaseException& exc) {
		error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		error.assign(*exc.get_message() ? exc.get_message() : "Unkown BaseException!");
		L_EXC("ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
	} catch (const Xapian::Error& exc) {
		error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;

		auto exc_msg_error = exc.get_msg();
		const char* error_str = exc.get_error_string();
		if (error_str) {
			exc_msg_error.append(" (").append(error_str).push_back(')');
		}

		if (exc_msg_error.empty()) {
			error.assign("Unkown Xapian::Error!");
		} else {
			error.assign(exc_msg_error);
		}
		L_EXC("ERROR: %s", error.c_str());
	} catch (const std::exception& exc) {
		error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		error.assign(*exc.what() ? exc.what() : "Unkown std::exception!");
		L_EXC("ERROR: %s", error.c_str());
	} catch (...) {
		error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		error.assign("Unknown exception!");
		std::exception exc;
		L_EXC("ERROR: %s", error.c_str());
	}

	if (error_code != HTTP_STATUS_OK) {
		if (written) {
			destroy();
			detach();
		} else {
			MsgPack err_response = {
				{ RESPONSE_STATUS, (int)error_code },
				{ RESPONSE_MESSAGE, split_string(error, '\n') }
			};

			write_http_response(error_code, err_response);
		}
	}

	clean_http_request();
	read_start_async.send();

	L_OBJ_END("HttpClient::run:END");
}


void
HttpClient::_options(enum http_method)
{
	L_CALL("HttpClient::_options()");

	write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_OPTIONS_RESPONSE | HTTP_BODY_RESPONSE, parser.http_major, parser.http_minor));
}


void
HttpClient::_head(enum http_method method)
{
	L_CALL("HttpClient::_head()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_NO_ID:
			write_http_response(HTTP_STATUS_OK);
			break;
		case Command::NO_CMD_ID:
			document_info_view(method, cmd);
			break;
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_get(enum http_method method)
{
	L_CALL("HttpClient::_get()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_NO_ID:
			home_view(method, cmd);
			break;
		case Command::NO_CMD_ID:
			search_view(method, cmd);
			break;
		case Command::CMD_SEARCH:
			path_parser.skip_id();  // Command has no ID
			search_view(method, cmd);
			break;
		case Command::CMD_SCHEMA:
			path_parser.skip_id();  // Command has no ID
			schema_view(method, cmd);
			break;
#if XAPIAND_DATABASE_WAL
		case Command::CMD_WAL:
			path_parser.skip_id();  // Command has no ID
			wal_view(method, cmd);
			break;
#endif
		case Command::CMD_INFO:
			path_parser.skip_id();  // Command has no ID
			info_view(method, cmd);
			break;
		case Command::CMD_STATS:
			path_parser.skip_id();  // Command has no ID
			stats_view(method, cmd);
			break;
		case Command::CMD_NODES:
			path_parser.skip_id();  // Command has no ID
			nodes_view(method, cmd);
			break;
		case Command::CMD_METADATA:
			path_parser.skip_id();  // Command has no ID
			meta_view(method, cmd);
			break;
		default:
			if (path_parser.off_cmd && path_parser.len_cmd > 1) {
				if (*(path_parser.off_cmd + 1) == '_') {
					path_parser.skip_id();  // Command has no ID
					path_parser.off_pmt = path_parser.off_cmd + 1;
					path_parser.len_pmt = path_parser.len_cmd + 1;
					meta_view(method, cmd);
					break;
				}
			}
#ifndef NDEBUG
		case Command::CMD_QUIT:
#endif
		case Command::CMD_TOUCH:
		case Command::BAD_QUERY:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_merge(enum http_method method)
{
	L_CALL("HttpClient::_merge()");


	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_ID:
			update_document_view(method, cmd);
			break;
		case Command::CMD_METADATA:
			path_parser.skip_id();  // Command has no ID
			update_meta_view(method, cmd);
			break;
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_put(enum http_method method)
{
	L_CALL("HttpClient::_put()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_ID:
			index_document_view(method, cmd);
			break;
		case Command::CMD_METADATA:
			path_parser.skip_id();  // Command has no ID
			write_meta_view(method, cmd);
			break;
		case Command::CMD_SCHEMA:
			path_parser.skip_id();  // Command has no ID
			write_schema_view(method, cmd);
			break;
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_post(enum http_method method)
{
	L_CALL("HttpClient::_post()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_ID:
			path_parser.skip_id();  // Command has no ID
			index_document_view(method, cmd);
			break;
		case Command::CMD_SCHEMA:
			path_parser.skip_id();  // Command has no ID
			write_schema_view(method, cmd);
			break;
		case Command::CMD_SEARCH:
			path_parser.skip_id();  // Command has no ID
			search_view(method, cmd);
			break;
		case Command::CMD_TOUCH:
			path_parser.skip_id();  // Command has no ID
			touch_view(method, cmd);
			break;
#ifndef NDEBUG
		case Command::CMD_QUIT:
			XapiandManager::manager->shutdown_asap.store(epoch::now<>());
			XapiandManager::manager->shutdown_sig(0);
			break;
#endif
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_patch(enum http_method method)
{
	L_CALL("HttpClient::_patch()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_ID:
			update_document_view(method, cmd);
			break;
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


void
HttpClient::_delete(enum http_method method)
{
	L_CALL("HttpClient::_delete()");

	auto cmd = url_resolve();
	switch (cmd) {
		case Command::NO_CMD_ID:
			delete_document_view(method, cmd);
			break;
		default:
			write_status_response(HTTP_STATUS_METHOD_NOT_ALLOWED);
			break;
	}
}


std::pair<ct_type_t, MsgPack>&
HttpClient::get_decoded_body()
{
	L_CALL("HttpClient::get_decoded_body()");

	if (decoded_body.first.empty()) {

		// Create MsgPack object for the body
		auto ct_type_str = content_type;
		if (ct_type_str.empty()) {
			ct_type_str = JSON_CONTENT_TYPE;
		}

		ct_type_t ct_type;
		MsgPack msgpack;
		if (!body.empty()) {
			rapidjson::Document rdoc;
			switch (xxh64::hash(ct_type_str)) {
				case xxh64::hash(FORM_URLENCODED_CONTENT_TYPE):
				case xxh64::hash(X_FORM_URLENCODED_CONTENT_TYPE):
					try {
						json_load(rdoc, body);
						msgpack = MsgPack(rdoc);
						ct_type = json_type;
					} catch (const std::exception&) {
						msgpack = MsgPack(body);
						ct_type = msgpack_type;
					}
					break;
				case xxh64::hash(JSON_CONTENT_TYPE):
					json_load(rdoc, body);
					msgpack = MsgPack(rdoc);
					ct_type = json_type;
					break;
				case xxh64::hash(MSGPACK_CONTENT_TYPE):
				case xxh64::hash(X_MSGPACK_CONTENT_TYPE):
					msgpack = MsgPack::unserialise(body);
					ct_type = msgpack_type;
					break;
				default:
					msgpack = MsgPack(body);
					ct_type = ct_type_t(ct_type_str);
					break;
			}
		}

		decoded_body = std::make_pair(ct_type, msgpack);
	}

	return decoded_body;
}


void
HttpClient::home_view(enum http_method method, Command)
{
	L_CALL("HttpClient::home_view()");

	endpoints.clear();
	endpoints.add(Endpoint("."));

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_SPAWN, method);

	auto local_node_ = local_node.load();
	auto document = db_handler.get_document(serialise_node_id(local_node_->id));

	auto obj = document.get_obj();
	if (obj.find(ID_FIELD_NAME) == obj.end()) {
		obj[ID_FIELD_NAME] = document.get_field(ID_FIELD_NAME) || document.get_value(ID_FIELD_NAME);
	}

	operation_ends = std::chrono::system_clock::now();

#ifdef XAPIAND_CLUSTERING
	obj[RESPONSE_CLUSTER_NAME] = XapiandManager::manager->opts.cluster_name;
#endif
	obj[RESPONSE_SERVER] = Package::STRING;
	obj[RESPONSE_URL] = Package::BUGREPORT;
	obj[RESPONSE_VERSIONS] = {
		{ "Xapiand", Package::REVISION.empty() ? Package::VERSION : format_string("%s_%s", Package::VERSION.c_str(), Package::REVISION.c_str()) },
		{ "Xapian", format_string("%d.%d.%d", Xapian::major_version(), Xapian::minor_version(), Xapian::revision()) },
#if defined(XAPIAND_V8)
		{ "V8", format_string("%u.%u", V8_MAJOR_VERSION, V8_MINOR_VERSION) },
#endif
#if defined(XAPIAND_CHAISCRIPT)
		{ "ChaiScript", format_string("%d.%d", chaiscript::Build_Info::version_major(), chaiscript::Build_Info::version_minor()) },
#endif
	};

	write_http_response(HTTP_STATUS_OK, obj);
}


void
HttpClient::document_info_view(enum http_method method, Command)
{
	L_CALL("HttpClient::document_info_view()");

	endpoints_maker(1s);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_SPAWN, method);

	MsgPack response;
	response[RESPONSE_DOCID] = db_handler.get_docid(path_parser.get_id());

	operation_ends = std::chrono::system_clock::now();

	write_http_response(HTTP_STATUS_OK, response);
}


void
HttpClient::delete_document_view(enum http_method method, Command)
{
	L_CALL("HttpClient::delete_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	std::string doc_id(path_parser.get_id());

	operation_begins = std::chrono::system_clock::now();

	enum http_status status_code;
	MsgPack response;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN, method);

	db_handler.delete_document(doc_id, query_field->commit);
	operation_ends = std::chrono::system_clock::now();
	status_code = HTTP_STATUS_OK;
	response[RESPONSE_DELETE] = {
		{ ID_FIELD_NAME, doc_id },
		{ RESPONSE_COMMIT,  query_field->commit }
	};

	Stats::cnt().add("del", std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count());
	L_TIME("Deletion took %s", delta_string(operation_begins, operation_ends).c_str());

	write_http_response(status_code, response);
}


void
HttpClient::index_document_view(enum http_method method, Command)
{
	L_CALL("HttpClient::index_document_view()");

	std::string doc_id;
	enum http_status status_code = HTTP_STATUS_BAD_REQUEST;

	if (method == HTTP_POST) {
		auto uuid = generator.newGuid(XapiandManager::manager->opts.uuid_compact);
		doc_id = Unserialise::uuid(uuid.serialise(), XapiandManager::manager->opts.uuid_repr);
	} else {
		doc_id = path_parser.get_id();
	}

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	operation_begins = std::chrono::system_clock::now();

	auto body_ = get_decoded_body();
	MsgPack response;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	bool stored = true;
	response = db_handler.index(doc_id, stored, body_.second, query_field->commit, body_.first).second;

	operation_ends = std::chrono::system_clock::now();

	Stats::cnt().add("index", std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count());
	L_TIME("Indexing took %s", delta_string(operation_begins, operation_ends).c_str());

	status_code = HTTP_STATUS_OK;
	if (response.find(ID_FIELD_NAME) == response.end()) {
		response[ID_FIELD_NAME] = doc_id;
	}
	response[RESPONSE_COMMIT] = query_field->commit;

	write_http_response(status_code, response);
}


void
HttpClient::write_schema_view(enum http_method method, Command)
{
	L_CALL("HttpClient::write_schema_view()");

	enum http_status status_code = HTTP_STATUS_BAD_REQUEST;

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	db_handler.write_schema(get_decoded_body().second, method == HTTP_PUT);

	operation_ends = std::chrono::system_clock::now();

	MsgPack response;
	status_code = HTTP_STATUS_OK;
	response = db_handler.get_schema()->get_full(true);

	write_http_response(status_code, response);
}


void
HttpClient::update_document_view(enum http_method method, Command)
{
	L_CALL("HttpClient::update_document_view()");

	endpoints_maker(2s);
	query_field_maker(QUERY_FIELD_COMMIT);

	std::string doc_id(path_parser.get_id());
	enum http_status status_code = HTTP_STATUS_BAD_REQUEST;

	operation_begins = std::chrono::system_clock::now();

	auto body_ = get_decoded_body();
	MsgPack response;
	db_handler.reset(endpoints, DB_WRITABLE | DB_SPAWN | DB_INIT_REF, method);
	if (method == HTTP_PATCH) {
		response = db_handler.patch(doc_id, body_.second, query_field->commit, body_.first).second;
	} else {
		bool stored = true;
		response = db_handler.merge(doc_id, stored, body_.second, query_field->commit, body_.first).second;
	}

	operation_ends = std::chrono::system_clock::now();

	Stats::cnt().add("patch", std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count());
	L_TIME("Updating took %s", delta_string(operation_begins, operation_ends).c_str());

	status_code = HTTP_STATUS_OK;
	if (response.find(ID_FIELD_NAME) == response.end()) {
		response[ID_FIELD_NAME] = doc_id;
	}
	response[RESPONSE_COMMIT] = query_field->commit;

	write_http_response(status_code, response);
}


void
HttpClient::meta_view(enum http_method method, Command)
{
	L_CALL("HttpClient::meta_view()");

	enum http_status status_code = HTTP_STATUS_OK;

	endpoints_maker(2s);

	MsgPack response;
	db_handler.reset(endpoints, DB_OPEN, method);

	std::string selector;
	auto key = path_parser.get_pmt();
	if (key.empty()) {
		std::string cmd = path_parser.get_cmd();
		auto needle = cmd.find_first_of("|{", 1);  // to get selector, find first of either | or {
		if (needle != std::string::npos) {
			selector = cmd.substr(needle);
		}
	} else {
		auto needle = key.find_first_of("|{", 1);  // to get selector, find first of either | or {
		if (needle != std::string::npos) {
			selector = key.substr(needle);
			key = key.substr(0, needle);
		}
	}

	if (key.empty()) {
		response = MsgPack(MsgPack::Type::MAP);
		for (auto& _key : db_handler.get_metadata_keys()) {
			auto metadata = db_handler.get_metadata(_key);
			if (!metadata.empty()) {
				response[_key] = MsgPack::unserialise(metadata);
			}
		}
	} else {
		auto metadata = db_handler.get_metadata(key);
		if (metadata.empty()) {
			status_code = HTTP_STATUS_NOT_FOUND;
		} else {
			response = MsgPack::unserialise(metadata);
		}
	}

	if (!selector.empty()) {
		response = response.select(selector);
	}

	write_http_response(status_code, response);
}


void
HttpClient::write_meta_view(enum http_method, Command)
{
	L_CALL("HttpClient::write_meta_view()");

	enum http_status status_code = HTTP_STATUS_BAD_REQUEST;

	MsgPack response;

	write_http_response(status_code, response);
}


void
HttpClient::update_meta_view(enum http_method, Command)
{
	L_CALL("HttpClient::update_meta_view()");

	enum http_status status_code = HTTP_STATUS_BAD_REQUEST;

	MsgPack response;

	write_http_response(status_code, response);
}


void
HttpClient::info_view(enum http_method method, Command)
{
	L_CALL("HttpClient::info_view()");

	MsgPack response;

	endpoints_maker(1s);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_OPEN, method);

	// There's no path, we're at root so we get the server's info
	if (!path_parser.off_pth) {
		XapiandManager::manager->server_status(response[RESPONSE_SERVER_INFO]);
	}

	response[RESPONSE_DATABASE_INFO] = db_handler.get_database_info();

	// Info about a specific document was requested
	if (path_parser.off_pmt) {
		response[RESPONSE_DOCUMENT_INFO] = db_handler.get_document_info(path_parser.get_pmt());
	}

	operation_ends = std::chrono::system_clock::now();

	write_http_response(HTTP_STATUS_OK, response);
}


void
HttpClient::nodes_view(enum http_method, Command)
{
	L_CALL("HttpClient::nodes_view()");

	path_parser.next();
	if (path_parser.next() != PathParser::State::END) {
		write_status_response(HTTP_STATUS_NOT_FOUND);
		return;
	}

	if (path_parser.len_pth || path_parser.len_pmt || path_parser.len_ppmt) {
		write_status_response(HTTP_STATUS_NOT_FOUND);
		return;
	}

	MsgPack nodes(MsgPack::Type::MAP);

	// FIXME: Get all nodes from cluster database:
	auto local_node_ = local_node.load();
	nodes[serialise_node_id(local_node_->id)] = {
		{ RESPONSE_NAME, local_node_->name },
	};

	write_http_response(HTTP_STATUS_OK, {
		{ RESPONSE_CLUSTER_NAME, XapiandManager::manager->opts.cluster_name },
		{ RESPONSE_NODES, nodes },
	});
}


void
HttpClient::touch_view(enum http_method method, Command)
{
	L_CALL("HttpClient::touch_view()");

	endpoints_maker(1s);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_WRITABLE|DB_SPAWN, method);

	db_handler.reopen();  // Ensure touch.

	operation_ends = std::chrono::system_clock::now();

	MsgPack response;
	response[RESPONSE_ENDPOINT] = endpoints.to_string();

	write_http_response(HTTP_STATUS_CREATED, response);
}


void
HttpClient::schema_view(enum http_method method, Command)
{
	L_CALL("HttpClient::schema_view()");

	std::string selector;
	auto cmd = path_parser.get_cmd();
	auto needle = cmd.find_first_of("|{", 1);  // to get selector, find first of either | or {
	if (needle != std::string::npos) {
		selector = cmd.substr(needle);
	}

	endpoints_maker(1s);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_OPEN, method);

	auto schema = db_handler.get_schema()->get_full(true);
	if (!selector.empty()) {
		schema = schema.select(selector);
	}

	operation_ends = std::chrono::system_clock::now();

	write_http_response(HTTP_STATUS_OK, schema);
}


#if XAPIAND_DATABASE_WAL
void
HttpClient::wal_view(enum http_method method, Command)
{
	L_CALL("HttpClient::wal_view()");

	endpoints_maker(1s);

	operation_begins = std::chrono::system_clock::now();

	db_handler.reset(endpoints, DB_OPEN, method);

	auto repr = db_handler.repr_wal(0, -1);

	operation_ends = std::chrono::system_clock::now();

	write_http_response(HTTP_STATUS_OK, repr);
}
#endif


void
HttpClient::search_view(enum http_method method, Command)
{
	L_CALL("HttpClient::search_view()");

	std::string selector;
	auto id = path_parser.get_id();
	if (id.empty()) {
		auto cmd = path_parser.get_cmd();
		auto needle = cmd.find_first_of("|{", 1);  // to get selector, find first of either | or {
		if (needle != std::string::npos) {
			selector = cmd.substr(needle);
		}
	} else {
		auto needle = id.find_first_of("|{", 1);  // to get selector, find first of either | or {
		if (needle != std::string::npos) {
			selector = id.substr(needle);
			id = id.substr(0, needle);
		}
	}

	endpoints_maker(1s);
	query_field_maker(id.empty() ? QUERY_FIELD_SEARCH : QUERY_FIELD_ID);

	bool single = !id.empty() && !isRange(id);
	Xapian::docid did = 0;

	MSet mset;
	std::vector<std::string> suggestions;

	int db_flags = DB_OPEN;

	operation_begins = std::chrono::system_clock::now();

	MsgPack aggregations;
	try {
		db_handler.reset(endpoints, db_flags, method);

		if (query_field->volatile_) {
			if (endpoints.size() != 1) {
				enum http_status error_code = HTTP_STATUS_BAD_REQUEST;
				MsgPack err_response = {
					{ RESPONSE_STATUS, (int)error_code },
					{ RESPONSE_MESSAGE, { "Expecting exactly one index with volatile" } }
				};
				write_http_response(error_code, err_response);
				return;
			}
			try {
				// Try the commit may have been done by some other thread and volatile should always get the latest.
				DatabaseHandler commit_handler(endpoints, db_flags | DB_COMMIT, method);
				commit_handler.commit();
			} catch (const CheckoutErrorCommited&) { }
		}

		db_handler.reopen();  // Ensure the database is current.

		if (single) {
			try {
				did = db_handler.get_docid(id);
			} catch (const DocNotFoundError&) { }
		} else {
			if (body.empty()) {
				mset = db_handler.get_mset(*query_field, nullptr, nullptr, suggestions);
			} else {
				auto body_ = get_decoded_body();
				AggregationMatchSpy aggs(body_.second, db_handler.get_schema());
				mset = db_handler.get_mset(*query_field, &body_.second, &aggs, suggestions);
				aggregations = aggs.get_aggregation().at(AGGREGATION_AGGS);
			}
		}
	} catch (const CheckoutError&) {
		/* At the moment when the endpoint does not exist and it is chunck it will return 200 response
		 * with zero matches this behavior may change in the future for instance ( return 404 ) */
		if (single) {
			throw;
		}
	}

	L_SEARCH("Suggested queries: %s", [&suggestions]() {
		MsgPack res(MsgPack::Type::ARRAY);
		for (const auto& suggestion : suggestions) {
			res.push_back(suggestion);
		}
		return res;
	}().to_string().c_str());

	int rc = 0;
	auto total_count = did ? 1 : mset.size();

	if (single && !total_count) {
		enum http_status error_code = HTTP_STATUS_NOT_FOUND;
		MsgPack err_response = {
			{ RESPONSE_STATUS, (int)error_code },
			{ RESPONSE_MESSAGE, http_status_str(error_code) }
		};
		write_http_response(error_code, err_response);
	} else {
		auto type_encoding = resolve_encoding();
		if (type_encoding == Encoding::unknown) {
			enum http_status error_code = HTTP_STATUS_NOT_ACCEPTABLE;
			MsgPack err_response = {
				{ RESPONSE_STATUS, (int)error_code },
				{ RESPONSE_MESSAGE, { "Response encoding gzip, deflate or identity not provided in the Accept-Encoding header" } }
			};
			write_http_response(error_code, err_response);
			L_SEARCH("ABORTED SEARCH");
			return;
		}

		bool indent_chunk = false;
		std::string first_chunk;
		std::string last_chunk;
		std::string sep_chunk;
		std::string eol_chunk;

		std::string l_first_chunk;
		std::string l_last_chunk;
		std::string l_eol_chunk;
		std::string l_sep_chunk;

		// Get default content type to return.
		auto ct_type = resolve_ct_type(MSGPACK_CONTENT_TYPE);

		if (!single) {
			MsgPack basic_query({
				{ RESPONSE_TOTAL_COUNT, total_count},
				{ RESPONSE_MATCHES_ESTIMATED, mset.get_matches_estimated()},
				{ RESPONSE_HITS, MsgPack(MsgPack::Type::ARRAY) },
			});
			MsgPack basic_response;
			if (aggregations) {
				basic_response[RESPONSE_AGGREGATIONS] = aggregations;
			}
			basic_response[RESPONSE_QUERY] = basic_query;

			if (Logging::log_level > LOG_DEBUG) {
				l_first_chunk = basic_response.to_string(4);
				l_first_chunk = l_first_chunk.substr(0, l_first_chunk.size() - 9) + "\n";
				l_last_chunk = "        ]\n    }\n}";
				l_eol_chunk = "\n";
				l_sep_chunk = ",";
			}

			if (is_acceptable_type(msgpack_type, ct_type) || is_acceptable_type(x_msgpack_type, ct_type)) {
				first_chunk = basic_response.serialise();
				// Remove zero size array and manually add the msgpack array header
				first_chunk.pop_back();
				if (total_count < 16) {
					first_chunk.push_back(static_cast<char>(0x90u | total_count));
				} else if (total_count < 65536) {
					char buf[3];
					buf[0] = static_cast<char>(0xdcu); _msgpack_store16(&buf[1], static_cast<uint16_t>(total_count));
					first_chunk.append(std::string(buf, 3));
				} else {
					char buf[5];
					buf[0] = static_cast<char>(0xddu); _msgpack_store32(&buf[1], static_cast<uint32_t>(total_count));
					first_chunk.append(std::string(buf, 5));
				}
			} else if (is_acceptable_type(json_type, ct_type)) {
				first_chunk = basic_response.to_string(indent);
				if (indent != -1) {
					first_chunk = first_chunk.substr(0, first_chunk.size() - (indent * 2) - 1) + "\n";
					last_chunk = std::string(indent * 2, ' ') + "]\n" + std::string(indent, ' ') + "}\n}";
					eol_chunk = "\n";
					sep_chunk = ",";
					indent_chunk = true;
				} else {
					first_chunk = first_chunk.substr(0, first_chunk.size() - 3);
					last_chunk = "]}}";
					sep_chunk = ",";
				}
			} else {
				enum http_status error_code = HTTP_STATUS_NOT_ACCEPTABLE;
				MsgPack err_response = {
					{ RESPONSE_STATUS, (int)error_code },
					{ RESPONSE_MESSAGE, { "Response type application/msgpack or application/json not provided in the Accept header" } }
				};
				write_http_response(error_code, err_response);
				L_SEARCH("ABORTED SEARCH");
				return;
			}
		}

		std::string buffer;
		std::string l_buffer;
		const auto m_e = mset.end();
		for (auto m = mset.begin(); did || m != m_e; ++rc, ++m) {
			auto document = db_handler.get_document(did ? did : *m);

			const auto data = document.get_data();
			if (data.empty()) {
				continue;
			}

			MsgPack obj;
			if (single) {
				// Figure out the document's ContentType.
				std::string ct_type_str = get_data_content_type(data);

				// If there's a ContentType in the blob store or in the ContentType's field
				// in the object, try resolving to it (or otherwise don't touch the current ct_type)
				auto blob_ct_type = resolve_ct_type(ct_type_str);
				if (blob_ct_type.empty()) {
					if (ct_type.empty()) {
						// No content type could be resolved, return NOT ACCEPTABLE.
						enum http_status error_code = HTTP_STATUS_NOT_ACCEPTABLE;
						MsgPack err_response = {
							{ RESPONSE_STATUS, (int)error_code },
							{ RESPONSE_MESSAGE, { "Response type " + ct_type_str + " not provided in the Accept header" } }
						};
						write_http_response(error_code, err_response);
						L_SEARCH("ABORTED SEARCH");
						return;
					}
				} else {
					// Returns blob_data in case that type is unkown
					ct_type = blob_ct_type;

					auto blob = document.get_blob();
					auto blob_data = unserialise_string_at(STORED_BLOB_DATA, blob);
					if (Logging::log_level > LOG_DEBUG) {
						if (Logging::log_level > LOG_DEBUG + 1 && ct_type.first == "image") {
							// From [https://www.iterm2.com/documentation-images.html]
							std::string b64_name = cppcodec::base64_rfc4648::encode("");
							std::string b64_response_body = cppcodec::base64_rfc4648::encode(blob_data);
							response_body = format_string("\033]1337;File=name=%s;inline=1;size=%d;width=20%%:",
								b64_name.c_str(),
								b64_response_body.size());
							response_body += b64_response_body.c_str();
							response_body += '\a';
						} else if (!blob_data.empty()) {
							response_body = "<blob " + bytes_string(blob_data.size()) + ">";
						}
					}
					if (type_encoding != Encoding::none) {
						auto encoded = encoding_http_response(type_encoding, blob_data, false, true, true);
						if (!encoded.empty() && encoded.size() <= blob_data.size()) {
							write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_BODY_RESPONSE, parser.http_major, parser.http_minor, 0, 0, encoded, ct_type.first + "/" + ct_type.second, readable_encoding(type_encoding)));
						} else {
							write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_BODY_RESPONSE, parser.http_major, parser.http_minor, 0, 0, blob_data, ct_type.first + "/" + ct_type.second, readable_encoding(Encoding::identity)));
						}
					} else {
						write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_BODY_RESPONSE, parser.http_major, parser.http_minor, 0, 0, blob_data, ct_type.first + "/" + ct_type.second));
					}
					return;
				}
			}

			if (obj.is_undefined()) {
				obj = MsgPack::unserialise(split_data_obj(data));
			}

			if (obj.find(ID_FIELD_NAME) == obj.end()) {
				obj[ID_FIELD_NAME] = document.get_value(ID_FIELD_NAME);
			}

			// Detailed info about the document:
			obj[RESPONSE_DOCID] = document.get_docid();
			if (!single) {
				obj[RESPONSE_RANK] = m.get_rank();
				obj[RESPONSE_WEIGHT] = m.get_weight();
				obj[RESPONSE_PERCENT] = m.get_percent();
				// int subdatabase = (document.get_docid() - 1) % endpoints.size();
				// auto endpoint = endpoints[subdatabase];
				// obj[RESPONSE_ENDPOINT] = endpoint.to_string();
			}

			if (!selector.empty()) {
				obj = obj.select(selector);
			}

			if (Logging::log_level > LOG_DEBUG) {
				if (single) {
					response_body += obj.to_string(4);
				} else {
					if (rc == 0) {
						response_body += l_first_chunk;
					}
					if (!l_buffer.empty()) {
						response_body += indent_string(l_buffer, ' ', 3 * 4) + l_sep_chunk + l_eol_chunk;
					}
					l_buffer = obj.to_string(4);
				}
			}

			auto result = serialize_response(obj, ct_type, indent);
			if (single) {
				if (type_encoding != Encoding::none) {
					auto encoded = encoding_http_response(type_encoding, result.first, false, true, true);
					if (!encoded.empty() && encoded.size() <= result.first.size()) {
						write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, encoded, result.second, readable_encoding(type_encoding)));
					} else {
						write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second, readable_encoding(Encoding::identity)));
					}
				} else {
					write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second));
				}
			} else {
				if (rc == 0) {
					if (type_encoding != Encoding::none) {
						auto encoded = encoding_http_response(type_encoding, first_chunk, true, true, false);
						write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_CHUNKED_RESPONSE | HTTP_TOTAL_COUNT_RESPONSE | HTTP_MATCHES_ESTIMATED_RESPONSE, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type.first + "/" + ct_type.second, readable_encoding(type_encoding)));
						if (!encoded.empty()) {
							write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, encoded));
						}
					} else {
						write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CHUNKED_RESPONSE | HTTP_TOTAL_COUNT_RESPONSE | HTTP_MATCHES_ESTIMATED_RESPONSE, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type.first + "/" + ct_type.second));
						if (!first_chunk.empty()) {
							write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, first_chunk));
						}
					}
				}

				if (!buffer.empty()) {
					auto indented_buffer = (indent_chunk ? indent_string(buffer, ' ', 3 * indent) : buffer) + sep_chunk + eol_chunk;
					if (type_encoding != Encoding::none) {
						auto encoded = encoding_http_response(type_encoding, indented_buffer, true, false, false);
						if (!encoded.empty()) {
							write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, encoded));
						}
					} else {
						if (!indented_buffer.empty()) {
							write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, indented_buffer));
						}
					}
				}
				buffer = result.first;
			}

			if (single) break;
		}

		if (Logging::log_level > LOG_DEBUG) {
			if (!single) {
				if (rc == 0) {
					response_body += l_first_chunk;
				}

				if (!l_buffer.empty()) {
					response_body += indent_string(l_buffer, ' ', 3 * 4) + l_sep_chunk + l_eol_chunk;
				}

				response_body += l_last_chunk;
			}
		}

		if (!single) {
			if (rc == 0) {
				if (type_encoding != Encoding::none) {
					auto encoded = encoding_http_response(type_encoding, first_chunk, true, true, false);
					write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE | HTTP_CHUNKED_RESPONSE | HTTP_TOTAL_COUNT_RESPONSE | HTTP_MATCHES_ESTIMATED_RESPONSE, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type.first + "/" + ct_type.second, readable_encoding(type_encoding)));
					if (!encoded.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, encoded));
					}
				} else {
					write(http_response(HTTP_STATUS_OK, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CHUNKED_RESPONSE | HTTP_TOTAL_COUNT_RESPONSE | HTTP_MATCHES_ESTIMATED_RESPONSE, parser.http_major, parser.http_minor, mset.size(), mset.get_matches_estimated(), "", ct_type.first + "/" + ct_type.second));
					if (!first_chunk.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, first_chunk));
					}
				}
			}

			if (!buffer.empty()) {
				auto indented_buffer = (indent_chunk ? indent_string(buffer, ' ', 3 * indent) : buffer) + sep_chunk + eol_chunk;
				if (type_encoding != Encoding::none) {
					auto encoded = encoding_http_response(type_encoding, indented_buffer, true, false, false);
					if (!encoded.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, encoded));
					}
				} else {
					if (!indented_buffer.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, indented_buffer));
					}
				}
			}

			if (!last_chunk.empty()) {
				if (type_encoding != Encoding::none) {
					auto encoded = encoding_http_response(type_encoding, last_chunk, true, false, true);
					if (!encoded.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, encoded));
					}
				} else {
					if (!last_chunk.empty()) {
						write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE, 0, 0, 0, 0, last_chunk));
					}
				}
			}

			write(http_response(HTTP_STATUS_OK, HTTP_CHUNKED_RESPONSE | HTTP_BODY_RESPONSE));
		}
	}

	operation_ends = std::chrono::system_clock::now();

	Stats::cnt().add("search", std::chrono::duration_cast<std::chrono::nanoseconds>(operation_ends - operation_begins).count());
	L_TIME("Searching took %s", delta_string(operation_begins, operation_ends).c_str());

	L_SEARCH("FINISH SEARCH");
}


void
HttpClient::write_status_response(enum http_status status, const std::string& message)
{
	L_CALL("HttpClient::write_status_response()");

	write_http_response(status, {
		{ RESPONSE_STATUS, (int)status },
		{ RESPONSE_MESSAGE, message.empty() ? MsgPack({ http_status_str(status) }) : split_string(message, '\n') }
	});
}


void
HttpClient::stats_view(enum http_method, Command)
{
	L_CALL("HttpClient::stats_view()");

	MsgPack response(MsgPack::Type::ARRAY);
	query_field_maker(QUERY_FIELD_TIME | QUERY_FIELD_PERIOD);
	XapiandManager::manager->get_stats_time(response, query_field->time, query_field->period);
	write_http_response(HTTP_STATUS_OK, response);
}


HttpClient::Command
HttpClient::url_resolve()
{
	L_CALL("HttpClient::url_resolve()");

	struct http_parser_url u;
	std::string b = repr(path, true, false);

	L_HTTP("URL: %s", b.c_str());

	if (http_parser_parse_url(path.data(), path.size(), 0, &u) == 0) {
		L_HTTP_PROTO_PARSER("HTTP parsing done!");

		if (u.field_set & (1 << UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			std::unique_ptr<char[]> path_buf_ptr(new char[path_size + 1]);
			auto path_buf_str = path_buf_ptr.get();
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
			if (query_parser.len) {
				try {
					indent = Serialise::boolean(query_parser.get()) == "t" ? 4 : -1;
				} catch (const Exception&) { }
			} else if (indent == -1) {
				indent = 4;
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
			auto cmd = path_parser.get_cmd();
			auto needle = cmd.find_first_of("|{", 1);  // to get selector, find first of either | or {
			return static_cast<Command>(xxh64::hash(lower_string(cmd.substr(0, needle))));
		}

	} else {
		L_HTTP_PROTO_PARSER("Parsing not done");
		// Bad query
		return Command::BAD_QUERY;
	}
}


void
HttpClient::endpoints_maker(std::chrono::duration<double, std::milli> timeout)
{
	endpoints.clear();

	PathParser::State state;
	while ((state = path_parser.next()) < PathParser::State::END) {
		_endpoint_maker(timeout);
	}
}


void
HttpClient::_endpoint_maker(std::chrono::duration<double, std::milli> timeout)
{
	bool has_node_name = false;

	std::string ns;
	if (path_parser.off_nsp) {
		ns = path_parser.get_nsp() + "/";
		if (startswith(ns, "/")) { /* ns without slash */
			ns = ns.substr(1);
		}
	}

	std::string _path;
	if (path_parser.off_pth) {
		_path = path_parser.get_pth();
		if (startswith(_path, "/")) { /* path without slash */
			_path.erase(0, 1);
		}
	}

	std::string index_path;
	if (ns.empty() && _path.empty()) {
		index_path = ".";
	} else if (ns.empty()) {
		index_path = _path;
	} else if (_path.empty()) {
		index_path = ns;
	} else {
		index_path = ns + "/" + _path;
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
			THROW(Error, "Node %s not found", node_name.c_str());
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
	L_HTTP("Endpoint: -> %s", endpoints.to_string().c_str());
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
			L_SEARCH("query=%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("q") != -1) {
			L_SEARCH("query=%s", query_parser.get().c_str());
			query_field->query.push_back(query_parser.get());
		}
		query_parser.rewind();

		while (query_parser.next("sort") != -1) {
			query_field->sort.push_back(query_parser.get());
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

	if (flag & QUERY_FIELD_PERIOD) {
		if (query_parser.next("period") != -1) {
			query_field->period = query_parser.get();
		} else {
			query_field->period = "1m";
		}
		query_parser.rewind();
	}
}


void
HttpClient::log_request()
{
	std::string request_prefix = " 🌎  ";

	auto request_headers_color = &NO_COL;
	auto request_head_color = &NO_COL;
	auto request_body_color = &NO_COL;

	switch (HTTP_PARSER_METHOD(&parser)) {
		case HTTP_OPTIONS:
			// rgb(13, 90, 167)
			request_headers_color = &rgba(30, 77, 124, 0.6);
			request_head_color = &brgb(30, 77, 124);
			request_body_color = &rgb(30, 77, 124);
			break;
		case HTTP_HEAD:
			// rgb(144, 18, 254)
			request_headers_color = &rgba(100, 64, 131, 0.6);
			request_head_color = &brgb(100, 64, 131);
			request_body_color = &rgb(100, 64, 131);
			break;
		case HTTP_GET:
			// rgb(101, 177, 251)
			request_headers_color = &rgba(34, 113, 191, 0.6);
			request_head_color = &brgb(34, 113, 191);
			request_body_color = &rgb(34, 113, 191);
			break;
		case HTTP_POST:
			// rgb(80, 203, 146)
			request_headers_color = &rgba(55, 100, 79, 0.6);
			request_head_color = &brgb(55, 100, 79);
			request_body_color = &rgb(55, 100, 79);
			break;
		case HTTP_PATCH:
			// rgb(88, 226, 194)
			request_headers_color = &rgba(51, 136, 116, 0.6);
			request_head_color = &brgb(51, 136, 116);
			request_body_color = &rgb(51, 136, 116);
			break;
		case HTTP_MERGE:
			// rgb(88, 226, 194)
			request_headers_color = &rgba(51, 136, 116, 0.6);
			request_head_color = &brgb(51, 136, 116);
			request_body_color = &rgb(51, 136, 116);
			break;
		case HTTP_PUT:
			// rgb(250, 160, 63)
			request_headers_color = &rgba(158, 95, 28, 0.6);
			request_head_color = &brgb(158, 95, 28);
			request_body_color = &rgb(158, 95, 28);
			break;
		case HTTP_DELETE:
			// rgb(246, 64, 68)
			request_headers_color = &rgba(151, 31, 34, 0.6);
			request_head_color = &brgb(151, 31, 34);
			request_body_color = &rgb(151, 31, 34);
			break;
		default:
			break;
	};

	auto request = *request_head_color + request_head + "\n" + *request_headers_color + request_headers + *request_body_color + request_body;
	L(LOG_DEBUG + 1, NO_COL, "%s%s", request_prefix.c_str(), indent_string(request, ' ', 4, false).c_str());
}


void
HttpClient::log_response()
{
	std::string response_prefix = " 💊  ";
	auto response_headers_color = &NO_COL;
	auto response_head_color = &NO_COL;
	auto response_body_color = &NO_COL;

	if ((int)response_status >= 200 && (int)response_status <= 299) {
		response_headers_color = &rgba(68, 136, 68, 0.6);
		response_head_color = &brgb(68, 136, 68);
		response_body_color = &rgb(68, 136, 68);
	} else if ((int)response_status >= 300 && (int)response_status <= 399) {
		response_prefix = " 💫  ";
		response_headers_color = &rgba(68, 136, 120, 0.6);
		response_head_color = &brgb(68, 136, 120);
		response_body_color = &rgb(68, 136, 120);
	} else if ((int)response_status == 404) {
		response_prefix = " 🕸  ";
		response_headers_color = &rgba(116, 100, 77, 0.6);
		response_head_color = &brgb(116, 100, 77);
		response_body_color = &rgb(116, 100, 77);
	} else if ((int)response_status >= 400 && (int)response_status <= 499) {
		response_prefix = " 💥  ";
		response_headers_color = &rgba(183, 70, 17, 0.6);
		response_head_color = &brgb(183, 70, 17);
		response_body_color = &rgb(183, 70, 17);
	} else if ((int)response_status >= 500 && (int)response_status <= 599) {
		response_prefix = " 🔥  ";
		response_headers_color = &rgba(190, 30, 10, 0.6);
		response_head_color = &brgb(190, 30, 10);
		response_body_color = &rgb(190, 30, 10);
	}

	auto response = *response_head_color + response_head + "\n" + *response_headers_color + response_headers + *response_body_color + response_body;
	L(LOG_DEBUG + 1, NO_COL, "%s%s", response_prefix.c_str(), indent_string(response, ' ', 4, false).c_str());
}


void
HttpClient::clean_http_request()
{
	L_CALL("HttpClient::clean_http_request()");

	response_ends = std::chrono::system_clock::now();

	response_log.load()->clear();
	if (parser.http_errno) {
		if (!response_logged.exchange(true)) L(LOG_ERR, LIGHT_RED, "HTTP parsing error (%s): %s", http_errno_name(HTTP_PARSER_ERRNO(&parser)), http_errno_description(HTTP_PARSER_ERRNO(&parser)));
	} else {
		int priority = LOG_DEBUG;
		auto color = &WHITE;

		if ((int)response_status >= 200 && (int)response_status <= 299) {
			color = &GREY;
		} else if ((int)response_status >= 300 && (int)response_status <= 399) {
			color = &STEEL_BLUE;
		} else if ((int)response_status >= 400 && (int)response_status <= 499) {
			color = &SADDLE_BROWN;
			priority = LOG_INFO;
		} else if ((int)response_status >= 500 && (int)response_status <= 599) {
			color = &LIGHT_PURPLE;
			priority = LOG_ERR;
		}
		if (!response_logged.exchange(true)) {
			if (Logging::log_level > LOG_DEBUG) {
				log_response();
			}
			L(priority, *color, "\"%s\" %d %s %s", request_head.c_str(), (int)response_status, bytes_string(response_size).c_str(), delta_string(request_begins, response_ends).c_str());
		}
	}

	if (Logging::log_level > LOG_DEBUG) {
		request_head.clear();
		request_headers.clear();
		request_body.clear();
		response_head.clear();
		response_headers.clear();
		response_body.clear();
	}
	path.clear();
	body.clear();
	decoded_body.first.clear();
	header_name.clear();
	header_value.clear();
	content_type.clear();
	content_length.clear();
	host.clear();
	response_status = HTTP_STATUS_OK;
	response_size = 0;

	indent = -1;
	query_field.reset();
	path_parser.clear();
	query_parser.clear();
	accept_set.clear();

	request_begining = true;
	L_TIME("Full request took %s, response took %s", delta_string(request_begins, response_ends).c_str(), delta_string(response_begins, response_ends).c_str());

	set_idle();

	http_parser_init(&parser, HTTP_REQUEST);
}


void
HttpClient::set_idle()
{
	L_CALL("HttpClient::set_idle()");

	// auto old_response_log = response_log.exchange(L_DELAYED(true, 300s, LOG_WARNING, PURPLE, "Client idle for too long...").release());
	// old_response_log->clear();

	idle = true;

	if (shutting_down && write_queue.empty()) {
		L_WARNING("Programmed shut down!");
		destroy();
		detach();
	}
}


ct_type_t
HttpClient::resolve_ct_type(std::string ct_type_str)
{
	L_CALL("HttpClient::resolve_ct_type(%s)", repr(ct_type_str).c_str());

	if (ct_type_str == JSON_CONTENT_TYPE || ct_type_str == MSGPACK_CONTENT_TYPE || ct_type_str == X_MSGPACK_CONTENT_TYPE) {
		if (is_acceptable_type(get_acceptable_type(json_type), json_type)) {
			ct_type_str = JSON_CONTENT_TYPE;
		} else if (is_acceptable_type(get_acceptable_type(msgpack_type), msgpack_type)) {
			ct_type_str = MSGPACK_CONTENT_TYPE;
		} else if (is_acceptable_type(get_acceptable_type(x_msgpack_type), x_msgpack_type)) {
			ct_type_str = X_MSGPACK_CONTENT_TYPE;
		}
	}
	ct_type_t ct_type(ct_type_str);

	std::vector<ct_type_t> ct_types;
	if (ct_type == json_type || ct_type == msgpack_type || ct_type == x_msgpack_type) {
		ct_types = msgpack_serializers;
	} else {
		ct_types.push_back(ct_type);
	}

	const auto& accepted_type = get_acceptable_type(ct_types);
	const auto accepted_ct_type = is_acceptable_type(accepted_type, ct_types);
	if (!accepted_ct_type) {
		return no_type;
	}

	return *accepted_ct_type;
}


const ct_type_t*
HttpClient::is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t& ct_type)
{
	L_CALL("HttpClient::is_acceptable_type(%s, %s)", repr(ct_type_pattern.to_string()), repr(ct_type.to_string()));

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


const ct_type_t*
HttpClient::is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<ct_type_t>& ct_types)
{
	L_CALL("HttpClient::is_acceptable_type((%s, %s, <ct_types>)", repr(ct_type_pattern.to_string()), repr(ct_type.to_string()));

	for (auto& ct_type : ct_types) {
		if (is_acceptable_type(ct_type_pattern, ct_type)) {
			return &ct_type;
		}
	}
	return nullptr;
}


template <typename T>
const ct_type_t&
HttpClient::get_acceptable_type(const T& ct)
{
	L_CALL("HttpClient::get_acceptable_type()");

	if (accept_set.empty()) {
		if (!content_type.empty()) accept_set.insert(std::tuple<double, int, ct_type_t, unsigned>(1, 0, content_type, 0));
		accept_set.insert(std::make_tuple(1, 1, ct_type_t(std::string(1, '*'), std::string(1, '*')), 0));
	}
	for (const auto& accept : accept_set) {
		if (is_acceptable_type(std::get<2>(accept), ct)) {
			auto _indent = std::get<3>(accept);
			if (_indent != -1) {
				indent = _indent;
			}
			return std::get<2>(accept);
		}
	}
	const auto& accept = *accept_set.begin();
	auto _indent = std::get<3>(accept);
	if (_indent != -1) {
		indent = _indent;
	}
	return std::get<2>(accept);
}


ct_type_t
HttpClient::serialize_response(const MsgPack& obj, const ct_type_t& ct_type, int _indent, bool serialize_error)
{
	L_CALL("HttpClient::serialize_response(%s, %s, %u, %s)", repr(obj.to_string()).c_str(), repr(ct_type.first + "/" + ct_type.second).c_str(), _indent, serialize_error ? "true" : "false");

	if (is_acceptable_type(ct_type, json_type)) {
		return ct_type_t(obj.to_string(_indent), json_type.first + "/" + json_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, msgpack_type)) {
		return ct_type_t(obj.serialise(), msgpack_type.first + "/" + msgpack_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, x_msgpack_type)) {
		return ct_type_t(obj.serialise(), x_msgpack_type.first + "/" + x_msgpack_type.second + "; charset=utf-8");
	} else if (is_acceptable_type(ct_type, html_type)) {
		std::function<std::string(const msgpack::object&)> html_serialize = serialize_error ? msgpack_to_html_error : msgpack_to_html;
		return ct_type_t(obj.external(html_serialize), html_type.first + "/" + html_type.second + "; charset=utf-8");
	} /* else if (is_acceptable_type(ct_type, text_type)) {
		error:
			{{ RESPONSE_STATUS }} - {{ RESPONSE_MESSAGE }}

		obj:
			{{ key1 }}: {{ val1 }}
			{{ key2 }}: {{ val2 }}
			...

		array:
			{{ val1 }}, {{ val2 }}, ...
	} */
	THROW(SerialisationError, "Type is not serializable");
}


void
HttpClient::write_http_response(enum http_status status, const MsgPack& response)
{
	L_CALL("HttpClient::write_http_response()");

	auto type_encoding = resolve_encoding();
	if (type_encoding == Encoding::unknown && status != HTTP_STATUS_NOT_ACCEPTABLE) {
		enum http_status error_code = HTTP_STATUS_NOT_ACCEPTABLE;
		MsgPack err_response = {
			{ RESPONSE_STATUS, (int)error_code },
			{ RESPONSE_MESSAGE, { "Response encoding gzip, deflate or identity not provided in the Accept-Encoding header" } }
		};
		write_http_response(error_code, err_response);
		return;
	}

	if (response.is_undefined()) {
		write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE, parser.http_major, parser.http_minor));
		return;
	}

	ct_type_t ct_type(content_type);
	std::vector<ct_type_t> ct_types;
	if (ct_type == json_type || ct_type == msgpack_type || content_type.empty()) {
		ct_types = msgpack_serializers;
	} else {
		ct_types.push_back(ct_type);
	}
	const auto& accepted_type = get_acceptable_type(ct_types);

	try {
		auto result = serialize_response(response, accepted_type, indent, (int)status >= 400);
		if (Logging::log_level > LOG_DEBUG) {
			if (is_acceptable_type(accepted_type, json_type)) {
				response_body += response.to_string(4);
			} else if (is_acceptable_type(accepted_type, msgpack_type)) {
				response_body += response.to_string(4);
			} else if (is_acceptable_type(accepted_type, x_msgpack_type)) {
				response_body += response.to_string(4);
			} else if (is_acceptable_type(accepted_type, html_type)) {
				response_body += response.to_string(4);
			} else if (is_acceptable_type(accepted_type, text_type)) {
				response_body += response.to_string(4);
			} else if (!response.empty()) {
				response_body += "...";
			}
		}
		if (type_encoding != Encoding::none) {
			auto encoded = encoding_http_response(type_encoding, result.first, false, true, true);
			if (!encoded.empty() && encoded.size() <= result.first.size()) {
				write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, encoded, result.second, readable_encoding(type_encoding)));
			} else {
				write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second, readable_encoding(Encoding::identity)));
			}
		} else {
			write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE, parser.http_major, parser.http_minor, 0, 0, result.first, result.second));
		}
	} catch (const SerialisationError& exc) {
		status = HTTP_STATUS_NOT_ACCEPTABLE;
		MsgPack response_err = {
			{ RESPONSE_STATUS, (int)status },
			{ RESPONSE_MESSAGE, { "Response type " + accepted_type.first + "/" + accepted_type.second + " " + exc.what() } }
		};
		auto response_str = response_err.to_string();
		if (type_encoding != Encoding::none) {
			auto encoded = encoding_http_response(type_encoding, response_str, false, true, true);
			if (!encoded.empty() && encoded.size() <= response_str.size()) {
				write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, encoded, accepted_type.first + "/" + accepted_type.second, readable_encoding(type_encoding)));
			} else {
				write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE | HTTP_CONTENT_ENCODING_RESPONSE, parser.http_major, parser.http_minor, 0, 0, response_str, accepted_type.first + "/" + accepted_type.second, readable_encoding(Encoding::identity)));
			}
		} else {
			write(http_response(status, HTTP_STATUS_RESPONSE | HTTP_HEADER_RESPONSE | HTTP_BODY_RESPONSE | HTTP_CONTENT_TYPE_RESPONSE, parser.http_major, parser.http_minor, 0, 0, response_str, accepted_type.first + "/" + accepted_type.second));
		}
		return;
	}
}


Encoding
HttpClient::resolve_encoding()
{
	L_CALL("HttpClient::resolve_encoding()");

	if (accept_encoding_set.empty()) {
		return Encoding::none;
	} else {
		for (const auto& encoding : accept_encoding_set) {
			switch(xxh64::hash(std::get<2>(encoding))) {
				case xxh64::hash("gzip"):
					return Encoding::gzip;
				case xxh64::hash("deflate"):
					return Encoding::deflate;
				case xxh64::hash("identity"):
					return Encoding::identity;
				case xxh64::hash("*"):
					return Encoding::identity;
				default:
					continue;
			}
		}
		return Encoding::unknown;
	}
}


std::string
HttpClient::readable_encoding(Encoding e)
{
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
HttpClient::encoding_http_response(Encoding e, const std::string& response, bool chunk, bool start, bool end)
{
	L_CALL("HttpClient::encoding_http_response(%s)", repr(response).c_str());

	bool gzip = false;
	switch (e) {
		case Encoding::gzip:
			gzip = true;
		case Encoding::deflate:
			if (chunk) {
				if (start) {
					encoding_compressor.reset(nullptr, 0, gzip);
					encoding_compressor.begin();
				}
				if (end) {
					auto ret = encoding_compressor.next(response.data(), response.size(), DeflateCompressData::FINISH_COMPRESS);
					return ret;
				} else {
					auto ret = encoding_compressor.next(response.data(), response.size());
					return ret;
				}
			} else {
				encoding_compressor.reset(response.data(), response.size(), gzip);
				it_compressor = encoding_compressor.begin();
				std::string encoding_respose;
				while (it_compressor) {
					encoding_respose.append(*it_compressor);
					++it_compressor;
				}
				return encoding_respose;
			}

		case Encoding::identity:
			return response;

		default:
			return std::string();
	}
}
