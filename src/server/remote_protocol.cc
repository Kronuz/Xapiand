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

#include "remote_protocol.h"

#ifdef XAPIAND_CLUSTERING

#include <netinet/tcp.h>                      // for TCP_NODELAY

#include "remote_protocol_server.h"           // For RemoteProtocolServer
#include "endpoint.h"                         // for Endpoint
#include "io.hh"                              // for io::*
#include "manager.h"                          // for XapiandManager
#include "node.h"                             // for Node, local_node
#include "remote_protocol_client.h"           // for XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION, XAPIAN_REMOTE_PROTOCOL_MAINOR_VERSION


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


RemoteProtocol::RemoteProtocol(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: BaseTCP(parent_, ev_loop_, ev_flags_, "Remote", TCP_TCP_NODELAY)
{
	bind(hostname, serv, tries);
}


void
RemoteProtocol::shutdown_impl(long long asap, long long now)
{
	L_CALL("RemoteProtocol::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		if (now != 0 || !XapiandManager::remote_clients()) {
			XapiandManager::remote_server_pool()->finish();
			XapiandManager::remote_client_pool()->finish();
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


void
RemoteProtocol::start()
{
	L_CALL("RemoteProtocol::start()");

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			std::static_pointer_cast<RemoteProtocolServer>(child)->start();
		}
	}
}


std::string
RemoteProtocol::__repr__() const
{
	return strings::format(STEEL_BLUE + "<RemoteProtocol {{cnt:{}}}{}{}{}>",
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}


std::string
RemoteProtocol::getDescription() const
{
	return strings::format("TCP {}:{} ({} v{}.{})", addr.sin_addr.s_addr ? inet_ntop(addr) : "", ntohs(addr.sin_port), description, XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION, XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION);
}

#endif  /* XAPIAND_CLUSTERING */
