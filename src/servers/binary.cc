/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "binary.h"

#ifdef XAPIAND_CLUSTERING

#include "../client_binary.h"

#include <assert.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */


Binary::Binary(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_)
	: BaseTCP(manager_, loop_, port_, "Binary", port_ == XAPIAND_BINARY_SERVERPORT ? 10 : 1, CONN_TCP_NODELAY)
{
	local_node.binary_port = port;

	L_OBJ(this, "CREATED CONFIGURATION FOR BINARY");
}


Binary::~Binary()
{
	L_OBJ(this, "DELETED CONFIGURATION FOR BINARY");
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

	if ((client_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		L_ERR(nullptr, "ERROR: cannot create binary connection: [%d] %s", errno, strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

	// if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
	// 	L_ERR(nullptr, "ERROR: setsockopt SO_KEEPALIVE (sock=%d): [%d] %s", sock, errno, strerror(errno));
	// }

	// struct linger ling = {0, 0};
	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	L_ERR(nullptr, "ERROR: setsockopt SO_LINGER (sock=%d): %s", sock, strerror(errno));
	// }

	if (flags & CONN_TCP_NODELAY) {
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
			L_ERR(nullptr, "ERROR: setsockopt TCP_NODELAY (sock=%d): %s", sock, strerror(errno));
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
Binary::async_signal_send()
{
	std::lock_guard<std::mutex> lk(bsmtx);
	for (auto it = servers_weak.begin(); it != servers_weak.end(); ) {
		auto server = (*it).lock();
		if (server) {
			server->async_signal.send();
			++it;
		} else {
			it = servers_weak.erase(it);
		}
	}
}


std::future<bool>
Binary::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint)
{
	auto future = tasks.enqueue([src_endpoint, dst_endpoint] (const std::shared_ptr<BinaryServer>& server) {
		return server->trigger_replication(src_endpoint, dst_endpoint);
	});

	async_signal_send();

	return future;
}


#endif  /* XAPIAND_CLUSTERING */
