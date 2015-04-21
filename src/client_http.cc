/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include "http_parser.h"

#include "utils.h"
#include "cJSON.h"

#include <assert.h>
#include <sys/socket.h>


//
// Xapian http client
//


const char* status_code[6][5] = {
	{},
	{},
	{
		"OK",
		"Created"
	},
	{},
	{
		"Bad Request",
		NULL,
		NULL,
		NULL,
		"Not Found"
	},
	{
		"Internal Server Error",
		NULL,
		"Bad Gateway"
	}
};

HttpClient::HttpClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(server_, loop, sock_, database_pool_, thread_pool_, active_timeout_, idle_timeout_)
{
	parser.data = this;
	http_parser_init(&parser, HTTP_REQUEST);

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = XapiandServer::total_clients;
	int http_clients = ++XapiandServer::http_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	LOG_CONN(this, "Got connection (sock=%d), %d http client(s) of a total of %d connected.\n", sock, http_clients, XapiandServer::total_clients);

	LOG_OBJ(this, "CREATED HTTP CLIENT! (%d clients)\n", http_clients);
	assert(http_clients <= total_clients);
}


HttpClient::~HttpClient()
{
	pthread_mutex_lock(&XapiandServer::static_mutex);
	int http_clients = --XapiandServer::http_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	if (server->manager->shutdown_asap) {
		if (http_clients <= 0) {
			server->manager->async_shutdown.send();
		}
	}

	LOG_OBJ(this, "DELETED HTTP CLIENT! (%d clients left)\n", http_clients);
	assert(http_clients >= 0);
}


void HttpClient::on_read(const char *buf, ssize_t received)
{
	size_t parsed = http_parser_execute(&parser, &settings, buf, received);
	if (parsed == received) {
		if (parser.state == 1 || parser.state == 18) { // dead or message_complete
			io_read.stop();
			thread_pool->addTask(this);
		}
	} else {
		enum http_errno err = HTTP_PARSER_ERRNO(&parser);
		const char *desc = http_errno_description(err);
		const char *msg = err != HPE_OK ? desc : "incomplete request";
		LOG_HTTP_PROTO(this, msg);
		// Handle error. Just close the connection.
		destroy();
	}
}


//
// HTTP parser callbacks.
//

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
			break;
	}

	return 0;
}


int HttpClient::on_data(http_parser* p, const char *at, size_t length) {
	HttpClient *self = static_cast<HttpClient *>(p->data);

	LOG_HTTP_PROTO_PARSER(self, "%3d. %s\n", p->state, repr(at, length).c_str());

	switch (p->state) {
		case 32: // path
			self->path = std::string(at, length);
			self->body = std::string();
			break;
		case 44:
			if (strcasecmp(std::string(at, length).c_str(),"host") == 0) {
				self->ishost = true;
			}
			break;
		case 60: // receiving data from the buffer (1024 bytes)
		case 62: // finished receiving data (last data)
			self->body += std::string(at, length);
			break;
		case 50:
			if (self->ishost) {
				self->host = std::string(at, length);
				self->ishost = false;
			}
			break;
	}
	return 0;
}


