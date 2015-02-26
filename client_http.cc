#include <sys/socket.h>

#include "utils.h"

#include "client_http.h"

//
// Xapian http client
//

HttpClient::HttpClient(ev::loop_ref &loop, int sock_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(loop, sock_, database_pool_, active_timeout_, idle_timeout_),
	  message_complete(false)
{
	parser.data = this;
	http_parser_init(&parser, HTTP_REQUEST);
	log(this, "Got connection (%d), %d http client(s) connected.\n", sock, ++total_clients);
}


HttpClient::~HttpClient()
{
	log(this, "Lost connection (%d), %d http client(s) connected.\n", sock, --total_clients);
}


void HttpClient::read_cb(ev::io &watcher)
{
	char buf[1024];

	ssize_t received = ::recv(watcher.fd, buf, sizeof(buf), 0);

	if (received < 0) {
		perror("read error");
		return;
	}

	if (received == 0) {
		// The peer has closed its half side of the connection.
		log(this, "BROKEN PIPE!\n");
		destroy();
	} else {
		size_t parsed = http_parser_execute(&parser, &settings, buf, received);
		if (parsed == received) {
			if (message_complete) {
				try {
					log(this, "PATH: '%s'\n", repr(path).c_str());
					log(this, "BODY: '%s'\n", repr(body).c_str());
					write("HTTP/1.1 200 OK\r\n"
						  "Content-Length: 3\r\n"
						  "Connection: close\r\n"
						  "\r\n"
						  "OK!");
					close();
				} catch (...) {
					log(this, "ERROR!\n");
				}
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
	HttpClient *self = static_cast<HttpClient *>(p->data);

	// log(self, "%3d. (INFO)\n", p->state);

	switch (p->state) {
		case 18:
			self->message_complete = true;
			break;
		case 19:
			self->message_complete = false;
			self->path.clear();
			self->body.clear();
			break;
	}

	return 0;
}


int HttpClient::on_data(http_parser* p, const char *at, size_t length) {
	HttpClient *self = static_cast<HttpClient *>(p->data);

	// log(self, "%3d. %s\n", p->state, repr(at, length).c_str());

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
