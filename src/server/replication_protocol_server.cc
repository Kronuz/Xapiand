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

#include "replication_protocol_server.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "database/lock.h"                  // for lock_shard
#include "database/pool.h"                  // for DatabasePool
#include "database/shard.h"                 // for Shard
#include "database/utils.h"                 // for query_field_t
#include "error.hh"                         // for error:name, error::description
#include "fs.hh"                            // for exists
#include "manager.h"                        // for XapiandManager
#include "readable_revents.hh"              // for readable_revents
#include "replication_protocol.h"           // for ReplicationProtocol
#include "replication_protocol_client.h"    // for ReplicationProtocolClient
#include "repr.hh"                          // for repr
#include "strict_stox.hh"                   // for strict_stoll
#include "tcp.h"                            // for TCP::socket

 // #undef L_DEBUG
 // #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
#undef L_REPLICATION
#define L_REPLICATION L_ROSY_BROWN
// #undef L_EV
// #define L_EV L_MEDIUM_PURPLE


ReplicationProtocolServer::ReplicationProtocolServer(const std::shared_ptr<ReplicationProtocol>& replication_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv, int tries)
	: MetaBaseServer<ReplicationProtocolServer>(replication_, ev_loop_, ev_flags_, "Replication", TCP_TCP_NODELAY | TCP_SO_REUSEPORT),
	  replication(*replication_),
	  trigger_replication_async(*ev_loop)
{
	bind(hostname, serv, tries);

	trigger_replication_async.set<ReplicationProtocolServer, &ReplicationProtocolServer::trigger_replication_async_cb>(this);
	trigger_replication_async.start();
	L_EV("Start replication protocol's async trigger replication signal event");
}


