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
#include "serialise.h"
#include "length.h"
#include "cJSON.h"
#include "manager.h"

#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Xapian http client
#define METHOD_DELETE  0
#define METHOD_HEAD    2
#define METHOD_GET     1
#define METHOD_POST    3
#define METHOD_PUT     4
#define METHOD_OPTIONS 6
#define METHOD_PATCH   24


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
	: BaseClient(server_, loop, sock_, database_pool_, thread_pool_, active_timeout_, idle_timeout_),
	  database(NULL)
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

	if (manager()->shutdown_asap) {
		if (http_clients <= 0) {
			manager()->async_shutdown.send();
		}
	}

	LOG_OBJ(this, "DELETED HTTP CLIENT! (%d clients left)\n", http_clients);
	assert(http_clients >= 0);

	delete database;
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
			case METHOD_POST:
				_search();
				break;
			case METHOD_HEAD:
				_head();
				break;
			case METHOD_PUT:
				_index();
				break;
			case METHOD_OPTIONS:
				write(http_response(200, HTTP_HEADER | HTTP_OPTIONS));
				break;
			case METHOD_PATCH:
				_patch();
			default:
				write(http_response(501, HTTP_HEADER | HTTP_CONTENT));
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
			database_pool->checkin(&database);
		}
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
	query_t e;
	int cmd = _endpointgen(e, false);

	switch (cmd) {
		case CMD_ID: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		case CMD_SCHEMA:
		default:
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
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
			return;
	}

	if (!database_pool->checkout(&database, endpoints, DB_SPAWN)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
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
	if(mset.size()) {
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
	if(found){
		cJSON_AddNumberToObject(root.get(), RESERVED_ID, docid);
		if (e.pretty) {
			unique_char_ptr _cprint(cJSON_Print(root.get()));
			result.assign(_cprint.get());
		} else {
			unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
			result.assign(_cprint.get());
		}
		result += "\n";
		result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
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
		write(http_response(404, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
	}

	database_pool->checkin(&database);
}


void HttpClient::_delete()
{
	std::string result;
	query_t e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		case CMD_SCHEMA:
		default:
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
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
			return;
	}

	if (!database_pool->checkout(&database, endpoints, DB_WRITABLE|DB_SPAWN)) {
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
	pthread_mutex_lock(&manager()->qmtx);
	update_pos_time();
	stats_cnt.del.cnt[b_time.minute]++;
	stats_cnt.del.sec[b_time.second]++;
	stats_cnt.del.tm_cnt[b_time.minute] += time;
	stats_cnt.del.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&manager()->qmtx);

	database_pool->checkin(&database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root.get(), "delete", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
	write(result);
}

void HttpClient::_index()
{
	std::string result;
	query_t e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		case CMD_SCHEMA:
		default:
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
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
			return;
	}

	if (!database_pool->checkout(&database, endpoints, DB_WRITABLE|DB_SPAWN|DB_INIT_REF)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	clock_t t = clock();

	unique_cJSON document(cJSON_Parse(body.c_str()), cJSON_Delete);
	if (!document) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	if (!database->index(document.get(), command, e.commit)) {
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	t = clock() - t;
	double time = (double)t / CLOCKS_PER_SEC;
	LOG(this, "Time take for index %f\n", time);
	pthread_mutex_lock(&manager()->qmtx);
	update_pos_time();
	stats_cnt.index.cnt[b_time.minute]++;
	stats_cnt.index.sec[b_time.second]++;
	stats_cnt.index.tm_cnt[b_time.minute] += time;
	stats_cnt.index.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&manager()->qmtx);

	database_pool->checkin(&database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root.get(), "index", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
	write(result);
}


void HttpClient::_patch()
{
	std::string result;
	query_t e;
	int cmd = _endpointgen(e, true);

	switch (cmd) {
		case CMD_ID: break;
		case CMD_SEARCH:
		case CMD_FACETS:
		case CMD_STATS:
		case CMD_SCHEMA:
		default:
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
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
			return;
	}

	if (!database_pool->checkout(&database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	unique_cJSON patches(cJSON_Parse(body.c_str()), cJSON_Delete);
	if (!patches) {
		LOG_ERR(this, "ERROR: JSON Before: [%s]\n", cJSON_GetErrorPtr());
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	if (!database->patch(patches.get(), command, e.commit)) {
		database_pool->checkin(&database);
		write(http_response(400, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	database_pool->checkin(&database);
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
	cJSON *data = cJSON_CreateObject(); // It is managed by root.
	cJSON_AddStringToObject(data, RESERVED_ID, command.c_str());
	(e.commit) ? cJSON_AddTrueToObject(data, "commit") : cJSON_AddFalseToObject(data, "commit");
	cJSON_AddItemToObject(root.get(), "update", data);
	if (e.pretty) {
		unique_char_ptr _cprint(cJSON_Print(root.get()));
		result.assign(_cprint.get());
	} else {
		unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
		result.assign(_cprint.get());
	}
	result += "\n\n";
	result = http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
	write(result);
}


void HttpClient::_stats(query_t &e)
{
	std::string result;
	unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);

	if (e.server) {
		unique_cJSON server_stats = manager()->server_status();
		cJSON_AddItemToObject(root.get(), "Server status", server_stats.release());
	}
	if (e.database) {
		if (!database_pool->checkout(&database, endpoints, DB_SPAWN)) {
			write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
			return;
		}
		unique_cJSON JSON_database = database->get_stats_database();
		cJSON_AddItemToObject(root.get(), "Database status", JSON_database.release());
		database_pool->checkin(&database);
	}
	if (!e.document.empty()) {
		if (!database_pool->checkout(&database, endpoints, DB_SPAWN)) {
			write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
			return;
		}
		unique_cJSON JSON_document = database->get_stats_docs(e.document);
		cJSON_AddItemToObject(root.get(), "Document status", JSON_document.release());
		database_pool->checkin(&database);
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
	result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
	write(result);
}


void HttpClient::_search()
{
	int cout_matched = 0;
	bool facets = false;
	bool schema = false;
	bool json_chunked = true;
	std::string result;

	query_t e;
	int cmd = _endpointgen(e, false);

	switch (cmd) {
		case CMD_ID:
			e.query.push_back(std::string(RESERVED_ID)  + ":" +  command);
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
		case CMD_SCHEMA:
			schema = true;
			break;
		default:
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
			write(http_response(400, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result));
			return;
	}

	if (!database_pool->checkout(&database, endpoints, DB_SPAWN)) {
		write(http_response(502, HTTP_HEADER | HTTP_CONTENT));
		return;
	}

	if (schema) {
		std::string schema_;
		if (database->get_metadata(RESERVED_SCHEMA, schema_)) {
			unique_cJSON jschema(cJSON_Parse(schema_.c_str()), cJSON_Delete);
			readable_schema(jschema.get());
			unique_char_ptr _cprint(cJSON_Print(jschema.get()));
            schema_ = std::string(_cprint.get()) + "\n";
			write(http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, schema_));
			database_pool->checkin(&database);
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
			write(http_response(200, HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, schema_));
			database_pool->checkin(&database);
			return;
		}
	}

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;
	clock_t t = clock();
	int rmset = database->get_mset(e, mset, spies, suggestions);
	cout_matched = mset.size();
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
		unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
		for (; spy != spies.end(); spy++) {
			std::string name_result = (*spy).first;
			cJSON *array_values = cJSON_CreateArray(); // It is managed by root.
			for (Xapian::TermIterator facet = (*spy).second->values_begin(); facet != (*spy).second->values_end(); ++facet) {
				cJSON *value = cJSON_CreateObject(); // It is managed by array_values.
                data_field_t field_t = database->get_data_field((*spy).first);
				cJSON_AddStringToObject(value, "value", Unserialise::unserialise(field_t.type, (*spy).first, *facet).c_str());
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
		result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
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
						write(http_response(500, HTTP_HEADER | HTTP_CONTENT));
					}
					database_pool->checkin(&database);
					LOG(this, "ABORTED SEARCH\n");
					return;
				}

				data = document.get_data();
				id = document.get_value(0);

				if (rc == 0 && json_chunked) {
					write(http_response(200, HTTP_HEADER | HTTP_JSON | HTTP_CHUNKED | HTTP_MATCHED_COUNT, cout_matched));
				}

				unique_cJSON object(cJSON_Parse(data.c_str()), cJSON_Delete);
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
				if(json_chunked) {
					result = http_response(200,  HTTP_CONTENT | HTTP_JSON | HTTP_CHUNKED, 0, result);
				} else {
					result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON, 0, result);
				}

				if (!write(result)) {
					break;
				}
			}
			if(json_chunked) {
				write("0\r\n\r\n");
			}
		} else {
			unique_cJSON root(cJSON_CreateObject(), cJSON_Delete);
			cJSON_AddStringToObject(root.get(), "Response empty", "No match found");
			if (e.pretty) {
				unique_char_ptr _cprint(cJSON_Print(root.get()));
				result.assign(_cprint.get());
			} else {
				unique_char_ptr _cprint(cJSON_PrintUnformatted(root.get()));
				result.assign(_cprint.get());
			}
			result += "\n\n";
			result = http_response(200,  HTTP_HEADER | HTTP_CONTENT | HTTP_JSON | HTTP_MATCHED_COUNT, 0, result);
			write(result);
		}
	}

	t = clock() - t;
	double time = (double)t / CLOCKS_PER_SEC;
	LOG(this, "Time take for search %f\n", time);
	pthread_mutex_lock(&manager()->qmtx);
	update_pos_time();
	stats_cnt.search.cnt[b_time.minute]++;
	stats_cnt.search.sec[b_time.second]++;
	stats_cnt.search.tm_cnt[b_time.minute] += time;
	stats_cnt.search.tm_sec[b_time.second] += time;
	pthread_mutex_unlock(&manager()->qmtx);

	database_pool->checkin(&database);
	LOG(this, "FINISH SEARCH\n");
}


int HttpClient::_endpointgen(query_t &e, bool writable)
{
	int cmd, retval;
	bool has_node_name = false;
	struct http_parser_url u;
	std::string b = repr(path);

	LOG(this, "URL: %s\n", b.c_str());
	if (http_parser_parse_url(b.c_str(), b.size(), 0, &u) == 0) {
		LOG(this, "Parsing done\n");

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

				std::string index_path = ns + path;
				std::string node_name;
				Endpoint asked_node("xapian://" + node_name + index_path);
				std::vector<Endpoint> asked_nodes;

				if (p.len_host) {
					node_name = urldecode(p.off_host, p.len_host);
					has_node_name = true;
				} else {
					double timeout;
					int num_endps = 1;
					if (writable) {
						timeout = 2;
					} else timeout = 1;

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
					Node node;
					if (!manager()->touch_node(node_name, &node)) {
						LOG(this, "Node %s not found\n", node_name.c_str());
						host = node_name;
						return CMD_UNKNOWN_HOST;
					}
					if (!node_port) node_port = node.binary_port;
					inet_ntop(AF_INET, &(node.addr.sin_addr), node_ip, INET_ADDRSTRLEN);
					Endpoint endpoint("xapian://" + std::string(node_ip) + ":" + std::to_string(node_port) + index_path, NULL, -1, node_name);
					endpoints.insert(endpoint);
				} else {
					std::vector<Endpoint>::iterator it_endp = asked_nodes.begin();
					for ( ; it_endp != asked_nodes.end(); it_endp++) {
						endpoints.insert(*it_endp);
					}
				}
				LOG_CONN_WIRE(this, "Endpoint: -> %s\n", endpoints.as_string().c_str());

				retval = url_path(path_buf.c_str(), path_size, &p);
			}
		}
		if ((parser.method == 4 || parser.method ==24) && endpoints.size()>1) {
			return CMD_BAD_ENDPS;
		}

		cmd = identify_cmd(command);

		if (u.field_set & (1 <<  UF_QUERY)) {
			size_t query_size = u.field_data[4].len;
			std::string query_buf(b.c_str() + u.field_data[4].off, u.field_data[4].len);

			parser_query_t q;

			memset(&q, 0, sizeof(q));
			if (url_qs("pretty", query_buf.c_str(), query_size, &q) != -1) {
				std::string pretty = Serialise::boolean(urldecode(q.offset, q.length));
				(pretty.compare("f") == 0) ? e.pretty = false : e.pretty = true;
			} else {
				e.pretty = false;
			}

			if (cmd == CMD_SEARCH || cmd == CMD_FACETS) {

				e.unique_doc = false;

				memset(&q, 0, sizeof(q));
				if (url_qs("offset", query_buf.c_str(), query_size, &q) != -1) {
					e.offset = strtouint(urldecode(q.offset, q.length).c_str());
				} else {
					e.offset = 0;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("check_at_least", query_buf.c_str(), query_size, &q) != -1) {
					e.check_at_least = strtouint(urldecode(q.offset, q.length).c_str());
				} else {
					e.check_at_least = 0;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("limit", query_buf.c_str(), query_size, &q) != -1) {
					e.limit = strtouint(urldecode(q.offset, q.length).c_str());
				} else {
					e.limit = 10;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("collapse_max", query_buf.c_str(), query_size, &q) != -1) {
					e.collapse_max = strtouint(urldecode(q.offset, q.length).c_str());
				} else {
					e.collapse_max = 1;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("spelling", query_buf.c_str(), query_size, &q) != -1) {
					std::string spelling = Serialise::boolean(urldecode(q.offset, q.length));
					(spelling.compare("f") == 0) ? e.spelling = false : e.spelling = true;
				} else {
					e.spelling = true;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("synonyms", query_buf.c_str(), query_size, &q) != -1) {
					std::string synonyms = Serialise::boolean(urldecode(q.offset, q.length));
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
				while (url_qs("sort", query_buf.c_str(), query_size, &q) != -1) {
					e.sort.push_back(urldecode(q.offset, q.length));
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
				if (url_qs("collapse", query_buf.c_str(), query_size, &q) != -1) {
					e.collapse = urldecode(q.offset, q.length);
				} else {
					e.collapse = "";
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("fuzzy", query_buf.c_str(), query_size, &q) != -1) {
					std::string fuzzy = Serialise::boolean(urldecode(q.offset, q.length));
					(fuzzy.compare("f") == 0) ? e.is_fuzzy = false : e.is_fuzzy = true;
				} else {
					e.is_fuzzy = false;
				}

				if(e.is_fuzzy) {
					memset(&q, 0, sizeof(q));
					if (url_qs("fuzzy.n_rset", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.n_rset = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.fuzzy.n_rset = 5;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("fuzzy.n_eset", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.n_eset = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.fuzzy.n_eset = 32;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("fuzzy.n_term", query_buf.c_str(), query_size, &q) != -1){
						e.fuzzy.n_term = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.fuzzy.n_term = 10;
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
					std::string nearest = Serialise::boolean(urldecode(q.offset, q.length));
					(nearest.compare("f") == 0) ? e.is_nearest = false : e.is_nearest = true;
				} else {
					e.is_nearest = false;
				}

				if(e.is_nearest) {
					memset(&q, 0, sizeof(q));
					if (url_qs("nearest.n_rset", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.n_rset = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.nearest.n_rset = 5;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("nearest.n_eset", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.n_eset = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.nearest.n_eset = 32;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("nearest.n_term", query_buf.c_str(), query_size, &q) != -1){
						e.nearest.n_term = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.nearest.n_term = 10;
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

			} else if (cmd == CMD_ID) {
				memset(&q, 0, sizeof(q));
				if (url_qs("commit", query_buf.c_str(), query_size, &q) != -1) {
					std::string pretty = Serialise::boolean(urldecode(q.offset, q.length));
					(pretty.compare("f") == 0) ? e.commit = false : e.commit = true;
				} else {
					e.commit = false;
				}

				if (isRange(command)) {
					memset(&q, 0, sizeof(q));
					if (url_qs("offset", query_buf.c_str(), query_size, &q) != -1) {
						e.offset = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.offset = 0;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("check_at_least", query_buf.c_str(), query_size, &q) != -1) {
						e.check_at_least = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.check_at_least = 0;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("limit", query_buf.c_str(), query_size, &q) != -1) {
						e.limit = strtouint(urldecode(q.offset, q.length).c_str());
					} else {
						e.limit = 10;
					}

					memset(&q, 0, sizeof(q));
					if (url_qs("sort", query_buf.c_str(), query_size, &q) != -1) {
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
			} else if (cmd == CMD_STATS) {
				memset(&q, 0, sizeof(q));
				if (url_qs("server", query_buf.c_str(), query_size, &q) != -1) {
					std::string server = Serialise::boolean(urldecode(q.offset, q.length));
					(server.compare("f") == 0) ? e.server = false : e.server = true;
				} else {
					e.server = false;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("database", query_buf.c_str(), query_size, &q) != -1) {
					std::string database = Serialise::boolean(urldecode(q.offset, q.length));
					(database.compare("f") == 0) ? e.database = false : e.database = true;
				} else {
					e.database = false;
				}

				memset(&q, 0, sizeof(q));
				if (url_qs("document", query_buf.c_str(), query_size, &q) != -1) {
					e.document = urldecode(q.offset, q.length);
				} else {
					e.document = "";
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


std::string HttpClient::http_response(int status, int mode, int matched_count, std::string content)
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

		if (mode & HTTP_MATCHED_COUNT) {
			response += "X-Matched-count: " + std::to_string(matched_count) + eol;
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


int
HttpClient::identify_cmd(const std::string &commad)
{
	if (strcasecmp(commad.c_str(), HTTP_SEARCH) == 0) {
		return CMD_SEARCH;
	} else if (strcasecmp(commad.c_str(), HTTP_FACETS) == 0) {
		return CMD_FACETS;
	} else if (strcasecmp(commad.c_str(), HTTP_STATS) == 0) {
		return CMD_STATS;
	} else if (strcasecmp(commad.c_str(), HTTP_SCHEMA) == 0) {
		return CMD_SCHEMA;
	} else return CMD_ID;
}