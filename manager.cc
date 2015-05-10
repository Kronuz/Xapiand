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

#include "manager.h"

#include "utils.h"
#include "server.h"
#include "length.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <net/if.h> /* for IFF_LOOPBACK */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>


const uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;


#define TIME_RE "((([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)?"

pcre *XapiandManager::compiled_time_re = NULL;


XapiandManager::XapiandManager(ev::loop_ref *loop_, const char *cluster_name_, const char *discovery_group_, int discovery_port_, int http_port_, int binary_port_)
	: loop(loop_ ? loop_: &dynamic_loop),
	  state(STATE_RESET),
	  cluster_name(cluster_name_),
	  discovery_io(*loop),
	  discovery_heartbeat(*loop),
	  break_loop(*loop),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  thread_pool("W%d", 10),
	  discovery_port(discovery_port_)
{
#ifdef HAVE_SRANDDEV
	sranddev();
#else
	srand(time(NULL));
#endif

	this_node.http_port = http_port_;
	this_node.binary_port = binary_port_;

	struct sockaddr_in addr;

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&servers_mutex_attr);
	pthread_mutexattr_settype(&servers_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&servers_mutex, &servers_mutex_attr);

	break_loop.set<XapiandManager, &XapiandManager::break_loop_cb>(this);
	break_loop.start();

	async_shutdown.set<XapiandManager, &XapiandManager::shutdown_cb>(this);
	async_shutdown.start();

	this_node.addr = host_address();

	if (discovery_port == 0) {
		discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	}
	bind_udp("discovery", discovery_sock, discovery_port, discovery_addr, 1, discovery_group_ ? discovery_group_ : XAPIAND_DISCOVERY_GROUP);

	int http_tries = 1;
	if (this_node.http_port == 0) {
		this_node.http_port = XAPIAND_HTTP_SERVERPORT;
		http_tries = 10;
	}
	bind_tcp("http", http_sock, this_node.http_port, addr, http_tries);

#ifdef HAVE_REMOTE_PROTOCOL
	int binary_tries = 1;
	if (this_node.binary_port == 0) {
		this_node.binary_port = XAPIAND_BINARY_SERVERPORT;
		binary_tries = 10;
	}
	bind_tcp("binary", binary_sock, this_node.binary_port, addr, binary_tries);
#endif  /* HAVE_REMOTE_PROTOCOL */

	assert(discovery_sock != -1 && http_sock != -1 && binary_sock != -1);

	discovery_io.set<XapiandManager, &XapiandManager::discovery_io_cb>(this);
	discovery_io.start(discovery_sock, ev::READ);

	discovery_heartbeat.set<XapiandManager, &XapiandManager::discovery_heartbeat_cb>(this);
	discovery_heartbeat.start(0, 1);

	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&servers_mutex);
	pthread_mutexattr_destroy(&servers_mutex_attr);

	LOG_OBJ(this, "DELETED MANAGER!\n");
}


void XapiandManager::check_tcp_backlog(int tcp_backlog)
{
#if defined(NET_CORE_SOMAXCONN)
	int name[3] = {CTL_NET, NET_CORE, NET_CORE_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"net.core.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#elif defined(KIPC_SOMAXCONN)
	int name[3] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"kern.ipc.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#endif
}

struct sockaddr_in XapiandManager::host_address()
{
	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		LOG_ERR(this, "ERROR: getifaddrs: %s\n", strerror(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) { // check it is IP4
				char ip[INET_ADDRSTRLEN];
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
				LOG_DISCOVERY(this, "Using %s, IP address = %s\n", ifa->ifa_name, ip);
				break;
			}
		}
		freeifaddrs(if_addr_struct);
	}
	return addr;
}

bool XapiandManager::bind_tcp(const char *type, int &sock, int &port, struct sockaddr_in &addr, int tries)
{
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;
	struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(this, "ERROR: %s socket: %s\n", type, strerror(errno));
		return false;
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_REUSEADDR (sock=%d): %s\n", type, sock, strerror(errno));
	}
