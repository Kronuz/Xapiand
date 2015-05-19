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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
	: Worker(manager_, loop_),
	  http_io(*loop),
	  discovery_io(*loop),
	  binary_io(*loop),
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

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

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
			if (discovery_sock != -1 && !ignored_errorno(errno, true)) {
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
			if (remote_cluster_name != manager()->cluster_name) {
				return;
			}

			// LOG_DISCOVERY(this, "%s on ip:%s, tcp:%d (http), tcp:%d (xapian), at pid:%d\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);

			Node node;
			Node remote_node;
			Database *database = NULL;
			std::string index_path;
			std::string node_name;
			size_t mastery_level;
			size_t remote_mastery_level;

			char cmd = buf[0];
			switch (cmd) {
				case DISCOVERY_HELLO:
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}
					if (remote_node == manager()->this_node) {
						// It's me! ...wave hello!
						manager()->discovery(DISCOVERY_WAVE, manager()->this_node.serialise());
					} else {
						if (manager()->touch_node(remote_node.name, &node)) {
							if (remote_node == node) {
								manager()->discovery(DISCOVERY_WAVE, manager()->this_node.serialise());
							} else {
								manager()->discovery(DISCOVERY_SNEER, remote_node.serialise());
							}
						} else {
							manager()->discovery(DISCOVERY_WAVE, manager()->this_node.serialise());
						}
					}
					break;

				case DISCOVERY_SNEER:
					if (manager()->state != STATE_READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (remote_node == manager()->this_node) {
							if (manager()->node_name.empty()) {
								LOG_DISCOVERY(this, "Node name %s already taken. Retrying other name...\n", manager()->this_node.name.c_str());
								manager()->reset_state();
							} else {
								LOG_ERR(this, "Cannot join the party. Node name %s already taken!\n", manager()->this_node.name.c_str());
								manager()->state = STATE_BAD;
								manager()->this_node.name.clear();
								manager()->shutdown_asap = time(NULL);
								manager()->async_shutdown.send();
							}
						}
					}
					break;

				case DISCOVERY_PING:
					if (manager()->state == STATE_READY) {
						if (unserialise_string(node_name, &ptr, end) == -1 || node_name.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (manager()->touch_node(node_name)) {
							// Received a ping, return pong
							manager()->discovery(DISCOVERY_PONG, serialise_string(manager()->this_node.name));
						} else {
							LOG_DISCOVERY(this, "Ignoring ping from unknown peer %s\n", node_name.c_str());
						}
					}
					break;

				case DISCOVERY_PONG:
					if (manager()->state == STATE_READY) {
						if (unserialise_string(node_name, &ptr, end) == -1 || node_name.empty()) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (manager()->touch_node(node_name)) {
							// Do nothing (node was touched)
						} else {
							LOG_DISCOVERY(this, "Ignoring pong from unknown peer %s\n", node_name.c_str());
						}
					}
					break;

				case DISCOVERY_BYE:
					if (manager()->state == STATE_READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						manager()->drop_node(remote_node.name);
						INFO(this, "Node %s left the party!\n", remote_node.name.c_str());
					}
					break;

				case DISCOVERY_DB:
					if (manager()->state == STATE_READY) {
						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}
						mastery_level = database_pool->get_mastery_level(index_path);
						if (mastery_level != -1) {
							LOG_DISCOVERY(this, "Found local database '%s' with m:%d!\n", index_path.c_str(), mastery_level);
							manager()->discovery(
								DISCOVERY_DB_WAVE,
								serialise_length(mastery_level) +  // The mastery level of the database
								serialise_string(index_path) +  // The path of the index
								manager()->this_node.serialise()  // The node where the index is at
							);
						}
					}
					break;

				case DISCOVERY_DB_WAVE:
					if (manager()->state == STATE_READY) {
						remote_mastery_level = unserialise_length(&ptr, end);
						if (remote_mastery_level == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}

						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (manager()->put_node(remote_node)) {
							INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (1)\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
						}

						LOG_DISCOVERY(this, "Node %s has '%s' with a mastery of %d!\n", remote_node.name.c_str(), index_path.c_str(), remote_mastery_level);
					}
					break;

				case DISCOVERY_DB_UPDATED:
					if (manager()->state == STATE_READY) {
						remote_mastery_level = unserialise_length(&ptr, end);
						if (remote_mastery_level == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}

						mastery_level = database_pool->get_mastery_level(index_path);
						if (mastery_level > remote_mastery_level) {
							if (remote_node.unserialise(&ptr, end) == -1) {
								LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
								return;
							}
							if (manager()->put_node(remote_node)) {
								INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (2)\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
							}

							Endpoint local_endpoint("xapian://" + manager()->this_node.host_port() + "/" + index_path);
							Endpoint remote_endpoint(index_path, remote_node);
							// Replicate database from the other node
							INFO(this, "Syncing database from %s...\n", remote_node.name.c_str());
							if (trigger_replication(remote_endpoint, local_endpoint)) {
								INFO(this, "Database being synchronized from %s...\n", remote_node.name.c_str());
							}
						}
					}
					break;

				case DISCOVERY_WAVE:
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}
					if (manager()->put_node(remote_node)) {
						INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian) (3)!\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
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

	int client_sock;
	if ((client_sock = accept_tcp(watcher.fd)) < 0) {
		if (!ignored_errorno(errno, false)) {
			LOG_ERR(this, "ERROR: accept http error (sock=%d): %s\n", http_sock, strerror(errno));
		}
	} else {
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

	int client_sock;
	if ((client_sock = accept_tcp(watcher.fd)) < 0) {
		if (!ignored_errorno(errno, false)) {
			LOG_ERR(this, "ERROR: accept binary error (sock=%d): %s\n", binary_sock, strerror(errno));
	    }
	} else {
		double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
		double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
		BinaryClient *client = new BinaryClient(this, loop, client_sock, database_pool, thread_pool, active_timeout, idle_timeout);

		if (!client->init_remote()) {
			delete client;
		}
	}
}


bool XapiandServer::trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	int sock;
	if ((sock = connect_tcp(src_endpoint.host.c_str(), std::to_string(src_endpoint.port).c_str())) < 0) {
		return false;
	}

	double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
	double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
	BinaryClient *client = new BinaryClient(this, loop, sock, database_pool, thread_pool, active_timeout, idle_timeout);

	if (!client->init_replication(src_endpoint, dst_endpoint)) {
		delete client;
		return false;
	}

	return true;
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


void XapiandServer::shutdown()
{
	Worker::shutdown();

	if (manager()->shutdown_asap) {
		if (http_clients <= 0) {
			manager()->shutdown_now = manager()->shutdown_asap;
		}
		destroy();
	}
	if (manager()->shutdown_now) {
		break_loop();
	}
}
