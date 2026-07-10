/*
 * Copyright (c) 2015-2020 Dubalu LLC
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

#include "discovery.h"

#ifdef XAPIAND_CLUSTERING

#include <algorithm>                        // for std::find_if
#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "color_tools.hh"                   // for color
#include "cuuid_v1.hh"                         // for UUID
#include "database/lock.h"                  // for lock_shard
#include "database/shard.h"                 // for Shard
#include "database/schemas_lru.h"           // for SchemasLRU
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "index_resolver_lru.h"             // for IndexSettings
#include "exception_xapian.h"               // for InvalidArgumentError
#include "manager.h"                        // for XapiandManager, XapiandManager::State
#include "xapiand_namegen.h"                        // for name_generator
#include "node.h"                           // for Node, local_node
#include "opts.h"                           // for opts::*
#include "random.hh"                        // for random_int
#include "repr.hh"                          // for repr
#include "utype.hh"                         // for toUType


#define L_RAFT_PROTO L_NOTHING
#define L_RAFT_PROTO_HB L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DISCOVERY
// #define L_DISCOVERY L_SALMON
// #undef L_RAFT
// #define L_RAFT L_MEDIUM_SEA_GREEN
// #undef L_RAFT_PROTO
// #define L_RAFT_PROTO L_SEA_GREEN
// #define L_RAFT_LOG L_LIGHT_SEA_GREEN
// #undef L_RAFT_PROTO_HB
// #define L_RAFT_PROTO_HB L_DIM_GREY
// #undef L_EV_BEGIN
// #define L_EV_BEGIN L_DELAYED_200
// #undef L_EV_END
// #define L_EV_END L_DELAYED_N_UNLOG


constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;

// Values in seconds
constexpr double RAFT_LEADER_HEARTBEAT_TIMEOUT    = HEARTBEAT_TIMEOUT;

constexpr double RAFT_LEADER_ELECTION_INIT        = 4.0 * RAFT_LEADER_HEARTBEAT_TIMEOUT;
constexpr double RAFT_LEADER_ELECTION_MIN         = 10.0 * RAFT_LEADER_HEARTBEAT_TIMEOUT;  // same as NODE_LIFESPAN
constexpr double RAFT_LEADER_ELECTION_MAX         = 30.0 * RAFT_LEADER_HEARTBEAT_TIMEOUT;

constexpr double CLUSTER_DISCOVERY_WAITING_FAST   = RAFT_LEADER_HEARTBEAT_TIMEOUT / 3.0 * 2.0;
constexpr double CLUSTER_DISCOVERY_WAITING_SLOW   = RAFT_LEADER_HEARTBEAT_TIMEOUT * 2.0;


// Map a logical cluster::RaftMessage onto its DiscoveryMessage wire value (the reverse map,
// wire -> logical, lives inline in the on_message router).
static Discovery::Message raft_to_wire(cluster::RaftMessage msg) {
	switch (msg) {
		case cluster::RaftMessage::REQUEST_VOTE:             return Discovery::Message::RAFT_REQUEST_VOTE;
		case cluster::RaftMessage::REQUEST_VOTE_RESPONSE:    return Discovery::Message::RAFT_REQUEST_VOTE_RESPONSE;
		case cluster::RaftMessage::APPEND_ENTRIES:           return Discovery::Message::RAFT_APPEND_ENTRIES;
		case cluster::RaftMessage::HEARTBEAT:                return Discovery::Message::RAFT_HEARTBEAT;
		case cluster::RaftMessage::APPEND_ENTRIES_RESPONSE:  return Discovery::Message::RAFT_APPEND_ENTRIES_RESPONSE;
		case cluster::RaftMessage::HEARTBEAT_RESPONSE:       return Discovery::Message::RAFT_HEARTBEAT_RESPONSE;
		case cluster::RaftMessage::ADD_COMMAND:              return Discovery::Message::RAFT_ADD_COMMAND;
	}
	return Discovery::Message::RAFT_HEARTBEAT;  // unreachable (all cases returned)
}


Discovery::Discovery(const char* group, unsigned int port)
	: port_(static_cast<unsigned short>(port)),
	  group_(group),
	  bus_(
		XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION,
		XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION,
		opts.cluster_name,
		toUType(Message::MAX),
		[this](int wire_type, std::string_view content, const asio::ip::udp::endpoint& from) {
			on_message(wire_type, content, from);
		}),
	  delegate_([this](cluster::RaftMessage msg, const std::string& payload) {
		send_message(raft_to_wire(msg), payload);
	  }),
	  raft_(bus_.io().get_executor(),
		cluster::RaftConfig{
			RAFT_LEADER_HEARTBEAT_TIMEOUT,
			RAFT_LEADER_ELECTION_INIT,
			RAFT_LEADER_ELECTION_MIN,
			RAFT_LEADER_ELECTION_MAX,
		},
		&delegate_),
	  cluster_discovery_(bus_.io().get_executor()),
	  cluster_enter_signal_(bus_.io().get_executor()),
	  message_send_signal_(bus_.io().get_executor()),
	  _ASYNC_elected_primaries(0, 600s)
{
	// Reproduce the classic discovery UDP socket options (udp.cc): SO_REUSEADDR + SO_REUSEPORT,
	// IP_MULTICAST_LOOP, IP_MULTICAST_TTL = 3, and join the group. Honor --discovery-interface
	// (IP_MULTICAST_IF + interface-scoped join) so a VPN default route can't swallow gossip.
	reactor::UdpOptions udp_opts;
	udp_opts.reuse_address = true;
	udp_opts.reuse_port = true;
	udp_opts.multicast_group = group_;
	udp_opts.multicast_loop = true;
	udp_opts.multicast_ttl = 3;
	udp_opts.multicast_interface = opts.discovery_interface;
	bus_.set_options(udp_opts);

	cluster_discovery_.set_callback([this] { cluster_discovery_cb(); });
	cluster_enter_signal_.set_callback([this] { cluster_enter_signal_cb(); });
	message_send_signal_.set_callback([this] { message_send_signal_cb(); });

	raft_.set_eligible(true);   // matches the classic raft_eligible(true) default
}


Discovery::~Discovery() noexcept
{
	try {
		bus_.stop();   // idempotent: stop the reactor loop + join the bus thread if still running
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
Discovery::run()
{
	L_CALL("Discovery::run()");

	// Bind + join the multicast group + launch the Bus reactor thread (the receive loop). The
	// raft election timer stays dormant until the node reaches JOINING (cluster_discovery_cb).
	bus_.start(port_);

	L_DISCOVERY("Discovery was started! (exploring)");
}


void
Discovery::start()
{
	L_CALL("Discovery::start()");

	cluster_discovery_.start(0, CLUSTER_DISCOVERY_WAITING_FAST);
	L_EV("Start discovery's cluster_discovery exploring event ({})", cluster_discovery_.repeat());
}


void
Discovery::stop()
{
	L_CALL("Discovery::stop()");

	// Graceful hand-off (the classic shutdown_impl + stop_impl): step down as leader so the
	// cluster re-elects promptly, then wave goodbye (peers drop us + re-run their own election
	// on CLUSTER_BYE). The BYE is sent synchronously (reliable); the relinquish is best-effort.
	raft_relinquish_leadership();

	auto local_node = Node::get_local_node();
	send_message(Message::CLUSTER_BYE, local_node->serialise());
	L_INFO("Waving goodbye to cluster {}!", opts.cluster_name);

	raft_.stop();
	L_EV("Stop raft's leader heartbeat + election timeout events");

	cluster_discovery_.stop();
	L_EV("Stop discovery's cluster_discovery event");

	L_DISCOVERY("Discovery was stopped!");
}


void
Discovery::finish()
{
	L_CALL("Discovery::finish()");

	// Stop the Bus reactor loop + join its thread (bounded, idempotent).
	bus_.stop();
}


bool
Discovery::join([[maybe_unused]] std::chrono::milliseconds timeout)
{
	L_CALL("Discovery::join()");

	// The Bus reactor thread is torn down by finish(); ensure it is joined (idempotent) and
	// report done so the manager's drain loop exits.
	bus_.stop();
	return true;
}


void
Discovery::send_message(Message type, const std::string& message)
{
	L_CALL("Discovery::send_message({}, <message>)", enum_name(type));

	L_DISCOVERY_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));
	bus_.send(toUType(type), message);
}


void
Discovery::on_message(int wire_type, std::string_view content, [[maybe_unused]] const asio::ip::udp::endpoint& from)
{
	// The Bus already validated the frame (version <= ours, type in range, cluster token
	// matches) and stripped the header, so `content` is the classic message body. This runs on
	// the bus reactor thread -- the same loop raft/timers run on, so nothing races.
	Message type = static_cast<Message>(wire_type);
	L_DISCOVERY_PROTO(">>> get_message ({}): {}", enum_name(type), repr(content));

	L_EV_BEGIN("Discovery::on_message:BEGIN {{ state:{}, type:{} }}", enum_name(XapiandManager::get_state()), enum_name(type));
	L_EV_END("Discovery::on_message:END {{ state:{}, type:{} }}", enum_name(XapiandManager::get_state()), enum_name(type));

	std::string message(content);

	switch (type) {
		case Message::CLUSTER_HELLO:
			cluster_hello(type, message);
			return;
		case Message::CLUSTER_WAVE:
			cluster_wave(type, message);
			return;
		case Message::CLUSTER_SNEER:
			cluster_sneer(type, message);
			return;
		case Message::CLUSTER_ENTER:
			cluster_enter(type, message);
			return;
		case Message::CLUSTER_BYE:
			cluster_bye(type, message);
			return;
		case Message::RAFT_HEARTBEAT:
			raft_.on_message(cluster::RaftMessage::HEARTBEAT, content);
			return;
		case Message::RAFT_APPEND_ENTRIES:
			raft_.on_message(cluster::RaftMessage::APPEND_ENTRIES, content);
			return;
		case Message::RAFT_HEARTBEAT_RESPONSE:
			raft_.on_message(cluster::RaftMessage::HEARTBEAT_RESPONSE, content);
			return;
		case Message::RAFT_APPEND_ENTRIES_RESPONSE:
			raft_.on_message(cluster::RaftMessage::APPEND_ENTRIES_RESPONSE, content);
			return;
		case Message::RAFT_REQUEST_VOTE:
			raft_.on_message(cluster::RaftMessage::REQUEST_VOTE, content);
			return;
		case Message::RAFT_REQUEST_VOTE_RESPONSE:
			raft_.on_message(cluster::RaftMessage::REQUEST_VOTE_RESPONSE, content);
			return;
		case Message::RAFT_ADD_COMMAND:
			raft_.on_message(cluster::RaftMessage::ADD_COMMAND, content);
			return;
		case Message::DB_UPDATED:
			db_updated(type, message);
			return;
		case Message::SCHEMA_UPDATED:
			schema_updated(type, message);
			return;
		case Message::INDEX_SETTINGS_UPDATED:
			index_settings_updated(type, message);
			return;
		case Message::PRIMARY_UPDATED:
			// Dispatch the following asynchronously...
			// it could be too slow for doing inside Discovery thread:
			XapiandManager::dispatch_command(XapiandManager::Command::ASYNC_PRIMARY_UPDATED, message);
			return;
		case Message::ELECT_PRIMARY:
			// Dispatch the following asynchronously...
			// it could be too slow for doing inside Discovery thread:
			XapiandManager::dispatch_command(XapiandManager::Command::ASYNC_ELECT_PRIMARY, message);
			return;
		case Message::ELECT_PRIMARY_RESPONSE:
			// Dispatch the following asynchronously...
			// it could be too slow for doing inside Discovery thread:
			XapiandManager::dispatch_command(XapiandManager::Command::ASYNC_ELECT_PRIMARY_RESPONSE, message);
			return;
		default: {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
	}
}

void
Discovery::cluster_hello([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_hello({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_HELLO [from {}]", remote_node.to_string());

	auto local_node = Node::get_local_node();

	if (!Node::is_superset(local_node, remote_node)) {
		auto put = Node::touch_node(remote_node, false);
		if (put.first == nullptr) {
			send_message(Message::CLUSTER_SNEER, remote_node.serialise());
			L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", remote_node.col().ansi(), remote_node.to_string(), remote_node.host(), remote_node.http_port, remote_node.remote_port, remote_node.replication_port);
		} else {
			send_message(Message::CLUSTER_WAVE, local_node->serialise());
			L_DEBUG("Touched node {}{}" + DEBUG_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", put.first->col().ansi(), put.first->to_string(), put.first->host(), put.first->http_port, put.first->remote_port, put.first->replication_port);
		}
	}
}

void
Discovery::cluster_wave([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_wave({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_WAVE [from {}]", remote_node.to_string());

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", remote_node.col().ansi(), remote_node.to_string(), remote_node.host(), remote_node.http_port, remote_node.remote_port, remote_node.replication_port);
	} else {
		L_DEBUG("Touched node {}{}" + DEBUG_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", put.first->col().ansi(), put.first->to_string(), put.first->host(), put.first->http_port, put.first->remote_port, put.first->replication_port);
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " is at the party! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", put.first->col().ansi(), put.first->to_string(), put.first->host(), put.first->http_port, put.first->remote_port, put.first->replication_port);
			// L_DIM_GREY("\n{}", Node::dump_nodes());
		}

		// After receiving WAVE, flag as WAITING_MORE so it waits just a little longer
		// (prevent it from switching to slow waiting)
		if (XapiandManager::exchange_state(XapiandManager::State::WAITING, XapiandManager::State::WAITING_MORE, 4s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
			// L_DEBUG("State changed: {} -> {}", enum_name(state), enum_name(XapiandManager::get_state()));
		}
	}
}

void
Discovery::cluster_sneer([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_sneer({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::RESET:
		case XapiandManager::State::WAITING:
		case XapiandManager::State::WAITING_MORE:
		case XapiandManager::State::JOINING:
			break;
		default:
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_SNEER [from {}]", remote_node.to_string());

	auto local_node = Node::get_local_node();
	if (remote_node == *local_node) {
		if (XapiandManager::manager(true)->node_name.empty()) {
			L_DISCOVERY("Node name {} already taken. Retrying other name...", local_node->name());
			if (XapiandManager::exchange_state(XapiandManager::get_state(), XapiandManager::State::RESET, 4s, "Node resetting is taking too long...", "Node reset done!")) {
				Node::reset();
				start();
			}
		} else {
			XapiandManager::set_state(XapiandManager::State::BAD);
			Node::set_local_node(std::make_shared<const Node>());
			L_CRIT("Cannot join the party. Node name {} already taken!", local_node->name());
			sig_exit(-EX_SOFTWARE);
		}
	}
}

void
Discovery::cluster_enter([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_enter({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_ENTER [from {}]", remote_node.to_string());

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", remote_node.col().ansi(), remote_node.to_string(), remote_node.host(), remote_node.http_port, remote_node.remote_port, remote_node.replication_port);
	} else {
		L_DEBUG("Touched node {}{}" + DEBUG_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", put.first->col().ansi(), put.first->to_string(), put.first->host(), put.first->http_port, put.first->remote_port, put.first->replication_port);
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " joined the party! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", put.first->col().ansi(), put.first->to_string(), put.first->host(), put.first->http_port, put.first->remote_port, put.first->replication_port);
			// L_DIM_GREY("\n{}", Node::dump_nodes());
		}
	}
}

void
Discovery::cluster_bye([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_bye({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::JOINING:
		case XapiandManager::State::SETUP:
		case XapiandManager::State::READY:
			break;
		default:
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_BYE [from {}]", remote_node.to_string());

	Node::drop_node(remote_node.name());

	if (raft_.role() == cluster::RaftRole::LEADER) {
		// If we're leader, check quorum or vote.
		auto total_nodes = Node::total_nodes();
		auto alive_nodes = Node::alive_nodes();
		if (!Node::quorum(total_nodes, alive_nodes)) {
			L_RAFT("Vote again! (no quorum, CLUSTER_BYE) {{ total_nodes:{}, alive_nodes:{} }}",
				total_nodes, alive_nodes);
			raft_.request_vote();
		}
	}

	auto leader_node = Node::get_leader_node();
	if (*leader_node == remote_node) {
		L_INFO("Leader node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), remote_node.to_string());

		raft_.request_vote();
	} else {
		L_INFO("Node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), remote_node.to_string());
	}

	L_DEBUG("Nodes still active after {} left: {}", remote_node.to_string(), Node::alive_nodes());
}

void
Discovery::db_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::db_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> DB_UPDATED (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);

	auto local_node = Node::get_local_node();
	if (Node::is_superset(local_node, remote_node)) {
		// It's just me, do nothing!
		return;
	}

	unserialise_length(&p, p_end);  // revision ignored

	auto path = std::string_view(p, p_end - p);
	L_DISCOVERY(">>> DB_UPDATED [from {}]: {}", remote_node.to_string(), repr(path));

	auto node = Node::touch_node(remote_node, false).first;
	if (node) {
		Endpoint local_endpoint(path);
		if (local_endpoint.empty()) {
			L_WARNING("Ignoring update for empty index: {}!", repr(path));
		} else {
			// Replicate database from the other node
			try {
				auto index_settings = XapiandManager::resolve_index_settings(local_endpoint.path, true);
				if (index_settings.shards.size() == 1) {
					const auto& shard_nodes = index_settings.shards[0].nodes;
					if (!shard_nodes.empty()) {
						node = Node::get_node(shard_nodes[0]);
						if (node) {
							Endpoint remote_endpoint(path, node);
							if (local_endpoint != remote_endpoint) {
								trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), local_endpoint.path, remote_endpoint, local_endpoint);
							}
						} else {
							L_WARNING("Ignoring update from unexistent node {}: {}!", repr(shard_nodes[0]), repr(path));
						}
					} else {
						L_WARNING("Ignoring update for misconfigured index: {}!", repr(path));
					}
				} else {
					L_WARNING("Ignoring update for unknown index: {}!", repr(path));
				}
			} catch (const Xapian::DatabaseNotAvailableError&) {
			} catch (const MissingTypeError&) { }
		}
	}
}

void
Discovery::schema_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::schema_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> SCHEMA_UPDATED (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);

	auto local_node = Node::get_local_node();
	if (Node::is_superset(local_node, remote_node)) {
		// It's just me, do nothing!
		return;
	}

	Xapian::rev version = unserialise_length(&p, p_end);

	auto uri = std::string(p, p_end - p);

	auto manager = XapiandManager::manager();
	if (manager) {
		manager->schemas->updated(uri, version);
	}
}

void
Discovery::index_settings_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::index_settings_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> INDEX_SETTINGS_UPDATED (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto local_node = Node::get_local_node();
	if (Node::is_superset(local_node, remote_node)) {
		// It's just me, do nothing!
		return;
	}

	auto uri = std::string(p, p_end - p);

	XapiandManager::invalidate_settings(uri);
}

void
Discovery::_ASYNC_primary_updated(const std::string& message)
{
	L_CALL("Discovery::_ASYNC_primary_updated(<message>) {{ state:{} }}", enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> PRIMARY_UPDATED (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);

	auto local_node = Node::get_local_node();
	if (Node::is_superset(local_node, remote_node)) {
		// It's just me, do nothing!
		return;
	}

	size_t shards = unserialise_length(&p, p_end);
	auto normalized_path = std::string_view(p, p_end - p);

	if (shards > 1) {
		for (size_t shard_num = 0; shard_num < shards; ++shard_num) {
			auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, ++shard_num);
			XapiandManager::resolve_index_settings(shard_normalized_path, false, false, nullptr, nullptr, false, false, true);
		}
	}

	XapiandManager::resolve_index_settings(normalized_path, false, false, nullptr, nullptr, false, false, true);
}

void
Discovery::_ASYNC_elect_primary(const std::string& message)
{
	L_CALL("Discovery::_ASYNC_elect_primary(<message>) {{ state:{} }}", enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> ELECT_PRIMARY (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();
	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT_PROTO(">>> ELECT_PRIMARY [from {}] (nonexistent node)",
			remote_node.to_string());
		return;
	}

	auto normalized_path = unserialise_string(&p, p_end);

	L_RAFT_PROTO(">>> ELECT_PRIMARY_RESPONSE [from {}]: {{ path:{} }}",
		node->to_string(), repr(normalized_path));

	std::string uuid;
	Xapian::rev revision;
	size_t total_nodes;

	auto index_settings = XapiandManager::resolve_index_settings(normalized_path);
	assert(index_settings.shards.size() == 1);
	if (index_settings.shards.size() == 1) {
		const auto& shard_nodes = index_settings.shards[0].nodes;
		total_nodes = shard_nodes.size();
		auto local_node = Node::get_local_node();
		for (const auto& shard_node_name : shard_nodes) {
			if (local_node->lower_name() == strings::lower(shard_node_name)) {
				try {
					lock_shard lk_shard(Endpoint{normalized_path}, DB_OPEN | DB_WRITABLE, false);
					auto shard = lk_shard.lock(0);
					auto db = shard->db();
					uuid = db->get_uuid();
					revision = db->get_revision();
				} catch (...) { }
				break;
			}
		}
	}

	if (!uuid.empty()) {
		std::string response;
		response.append(serialise_string(normalized_path));
		response.append(serialise_string(uuid));
		response.append(serialise_length(revision));
		response.append(serialise_bool(raft_.eligible()));
		response.append(serialise_length(total_nodes));
		message_send_args.enqueue(std::make_pair(Message::ELECT_PRIMARY_RESPONSE, response));
		message_send_signal_.send();
	}
}

void
Discovery::_ASYNC_elect_primary_response(const std::string& message)
{
	L_CALL("Discovery::_ASYNC_elect_primary_response(<message>) {{ state:{} }}", enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::READY:
			break;
		default:
			L_RAFT_PROTO(">>> ELECT_PRIMARY_RESPONSE (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::get_state()), raft_.term());
			return;
	}

	if (raft_.role() != cluster::RaftRole::LEADER) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();
	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT_PROTO(">>> ELECT_PRIMARY_RESPONSE [from {}] (nonexistent node)",
			remote_node.to_string());
		return;
	}

	auto normalized_path = std::string(unserialise_string(&p, p_end));
	auto uuid = unserialise_string(&p, p_end);
	Xapian::rev revision = unserialise_length(&p, p_end);
	auto eligible = unserialise_bool(&p, p_end);
	size_t total_nodes = unserialise_length(&p, p_end);

	L_RAFT_PROTO(">>> ELECT_PRIMARY_RESPONSE [from {}]: {{ path:{}, uuid:{}, revision:{}, total_nodes:{} }}",
		node->to_string(), repr(normalized_path), repr(uuid), revision, total_nodes);

	auto& voters = _ASYNC_elected_primaries[normalized_path];
	auto emplaced = voters.emplace(node->lower_name(), PrimaryShardVoter{});
	if (emplaced.second) {
		emplaced.first->second.uuid = uuid;
		emplaced.first->second.revision = revision;
		emplaced.first->second.eligible = eligible;
		if (Node::quorum(total_nodes, voters.size())) {
			auto nodes = IndexResolverLRU::resolve_nodes(XapiandManager::resolve_index_settings(normalized_path));
			assert(nodes.size() == 1);
			if (nodes.size() == 1) {
				Xapian::rev max_revision = 0;
				std::shared_ptr<const Node> elected_node;
				const auto& shards = nodes[0];
				total_nodes = shards.size();
				size_t ok_nodes = 0;
				assert(total_nodes);
				if (shards[0]->is_active()) {
					// Shard's primary is active, abort election!
					_ASYNC_elected_primaries.erase(normalized_path);
				} else {
					for (const auto& shard_node : shards) {
						if (shard_node->is_active()) {
							auto it = voters.find(shard_node->lower_name());
							if (it != voters.end()) {
								++ok_nodes;
								if (it->second.eligible) {
									if (!elected_node || (it->second.uuid == uuid && it->second.revision > max_revision)) {
										max_revision = it->second.revision;
										elected_node = shard_node;
									}
								}
							}
						}
					}
					if (Node::quorum(total_nodes, ok_nodes)) {
						_ASYNC_elected_primaries.erase(normalized_path);
						if (elected_node) {
							L_RAFT("Elected primary node for shard {} is {}", repr(normalized_path), elected_node->to_string());
							XapiandManager::resolve_index_settings(normalized_path, true, true, nullptr, elected_node);
						}
					}
				}
			}
		}
	}
}

void
Discovery::_ASYNC_elect_primary_send(const std::string& normalized_path)
{
	L_CALL("Discovery::elect_primary()");

	_ASYNC_elected_primaries.erase(normalized_path);

	std::string message;
	message.append(serialise_string(normalized_path));
	message_send_args.enqueue(std::make_pair(Message::ELECT_PRIMARY, message));
	message_send_signal_.send();
}

void
Discovery::cluster_discovery_cb()
{
	L_CALL("Discovery::cluster_discovery_cb() {{ state:{} }}", enum_name(XapiandManager::get_state()));

	L_EV_BEGIN("Discovery::cluster_discovery_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::get_state()));
	L_EV_END("Discovery::cluster_discovery_cb:END {{ state:{} }}", enum_name(XapiandManager::get_state()));

	switch (XapiandManager::get_state()) {
		case XapiandManager::State::RESET: {
			auto local_node = Node::get_local_node();
			auto node_copy = std::make_unique<Node>(*local_node);
			auto drop_name = node_copy->name();
			auto manager = XapiandManager::manager(true);
			if (manager->node_name.empty()) {
				node_copy->name(name_generator());
			} else {
				node_copy->name(manager->node_name);
			}
			if (!drop_name.empty() && drop_name != node_copy->name()) {
				Node::drop_node(drop_name);
			}
			Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));
			local_node = Node::get_local_node();
			if (XapiandManager::exchange_state(XapiandManager::State::RESET, XapiandManager::State::WAITING, 4s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(XapiandManager::State::RESET), enum_name(XapiandManager::get_state()));
				L_INFO("Advertising as {}{}" + INFO_COL + "...", local_node->col().ansi(), local_node->name());
				send_message(Message::CLUSTER_HELLO, local_node->serialise());
			}
			break;
		}
		case XapiandManager::State::WAITING: {
			// We're here because no one sneered nor entered during
			// CLUSTER_DISCOVERY_WAITING_FAST, wait longer then...

			cluster_discovery_.set_repeat(CLUSTER_DISCOVERY_WAITING_SLOW);
			cluster_discovery_.again();
			L_EV("Reset discovery's cluster_discovery event ({})", cluster_discovery_.repeat());

			if (XapiandManager::exchange_state(XapiandManager::State::WAITING, XapiandManager::State::WAITING_MORE, 4s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(XapiandManager::State::WAITING), enum_name(XapiandManager::get_state()));
			}
			break;
		}
		case XapiandManager::State::WAITING_MORE: {
			cluster_discovery_.stop();
			L_EV("Stop discovery's cluster_discovery event");

			if (XapiandManager::exchange_state(XapiandManager::State::WAITING_MORE, XapiandManager::State::JOINING, 4s, "Joining cluster is taking too long...", "Joining cluster is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(XapiandManager::State::WAITING_MORE), enum_name(XapiandManager::get_state()));
				L_INFO("Joining cluster {}...", repr(opts.cluster_name));
				raft_.request_vote();
			}
			break;
		}
		default: {
			break;
		}
	}
}

void
Discovery::raft_request_vote()
{
	L_CALL("Discovery::raft_request_vote()");

	// Step down to follower + reset the election timeout (classic raft_request_vote ->
	// _raft_request_vote(false)); thread-safe (posts onto the bus loop).
	raft_.request_vote();
}


void
Discovery::raft_relinquish_leadership()
{
	L_CALL("Discovery::raft_relinquish_leadership()");

	// Go ineligible + hand off leadership immediately if leader/candidate (classic
	// raft_relinquish_leadership); thread-safe (posts onto the bus loop).
	raft_.relinquish_leadership();
}


void
Discovery::raft_add_command(const std::string& command)
{
	L_CALL("Discovery::raft_add_command({})", repr(command));

	// Append a command to the replicated log (forwarded to the leader if we are not it);
	// thread-safe (posts onto the bus loop).
	raft_.add_command(command);
}

void
Discovery::_message_send(Message type, const std::string& message)
{
	auto local_node = Node::get_local_node();
	send_message(type,
		local_node->serialise() +   // The node where the index is at
		message);

	L_DEBUG("Sending {} message: {}", enum_name(type), repr(message));
}

void
Discovery::cluster_enter_signal_cb()
{
	L_CALL("Discovery::cluster_enter_signal_cb()");

	L_EV_BEGIN("Discovery::cluster_enter_signal_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::get_state()));
	L_EV_END("Discovery::cluster_enter_signal_cb:END {{ state:{} }}", enum_name(XapiandManager::get_state()));

	auto local_node = Node::get_local_node();

	if (raft_.role() == cluster::RaftRole::LEADER) {
		// If we're leader, check quorum or vote.
		auto total_nodes = Node::total_nodes();
		auto alive_nodes = Node::alive_nodes();
		if (!Node::quorum(total_nodes, alive_nodes)) {
			L_RAFT("Vote again! (no quorum, CLUSTER_ENTER) {{ total_nodes:{}, alive_nodes:{} }}",
				total_nodes, alive_nodes);
			raft_.request_vote();
		}
	}

	send_message(Message::CLUSTER_ENTER, local_node->serialise());
}

void
Discovery::cluster_enter()
{
	L_CALL("Discovery::cluster_enter()");

	cluster_enter_signal_.send();
}

void
Discovery::message_send_signal_cb()
{
	L_CALL("Discovery::message_send_signal_cb()");

	L_EV_BEGIN("Discovery::message_send_signal_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::get_state()));
	L_EV_END("Discovery::message_send_signal_cb:END {{ state:{} }}", enum_name(XapiandManager::get_state()));

	std::pair<Message, std::string> message;
	while (message_send_args.try_dequeue(message)) {
		_message_send(message.first, message.second);
	}
}

void
Discovery::db_updated_send(Xapian::rev revision, std::string_view path)
{
	L_CALL("Discovery::db_updated_send({}, {})", revision, repr(path));

	auto message = serialise_length(revision);
	message.append(path);

	message_send_args.enqueue(std::make_pair(Message::DB_UPDATED, message));

	message_send_signal_.send();
}

void
Discovery::schema_updated_send(Xapian::rev revision, std::string_view path)
{
	L_CALL("Discovery::schema_updated_send({}, {})", revision, repr(path));

	auto message = serialise_length(revision);
	message.append(path);

	message_send_args.enqueue(std::make_pair(Message::SCHEMA_UPDATED, message));

	message_send_signal_.send();
}

void
Discovery::settings_updated_send([[maybe_unused]] Xapian::rev revision, std::string_view path)
{
	L_CALL("Discovery::settings_updated_send({}, {})", revision, repr(path));

	auto message = std::string(path);

	message_send_args.enqueue(std::make_pair(Message::INDEX_SETTINGS_UPDATED, message));

	message_send_signal_.send();
}

void
Discovery::primary_updated_send(size_t shards, std::string_view path)
{
	L_CALL("Discovery::primary_updated_send({}, {})", shards, repr(path));

	auto message = serialise_length(shards);
	message.append(path);

	message_send_args.enqueue(std::make_pair(Message::PRIMARY_UPDATED, message));

	message_send_signal_.send();
}

std::string
Discovery::__repr__() const
{
	const char* role =
		raft_.role() == cluster::RaftRole::LEADER ? "RAFT_LEADER" :
		raft_.role() == cluster::RaftRole::CANDIDATE ? "RAFT_CANDIDATE" : "RAFT_FOLLOWER";
	return strings::format(STEEL_BLUE + "<Discovery ({}) {{ term:{} }}>", role, raft_.term());
}


std::string
Discovery::getDescription() const
{
	L_CALL("Discovery::getDescription()");

	return strings::format("UDP {}:{} (Discovery v{}.{})", group_, port_, XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION, XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION);
}

void
db_updated_send(Xapian::rev revision, std::string path)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->discovery->db_updated_send(revision, path);
	}
}

void
schema_updated_send(Xapian::rev revision, std::string path)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->discovery->schema_updated_send(revision, path);
	}
}

void
settings_updated_send(Xapian::rev revision, std::string path)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->discovery->settings_updated_send(revision, path);
	}
}

void
primary_updated_send(size_t shards, std::string path)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->discovery->primary_updated_send(shards, path);
	}
}

#endif
