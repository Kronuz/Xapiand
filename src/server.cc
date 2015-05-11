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

#include "server.h"

#include "utils.h"
#include "length.h"

#include "client_http.h"
#include "client_binary.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#include <xapian.h>


const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


//
// Xapian Server
//

pthread_mutex_t XapiandServer::static_mutex = PTHREAD_MUTEX_INITIALIZER;
int XapiandServer::total_clients = 0;
int XapiandServer::http_clients = 0;
int XapiandServer::binary_clients = 0;


XapiandServer::XapiandServer(XapiandManager *manager_, ev::loop_ref *loop_, int discovery_sock_, int http_sock_, int binary_sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_)
	: manager(manager_),
	  iterator(manager->attach_server(this)),
	  loop(loop_ ? loop_: &dynamic_loop),
	  http_io(*loop),
	  binary_io(*loop),
	  break_loop(*loop),
	  discovery_sock(discovery_sock_),
	  http_sock(http_sock_),
	  binary_sock(binary_sock_),
	  database_pool(database_pool_),
	  thread_pool(thread_pool_)
{
#ifdef HAVE_SRANDDEV
	sranddev();
#else
	srand(time(NULL));
#endif

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&clients_mutex_attr);
	pthread_mutexattr_settype(&clients_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&clients_mutex, &clients_mutex_attr);

	break_loop.set<XapiandServer, &XapiandServer::break_loop_cb>(this);
	break_loop.start();

	discovery_io.set<XapiandServer, &XapiandServer::io_accept_discovery>(this);
	discovery_io.start(discovery_sock, ev::READ);

	http_io.set<XapiandServer, &XapiandServer::io_accept_http>(this);
	http_io.start(http_sock, ev::READ);

#ifdef HAVE_REMOTE_PROTOCOL
	binary_io.set<XapiandServer, &XapiandServer::io_accept_binary>(this);
	binary_io.start(binary_sock, ev::READ);
#endif  /* HAVE_REMOTE_PROTOCOL */

	LOG_OBJ(this, "CREATED SERVER!\n");
}


XapiandServer::~XapiandServer()
{
	destroy();

	break_loop.stop();

	manager->detach_server(this);

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&clients_mutex);
	pthread_mutexattr_destroy(&clients_mutex_attr);

	LOG_OBJ(this, "DELETED SERVER!\n");
}


void XapiandServer::run()
{
	LOG_OBJ(this, "Starting server loop...\n");
	loop->run(0);
	LOG_OBJ(this, "Server loop ended!\n");
}


