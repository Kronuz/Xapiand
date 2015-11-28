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

#include "udp_base.h"

#include "server.h"

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>


BaseUDP::BaseUDP(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string &description_, const std::string &group_, int tries_)
	: manager(manager_),
	  port(port_),
	  description(description_),
	  loop(loop_)
{
	LOG(this, "Num of share UDP: %d\n", manager.use_count());
	bind(tries_, group_);
	LOG(this, "Listening sock=%d\n", sock);
}


BaseUDP::~BaseUDP()
{
	close(sock);
	sock = -1;
}


void
BaseUDP::bind(int tries, const std::string &group)
{
	int optval = 1;
	unsigned char ttl = 3;
	struct ip_mreq mreq;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		LOG_ERR(this, "ERROR: %s socket: [%d] %s\n", description.c_str(), errno, strerror(errno));
		assert(false);
	}

	// use setsockopt() to allow multiple listeners connected to the same port
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_REUSEPORT (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_LOOP (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_TTL (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_ADD_MEMBERSHIP (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
		close(sock);
		assert(false);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to all addresses (differs from sender)

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			if (!ignored_errorno(errno, true)) {
				if (i == tries - 1) break;
				LOG_DEBUG(nullptr, "ERROR: %s bind error (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
				continue;
			}
		}

		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
			LOG_ERR(nullptr, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
		}

		addr.sin_addr.s_addr = inet_addr(group.c_str());  // setup s_addr for sender (send to group)
		return;
	}

	LOG_ERR(nullptr, "ERROR: %s bind error (sock=%d): [%d] %s\n", description.c_str(), sock, errno, strerror(errno));
	close(sock);
	assert(false);
}


void
BaseUDP::sending_message(const std::string &message)
{
	std::unique_lock<std::mutex> lk(manager->get_lock());
	if (sock != -1) {
		LOG_UDP_WIRE(this, "(sock=%d) <<-- '%s'\n", sock, repr(message).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(sock, message.c_str(), message.size(), MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr));
#else
		ssize_t written = ::sendto(sock, message.c_str(), message.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
#endif

		if (written < 0) {
			if (sock != -1 && !ignored_errorno(errno, true)) {
				LOG_ERR(this, "ERROR: sendto error (sock=%d): %s\n", sock, strerror(errno));
				manager->shutdown();
			}
		}
	}
}
