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

#include "discovery.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                          // for assert
#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "color_tools.hh"                   // for color
#include "cuuid/uuid.h"                     // for UUID
#include "database/schemas_lru.h"           // for SchemasLRU
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "exception_xapian.h"               // for InvalidArgumentError
#include "manager.h"                        // for XapiandManager, XapiandManager::State
#include "namegen.h"                        // for name_generator
#include "node.h"                           // for Node, local_node
#include "opts.h"                           // for opts::*
#include "random.hh"                        // for random_int
#include "readable_revents.hh"              // for readable_revents
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


// Values in seconds
constexpr double CLUSTER_DISCOVERY_WAITING_FAST   = 0.200;
constexpr double CLUSTER_DISCOVERY_WAITING_SLOW   = 0.600;

constexpr double RAFT_HEARTBEAT_LEADER_TIMEOUT     = 0.300;

constexpr double RAFT_LEADER_ELECTION_MIN = 2.5 * RAFT_HEARTBEAT_LEADER_TIMEOUT;
constexpr double RAFT_LEADER_ELECTION_MAX = 5.0 * RAFT_HEARTBEAT_LEADER_TIMEOUT;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;


static inline bool raft_has_consensus(size_t total, size_t votes) {
	return !total || votes > total / 2;
}


Discovery::Discovery(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv)
	: UDP("Discovery", XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION, XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION, UDP_SO_REUSEPORT | UDP_IP_MULTICAST_LOOP | UDP_IP_MULTICAST_TTL | UDP_IP_ADD_MEMBERSHIP),
	  Worker(parent_, ev_loop_, ev_flags_),
	  io(*ev_loop),
	  cluster_discovery(*ev_loop),
	  cluster_enter_async(*ev_loop),
	  raft_leader_election_timeout(*ev_loop),
	  raft_leader_heartbeat(*ev_loop),
	  raft_request_vote_async(*ev_loop),
	  raft_add_command_async(*ev_loop),
	  message_send_async(*ev_loop),
	  raft_role(Role::RAFT_FOLLOWER),
	  raft_votes_granted(0),
	  raft_votes_denied(0),
	  raft_current_term(0),
	  raft_commit_index(0),
	  raft_last_applied(0)
{
	bind(hostname, serv, 1);
	io.set<Discovery, &Discovery::io_accept_cb>(this);
	cluster_discovery.set<Discovery, &Discovery::cluster_discovery_cb>(this);

	cluster_enter_async.set<Discovery, &Discovery::cluster_enter_async_cb>(this);
	cluster_enter_async.start();
	L_EV("Start discovery's async cluster_enter signal event");

	raft_leader_election_timeout.set<Discovery, &Discovery::raft_leader_election_timeout_cb>(this);
	raft_leader_heartbeat.set<Discovery, &Discovery::raft_leader_heartbeat_cb>(this);

	raft_request_vote_async.set<Discovery, &Discovery::raft_request_vote_async_cb>(this);
	raft_request_vote_async.start();
	L_EV("Start raft's async raft_request_vote signal event");

	raft_add_command_async.set<Discovery, &Discovery::raft_add_command_async_cb>(this);
	raft_add_command_async.start();
	L_EV("Start discovery's async raft_add_command signal event");

	message_send_async.set<Discovery, &Discovery::message_send_async_cb>(this);
	message_send_async.start();
	L_EV("Start discovery's async db_updated_send signal event");
}


Discovery::~Discovery() noexcept
{
	try {
		Worker::deinit();

		UDP::close();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
Discovery::shutdown_impl(long long asap, long long now)
{
	L_CALL("Discovery::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		if (is_runner()) {
			break_loop(false);
		} else {
			detach(false);
		}
	}
}


void
Discovery::destroy_impl()
{
	L_CALL("Discovery::destroy_impl()");

	Worker::destroy_impl();

	UDP::close();
}


void
Discovery::start_impl()
{
	L_CALL("Discovery::start_impl()");

	Worker::start_impl();

	cluster_discovery.start(0, CLUSTER_DISCOVERY_WAITING_FAST);
	L_EV("Start discovery's cluster_discovery exploring event ({})", cluster_discovery.repeat);

	io.start(sock, ev::READ);
	L_EV("Start discovery's server accept event {{ sock:{} }}", sock);

	L_DISCOVERY("Discovery was started! (exploring)");
}


void
Discovery::stop_impl()
{
	L_CALL("Discovery::stop_impl()");

	Worker::stop_impl();

	auto local_node = Node::get_local_node();
	send_message(Message::CLUSTER_BYE, local_node->serialise());
	L_INFO("Waving goodbye to cluster {}!", opts.cluster_name);

	raft_leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	raft_leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	cluster_discovery.stop();
	L_EV("Stop discovery's cluster_discovery event");

	io.stop();
	L_EV("Stop discovery's server accept event");

	L_DISCOVERY("Discovery was stopped!");
}


void
Discovery::operator()()
{
	L_CALL("Discovery::operator()()");

	L_EV("Starting cluster_discovery server loop...");
	run_loop();
	L_EV("Discovery server loop ended!");

	detach(false);
}


void
Discovery::send_message(Message type, const std::string& message)
{
	L_CALL("Discovery::send_message({}, <message>)", enum_name(type));

	L_DISCOVERY_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));
	UDP::send_message(toUType(type), message);
}


