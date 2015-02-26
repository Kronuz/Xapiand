#include <sys/socket.h>

#include "utils.h"

#include "client_http.h"

//
// Xapian http client
//

HttpClient::HttpClient(ev::loop_ref &loop, int sock_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(loop, sock_, database_pool_, active_timeout_, idle_timeout_)
{
	parser.data = this;
	http_parser_init(&parser, HTTP_REQUEST);
	LOG_CONN(this, "Got connection (sock=%d), %d http client(s) connected.\n", sock, ++total_clients);
}


HttpClient::~HttpClient()
{
	LOG_CONN(this, "Lost connection (sock=%d), %d http client(s) connected.\n", sock, --total_clients);
}


void HttpClient::read_cb(ev::io &watcher)
{
	char buf[1024];

	ssize_t received = ::read(watcher.fd, buf, sizeof(buf));

	if (received < 0) {
		if (errno != EAGAIN) LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", sock, strerror(errno));
		return;
	}

	if (received == 0) {
		// The peer has closed its half side of the connection.
		LOG_CONN(this, "Received EOF (sock=%d)!\n", sock);
		destroy();
	} else {
		LOG_CONN(this, "(sock=%d) -->> '%s'\n", sock, repr(buf, received).c_str());

		size_t parsed = http_parser_execute(&parser, &settings, buf, received);
		if (parsed == received) {
			if (parser.state == 1 || parser.state == 18) { // dead or message_complete
				try {
					LOG_HTTP_PROTO(this, "METHOD: %d\n", parser.method);
					LOG_HTTP_PROTO(this, "PATH: '%s'\n", repr(path).c_str());
					LOG_HTTP_PROTO(this, "BODY: '%s'\n", repr(body).c_str());
					write("HTTP/1.1 200 OK\r\n"
						  "Content-Length: 3\r\n"
						  "Connection: close\r\n"
						  "\r\n"
						  "OK!");
					close();
				} catch (...) {
					LOG_ERR(this, "ERROR!\n");
				}
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
	}

	return 0;
}
