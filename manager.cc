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


#define TIME_RE "((([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)?"

pcre *XapiandManager::compiled_time_re = NULL;


XapiandManager::XapiandManager(ev::loop_ref *loop_, const char *cluster_name_, const char *gossip_group_, int gossip_port_, int http_port_, int binary_port_)
	: loop(loop_ ? loop_: &dynamic_loop),
	  state(STATE_RESET),
	  cluster_name(cluster_name_),
	  gossip_io(*loop),
	  gossip_heartbeat(*loop),
	  break_loop(*loop),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  thread_pool("W%d", 10),
	  gossip_port(gossip_port_)
{
	sranddev();

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

	if (gossip_port == 0) {
		gossip_port = XAPIAND_GOSSIP_SERVERPORT;
	}
	bind_udp("gossip", gossip_sock, gossip_port, gossip_addr, 1, gossip_group_ ? gossip_group_ : XAPIAND_GOSSIP_GROUP);

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

	assert(gossip_sock != -1 && http_sock != -1 && binary_sock != -1);

	gossip_io.set<XapiandManager, &XapiandManager::gossip_io_cb>(this);
	gossip_io.start(gossip_sock, ev::READ);

	gossip_heartbeat.set<XapiandManager, &XapiandManager::gossip_heartbeat_cb>(this);
	gossip_heartbeat.start(0, 1);

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
				LOG_GOSSIP(this, "Using %s, IP address = %s\n", ifa->ifa_name, ip);
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
	struct ip_mreq mreq;
	struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(this, "ERROR: %s socket: %s\n", type, strerror(errno));
		return false;
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}
#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}
#endif
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
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
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		LOG_ERR(this, "ERROR: %s setsockopt (sock=%d): %s\n", type, sock, strerror(errno));
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
	if (gossip_sock == -1 && http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	gossip(GOSSIP_DEATH, this_node);

	if (gossip_sock != -1) {
		::close(gossip_sock);
		gossip_sock = -1;
	}

	if (http_sock != -1) {
		::close(http_sock);
		http_sock = -1;
	}

	if (binary_sock != -1) {
		::close(binary_sock);
		binary_sock = -1;
	}

	gossip_io.stop();

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


void XapiandManager::gossip_heartbeat_cb(ev::timer &watcher, int revents)
{
	if (state != STATE_READY) {
		if (state == STATE_RESET) {
			this_node.name = name_generator();
		}
		gossip(GOSSIP_HELLO, this_node);
	} else {
		gossip(GOSSIP_PING, this_node);
	}
	if (state && state-- == 1) {
		gossip_heartbeat.set(0, 10);
	}
}


void XapiandManager::gossip_io_cb(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid gossip event (sock=%d): %s\n", gossip_sock, strerror(errno));
		return;
	}

	if (gossip_sock == -1) {
		return;
	}

	assert(gossip_sock == watcher.fd);

	if (gossip_sock != -1 && revents & EV_READ) {
		char buf[1024];
		struct sockaddr_in addr;
		socklen_t addrlen;

		ssize_t received = ::recvfrom(gossip_sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);

		if (received < 0) {
			if (errno != EAGAIN && gossip_sock != -1) {
				LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", gossip_sock, strerror(errno));
				destroy();
			}
		} else if (received == 0) {
			// The peer has closed its half side of the connection.
			LOG_CONN(this, "Received EOF (sock=%d)!\n", gossip_sock);
			destroy();
		} else {
			LOG_GOSSIP(this, "(sock=%d) -->> '%s'\n", gossip_sock, repr(buf, received).c_str());

			if (received < 2) {
				LOG_GOSSIP(this, "Badly formed message: Incomplete!\n");
				return;
			}

			// LOG_GOSSIP(this, "%s says '%s'\n", inet_ntoa(addr.sin_addr), repr(buf, received).c_str());

			const char *ptr = buf + 1;
			Node remote_node;
			size_t decoded;

			if ((decoded = decode_length(&ptr, buf + received, true)) == -1) {
				LOG_GOSSIP(this, "Badly formed message: No cluster name!\n");
				return;
			}
			std::string remote_cluster_name(ptr, decoded);
			ptr += decoded;
			if (remote_cluster_name != cluster_name) {
				return;
			}

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_GOSSIP(this, "Badly formed message: No address!\n");
				return;
			}
			remote_node.addr.sin_addr.s_addr = decoded;

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_GOSSIP(this, "Badly formed message: No http port!\n");
				return;
			}
			remote_node.http_port = decoded;

			if ((decoded = decode_length(&ptr, buf + received, false)) == -1) {
				LOG_GOSSIP(this, "Badly formed message: No binary port!\n");
				return;
			}
			remote_node.binary_port = decoded;

			if ((decoded = decode_length(&ptr, buf + received, true)) <= 0) {
				LOG_GOSSIP(this, "Badly formed message: No name length!\n");
				return;
			}
			remote_node.name = std::string(ptr, decoded);
			ptr += decoded;

			int remote_pid = decode_length(&ptr, buf + received, false);

			LOG_GOSSIP(this, "%s on %s: http:%d, binary:%d at pid:%d\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port, remote_pid);

			Node *node;
			time_t now = time(NULL);

			switch (buf[0]) {
				case GOSSIP_HELLO:
					if (remote_node.addr.sin_addr.s_addr == this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == this_node.http_port &&
						remote_node.binary_port == this_node.binary_port) {
						gossip(GOSSIP_WAVE, this_node);
					} else {
						try {
							node = &nodes.at(stringtolower(remote_node.name));
							if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
								remote_node.http_port == node->http_port &&
								remote_node.binary_port == node->binary_port) {
								gossip(GOSSIP_WAVE, this_node);
							} else {
								gossip(GOSSIP_SNEER, *node);
							}
						} catch (const std::out_of_range& err) {
							gossip(GOSSIP_WAVE, this_node);
						}
					}
					break;

				case GOSSIP_WAVE:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
					} catch (const std::out_of_range& err) {
						node = &nodes[stringtolower(remote_node.name)];
						node->name = remote_node.name;
						node->addr.sin_addr.s_addr = remote_node.addr.sin_addr.s_addr;
						node->http_port = remote_node.http_port;
						node->binary_port = remote_node.binary_port;
						INFO(this, "Node %s joined the party!\n", remote_node.name.c_str());
					}
					node->touched = now;
					break;

				case GOSSIP_SNEER:
					if (state != STATE_READY &&
						remote_node.name == this_node.name &&
						remote_node.addr.sin_addr.s_addr == this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == this_node.http_port &&
						remote_node.binary_port == this_node.binary_port) {
						state = STATE_RESET;
						gossip_heartbeat.set(0, 1);
						LOG_GOSSIP(this, "Retrying other name");
					}
					break;

				case GOSSIP_PING:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
						node->touched = now;
						// Received a ping, return pong
						gossip(GOSSIP_PONG, this_node);
					} catch (const std::out_of_range& err) {
						LOG_GOSSIP(this, "Ignoring ping from unknown peer");
					}
					break;

				case GOSSIP_PONG:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
						node->touched = now;
					} catch (const std::out_of_range& err) {
						LOG_GOSSIP(this, "Ignoring pong from unknown peer");
					}
			}
		}
	}
}