ReplicationProtocolServer::~ReplicationProtocolServer() noexcept
{
	try {
		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
ReplicationProtocolServer::shutdown_impl(long long asap, long long now)
{
	L_CALL("ReplicationProtocolServer::stop_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		auto manager = XapiandManager::manager();
		if (now != 0 || (manager && !manager->replication_clients)) {
			stop(false);
			destroy(false);

			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


void
ReplicationProtocolServer::start_impl()
{
	L_CALL("ReplicationProtocolServer::start_impl()");

	Worker::start_impl();

	io.start(sock == -1 ? replication.sock : sock, ev::READ);
	L_EV("Start replication protocol's server accept event not needed {{sock:{}}}", sock == -1 ? replication.sock : sock);
}


int
ReplicationProtocolServer::accept()
{
	L_CALL("ReplicationProtocolServer::accept()");

	if (sock != -1) {
		return TCP::accept();
	}
	return replication.accept();
}


void
ReplicationProtocolServer::io_accept_cb([[maybe_unused]] ev::io& watcher, int revents)
{
	L_CALL("ReplicationProtocolServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("ReplicationProtocolServer::io_accept_cb:BEGIN");
	L_EV_END("ReplicationProtocolServer::io_accept_cb:END");

	assert(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("ReplicationProtocolServer::io_accept_cb", "ReplicationProtocolServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid replication protocol event {{sock:{}}}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock != -1) {
		auto client = Worker::make_shared<ReplicationProtocolClient>(share_this<ReplicationProtocolServer>(), ev_loop, ev_flags, active_timeout, idle_timeout);

		if (!client->init_replication(client_sock)) {
			io::close(client_sock);
			client->detach();
			return;
		}

		client->start();
	}
}


void
ReplicationProtocolServer::trigger_replication()
{
	L_CALL("ReplicationProtocolServer::trigger_replication()");

	trigger_replication_async.send();
}


void
ReplicationProtocolServer::trigger_replication_async_cb(ev::async&, [[maybe_unused]] int revents)
{
	L_CALL("ReplicationProtocolServer::trigger_replication_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("ReplicationProtocolServer::trigger_replication_async_cb:BEGIN");
	L_EV_END("ReplicationProtocolServer::trigger_replication_async_cb:END");

	TriggerReplicationArgs args;
	while (replication.trigger_replication_args.try_dequeue(args)) {
		trigger_replication(args);
	}
}


void
ReplicationProtocolServer::trigger_replication(const TriggerReplicationArgs& args)
{
	L_CALL("ReplicationProtocolServer::trigger_replication({{src_endpoint:{}, dst_endpoint:{}}})", args.src_endpoint.to_string(), args.dst_endpoint.to_string());

	if (args.src_endpoint.is_local()) {
		assert(!args.cluster_database);
		return;
	}

	bool replicated = false;
	std::vector<std::vector<std::shared_ptr<const Node>>> nodes;

	if (strings::startswith(args.dst_endpoint.path, ".xapiand/")) {
		// Index databases are always replicated
		replicated = true;
	}

	if (!replicated) {
		// Otherwise, check if the local node resolves as replicator
		auto local_node = Node::get_local_node();
		nodes = XapiandManager::resolve_nodes(XapiandManager::resolve_index_settings(args.dst_endpoint.path));
		assert(nodes.size() == 1);
		if (nodes.size() != 1) {
			L_ERR("Replication ignored endpoint: {}", repr(args.dst_endpoint.to_string()));
			assert(!args.cluster_database);
			return;
		}
		const auto& shards = nodes[0];
		for (const auto& shard_node : shards) {
			if (Node::is_superset(local_node, shard_node)) {
				replicated = true;
				break;
			}
		}
	}

	if (!replicated) {
		if (exists(args.dst_endpoint.path + "/iamglass")) {
			// If we're not replicating it, but database is already there, try removing it.

			// Get nodes for the endpoint.
			if (nodes.empty()) {
				nodes = XapiandManager::resolve_nodes(XapiandManager::resolve_index_settings(args.dst_endpoint.path));
				assert(nodes.size() == 1);
				if (nodes.size() != 1) {
					L_ERR("Replication ignored endpoint: {}", repr(args.dst_endpoint.to_string()));
					assert(!args.cluster_database);
					return;
				}
			}

			// Get fast write lock for replication or retry later
			std::unique_ptr<lock_shard> lk_shard_ptr;
			try {
				lk_shard_ptr = std::make_unique<lock_shard>(args.dst_endpoint, DB_REPLICA, false);
				lk_shard_ptr->lock(0, [=] {
					// If it cannot checkout because database is busy, retry when ready...
					::trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), args.dst_endpoint.path, args.src_endpoint, args.dst_endpoint);
				});
			} catch (const Xapian::DatabaseNotAvailableError&) {
				L_REPLICATION("Stalled endpoint removal deferred (not available): {} -->  {}", repr(args.src_endpoint.to_string()), repr(args.dst_endpoint.to_string()));
				return;
			} catch (...) {
				L_EXC("ERROR: Stalled endpoint removal ended with an unhandled exception");
				return;
			}

			// Retrieve local database uuid and revision.
			auto shard = lk_shard_ptr->locked();
			auto db = shard->db();
			auto uuid = db->get_uuid();
			auto revision = db->get_revision();

			size_t total = 0;
			size_t ok = 0;

			// Figure out remote uuid and revisions.
			const auto& shards = nodes[0];
			for (const auto& shard_node : shards) {
				++total;
				try {
					lock_shard lk_shard(Endpoint{args.dst_endpoint.path, shard_node}, DB_WRITABLE, false);
					auto remote_shard = lk_shard.lock(0);
					auto remote_db = remote_shard->db();
					auto remote_uuid = remote_db->get_uuid();
					auto remote_revision = remote_db->get_revision();
					if (remote_uuid == uuid && remote_revision >= revision) {
						++ok;
					}
				} catch (...) { }
			}

			// If there are enough remote valid databases, remove the local one.
			if (Node::quorum(total, ok)) {
				L_REPLICATION("Remove stalled shard: {}", args.dst_endpoint.path);

				// Close internal databases
				shard->do_close();

				// get exclusive lock
				XapiandManager::manager(true)->database_pool->lock(shard);

				// Now we are sure no readers are using the database before removing the files
				delete_files(shard->endpoint.path, {"*glass", "wal.*", "flintlock"});

				// release exclusive lock
				XapiandManager::manager(true)->database_pool->unlock(shard);
			} else {
				L_WARNING("Stalled shard: {}", args.dst_endpoint.path);
			}

			return;
		}
	}

	if (!replicated) {
		assert(!args.cluster_database);
		return;
	}

	auto node = args.src_endpoint.node();
	if (!node || node->empty()) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database (Endpoint node is invalid: {})", args.src_endpoint.node_name);
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}
	if (!node->is_active()) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database (Endpoint node is inactive: {})", args.src_endpoint.node_name);
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}
	int port = node->replication_port;
	if (port == 0) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database (Endpoint node without a valid port: {})", args.src_endpoint.node_name);
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}
	auto& host = node->host();
	if (host.empty()) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database (Endpoint node without a valid host: {})", args.src_endpoint.node_name);
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}

	auto client = Worker::make_shared<ReplicationProtocolClient>(share_this<ReplicationProtocolServer>(), ev_loop, ev_flags, active_timeout, idle_timeout, args.cluster_database);

	if (!client->init_replication(host, port, args.src_endpoint, args.dst_endpoint)) {
		client->detach();
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database");
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}

	client->start();
	L_DEBUG("Database {} being synchronized from {}{}" + DEBUG_COL + "...", repr(args.src_endpoint.to_string()), node->col().ansi(), node->name());
}


std::string
ReplicationProtocolServer::__repr__() const
{
	return strings::format(STEEL_BLUE + "<ReplicationProtocolServer {{cnt:{}, sock:{}}}{}{}{}>",
		use_count(),
		sock == -1 ? replication.sock : sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}

#endif /* XAPIAND_CLUSTERING */
