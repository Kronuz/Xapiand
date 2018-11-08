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

#include "config.h"                 // for HAVE_SYS_SYSCTL_H, XAPIAND_TCP_BACKLOG

#include "base_tcp.h"

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

#include "io.hh"                    // for close, ignored_errno
#include "log.h"                    // for L_ERR, L_OBJ, L_CRIT, L_DEBUG
#include "manager.h"                // for sig_exit


TCP::TCP(int port_, std::string  description_, int tries_, int flags_)
	: port(port_),
	  sock(-1),
	  closed(false),
	  flags(flags_),
	  description(std::move(description_))
{
	bind(tries_);
}


TCP::~TCP()
{
	if (sock != -1) {
		io::close(sock);
	}
}


void
TCP::close() {
	if (!closed.exchange(true)) {
		io::shutdown(sock, SHUT_RDWR);
	}
}


void
TCP::bind(int tries)
{
	struct sockaddr_in addr;
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;

	if ((sock = io::socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		L_CRIT("ERROR: %s socket: [%d] %s", description, errno, strerror(errno));
		sig_exit(-EX_IOERR);
	}

	// use io::setsockopt() to allow multiple listeners connected to the same address
	if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_REUSEADDR (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}
#endif

	if (io::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	}

	// struct linger ling = {0, 0};
	// if (io::setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: %s setsockopt SO_LINGER (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	// }

	if ((flags & CONN_TCP_DEFER_ACCEPT) != 0) {
		// Activate TCP_DEFER_ACCEPT (dataready's SO_ACCEPTFILTER) for HTTP connections only.
		// We want the HTTP server to wakeup accepting connections that already have some data
		// to read; this is not the case for binary servers where the server is the one first
		// sending data.

#ifdef SO_ACCEPTFILTER
		struct accept_filter_arg af = {"dataready", ""};

		if (io::setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &af, sizeof(af)) == -1) {
			L_ERR("ERROR: Failed to enable the 'dataready' Accept Filter: setsockopt SO_ACCEPTFILTER (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif

#ifdef TCP_DEFER_ACCEPT
		if (io::setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_DEFER_ACCEPT (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
#endif
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	for (int i = 0; i < tries; ++i, ++port) {
		addr.sin_port = htons(port);

		if (io::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
			if (!io::ignored_errno(errno, true, true, true)) {
				if (i == tries - 1) { break; }
				L_DEBUG("ERROR: %s bind error (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
				continue;
			}
		}

		if (io::fcntl(sock, F_SETFL, io::fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
			L_CRIT("ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock, errno, strerror(errno));
			sig_exit(-EX_CONFIG);
		}

		check_backlog(tcp_backlog);
		io::listen(sock, tcp_backlog);
		return;
	}

	L_CRIT("ERROR: %s bind error (sock=%d): [%d] %s", description, sock, errno, strerror(errno));
	close();
	sig_exit(-EX_CONFIG);
}


int
TCP::accept()
{
	int client_sock;

	int optval = 1;

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((client_sock = io::accept(sock, (struct sockaddr *)&addr, &addrlen)) == -1) {
		if (!io::ignored_errno(errno, true, true, true)) {
			L_ERR("ERROR: accept error (sock=%d): [%d] %s", sock, errno, strerror(errno));
		}
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}
#endif

	// if (io::setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_KEEPALIVE (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	// struct linger ling = {0, 0};
	// if (io::setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_LINGER (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	// }

	if ((flags & CONN_TCP_NODELAY) != 0) {
		if (io::setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_NODELAY (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
		}
	}

	if (io::fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK (client_sock=%d): [%d] %s", client_sock, errno, strerror(errno));
	}

	return client_sock;
}


void
TCP::check_backlog(int tcp_backlog)
{
#ifdef HAVE_SYS_SYSCTL_H
#if defined(KIPC_SOMAXCONN)
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
		L_WARNING_ONCE("WARNING: The TCP backlog setting of %d cannot be enforced because "
				_SYSCTL_NAME
				" is set to the lower value of %d.", tcp_backlog, somaxconn);
	}
#undef _SYSCTL_NAME
#elif defined(__linux__)
	int fd = io::open("/proc/sys/net/core/somaxconn", O_RDONLY);
	if unlikely(fd == -1) {
		L_ERR("ERROR: Unable to open /proc/sys/net/core/somaxconn: [%d] %s", errno, std::strerror(errno));
		return;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/net/core/somaxconn: [%d] %s", errno, std::strerror(errno));
		return;
	}
	int somaxconn = atoi(line);
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		L_WARNING_ONCE("WARNING: The TCP backlog setting of %d cannot be enforced because "
				"/proc/sys/net/core/somaxconn"
				" is set to the lower value of %d.", tcp_backlog, somaxconn);
	}
#else
	L_WARNING_ONCE("WARNING: No way of getting TCP backlog setting of %d.", tcp_backlog);
#endif
}


int
TCP::connect(int sock_, const std::string& hostname, const std::string& servname)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_protocol = 0;

	struct addrinfo *result;
	if (getaddrinfo(hostname.c_str(), servname.c_str(), &hints, &result) != 0) {
		L_ERR("Couldn't resolve host %s:%s", hostname, servname);
		io::close(sock_);
		return -1;
	}

	if (io::connect(sock_, result->ai_addr, result->ai_addrlen) == -1) {
		if (!io::ignored_errno(errno, true, true, true)) {
			L_ERR("ERROR: connect error to %s:%s (sock=%d): [%d] %s", hostname, servname, sock_, errno, strerror(errno));
			freeaddrinfo(result);
			io::close(sock_);
			return -1;
		}
	}

	freeaddrinfo(result);

	if (io::fcntl(sock_, F_SETFL, io::fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s", sock_, errno, strerror(errno));
	}

	return sock_;
}


BaseTCP::BaseTCP(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, std::string  description_, int tries_, int flags_)
	: TCP(port_, description_, tries_, flags_),
	  Worker(parent_, ev_loop_, ev_flags_)
{
}


BaseTCP::~BaseTCP()
{
	TCP::close();

	Worker::deinit();
}


void
BaseTCP::shutdown_impl(long long asap, long long now)
{
	L_CALL("BaseTCP::shutdown_impl(%lld, %lld)", asap, now);

	Worker::shutdown_impl(asap, now);

	stop(false);
	destroy(false);

	if (now != 0) {
		detach();
	}
}


void
BaseTCP::destroy_impl()
{
	L_CALL("BaseTCP::destroy_impl()");

	Worker::destroy_impl();

	TCP::close();
}
