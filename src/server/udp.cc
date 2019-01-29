/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "udp.h"

#include <arpa/inet.h>              // for inet_addr, htonl, htons
#include <cstring>                  // for memset
#include <errno.h>                  // for errno
#include <utility>
#include <fcntl.h>                  // for fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <sys/types.h>              // for SOL_SOCKET, SO_NOSIGPIPE
#include <sys/socket.h>             // for setsockopt, bind, recvfrom, sendto
#include <netdb.h>                  // for getaddrinfo, addrinfo
#include <sysexits.h>               // for EX_CONFIG

#include "error.hh"                 // for error:name, error::description
#include "exception.h"              // for MSG_NetworkError, NetworkError
#include "io.hh"                    // for close, ignored_errno
#include "length.h"                 // for serialise_string, unserialise_string
#include "log.h"                    // for L_ERR, L_OBJ, L_CRIT, L_CONN, L_UDP_WIRE
#include "manager.h"                // for XapiandManager, sig_exit, Xapiand...
#include "opts.h"                   // for opts


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_CONN
// #define L_CONN L_LIGHT_GREEN


UDP::UDP(const char* description, uint8_t major_version, uint8_t minor_version, int flags)
	: sock(-1),
	  closed(true),
	  flags(flags),
	  description(description),
	  major_version(major_version),
	  minor_version(minor_version),
	  addr{}
{}


