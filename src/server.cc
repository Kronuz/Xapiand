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

#include "server.h"


//
// Xapian Server.
//

pthread_mutex_t XapiandServer::static_mutex = PTHREAD_MUTEX_INITIALIZER;
int XapiandServer::total_clients  = 0;
int XapiandServer::http_clients   = 0;
int XapiandServer::binary_clients = 0;


XapiandServer::XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_)
	: Worker(manager_, loop_),
	  async_setup_node(*loop)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	async_setup_node.set<XapiandServer, &XapiandServer::async_setup_node_cb>(this);
	async_setup_node.start();
	LOG_EV(this, "\tStart async setup node event\n");

	LOG_OBJ(this, "CREATED XAPIAN SERVER!\n");
}


XapiandServer::~XapiandServer()
{
	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	LOG_OBJ(this, "DELETED XAPIAN SERVER!\n");
}


void XapiandServer::run()
{
	LOG_EV(this, "\tStarting server loop...\n");
	loop->run(0);
	LOG_EV(this, "\tServer loop ended!\n");
}


void XapiandServer::async_setup_node_cb(ev::async &watcher, int revents)
{
	manager()->setup_node(this);

	async_setup_node.stop();
	LOG_EV(this, "\tStop async setup node event\n");
}


void XapiandServer::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (servers.empty()) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	// Delete servers.
	while (!servers.empty()) {
		delete servers.back();
		servers.pop_back();
	}

	async_setup_node.stop();
	LOG_EV(this, "\tStop async setup node event\n");

	pthread_mutex_unlock(&qmtx);

	LOG_OBJ(this, "DESTROYED XAPIAN SERVER!\n");
}


void XapiandServer::shutdown()
{
	Worker::shutdown();

	if (manager()->shutdown_asap) {
		if (http_clients <= 0) {
			manager()->shutdown_now = manager()->shutdown_asap;
		}
		destroy();
	}
	if (manager()->shutdown_now) {
		break_loop();
	}
}


void XapiandServer::register_server(BaseServer *server) {
	pthread_mutex_lock(&qmtx);
	servers.push_back(server);
	pthread_mutex_unlock(&qmtx);
}