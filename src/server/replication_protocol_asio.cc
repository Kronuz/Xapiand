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

#include "config.h"   // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

// IMPORTANT: libev (via these Xapiand headers) must be included BEFORE the Asio-pulling
// replication_protocol_asio.h (EV_ERROR enum vs macro clash). The outbound trigger's decision
// logic (node resolution, shard locks, stalled-DB removal) is verbatim from the classic
// ReplicationProtocolServer::trigger_replication.
#include <cassert>                          // for assert
#include <sysexits.h>                       // for EX_SOFTWARE
#include <vector>                           // for std::vector

#include "database/flags.h"                 // for DB_*
#include "database/lock.h"                  // for lock_shard
#include "database/pool.h"                  // for DatabasePool
#include "database/shard.h"                 // for Shard
#include "fs.hh"                            // for exists, delete_files, quarantine_files
#include "index_resolver_lru.h"             // for IndexResolverLRU
#include "manager.h"                        // for XapiandManager
#include "node.h"                           // for Node
#include "random.hh"                        // for random_int
#include "repr.hh"                          // for repr

#include "server/replication_protocol_asio.h"   // the service + connect_and_replicate (Asio)

#include <asio.hpp>

namespace replication {

void cluster_database_replication_failed() {
	L_CRIT("Cannot replicate cluster database");
	sig_exit(-EX_SOFTWARE);
}


void
ReplicationProtocolAsioService::trigger_replication(const TriggerReplicationArgs& args)
{
	L_CALL("ReplicationProtocolAsioService::trigger_replication({{src_endpoint:{}, dst_endpoint:{}}})", args.src_endpoint.to_string(), args.dst_endpoint.to_string());

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
		nodes = IndexResolverLRU::resolve_nodes(XapiandManager::resolve_index_settings(args.dst_endpoint.path));
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
				nodes = IndexResolverLRU::resolve_nodes(XapiandManager::resolve_index_settings(args.dst_endpoint.path));
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
				lk_shard_ptr = std::make_unique<lock_shard>(args.dst_endpoint, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_REPLICA, false);
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

			// Figure out remote uuid and revisions.
			const auto& shards = nodes[0];
			size_t total_nodes = shards.size();
			size_t ok_nodes = 0;
			for (const auto& shard_node : shards) {
				try {
					lock_shard lk_shard(Endpoint{args.dst_endpoint.path, shard_node}, DB_OPEN | DB_WRITABLE, false);
					auto remote_shard = lk_shard.lock(0);
					auto remote_db = remote_shard->db();
					auto remote_uuid = remote_db->get_uuid();
					auto remote_revision = remote_db->get_revision();
					if (remote_uuid == uuid && remote_revision >= revision) {
						++ok_nodes;
					}
				} catch (...) { }
			}

			L_REPLICATION("Remove stalled shard: {}", args.dst_endpoint.path);

			// Close internal databases
			shard->do_close();

			// get exclusive lock
			XapiandManager::manager(true)->database_pool->lock(shard);

			// Now we are sure no readers are using the database before removing/moving the files
			if (Node::quorum(total_nodes, ok_nodes)) {
				// If there are enough remote valid databases, remove the local one.
				delete_files(shard->endpoint.path, {"*glass", "wal.*", "flintlock"});
			} else {
				// Quarantine WAL instead of deleting
				L_WARNING("Stalled shard: {}", args.dst_endpoint.path);
				quarantine_files(shard->endpoint.path, {"*glass", "wal.*", "flintlock"});
			}

			// release exclusive lock
			XapiandManager::manager(true)->database_pool->unlock(shard);

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

	// Spawn the outbound coroutine on one of the reactors (round-robin). It does the blocking
	// lock + connect on the offload pool, then drives the client role. The retry-on-failure is
	// scheduled inside init_replication_protocol (the delayed_debounce), and a cluster-database
	// failure is fatal (cluster_database_replication_failed), matching the classic path.
	std::size_t idx = next_reactor_++ % server_.reactors();
	reactor::Reactor& r = server_.reactor(idx);
	std::string host_copy(host);
	int port_copy = port;
	Endpoint src = args.src_endpoint;
	Endpoint dst = args.dst_endpoint;
	bool cluster_database = args.cluster_database;
	asio::post(r.io(), [&r, host_copy, port_copy, src, dst, cluster_database]() {
		asio::co_spawn(r.io(), detail::connect_and_replicate(&r, host_copy, port_copy, src, dst, cluster_database), asio::detached);
	});

	L_DEBUG("Database {} being synchronized from {}{}" + DEBUG_COL + "...", repr(args.src_endpoint.path), node->col().ansi(), node->name());
}

}  // namespace replication

#endif  // XAPIAND_CLUSTERING