void
Discovery::io_accept_cb([[maybe_unused]] ev::io &watcher, int revents)
{
	L_CALL("Discovery::io_accept_cb(<watcher>, {:#x} ({})) {{ sock:{} }}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("Discovery::io_accept_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::state()));
	L_EV_END("Discovery::io_accept_cb:END {{ state:{} }}", enum_name(XapiandManager::state()));

	assert(sock == -1 || sock == watcher.fd);

	if (closed) {
		return;
	}

	L_DEBUG_HOOK("Discovery::io_accept_cb", "Discovery::io_accept_cb(<watcher>, {:#x} ({})) {{ sock:{} }}", revents, readable_revents(revents), watcher.fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid cluster_discovery event {{ sock:{} }}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	if (revents & EV_READ) {
		while (true) {
			try {
				std::string message;
				auto raw_type = get_message(message, static_cast<char>(Message::MAX));
				if (raw_type == '\xff') {
					break;  // no message
				}
				Message type = static_cast<Message>(raw_type);
				L_DISCOVERY_PROTO(">>> get_message ({}): {}", enum_name(type), repr(message));
				discovery_server(type, message);
			} catch (...) {
				L_EXC("ERROR: Unhandled exception in discovery_server");
				break;
			}
		}
	}
}


void
Discovery::discovery_server(Message type, const std::string& message)
{
	L_CALL("Discovery::discovery_server({}, <message>)", enum_name(type));

	L_EV_BEGIN("Discovery::discovery_server:BEGIN {{ state:{}, type:{} }}", enum_name(XapiandManager::state()), enum_name(type));
	L_EV_END("Discovery::discovery_server:END {{ state:{}, type:{} }}", enum_name(XapiandManager::state()), enum_name(type));

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
		case Message::RAFT_APPEND_ENTRIES:
			raft_append_entries(type, message);
			return;
		case Message::RAFT_HEARTBEAT_RESPONSE:
		case Message::RAFT_APPEND_ENTRIES_RESPONSE:
			raft_append_entries_response(type, message);
			return;
		case Message::RAFT_REQUEST_VOTE:
			raft_request_vote(type, message);
			return;
		case Message::RAFT_REQUEST_VOTE_RESPONSE:
			raft_request_vote_response(type, message);
			return;
		case Message::RAFT_ADD_COMMAND:
			raft_add_command(type, message);
			return;
		case Message::DB_UPDATED:
			db_updated(type, message);
			return;
		case Message::SCHEMA_UPDATED:
			schema_updated(type, message);
			return;
		case Message::PRIMARY_UPDATED:
			primary_updated(type, message);
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
	L_CALL("Discovery::cluster_hello({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_HELLO [from {}]", repr(remote_node.to_string()));

	auto local_node = Node::get_local_node();

	if (!Node::is_superset(local_node, remote_node)) {
		auto put = Node::touch_node(remote_node, false, false);
		if (put.first == nullptr) {
			send_message(Message::CLUSTER_SNEER, remote_node.serialise());
		} else {
			send_message(Message::CLUSTER_WAVE, local_node->serialise());
		}
	}
}


