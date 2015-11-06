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

#include "server_discovery.h"

#include "binary.h"
#include "discovery.h"
#include "server.h"

#include <assert.h>


DiscoveryServer::DiscoveryServer(std::shared_ptr<XapiandServer>&& server_, ev::loop_ref *loop_, std::unique_ptr<Discovery> &discovery_)
	: BaseServer(std::move(server_), loop_, discovery_->sock),
	discovery(discovery_)
{
	LOG_EV(this, "Start discovery event (sock=%d)\n", discovery->sock);
	LOG_OBJ(this, "CREATED DISCOVERY SERVER!\n");
}


DiscoveryServer::~DiscoveryServer()
{
	LOG_OBJ(this, "DELETED DISCOVERY SERVER!\n");
}


void
DiscoveryServer::io_accept(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		LOG_EV(this, "ERROR: got invalid discovery event (sock=%d): %s\n", discovery->sock, strerror(errno));
		return;
	}

	assert(discovery->sock == watcher.fd || discovery->sock == -1);

	if (revents & EV_READ) {
		char buf[1024];
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);

		ssize_t received = ::recvfrom(watcher.fd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);

		if (received < 0) {
			if (!ignored_errorno(errno, true)) {
				LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", discovery->sock, strerror(errno));
				server()->shutdown();
			}
		} else if (received == 0) {
			// If no messages are available to be received and the peer has performed an orderly shutdown.
			LOG_CONN(this, "Received EOF (sock=%d)!\n", discovery->sock);
			server()->shutdown();
		} else {
			LOG_UDP_WIRE(this, "Discovery: (sock=%d) -->> '%s'\n", discovery->sock, repr(buf, received).c_str());

			if (received < 4) {
				LOG_DISCOVERY(this, "Badly formed message: Incomplete!\n");
				return;
			}

			uint16_t remote_protocol_version = *(uint16_t *)(buf + 1);
			if ((remote_protocol_version & 0xff) > XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) {
				LOG_DISCOVERY(this, "Badly formed message: Protocol version mismatch %x vs %x!\n", remote_protocol_version & 0xff, XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION);
				return;
			}

			const char *ptr = buf + 3;
			const char *end = buf + received;

			std::string remote_cluster_name;
			if (unserialise_string(remote_cluster_name, &ptr, end) == -1 || remote_cluster_name.empty()) {
				LOG_DISCOVERY(this, "Badly formed message: No cluster name!\n");
				return;
			}
			if (remote_cluster_name != discovery->manager->cluster_name) {
				return;
			}

			const Node *node;
			Node remote_node;
			std::string index_path;
			std::string mastery_str;
			long long mastery_level;
			long long remote_mastery_level;
			time_t now = epoch::now();
			int region;

			char cmd = buf[0];
			switch (cmd) {
				case toUType(Discovery::Message::HELLO):
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}
					if (remote_node == local_node) {
						// It's me! ...wave hello!
						discovery->send_message(Discovery::Message::WAVE, local_node.serialise());
					} else {
						if (discovery->manager->touch_node(remote_node.name, remote_node.region.load(), &node)) {
							if (remote_node == *node) {
								discovery->send_message(Discovery::Message::WAVE, local_node.serialise());
							} else {
								discovery->send_message(Discovery::Message::SNEER, remote_node.serialise());
							}
						} else {
							discovery->send_message(Discovery::Message::WAVE, local_node.serialise());
						}
					}
					break;

				case toUType(Discovery::Message::SNEER):
					if (discovery->manager->state != XapiandManager::State::READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (remote_node == local_node) {
							if (discovery->manager->node_name.empty()) {
								LOG_DISCOVERY(this, "Node name %s already taken. Retrying other name...\n", local_node.name.c_str());
								discovery->manager->reset_state();
							} else {
								LOG_ERR(this, "Cannot join the party. Node name %s already taken!\n", local_node.name.c_str());
								discovery->manager->state = XapiandManager::State::BAD;
								local_node.name.clear();
								discovery->manager->shutdown_asap = now;
								discovery->manager->async_shutdown.send();
							}
						}
					}
					break;

				case toUType(Discovery::Message::WAVE):
				case toUType(Discovery::Message::HEARTBEAT):
					if (remote_node.unserialise(&ptr, end) == -1) {
						LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
						return;
					}

					if (remote_node == local_node) {
						region = local_node.region.load();
					} else {
						region = remote_node.region.load();
					}

					if (discovery->manager->touch_node(remote_node.name, region, &node)) {
						if (remote_node != *node && remote_node.name != local_node.name) {
							if (cmd == toUType(Discovery::Message::HEARTBEAT) || node->touched < now - HEARTBEAT_MAX) {
								discovery->manager->drop_node(remote_node.name);
								INFO(this, "Stalled node %s left the party!\n", remote_node.name.c_str());
								if (discovery->manager->put_node(remote_node)) {
									INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian) (2)!\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
									local_node.regions.store(-1);
									discovery->manager->get_region();
								} else {
									LOG_ERR(this, "ERROR: Cannot register remote node (1): %s\n", remote_node.name.c_str());
								}
							}
						}
					} else {
						if (discovery->manager->put_node(remote_node)) {
							INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian) (1)!\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
							local_node.regions.store(-1);
							discovery->manager->get_region();
						} else {
							LOG_ERR(this, "ERROR: Cannot register remote node (2): %s\n", remote_node.name.c_str());
						}
					}
					break;

				case toUType(Discovery::Message::BYE):
					if (discovery->manager->state == XapiandManager::State::READY) {
						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						discovery->manager->drop_node(remote_node.name);
						INFO(this, "Node %s left the party!\n", remote_node.name.c_str());
					}
					break;

				case toUType(Discovery::Message::DB):
					if (discovery->manager->state == XapiandManager::State::READY) {
						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}

						if (discovery->manager->get_region() == discovery->manager->get_region(index_path) /* FIXME: missing leader check */) {
							if (discovery->manager->endp_r.get_master_node(index_path, &node, discovery->manager)) {
								discovery->send_message(
												Discovery::Message::BOSSY_DB_WAVE,
												serialise_string(mastery_str) +  // The mastery level of the database
												serialise_string(index_path) +  // The path of the index
												node->serialise()					// The node where the index master is at
												);
								return;
							}
						}

						mastery_level = manager()->database_pool.get_mastery_level(index_path);
						if (mastery_level != -1) {
								LOG_DISCOVERY(this, "Found local database '%s' with m:%llx!\n", index_path.c_str(), mastery_level);
								mastery_str = std::to_string(mastery_level);
								discovery->send_message(
												Discovery::Message::DB_WAVE,
												serialise_string(mastery_str) +  // The mastery level of the database
												serialise_string(index_path) +  // The path of the index
												local_node.serialise()  // The node where the index is at
												);
						}
					}
					break;

				case toUType(Discovery::Message::DB_WAVE):
				case toUType(Discovery::Message::BOSSY_DB_WAVE):
					if (discovery->manager->state == XapiandManager::State::READY) {
						if (unserialise_string(mastery_str, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper mastery!\n");
							return;
						}
						remote_mastery_level = strtoll(mastery_str);

						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}

						if (remote_node.unserialise(&ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
							return;
						}
						if (discovery->manager->put_node(remote_node)) {
							INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (3)\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
						}

						LOG_DISCOVERY(this, "Node %s has '%s' with a mastery of %llx!\n", remote_node.name.c_str(), index_path.c_str(), remote_mastery_level);

						if (discovery->manager->get_region() == discovery->manager->get_region(index_path)) {
							LOG(this, "The DB is in the same region that this cluster!\n");
							Endpoint index(index_path, &remote_node, remote_mastery_level, remote_node.name);
							discovery->manager->endp_r.add_index_endpoint(index, true, cmd == toUType(Discovery::Message::BOSSY_DB_WAVE));
						} else if (discovery->manager->endp_r.exists(index_path)) {
							LOG(this, "The DB is in the LRU of this node!\n");
							Endpoint index(index_path, &remote_node, remote_mastery_level, remote_node.name);
							discovery->manager->endp_r.add_index_endpoint(index, false, cmd == toUType(Discovery::Message::BOSSY_DB_WAVE));
						}
					}
					break;

				case toUType(Discovery::Message::DB_UPDATED):
					if (discovery->manager->state == XapiandManager::State::READY) {
						if (unserialise_string(mastery_str, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No proper mastery!\n");
							return;
						}
						remote_mastery_level = strtoll(mastery_str);

						if (unserialise_string(index_path, &ptr, end) == -1) {
							LOG_DISCOVERY(this, "Badly formed message: No index path!\n");
							return;
						}

						mastery_level = manager()->database_pool.get_mastery_level(index_path);
						if (mastery_level == -1) {
							return;
						}

						if (mastery_level > remote_mastery_level) {
							LOG_DISCOVERY(this, "Mastery of remote's %s wins! (local:%llx > remote:%llx) - Updating!\n", index_path.c_str(), mastery_level, remote_mastery_level);
							if (remote_node.unserialise(&ptr, end) == -1) {
								LOG_DISCOVERY(this, "Badly formed message: No proper node!\n");
								return;
							}
							if (discovery->manager->put_node(remote_node)) {
								INFO(this, "Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (4)\n", remote_node.name.c_str(), inet_ntoa(remote_node.addr.sin_addr), remote_node.http_port, remote_node.binary_port);
							}

							Endpoint local_endpoint(index_path);
							Endpoint remote_endpoint(index_path, &remote_node);
#ifdef HAVE_REMOTE_PROTOCOL
							// Replicate database from the other node
							INFO(this, "Request syncing database from %s...\n", remote_node.name.c_str());
							if (manager()->binary->trigger_replication(remote_endpoint, local_endpoint, server())) {
								INFO(this, "Database being synchronized from %s...\n", remote_node.name.c_str());
							}
#endif
						} else if (mastery_level != remote_mastery_level) {
							LOG_DISCOVERY(this, "Mastery of local's %s wins! (local:%llx <= remote:%llx) - Ignoring update!\n", index_path.c_str(), mastery_level, remote_mastery_level);
						}
					}
					break;
			}
		}
	}
}
