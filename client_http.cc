#include <sys/socket.h>

#include "client_http.h"

//
// Xapian http client
//

HttpClient::HttpClient(ev::loop_ref &loop, int sock_, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(loop, sock_, thread_pool_, database_pool_, active_timeout_, idle_timeout_)
{
	printf("Got connection, %d http client(s) connected.\n", ++total_clients);
}


HttpClient::~HttpClient()
{
	printf("Lost connection, %d http client(s) connected.\n", --total_clients);
}


void HttpClient::read_cb(ev::io &watcher)
{
	char buf[1024];

	ssize_t received = recv(watcher.fd, buf, sizeof(buf), 0);

	if (received < 0) {
		perror("read error");
		return;
	}

	if (received == 0) {
		// Gack - we're deleting ourself inside of ourself!
		destroy();
	} else {
		http_parser_init(&parser, HTTP_REQUEST);
		parser.data = this;
		size_t parsed = http_parser_execute(&parser, &settings, buf, received);
		if (parsed == received) {
			try {
				printf("PATH: %s\n", path.c_str());
				printf("BODY: %s\n", body.c_str());
				write("HTTP/1.1 200 OK\r\n"
					  "Connection: close\r\n"
					  "\r\n"
					  "OK!");
				close();
			} catch (...) {
				printf("ERROR!\n");
			}
		} else {
			// Handle error. Just close the connection.
			destroy();
		}
	}
}


//
// HTTP parser callbacks.
//

const http_parser_settings HttpClient::settings = {
	.on_message_begin = on_info,
	.on_headers_complete = on_info,
	.on_message_complete = on_info,
	.on_header_field = on_data,
	.on_header_value = on_data,
	.on_url = on_data,
	.on_status = on_data,
	.on_body = on_data
};


int HttpClient::on_info(http_parser* p) {
	return 0;
}


int HttpClient::on_data(http_parser* p, const char *at, size_t length) {
	std::string data;
	HttpClient *self = static_cast<HttpClient *>(p->data);

	// printf("%3d. %s\n", p->state, std::string(at, length).c_str());

	switch (p->state) {
		case 32: // path
			self->path = std::string(at, length);
			break;
		case 62: // data
			self->body = std::string(at, length);
			break;
	}

	return 0;
}