void
Discovery::cluster_wave([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_wave({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_WAVE [from {}]", repr(remote_node.to_string()));

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", remote_node.col().ansi(), repr(remote_node.to_string()), remote_node.host(), remote_node.http_port, remote_node.remote_port, remote_node.replication_port);
	} else {
		auto node = put.first;
		L_DEBUG("Added node {}{}" + DEBUG_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " is at the party! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
			// L_DIM_GREY("\n{}", Node::dump_nodes());
		}

		// After receiving WAVE, flag as WAITING_MORE so it waits just a little longer
		// (prevent it from switching to slow waiting)
		if (XapiandManager::exchange_state(XapiandManager::State::WAITING, XapiandManager::State::WAITING_MORE, 3s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
			// L_DEBUG("State changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
		}
	}
}


void
Discovery::cluster_sneer([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_sneer({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::RESET &&
		XapiandManager::state() != XapiandManager::State::WAITING &&
		XapiandManager::state() != XapiandManager::State::WAITING_MORE &&
		XapiandManager::state() != XapiandManager::State::JOINING) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_SNEER [from {}]", repr(remote_node.to_string()));

	auto local_node = Node::get_local_node();
	if (remote_node == *local_node) {
		if (XapiandManager::node_name().empty()) {
			L_DISCOVERY("Node name {} already taken. Retrying other name...", local_node->name());
			XapiandManager::reset_state();
		} else {
			XapiandManager::state().store(XapiandManager::State::BAD);
			Node::set_local_node(std::make_shared<const Node>());
			L_CRIT("Cannot join the party. Node name {} already taken!", local_node->name());
			sig_exit(-EX_SOFTWARE);
		}
	}
}


void
Discovery::cluster_enter([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_enter({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_ENTER [from {}]", repr(remote_node.to_string()));

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", remote_node.col().ansi(), repr(remote_node.to_string()), remote_node.host(), remote_node.http_port, remote_node.remote_port, remote_node.replication_port);
	} else {
		auto node = put.first;
		L_DEBUG("Added node {}{}" + DEBUG_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " joined the party! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
			// L_DIM_GREY("\n{}", Node::dump_nodes());
		}
	}
}


void
Discovery::cluster_bye([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_bye({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">>> CLUSTER_BYE [from {}]", repr(remote_node.to_string()));

	Node::drop_node(remote_node.name());

	if (raft_role == Role::RAFT_LEADER) {
		// If we're leader, check consensus or vote.
		auto total_nodes = Node::total_nodes();
		auto alive_nodes = Node::alive_nodes();
		if (!raft_has_consensus(total_nodes, alive_nodes)) {
			L_RAFT("Vote again! (no consensus) {{ total_nodes:{}, alive_nodes:{} }}",
				total_nodes, alive_nodes);
			_raft_request_vote(false);
		}
	}

	auto leader_node = Node::get_leader_node();
	if (*leader_node == remote_node) {
		L_INFO("Leader node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), repr(remote_node.to_string()));

		Node::set_leader_node(std::make_shared<const Node>());
		XapiandManager::renew_leader();
	} else {
		L_INFO("Node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), repr(remote_node.to_string()));
	}

	L_DEBUG("Nodes still active after {} left: {}", repr(remote_node.to_string()), Node::alive_nodes());
}


void
Discovery::raft_request_vote([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::raft_request_vote({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE (invalid state: {}) {{ current_term:{} }}",
			enum_name(XapiandManager::state().load()), raft_current_term);
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE [from {}] (nonexistent node) {{ current_term:{} }}",
			repr(remote_node.to_string()), raft_current_term);
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > raft_current_term) {
		// set currentTerm = T,
		raft_current_term = term;
		// convert to follower
		raft_role = Role::RAFT_FOLLOWER;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));
	}

	L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE [from {}]{} {{ term:{} }}",
		repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);

	auto local_node = Node::get_local_node();

	if (term == raft_current_term) {
		if (raft_voted_for.empty()) {
			if (Node::is_local(node)) {
				raft_voted_for = *node;
				if (raft_voters.insert(local_node->name()).second) {
					++raft_votes_granted;
				}
				L_RAFT("I vote {} for term {} (me)", repr(raft_voted_for.to_string()), term);
			} else if (raft_role == Role::RAFT_FOLLOWER) {
				uint64_t remote_last_log_term = unserialise_length(&p, p_end);
				size_t remote_last_log_index = unserialise_length(&p, p_end);
				// §5.4.1
				auto last_log_index = raft_log.size();
				auto last_log_term = last_log_index > 0 ? raft_log[last_log_index - 1].term : 0;
				if (last_log_term < remote_last_log_term) {
					// If the logs have last entries with different terms, then the
					// raft_log with the later term is more up-to-date.
					raft_voted_for = *node;
					if (raft_voters.insert(local_node->name()).second) {
						++raft_votes_denied;
					}
					L_RAFT("I vote {} for term {} (raft_log term is newer)", repr(raft_voted_for.to_string()), term);
				} else if (last_log_term == remote_last_log_term) {
					// If the logs end with the same term, then whichever
					// raft_log is longer is more up-to-date.
					if (raft_log.size() <= remote_last_log_index) {
						raft_voted_for = *node;
						if (raft_voters.insert(local_node->name()).second) {
							++raft_votes_denied;
						}
						L_RAFT("I vote {} for term {} (raft_log index size concurs)", repr(raft_voted_for.to_string()), term);
					} else {
						L_RAFT("I don't vote {} for term {} (raft_log index is shorter)", repr(node->to_string()), term);
					}
				} else {
					L_RAFT("I don't vote {} for term {} (raft_log term is older)", repr(node->to_string()), term);
				}
			}
		} else {
			L_RAFT("I already voted {} for term {}", repr(raft_voted_for.to_string()), term);
		}
	}

	// L_DIM_GREY("\n{}", Node::dump_nodes());
	auto total_nodes = Node::total_nodes();
	L_RAFT_PROTO("<<< RAFT_REQUEST_VOTE_RESPONSE {{ node:{}, term:{}, total_nodes:{}, voted_for:{} }}",
		repr(local_node->to_string()), term, total_nodes, repr(raft_voted_for.to_string()));

	send_message(Message::RAFT_REQUEST_VOTE_RESPONSE,
		local_node->serialise() +
		serialise_length(term) +
		serialise_length(total_nodes) +
		raft_voted_for.serialise());
}


void
Discovery::raft_request_vote_response([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::raft_request_vote_response({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE_RESPONSE (invalid state: {}) {{ current_term:{} }}",
			enum_name(XapiandManager::state().load()), raft_current_term);
		return;
	}

	if (raft_role != Role::RAFT_CANDIDATE) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE_RESPONSE [from {}] (nonexistent node) {{ current_term:{} }}",
			repr(remote_node.to_string()), raft_current_term);
		return;
	}

	auto local_node = Node::get_local_node();

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > raft_current_term) {
		// set currentTerm = T,
		raft_current_term = term;
		// convert to follower
		raft_role = Role::RAFT_FOLLOWER;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));
	}

	L_RAFT_PROTO(">>> RAFT_REQUEST_VOTE_RESPONSE [from {}]{} {{ term:{} }}",
		repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);

	if (term == raft_current_term) {
		size_t total_nodes = unserialise_length(&p, p_end);
		total_nodes = std::max(total_nodes, Node::total_nodes());

		if (raft_voters.insert(node->name()).second) {
			auto voted_for_node = Node::touch_node(Node::unserialise(&p, p_end), false).first;
			if (voted_for_node) {
				L_RAFT("Node {} just casted a secret vote for {}", repr(node->to_string()), repr(voted_for_node->to_string()));
				if (Node::is_superset(local_node, voted_for_node)) {
					++raft_votes_granted;
				} else {
					++raft_votes_denied;
				}
			} else {
				++raft_votes_denied;
			}
		}
		if (raft_has_consensus(total_nodes, raft_votes_granted + raft_votes_denied)) {
			L_RAFT("Consensus reached: {} votes granted and {} denied, out of {}", raft_votes_granted, raft_votes_denied, total_nodes);
			if (raft_votes_granted > raft_votes_denied) {
				raft_role = Role::RAFT_LEADER;
				raft_voted_for.clear();
				raft_next_indexes.clear();
				raft_match_indexes.clear();

				_raft_leader_heartbeat_reset(RAFT_HEARTBEAT_LEADER_TIMEOUT);
				_raft_set_leader_node(local_node);

				auto entry_index = raft_log.size();
				auto prev_log_index = entry_index - 1;
				auto prev_log_term = entry_index > 1 ? raft_log[prev_log_index - 1].term : 0;

				L_RAFT_PROTO("<<< RAFT_APPEND_ENTRIES {{ node:{}, raft_current_term:{}, prev_log_index:{}, prev_log_term:{}, raft_commit_index:{} }}",
					repr(local_node->to_string()), raft_current_term, prev_log_index, prev_log_term, raft_commit_index);
				send_message(Message::RAFT_APPEND_ENTRIES,
					local_node->serialise() +
					serialise_length(raft_current_term) +
					serialise_length(prev_log_index) +
					serialise_length(prev_log_term) +
					serialise_length(raft_commit_index));

				// First time we elect a leader's, we setup node
				if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP, 3s, "Node setup is taking too long...", "Node setup is finally done!")) {
					// L_DEBUG("Role changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
					XapiandManager::setup_node();
				}
			}
		} else {
			L_RAFT("No consensus reached: {} votes granted and {} denied, out of {}", raft_votes_granted, raft_votes_denied, total_nodes);
		}
	}
}


void
Discovery::raft_append_entries(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_append_entries({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		if (type == Message::RAFT_HEARTBEAT) {
			L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::state().load()), raft_current_term);
		} else {
			L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::state().load()), raft_current_term);
		}
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		if (type == Message::RAFT_HEARTBEAT) {
			L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT [from {}] (nonexistent node) {{ current_term:{} }}",
				repr(remote_node.to_string()), raft_current_term);
		} else {
			L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES [from {}] (nonexistent node) {{ current_term:{} }}",
				repr(remote_node.to_string()), raft_current_term);
		}
		return;
	}

	auto local_node = Node::get_local_node();

	uint64_t term = unserialise_length(&p, p_end);

	if (raft_role == Role::RAFT_LEADER) {
		if (!Node::is_superset(local_node, node)) {
			L_RAFT_PROTO(">>> {} [from {}]{} {{ term:{} }}",
				enum_name(type), repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);
			// If another leader is around or there is no way to reach consnsus,
			// immediately run for election.
			_raft_request_vote(true);
		}
		return;
	}

	if (term < raft_current_term) {
		L_RAFT_PROTO(">>> {} [from {}] (received older term) {{ term:{} }}",
			enum_name(type), repr(node->to_string()), term);
		// If term from heartbeat is older,
		// immediately run for election.
		_raft_request_vote(true);
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	if (term > raft_current_term) {
		// set currentTerm = T,
		raft_current_term = term;
		// convert to follower
		raft_role = Role::RAFT_FOLLOWER;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		// _raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));  // resetted below!
	}

	if (type == Message::RAFT_HEARTBEAT) {
		L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT [from {}]{} {{ term:{} }}",
			repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);
	} else {
		L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES [from {}]{} {{ term:{} }}",
			repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);
	}

	size_t next_index;
	size_t match_index;
	bool success = false;

	if (term == raft_current_term) {
		size_t prev_log_index = unserialise_length(&p, p_end);
		uint64_t prev_log_term = unserialise_length(&p, p_end);

		if (raft_role == Role::RAFT_CANDIDATE) {
			// If AppendEntries RPC received from new leader:
			// convert to follower
			raft_role = Role::RAFT_FOLLOWER;
			raft_voted_for.clear();
			raft_next_indexes.clear();
			raft_match_indexes.clear();
		}

		_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));
		_raft_set_leader_node(node);

		// Reply false if raft_log doesn’t contain an entry at
		// prevLogIndex whose term matches prevLogTerm
		auto last_index = raft_log.size();
		auto entry_index = prev_log_index + 1;
		if (type == Message::RAFT_HEARTBEAT) {
			L_RAFT_PROTO_HB("   {{ entry_index:{}, prev_log_index:{}, last_index:{}, prev_log_term:{} }}",
				entry_index, prev_log_index, last_index, prev_log_term);
		} else {
			L_RAFT_PROTO("   {{ entry_index:{}, prev_log_index:{}, last_index:{}, prev_log_term:{} }}",
				entry_index, prev_log_index, last_index, prev_log_term);
		}
		if (entry_index <= 1 || (prev_log_index <= last_index && raft_log[prev_log_index - 1].term == prev_log_term)) {
			size_t leader_commit = unserialise_length(&p, p_end);

			if (p != p_end) {
				size_t last_log_index = unserialise_length(&p, p_end);
				uint64_t entry_term = unserialise_length(&p, p_end);
				auto entry_command = unserialise_string(&p, p_end);
				if (type == Message::RAFT_HEARTBEAT) {
					L_RAFT_PROTO_HB("   {{ last_log_index:{}, entry_term:{}, entry_command:{} }}",
						last_log_index, entry_term, repr(entry_command));
				} else {
					L_RAFT_PROTO("   {{ last_log_index:{}, entry_term:{}, entry_command:{} }}",
						last_log_index, entry_term, repr(entry_command));
				}

				if (entry_index <= last_index) {
					if (entry_index > 1 && raft_log[prev_log_index - 1].term != entry_term) {
						// If an existing entry conflicts with a new one (same
						// index but different terms),
						// delete the existing entry and all that follow it
						// and append new entries
						raft_log.resize(entry_index);
						raft_log.push_back({
							entry_term,
							std::string(entry_command),
						});
						last_index = raft_log.size();
					} else if (entry_index == last_log_index) {
						// If a valid existing entry already exists
						// and it's the last one, just ignore the message
						return;
					}
				} else {
					// Append any new entries not already in the raft_log
					raft_log.push_back({
						entry_term,
						std::string(entry_command),
					});
					last_index = raft_log.size();
				}
#ifdef L_RAFT_LOG
				if (type != Message::RAFT_HEARTBEAT) {
					std::string log;
					for (size_t i = 0; i < raft_log.size(); ++i) {
						log += strings::format("\n    {} raft_log[{}] -> {{ term:{}, command:{} }}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
					}
					L_RAFT_LOG("Command received for raft_log: {}{}", repr(entry_command), log.empty() ? "\n    (empty)" : log);
				}
#endif
			}

			// If leaderCommit > commitIndex,
			// set commitIndex = min(leaderCommit, index of last new entry)
			if (leader_commit > raft_commit_index) {
				raft_commit_index = std::min(leader_commit, entry_index);
				if (raft_commit_index > raft_last_applied) {
					L_RAFT("Commit {{ raft_commit_index:{}, raft_last_applied:{}, last_index:{} }}",
						raft_commit_index, raft_last_applied, last_index);

					// If commitIndex > lastApplied:
					while (raft_commit_index > raft_last_applied && raft_last_applied < last_index) {
						// apply raft_log[lastApplied] to state machine
						const auto& command = raft_log[raft_last_applied].command;
						_raft_apply_command(command);
						// and increment lastApplied
						++raft_last_applied;
					}
				}
			}

			if (leader_commit == raft_commit_index) {
				// First time we reach leader's commit, we setup node
				if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP, 3s, "Node setup is taking too long...", "Node setup is finally done!")) {
					// L_DEBUG("Role changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
					XapiandManager::setup_node();
				}
			}

			next_index = last_index + 1;
			match_index = entry_index;
			success = true;
		}
	}

	Message response_type;
	if (type == Message::RAFT_HEARTBEAT) {
		response_type = Message::RAFT_HEARTBEAT_RESPONSE;
		L_RAFT_PROTO_HB("<<< RAFT_HEARTBEAT_RESPONSE {{ node:{}, term:{}, success:{}, next_index:{}, match_index:{} }}",
			repr(local_node->to_string()), term, success, next_index, match_index);
	} else {
		response_type = Message::RAFT_APPEND_ENTRIES_RESPONSE;
		L_RAFT_PROTO("<<< RAFT_APPEND_ENTRIES_RESPONSE {{ node:{}, term:{}, success:{}, next_index:{}, match_index:{} }}",
			repr(local_node->to_string()), term, success, next_index, match_index);
	}
	send_message(response_type,
		local_node->serialise() +
		serialise_length(term) +
		serialise_length(success) +
		(success
			? serialise_length(next_index) +
			  serialise_length(match_index)
			: ""
		));
}


