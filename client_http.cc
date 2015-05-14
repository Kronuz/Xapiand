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

#include "http_parser.h"

#include "multivalue.h"
#include "utils.h"
#include "length.h"
#include "cJSON.h"
#include "manager.h"

#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>


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
		"Not Implemented",
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
			written = 0;
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
	std::string error;

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
			//GET
			case 1:
			case 3:
				_search();
				break;
			//HEAD
			case 2:
				_head();
				break;
			//PUT
			case 4:
				_index();
				break;
			//OPTIONS
			case 6:
				write(http_response(200, HTTP_HEADER | HTTP_OPTIONS));
				break;
			//PATCH
			case 24:
				_patch();
			default:
				write(http_response(501, HTTP_HEADER | HTTP_CONTENT));
				break;
		}
	} catch (const Xapian::Error &err) {
		error.assign(err.get_error_string());
	} catch (const std::exception &err) {
		error.assign(err.what());
	} catch (...) {
		error.assign("Unkown error!");
	}
	if (!error.empty()) {
		LOG_ERR(this, "ERROR: %s\n", error.c_str());
		if (written) {
			destroy();
		} else {
			write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
		}
	}
	io_read.start();
}

void HttpClient::_head()
{
	bool found = true;
	std::string result;
	Xapian::docid docid = 0;
	Xapian::QueryParser queryparser;
	cJSON *root = cJSON_CreateObject();
	query_t e;
	int cmd = _endpointgen(e);

	switch (cmd) {
		case CMD_NUMBER: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		default:
			cJSON *err_response = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "Response", err_response);
			if (cmd == CMD_UNKNOWN)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown task "+command).c_str());

			if (cmd == CMD_UNKNOWN_HOST)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown host "+host).c_str());

			else
				cJSON_AddStringToObject(err_response, "Error message","BAD QUERY");
			result = cJSON_PrintUnformatted(root);
			result += "\n";
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
			cJSON_Delete(root);
			return;
	}

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints, false)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	queryparser.add_prefix("id", "Q");
	Xapian::Query query = queryparser.parse_query(std::string("id:" + command));
	Xapian::Enquire enquire(*database->db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	if(mset.size()) {
			Xapian::MSetIterator m = mset.begin();
		int t = 3;
		for (; t >= 0; --t) {
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

	if(found){
		cJSON_AddNumberToObject(root,"id",docid);
		result = cJSON_PrintUnformatted(root);
		result += "\n";
		result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
		write(result);
	} else {
		cJSON_AddStringToObject(root,"Error", "Document not found");
		result = cJSON_PrintUnformatted(root);
		result += "\n";
		write(http_response(404, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
	}

	database_pool->checkin(&database);
	cJSON_Delete(root);
}

void HttpClient::_delete()
{
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	std::string result;
	query_t e;
	int cmd = _endpointgen(e);

	switch (cmd) {
		case CMD_NUMBER: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		default:
			cJSON *err_response = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "Response", err_response);
			if (cmd == CMD_UNKNOWN)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown task "+command).c_str());

			if (cmd == CMD_UNKNOWN_HOST)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown host "+host).c_str());

			else
				cJSON_AddStringToObject(err_response, "Error message","BAD QUERY");
			result = cJSON_PrintUnformatted(root);
			result += "\n";
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
			cJSON_Delete(root);
			cJSON_Delete(data);
			return;
	}

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints, true)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	clock_t t = clock();
	if (!database->drop(command, e.commit)) {
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	t = clock() - t;
	double time = (double)t / CLOCKS_PER_SEC;
	LOG(this, "Time take for delete %f\n", time);
	pthread_mutex_lock(&qmtx);
	update_pos_time();
	stats_cnt.del.cnt[b_time.minute]++;
	stats_cnt.del.sec[b_time.second]++;
	stats_cnt.del.tm_cnt[b_time.minute] += time;
	stats_cnt.del.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&qmtx);

	database_pool->checkin(&database);
	cJSON_AddStringToObject(data, "id", command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root, "delete", data);
	if (e.pretty) {
		result = cJSON_Print(root);
	} else {
		result = cJSON_PrintUnformatted(root);
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
	write(result);
	cJSON_Delete(root);
}

void HttpClient::_index()
{
	std::string result;
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	query_t e;
	int cmd = _endpointgen(e);

	switch (cmd) {
		case CMD_NUMBER: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		default:
			cJSON *err_response = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "Response", err_response);
			if (cmd == CMD_UNKNOWN)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown task "+command).c_str());

			if (cmd == CMD_UNKNOWN_HOST)
				cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown host "+host).c_str());

			else
				cJSON_AddStringToObject(err_response, "Error message","BAD QUERY");
			result = cJSON_PrintUnformatted(root);
			result += "\n";
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
			cJSON_Delete(root);
			cJSON_Delete(data);
			return;
	}

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints, true)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	clock_t t = clock();

	cJSON *document = cJSON_Parse(body.c_str());
	if (!document) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	if (!database->index(document, command, e.commit)) {
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}
	cJSON_Delete(document);

	t = clock() - t;
	double time = (double)t / CLOCKS_PER_SEC;
	LOG(this, "Time take for index %f\n", time);
	pthread_mutex_lock(&qmtx);
	update_pos_time();
	stats_cnt.index.cnt[b_time.minute]++;
	stats_cnt.index.sec[b_time.second]++;
	stats_cnt.index.tm_cnt[b_time.minute] += time;
	stats_cnt.index.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&qmtx);

	database_pool->checkin(&database);
	cJSON_AddStringToObject(data, "id", command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root, "index", data);
	if (e.pretty) {
		result = cJSON_Print(root);
	} else {
		result = cJSON_PrintUnformatted(root);
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
	write(result);
	cJSON_Delete(root);
}


void HttpClient::_patch()
{
	std::string result;
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	query_t e;
	int cmd = _endpointgen(e);

	switch (cmd) {
		case CMD_NUMBER: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		default:
			cJSON *err_response = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "Response", err_response);
			if (cmd == CMD_UNKNOWN)
				cJSON_AddStringToObject(err_response, "Error message", std::string("Unknown task " + command).c_str());

			if (cmd == CMD_UNKNOWN_HOST)
				cJSON_AddStringToObject(err_response, "Error message", std::string("Unknown host " + host).c_str());
			else
				cJSON_AddStringToObject(err_response, "Error message", "BAD QUERY");

			result = cJSON_PrintUnformatted(root);
			result += "\n";
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
			cJSON_Delete(root);
			cJSON_Delete(data);
			return;
	}

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints, true)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	cJSON *patches = cJSON_Parse(body.c_str());
	if (!patches) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	if (!database->patch(patches, command, e.commit)) {
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	database_pool->checkin(&database);
	cJSON_AddStringToObject(data, "id", command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root, "update", data);
	if (e.pretty) {
		result = cJSON_Print(root);
	} else {
		result = cJSON_PrintUnformatted(root);
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
	write(result);
	cJSON_Delete(patches);
	cJSON_Delete(root);
}


void HttpClient::_stats(query_t &e)
{
	std::string result;
	cJSON *root = cJSON_CreateObject();

	if (e.server) {
		cJSON_AddItemToObject(root, "Server status", server->manager->server_status());
	}
	if (e.database) {
		_endpointgen(e);
		Database *database = NULL;
		if (!database_pool->checkout(&database, endpoints, false)) {
			write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
			return;
		}
		cJSON *JSON_database = database->get_stats_database();
		cJSON_AddItemToObject(root, "Database status", JSON_database);
		database_pool->checkin(&database);
	}
	if (e.document >= 0) {
		_endpointgen(e);
		Database *database = NULL;
		if (!database_pool->checkout(&database, endpoints, false)) {
			write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
			return;
		}
		cJSON *JSON_document = database->get_stats_docs(e.document);
		cJSON_AddItemToObject(root, "Document status", JSON_document);
		database_pool->checkin(&database);
	}
	if (e.stats.size() != 0) {
		cJSON_AddItemToObject(root, "Stats time", server->manager->get_stats_time(e.stats));
	}
	if (e.pretty) {
		result = cJSON_Print(root);
	} else {
		result = cJSON_PrintUnformatted(root);
	}
	result += "\n\n";
	result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
	write(result);
	cJSON_Delete(root);
}

void HttpClient::_search()
{
	bool facets = false;
	bool json_chunked = true;
	std::string result;

	query_t e;
	int cmd = _endpointgen(e);

	switch (cmd) {
		case CMD_NUMBER:
			e.query.push_back(std::string("id:" + command));
			e.offset = 0;
			e.limit = 1;
			e.check_at_least = 0;
			e.spelling = true;
			e.synonyms = false;
			e.unique_doc = true;
			json_chunked = false;
			break;
		case CMD_SEARCH:
			e.check_at_least = 0;
			break;
		case CMD_FACETS:
			facets = true;
			break;
		case CMD_STATS:
			_stats(e);
			return;
		default:
			if (Is_id_range(command)){
				e.query.push_back(std::string("id:" + command));
				e.offset = 0;
				e.limit = 1000;
				e.check_at_least = 0;
				e.spelling = true;
				e.synonyms = false;
				e.unique_doc = true;
			}else {
				cJSON *root = cJSON_CreateObject();
				cJSON *err_response = cJSON_CreateObject();
				cJSON_AddItemToObject(root, "Response", err_response);
				if (cmd == CMD_UNKNOWN)
					cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown task "+command).c_str());

				if (cmd == CMD_UNKNOWN_HOST)
					cJSON_AddStringToObject(err_response, "Error message",std::string("Unknown host "+host).c_str());

				else
					cJSON_AddStringToObject(err_response, "Error message","BAD QUERY");
				result = cJSON_PrintUnformatted(root);
				result += "\n";
				write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result));
				cJSON_Delete(root);
				return;
			}
	}

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints, false)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;
	clock_t t = clock();
	int rmset = database->get_mset(e, mset, spies, suggestions);
	if (rmset == 1) {
		LOG(this, "get_mset return 1\n");
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		database_pool->checkin(&database);
		LOG(this, "ABORTED SEARCH\n");
		return;
	}
	if (rmset == 2) {
		LOG(this, "get_mset return 2\n");
		write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
		database_pool->checkin(&database);
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
		cJSON *root = cJSON_CreateObject();
		for (; spy != spies.end(); spy++) {
			std::string name_result = (*spy).first;
			cJSON *array_values = cJSON_CreateArray();
			cJSON_AddItemToObject(root, name_result.c_str(), array_values);
			for (Xapian::TermIterator facet = (*spy).second->values_begin(); facet != (*spy).second->values_end(); ++facet) {
				cJSON *value = cJSON_CreateObject();
				cJSON_AddStringToObject(value, "value", unserialise(database->field_type((*spy).first)[1], (*spy).first, *facet).c_str());
				cJSON_AddNumberToObject(value, "termfreq", facet.get_termfreq());
				cJSON_AddItemToArray(array_values, value);
			}
		}
		if (e.pretty) {
			result = cJSON_Print(root);
		} else {
			result = cJSON_PrintUnformatted(root);
		}
		result += "\n\n";
		result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
		write(result);
		cJSON_Delete(root);
	} else {
		int rc = 0;
		if (mset.size() != 0) {
			for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); rc++, m++) {
				Xapian::docid docid = 0;
				std::string id;
				std::string type;
				int rank = 0;
				double weight = 0, percent = 0;
				std::string data;

				int t = 3;
				for (; t >= 0; --t) {
					try {
						docid = *m;
						rank = m.get_rank();
						weight = m.get_weight();
						percent = m.get_percent();
						break;
					} catch (const Xapian::Error &err) {
						database->reopen();
						if (database->get_mset(e, mset, spies, suggestions, rc)== 0) {
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
						write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
					}
					database_pool->checkin(&database);
					LOG(this, "ABORTED SEARCH\n");
					return;
				}

				data = document.get_data();
				id = document.get_value(0);
				type = document.get_value(1);

				if (rc == 0 && json_chunked) {
					write(http_response(200, HTTP_HEADER | HTTP_JSON | HTTP_CHUNKED));
				}

				cJSON *object = cJSON_Parse(data.c_str());
				cJSON *object_data = cJSON_GetObjectItem(object, RESERVED_DATA);
				if (object_data) {
					object_data = cJSON_Duplicate(object_data, 1);
					cJSON_Delete(object);
					object = object_data;
				} else {
					database->clean_reserved(object);
					cJSON_AddStringToObject(object, "_id", id.c_str());
					cJSON_AddStringToObject(object, "_type", type.c_str());
				}

				if (e.pretty) {
					result = cJSON_Print(object);
				} else {
					result = cJSON_PrintUnformatted(object);
				}
				result += "\n\n";
				if(json_chunked) {
					result = http_response(200,  HTTP_CONTENT | HTTP_JSON | HTTP_CHUNKED, result);
				} else {
					result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
				}

				if (!write(result)) {
					break;
				}
				cJSON_Delete(object);
			}
			if(json_chunked) {
				write("0\r\n\r\n");
			}
		} else {
			cJSON *root = cJSON_CreateObject();
			cJSON_AddStringToObject(root, "Response empty","No match found");
			if (e.pretty) {
				result = cJSON_Print(root);
			} else {
				result = cJSON_PrintUnformatted(root);
			}
			result += "\n\n";
			result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, result);
			write(result);
			cJSON_Delete(root);
		}
	}

	t = clock() - t;
	double time = (double)t / CLOCKS_PER_SEC;
	LOG(this, "Time take for search %f\n", time);
	pthread_mutex_lock(&qmtx);
	update_pos_time();
	stats_cnt.search.cnt[b_time.minute]++;
	stats_cnt.search.sec[b_time.second]++;
	stats_cnt.search.tm_cnt[b_time.minute] += time;
	stats_cnt.search.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&qmtx);

	database_pool->checkin(&database);
	LOG(this, "FINISH SEARCH\n");
}

