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
#include "length.h"

#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>


BaseUDP::BaseUDP(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& description_, uint16_t version_, const std::string& group_, int tries_)
	: Worker(manager_, loop_),
	  port(port_),
	  description(description_),
	  version(version_)
{
	bind(tries_, group_);

	L_OBJ(this, "CREATED BASE UDP!");
}


BaseUDP::~BaseUDP()
{
	destroy_impl();

	L_OBJ(this, "DELETED BASE UDP!");
}


void
BaseUDP::destroy_impl()
{
	L_OBJ(this, "DESTROYING BASE UDP!");

	if (sock == -1) {
		return;
	}

	::shutdown(sock, SHUT_RDWR);
	io::close(sock);
	sock = -1;

	L_OBJ(this, "DESTROYED BASE UDP!");
}


void
BaseUDP::shutdown_impl(bool asap, bool now)
{
	L_OBJ(this , "SHUTDOWN BASE UDP! (%d %d)", asap, now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now) {
		detach();
	}
}


void
BaseUDP::bind(int tries, const std::string& group)
{
	int optval = 1;
	unsigned char ttl = 3;
	struct ip_mreq mreq;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		L_CRIT(this, "ERROR: %s socket: [%d] %s", description.c_str(), errno, strerror(errno));
		exit(EX_CONFIG);
	}

	// use setsockopt() to allow multiple listeners connected to the same port
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		L_ERR(this, "ERROR: %s setsockopt SO_REUSEPORT (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		L_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_LOOP (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		L_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_TTL (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		L_CRIT(this, "ERROR: %s setsockopt IP_ADD_MEMBERSHIP (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
		io::close(sock);
		exit(EX_CONFIG);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to all addresses (differs from sender)

	for (int i = 0; i < tries; ++i, ++port) {
		addr.sin_port = htons(port);

		if (::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			if (!ignored_errorno(errno, true)) {
				if (i == tries - 1) break;
				L_DEBUG(nullptr, "ERROR: %s bind error (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
				continue;
			}
		}

		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
			L_ERR(nullptr, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}

		addr.sin_addr.s_addr = inet_addr(group.c_str());  // setup s_addr for sender (send to group)
		return;
	}

	L_CRIT(nullptr, "ERROR: %s bind error (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	io::close(sock);
	exit(EX_CONFIG);
}


void
BaseUDP::sending_message(const std::string& message)
{
	if (sock != -1) {
		L_UDP_WIRE(this, "(sock=%d) <<-- '%s'", sock, repr(message).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(sock, message.c_str(), message.size(), MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr));
#else
		ssize_t written = ::sendto(sock, message.c_str(), message.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
#endif

		if (written < 0) {
			if (sock != -1 && !ignored_errorno(errno, true)) {
				L_ERR(this, "ERROR: sendto error (sock=%d): %s", sock, strerror(errno));
				manager()->shutdown();
			}
		}
	}
}


void
BaseUDP::send_message(char type, const std::string& content)
{
	if (!content.empty()) {
		std::string message(1, type);
		message.append(std::string((const char *)&version, sizeof(uint16_t)));
		message.append(serialise_string(manager()->cluster_name));
		message.append(content);
		sending_message(message);
	}
}


char
BaseUDP::get_message(std::string& result, char max_type)
{
	char buf[1024];
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	ssize_t received = ::recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);
	if (received < 0) {
		if (!ignored_errorno(errno, true)) {
			L_ERR(this, "ERROR: read error (sock=%d): %s", sock, strerror(errno));
			throw MSG_NetworkError(strerror(errno));
		}
		L_CONN(this, "Received EOF (sock=%d)!", sock);
		throw MSG_DummyException("No message received!");
	} else if (received == 0) {
		// If no messages are available to be received and the peer has performed an orderly shutdown.
		L_CONN(this, "Received EOF (sock=%d)!", sock);
		throw MSG_DummyException("No message received!");
	} else if (received < 4) {
		throw MSG_NetworkError("Badly formed message: Incomplete!");
	}

	L_UDP_WIRE(this, "(sock=%d) -->> '%s'", sock, repr(buf, received).c_str());

	const char *p = buf;
	const char *p_end = p + received;

	char type = *p++;
	if (type >= max_type) {
		throw MSG_NetworkError("Invalid message type %u", unsigned(type));
	}

	uint16_t remote_protocol_version = *(uint16_t *)p;
	if ((remote_protocol_version & 0xff) > version) {
		throw MSG_NetworkError("Badly formed message: Protocol version mismatch!");
	}
	p += sizeof(uint16_t);

	std::string remote_cluster_name = unserialise_string(&p, p_end);
	if (remote_cluster_name.empty()) {
		throw MSG_NetworkError("Badly formed message: No cluster name!");
	}

	if (remote_cluster_name != manager()->cluster_name) {
		throw MSG_NetworkError("Badly formed message: No cluster name!");
	}

	result = std::string(p, p_end - p);
	return type;
}
