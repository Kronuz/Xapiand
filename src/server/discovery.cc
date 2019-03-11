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

#include <errno.h>                          // for errno
#include <sysexits.h>                       // for EX_SOFTWARE

#include "cassert.h"                        // for ASSERT
#include "color_tools.hh"                   // for color
#include "cuuid/uuid.h"                     // for UUID
#include "epoch.hh"                         // for epoch::now
#include "error.hh"                         // for error:name, error::description
#include "ignore_unused.h"                  // for ignore_unused
#include "manager.h"                        // for XapiandManager, XapiandManager::StateNames, XapiandManager::State
#include "namegen.h"                        // for name_generator
#include "node.h"                           // for Node, local_node
#include "opts.h"                           // for opts::*
#include "random.hh"                        // for random_int
#include "readable_revents.hh"              // for readable_revents
#include "repr.hh"                          // for repr
#include "utype.hh"                         // for toUType


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DISCOVERY
// #define L_DISCOVERY L_SALMON
// #undef L_RAFT
// #define L_RAFT L_SEA_GREEN
// #undef L_EV_BEGIN
// #define L_EV_BEGIN L_DELAYED_200
// #undef L_EV_END
// #define L_EV_END L_DELAYED_N_UNLOG


static inline bool raft_has_consensus(size_t votes) {
	auto active_nodes = Node::active_nodes();
	return active_nodes == 1 || votes > active_nodes / 2;
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
	  db_update_send_async(*ev_loop),
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

	db_update_send_async.set<Discovery, &Discovery::db_update_send_async_cb>(this);
	db_update_send_async.start();
	L_EV("Start discovery's async db_update_send signal event");
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
	L_EV("Start discovery's server accept event {{sock:{}}}", sock);

	L_DISCOVERY("Discovery was started! (exploring)");
}


void
Discovery::stop_impl()
{
	L_CALL("Discovery::stop_impl()");

	Worker::stop_impl();

	auto local_node = Node::local_node();
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
	L_CALL("Discovery::send_message({}, <message>)", MessageNames(type));

	L_DISCOVERY_PROTO("<< send_message ({}): {}", MessageNames(type), repr(message));
	UDP::send_message(toUType(type), message);
}