void
Discovery::raft_append_entries_response([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::raft_append_entries_response({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		if (type == Message::RAFT_HEARTBEAT_RESPONSE) {
			L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT_RESPONSE (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::state().load()), raft_current_term);
		} else {
			L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES_RESPONSE (invalid state: {}) {{ current_term:{} }}",
				enum_name(XapiandManager::state().load()), raft_current_term);
		}
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		if (type == Message::RAFT_HEARTBEAT_RESPONSE) {
			L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT_RESPONSE [from {}] (nonexistent node) {{ current_term:{} }}",
				repr(remote_node.to_string()), raft_current_term);
		} else {
			L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES_RESPONSE [from {}] (nonexistent node) {{ current_term:{} }}",
				repr(remote_node.to_string()), raft_current_term);
		}
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > raft_current_term) {
		// set currentTerm = T,
		raft_current_term = term;
		// convert to follower
		raft_role = Role::RAFT_FOLLOWER;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));
	}

	if (type == Message::RAFT_HEARTBEAT_RESPONSE) {
		L_RAFT_PROTO_HB(">>> RAFT_HEARTBEAT_RESPONSE [from {}]{} {{ term:{} }}",
			repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);
	} else {
		L_RAFT_PROTO(">>> RAFT_APPEND_ENTRIES_RESPONSE [from {}]{} {{ term:{} }}",
			repr(node->to_string()), term == raft_current_term ? "" : " (wrong term)", term);
	}

	if (term == raft_current_term) {
		bool success = unserialise_length(&p, p_end);
		if (success) {
			// If successful:
			// update nextIndex and matchIndex for follower
			size_t next_index = unserialise_length(&p, p_end);
			size_t match_index = unserialise_length(&p, p_end);
			raft_next_indexes[node->lower_name()] = next_index;
			raft_match_indexes[node->lower_name()] = match_index;
			if (type == Message::RAFT_HEARTBEAT_RESPONSE) {
				L_RAFT_PROTO_HB("   {{ success:{}, next_index:{}, match_index:{} }}", success, next_index, match_index);
			} else {
				L_RAFT_PROTO("   {{ success:{}, next_index:{}, match_index:{} }}", success, next_index, match_index);
			}
		} else {
			// If AppendEntries fails because of raft_log inconsistency:
			// decrement nextIndex and retry
			auto it = raft_next_indexes.find(node->lower_name());
			auto& next_index = it == raft_next_indexes.end()
				? raft_next_indexes[node->lower_name()] = raft_log.size() + 2
				: it->second;
			if (next_index > 1) {
				--next_index;
			}
			if (type == Message::RAFT_HEARTBEAT_RESPONSE) {
				L_RAFT_PROTO_HB("   {{ success:{}, next_index:{} }}", success, next_index);
			} else {
				L_RAFT_PROTO("   {{ success:{}, next_index:{} }}", success, next_index);
			}
		}
		if (_raft_commit_log()) {
#ifdef L_RAFT_LOG
			std::string log;
			for (size_t i = 0; i < raft_log.size(); ++i) {
				log += strings::format("\n    {} raft_log[{}] -> {{ term:{}, command:{} }}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
			}
			L_RAFT_LOG("Commands after commit:{}", log.empty() ? "\n    (empty)" : log);
#endif
		}
	}
}


