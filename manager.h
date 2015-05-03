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

#ifndef XAPIAND_INCLUDED_MANAGER_H
#define XAPIAND_INCLUDED_MANAGER_H

#include "xapiand.h"

#include "database.h"
#include "threadpool.h"

#include <list>
#include <ev++.h>
#include <pthread.h>
#include <netinet/in.h>


enum gossip_type {
    GOSSIP_PING,		// Ping
    GOSSIP_PONG,		// Pong
    GOSSIP_DEATH,		// Announce death
    GOSSIP_MAX
};


class XapiandServer;
class BaseClient;


class XapiandManager {
	ev::dynamic_loop dynamic_loop;
	ev::loop_ref *loop;

	ev::io gossip_io;
	ev::timer gossip_heartbeat;

	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	struct sockaddr_in host_addr;

	struct sockaddr_in gossip_addr;
	int gossip_port, gossip_sock;
	int http_port, http_sock;
	int binary_port, binary_sock;

	static pcre *compiled_time_re;

	DatabasePool database_pool;
	ThreadPool thread_pool;

	ev::async break_loop;
	void break_loop_cb(ev::async &watcher, int revents);

	void gossip_heartbeat_cb(ev::timer &watcher, int revents);
	void gossip_io_cb(ev::io &watcher, int revents);
	void gossip(const char *message, size_t size);
	void gossip(gossip_type type);

	void check_tcp_backlog(int tcp_backlog);
	void shutdown_cb(ev::async &watcher, int revents);
	struct sockaddr_in host_address();
	bool bind_tcp(const char *type, int &sock, int &port, struct sockaddr_in &addr, int tries);
	bool bind_udp(const char *type, int &sock, int &port, struct sockaddr_in &addr, int tries, const char *group);
	void destroy();

	std::list<XapiandServer *>::const_iterator attach_server(XapiandServer *server);
	void detach_server(XapiandServer *server);

protected:
	friend class XapiandServer;
	pthread_mutex_t servers_mutex;
	pthread_mutexattr_t servers_mutex_attr;
	std::list<XapiandServer *>servers;

public:
	time_t shutdown_asap;
	time_t shutdown_now;
	ev::async async_shutdown;

	XapiandManager(ev::loop_ref *loop_, const char *gossip_group_, int gossip_port_, int http_port_, int binary_port_);
	~XapiandManager();

	void run(int num_servers);
	void sig_shutdown_handler(int sig);
	void shutdown();
	cJSON* server_status();
	cJSON* get_stats_time(const std::string &time_req);
	cJSON* get_stats_json(pos_time_t first_time, pos_time_t second_time);
};


#endif /* XAPIAND_INCLUDED_MANAGER_H */