UDP::~UDP() noexcept
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
UDP::close(bool close) {
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
UDP::bind(const char* hostname, unsigned int serv, int tries)
{
	if (!closed.exchange(false) || !tries) {
		return;
	}

	const int on = 1;
	const int off = 0;

	L_CONN("Binding UDP %s:%d", hostname ? hostname : "0.0.0.0", serv);

	for (; --tries >= 0; ++serv) {
		char servname[6];  // strlen("65535") + 1
		snprintf(servname, sizeof(servname), "%d", serv);

		struct addrinfo hints = {};
		hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;  // No effect if hostname != nullptr
		hints.ai_family = PF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		struct addrinfo *addrinfo;
		if (int err = getaddrinfo(hostname, servname, &hints, &addrinfo)) {
			L_CRIT("ERROR: getaddrinfo %s:%s {sock:%d}: %s", hostname ? hostname : "0.0.0.0", servname, sock, gai_strerror(err));
			sig_exit(-EX_CONFIG);
			return;
		}

		for (auto ai = addrinfo; ai != nullptr; ai = ai->ai_next) {
			if ((sock = io::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
				if (ai->ai_next == nullptr) {
					freeaddrinfo(addrinfo);
					L_CRIT("ERROR: %s socket: %s (%d): %s", description, error::name(errno), errno, error::description(errno));
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s socket: %s (%d): %s", description, error::name(errno), errno, error::description(errno));
				continue;
			}

			if (io::fcntl(sock, F_SETFL, io::fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
				freeaddrinfo(addrinfo);
				if (!tries) {
					L_CRIT("ERROR: %s fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				L_CONN("ERROR: %s fcntl O_NONBLOCK {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
				break;
			}

			if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
				freeaddrinfo(addrinfo);
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

			if ((flags & UDP_SO_REUSEPORT) != 0) {
		#ifdef SO_REUSEPORT_LB
				if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT_LB, &on, sizeof(int)) == -1) {
					freeaddrinfo(addrinfo);
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
				if (io::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
					freeaddrinfo(addrinfo);
					if (!tries) {
						L_CRIT("ERROR: %s setsockopt SO_REUSEPORT {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					break;
				}
		#endif
			}

			size_t sndbuf_size = 0;
			socklen_t sndbuf_size_len = sizeof(size_t);
			if (io::getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &sndbuf_size_len) == -1) {
				if (!tries) {
					L_CRIT("ERROR: %s getsockopt SO_SNDBUF {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				break;
			}
			for (size_t size = 4194304; size >= 262144 && size > sndbuf_size; size /= 2) {
				if (io::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size_t)) != -1) {
					if (size != 4194304) {
						L_WARNING("WARNING: %s SO_SNDBUF is set to %zu {sock:%d}", description, size, sock);
					}
					sndbuf_size = 0;
					break;
				}
			}
			if (sndbuf_size) {
				L_WARNING("WARNING: %s SO_SNDBUF is set to %zu {sock:%d}", description, sndbuf_size, sock);
			}

			size_t rcvbuf_size = 0;
			socklen_t rcvbuf_size_len = sizeof(size_t);
			if (io::getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, &rcvbuf_size_len) == -1) {
				if (!tries) {
					L_CRIT("ERROR: %s getsockopt SO_RCVBUF {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				break;
			}
			for (size_t size = 4194304; size >= 262144 && size > rcvbuf_size; size /= 2) {
				if (io::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size_t)) != -1) {
					if (size != 4194304) {
						L_WARNING("WARNING: %s SO_RCVBUF is set to %zu {sock:%d}", description, size, sock);
					}
					rcvbuf_size = 0;
					break;
				}
			}
			if (rcvbuf_size) {
				L_WARNING("WARNING: %s SO_RCVBUF is set to %zu {sock:%d}", description, rcvbuf_size, sock);
			}

			auto* onoff = ((flags & UDP_IP_MULTICAST_LOOP) != 0) ? &on : &off;
			if (io::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, onoff, sizeof(int)) == -1) {
				freeaddrinfo(addrinfo);
				if (!tries) {
					L_CRIT("ERROR: %s setsockopt IP_MULTICAST_LOOP {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
					close();
					sig_exit(-EX_CONFIG);
					return;
				}
				break;
			}

			if ((flags & UDP_IP_MULTICAST_TTL) != 0) {
				unsigned char ttl = 3;
				if (io::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) == -1) {
					freeaddrinfo(addrinfo);
					if (!tries) {
						L_CRIT("ERROR: %s setsockopt IP_MULTICAST_TTL {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					break;
				}
			}

			struct ip_mreq mreq = {};
			if ((flags & UDP_IP_ADD_MEMBERSHIP) != 0) {
				ASSERT(hostname);
				mreq.imr_multiaddr = reinterpret_cast<struct sockaddr_in*>(ai->ai_addr)->sin_addr;
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				if (io::setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
					freeaddrinfo(addrinfo);
					if (!tries) {
						L_CRIT("ERROR: %s setsockopt IP_ADD_MEMBERSHIP {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
						close();
						sig_exit(-EX_CONFIG);
						return;
					}
					break;
				}
			}

			addr = *reinterpret_cast<struct sockaddr_in*>(ai->ai_addr);

#ifdef __APPLE__
			// Binding to the multicast group address results in errno
			// EADDRNOTAVAIL "Can't assign requested address" during
			// sendto(2) under OS X, for some reason...
			// So we bind to INADDR_ANY instead:
			if ((flags & UDP_IP_ADD_MEMBERSHIP) != 0) {
				reinterpret_cast<struct sockaddr_in*>(ai->ai_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
			}
#endif

			if (io::bind(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
				freeaddrinfo(addrinfo);
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

			// L_RED("UDP addr -> %s:%d", inet_ntop(addr), ntohs(addr.sin_port));

			freeaddrinfo(addrinfo);
			return;
		}
	}

	L_CRIT("ERROR: %s unknown bind error {sock:%d}: %s (%d): %s", description, sock, error::name(errno), errno, error::description(errno));
	close();
	sig_exit(-EX_CONFIG);
}


ssize_t
UDP::send_message(const std::string& message)
{
	if (!closed) {
		L_UDP_WIRE("{sock:%d} <<-- %s", sock, repr(message));

#ifdef MSG_NOSIGNAL
		ssize_t written = io::sendto(sock, message.c_str(), message.size(), MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr));
#else
		ssize_t written = io::sendto(sock, message.c_str(), message.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
#endif

		if (written < 0) {
			if (!io::ignored_errno(errno, true, false, false)) {
				L_ERR_ONCE_PER_MINUTE("ERROR: sendto error {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
			}
		}
		return written;
	}
	return 0;
}


ssize_t
UDP::send_message(char type, const std::string& content)
{
	if (!content.empty()) {
		std::string message;
		message.push_back(major_version);
		message.push_back(minor_version);
		message.push_back(type);
		message.append(serialise_string(opts.cluster_name));
		message.append(content);
		return send_message(message);
	}
	return 0;
}


char
UDP::get_message(std::string& result, char max_type)
{
	char buf[1500];
	ssize_t received = io::recv(sock, buf, sizeof(buf), 0);
	if (received < 0) {
		if (!io::ignored_errno(errno, true, false, false)) {
			L_ERR("ERROR: read error {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
			THROW(NetworkError, error::description(errno));
		}
		// L_CONN("Received ERROR {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
		return '\xff';
	} else if (received == 0) {
		// If no messages are available to be received and the peer has performed an orderly shutdown.
		L_CONN("Received EOF {sock:%d}!", sock);
		return '\xff';
	} else if (received < 4) {
		L_CONN("Badly formed message: Incomplete!");
		return '\xff';
	}

	L_UDP_WIRE("{sock:%d} -->> %s", sock, repr(buf, received));

	const char *p = buf;
	const char *p_end = p + received;

	uint8_t received_major_version = *p++;
	uint8_t received_minor_version = *p++;
	if (received_major_version > major_version || (received_major_version == major_version && received_minor_version > minor_version)) {
		L_CONN("Badly formed message: Protocol version mismatch!");
		return '\xff';
	}

	char type = *p++;
	if (type >= max_type) {
		L_CONN("Badly formed message: Invalid message type %u", unsigned(type));
		return '\xff';
	}

	auto remote_cluster_name = unserialise_string(&p, p_end);
	if (remote_cluster_name.empty()) {
		L_CONN("Badly formed message: No cluster name!");
		return '\xff';
	}

	if (remote_cluster_name != opts.cluster_name) {
		return '\xff';
	}

	result = std::string(p, p_end - p);
	return type;
}
