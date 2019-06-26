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

#include "http.h"

#include "config.h"                           // for XAPIAND_HTTP_SERVERPORT, XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION

#include "tcp.h"                              // for BaseTCP, TCP_TCP_NODELAY, TCP_TCP_DEFER_ACCEPT
#include "http_server.h"                      // For RemoteProtocolServer
#include "manager.h"                          // for XapiandManager
#include "log.h"                              // for L_OBJ
#include "node.h"                             // for Node::local_node


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


Http::Http(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: BaseTCP(parent_, ev_loop_, ev_flags_, "HTTP", TCP_TCP_NODELAY | TCP_TCP_DEFER_ACCEPT)
{
	bind(hostname, serv, tries);
}


void
Http::shutdown_impl(long long asap, long long now)
{
	L_CALL("Http::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		auto manager = XapiandManager::manager();
		if (now != 0 || (manager && !manager->http_clients)) {
			if (manager) {
				manager->http_server_pool->finish();
				manager->http_client_pool->finish();
			}
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


std::string
Http::getDescription() const
{
	return strings::format("TCP {}:{} ({} v{}.{})", addr.sin_addr.s_addr ? inet_ntop(addr) : "", ntohs(addr.sin_port), description, XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION, XAPIAND_HTTP_PROTOCOL_MINOR_VERSION);
}


void
Http::start()
{
	L_CALL("Http::start()");

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			std::static_pointer_cast<HttpServer>(child)->start();
		}
	}
}

std::string
Http::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Http {{cnt:{}}}{}{}{}>",
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}