void HttpClient::run()
{
	try {
		//LOG_HTTP_PROTO(this, "METHOD: %d\n", parser.method);
		//LOG_HTTP_PROTO(this, "PATH: '%s'\n", repr(path).c_str());
		//LOG_HTTP_PROTO(this, "HOST: '%s'\n", repr(host).c_str());
		//LOG_HTTP_PROTO(this, "BODY: '%s'\n", repr(body).c_str());
		if (path == "/quit") {
			server->manager->async_shutdown.send();
			return;
		}

		switch (parser.method) {
			//DELETE
			case 0:
				_delete();
				break;
			//GET (search)
			case 1:
				_search();
				break;
			//PUT
			case 4:
				_index();
				break;
			default:
				break;
		}

		/*-------------
		std::string content;
		cJSON *json = cJSON_Parse(body.c_str());
		cJSON *query = json ? cJSON_GetObjectItem(json, "query") : NULL;
		cJSON *term = query ? cJSON_GetObjectItem(query, "term") : NULL;
		cJSON *text = term ? cJSON_GetObjectItem(term, "text") : NULL;

		cJSON *root = cJSON_CreateObject();
		cJSON *response = cJSON_CreateObject();
		cJSON_AddItemToObject(root, "response", response);
		if (text) {
			cJSON_AddStringToObject(response, "status", "OK");
			cJSON_AddStringToObject(response, "query", text->valuestring);
			cJSON_AddStringToObject(response, "title", "The title");
			cJSON_AddNumberToObject(response, "items", 7);
			const char *strings[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
			cJSON *results = cJSON_CreateArray();
			cJSON_AddItemToObject(response, "results", results);
			for (int i = 0; i < 7; i++) {
				cJSON *result = cJSON_CreateObject();
				cJSON_AddNumberToObject(result, "id", i);
				cJSON_AddStringToObject(result, "name", strings[i]);
				cJSON_AddItemToArray(results, result);
			}
		} else {
			cJSON_AddStringToObject(response, "status", "ERROR");
			const char *message = cJSON_GetErrorPtr();
			if (message) {
				LOG_HTTP_PROTO(this, "JSON error before: [%s]\n", message);
				cJSON_AddStringToObject(response, "message", message);
			}
		}
		cJSON_Delete(json);

		bool pretty = false;
		char *out;
		if (pretty) {
			out = cJSON_Print(root);
		} else {
			out = cJSON_PrintUnformatted(root);
		}
		content = out;
		cJSON_Delete(root);
		free(out);

		char tmp[20];
		content += "\r\n";
		std::string http_response;
		http_response += "HTTP/";
		sprintf(tmp, "%d.%d", parser.http_major, parser.http_minor);
		http_response += tmp;
		http_response += " 200 OK\r\n";
		http_response += "Content-Type: application/json; charset=UTF-8\r\n";
		http_response += "Content-Length: ";
		sprintf(tmp, "%ld", (unsigned long)content.size());
		http_response += tmp;
		http_response += "\r\n";
		write(http_response + "\r\n" + content);
		if (parser.state == 1) {
			close();
		}//------------*/
	} catch (...) {
		LOG_ERR(this, "ERROR!\n");
	}
	io_read.start();
}

void HttpClient::_delete()
{
	struct query_t e;
	_endpointgen(e);
	Database *database = NULL;
	LOG(this, "Delete Document: %s\n", command.c_str());
	LOG(this, "Doing the checkout\n");
	if (!database_pool->checkout(&database, endpoints, true)) {
		LOG(this, "Checkout for delete aborted.\n");
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}	database->drop(command.c_str(), true);
	LOG(this, "Doing the checkin.\n");
	database_pool->checkin(&database);
	LOG(this, "FINISH DELETE\n");
	write(http_response(200, HTTP_HEADER | HTTP_CONTENT));
}

void HttpClient::_index()
{
	struct query_t e;
	_endpointgen(e);
	Database *database = NULL;
	LOG(this, "Doing the checkout for index\n");
	if (!database_pool->checkout(&database, endpoints, true)) {
		LOG(this, "Checkout for index aborted.\n");
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}
	Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(database->db);
	LOG(this, "Documents in the database: %d\n", wdb->get_doccount());
	LOG(this, "Index %s\n", body.c_str());
	database->index(body, command, true);
	LOG(this, "Documents in the database: %d\n", wdb->get_doccount());
	LOG(this, "Doing the checkin for index.\n");
	database_pool->checkin(&database);
	LOG(this, "Documents in the database: %d\n", wdb->get_doccount());
	LOG(this, "FINISH INDEX\n");
	write(http_response(200, HTTP_HEADER | HTTP_CONTENT));
}

