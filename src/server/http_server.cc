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

#include "http_server.h"

#include <errno.h>                          // for errno
#include <utility>

#include "base_server.h"                    // for BaseServer
#include "cassert.h"                        // for ASSERT
#include "error.hh"                         // for error:name, error::description
#include "ev/ev++.h"                        // for io, ::READ, loop_ref (ptr only)
#include "http.h"                           // for Http
#include "http_client.h"                    // for HttpClient
#include "ignore_unused.h"                  // for ignore_unused
#include "io.hh"                            // for ignored_errno
#include "log.h"                            // for L_EV, L_OBJ, L_CALL, L_ERR
#include "readable_revents.hh"              // for readable_revents
#include "worker.h"                         // for Worker


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


HttpServer::HttpServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const std::shared_ptr<Http>& http)
	: BaseServer<HttpServer>(parent_, ev_loop_, ev_flags_, http->port, "Http", TCP_TCP_NODELAY | TCP_TCP_DEFER_ACCEPT | TCP_SO_REUSEPORT),
	  http(http)
{
}


HttpServer::~HttpServer()
{
	Worker::deinit();
}


void
HttpServer::start_impl()
{
	L_CALL("HttpServer::start_impl()");

	Worker::start_impl();

#if defined(__linux) || defined(__linux__) || defined(linux) || defined(SO_REUSEPORT_LB)
	// In Linux, accept(2) on sockets using SO_REUSEPORT do a load balancing
	// of the incoming clients. It's not the case in other systems; FreeBSD is
	// adding SO_REUSEPORT_LB for that.
	http->close(true);
	bind(1);
	io.start(sock, ev::READ);
	L_EV("Start http's server accept event (sock=%d)", sock);
#else
	io.start(http->sock, ev::READ);
	L_EV("Start http's server accept event (sock=%d)", http->sock);
#endif
}


int
HttpServer::accept()
{
	L_CALL("HttpServer::accept()");

	if (sock != -1) {
		// If using SO_REUSEPORT for load balancing, this->sock
		// will be opened and binary->sock will not.
		return TCP::accept();
	}
	return http->accept();
}


void
HttpServer::io_accept_cb(ev::io& watcher, int revents)
{
	L_CALL("HttpServer::io_accept_cb(<watcher>, 0x%x (%s)) {sock: %d}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("HttpServer::io_accept_cb:BEGIN");
	L_EV_END("HttpServer::io_accept_cb:END");

	ignore_unused(watcher);
	ASSERT(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("HttpServer::io_accept_cb", "HttpServer::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid http event {sock:%d}: %s (%d): %s", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock == -1) {
		if (!io::ignored_errno(errno, true, true, false)) {
			L_ERR("ERROR: accept http error {sock:%d}: %s (%d): %s", watcher.fd, error::name(errno), errno, error::description(errno));
		}
	} else {
		Worker::make_shared<HttpClient>(share_this<HttpServer>(), ev_loop, ev_flags, client_sock);
	}
}