void XapiandServer::io_accept_discovery(ev::io &watcher, int revents)
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
			const char *end = buf + received;
			size_t decoded;

			std::string remote_cluster_name;
			if (unserialise_string(remote_cluster_name, &ptr, end) == -1 || remote_cluster_name.empty()) {
				LOG_DISCOVERY(this, "Badly formed message: No cluster name!\n");
				return;
			}
			if (remote_cluster_name != manager->cluster_name) {
				return;
			}

			// LOG_DISCOVERY(this, "%s on ip:%s, tcp:%d (http), tcp:%d (xapian), at pid:%d\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);

			Node *node;
			Node remote_node;
			Database *database = NULL;
			std::string index_path;
			std::string node_name;
			size_t mastery_level;
			Endpoints endpoints;

			time_t now = time(NULL);

			switch (buf[0]) {
				case DISCOVERY_HELLO:
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}
					if (remote_node.addr.sin_addr.s_addr == manager->this_node.addr.sin_addr.s_addr &&
						remote_node.http_port == manager->this_node.http_port &&
						remote_node.binary_port == manager->this_node.binary_port) {
						// It's me! ...wave hello!
						manager->discovery(DISCOVERY_WAVE, manager->this_node.serialise());
					} else {
						try {
							node_name = stringtolower(remote_node.name);
							if (node_name == stringtolower(manager->this_node.name)) {
								node = &manager->this_node;
							} else {
								node = &manager->nodes.at(node_name);
							}
							if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
								remote_node.http_port == node->http_port &&
								remote_node.binary_port == node->binary_port) {
								manager->discovery(DISCOVERY_WAVE, manager->this_node.serialise());
							} else {
								manager->discovery(DISCOVERY_SNEER, remote_node.serialise());
							}
						} catch (const std::out_of_range& err) {
							manager->discovery(DISCOVERY_WAVE, manager->this_node.serialise());
						}
					}
					break;

				case DISCOVERY_SNEER:
					if (manager->state != STATE_READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (remote_node.name == manager->this_node.name &&
							remote_node.addr.sin_addr.s_addr == manager->this_node.addr.sin_addr.s_addr &&
							remote_node.http_port == manager->this_node.http_port &&
							remote_node.binary_port == manager->this_node.binary_port) {
							if (manager->node_name.empty()) {
								LOG_DISCOVERY(this, "Node name %s already taken. Retrying other name...\n", manager->this_node.name.c_str());
								manager->state = STATE_RESET;
								manager->discovery_heartbeat.set(0, 1);
							} else {
								LOG_ERR(this, "Cannot join the party. Node name %s already taken!\n", manager->this_node.name.c_str());
								manager->state = STATE_BAD;
								manager->this_node.name.clear();
								manager->shutdown_asap = time(NULL);
								manager->async_shutdown.send();
							}
						}
					}
					break;

				case DISCOVERY_PING:
					if (manager->state == STATE_READY) {
						if (unserialise_string(node_name, &ptr, end) == -1 || node_name.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						try {
							node_name = stringtolower(node_name);
							if (node_name == stringtolower(manager->this_node.name)) {
								node = &manager->this_node;
							} else {
								node = &manager->nodes.at(node_name);
							}
							node->touched = now;
							// Received a ping, return pong
							manager->discovery(DISCOVERY_PONG, serialise_string(manager->this_node.name));
						} catch (const std::out_of_range& err) {
							LOG_DISCOVERY(this, "Ignoring ping from unknown peer %s\n", node_name.c_str());
						}
					}
					break;

				case DISCOVERY_PONG:
					if (manager->state == STATE_READY) {
						if (unserialise_string(node_name, &ptr, end) == -1 || node_name.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						try {
							node_name = stringtolower(node_name);
							if (node_name == stringtolower(manager->this_node.name)) {
								node = &manager->this_node;
							} else {
								node = &manager->nodes.at(node_name);
							}
							node->touched = now;
						} catch (const std::out_of_range& err) {
							LOG_DISCOVERY(this, "Ignoring pong from unknown peer %s\n", node_name.c_str());
						}
					}
					break;

				case DISCOVERY_BYE:
					if (manager->state == STATE_READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						manager->nodes.erase(stringtolower(remote_node.name));
						INFO(this, "Node %s left the party!\n", remote_node.name.c_str());
					}
					break;

				case DISCOVERY_DB:
					if (manager->state == STATE_READY) {
						if (unserialise_string(index_path, &ptr, end) == -1 || node_name.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}
						endpoints.insert(Endpoint(index_path));
						if (database_pool->checkout(&database, endpoints, true)) {
							database_pool->checkin(&database);
							LOG_DISCOVERY(this, "Found database!\n");
							manager->discovery(
								DISCOVERY_DB_WAVE,
								serialise_length(0) +  // The mastery level of the database
								serialise_string(index_path) +  // The path of the index
								manager->this_node.serialise()  // The node where the index is at
							);
						}
					}
					break;

				case DISCOVERY_DB_WAVE:
					if (manager->state == STATE_READY) {
						mastery_level = unserialise_length(&ptr, end);
						if (mastery_level == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (unserialise_string(index_path, &ptr, end) == -1 || index_path.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}
					} else {
						break;
					}
					// continues as if it's a DISCOVERY_WAVE (adding the node):
				case DISCOVERY_WAVE:
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}
					try {
						node_name = stringtolower(remote_node.name);
						if (node_name == stringtolower(manager->this_node.name)) {
							node = &manager->this_node;
						} else {
							node = &manager->nodes.at(node_name);
						}
					} catch (const std::out_of_range& err) {
						node = &manager->nodes[stringtolower(remote_node.name)];
						node->name = remote_node.name;
						node->addr.sin_addr.s_addr = remote_node.addr.sin_addr.s_addr;
						node->http_port = remote_node.http_port;
						node->binary_port = remote_node.binary_port;
						INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)!\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
					}
					if (remote_node.addr.sin_addr.s_addr == node->addr.sin_addr.s_addr &&
						remote_node.http_port == node->http_port &&
						remote_node.binary_port == node->binary_port) {
						node->touched = now;
					}
					break;

			}
		}
	}
}


void XapiandServer::io_accept_http(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid http event (sock=%d): %s\n", http_sock, strerror(errno));
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sock = ::accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sock < 0) {
		if (errno != EAGAIN) {
			LOG_ERR(this, "ERROR: accept http error (sock=%d): %s\n", http_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new HttpClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}


#ifdef HAVE_REMOTE_PROTOCOL
void XapiandServer::io_accept_binary(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid binary event (sock=%d): %s\n", binary_sock, strerror(errno));
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sock = ::accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sock < 0) {
		if (errno != EAGAIN) {
			LOG_ERR(this, "ERROR: accept binary error (sock=%d): %s\n", binary_sock, strerror(errno));
		}
	} else {
		fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		new BinaryClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);
	}
}
#endif  /* HAVE_REMOTE_PROTOCOL */


void XapiandServer::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (discovery_sock == -1 && http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	discovery_sock = -1;
	http_sock = -1;
	binary_sock = -1;

	discovery_io.stop();
	http_io.stop();
	binary_io.stop();

	pthread_mutex_unlock(&qmtx);

	// http and binary sockets are closed in the manager.

	LOG_OBJ(this, "DESTROYED SERVER!\n");
}


void XapiandServer::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking server loop!\n");
	loop->break_loop();
}


std::list<BaseClient *>::const_iterator XapiandServer::attach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	std::list<BaseClient *>::const_iterator iterator = clients.insert(clients.end(), client);
	pthread_mutex_unlock(&clients_mutex);
	return iterator;
}


void XapiandServer::detach_client(BaseClient *client)
{
	pthread_mutex_lock(&clients_mutex);
	if (client->iterator != clients.end()) {
		clients.erase(client->iterator);
		client->iterator = clients.end();
		LOG_OBJ(this, "DETACHED CLIENT!\n");
	}
	pthread_mutex_unlock(&clients_mutex);
}


void XapiandServer::shutdown()
{
	pthread_mutex_lock(&clients_mutex);
	std::list<BaseClient *>::const_iterator it(clients.begin());
	while (it != clients.end()) {
		BaseClient *client = *(it++);
		client->shutdown();
	}
	pthread_mutex_unlock(&clients_mutex);

	if (manager->shutdown_asap) {
		if (http_clients <= 0) {
			manager->shutdown_now = manager->shutdown_asap;
		}
		destroy();
	}
	if (manager->shutdown_now) {
		break_loop.send();
	}
}
