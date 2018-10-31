/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "base_server.h"

#include "ignore_unused.h"                    // for ignore_unused
#include "log.h"                              // for L_OBJ
#include "readable_revents.hh"                // for readable_revents


BaseServer::BaseServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(parent_, ev_loop_, ev_flags_),
	  io(*ev_loop),
	  start_async(*ev_loop)
{
	io.set<BaseServer, &BaseServer::io_accept_cb>(this);

	start_async.set<BaseServer, &BaseServer::start_async_cb>(this);
	start_async.start();
	L_EV("Start async start event");

	L_OBJ("CREATED BASE SERVER!");
}


BaseServer::~BaseServer()
{
	destroyer();

	L_OBJ("DELETED BASE SERVER!");
}


void
BaseServer::start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("BaseServer::write_start_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	start_impl();
}


void
BaseServer::start()
{
	L_CALL("BaseServer::start()");

	start_async.send();
}


void
BaseServer::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("BaseServer::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
	}
}


void
BaseServer::destroy_impl()
{
	destroyer();
}


void
BaseServer::destroyer()
{
	L_CALL("BaseServer::destroyer()");

	io.stop();
	L_EV("Stop io accept event");

	start_async.stop();
	L_EV("Stop async start event");
}
