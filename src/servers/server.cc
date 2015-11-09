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

#include "server_base.h"


std::mutex XapiandServer::static_mutex;
std::atomic_int XapiandServer::total_clients(0);
std::atomic_int XapiandServer::http_clients(0);
std::atomic_int XapiandServer::binary_clients(0);


XapiandServer::XapiandServer(std::shared_ptr<XapiandManager> manager_, ev::loop_ref *loop_)
	: Worker(std::move(manager_), loop_),
	  async_setup_node(*loop)
{
	async_setup_node.set<XapiandServer, &XapiandServer::async_setup_node_cb>(this);
	async_setup_node.start();
	LOG_EV(this, "\tStart async setup node event\n");

	LOG_OBJ(this, "CREATED XAPIAN SERVER!\n");
}


XapiandServer::~XapiandServer()
{
	destroy();

	LOG_OBJ(this, "DELETED XAPIAN SERVER!\n");
}


void
XapiandServer::run()
{
	LOG_EV(this, "\tStarting server loop...\n");
	loop->run();
	LOG_EV(this, "\tServer loop ended!\n");
}


void
XapiandServer::async_setup_node_cb(ev::async &, int)
{
	manager()->setup_node(share_this<XapiandServer>());

	async_setup_node.stop();
	LOG_EV(this, "\tStop async setup node event\n");
}


void
XapiandServer::destroy()
{
	std::lock_guard<std::mutex> lk(qmtx);

	async_setup_node.stop();
	LOG_EV(this, "\tStop async setup node event\n");

	LOG_OBJ(this, "DESTROYED XAPIAN SERVER!\n");
}


void
XapiandServer::shutdown()
{
	std::unique_lock<std::mutex> lk(qmtx);
	for (auto & server : servers) {
		server->shutdown();
	}
	lk.unlock();

	Worker::shutdown();

	time_t shutdown_asap = manager()->shutdown_asap;
	if (shutdown_asap) {
		if (http_clients <= 0) {
			manager()->shutdown_now.store(shutdown_asap);
		}
		destroy();
	}

	time_t shutdown_now = manager()->shutdown_now;
	if (shutdown_now) {
		LOG_OBJ(this, "Breaking Server loop!\n");
		break_loop();
	}
}


void
XapiandServer::register_server(std::unique_ptr<BaseServer>&& server)
{
	std::lock_guard<std::mutex> lk(qmtx);
	servers.push_back(std::move(server));
}
