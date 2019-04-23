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

#include "remote_protocol_server.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "remote_protocol.h"                // for RemoteProtocol
#include "remote_protocol_client.h"         // for RemoteProtocolClient
#include "endpoint.h"                       // for Endpoints
#include "error.hh"                         // for error:name, error::description
#include "fs.hh"                            // for exists
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


RemoteProtocolServer::RemoteProtocolServer(const std::shared_ptr<RemoteProtocol>& remote_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: MetaBaseServer<RemoteProtocolServer>(remote_, ev_loop_, ev_flags_, "Remote", TCP_TCP_NODELAY | TCP_SO_REUSEPORT),
	  remote(*remote_)
{
	bind(hostname, serv, tries);

	L_EV("Start remote protocol's async trigger replication signal event");
}


RemoteProtocolServer::~RemoteProtocolServer() noexcept
{
	try {
		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
RemoteProtocolServer::start_impl()
{
	L_CALL("RemoteProtocolServer::start_impl()");

	Worker::start_impl();

	io.start(sock == -1 ? remote.sock : sock, ev::READ);
	L_EV("Start remote protocol's server accept event {{sock:{}}}", sock == -1 ? remote.sock : sock);
}


int
RemoteProtocolServer::accept()
{
	L_CALL("RemoteProtocolServer::accept()");

	if (sock != -1) {
		return TCP::accept();
	}
	return remote.accept();
}


void
RemoteProtocolServer::io_accept_cb([[maybe_unused]] ev::io& watcher, int revents)
{
	L_CALL("RemoteProtocolServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("RemoteProtocolServer::io_accept_cb:BEGIN");
	L_EV_END("RemoteProtocolServer::io_accept_cb:END");

	assert(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("RemoteProtocolServer::io_accept_cb", "RemoteProtocolServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid remote protocol event {{sock:{}}}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock != -1) {
		auto client = Worker::make_shared<RemoteProtocolClient>(share_this<RemoteProtocolServer>(), ev_loop, ev_flags, active_timeout, idle_timeout);

		if (!client->init_remote(client_sock)) {
			io::close(client_sock);
			client->detach();
			return;
		}

		client->start();
	}
}


std::string
RemoteProtocolServer::__repr__() const
{
	return string::format(STEEL_BLUE + "<RemoteProtocolServer {{cnt:{}, sock:{}}}{}{}{}>",
		use_count(),
		sock == -1 ? remote.sock : sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}

#endif /* XAPIAND_CLUSTERING */