void
Discovery::raft_add_command([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::raft_add_command({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT_PROTO(">>> RAFT_ADD_COMMAND (invalid state: {}) {{ current_term:{} }}",
			enum_name(XapiandManager::state().load()), raft_current_term);
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT_PROTO(">>> RAFT_ADD_COMMAND [from {}] (nonexistent node) {{ current_term:{} }}",
			repr(remote_node.to_string()), raft_current_term);
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	auto command = std::string(unserialise_string(&p, p_end));
	raft_add_command(command);
}


void
Discovery::db_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::db_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::READY) {
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
	L_DISCOVERY(">>> DB_UPDATED [from {}]: {}", repr(remote_node.to_string()), repr(path));

	auto node = Node::touch_node(remote_node, false).first;
	if (node) {
		Endpoint local_endpoint(path);
		if (local_endpoint.empty()) {
			L_WARNING("Ignoring update for empty database path: {}!", repr(path));
		} else {
			// Replicate database from the other node
			Endpoint remote_endpoint(path, node);
			trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), local_endpoint.path, remote_endpoint, local_endpoint);
		}
	}
}



void
Discovery::schema_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::schema_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::READY) {
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

	XapiandManager::schemas()->updated(uri, version);
}


void
Discovery::primary_updated([[maybe_unused]] Message type, const std::string& message)
{
	L_CALL("Discovery::primary_updated({}, <message>) {{ state:{} }}", enum_name(type), enum_name(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::READY) {
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

	auto path = std::string(p, p_end - p);

	XapiandManager::sweep_primary(shards, path);
}


void
Discovery::cluster_discovery_cb(ev::timer&, [[maybe_unused]] int revents)
{
	auto state = XapiandManager::state().load();

	L_CALL("Discovery::cluster_discovery_cb(<watcher>, {:#x} ({})) {{ state:{} }}", revents, readable_revents(revents), enum_name(state));

	L_EV_BEGIN("Discovery::cluster_discovery_cb:BEGIN {{ state:{} }}", enum_name(state));
	L_EV_END("Discovery::cluster_discovery_cb:END {{ state:{} }}", enum_name(state));

	switch (state) {
		case XapiandManager::State::RESET: {
			auto local_node = Node::get_local_node();
			auto node_copy = std::make_unique<Node>(*local_node);
			auto drop_name = node_copy->name();
			if (XapiandManager::node_name().empty()) {
				node_copy->name(name_generator());
			} else {
				node_copy->name(XapiandManager::node_name());
			}
			if (!drop_name.empty() && drop_name != node_copy->name()) {
				Node::drop_node(drop_name);
			}
			Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));
			local_node = Node::get_local_node();
			if (XapiandManager::exchange_state(XapiandManager::State::RESET, XapiandManager::State::WAITING, 3s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
				L_INFO("Advertising as {}{}" + INFO_COL + "...", local_node->col().ansi(), local_node->name());
				send_message(Message::CLUSTER_HELLO, local_node->serialise());
			}
			break;
		}
		case XapiandManager::State::WAITING: {
			// We're here because no one sneered nor entered during
			// CLUSTER_DISCOVERY_WAITING_FAST, wait longer then...

			cluster_discovery.repeat = CLUSTER_DISCOVERY_WAITING_SLOW;
			cluster_discovery.again();
			L_EV("Reset discovery's cluster_discovery event ({})", cluster_discovery.repeat);

			if (XapiandManager::exchange_state(XapiandManager::State::WAITING, XapiandManager::State::WAITING_MORE, 3s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
			}
			break;
		}
		case XapiandManager::State::WAITING_MORE: {
			cluster_discovery.stop();
			L_EV("Stop discovery's cluster_discovery event");

			if (XapiandManager::exchange_state(XapiandManager::State::WAITING_MORE, XapiandManager::State::JOINING, 3s, "Joining cluster is taking too long...", "Joining cluster is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
				XapiandManager::join_cluster();
			}
			break;
		}
		default: {
			break;
		}
	}
}


void
Discovery::raft_leader_election_timeout_cb(ev::timer&, [[maybe_unused]] int revents)
{
	L_CALL("Discovery::raft_leader_election_timeout_cb(<watcher>, {:#x} ({})) {{ state:{} }}", revents, readable_revents(revents), enum_name(XapiandManager::state().load()));

	L_EV_BEGIN("Discovery::raft_leader_election_timeout_cb:BEGIN");
	L_EV_END("Discovery::raft_leader_election_timeout_cb:END");

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT_PROTO("<<< LEADER_ELECTION (invalid state: {})", enum_name(XapiandManager::state().load()));
		return;
	}

	if (XapiandManager::state() == XapiandManager::State::JOINING) {
		if (raft_current_term == 0) {
			++raft_current_term;

			auto local_node = Node::get_local_node();

			raft_role = Role::RAFT_LEADER;
			raft_voted_for.clear();
			raft_next_indexes.clear();
			raft_match_indexes.clear();

			_raft_leader_heartbeat_reset(RAFT_HEARTBEAT_LEADER_TIMEOUT);
			_raft_set_leader_node(local_node);

			// First time we elect a leader's, we setup node
			if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP, 3s, "Node setup is taking too long...", "Node setup is finally done!")) {
				// L_DEBUG("Role changed: {} -> {}", enum_name(state), enum_name(XapiandManager::state().load()));
				XapiandManager::setup_node();
			}
			return;
		}
	}

	if (raft_role == Role::RAFT_LEADER) {
		// We're a leader, we shouldn't be here!
		return;
	}

	// If election timeout elapses without receiving AppendEntries
	// RPC from current leader or granting vote to candidate:
	// convert to candidate
	_raft_request_vote(true);
}


