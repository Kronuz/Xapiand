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

#include "binary_server.h"

#ifdef XAPIAND_CLUSTERING

#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "binary.h"                         // for Binary
#include "binary_client.h"                  // for BinaryClient
#include "cassert.h"                        // for ASSERT
#include "endpoint.h"                       // for Endpoints
#include "error.hh"                         // for error:name, error::description
#include "fs.hh"                            // for exists
#include "ignore_unused.h"                  // for ignore_unused
#include "manager.h"                        // for XapiandManager
#include "readable_revents.hh"              // for readable_revents
#include "repr.hh"                          // for repr
#include "tcp.h"                            // for TCP::socket


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_EV
// #define L_EV L_MEDIUM_PURPLE


BinaryServer::BinaryServer(const std::shared_ptr<Binary>& binary_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: MetaBaseServer<BinaryServer>(binary_, ev_loop_, ev_flags_, "Binary", TCP_TCP_NODELAY | TCP_SO_REUSEPORT),
	  binary(*binary_)
{
	bind(hostname, serv, tries);

	L_EV("Start binary's async trigger replication signal event");
}


BinaryServer::~BinaryServer() noexcept
{
	try {
		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
BinaryServer::start_impl()
{
	L_CALL("BinaryServer::start_impl()");

	Worker::start_impl();

	io.start(sock == -1 ? binary.sock : sock, ev::READ);
	L_EV("Start binary's server accept event {sock:%d}", sock == -1 ? binary.sock : sock);
}


int
BinaryServer::accept()
{
	L_CALL("BinaryServer::accept()");

	if (sock != -1) {
		return TCP::accept();
	}
	return binary.accept();
}


void
BinaryServer::io_accept_cb(ev::io& watcher, int revents)
{
	L_CALL("BinaryServer::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("BinaryServer::io_accept_cb:BEGIN");
	L_EV_END("BinaryServer::io_accept_cb:END");

	ignore_unused(watcher);
	ASSERT(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("BinaryServer::io_accept_cb", "BinaryServer::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid binary event {sock:%d}: %s (%d): %s", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock != -1) {
		auto client = Worker::make_shared<BinaryClient>(share_this<BinaryServer>(), ev_loop, ev_flags, client_sock, active_timeout, idle_timeout);

		if (!client->init_remote()) {
			client->detach();
			return;
		}

		client->start();
	}
}


std::string
BinaryServer::__repr__() const
{
	return string::format("<BinaryServer {cnt:%ld, sock:%d}%s%s%s>",
		use_count(),
		sock == -1 ? binary.sock : sock,
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}

#endif /* XAPIAND_CLUSTERING */