int HttpClient::_endpointgen(query_t &e)
{
	int cmd;
	struct http_parser_url u;
	std::string b = repr(path);

	LOG_CONN_WIRE(this,"URL: %s\n", b.c_str());
	if (http_parser_parse_url(b.c_str(), b.size(), 0, &u) == 0) {
		LOG_CONN_WIRE(this,"Parsing done\n");

		if (u.field_set & (1 <<  UF_PATH )) {
			size_t path_size = u.field_data[3].len;
			std::string path_buf(b.c_str() + u.field_data[3].off, u.field_data[3].len);

			endpoints.clear();

			parser_url_path_t p;
			memset(&p, 0, sizeof(p));
			while (url_path(path_buf.c_str(), path_size, &p) == 0) {

				command  = urldecode(p.off_command, p.len_command);

				if (command.empty()) {
					return CMD_BAD_QUERY;
				}

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

				std::string node_name;
				if (p.len_host) {
					node_name = urldecode(p.off_host, p.len_host);
				} else if (!host.empty()) {
					node_name = host;
				}

				std::string index_path = ns + path;
				Endpoint index("xapian://" + node_name + index_path);
				server->manager->discovery(DISCOVERY_DB, serialise_string(index.path));
				int node_port = (index.port == XAPIAND_BINARY_SERVERPORT) ? 0 : index.port;
				node_name = index.host.empty() ? node_name : index.host;

				// Convert node to endpoint:
				char node_ip[INET_ADDRSTRLEN];
				Node node;
				if (!server->manager->touch_node(node_name, &node)) {
					LOG(this, "Node %s not found\n", node_name.c_str());
					host = node_name;
					return CMD_UNKNOWN_HOST;
				}
				if (!node_port) node_port = node.binary_port;
				inet_ntop(AF_INET, &(node.addr.sin_addr), node_ip, INET_ADDRSTRLEN);
				Endpoint endpoint("xapian://" + std::string(node_ip) + ":" + std::to_string(node_port) + index_path);

				endpoints.insert(endpoint);

				LOG_CONN_WIRE(this,"Endpoint: -> %s\n", endp.c_str());
			}
		}

		cmd = identify_cmd(command);

		if (u.field_set & (1 <<  UF_QUERY )) {
			size_t query_size = u.field_data[4].len;
			std::string query_buf(b.c_str() + u.field_data[4].off, u.field_data[4].len);

			parser_query_t q;

			memset(&q, 0, sizeof(q));
			if (url_qs("pretty", query_buf.c_str(), query_size, &q) != -1) {
				std::string pretty = serialise_bool(urldecode(q.offset, q.length));
				(pretty.compare("f") == 0) ? e.pretty = false : e.pretty = true;
			} else {
				e.pretty = false;
			}

			if (cmd == CMD_SEARCH || cmd == CMD_FACETS) {

				e.unique_doc = false;

				memset(&q, 0, sizeof(q));
				if (url_qs("offset", query_buf.c_str(), query_size, &q) != -1) {
					e.offset = atoi(urldecode(q.offset, q.length).c_str());
				} else {
					e.offset = 0;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("check_at_least", query_buf.c_str(), query_size, &q) != -1) {
					e.check_at_least = atoi(urldecode(q.offset, q.length).c_str());
				} else {
					e.check_at_least = 0;
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

				memset(&q, 0, sizeof(q));
				if (url_qs("fuzzy", query_buf.c_str(), query_size, &q) != -1) {
					std::string fuzzy = serialise_bool(urldecode(q.offset, q.length));
					(fuzzy.compare("f") == 0) ? e.is_fuzzy = false : e.is_fuzzy = true;
				} else {
					e.is_fuzzy = false;
				}

				if(e.is_fuzzy) {
					memset(&q, 0, sizeof(q));
					if (url_qs("fuzzy.n_rset", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.n_rset = atoi(urldecode(q.offset, q.length).c_str());
					} else {
						e.fuzzy.n_rset = 5;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("fuzzy.n_eset", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.n_eset = atoi(urldecode(q.offset, q.length).c_str());
					} else {
						e.fuzzy.n_eset = 32;
					}

					memset(&q, 0, sizeof(q));
					while (url_qs("fuzzy.field", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.field.push_back(urldecode(q.offset, q.length));
					}

					memset(&q, 0, sizeof(q));
					while (url_qs("fuzzy.type", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.type.push_back(urldecode(q.offset, q.length));
					}
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("nearest", query_buf.c_str(), query_size, &q) != -1) {
					std::string nearest = serialise_bool(urldecode(q.offset, q.length));
					(nearest.compare("f") == 0) ? e.is_nearest = false : e.is_nearest = true;
				} else {
					e.is_nearest = false;
				}

				if(e.is_nearest) {
					memset(&q, 0, sizeof(q));
					if (url_qs("nearest.n_rset", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.n_rset = atoi(urldecode(q.offset, q.length).c_str());
					} else {
						e.nearest.n_rset = 5;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("nearest.n_eset", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.n_eset = atoi(urldecode(q.offset, q.length).c_str());
					} else {
						e.nearest.n_eset = 32;
					}

					memset(&q, 0, sizeof(q));
					while (url_qs("nearest.field", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.field.push_back(urldecode(q.offset, q.length));
					}

					memset(&q, 0, sizeof(q));
					while (url_qs("nearest.type", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.type.push_back(urldecode(q.offset, q.length));
					}
				}

			} else if (cmd == CMD_NUMBER) {
				memset(&q, 0, sizeof(q));
				if (url_qs("commit", query_buf.c_str(), query_size, &q) != -1) {
					std::string pretty = serialise_bool(urldecode(q.offset, q.length));
					(pretty.compare("f") == 0) ? e.commit = false : e.commit = true;
				} else {
					e.commit = false;
				}
			} else if (cmd == CMD_STATS) {
				memset(&q, 0, sizeof(q));
				if (url_qs("server", query_buf.c_str(), query_size, &q) != -1) {
					std::string server = serialise_bool(urldecode(q.offset, q.length));
					(server.compare("f") == 0) ? e.server = false : e.server = true;
				} else {
					e.server = false;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("database", query_buf.c_str(), query_size, &q) != -1) {
					std::string database = serialise_bool(urldecode(q.offset, q.length));
					(database.compare("f") == 0) ? e.database = false : e.database = true;
				} else {
					e.database = false;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("document", query_buf.c_str(), query_size, &q) != -1) {
					e.document = strtoint(urldecode(q.offset, q.length));
				} else {
					e.document = -1;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("stats", query_buf.c_str(), query_size, &q) != -1) {
					e.stats = urldecode(q.offset, q.length);
				} else {
					e.stats = "";
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
		
		if (mode & HTTP_OPTIONS) {
			response += "Allow: GET,HEAD,POST,PUT,PATCH,OPTIONS" + eol;
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