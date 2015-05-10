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

#ifndef XAPIAND_INCLUDED_DISCOVERY_H
#define XAPIAND_INCLUDED_DISCOVERY_H

#include "xapiand.h"

#include "threadpool.h"

#include <assert.h>
#include <ev++.h>
#include <netinet/in.h>

#include <string>
#include <unordered_map>


class XapiandManager;

struct Node {
	std::string name;
	struct sockaddr_in addr;
	int http_port;
	int binary_port;
	time_t touched;
};


enum discovery_type {
	DISCOVERY_HELLO,    // New node saying hello
	DISCOVERY_WAVE,     // Nodes waving hello to the new node
	DISCOVERY_SNEER,    // Nodes telling the client they don't agree on the new node's name
    DISCOVERY_PING,     // Ping
    DISCOVERY_PONG,     // Pong
    DISCOVERY_BYE,      // Node says goodbye
    DISCOVERY_MAX
};


class Discovery : public Task {
private:
	ev::dynamic_loop dynamic_loop;
	ev::loop_ref *loop;
	ev::async break_loop;

	ev::io discovery_io;
	ev::timer discovery_heartbeat;

	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	std::string cluster_name;
	std::string node_name;

	struct sockaddr_in discovery_addr;
	int discovery_port, discovery_sock;

	void destroy();

	void discovery_heartbeat_cb(ev::timer &watcher, int revents);
	void io_accept_discovery(ev::io &watcher, int revents);
	void discovery(const char *message, size_t size);

public:
	XapiandManager *manager;

	std::unordered_map<std::string, Node> nodes;

	Discovery(XapiandManager *manager_, ev::loop_ref *loop_, const char *cluster_name_, const char *node_name_, const char *group_, int port_);
	~Discovery();

	void run();
	void shutdown();

	void discovery(discovery_type type, Node &node);

protected:
	friend class XapiandManager;
};

#endif /* XAPIAND_INCLUDED_DISCOVERY_H */
