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

#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include "xapiand.h"

#include "threadpool.h"
#include "database.h"
#include "worker.h"
#include "manager.h"

#include "ev/ev++.h"
#include <list>


class XapiandServer : public Task, public Worker {
private:
	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	ev::io discovery_io;
	int discovery_sock;

	ev::io http_io;
	int http_sock;

	ev::io binary_io;
	int binary_sock;

	DatabasePool *database_pool;
	ThreadPool *thread_pool;

	void destroy();

	void io_accept_discovery(ev::io &watcher, int revents);

	void io_accept_http(ev::io &watcher, int revents);

#ifdef HAVE_REMOTE_PROTOCOL
	void io_accept_binary(ev::io &watcher, int revents);
#endif  /* HAVE_REMOTE_PROTOCOL */

public:
	XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_, int discovery_sock_,int http_sock_, int binary_sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_);
	~XapiandServer();

	void run();
	void shutdown();

	inline XapiandManager * manager() const {
		return static_cast<XapiandManager *>(_parent);
	}

	static pthread_mutex_t static_mutex;
	static int total_clients;
	static int http_clients;
	static int binary_clients;

protected:
	friend class BaseClient;
	friend class XapiandManager;

	bool trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