void
Discovery::raft_leader_heartbeat_cb(ev::timer&, [[maybe_unused]] int revents)
{
	// L_CALL("Discovery::raft_leader_heartbeat_cb(<watcher>, {:#x} ({})) {{ state:{} }}", revents, readable_revents(revents), enum_name(XapiandManager::state().load()));

	L_EV_BEGIN("Discovery::raft_leader_heartbeat_cb:BEGIN");
	L_EV_END("Discovery::raft_leader_heartbeat_cb:END");

	if (XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT_PROTO_HB("<<< RAFT_HEARTBEAT (invalid state: {})",
			enum_name(XapiandManager::state().load()));
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	auto total_nodes = Node::total_nodes();
	auto alive_nodes = Node::alive_nodes();
	if (!raft_has_consensus(total_nodes, alive_nodes)) {
		L_RAFT_PROTO_HB("<<< RAFT_HEARTBEAT (no consensus)");
		L_RAFT("Vote again! (no consensus) {{ total_nodes:{}, alive_nodes:{} }}",
			total_nodes, alive_nodes);
		_raft_request_vote(false);
		return;
	}

	// If last raft_log index ≥ nextIndex for a follower:
	// send AppendEntries RPC with raft_log entries starting at nextIndex
	auto last_log_index = raft_log.size();
	if (last_log_index > 0) {
		auto entry_index = last_log_index + 1;
		for (const auto& next_index_pair : raft_next_indexes) {
			if (Node::is_alive(next_index_pair.first)) {
				if (entry_index > next_index_pair.second) {
					entry_index = next_index_pair.second;
				}
			}
		}
		if (entry_index > 0 && entry_index <= last_log_index) {
			auto local_node = Node::get_local_node();
			auto prev_log_index = entry_index - 1;
			auto prev_log_term = entry_index > 1 ? raft_log[prev_log_index - 1].term : 0;
			auto entry_term = raft_log[entry_index - 1].term;
			auto entry_command = raft_log[entry_index - 1].command;
			L_RAFT_PROTO("<<< RAFT_APPEND_ENTRIES {{ node:{}, raft_current_term:{}, prev_log_index:{}, prev_log_term:{}, raft_commit_index:{}, last_log_index:{}, entry_term:{}, entry_command:{} }}",
				repr(local_node->to_string()), raft_current_term, prev_log_index, prev_log_term, raft_commit_index, last_log_index, entry_term, repr(entry_command));
			send_message(Message::RAFT_APPEND_ENTRIES,
				local_node->serialise() +
				serialise_length(raft_current_term) +
				serialise_length(prev_log_index) +
				serialise_length(prev_log_term) +
				serialise_length(raft_commit_index) +
				serialise_length(last_log_index) +
				serialise_length(entry_term) +
				serialise_string(entry_command));
			return;
		}
	}

	auto local_node = Node::get_local_node();
	auto last_log_term = last_log_index > 0 ? raft_log[last_log_index - 1].term : 0;
	L_RAFT_PROTO_HB("<<< RAFT_HEARTBEAT {{ node:{}, raft_current_term:{}, last_log_index:{}, last_log_term:{}, raft_commit_index:{} }}",
		repr(local_node->to_string()), raft_current_term, last_log_index, last_log_term, raft_commit_index);
	send_message(Message::RAFT_HEARTBEAT,
		local_node->serialise() +
		serialise_length(raft_current_term) +
		serialise_length(last_log_index) +
		serialise_length(last_log_term) +
		serialise_length(raft_commit_index));
}


void
Discovery::_raft_leader_heartbeat_reset(double timeout)
{
	L_CALL("Discovery::_raft_leader_heartbeat_reset({})", timeout);

	raft_leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	raft_leader_heartbeat.repeat = timeout;
	raft_leader_heartbeat.again();
	L_EV("Restart raft's leader heartbeat event ({})", raft_leader_heartbeat.repeat);
}


void
Discovery::_raft_leader_election_timeout_reset(double timeout)
{
	L_CALL("Discovery::_raft_leader_election_timeout_reset({})", timeout);

	raft_leader_election_timeout.repeat = timeout;
	raft_leader_election_timeout.again();
	L_EV("Restart raft's leader election timeout event ({})", raft_leader_election_timeout.repeat);

	raft_leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");
}


void
Discovery::_raft_set_leader_node(const std::shared_ptr<const Node>& node)
{
	L_CALL("Discovery::_raft_set_leader_node({})", repr(node->name()));

	auto leader_node = Node::get_leader_node();
	if (!Node::is_superset(leader_node, node)) {
		Node::set_leader_node(node);
		XapiandManager::new_leader();
	}
}


void
Discovery::_raft_apply_command(const std::string& command)
{
	L_CALL("Discovery::_raft_apply_command({})", repr(command));

	L_RAFT("Apply command: {}", repr(command));

	XapiandManager::raft_apply_command(command);
}


bool
Discovery::_raft_commit_log()
{
	L_CALL("Discovery::_raft_commit_log()");

	// If there exists an N such that N > commitIndex,
	// a majority of matchIndex[i] ≥ N,
	// and raft_log[N].term == currentTerm:
	// set commitIndex = N
	auto old_raft_last_applied = raft_last_applied;
	auto last_index = raft_log.size();
	for (size_t index = raft_commit_index + 1; index <= last_index; ++index) {
		if (raft_log[index - 1].term == raft_current_term) {
			size_t matches = 1;
			for (const auto& match_index_pair : raft_match_indexes) {
				if (match_index_pair.second >= index) {
					++matches;
				}
			}
			if (raft_has_consensus(Node::total_nodes(), matches)) {
				raft_commit_index = index;
				if (raft_commit_index > raft_last_applied) {
					L_RAFT("Commit {{ raft_commit_index:{}, raft_last_applied:{}, last_index:{} }}",
						raft_commit_index, raft_last_applied, last_index);

					// If commitIndex > lastApplied:
					while (raft_commit_index > raft_last_applied && raft_last_applied < last_index) {
						// apply raft_log[lastApplied] to state machine
						const auto& command = raft_log[raft_last_applied].command;
						_raft_apply_command(command);
						// and increment lastApplied
						++raft_last_applied;
					}
				}
			}
		}
	}
	return old_raft_last_applied != raft_last_applied;
}


void
Discovery::_raft_request_vote(bool immediate)
{
	L_CALL("Discovery::_raft_request_vote({})", immediate);

	Node::set_leader_node(std::make_shared<const Node>());

	if (immediate) {
		++raft_current_term;
		raft_role = Role::RAFT_CANDIDATE;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		raft_votes_granted = 0;
		raft_votes_denied = 0;
		raft_voters.clear();

		_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));

		auto last_log_index = raft_log.size();
		auto last_log_term = last_log_index > 0 ? raft_log[last_log_index - 1].term : 0;

		auto local_node = Node::get_local_node();
		L_RAFT_PROTO("<<< RAFT_REQUEST_VOTE {{ node:{}, raft_current_term:{}, last_log_term:{}, last_log_index:{}, state:{}, timeout:{}, alive_nodes:{}, leader:{} }}",
			repr(local_node->to_string()), raft_current_term, last_log_term, last_log_index, enum_name(raft_role), raft_leader_election_timeout.repeat, Node::alive_nodes(), Node::get_leader_node()->empty() ? "<none>" : Node::get_leader_node()->to_string());
		send_message(Message::RAFT_REQUEST_VOTE,
			local_node->serialise() +
			serialise_length(raft_current_term) +
			serialise_length(last_log_term) +
			serialise_length(last_log_index));
	} else {
		raft_role = Role::RAFT_FOLLOWER;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();

		if (raft_current_term == 0) {
			_raft_leader_election_timeout_reset(RAFT_LEADER_ELECTION_MAX);
		} else {
			_raft_leader_election_timeout_reset(random_real(RAFT_LEADER_ELECTION_MIN, RAFT_LEADER_ELECTION_MAX));
		}
	}
}


