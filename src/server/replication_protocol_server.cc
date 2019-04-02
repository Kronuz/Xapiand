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

#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "cassert.h"                        // for ASSERT
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

	ASSERT(sock == -1 || sock == watcher.fd);

	L_DEBUG_HOOK("ReplicationProtocolServer::io_accept_cb", "ReplicationProtocolServer::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	if ((EV_ERROR & revents) != 0) {
		L_EV("ERROR: got invalid replication protocol event {{sock:{}}}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	int client_sock = accept();
	if (client_sock != -1) {
		auto client = Worker::make_shared<ReplicationProtocolClient>(share_this<ReplicationProtocolServer>(), ev_loop, ev_flags, active_timeout, idle_timeout);

		client->init(client_sock);

		if (!client->init_replication()) {
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
	if (args.src_endpoint.is_local()) {
		ASSERT(!args.cluster_database);
		return;
	}

	bool replicated = false;

	const auto& normalized_path = args.src_endpoint.path;

	if (normalized_path == ".xapiand") {
		// Cluster database is always updated
		replicated = true;
	}

	if (string::startswith(normalized_path, ".xapiand/")) {
		// Index databases are always replicated
		replicated = true;
	}

	if (!replicated && exists(normalized_path + "/iamglass")) {
		// If database is already there, its also always updated
		replicated = true;
	}

	if (!replicated) {
		// Otherwise, check if the local node resolves as replicator
		auto local_node = Node::local_node();
		auto nodes = XapiandManager::resolve_index_nodes(normalized_path);
		for (const auto& shard_nodes : nodes) {
			for (const auto& node : shard_nodes) {
				if (Node::is_superset(local_node, node)) {
					replicated = true;
					break;
				}
			}
		}
	}

	if (!replicated) {
		ASSERT(!args.cluster_database);
		return;
	}

	auto node = args.src_endpoint.node();
	if (!node) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database (nonexistent node: {})", args.src_endpoint.node_name);
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}

	int port = (node->replication_port == XAPIAND_REPLICATION_SERVERPORT) ? XAPIAND_REPLICATION_SERVERPORT : node->replication_port;
	auto& host = node->host();

	auto client = Worker::make_shared<ReplicationProtocolClient>(share_this<ReplicationProtocolServer>(), ev_loop, ev_flags, active_timeout, idle_timeout, args.cluster_database);

	if (!client->init_replication(args.src_endpoint, args.dst_endpoint)) {
		client->detach();
		return;
	}

	int client_sock = TCP::connect(host.c_str(), std::to_string(port).c_str());
	if (client_sock == -1) {
		if (args.cluster_database) {
			L_CRIT("Cannot replicate cluster database");
			sig_exit(-EX_SOFTWARE);
		}
		return;
	}

	L_CONN("Connected to {}! (in socket {})", repr(args.src_endpoint.to_string()), client_sock);
	client->init(client_sock);

	client->start();
	L_DEBUG("Database {} being synchronized from {}{}" + DEBUG_COL + "...", repr(args.src_endpoint.to_string()), node->col().ansi(), node->name());
}


std::string
ReplicationProtocolServer::__repr__() const
{
	return string::format("<ReplicationProtocolServer {{cnt:{}, sock:{}}}{}{}{}>",
		use_count(),
		sock == -1 ? replication.sock : sock,
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}

#endif /* XAPIAND_CLUSTERING */
