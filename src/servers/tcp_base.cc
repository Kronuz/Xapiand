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

#include "tcp_base.h"

#include "../utils.h"
#include "../manager.h"

#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <netdb.h> /* for getaddrinfo */


BaseTCP::BaseTCP(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string &description_, int tries_, int flags_)
	: Worker(manager_, loop_),
	  port(port_),
	  flags(flags_),
	  description(description_)
{
	bind(tries_);
	L_DEBUG(this, "Listening sock=%d", sock);

	L_OBJ(this, "CREATED BASE TCP!");
}


BaseTCP::~BaseTCP()
{
	destroy();

	L_OBJ(this, "DELETED BASE TCP!");
}

void
BaseTCP::destroy()
{
	L_OBJ(this, "DESTROYING BASE TCP!");

	if (sock == -1) {
		return;
	}

	io::close(sock);
	sock = -1;

	L_OBJ(this, "DESTROYED BASE TCP!");
}

void
BaseTCP::shutdown(bool asap, bool now)
{
	L_OBJ(this , "SHUTDOWN BASE TCP! (%d %d)", asap, now);

	::shutdown(sock, SHUT_RDWR);
	destroy();

	Worker::shutdown(asap, now);
}

void
BaseTCP::bind(int tries)
{
	struct sockaddr_in addr;
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		L_CRIT(nullptr, "ERROR: %s socket: [%d] %s", description.c_str(), errno, strerror(errno));
		exit(EX_IOERR);
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: %s setsockopt SO_REUSEADDR (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}
#endif

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	}

	// struct linger ling = {0, 0};
	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	L_ERR(nullptr, "ERROR: %s setsockopt SO_LINGER (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	// }

	if (flags & CONN_TCP_DEFER_ACCEPT) {
		// Activate TCP_DEFER_ACCEPT (dataready's SO_ACCEPTFILTER) for HTTP connections only.
		// We want the HTTP server to wakeup accepting connections that already have some data
		// to read; this is not the case for binary servers where the server is the one first
		// sending data.

#ifdef SO_ACCEPTFILTER
		struct accept_filter_arg af = {"dataready", ""};

		if (setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &af, sizeof(af)) < 0) {
			L_ERR(nullptr, "ERROR: setsockopt SO_ACCEPTFILTER (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif

#ifdef TCP_DEFER_ACCEPT
		int optval = 1;

		if (setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval)) < 0) {
			L_ERR(nullptr, "ERROR: setsockopt TCP_DEFER_ACCEPT (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

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

		check_backlog(tcp_backlog);
		listen(sock, tcp_backlog);
		return;
	}

	L_CRIT(nullptr, "ERROR: %s bind error (sock=%d): [%d] %s", description.c_str(), sock, errno, strerror(errno));
	io::close(sock);
	exit(EX_CONFIG);
}


int
BaseTCP::accept()
{
	int client_sock;

	int optval = 1;

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((client_sock = ::accept(sock, (struct sockaddr *)&addr, &addrlen)) < 0) {
		if (!ignored_errorno(errno, true)) {
			L_ERR(nullptr, "ERROR: accept error (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
		}
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: setsockopt SO_NOSIGPIPE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}
#endif

	// if (setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
	// 	L_ERR(nullptr, "ERROR: setsockopt SO_KEEPALIVE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	// struct linger ling = {0, 0};
	// if (setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	L_ERR(nullptr, "ERROR: setsockopt SO_LINGER (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	if (flags & CONN_TCP_NODELAY) {
		if (setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
			L_ERR(nullptr, "ERROR: setsockopt TCP_NODELAY (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
		}
	}

	if (fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
		L_ERR(nullptr, "ERROR: fcntl O_NONBLOCK (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}

	return client_sock;
}


void
BaseTCP::check_backlog(int)
{
#if defined(NET_CORE_SOMAXCONN)
	int name[3] = {CTL_NET, NET_CORE, NET_CORE_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		L_ERR(nullptr, "ERROR: sysctl: [%d] %s", errno, strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		L_ERR(nullptr, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"net.core.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#elif defined(KIPC_SOMAXCONN)
	int name[3] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		L_ERR(nullptr, "ERROR: sysctl: [%d] %s", errno, strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		L_ERR(nullptr, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"kern.ipc.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#endif
}


int BaseTCP::connect(int sock_, const std::string &hostname, const std::string &servname)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_protocol = 0;

	struct addrinfo *result;
	if (getaddrinfo(hostname.c_str(), servname.c_str(), &hints, &result) < 0) {
		L_ERR(nullptr, "Couldn't resolve host %s:%s", hostname.c_str(), servname.c_str());
		io::close(sock_);
		return -1;
	}

	if (::connect(sock_, result->ai_addr, result->ai_addrlen) < 0) {
		if (!ignored_errorno(errno, true)) {
			L_ERR(nullptr, "ERROR: connect error to %s:%s (sock=%d): [%d] %s", hostname.c_str(), servname.c_str(), sock_, errno, strerror(errno));
			freeaddrinfo(result);
			io::close(sock_);
			return -1;
		}
	}

	freeaddrinfo(result);

	if (fcntl(sock_, F_SETFL, fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) < 0) {
		L_ERR(nullptr, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock_, errno, strerror(errno));
	}

	return sock_;
}
