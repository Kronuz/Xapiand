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

#include "binary.h"

#ifdef XAPIAND_CLUSTERING

#include <netinet/tcp.h>                      // for TCP_NODELAY

#include "binary_server.h"                    // For BinaryServer
#include "endpoint.h"                         // for Endpoint
#include "io.hh"                              // for io::*
#include "node.h"                             // for Node, local_node
#include "remote_protocol.h"                  // for XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION, XAPIAN_REMOTE_PROTOCOL_MAINOR_VERSION


Binary::Binary(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port)
	: BaseTCP(parent_, ev_loop_, ev_flags_, port, "Binary", TCP_TCP_NODELAY, port == XAPIAND_BINARY_SERVERPORT ? 10 : 1)
{
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->binary_port = port;
	Node::local_node(std::shared_ptr<const Node>(node_copy.release()));
}


std::string
Binary::getDescription() const
{
	std::string proxy((port == XAPIAND_BINARY_SERVERPORT && XAPIAND_BINARY_SERVERPORT != XAPIAND_BINARY_PROXY) ? "->" + std::to_string(XAPIAND_BINARY_PROXY) : "");
	return "TCP:" + std::to_string(port) + proxy + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + ")";
}


void
Binary::add_server(const std::shared_ptr<BinaryServer>& server)
{
	std::lock_guard<std::mutex> lk(bsmtx);
	servers_weak.push_back(server);
}


void
Binary::start()
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


void
Binary::process_tasks()
{
	std::lock_guard<std::mutex> lk(bsmtx);
	for (auto it = servers_weak.begin(); it != servers_weak.end(); ) {
		auto server = it->lock();
		if (server) {
			server->process_tasks();
			++it;
		} else {
			it = servers_weak.erase(it);
		}
	}
}


void
Binary::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint, bool cluster_database)
{
	tasks.enqueue([
		src_endpoint,
		dst_endpoint,
		cluster_database
	] (const std::shared_ptr<BinaryServer>& server) mutable {
		server->trigger_replication(src_endpoint, dst_endpoint, cluster_database);
	});

	process_tasks();
}


#endif  /* XAPIAND_CLUSTERING */
