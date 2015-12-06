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
#include "server_binary.h"
#include "server_http.h"
#include "server_discovery.h"
#include "server_raft.h"


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
	L_EV(this, "\tStart async setup node event");

	L_OBJ(this, "CREATED XAPIAN SERVER! [%llx]", this);
}


XapiandServer::~XapiandServer()
{
	destroy();

	L_OBJ(this, "DELETED XAPIAN SERVER! [%llx]", this);
}


void
XapiandServer::run()
{
	L_EV(this, "\tStarting server loop...");
	loop->run();
	L_EV(this, "\tServer loop ended!");
}


void
XapiandServer::async_setup_node_cb(ev::async &, int)
{
	L_EV_BEGIN(this, "XapiandServer::async_setup_cb:BEGIN");
	manager()->setup_node(share_this<XapiandServer>());

	async_setup_node.stop();
	L_EV(this, "\tStop async setup node event");
	L_EV_END(this, "XapiandServer::async_setup_cb:END");
}


void
XapiandServer::destroy()
{
	std::lock_guard<std::mutex> lk(qmtx);

	async_setup_node.stop();
	L_EV(this, "\tStop async setup node event");

	L_OBJ(this, "DESTROYED XAPIAN SERVER! [%llx]", this);
}


void
XapiandServer::shutdown()
{
	L_CALL(this, "XapiandServer::shutdown()");

	Worker::shutdown();

	time_t shutdown_asap = XapiandManager::shutdown_asap;
	if (shutdown_asap) {
		if (http_clients <= 0) {
			XapiandManager::shutdown_now.store(shutdown_asap);
		}
		destroy();
	}

	time_t shutdown_now = XapiandManager::shutdown_now;
	if (shutdown_now) {
		L_DEBUG(this, "Breaking Server loop!");
		break_loop();
	}
}
