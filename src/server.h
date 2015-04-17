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

#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include "xapiand.h"

#include "threadpool.h"
#include "database.h"
#include "manager.h"

#include <ev++.h>
#include <list>


class XapiandServer : public Task {
private:
	ev::dynamic_loop dynamic_loop;
	ev::loop_ref *loop;
	ev::async break_loop;

	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	ev::io http_io;
	int http_sock;

	ev::io binary_io;
	int binary_sock;

	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	void destroy();
	void bind_http();
	void bind_binary();

	void io_accept_http(ev::io &watcher, int revents);
#ifdef HAVE_REMOTE_PROTOCOL
	void io_accept_binary(ev::io &watcher, int revents);
#endif  /* HAVE_REMOTE_PROTOCOL */

	void break_loop_cb(ev::async &watcher, int revents);

public:
	XapiandManager *manager;
	pthread_mutex_t clients_mutex;
	pthread_mutexattr_t clients_mutex_attr;
	std::list<BaseClient *>clients;

	XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_, int http_sock_, int binary_sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_);
	~XapiandServer();

	void run();
	void shutdown();

	static pthread_mutex_t static_mutex;
	static int total_clients;
	static int http_clients;
	static int binary_clients;

protected:
	friend class BaseClient;
	friend class XapiandManager;
	std::list<XapiandServer *>::const_iterator iterator;
	std::list<BaseClient *>::const_iterator attach_client(BaseClient *client);
	void detach_client(BaseClient *client);
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
