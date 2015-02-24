#ifndef XAPIAND_INCLUDED_CLIENT_HTTP_H
#define XAPIAND_INCLUDED_CLIENT_HTTP_H

#include "http_parser.h"

#include "client_base.h"

//
//   A single instance of a non-blocking Xapiand HTTP protocol handler
//
class HttpClient : public BaseClient {
	struct http_parser parser;

	void read_cb(ev::io &watcher);

	static const http_parser_settings settings;

	std::string path;
	std::string body;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

public:
	void run();

	HttpClient(int s, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	~HttpClient();
};

#endif /* XAPIAND_INCLUDED_CLIENT_HTTP_H */