void
Discovery::io_accept_cb(ev::io &watcher, int revents)
{
	L_CALL("Discovery::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	L_EV_BEGIN("Discovery::io_accept_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Discovery::io_accept_cb:END {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(watcher);
	ASSERT(sock == -1 || sock == watcher.fd);

	if (closed) {
		return;
	}

	L_DEBUG_HOOK("Discovery::io_accept_cb", "Discovery::io_accept_cb(<watcher>, {:#x} ({})) {{sock:{}}}", revents, readable_revents(revents), watcher.fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid cluster_discovery event {{sock:{}}}: {} ({}): {}", watcher.fd, error::name(errno), errno, error::description(errno));
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
				L_DISCOVERY_PROTO(">> get_message ({}): {}", MessageNames(type), repr(message));
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
	L_CALL("Discovery::discovery_server({}, <message>)", MessageNames(type));

	L_EV_BEGIN("Discovery::discovery_server:BEGIN {{state:{}, type:{}}}", XapiandManager::StateNames(XapiandManager::state()), MessageNames(type));
	L_EV_END("Discovery::discovery_server:END {{state:{}, type:{}}}", XapiandManager::StateNames(XapiandManager::state()), MessageNames(type));

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
		default: {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
	}
}


void
Discovery::cluster_hello(Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_hello({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> {} [from {}]", MessageNames(type), remote_node.name());


	auto local_node = Node::local_node();

	if (!Node::is_superset(local_node, remote_node)) {
		auto put = Node::touch_node(remote_node, false);
		if (put.first == nullptr) {
			send_message(Message::CLUSTER_SNEER, remote_node.serialise());
		} else {
			send_message(Message::CLUSTER_WAVE, local_node->serialise());
		}
	}
}


void
Discovery::cluster_wave(Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_wave({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> {} [from {}]", MessageNames(type), remote_node.name());

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node: {}[{}] {}", remote_node.col().ansi(), remote_node.idx, remote_node.name());
	} else {
		auto node = put.first;
		L_DEBUG("Added node: {}[{}] {}", node->col().ansi(), node->idx, node->name());
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " is at the party on ip:{}, tcp:{} (http), tcp:{} (xapian)!", node->col().ansi(), node->name(), node->host(), node->http_port, node->remote_port);
		}

		// After receiving WAVE, flag as WAITING_MORE so it waits just a little longer
		// (prevent it from switching to slow waiting)
		if (XapiandManager::exchange_state(XapiandManager::State::WAITING, XapiandManager::State::WAITING_MORE, 3s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
			// L_DEBUG("State changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
		}
	}
}


void
Discovery::cluster_sneer(Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_sneer({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::RESET &&
		XapiandManager::state() != XapiandManager::State::WAITING &&
		XapiandManager::state() != XapiandManager::State::WAITING_MORE &&
		XapiandManager::state() != XapiandManager::State::JOINING) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> {} [from {}]", MessageNames(type), remote_node.name());

	auto local_node = Node::local_node();
	if (remote_node == *local_node) {
		if (XapiandManager::node_name().empty()) {
			L_DISCOVERY("Node name {} already taken. Retrying other name...", local_node->name());
			XapiandManager::reset_state();
		} else {
			XapiandManager::state().store(XapiandManager::State::BAD);
			Node::local_node(std::make_shared<const Node>());
			L_CRIT("Cannot join the party. Node name {} already taken!", local_node->name());
			sig_exit(-EX_SOFTWARE);
		}
	}
}


void
Discovery::cluster_enter(Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_enter({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> {} [from {}]", MessageNames(type), remote_node.name());

	auto put = Node::touch_node(remote_node, true);
	if (put.first == nullptr) {
		L_ERR("Denied node: {}[{}] {}", remote_node.col().ansi(), remote_node.idx, remote_node.name());
	} else {
		auto node = put.first;
		L_DEBUG("Added node: {}[{}] {}", node->col().ansi(), node->idx, node->name());
		if (put.second) {
			L_INFO("Node {}{}" + INFO_COL + " joined the party on ip:{}, tcp:{} (http), tcp:{} (xapian)!", node->col().ansi(), node->name(), node->host(), node->http_port, node->remote_port);
		}
	}
}


void
Discovery::cluster_bye(Message type, const std::string& message)
{
	L_CALL("Discovery::cluster_bye({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> {} [from {}]", MessageNames(type), remote_node.name());

	Node::drop_node(remote_node.name());

	auto leader_node = Node::leader_node();
	if (*leader_node == remote_node) {
		L_INFO("Leader node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), remote_node.name());

		Node::leader_node(std::make_shared<const Node>());
		XapiandManager::renew_leader();
	} else {
		L_INFO("Node {}{}" + INFO_COL + " left the party!", remote_node.col().ansi(), remote_node.name());
	}

	L_DEBUG("Nodes still active after {} left: {}", remote_node.name(), Node::active_nodes());
}


void
Discovery::raft_request_vote(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_request_vote({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> {} (invalid state: {})", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT(">> {} [from {}] (nonexistent node)", MessageNames(type), remote_node.name());
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
		_raft_leader_election_timeout_reset();
	}

	L_RAFT(">> {} [from {}]{}", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	auto granted = false;
	if (term == raft_current_term) {
		if (raft_voted_for.empty()) {
			if (Node::is_local(node)) {
				raft_voted_for = *node;
				L_RAFT("I vote for {} (1)", raft_voted_for.name());
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
					L_RAFT("I vote for {} (raft_log term is newer)", raft_voted_for.name());
				} else if (last_log_term == remote_last_log_term) {
					// If the logs end with the same term, then whichever
					// raft_log is longer is more up-to-date.
					if (raft_log.size() <= remote_last_log_index) {
						raft_voted_for = *node;
						L_RAFT("I vote for {} (raft_log index size concurs)", raft_voted_for.name());
					} else {
						L_RAFT("I don't vote for {} (raft_log index is shorter)", raft_voted_for.name());
					}
				} else {
					L_RAFT("I don't vote for {} (raft_log term is older)", raft_voted_for.name());
				}
			}
		} else {
			L_RAFT("I already voted for {}", raft_voted_for.name());
		}
		granted = raft_voted_for == *node;
	}

	L_RAFT("   << REQUEST_VOTE_RESPONSE {{node:{}, term:{}, granted:{}}}", node->name(), term, granted);
	send_message(Message::RAFT_REQUEST_VOTE_RESPONSE,
		node->serialise() +
		serialise_length(term) +
		serialise_length(granted));
}


void
Discovery::raft_request_vote_response(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_request_vote_response({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (raft_role != Role::RAFT_CANDIDATE) {
		return;
	}

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> {} (invalid state: {})", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT(">> {} [from {}] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	auto local_node = Node::local_node();

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
		_raft_leader_election_timeout_reset();
	}

	L_RAFT(">> {} [from {}]{}", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	if (term == raft_current_term) {
		if (Node::is_superset(local_node, node)) {
			bool granted = unserialise_length(&p, p_end);
			if (granted) {
				++raft_votes_granted;
			} else {
				++raft_votes_denied;
			}
			L_RAFT("Number of servers: {}; Votes granted: {}; Votes denied: {}", Node::active_nodes(), raft_votes_granted, raft_votes_denied);
			if (raft_has_consensus(raft_votes_granted + raft_votes_denied)) {
				if (raft_votes_granted > raft_votes_denied) {
					raft_role = Role::RAFT_LEADER;
					raft_voted_for.clear();
					raft_next_indexes.clear();
					raft_match_indexes.clear();

					_raft_leader_heartbeat_start();
					_raft_set_leader_node(node);

					auto entry_index = raft_log.size();
					auto prev_log_index = entry_index - 1;
					auto prev_log_term = entry_index > 1 ? raft_log[prev_log_index - 1].term : 0;

					L_RAFT("   << HEARTBEAT {{node:{}, term:{}, prev_log_term:{}, prev_log_index:{}, raft_commit_index:{}}}",
						local_node->name(), raft_current_term, prev_log_term, prev_log_index, raft_commit_index);
					send_message(Message::RAFT_HEARTBEAT,
						local_node->serialise() +
						serialise_length(raft_current_term) +
						serialise_length(prev_log_index) +
						serialise_length(prev_log_term) +
						serialise_length(raft_commit_index));

					// First time we elect a leader's, we setup node
					if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP, 3s, "Node setup is taking too long...", "Node setup is finally done!")) {
						// L_DEBUG("Role changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
						XapiandManager::setup_node();
					}
				}
			}
		}
	}
}


void
Discovery::raft_append_entries(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_append_entries({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> {} (invalid state: {})", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT(">> {} [from {}] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	auto local_node = Node::local_node();

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
		// _raft_leader_election_timeout_reset();  // resetted below!
	}

	if (raft_role == Role::RAFT_LEADER) {
		if (!Node::is_superset(local_node, node)) {
			// If another leader is around, immediately run for election
			_raft_request_vote(true);
		}
		return;
	}

	L_RAFT(">> {} [from {}]{}", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

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

		_raft_leader_election_timeout_reset();
		_raft_set_leader_node(node);

		// Reply false if raft_log doesn’t contain an entry at
		// prevLogIndex whose term matches prevLogTerm
		auto last_index = raft_log.size();
		auto entry_index = prev_log_index + 1;
		// L_RAFT("   {{entry_index:{}, prev_log_index:{}, last_index:{}, prev_log_term:{}}}", entry_index, prev_log_index, last_index, prev_log_term);
		if (entry_index <= 1 || (prev_log_index <= last_index && raft_log[prev_log_index - 1].term == prev_log_term)) {
			if (type == Message::RAFT_APPEND_ENTRIES) {
				size_t last_log_index = unserialise_length(&p, p_end);
				uint64_t entry_term = unserialise_length(&p, p_end);
				auto entry_command = unserialise_string(&p, p_end);
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
			}

			// If leaderCommit > commitIndex,
			// set commitIndex = min(leaderCommit, index of last new entry)
			size_t leader_commit = unserialise_length(&p, p_end);
			if (leader_commit > raft_commit_index) {
				raft_commit_index = std::min(leader_commit, entry_index);
				if (raft_commit_index > raft_last_applied) {
					L_RAFT("committed {{raft_commit_index:{}}}", raft_commit_index);

					// If commitIndex > lastApplied:
					while (raft_commit_index > raft_last_applied) {
						// increment lastApplied,
						++raft_last_applied;
						// apply raft_log[lastApplied] to state machine
						const auto& command = raft_log[raft_last_applied - 1].command;
						_raft_apply(command);
					}
				}
			}

			if (leader_commit == raft_commit_index) {
				// First time we reach leader's commit, we setup node
				if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP, 3s, "Node setup is taking too long...", "Node setup is finally done!")) {
					// L_DEBUG("Role changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
					XapiandManager::setup_node();
				}
			}

			next_index = last_index + 1;
			match_index = entry_index;
			success = true;
		}
	}

	Message response_type;
	if (type != Message::RAFT_HEARTBEAT) {
		response_type = Message::RAFT_APPEND_ENTRIES_RESPONSE;
	} else {
		response_type = Message::RAFT_HEARTBEAT_RESPONSE;
	}
	L_RAFT("   << {} {{node:{}, term:{}, success:{}}}",
		MessageNames(response_type), local_node->name(), term, success);
	send_message(response_type,
		local_node->serialise() +
		serialise_length(term) +
		serialise_length(success) +
		(success
			? serialise_length(next_index) +
			  serialise_length(match_index)
			: ""
		));

#ifdef L_RAFT_LOG
	for (size_t i = 0; i < raft_log.size(); ++i) {
		L_RAFT_LOG("   {} raft_log[{}] -> {{term:{}, command:{}}}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
	}
#endif
}


void
Discovery::raft_append_entries_response(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_append_entries_response({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> {} (invalid state: {})", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT(">> {} [from {}] (nonexistent node)", MessageNames(type), remote_node.name());
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
		_raft_leader_election_timeout_reset();
	}

	L_RAFT(">> {} [from {}]{}", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	if (term == raft_current_term) {
		bool success = unserialise_length(&p, p_end);
		if (success) {
			// If successful:
			// update nextIndex and matchIndex for follower
			size_t next_index = unserialise_length(&p, p_end);
			size_t match_index = unserialise_length(&p, p_end);
			raft_next_indexes[node->lower_name()] = next_index;
			raft_match_indexes[node->lower_name()] = match_index;
			L_RAFT("   {{success:{}, next_index:{}, match_index:{}}}", success, next_index, match_index);
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
			L_RAFT("   {{success:{}, next_index:{}}}", success, next_index);
		}
		_raft_commit_log();

#ifdef L_RAFT_LOG
		for (size_t i = 0; i < raft_log.size(); ++i) {
			L_RAFT_LOG("{} raft_log[{}] -> {{term:{}, command:{}}}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
		}
#endif
	}
}


void
Discovery::raft_add_command(Message type, const std::string& message)
{
	L_CALL("Discovery::raft_add_command({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> {} (invalid state: {})", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node, false).first;
	if (!node) {
		L_RAFT(">> {} [from {}] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	auto command = std::string(unserialise_string(&p, p_end));
	raft_add_command(command);
}


void
Discovery::db_updated(Message type, const std::string& message)
{
	L_CALL("Discovery::db_updated({}, <message>) {{state:{}}}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);

	auto local_node = Node::local_node();
	if (Node::is_superset(local_node, remote_node)) {
		// It's just me, do nothing!
		return;
	}

	auto path = std::string(p, p_end);
	L_DISCOVERY(">> {} [from {}]: {}", MessageNames(type), remote_node.name(), repr(path));

	auto node = Node::touch_node(remote_node, false).first;
	if (node) {
		Endpoint local_endpoint(path);
		if (local_endpoint.empty()) {
			L_WARNING("Ignoring update for empty database path: {}!", repr(path));
		} else {
			// Replicate database from the other node
			Endpoint remote_endpoint(path, node->idx);
			trigger_replication()->delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, local_endpoint.path, remote_endpoint, local_endpoint);
		}
	}
}


void
Discovery::cluster_discovery_cb(ev::timer&, int revents)
{
	auto state = XapiandManager::state().load();

	L_CALL("Discovery::cluster_discovery_cb(<watcher>, {:#x} ({})) {{state:{}}}", revents, readable_revents(revents), XapiandManager::StateNames(state));

	L_EV_BEGIN("Discovery::cluster_discovery_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(state));
	L_EV_END("Discovery::cluster_discovery_cb:END {{state:{}}}", XapiandManager::StateNames(state));

	ignore_unused(revents);

	switch (state) {
		case XapiandManager::State::RESET: {
			auto local_node = Node::local_node();
			auto node_copy = std::make_unique<Node>(*local_node);
			std::string drop = node_copy->name();

			if (XapiandManager::node_name().empty()) {
				node_copy->name(name_generator());
			} else {
				node_copy->name(XapiandManager::node_name());
			}
			Node::local_node(std::shared_ptr<const Node>(node_copy.release()));

			if (!drop.empty()) {
				Node::drop_node(drop);
			}

			local_node = Node::local_node();
			if (XapiandManager::exchange_state(XapiandManager::State::RESET, XapiandManager::State::WAITING, 3s, "Waiting for other nodes is taking too long...", "Waiting for other nodes is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
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
				// L_DEBUG("State changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
			}
			break;
		}
		case XapiandManager::State::WAITING_MORE: {
			cluster_discovery.stop();
			L_EV("Stop discovery's cluster_discovery event");

			if (XapiandManager::exchange_state(XapiandManager::State::WAITING_MORE, XapiandManager::State::JOINING, 3s, "Joining cluster is taking too long...", "Joining cluster is finally done!")) {
				// L_DEBUG("State changed: {} -> {}", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
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
Discovery::raft_leader_election_timeout_cb(ev::timer&, int revents)
{
	L_CALL("Discovery::raft_leader_election_timeout_cb(<watcher>, {:#x} ({})) {{state:{}}}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::state().load()));

	L_EV_BEGIN("Discovery::raft_leader_election_timeout_cb:BEGIN");
	L_EV_END("Discovery::raft_leader_election_timeout_cb:END");

	ignore_unused(revents);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT("   << LEADER_ELECTION (invalid state: {})", XapiandManager::StateNames(XapiandManager::state().load()));
		return;
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
Discovery::raft_leader_heartbeat_cb(ev::timer&, int revents)
{
	// L_CALL("Discovery::raft_leader_heartbeat_cb(<watcher>, {:#x} ({})) {{state:{}}}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::state().load()));

	L_EV_BEGIN("Discovery::raft_leader_heartbeat_cb:BEGIN");
	L_EV_END("Discovery::raft_leader_heartbeat_cb:END");

	ignore_unused(revents);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT("   << HEARTBEAT (invalid state: {})", XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	// If last raft_log index ≥ nextIndex for a follower:
	// send AppendEntries RPC with raft_log entries starting at nextIndex
	auto last_log_index = raft_log.size();
	if (last_log_index > 0) {
		auto entry_index = last_log_index + 1;
		for (const auto& next_index_pair : raft_next_indexes) {
			if (entry_index > next_index_pair.second) {
				entry_index = next_index_pair.second;
			}
		}
		if (entry_index > 0 && entry_index <= last_log_index) {
			auto local_node = Node::local_node();
			auto prev_log_index = entry_index - 1;
			auto prev_log_term = entry_index > 1 ? raft_log[prev_log_index - 1].term : 0;
			auto entry_term = raft_log[entry_index - 1].term;
			auto entry_command = raft_log[entry_index - 1].command;
			L_RAFT("   << APPEND_ENTRIES {{raft_current_term:{}, prev_log_index:{}, prev_log_term:{}, last_log_index:{}, entry_term:{}, entry_command:{}, raft_commit_index:{}}}",
				raft_current_term, prev_log_index, prev_log_term, last_log_index, entry_term, repr(entry_command), raft_commit_index);
			send_message(Message::RAFT_APPEND_ENTRIES,
				local_node->serialise() +
				serialise_length(raft_current_term) +
				serialise_length(prev_log_index) +
				serialise_length(prev_log_term) +
				serialise_length(last_log_index) +
				serialise_length(entry_term) +
				serialise_string(entry_command) +
				serialise_length(raft_commit_index));
			return;
		}
	}

	auto local_node = Node::local_node();
	auto last_log_term = last_log_index > 0 ? raft_log[last_log_index - 1].term : 0;
	L_RAFT("   << HEARTBEAT {{last_log_term:{}, last_log_index:{}, raft_commit_index:{}}}", last_log_term, last_log_index, raft_commit_index);
	send_message(Message::RAFT_HEARTBEAT,
		local_node->serialise() +
		serialise_length(raft_current_term) +
		serialise_length(last_log_index) +
		serialise_length(last_log_term) +
		serialise_length(raft_commit_index));
}


void
Discovery::_raft_leader_heartbeat_start(double min, double max)
{
	L_CALL("Discovery::_raft_leader_heartbeat_start()");

	raft_leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	raft_leader_heartbeat.repeat = random_real(min, max);
	raft_leader_heartbeat.again();
	L_EV("Restart raft's leader heartbeat event ({})", raft_leader_heartbeat.repeat);
}


void
Discovery::_raft_leader_election_timeout_reset(double min, double max)
{
	L_CALL("Discovery::_raft_leader_election_timeout_reset({}, {})", min, max);

	raft_leader_election_timeout.repeat = random_real(min, max);
	raft_leader_election_timeout.again();
	L_EV("Restart raft's leader election timeout event ({})", raft_leader_election_timeout.repeat);

	raft_leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");
}


void
Discovery::_raft_set_leader_node(const std::shared_ptr<const Node>& node)
{
	L_CALL("Discovery::_raft_set_leader_node({})", repr(node->name()));

	auto leader_node = Node::leader_node();
	L_CALL("leader_node -> {}", leader_node->__repr__());
	if (!Node::is_superset(leader_node, node)) {
		Node::leader_node(node);
		XapiandManager::new_leader();
	}
}


void
Discovery::_raft_apply(const std::string& command)
{
	L_CALL("Discovery::_raft_apply({})", repr(command));

	const char *p = command.data();
	const char *p_end = p + command.size();

	size_t idx = unserialise_length(&p, p_end);
	auto node_name = unserialise_string(&p, p_end);

	Node indexed_node;

	auto node = Node::get_node(node_name);
	if (node) {
		indexed_node = *node;
		indexed_node.idx = idx;
	} else {
		indexed_node.name(std::string(node_name));
		indexed_node.idx = idx;
	}

	auto put = Node::touch_node(indexed_node, false);
	if (put.first == nullptr) {
		L_ERR("Denied node: {}[{}] {}", node->col().ansi(), node->idx, node->name());
	} else {
		node = put.first;
		L_DEBUG("Added node: {}[{}] {}", node->col().ansi(), node->idx, node->name());
	}
}


void
Discovery::_raft_commit_log()
{
	L_CALL("Discovery::_raft_commit_log()");

	// If there exists an N such that N > commitIndex,
	// a majority of matchIndex[i] ≥ N,
	// and raft_log[N].term == currentTerm:
	// set commitIndex = N
	for (size_t index = raft_commit_index + 1; index <= raft_log.size(); ++index) {
		if (raft_log[index - 1].term == raft_current_term) {
			size_t matches = 1;
			for (const auto& match_index_pair : raft_match_indexes) {
				if (match_index_pair.second >= index) {
					++matches;
				}
			}
			if (raft_has_consensus(matches)) {
				raft_commit_index = index;
				L_RAFT("committed {{matches:{}, active_nodes:{}, raft_commit_index:{}}}",
					matches, Node::active_nodes(), raft_commit_index);

				// If commitIndex > lastApplied:
				while (raft_commit_index > raft_last_applied) {
					// increment lastApplied,
					++raft_last_applied;
					// apply raft_log[lastApplied] to state machine
					const auto& command = raft_log[raft_last_applied - 1].command;
					_raft_apply(command);
				}
			} else {
				L_RAFT("not committed {{matches:{}, active_nodes:{}, raft_commit_index:{}}}",
					matches, Node::active_nodes(), raft_commit_index);
			}
		}
	}
}


void
Discovery::_raft_request_vote(bool immediate)
{
	L_CALL("Discovery::_raft_request_vote({})", immediate);

	if (immediate) {
		++raft_current_term;
		raft_role = Role::RAFT_CANDIDATE;
		raft_voted_for.clear();
		raft_next_indexes.clear();
		raft_match_indexes.clear();
		raft_votes_granted = 0;
		raft_votes_denied = 0;

		_raft_leader_election_timeout_reset();

		auto last_log_index = raft_log.size();
		auto last_log_term = last_log_index > 0 ? raft_log[last_log_index - 1].term : 0;

		auto local_node = Node::local_node();
		L_RAFT("   << REQUEST_VOTE {{ node:{}, term:{}, last_log_term:{}, last_log_index:{}, state:{}, timeout:{}, active_nodes:{}, leader:{} }}",
			local_node->name(), raft_current_term, last_log_term, last_log_index, RoleNames(raft_role), raft_leader_election_timeout.repeat, Node::active_nodes(), Node::leader_node()->empty() ? "<none>" : Node::leader_node()->name());
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

		_raft_leader_election_timeout_reset(0, RAFT_LEADER_ELECTION_MAX - RAFT_LEADER_ELECTION_MIN);
	}
}


void
Discovery::raft_request_vote()
{
	L_CALL("Discovery::raft_request_vote()");

	raft_request_vote_async.send();
}


void
Discovery::raft_request_vote_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::raft_request_vote_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::raft_request_vote_async_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Discovery::raft_request_vote_async_cb:END {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

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
		for (size_t i = 0; i < raft_log.size(); ++i) {
			L_RAFT_LOG("{} raft_log[{}] -> {{term:{}, command:{}}}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
		}
#endif
	} else {
		auto local_node = Node::local_node();
		send_message(Message::RAFT_ADD_COMMAND,
			local_node->serialise() +
			serialise_string(command));
	}
}


void
Discovery::raft_add_command_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::raft_add_command_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::raft_add_command_async_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Discovery::raft_add_command_async_cb:END {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

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
Discovery::_db_update_send(const std::string& path)
{
	auto local_node = Node::local_node();
	send_message(Message::DB_UPDATED,
		local_node->serialise() +   // The node where the index is at
		path);  // The path of the index

	L_DEBUG("Sending database updated signal for {}", repr(path));
}


void
Discovery::cluster_enter_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::cluster_enter_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::cluster_enter_async_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Discovery::cluster_enter_async_cb:END {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

	auto local_node = Node::local_node();
	send_message(Message::CLUSTER_ENTER, local_node->serialise());
}


void
Discovery::cluster_enter()
{
	L_CALL("Discovery::cluster_enter()");

	cluster_enter_async.send();
}


void
Discovery::db_update_send_async_cb(ev::async&, int revents)
{
	L_CALL("Discovery::db_update_send_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("Discovery::db_update_send_async_cb:BEGIN {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Discovery::db_update_send_async_cb:END {{state:{}}}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

	std::string path;
	while (db_update_send_args.try_dequeue(path)) {
		_db_update_send(path);
	}
}


void
Discovery::db_update_send(const std::string& path)
{
	L_CALL("Discovery::db_update_send({})", repr(path));

	db_update_send_args.enqueue(path);

	db_update_send_async.send();
}


std::string
Discovery::__repr__() const
{
	return string::format("<Discovery ({}) {{cnt:{}, sock:{}}}{}{}{}>",
		RoleNames(raft_role),
		use_count(),
		sock,
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}


std::string
Discovery::getDescription() const
{
	L_CALL("Discovery::getDescription()");

	return string::format("UDP {}:{} ({} v{}.{})", addr.sin_addr.s_addr ? inet_ntop(addr) : "", ntohs(addr.sin_port), description, XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION, XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION);
}


void
db_updater_send(std::string path)
{
	XapiandManager::discovery()->db_update_send(path);
}

#endif
