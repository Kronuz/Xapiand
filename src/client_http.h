#ifndef XAPIAND_INCLUDED_CLIENT_HTTP_H
#define XAPIAND_INCLUDED_CLIENT_HTTP_H

#include "http_parser.h"

#include "client_base.h"

//
//   A single instance of a non-blocking Xapiand HTTP protocol handler
//
class HttpClient : public BaseClient {
	struct http_parser parser;

	void on_read(const char *buf, ssize_t received);

	static const http_parser_settings settings;

	std::string path;
	std::string body;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

public:
	HttpClient(ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	~HttpClient();
};

#endif /* XAPIAND_INCLUDED_CLIENT_HTTP_H */