#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): %s\n", type, sock, strerror(errno));
	}
#endif
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_LINGER (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt TCP_NODELAY (sock=%d): %s\n", type, sock, strerror(errno));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(this, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
		} else {
			fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
			check_tcp_backlog(tcp_backlog);
			listen(sock, tcp_backlog);
			return true;
		}
	}

	LOG_ERR(this, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
	::close(sock);
	sock = -1;
	return false;
}


bool XapiandManager::bind_udp(const char *type, int &sock, int &port, struct sockaddr_in &addr, int tries, const char *group)
{
	int optval = 1;
	unsigned char ttl = 3;
	struct ip_mreq mreq;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		LOG_ERR(this, "ERROR: %s socket: %s\n", type, strerror(errno));
		return false;
	}

	// use setsockopt() to allow multiple listeners connected to the same port
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt SO_REUSEPORT (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_LOOP (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_MULTICAST_TTL (sock=%d): %s\n", type, sock, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt IP_ADD_MEMBERSHIP (sock=%d): %s\n", type, sock, strerror(errno));
		::close(sock);
		sock = -1;
		return false;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to all addresses (differs from sender)

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(this, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
		} else {
			fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
			addr.sin_addr.s_addr = inet_addr(group);  // setup s_addr for sender (send to group)
			return true;
		}
	}

	LOG_ERR(this, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
	::close(sock);
	sock = -1;
	return false;
}


void XapiandManager::sig_shutdown_handler(int sig)
{
	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	time_t now = time(NULL);
	if (shutdown_now && sig != SIGTERM) {
		if (sig && shutdown_now + 1 < now) {
			INFO(this, "You insist... exiting now.\n");
			// remove pid file here, use: getpid();
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (shutdown_asap && sig != SIGTERM) {
		if (shutdown_asap + 1 < now) {
			shutdown_now = now;
			INFO(this, "Trying immediate shutdown.\n");
		}
	} else {
		shutdown_asap = now;
		switch (sig) {
			case SIGINT:
				INFO(this, "Received SIGINT scheduling shutdown...\n");
				break;
			case SIGTERM:
				INFO(this, "Received SIGTERM scheduling shutdown...\n");
				break;
			default:
				INFO(this, "Received shutdown signal, scheduling shutdown...\n");
		};
	}
	shutdown();
}


void XapiandManager::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (discovery_sock == -1 && http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	discovery(DISCOVERY_BYE, this_node);

	if (discovery_sock != -1) {
		::close(discovery_sock);
		discovery_sock = -1;
	}

	if (http_sock != -1) {
		::close(http_sock);
		http_sock = -1;
	}

	if (binary_sock != -1) {
		::close(binary_sock);
		binary_sock = -1;
	}

	discovery_io.stop();

	pthread_mutex_unlock(&qmtx);

	LOG_OBJ(this, "DESTROYED MANAGER!\n");
}


void XapiandManager::shutdown_cb(ev::async &watcher, int revents)
{
	sig_shutdown_handler(0);
}


std::list<XapiandServer *>::const_iterator XapiandManager::attach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator iterator = servers.insert(servers.end(), server);
	pthread_mutex_unlock(&servers_mutex);
	return iterator;
}


void XapiandManager::detach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	if (server->iterator != servers.end()) {
		servers.erase(server->iterator);
		server->iterator = servers.end();
		LOG_OBJ(this, "DETACHED SERVER!\n");
	}
	pthread_mutex_unlock(&servers_mutex);
}


void XapiandManager::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking manager loop!\n");
	loop->break_loop();
}


void XapiandManager::shutdown()
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator it(servers.begin());
	while (it != servers.end()) {
		XapiandServer *server = *(it++);
		server->shutdown();
	}
	pthread_mutex_unlock(&servers_mutex);

	if (shutdown_asap) {
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop.send();
	}
}


