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
std::atomic_int XapiandServer::max_total_clients(0);
std::atomic_int XapiandServer::max_http_clients(0);
std::atomic_int XapiandServer::max_binary_clients(0);


XapiandServer::XapiandServer(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(std::move(manager_), ev_loop_, ev_flags_),
	  async_setup_node(*ev_loop)
{
	async_setup_node.set<XapiandServer, &XapiandServer::async_setup_node_cb>(this);
	async_setup_node.start();
	L_EV(this, "Start server's async setup node event");

	L_OBJ(this, "CREATED XAPIAN SERVER!");
}


XapiandServer::~XapiandServer()
{
	destroyer();

	L_OBJ(this, "DELETED XAPIAN SERVER!");
}


void
XapiandServer::run()
{
	L_EV(this, "Starting server loop...");
	ev_loop->run();
	L_EV(this, "Server loop ended!");

	cleanup();
}


void
XapiandServer::async_setup_node_cb(ev::async&, int)
{
	L_EV_BEGIN(this, "XapiandServer::async_setup_cb:BEGIN");
	manager()->setup_node(share_this<XapiandServer>());

	async_setup_node.stop();
	L_EV(this, "Stop server's async setup node event");
	L_EV_END(this, "XapiandServer::async_setup_cb:END");
}


void
XapiandServer::destroy_impl()
{
	destroyer();
}


void
XapiandServer::destroyer()
{
	L_OBJ(this, "DESTROYING XAPIAN SERVER!");

	std::lock_guard<std::mutex> lk(qmtx);

	async_setup_node.stop();
	L_EV(this, "Stop server's async setup node event");

	L_OBJ(this, "DESTROYED XAPIAN SERVER!");
}


void
XapiandServer::shutdown_impl(time_t asap, time_t now)
{
	L_OBJ(this , "SHUTDOWN XAPIAN SERVER! (%d %d)", asap, now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now) {
		detach();
		break_loop();
	}
}
