#include <sys/socket.h>

#include "client_http.h"

//
// Xapian http client
//

HttpClient::HttpClient(int sock_, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(sock_, thread_pool_, database_pool_, active_timeout_, idle_timeout_)
{
	printf("Got connection, %d http client(s) connected.\n", ++total_clients);
}


HttpClient::~HttpClient()
{
	printf("Lost connection, %d http client(s) connected.\n", --total_clients);
}


void HttpClient::write_cb(ev::io &watcher)
{
	pthread_mutex_lock(&qmtx);

	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		Buffer* buffer = write_queue.front();

		// printf(">>> ");
		// print_string(std::string(buffer->dpos(), buffer->nbytes()));

		ssize_t written = write(watcher.fd, buffer->dpos(), buffer->nbytes());
		if (written < 0) {
			perror("read error");
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				write_queue.pop(buffer);
				delete buffer;
			}
		}
	}

	pthread_mutex_unlock(&qmtx);
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
		delete this;
	} else {
		http_parser_init(&parser, HTTP_REQUEST);
		size_t parsed = http_parser_execute(&parser, &settings, buf, received);
		if (parser.upgrade) {
			/* handle new protocol */
		} else if (parsed != received) {
			// Handle error. Just close the connection.
			delete this;
		}
	}
}

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

	switch (p->state) {
		case 32: // path
			data = std::string(at, length);
			printf("%2d. %s\n", p->state, data.c_str());
			break;
		case 62: // data
			data = std::string(at, length);
			printf("%2d. %s\n", p->state, data.c_str());
			break;
//		default:
//			data = std::string(at, length);
//			printf("%2d. %s\n", p->state, data.c_str());
//			break;
	}

	return 0;
}
