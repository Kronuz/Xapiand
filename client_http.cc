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

#include <assert.h>
#include <sys/socket.h>

#include "xapiand.h"
#include "utils.h"
#include "cJSON.h"

#include "client_http.h"

//
// Xapian http client
//

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
			break;
		case 62: // data
			self->body = std::string(at, length);
			break;
        case 50:
            self->host = std::string(at, length);
            break;
        
	}

	return 0;
}


void HttpClient::run()
{
    try {
        LOG_HTTP_PROTO(this, "METHOD: %d\n", parser.method);
        LOG_HTTP_PROTO(this, "PATH: '%s'\n", repr(path).c_str());
        LOG_HTTP_PROTO(this, "HOST: '%s'\n", repr(host).c_str());
        LOG_HTTP_PROTO(this, "BODY: '%s'\n", repr(body).c_str());
        if (path == "/quit") {
            server->manager->async_shutdown.send();
            return;
        }
        
        struct http_parser_url u;
        const char *b = repr(path).c_str();
        LOG_CONN_WIRE(this,"URL: %s\n",repr(path).c_str());
        if(http_parser_parse_url(b, strlen(b), 0, &u) == 0){
            LOG_CONN_WIRE(this,"Parsing done\n");
            
            if (u.field_set & (1 <<  UF_PATH )){
                char path_[u.field_data[3].len];
                memcpy(path_, b + u.field_data[3].off, u.field_data[3].len);
                path_[u.field_data[3].len] = '\0';
                
                LOG_CONN_WIRE(this,"PATH: -> %s END\n", path_);
                
                struct parser_url_path_t p;
                memset(&p, 0, sizeof(p));
                
                const char *n0 = path_;
                while (url_path(&n0, &p) == 0) {
                    std::string endp;
                    
                    if(!p.len_host){
                        endp = ("xapian://"+ host + urldecode(p.off_namespace, p.len_namespace)+ "/" + urldecode(p.off_path, p.len_path));
                        
                    } else {
                        endp = ("xapian://"+ urldecode(p.off_host, p.len_host)  + urldecode(p.off_namespace, p.len_namespace)+ "/" + urldecode(p.off_path, p.len_path));
                    }
                    
                    if(p.len_command){
                        command = urldecode(p.off_command, p.len_command);
                        LOG_CONN_WIRE(this,"command: -> %s\n", command.c_str());
                    }
                    
                    Endpoint endpoint = Endpoint(endp, std::string(), XAPIAND_HTTP_SERVERPORT);
                    endpoints.push_back(endpoint);
                    
                    LOG_CONN_WIRE(this,"Endpoint: -> %s\n", endp.c_str());
                    
                    /*if(strcasecmp("SEARCH",command.c_str())==0){
                     LOG_CONN_WIRE(this,"Es igual: %s\n",command.c_str());
                     } else {
                     LOG_CONN_WIRE(this,"No son iguales: %s\n",command.c_str());
                     }*/
                    
                }
            }
            
        } else LOG_CONN_WIRE(this,"Parsing not done\n");
        
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
        if (parser.state == 1) close();
    } catch (...) {
        LOG_ERR(this, "ERROR!\n");
    }
    io_read.start();
}
