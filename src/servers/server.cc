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

#include <algorithm>             // for move
#include <chrono>                // for operator""ms
#include <ratio>                 // for ratio
#include <type_traits>           // for remove_reference<>::type

#include "log.h"                 // for L_EV, L_OBJ, L_CALL, L_EV_BEGIN
#include "manager.h"             // for XapiandManager, XapiandManager::manager
#include "utils.h"               // for readable_revents


std::atomic_int XapiandServer::total_clients(0);
std::atomic_int XapiandServer::http_clients(0);
std::atomic_int XapiandServer::binary_clients(0);
std::atomic_int XapiandServer::max_total_clients(0);
std::atomic_int XapiandServer::max_http_clients(0);
std::atomic_int XapiandServer::max_binary_clients(0);


XapiandServer::XapiandServer(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(std::move(manager_), ev_loop_, ev_flags_),
	  setup_node_async(*ev_loop)
{
	setup_node_async.set<XapiandServer, &XapiandServer::setup_node_async_cb>(this);
	setup_node_async.start();
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
	L_CALL(this, "XapiandServer::run()");

	L_EV(this, "Starting server loop...");
	run_loop();
	L_EV(this, "Server loop ended!");

	detach();
}


void
XapiandServer::setup_node_async_cb(ev::async&, int revents)
{
	L_CALL(this, "XapiandServer::setup_node_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str()); (void)revents;

	L_EV_BEGIN(this, "XapiandServer::setup_async_cb:BEGIN");
	XapiandManager::manager->setup_node(share_this<XapiandServer>());

	setup_node_async.stop();
	L_EV(this, "Stop server's async setup node event");
	L_EV_END(this, "XapiandServer::setup_async_cb:END");
}


void
XapiandServer::destroy_impl()
{
	destroyer();
}


void
XapiandServer::destroyer()
{
	L_CALL(this, "XapiandServer::destroyer()");

	std::lock_guard<std::mutex> lk(qmtx);

	setup_node_async.stop();
	L_EV(this, "Stop server's async setup node event");
}


void
XapiandServer::shutdown_impl(time_t asap, time_t now)
{
	L_CALL(this, "XapiandServer::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now) {
		detach();
		break_loop();
	}
}