void XapiandManager::gossip(const char *buf, size_t buf_size)
{
	if (gossip_sock != -1) {
		LOG_GOSSIP_WIRE(this, "(sock=%d) <<-- '%s'\n", gossip_sock, repr(buf, buf_size).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(gossip_sock, buf, buf_size, MSG_NOSIGNAL, (struct sockaddr *)&gossip_addr, sizeof(gossip_addr));
#else
		ssize_t written = ::sendto(gossip_sock, buf, buf_size, 0, (struct sockaddr *)&gossip_addr, sizeof(gossip_addr));
#endif

		if (written < 0) {
			if (errno != EAGAIN && gossip_sock != -1) {
				LOG_ERR(this, "ERROR: sendto error (sock=%d): %s\n", gossip_sock, strerror(errno));
				destroy();
			}
		} else if (written == 0) {
			// nothing written?
		} else {

		}
	}
}


void XapiandManager::gossip(gossip_type type, Node &node)
{
	std::string message((const char *)&type, 1);
	message.append(encode_length(cluster_name.size()));
	message.append(cluster_name);
	message.append(encode_length(node.addr.sin_addr.s_addr));
	message.append(encode_length(node.http_port));
	message.append(encode_length(node.binary_port));
	message.append(encode_length(node.name.size()));
	message.append(node.name);
	message.append(encode_length(getpid()));
	gossip(message.c_str(), message.size());
}


void XapiandManager::run(int num_servers)
{
#ifdef HAVE_REMOTE_PROTOCOL
	INFO(this, "Listening on tcp:%d (http), tcp:%d (xapian), udp:%d (gossip)...\n", this_node.http_port, this_node.binary_port, gossip_port);
#else
	INFO(this, "Listening on tcp:%d (http), udp:%d (gossip)...\n", this_node.http_port, this_node.gossip_port);
#endif  /* HAVE_REMOTE_PROTOCOL */

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