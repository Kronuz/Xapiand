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

#include "endpoint.h"                         // for Endpoint
#include "io.hh"                              // for io::*
#include "node.h"                             // for Node, local_node

#include "remote_protocol.h"
#include "binary_server.h"


Binary::Binary(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_)
	: BaseTCP(parent_, ev_loop_, ev_flags_, port_, "Binary", port_ == XAPIAND_BINARY_SERVERPORT ? 10 : 1, CONN_TCP_NODELAY)
{
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->binary_port = port;
	Node::local_node(std::shared_ptr<const Node>(node_copy.release()));

	L_OBJ("CREATED CONFIGURATION FOR BINARY");
}


Binary::~Binary()
{
	L_OBJ("DELETED CONFIGURATION FOR BINARY");
}


std::string
Binary::getDescription() const noexcept
{
	std::string proxy((port == XAPIAND_BINARY_SERVERPORT && XAPIAND_BINARY_SERVERPORT != XAPIAND_BINARY_PROXY) ? "->" + std::to_string(XAPIAND_BINARY_PROXY) : "");
	return "TCP:" + std::to_string(port) + proxy + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + ")";
}


int
Binary::connection_socket()
{
	int client_sock;
	int optval = 1;

	if ((client_sock = io::socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		L_ERR("ERROR: cannot create binary connection: [%d] %s", errno, strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

	// if (io::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_KEEPALIVE (sock=%d): [%d] %s", sock, errno, strerror(errno));
	// }

	// struct linger ling = {0, 0};
	// if (io::setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_LINGER (sock=%d): %s", sock, strerror(errno));
	// }

	if (flags & CONN_TCP_NODELAY) {
		if (io::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_NODELAY (sock=%d): %s", sock, strerror(errno));
		}
	}

	return client_sock;
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
		// TODO: Start servers
		// auto server = it->lock();
		// server->start();
	}
}


void
Binary::signal_send_async()
{
	std::lock_guard<std::mutex> lk(bsmtx);
	for (auto it = servers_weak.begin(); it != servers_weak.end(); ) {
		auto server = it->lock();
		if (server) {
			server->signal_async.send();
			++it;
		} else {
			it = servers_weak.erase(it);
		}
	}
}


void
Binary::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint, std::promise<bool>* promise)
{
	tasks.enqueue([
		src_endpoint,
		dst_endpoint,
		promise
	] (const std::shared_ptr<BinaryServer>& server) mutable {
		server->trigger_replication(src_endpoint, dst_endpoint, promise);
	});

	signal_send_async();
}


#endif  /* XAPIAND_CLUSTERING */
