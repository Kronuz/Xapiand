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
	std::string host;
	std::string command; //command or ID

	bool ishost = false;

	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

public:
	HttpClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	~HttpClient();

	void run();
	void _delete();
	void _index();
	void _search();
	void _endpointgen(struct query_t &e);
	std::string http_header_responde(int status, bool Content_json, bool Content_length, bool chunked, std::string size, std::string content);
};

#endif /* XAPIAND_INCLUDED_CLIENT_HTTP_H */