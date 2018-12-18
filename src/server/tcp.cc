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
#include <sys/types.h>              // for SOL_SOCKET, SO_NOSIGPIPE
#include <sys/socket.h>             // for setsockopt, bind, connect
#include <netdb.h>                  // for getaddrinfo, addrinfo
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
// #undef L_CONN
// #define L_CONN L_LIGHT_GREEN


TCP::TCP(const char* description, int flags)
	: sock(-1),
	  closed(true),
	  flags(flags),
	  description(description),
	  addr{}
{}


TCP::~TCP() noexcept
{
	try {
		if (sock != -1) {
			if (io::close(sock) == -1) {
				L_WARNING("WARNING: close {sock:%d} - %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
			}
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
			if (io::close(sock) == -1) {
				L_WARNING("WARNING: close {sock:%d} - %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
			}
			sock = -1;
		} else {
			io::shutdown(sock, SHUT_RDWR);
		}
	}
	return was_closed;
}


void
TCP::bind(const char* hostname, unsigned int serv, int tries)
{
	L_CALL("TCP::bind(%d)", tries);

	if (!closed.exchange(false) || !tries) {
		return;
	}

	int optval = 1;

	L_CONN("Binding TCP %s:%d", hostname ? hostname : "0.0.0.0", serv);

	for (; --tries >= 0; ++serv) {
		char servname[6];  // strlen("65535") + 1
		snprintf(servname, sizeof(servname), "%d", serv);

		struct addrinfo hints = {};
		hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;  // No effect if hostname != nullptr
		hints.ai_family = PF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		struct addrinfo *servinfo;
		if (int err = getaddrinfo(hostname, servname, &hints, &servinfo)) {
			L_CRIT("ERROR: getaddrinfo %s:%s {sock:%d}: %s", hostname ? hostname : "0.0.0.0", servname, sock, gai_strerror(err));
			sig_exit(-EX_CONFIG);
			return;
		}

		for (auto p = servinfo; p != nullptr; p = p->ai_next) {
			if ((sock = io::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				if (p->ai_next == nullptr) {
					freeaddrinfo(servinfo);
					L_CRIT("ERROR: %s socket: %s (%d): %s", description, error::name(errno), errno, error::description(errno));
					sig_exit(-EX_IOERR);
					return;
				}
				L_CONN("ERROR: %s socket: %s (%d): %s", description, error::name(errno), errno, error::description(errno));
				continue;
			}

			if (io::fcntl(sock, F_SETFL, io::fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				break;
			}

			if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s setsockopt SO_REUSEADDR {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s setsockopt SO_REUSEADDR {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}

			if ((flags & TCP_SO_REUSEPORT) != 0) {
#ifdef SO_REUSEPORT_LB
				if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &optval, sizeof(optval)) == -1) {
					freeaddrinfo(servinfo);
					if (!tries) {
						L_CRIT("ERROR: %s setsockopt SO_REUSEPORT_LB {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					L_CONN("ERROR: %s setsockopt SO_REUSEPORT_LB {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					break;
				}
#else
				if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
					freeaddrinfo(servinfo);
					if (!tries) {
						L_CRIT("ERROR: %s setsockopt SO_REUSEPORT {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					L_CONN("ERROR: %s setsockopt SO_REUSEPORT {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					break;
				}
#endif
			}

#ifdef SO_NOSIGPIPE
			if (io::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s setsockopt SO_NOSIGPIPE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s setsockopt SO_NOSIGPIPE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}
#endif

			if (io::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s setsockopt SO_KEEPALIVE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s setsockopt SO_KEEPALIVE {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}

			struct linger linger;
			linger.l_onoff = 1;
			linger.l_linger = 0;
			if (io::setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s setsockopt SO_LINGER {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s setsockopt SO_LINGER {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}

			if ((flags & TCP_TCP_DEFER_ACCEPT) != 0) {
				// Activate TCP_DEFER_ACCEPT (dataready's SO_ACCEPTFILTER) for HTTP connections only.
				// We want the HTTP server to wakeup accepting connections that already have some data
				// to read; this is not the case for binary servers where the server is the one first
				// sending data.

#ifdef SO_ACCEPTFILTER
				struct accept_filter_arg af = {"dataready", ""};

				if (io::setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &af, sizeof(af)) == -1) {
					freeaddrinfo(servinfo);
					if (!tries) {
						L_CRIT("ERROR: Failed to enable the 'dataready' Accept Filter: setsockopt SO_ACCEPTFILTER {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					L_CONN("ERROR: Failed to enable the 'dataready' Accept Filter: setsockopt SO_ACCEPTFILTER {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
					close();
					break;
				}
#endif

#ifdef TCP_DEFER_ACCEPT
				if (io::setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval)) == -1) {
					freeaddrinfo(servinfo);
					if (!tries) {
						L_CRIT("ERROR: setsockopt TCP_DEFER_ACCEPT {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					L_CONN("ERROR: setsockopt TCP_DEFER_ACCEPT {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
					close();
					break;
				}
#endif
			}

			addr = *reinterpret_cast<struct sockaddr_in*>(p->ai_addr);

			if (io::bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}

			if (io::listen(sock, checked_tcp_backlog(XAPIAND_TCP_BACKLOG)) == -1) {
				freeaddrinfo(servinfo);
				if (!tries) {
					L_CRIT("ERROR: %s listen error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s listen error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				close();
				break;
			}

			// L_RED("TCP addr -> %s:%d", fast_inet_ntop4(addr.sin_addr), ntohs(addr.sin_port));

			freeaddrinfo(servinfo);
			return;
		}
	}

	L_CRIT("ERROR: %s unknown bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	close();
	sig_exit(-EX_CONFIG);
}


int
TCP::accept()
{
	L_CALL("TCP::accept() {sock=%d}", sock);

	int client_sock;

	int optval = 1;

	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);

	if ((client_sock = io::accept(sock, (struct sockaddr *)&client_addr, &addrlen)) == -1) {
		if (!io::ignored_errno(errno, true, true, true)) {
			L_ERR("ERROR: accept error {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
		}
		return -1;
	}

	if (io::fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
		io::close(client_sock);
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
		io::close(client_sock);
		return -1;
	}
#endif

	if (io::setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_KEEPALIVE {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
		io::close(client_sock);
		return -1;
	}

	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	if (io::setsockopt(client_sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
		L_ERR("ERROR: setsockopt SO_LINGER {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
		io::close(client_sock);
		return -1;
	}

	if ((flags & TCP_TCP_NODELAY) != 0) {
		if (io::setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
			L_ERR("ERROR: setsockopt TCP_NODELAY {client_sock:%d}: %s (%d): %s", client_sock, error::name(errno), errno, error::description(errno));
			io::close(client_sock);
			return -1;
		}
	}

	return client_sock;
}


int
TCP::checked_tcp_backlog(int tcp_backlog)
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
		return tcp_backlog;
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
		return tcp_backlog;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/net/core/somaxconn: %s (%d): %s", error::name(errno), errno, error::description(errno));
		return tcp_backlog;
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
	return tcp_backlog;
}


int
TCP::connect(int sock_, const char* hostname, const char* servname)
{
	L_CALL("TCP::connect(%d, %s, servname)", sock_, hostname, servname);

	struct addrinfo hints = {};
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *servinfo;
	if (int err = getaddrinfo(hostname, servname, &hints, &servinfo)) {
		L_ERR("Couldn't resolve host %s:%s: %s", hostname, servname, gai_strerror(err));
		return -1;
	}

	for (auto p = servinfo; p != nullptr; p = p->ai_next) {
		if (io::connect(sock_, p->ai_addr, p->ai_addrlen) != -1) {
			freeaddrinfo(servinfo);
			return 0;
		}
		if (errno == EINPROGRESS || errno == EALREADY) {
			freeaddrinfo(servinfo);
			return 0;
		}
	}

	L_ERR("ERROR: connect error to %s:%s {sock:%d}: %s (%d): %s", hostname, servname, sock_, error::name(errno), errno, error::description(errno));
	freeaddrinfo(servinfo);
	return -1;

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

	if (io::fcntl(sock_, F_SETFL, io::fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) == -1) {
		L_ERR("ERROR: fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
		io::close(sock_);
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (io::setsockopt(sock_, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_NOSIGPIPE {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
		io::close(sock_);
		return -1;
	}
#endif

	if (io::setsockopt(sock_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt SO_KEEPALIVE {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
		io::close(sock_);
		return -1;
	}

	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	if (io::setsockopt(sock_, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
		L_ERR("ERROR: setsockopt SO_LINGER {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
		io::close(sock_);
		return -1;
	}

	if (io::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NODELAY {sock:%d}: %s (%d): %s", sock_, error::name(errno), errno, error::description(errno));
		io::close(sock_);
		return -1;
	}

	return sock_;
}



BaseTCP::BaseTCP(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* description, int flags)
	: TCP(description, flags),
	  Worker(parent_, ev_loop_, ev_flags_)
{
}


BaseTCP::~BaseTCP() noexcept
{
	try {
		Worker::deinit();

		TCP::close();
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
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
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