void XapiandManager::discovery_heartbeat_cb(ev::timer &watcher, int revents)
{
	if (state != STATE_READY) {
		if (state == STATE_RESET) {
			if (!this_node.name.empty()) {
				nodes.erase(stringtolower(this_node.name));
			}
			this_node.name = name_generator();
		}
		discovery(DISCOVERY_HELLO, this_node);
	} else {
		discovery(DISCOVERY_PING, this_node);
	}
	if (state && state-- == 1) {
		discovery_heartbeat.set(0, 10);
	}
}


void XapiandManager::discovery_io_cb(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid discovery event (sock=%d): %s\n", discovery_sock, strerror(errno));
		return;
	}

	if (discovery_sock == -1) {
		return;
	}

	assert(discovery_sock == watcher.fd);

	if (discovery_sock != -1 && revents & EV_READ) {
		char buf[1024];
		struct sockaddr_in addr;
		socklen_t addrlen;

		ssize_t received = ::recvfrom(discovery_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);

		if (received < 0) {
			if (errno != EAGAIN && discovery_sock != -1) {
				LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", discovery_sock, strerror(errno));
				destroy();
			}
		} else if (received == 0) {
			// The peer has closed its half side of the connection.
			LOG_CONN(this, "Received EOF (sock=%d)!\n", discovery_sock);
			destroy();
		} else {
			LOG_DISCOVERY_WIRE(this, "(sock=%d) -->> '%s'\n", discovery_sock, repr(buf, received).c_str());

			if (received < 4) {
				LOG_DISCOVERY(this, "Badly formed message: Incomplete!\n");
				return;
			}

			// LOG(this, "%s says '%s'\n", inet_ntoa(addr.sin_addr), repr(buf, received).c_str());
			uint16_t remote_protocol_version = *(uint16_t *)(buf + 1);
			if ((remote_protocol_version & 0xff) > XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) {
				LOG_DISCOVERY(this, "Badly formed message: Protocol version mismatch %x vs %x!\n", remote_protocol_version & 0xff, XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION);
				return;
			}

			const char *ptr = buf + 3;
			Node remote_node;
			size_t decoded;

			if ((decoded = decode_length(&ptr, buf + received, true)) == -1) {
				LOG_DISCOVERY(this, "Badly formed message: No cluster name!\n");
				return;
			}
			std::string remote_cluster_name(ptr, decoded);
			ptr += decoded;
			if (remote_cluster_name != cluster_name) {
				return;
			}

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_DISCOVERY(this, "Badly formed message: No address!\n");
				return;
			}
			remote_node.addr.sin_addr.s_addr = (int) decoded;

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_DISCOVERY(this, "Badly formed message: No http port!\n");
				return;
			}
			remote_node.http_port = (int) decoded;

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_DISCOVERY(this, "Badly formed message: No binary port!\n");
				return;
			}
			remote_node.binary_port = (int) decoded;

			if ((decoded = decode_length(&ptr, buf + received, true)) <= 0) {
				LOG_DISCOVERY(this, "Badly formed message: No name length!\n");
				return;
			}
			remote_node.name = std::string(ptr, decoded);
			ptr += decoded;

			int remote_pid = (int) decode_length(&ptr, buf + received, false);

			// LOG_DISCOVERY(this, "%s on ip:%s, tcp:%d (http), tcp:%d (xapian), at pid:%d\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port, remote_pid);

			Node *node;
			time_t now = time(NULL);

			switch (buf[0]) {
				case DISCOVERY_HELLO:
					if (remote_node.addr.sin_addr.s_addr == this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == this_node.http_port &&
						remote_node.binary_port == this_node.binary_port) {
						discovery(DISCOVERY_WAVE, this_node);
					} else {
						try {
							node = &nodes.at(stringtolower(remote_node.name));
							if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
								remote_node.http_port == node->http_port &&
								remote_node.binary_port == node->binary_port) {
								discovery(DISCOVERY_WAVE, this_node);
							} else {
								discovery(DISCOVERY_SNEER, remote_node);
							}
						} catch (const std::out_of_range& err) {
							discovery(DISCOVERY_WAVE, this_node);
						}
					}
					break;

				case DISCOVERY_WAVE:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
					} catch (const std::out_of_range& err) {
						node = &nodes[stringtolower(remote_node.name)];
						node->name = remote_node.name;
						node->addr.sin_addr.s_addr = remote_node.addr.sin_addr.s_addr;
						node->http_port = remote_node.http_port;
						node->binary_port = remote_node.binary_port;
						INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian), at pid:%d!\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port, remote_pid);
					}
					if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
						remote_node.http_port == node->http_port &&
						remote_node.binary_port == node->binary_port) {
						node->touched = now;
					}
					break;

				case DISCOVERY_SNEER:
					if (state != STATE_READY &&
						remote_node.name == this_node.name &&
						remote_node.addr.sin_addr.s_addr == this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == this_node.http_port &&
						remote_node.binary_port == this_node.binary_port) {
						state = STATE_RESET;
						discovery_heartbeat.set(0, 1);
						LOG_DISCOVERY(this, "Retrying other name");
					}
					break;

				case DISCOVERY_PING:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
						node->touched = now;
						// Received a ping, return pong
						discovery(DISCOVERY_PONG, this_node);
					} catch (const std::out_of_range& err) {
						LOG_DISCOVERY(this, "Ignoring ping from unknown peer");
					}
					break;

				case DISCOVERY_PONG:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
						node->touched = now;
					} catch (const std::out_of_range& err) {
						LOG_DISCOVERY(this, "Ignoring pong from unknown peer");
					}
					break;

				case DISCOVERY_BYE:
					nodes.erase(stringtolower(remote_node.name));
					INFO(this, "Node %s left the party!\n", remote_node.name.c_str());
					break;
			}
		}
	}
}


