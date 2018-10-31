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

#include "http.h"

#include "config.h"                           // for XAPIAND_HTTP_SERVERPORT, XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION

#include "base_tcp.h"                         // for BaseTCP, CONN_TCP_DEFER_ACCEPT, CONN_...
#include "http_server.h"                      // For BinaryServer
#include "log.h"                              // for L_OBJ
#include "node.h"                             // for Node::local_node


Http::Http(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_)
	: BaseTCP(parent_, ev_loop_, ev_flags_, port_, "HTTP", port_ == XAPIAND_HTTP_SERVERPORT ? 10 : 1, CONN_TCP_NODELAY | CONN_TCP_DEFER_ACCEPT)
{
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->http_port = port;
	Node::local_node(std::shared_ptr<const Node>(node_copy.release()));

	L_OBJ("CREATED CONFIGURATION FOR HTTP");
}


Http::~Http()
{
	L_OBJ("DELETED CONFIGURATION FOR HTTP");
}


std::string
Http::getDescription() const noexcept
{
	return "TCP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_HTTP_PROTOCOL_MINOR_VERSION) + ")";
}


void
Http::add_server(const std::shared_ptr<HttpServer>& server)
{
	std::lock_guard<std::mutex> lk(bsmtx);
	servers_weak.push_back(server);
}


void
Http::start()
{
	std::lock_guard<std::mutex> lk(bsmtx);
	for (auto it = servers_weak.begin(); it != servers_weak.end(); ) {
		auto server = it->lock();
		if (server) {
			server->start();
			++it;
		} else {
			it = servers_weak.erase(it);
		}
	}
}