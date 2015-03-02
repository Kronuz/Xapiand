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

#ifndef XAPIAND_INCLUDED_MANAGER_H
#define XAPIAND_INCLUDED_MANAGER_H

#include "database.h"
#include "threadpool.h"

#include <list>
#include <ev++.h>
#include <pthread.h>


class XapiandServer;
class BaseClient;


class XapiandManager {
	int http_port, http_sock;
	int binary_port, binary_sock;

	DatabasePool database_pool;
	ThreadPool thread_pool;

	ev::async break_loop;
	void break_loop_cb(ev::async &watcher, int revents);

	void check_tcp_backlog(int tcp_backlog);
	void shutdown_cb(ev::async &watcher, int revents);
	void bind_http();
	void bind_binary();
	void destroy();

	std::list<XapiandServer *>::const_iterator attach_server(XapiandServer *server);
	void detach_server(XapiandServer *server);

protected:
	friend XapiandServer;
	pthread_mutex_t servers_mutex;
	pthread_mutexattr_t servers_mutex_attr;
	std::list<XapiandServer *>servers;

public:
	static ev::default_loop default_loop;
	time_t shutdown_asap = (time_t)0;
	time_t shutdown_now = (time_t)0;
	ev::async async_shutdown;

	XapiandManager(int http_port_, int binary_port_);
	~XapiandManager();

	void run(int num_servers);
	void sig_shutdown_handler(int sig);
	void shutdown();
};


#endif /* XAPIAND_INCLUDED_MANAGER_H */