void XapiandManager::discovery(const char *buf, size_t buf_size)
{
	if (discovery_sock != -1) {
		LOG_DISCOVERY_WIRE(this, "(sock=%d) <<-- '%s'\n", discovery_sock, repr(buf, buf_size).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(discovery_sock, buf, buf_size, MSG_NOSIGNAL, (struct sockaddr *)&discovery_addr, sizeof(discovery_addr));
#else
		ssize_t written = ::sendto(discovery_sock, buf, buf_size, 0, (struct sockaddr *)&discovery_addr, sizeof(discovery_addr));
#endif

		if (written < 0) {
			if (errno != EAGAIN && discovery_sock != -1) {
				LOG_ERR(this, "ERROR: sendto error (sock=%d): %s\n", discovery_sock, strerror(errno));
				destroy();
			}
		} else if (written == 0) {
			// nothing written?
		} else {

		}
	}
}


void XapiandManager::discovery(discovery_type type, Node &node)
{
	std::string message((const char *)&type, 1);
	message.append(std::string((const char *)&XAPIAND_DISCOVERY_PROTOCOL_VERSION, sizeof(uint16_t)));
	message.append(encode_length(cluster_name.size()));
	message.append(cluster_name);
	message.append(encode_length(node.addr.sin_addr.s_addr));
	message.append(encode_length(node.http_port));
	message.append(encode_length(node.binary_port));
	message.append(encode_length(node.name.size()));
	message.append(node.name);
	message.append(encode_length(getpid()));
	discovery(message.c_str(), message.size());
}


void XapiandManager::run(int num_servers)
{
	std::string msg("Listening on ");
	if (this_node.http_port != -1) {
		msg += "tcp:" + std::to_string(this_node.http_port) + " (http), ";
	}
	if (this_node.binary_port != -1) {
		msg += "tcp:" + std::to_string(this_node.binary_port) + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + "), ";
	}
	if (discovery_port != -1) {
		msg += "udp:" + std::to_string(discovery_port) + " (discovery v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + "), ";
	}
	msg += "at pid:" + std::to_string(getpid()) + "...\n";

	INFO(this, msg.c_str());

	ThreadPool server_pool("S%d", num_servers);
	for (int i = 0; i < num_servers; i++) {
		XapiandServer *server = new XapiandServer(this, NULL, http_sock, binary_sock, &database_pool, &thread_pool);
		server_pool.addTask(server);
	}

	LOG_OBJ(this, "Starting manager loop...\n");
	loop->run();
	LOG_OBJ(this, "Manager loop ended!\n");

	LOG_OBJ(this, "Waiting for threads...\n");

	server_pool.finish();
	server_pool.join();

	LOG_OBJ(this, "Server ended!\n");
}