void HttpClient::_search()
{
	std::string result;
	std::string http_header;
	std::string http_error_header;
	std::string name_result;
	int rc = 0;
	int rmset;

	struct query_t e;
	_endpointgen(e);

	Database *database = NULL;
	LOG(this, "Doing the checkout for search\n");
	if (!database_pool->checkout(&database, endpoints, false)) {
		LOG(this, "Checkout for search aborted.\n");
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	/*
	 NOTE:	Missing spies
			Ask if add get_termlist
	 */
	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	rmset = database->get_mset(e, mset, suggestions);
	if (rmset == 1) {
		LOG(this, "get_mset return 1\n");
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}
	if (rmset == 2) {
		LOG(this, "get_mset return 2\n");
		write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
		return;
	}


	LOG(this, "Suggered querys\n");
	std::vector<std::string>::const_iterator it_s(suggestions.begin());
	for ( ; it_s != suggestions.end(); it_s++) {
		LOG(this, "\t%s\n", (*it_s).c_str());
	}


	for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); rc++) {
		Xapian::docid did = 0;
		int rank = 0;
		double weight = 0, percent = 0;
		std::string data;

		int t = 3;
		for (; t >= 0; --t) {
			try {
				did = *m;
				rank = m.get_rank();
				weight = m.get_weight();
				percent = m.get_percent();
				data = m.get_document().get_data();
				m++;
				break;
			} catch (Xapian::Error &er) {
				database->reopen();
				if (database->get_mset(e, mset, suggestions, rc)== 0) {
					m = mset.begin();
				} else {
					t = -1;
				}
			}
		}
		if (t < 0) {
			if (rc == 0) {
				write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
			} else {
				// err obj
				write("0\r\n\r\n");
			}
			return;
		}

		if (rc == 0) {
			write(http_response(200, HTTP_HEADER | HTTP_JSON | HTTP_CHUNKED));
		}

		cJSON *root = cJSON_CreateObject();
		cJSON *response = cJSON_CreateObject();
		//LOG(this, "loop %d docid:%d rank:%d data:%s\n",rc,did,m.get_rank(),std::string(m.get_document().get_data()).c_str());
		name_result = "result-" + std::to_string(rc);
		cJSON_AddItemToObject(root, name_result.c_str(), response);
		cJSON_AddNumberToObject(response, "docid", did);
		cJSON_AddNumberToObject(response, "rank", rank);
		cJSON_AddNumberToObject(response, "weight", weight);
		cJSON_AddNumberToObject(response, "percent", percent);
		cJSON_AddStringToObject(response, "data", data.c_str());
		result = cJSON_PrintUnformatted(root);
		result += "\n";
		result = http_response(200,  HTTP_CONTENT | HTTP_JSON | HTTP_CHUNKED, result);

		if (!write(result)) {
			break;
		}

		cJSON_Delete(root);
	}
	write("0\r\n\r\n");

	LOG(this, "Doing the checkin for search.\n");
	database_pool->checkin(&database);
	LOG(this, "FINISH SEARCH\n");
}

