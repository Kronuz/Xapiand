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

#include "discovery.h"

#include "length.h"
#include "manager.h"

#include <unistd.h>
#include <arpa/inet.h>

#include <string>


const uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;


Discovery::Discovery(XapiandManager *manager_, ev::loop_ref *loop_, const char *cluster_name_, const char *node_name_, const char *discovery_group_, int discovery_port_)
	: manager(manager_),
	  loop(loop_ ? loop_: &dynamic_loop),
	  discovery_io(*loop),
	  discovery_heartbeat(*loop),
	  break_loop(*loop),
	  cluster_name(cluster_name_),
	  node_name(node_name_),
	  discovery_port(discovery_port_)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

#ifdef HAVE_SRANDDEV
	sranddev();
#else
	srand(time(NULL));
#endif

	if (discovery_port == 0) {
		discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	}
	bind_udp("discovery", discovery_sock, discovery_port, discovery_addr, 1, discovery_group_ ? discovery_group_ : XAPIAND_DISCOVERY_GROUP);

	assert(discovery_sock != -1);

	discovery_io.set<Discovery, &Discovery::io_accept_discovery>(this);
	discovery_io.start(discovery_sock, ev::READ);

	discovery_heartbeat.set<Discovery, &Discovery::discovery_heartbeat_cb>(this);
	discovery_heartbeat.start(0, 1);

	LOG_OBJ(this, "CREATED DISCOVERY!\n");
}


Discovery::~Discovery()
{
	destroy();

	break_loop.stop();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	LOG_OBJ(this, "DELETED DISCOVERY!\n");
}


void
Discovery::run()
{
	LOG_OBJ(this, "Starting discovery loop...\n");
	loop->run(0);
	LOG_OBJ(this, "Discovery loop ended!\n");
}


void
Discovery::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (discovery_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	discovery(DISCOVERY_BYE, manager->this_node);

	if (discovery_sock != -1) {
		::close(discovery_sock);
		discovery_sock = -1;
	}

	discovery_io.stop();

	pthread_mutex_unlock(&qmtx);

	LOG_OBJ(this, "DESTROYED MANAGER!\n");
}


void
Discovery::shutdown()
{
	if (manager->shutdown_asap) {
		destroy();
	}
	if (manager->shutdown_now) {
		break_loop.send();
	}
}


void Discovery::discovery_heartbeat_cb(ev::timer &watcher, int revents)
{
	if (manager->state != STATE_READY) {
		if (manager->state == STATE_RESET) {
			if (!manager->this_node.name.empty()) {
				nodes.erase(stringtolower(manager->this_node.name));
			}
			if (node_name.empty()) {
				manager->this_node.name = name_generator();
			} else {
				manager->this_node.name = node_name;
			}
		}
		discovery(DISCOVERY_HELLO, manager->this_node);
	} else {
		discovery(DISCOVERY_PING, manager->this_node);
	}
	if (manager->state && manager->state-- == 1) {
		discovery_heartbeat.set(0, 10);
	}
}


void Discovery::io_accept_discovery(ev::io &watcher, int revents)
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
					if (remote_node.addr.sin_addr.s_addr == manager->this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == manager->this_node.http_port &&
						remote_node.binary_port == manager->this_node.binary_port) {
						discovery(DISCOVERY_WAVE, manager->this_node);
					} else {
						try {
							node = &nodes.at(stringtolower(remote_node.name));
							if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
								remote_node.http_port == node->http_port &&
								remote_node.binary_port == node->binary_port) {
								discovery(DISCOVERY_WAVE, manager->this_node);
							} else {
								discovery(DISCOVERY_SNEER, remote_node);
							}
						} catch (const std::out_of_range& err) {
							discovery(DISCOVERY_WAVE, manager->this_node);
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
					if (manager->state != STATE_READY &&
						remote_node.name == manager->this_node.name &&
						remote_node.addr.sin_addr.s_addr == manager->this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == manager->this_node.http_port &&
						remote_node.binary_port == manager->this_node.binary_port) {
						if (node_name.empty()) {
							LOG_DISCOVERY(this, "Node name %s already taken. Retrying other name...\n", manager->this_node.name.c_str());
							manager->state = STATE_RESET;
							discovery_heartbeat.set(0, 1);
						} else {
							LOG_ERR(this, "Cannot join the party. Node name %s already taken!\n", manager->this_node.name.c_str());
							manager->state = STATE_BAD;
							manager->this_node.name.clear();
							manager->shutdown_asap = time(NULL);
							manager->async_shutdown.send();
						}
					}
					break;

				case DISCOVERY_PING:
					try {
						node = &nodes.at(stringtolower(remote_node.name));
						node->touched = now;
						// Received a ping, return pong
						discovery(DISCOVERY_PONG, manager->this_node);
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


void Discovery::discovery(const char *buf, size_t buf_size)
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


void Discovery::discovery(discovery_type type, Node &node)
{
	if (node.name.empty()) {
		return;
	}
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