cJSON* XapiandManager::server_status()
{
	cJSON *root_status = cJSON_CreateObject();
	std::string contet_ser;
	pthread_mutex_lock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status, "Connections", XapiandServer::total_clients);
	cJSON_AddNumberToObject(root_status, "Http connections", XapiandServer::http_clients);
	cJSON_AddNumberToObject(root_status, "Xapian remote connections", XapiandServer::binary_clients);
	pthread_mutex_unlock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status, "Size thread pool", thread_pool.length());
	return root_status;
}


cJSON* XapiandManager::get_stats_time(const std::string &time_req)
{
	cJSON *root_stats = cJSON_CreateObject();
	pos_time_t first_time, second_time;
	int len = (int) time_req.size();
	group_t *g = NULL;
	int ret = pcre_search(time_req.c_str(), len, 0, 0, TIME_RE, &compiled_time_re, &g);
	if (ret == 0 && (g[0].end - g[0].start) == len) {
		if ((g[1].end - g[1].start) > 0) {
			first_time.minute = 60 * (((g[3].end - g[3].start) > 0) ? strtoint(std::string(time_req.c_str() + g[3].start, g[3].end - g[3].start)) : 0);
			first_time.minute += ((g[4].end - g[4].start) > 0) ? strtoint(std::string(time_req.c_str() + g[4].start, g[4].end - g[4].start -1)) : 0;
			first_time.second = ((g[5].end - g[5].start) > 0) ? strtoint(std::string(time_req.c_str() + g[5].start, g[5].end - g[5].start -1)) : 0;
			if ((g[6].end - g[6].start) > 0) {
				second_time.minute = 60 * (((g[8].end - g[8].start) > 0) ? strtoint(std::string(time_req.c_str() + g[8].start, g[8].end - g[8].start)) : 0);
				second_time.minute += ((g[9].end - g[9].start) > 0) ? strtoint(std::string(time_req.c_str() + g[9].start, g[9].end - g[9].start -1)) : 0;
				second_time.second = ((g[10].end - g[10].start) > 0) ? strtoint(std::string(time_req.c_str() + g[10].start, g[10].end - g[10].start -1)) : 0;
			} else {
				second_time.minute = 0;
				second_time.second = 0;
			}
			if (g) {
				free(g);
				g = NULL;
			}
			return get_stats_json(first_time, second_time);
		} else {
			cJSON_AddStringToObject(root_stats, "Error in time argument input", "Incorrect input.");
			return root_stats;
		}
	}

	cJSON_AddStringToObject(root_stats, "Error in time argument input", "Incorrect input.");
	return root_stats;
}