void
Discovery::raft_request_vote()
{
	L_CALL("Discovery::raft_request_vote()");

	raft_request_vote_async.send();
}


void
Discovery::raft_request_vote_async_cb(ev::async&, [[maybe_unused]] int revents)
{
	L_CALL("Discovery::raft_request_vote_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::raft_request_vote_async_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::state()));
	L_EV_END("Discovery::raft_request_vote_async_cb:END {{ state:{} }}", enum_name(XapiandManager::state()));

	_raft_request_vote(false);
}


void
Discovery::_raft_add_command(const std::string& command)
{
	L_CALL("Discovery::_raft_add_command({})", repr(command));

	if (raft_role == Role::RAFT_LEADER) {
		raft_log.push_back({
			raft_current_term,
			command,
		});

		_raft_commit_log();

#ifdef L_RAFT_LOG
		std::string log;
		for (size_t i = 0; i < raft_log.size(); ++i) {
			log += strings::format("\n    {} raft_log[{}] -> {{ term:{}, command:{} }}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
		}
		L_RAFT_LOG("Command added to raft_log: {}{}", repr(command), log.empty() ? "\n    (empty)" : log);
#endif
	} else {
		auto local_node = Node::get_local_node();
		send_message(Message::RAFT_ADD_COMMAND,
			local_node->serialise() +
			serialise_string(command));
	}
}


void
Discovery::raft_add_command_async_cb(ev::async&, [[maybe_unused]] int revents)
{
	L_CALL("Discovery::raft_add_command_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::raft_add_command_async_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::state()));
	L_EV_END("Discovery::raft_add_command_async_cb:END {{ state:{} }}", enum_name(XapiandManager::state()));

	std::string command;
	while (raft_add_command_args.try_dequeue(command)) {
		_raft_add_command(command);
	}
}


void
Discovery::raft_add_command(const std::string& command)
{
	L_CALL("Discovery::raft_add_command({})", repr(command));

	raft_add_command_args.enqueue(command);

	raft_add_command_async.send();
}


void
Discovery::_message_send(Message type, const std::string& message)
{
	assert(type == Message::DB_UPDATED || type == Message::SCHEMA_UPDATED || type == Message::PRIMARY_UPDATED);

	auto local_node = Node::get_local_node();
	send_message(type,
		local_node->serialise() +   // The node where the index is at
		message);

	L_DEBUG("Sending {} message: {}", enum_name(type), repr(message));
}


void
Discovery::cluster_enter_async_cb(ev::async&, [[maybe_unused]] int revents)
{
	L_CALL("Discovery::cluster_enter_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::cluster_enter_async_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::state()));
	L_EV_END("Discovery::cluster_enter_async_cb:END {{ state:{} }}", enum_name(XapiandManager::state()));

	auto local_node = Node::get_local_node();

	if (raft_role == Role::RAFT_LEADER) {
		// If we're leader, check consensus or vote.
		auto total_nodes = Node::total_nodes();
		auto alive_nodes = Node::alive_nodes();
		if (!raft_has_consensus(total_nodes, alive_nodes)) {
			L_RAFT("Vote again! (no consensus) {{ total_nodes:{}, alive_nodes:{} }}",
				total_nodes, alive_nodes);
			_raft_request_vote(false);
		}
	}

	send_message(Message::CLUSTER_ENTER, local_node->serialise());
}


void
Discovery::cluster_enter()
{
	L_CALL("Discovery::cluster_enter()");

	cluster_enter_async.send();
}


void
Discovery::message_send_async_cb(ev::async&, [[maybe_unused]] int revents)
{
	L_CALL("Discovery::message_send_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::message_send_async_cb:BEGIN {{ state:{} }}", enum_name(XapiandManager::state()));
	L_EV_END("Discovery::message_send_async_cb:END {{ state:{} }}", enum_name(XapiandManager::state()));

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

	message_send_async.send();
}


void
Discovery::schema_updated_send(Xapian::rev revision, std::string_view path)
{
	L_CALL("Discovery::schema_updated_send({}, {})", revision, repr(path));

	auto message = serialise_length(revision);
	message.append(path);

	message_send_args.enqueue(std::make_pair(Message::SCHEMA_UPDATED, message));

	message_send_async.send();
}


void
Discovery::primary_updated_send(size_t shards, std::string_view path)
{
	L_CALL("Discovery::primary_updated_send({}, {})", shards, repr(path));

	auto message = serialise_length(shards);
	message.append(path);

	message_send_args.enqueue(std::make_pair(Message::PRIMARY_UPDATED, message));

	message_send_async.send();
}


std::string
Discovery::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Discovery ({}) {{ cnt:{}, sock:{} }}{}{}{}>",
		enum_name(raft_role),
		use_count(),
		sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}


std::string
Discovery::getDescription() const
{
	L_CALL("Discovery::getDescription()");

	return strings::format("UDP {}:{} ({} v{}.{})", addr.sin_addr.s_addr ? inet_ntop(addr) : "", ntohs(addr.sin_port), description, XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION, XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION);
}


void
db_updated_send(Xapian::rev revision, std::string path)
{
	XapiandManager::discovery()->db_updated_send(revision, path);
}

void
schema_updated_send(Xapian::rev revision, std::string path)
{
	XapiandManager::discovery()->schema_updated_send(revision, path);
}

void
primary_updated_send(size_t shards, std::string path)
{
	XapiandManager::discovery()->primary_updated_send(shards, path);
}

#endif