void HttpClient::_endpointgen(struct query_t &e)
{
	struct http_parser_url u;
	std::string b = repr(path);
	LOG_CONN_WIRE(this,"URL: %s\n", b.c_str());
	if(http_parser_parse_url(b.c_str(), b.size(), 0, &u) == 0){
		LOG_CONN_WIRE(this,"Parsing done\n");

		if (u.field_set & (1 <<  UF_PATH )){
			size_t path_size = u.field_data[3].len;
			std::string path_buf(b.c_str() + u.field_data[3].off, u.field_data[3].len);

			struct parser_url_path_t p;
			memset(&p, 0, sizeof(p));
			std::string endp;
			std::string nsp_;
			std::string pat_;
			std::string hos_;

			while (url_path(path_buf.c_str(), path_size, &p) == 0) {
				command  = urldecode(p.off_command, p.len_command);

				if (p.len_namespace) {
					nsp_ = urldecode(p.off_namespace, p.len_namespace) + "/";
				} else {
					nsp_ = "";
				}
				if (p.len_path) {
					pat_ = urldecode(p.off_path, p.len_path);
				} else {
					pat_ = "";
				}
				if (p.len_host) {
					hos_ = urldecode(p.off_host, p.len_host);
				} else if(!host.empty()) {
					hos_ = host;
				} else {
					hos_ = "127.0.0.1";
				}
				endp = "xapian://" + hos_ + nsp_ + pat_;
				endpoints.insert(Endpoint(endp, std::string(), XAPIAND_BINARY_SERVERPORT));

				LOG_CONN_WIRE(this,"Endpoint: -> %s\n", endp.c_str());
			}
		}

		/*LOG(this,"FIRST QUERY PARSER SIZE %d\n",u.field_data[4].len);
		 LOG(this,"FIRST QUERY PARSER OFFSET %d\n",u.field_data[4].off);
		 LOG(this,"BUFFER %s\n",b.c_str());*/
		if (u.field_set & (1 <<  UF_QUERY )) {
			size_t query_size = u.field_data[4].len;
			std::string query_buf(b.c_str() + u.field_data[4].off, u.field_data[4].len);

			struct parser_query_t q;

			memset(&q, 0, sizeof(q));
			if (url_qs("offset", query_buf.c_str(), query_size, &q) != -1) {
				e.offset = atoi(urldecode(q.offset, q.length).c_str());
			} else {
				e.offset = 0;
			}

			memset(&q, 0, sizeof(q));
			if (url_qs("limit", query_buf.c_str(), query_size, &q) != -1) {
				e.limit = atoi(urldecode(q.offset, q.length).c_str());
			} else {
				e.limit = 10;
			}

			memset(&q, 0, sizeof(q));
			if (url_qs("spelling", query_buf.c_str(), query_size, &q) != -1) {
				std::string spelling = serialise_bool(urldecode(q.offset, q.length));
				(spelling.compare("f") == 0) ? e.spelling = false : e.spelling = true;
			} else {
				e.spelling = true;
			}

			memset(&q, 0, sizeof(q));
			if (url_qs("synonyms", query_buf.c_str(), query_size, &q) != -1) {
				std::string synonyms = serialise_bool(urldecode(q.offset, q.length));
				(synonyms.compare("f") == 0) ? e.synonyms = false : e.synonyms = true;
			} else {
				e.synonyms = false;
			}

			memset(&q, 0, sizeof(q));
			LOG(this, "Buffer: %s\n", query_buf.c_str());
			while (url_qs("query", query_buf.c_str(), query_size, &q) != -1) {
				LOG(this, "%s\n", urldecode(q.offset, q.length).c_str());
				e.query.push_back(urldecode(q.offset, q.length));
			}

			memset(&q, 0, sizeof(q));
			while (url_qs("partial", query_buf.c_str(), query_size, &q) != -1) {
				e.partial.push_back(urldecode(q.offset, q.length));
			}

			memset(&q, 0, sizeof(q));
			while (url_qs("terms", query_buf.c_str(), query_size, &q) != -1) {
				e.terms.push_back(urldecode(q.offset, q.length));
			}

			memset(&q, 0, sizeof(q));
			while (url_qs("order", query_buf.c_str(), query_size, &q) != -1) {
				e.order.push_back(urldecode(q.offset, q.length));
			}

			memset(&q, 0, sizeof(q));
			while (url_qs("facets", query_buf.c_str(), query_size, &q) != -1) {
				e.facets.push_back(urldecode(q.offset, q.length));
			}

			memset(&q, 0, sizeof(q));
			while (url_qs("language", query_buf.c_str(), query_size, &q) != -1) {
				e.language.push_back(urldecode(q.offset, q.length));
			}
		}
	} else {
		LOG_CONN_WIRE(this,"Parsing not done\n");
	}
}

std::string HttpClient::http_response(int status, int mode, std::string content)
{
	char buffer[20];
	std::string response;
	std::string eol("\r\n");

	if (mode & HTTP_HEADER) {
		snprintf(buffer, sizeof(buffer), "HTTP/%d.%d %d ", parser.http_major, parser.http_minor, status);
		response += buffer;
		response += status_code[status / 100][status % 100] + eol;

		if (mode & HTTP_JSON) {
			response += "Content-Type: application/json; charset=UTF-8" + eol;
		}

		if (mode & HTTP_CHUNKED) {
			response += "Transfer-Encoding: chunked" + eol;
		} else {
			response += "Content-Length: ";
			snprintf(buffer, sizeof(buffer), "%lu", content.size());
			response += buffer + eol;
		}
		response += eol;
	}

	if (mode & HTTP_CONTENT) {
		if (mode & HTTP_CHUNKED) {
			snprintf(buffer, sizeof(buffer), "%lx", content.size());
			response += buffer + eol;
			response += content + eol;
		} else {
			response += content;
		}
	}

	return response;
}