cJSON* XapiandManager::get_stats_json(pos_time_t first_time, pos_time_t second_time)
{
	cJSON *root_stats = cJSON_CreateObject();
	cJSON *time_period = cJSON_CreateObject();

	pthread_mutex_lock(&XapiandServer::static_mutex);
	update_pos_time();
	time_t now_time = init_time;
	pos_time_t b_time_cpy = b_time;
	times_row_t stats_cnt_cpy = stats_cnt;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	int aux_second_sec;
	int aux_first_sec;
	int aux_second_min;
	int aux_first_min;
	bool seconds = (first_time.minute == 0) ? true : false;

	if (second_time.minute == 0 && second_time.second == 0) {
		aux_second_sec =  first_time.second;
		aux_first_sec = 0;
		aux_second_min =  first_time.minute;
		aux_first_min = 0;
		second_time.minute = b_time_cpy.minute - first_time.minute;
		second_time.second = b_time_cpy.second - first_time.second;
		first_time.minute = b_time_cpy.minute;
		first_time.second = b_time_cpy.second;
	} else {
		aux_second_sec =  second_time.second;
		aux_first_sec = first_time.second;
		aux_second_min =  second_time.minute;
		aux_first_min = first_time.minute;
		first_time.minute = b_time_cpy.minute - first_time.minute;
		first_time.second = b_time_cpy.second - first_time.second;
		second_time.minute = b_time_cpy.minute - second_time.minute;
		second_time.second = b_time_cpy.second - second_time.second;
	}

	if ((aux_first_min * SLOT_TIME_SECOND + aux_first_sec) > (aux_second_min * SLOT_TIME_SECOND + aux_second_sec)) {
		cJSON_AddStringToObject(root_stats, "Error in time argument input", "First argument must be less or equal than the second.");
	} else {
		int cnt[3] = {0, 0, 0};
		double tm_cnt[3] = {0.0, 0.0, 0.0};
		struct tm *timeinfo = localtime(&now_time);
		cJSON_AddStringToObject(time_period, "System time", asctime(timeinfo));
		if (seconds) {
			for (int i = second_time.second; i <= first_time.second; i++) {
				int j = (i < 0) ? SLOT_TIME_SECOND + i : i;
				cnt[0] += stats_cnt_cpy.index.sec[j];
				cnt[1] += stats_cnt_cpy.search.sec[j];
				cnt[2] += stats_cnt_cpy.del.sec[j];
				tm_cnt[0] += stats_cnt_cpy.index.tm_sec[j];
				tm_cnt[1] += stats_cnt_cpy.search.tm_sec[j];
				tm_cnt[2] += stats_cnt_cpy.del.tm_sec[j];
			}
			time_t p_time = now_time - aux_second_sec;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period start", asctime(timeinfo));
			p_time = now_time - aux_first_sec;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period end", asctime(timeinfo));
		} else {
			for (int i = second_time.minute; i <= first_time.minute; i++) {
				int j = (i < 0) ? SLOT_TIME_MINUTE + i : i;
				cnt[0] += stats_cnt_cpy.index.cnt[j];
				cnt[1] += stats_cnt_cpy.search.cnt[j];
				cnt[2] += stats_cnt_cpy.del.cnt[j];
				tm_cnt[0] += stats_cnt_cpy.index.tm_cnt[j];
				tm_cnt[1] += stats_cnt_cpy.search.tm_cnt[j];
				tm_cnt[2] += stats_cnt_cpy.del.tm_cnt[j];
			}
			time_t p_time = now_time - aux_second_min * SLOT_TIME_SECOND;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period start", asctime(timeinfo));
			p_time = now_time - aux_first_min * SLOT_TIME_SECOND;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period end", asctime(timeinfo));
		}
		cJSON_AddItemToObject(root_stats, "Time", time_period);
		cJSON_AddNumberToObject(root_stats, "Docs index", cnt[0]);
		cJSON_AddNumberToObject(root_stats, "Number searches", cnt[1]);
		cJSON_AddNumberToObject(root_stats, "Docs deleted", cnt[2]);
		cJSON_AddNumberToObject(root_stats, "Average time indexing", tm_cnt[0] / ((cnt[0] == 0) ? 1 : cnt[0]));
		cJSON_AddNumberToObject(root_stats, "Average search time", tm_cnt[1] / ((cnt[1] == 0) ? 1 : cnt[1]));
		cJSON_AddNumberToObject(root_stats, "Average deletion time", tm_cnt[2] / ((cnt[2] == 0) ? 1 : cnt[2]));
	}

	return root_stats;
}