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

#include "tcp.h"

#include <arpa/inet.h>              // for htonl, htons
#include <cstring>                  // for memset
#include <errno.h>                  // for errno
#include <fcntl.h>                  // for fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <netdb.h>                  // for addrinfo, freeaddrinfo, getaddrinfo
#include <netinet/in.h>             // for sockaddr_in, INADDR_ANY, IPPROTO_TCP
#include <netinet/tcp.h>            // for TCP_NODELAY
#include <sys/socket.h>             // for setsockopt, SOL_SOCKET, SO_NOSIGPIPE
#include <utility>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>             // for sysctl, CTL_KERN, KIPC_SOMAXCONN
#endif
#include <sysexits.h>               // for EX_CONFIG, EX_IOERR

#include "error.hh"                 // for error:name, error::description
#include "io.hh"                    // for close, ignored_errno
#include "log.h"                    // for L_ERR, L_OBJ, L_CRIT, L_CONN
#include "manager.h"                // for sig_exit


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY


TCP::TCP(int port, const char* description, int flags)
	: port(port),
	  sock(-1),
	  closed(true),
	  flags(flags),
	  description(description)
{}


TCP::~TCP() noexcept
{
	try {
		if (sock != -1) {
			io::close(sock);
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


bool
TCP::close(bool close) {
	L_CALL("TCP::close(%s)", close ? "true" : "false");

	bool was_closed = closed.exchange(true);
	if (!was_closed && sock != -1) {
		if (close) {
			// Dangerously close socket!
			// (make sure no threads are using the file descriptor)
			io::close(sock);
			sock = -1;
		} else {
			io::shutdown(sock, SHUT_RDWR);
		}
	}
	return was_closed;
}


void
TCP::bind(int tries)
{
	L_CALL("TCP::bind(%d)", tries);

	if (!closed.exchange(false)) {
		return;
	}

	struct sockaddr_in addr;
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;

	if ((sock = io::socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		L_CRIT("ERROR: %s socket: %s (%d): %s", description, error::name(errno), errno, error::description(errno));
		sig_exit(-EX_IOERR);
	}

	if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_REUSEADDR {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	}

	if ((flags & TCP_SO_REUSEPORT) != 0) {
#ifdef SO_REUSEPORT_LB
		if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: %s setsockopt SO_REUSEPORT_LB {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
		}
#else
		if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: %s setsockopt SO_REUSEPORT {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
		}
#endif
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_NOSIGPIPE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	}
#endif

	if (io::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: %s setsockopt SO_KEEPALIVE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	}

	// struct linger ling = {0, 0};
	// if (io::setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: %s setsockopt SO_LINGER {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	// }

	if ((flags & TCP_TCP_DEFER_ACCEPT) != 0) {
		// Activate TCP_DEFER_ACCEPT (dataready's SO_ACCEPTFILTER) for HTTP connections only.
		// We want the HTTP server to wakeup accepting connections that already have some data
		// to read; this is not the case for binary servers where the server is the one first
		// sending data.

#ifdef SO_ACCEPTFILTER
		struct accept_filter_arg af = {"dataready", ""};

		if (io::setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &af, sizeof(af)) == -1) {
			L_ERR("ERROR: Failed to enable the 'dataready' Accept Filter: setsockopt SO_ACCEPTFILTER {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
		}
#endif

#ifdef TCP_DEFER_ACCEPT
		if (io::setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_DEFER_ACCEPT {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
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
				L_CONN("ERROR: %s bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				continue;
			}
		}

		if (io::fcntl(sock, F_SETFL, io::fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
			L_CRIT("ERROR: fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
			sig_exit(-EX_CONFIG);
		}

		_check_backlog(tcp_backlog);
		io::listen(sock, tcp_backlog);
		return;
	}

	L_CRIT("ERROR: %s bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	close();
	sig_exit(-EX_CONFIG);
}


int
TCP::accept()
{
	L_CALL("TCP::accept() {sock=%d}", sock);

	int client_sock;

	int optval = 1;

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((client_sock = io::accept(sock, (struct sockaddr *)&addr, &addrlen)) == -1) {
		if (!io::ignored_errno(errno, true, true, true)) {
			L_ERR("ERROR: accept error {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
		}
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
	}
#endif

	if (io::setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_KEEPALIVE {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
	}

	// struct linger ling = {0, 0};
	// if (io::setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_LINGER {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
	// }

	if ((flags & TCP_TCP_NODELAY) != 0) {
		if (io::setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_NODELAY {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
		}
	}

	if (io::fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
	}

	return client_sock;
}


void
TCP::_check_backlog(int tcp_backlog)
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
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): %s (%d): %s", error::name(errno), errno, error::description(errno));
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
		L_ERR("ERROR: Unable to open /proc/sys/net/core/somaxconn: %s (%d): %s", error::name(errno), errno, error::description(errno));
		return;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/net/core/somaxconn: %s (%d): %s", error::name(errno), errno, error::description(errno));
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
	L_CALL("TCP::connect(%d, %s, servname)", sock_, hostname, servname);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_protocol = 0;

	struct addrinfo *result;
	if (getaddrinfo(hostname.c_str(), servname.c_str(), &hints, &result) != 0) {
		L_ERR("Couldn't resolve host %s:%s", hostname, servname);
		return -1;
	}

	if (io::connect(sock_, result->ai_addr, result->ai_addrlen) == -1) {
		if (!io::ignored_errno(errno, true, true, true)) {
			L_ERR("ERROR: connect error to %s:%s {sock:%d}: %s (%d): %s", hostname, servname, sock_, error::name(errno), errno, error::description(errno));
			freeaddrinfo(result);
			return -1;
		}
	}

	freeaddrinfo(result);

	if (io::fcntl(sock_, F_SETFL, io::fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
	}

	return 0;
}


int
TCP::socket()
{
	L_CALL("TCP::socket()");

	int sock_;
	int optval = 1;

	if ((sock_ = io::socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		L_ERR("ERROR: cannot create binary connection: %s (%d): %s", error::name(errno), errno, error::description(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(sock_, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
	}
#endif

	if (io::setsockopt(sock_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_KEEPALIVE {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
	}

	// struct linger ling = {0, 0};
	// if (io::setsockopt(sock_, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1) {
	// 	L_ERR("ERROR: setsockopt SO_LINGER {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
	// }

	if (io::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NODELAY {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
	}

	return sock_;
}



BaseTCP::BaseTCP(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port, const char* description, int flags)
	: TCP(port, description, flags),
	  Worker(parent_, ev_loop_, ev_flags_)
{
}


BaseTCP::~BaseTCP() noexcept
{
	try {
		TCP::close();

		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
BaseTCP::shutdown_impl(long long asap, long long now)
{
	L_CALL("BaseTCP::shutdown_impl(%lld, %lld)", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		if (now != 0) {
			detach();
			if (is_runner()) {
				break_loop();
			}
		}
	}
}


void
BaseTCP::destroy_impl()
{
	L_CALL("BaseTCP::destroy_impl()");

	Worker::destroy_impl();

	TCP::close();
}
