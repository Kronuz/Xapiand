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

#include "tcp_base.h"

#include <arpa/inet.h>              // for htonl, htons
#include <netdb.h>                  // for addrinfo, freeaddrinfo, getaddrinfo
#include <netinet/in.h>             // for sockaddr_in, INADDR_ANY, IPPROTO_TCP
#include <netinet/tcp.h>            // for TCP_NODELAY
#include <cstring>                  // for strerror, memset
#include <utility>
#include <errno.h>                  // for __error, errno
#include <fcntl.h>                  // for fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <sys/socket.h>             // for setsockopt, SOL_SOCKET, SO_NOSIGPIPE
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>             // for sysctl, CTL_KERN, KIPC_SOMAXCONN
#endif
#include <sysexits.h>               // for EX_CONFIG, EX_IOERR

#include "io_utils.h"               // for close
#include "log.h"                    // for L_ERR, L_OBJ, L_CRIT, L_DEBUG
#include "manager.h"                // for sig_exit
#include "utils.h"                  // for ignored_errorno


BaseTCP::BaseTCP(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, std::string  description_, int tries_, int flags_)
	: Worker(manager_, ev_loop_, ev_flags_),
	  port(port_),
	  flags(flags_),
	  description(std::move(description_))
{
	bind(tries_);

	L_OBJ("CREATED BASE TCP!");
}


BaseTCP::~BaseTCP()
{
	destroyer();

	io::close(sock);
	sock = -1;

	L_OBJ("DELETED BASE TCP!");
}


void
BaseTCP::destroy_impl()
{
	destroyer();
}


void
BaseTCP::destroyer()
{
	L_CALL("BaseTCP::destroyer()");

	if (sock == -1) {
		return;
	}

	::shutdown(sock, SHUT_RDWR);
}


void
BaseTCP::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("BaseTCP::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
	}
}


void
BaseTCP::bind(int tries)
{
	struct sockaddr_in addr;
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		L_CRIT("ERROR: %s socket: [%d] %s", description, errno, strerror(errno));
		sig_exit(-EX_IOERR);
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: %s setsockopt SO_REUSEADDR (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}
#endif

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}

	// struct linger ling = {0, 0};
	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	L_ERR("ERROR: %s setsockopt SO_LINGER (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	// }

	if ((flags & CONN_TCP_DEFER_ACCEPT) != 0) {
		// Activate TCP_DEFER_ACCEPT (dataready's SO_ACCEPTFILTER) for HTTP connections only.
		// We want the HTTP server to wakeup accepting connections that already have some data
		// to read; this is not the case for binary servers where the server is the one first
		// sending data.

#ifdef SO_ACCEPTFILTER
		struct accept_filter_arg af = {"dataready", ""};

		if (setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &af, sizeof(af)) < 0) {
			L_ERR("ERROR: Failed to enable the 'dataready' Accept Filter: setsockopt SO_ACCEPTFILTER (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif

#ifdef TCP_DEFER_ACCEPT
		if (setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval)) < 0) {
			L_ERR("ERROR: setsockopt TCP_DEFER_ACCEPT (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	for (int i = 0; i < tries; ++i, ++port) {
		addr.sin_port = htons(port);

		if (::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			if (!ignored_errorno(errno, true, true)) {
				if (i == tries - 1) { break; }
				L_DEBUG("ERROR: %s bind error (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
				continue;
			}
		}

		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
			L_ERR("ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}

		check_backlog(tcp_backlog);
		listen(sock, tcp_backlog);
		return;
	}

	L_CRIT("ERROR: %s bind error (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	io::close(sock);
	sig_exit(-EX_CONFIG);
}


int
BaseTCP::accept()
{
	int client_sock;

	int optval = 1;

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((client_sock = ::accept(sock, (struct sockaddr *)&addr, &addrlen)) < 0) {
		if (!ignored_errorno(errno, true, true)) {
			L_ERR("ERROR: accept error (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}
#endif

	// if (setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
	// 	L_ERR("ERROR: setsockopt SO_KEEPALIVE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	// struct linger ling = {0, 0};
	// if (setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	L_ERR("ERROR: setsockopt SO_LINGER (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	if ((flags & CONN_TCP_NODELAY) != 0) {
		if (setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
			L_ERR("ERROR: setsockopt TCP_NODELAY (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
		}
	}

	if (fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
		L_ERR("ERROR: fcntl O_NONBLOCK (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}

	return client_sock;
}


void
BaseTCP::check_backlog(int tcp_backlog)
{
#ifdef HAVE_SYS_SYSCTL_H
#if defined(NCORE_SOMAXCONN)
#define _SYSCTL_NAME "net.core.somaxconn"  // Linux?
	int mib[] = {CTL_NET, NET_CORE, NCORE_SOMAXCONN};
	size_t mib_len = sizeof(mib) / sizeof(int);
#elif defined(KIPC_SOMAXCONN)
#define _SYSCTL_NAME "kern.ipc.somaxconn"  // FreeBSD, Apple
	int mib[] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	size_t mib_len = sizeof(mib) / sizeof(int);
#endif
#endif
#ifdef _SYSCTL_NAME
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(mib, mib_len, &somaxconn, &somaxconn_len, nullptr, 0) < 0) {
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		L_WARNING("WARNING: The TCP backlog setting of %d cannot be enforced because "
				_SYSCTL_NAME
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#undef _SYSCTL_NAME
#else
	L_WARNING("WARNING: No way of getting TCP backlog setting of %d.", tcp_backlog);
#endif
}


int
BaseTCP::connect(int sock_, const std::string& hostname, const std::string& servname)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_protocol = 0;

	struct addrinfo *result;
	if (getaddrinfo(hostname.c_str(), servname.c_str(), &hints, &result) < 0) {
		L_ERR("Couldn't resolve host %s:%s", hostname, servname);
		io::close(sock_);
		return -1;
	}

	if (::connect(sock_, result->ai_addr, result->ai_addrlen) < 0) {
		if (!ignored_errorno(errno, true, true)) {
			L_ERR("ERROR: connect error to %s:%s (sock=%d): [%d] %s", hostname, servname, sock_, errno, strerror(errno));
			freeaddrinfo(result);
			io::close(sock_);
			return -1;
		}
	}

	freeaddrinfo(result);

	if (fcntl(sock_, F_SETFL, fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) < 0) {
		L_ERR("ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock_, errno, strerror(errno));
	}

	return sock_;
}
