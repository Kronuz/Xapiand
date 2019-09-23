/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <utility>

#include "base_server.h"                    // for BaseServer
#include "error.hh"                         // for error:name, error::description
#include "ev/ev++.h"                        // for io, ::READ, loop_ref (ptr only)
#include "http.h"                           // for Http
#include "http_client.h"                    // for HttpClient
#include "io.hh"                            // for ignored_errno
#include "log.h"                            // for L_EV, L_OBJ, L_CALL, L_ERR
#include "manager.h"                        // for XapiandManager
#include "readable_revents.hh"              // for readable_revents
#include "worker.h"                         // for Worker


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_EV
// #define L_EV L_MEDIUM_PURPLE


HttpServer::HttpServer(const std::shared_ptr<Http>& http_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: MetaBaseServer<HttpServer>(http_, ev_loop_, ev_flags_, "Http", TCP_TCP_NODELAY | TCP_TCP_DEFER_ACCEPT | TCP_SO_REUSEPORT),
	  http(*http_)
{
	bind(hostname, serv, tries);
}


HttpServer::~HttpServer() noexcept
{
	try {
		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
HttpServer::shutdown_impl(long long asap, long long now)
{
	L_CALL("HttpServer::stop_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);  // immediately stop (accept no more connections)

		auto manager = XapiandManager::manager();
		if (now != 0 || !manager || manager->ready_to_end_http()) {
			destroy(false);
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


void
HttpServer::start_impl()
{
	L_CALL("HttpServer::start_impl()");

	Worker::start_impl();

	io.start(sock == -1 ? http.sock : sock, ev::READ);
	L_EV("Start http's server accept event {{sock:{}}}", sock == -1 ? http.sock : sock);
}


int
HttpServer::accept()
{
	L_CALL("HttpServer::accept()");

	if (sock != -1) {
		return TCP::accept();
	}
	return http.accept();
}


void
HttpServer::io_accept_cb([[maybe_unused]] ev::io& watcher, int revents)
{
	L_CALL("HttpServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock: {}}}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("HttpServer::io_accept_cb:BEGIN");
	L_EV_END("HttpServer::io_accept_cb:END");

	assert(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("HttpServer::io_accept_cb", "HttpServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid http event {{sock:{}}}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock != -1) {
		auto client = Worker::make_shared<HttpClient>(share_this<HttpServer>(), ev_loop, ev_flags);

		if (!client->init(client_sock)) {
			io::close(client_sock);
			client->detach();
			client.reset();
			detach_children();
			return;
		}

		client->start();
	}
}


std::string
HttpServer::__repr__() const
{
	return strings::format(STEEL_BLUE + "<HttpServer {{cnt:{}, sock:{}}}{}{}{}>",
		use_count(),
		sock == -1 ? http.sock : sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}
