/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "raft.h"

#ifdef XAPIAND_CLUSTERING

#include <errno.h>                          // for errno

#include "cassert.h"                        // for ASSERT
#include "color_tools.hh"                   // for color
#include "error.hh"                         // for error:name, error::description
#include "ignore_unused.h"                  // for ignore_unused
#include "length.h"                         // for serialise_length, unserialise_length
#include "log.h"                            // for L_CALL, L_EV
#include "manager.h"                        // for XapiandManager
#include "node.h"                           // for Node::local_node, Node::leader_node
#include "opts.h"                           // for opts::*
#include "random.hh"                        // for random_real
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


Raft::Raft(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv)
	: UDP("Raft", XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION, XAPIAND_RAFT_PROTOCOL_MINOR_VERSION, UDP_SO_REUSEPORT | UDP_IP_MULTICAST_LOOP | UDP_IP_MULTICAST_TTL | UDP_IP_ADD_MEMBERSHIP),
	  Worker(parent_, ev_loop_, ev_flags_),
	  io(*ev_loop),
	  raft_leader_election_timeout(*ev_loop),
	  raft_leader_heartbeat(*ev_loop),
	  raft_request_vote_async(*ev_loop),
	  raft_add_command_async(*ev_loop),
	  raft_role(Role::RAFT_FOLLOWER),
	  raft_votes_granted(0),
	  raft_votes_denied(0),
	  raft_current_term(0),
	  raft_commit_index(0),
	  raft_last_applied(0)
{
	bind(hostname, serv, 1);

	io.set<Raft, &Raft::io_accept_cb>(this);

	raft_leader_election_timeout.set<Raft, &Raft::raft_leader_election_timeout_cb>(this);
	raft_leader_heartbeat.set<Raft, &Raft::raft_leader_heartbeat_cb>(this);

	raft_request_vote_async.set<Raft, &Raft::raft_request_vote_async_cb>(this);
	raft_request_vote_async.start();
	L_EV("Start raft's async raft_request_vote signal event");

	raft_add_command_async.set<Raft, &Raft::raft_add_command_async_cb>(this);
	raft_add_command_async.start();
	L_EV("Start discovery's async raft_add_command signal event");
}


Raft::~Raft() noexcept
{
	try {
		Worker::deinit();

		UDP::close();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
Raft::shutdown_impl(long long asap, long long now)
{
	L_CALL("Raft::shutdown_impl(%lld, %lld)", asap, now);

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
Raft::destroy_impl()
{
	L_CALL("Raft::destroy_impl()");

	Worker::destroy_impl();

	UDP::close();
}


void
Raft::start_impl()
{
	L_CALL("Raft::start_impl()");

	Worker::start_impl();

	raft_role = Role::RAFT_FOLLOWER;
	raft_voted_for.clear();
	raft_next_indexes.clear();
	raft_match_indexes.clear();

	_raft_leader_election_timeout_reset();

	io.start(sock, ev::READ);
	L_EV("Start raft's server accept event {sock:%d}", sock);

	L_RAFT("Raft was started!");
}


void
Raft::stop_impl()
{
	L_CALL("Raft::stop_impl()");

	Worker::stop_impl();

	raft_leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	raft_leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	io.stop();
	L_EV("Stop raft's server accept event");

	L_RAFT("Raft was stopped!");
}


void
Raft::operator()()
{
	L_CALL("Raft::operator()()");

	L_EV("Starting raft server loop...");
	run_loop();
	L_EV("Raft server loop ended!");

	detach();
}


void
Raft::send_message(Message type, const std::string& message)
{
	L_CALL("Raft::send_message(%s, <message>)", MessageNames(type));

	L_RAFT_PROTO("<< send_message (%s): %s", MessageNames(type), repr(message));

	UDP::send_message(toUType(type), message);
}


void
Raft::io_accept_cb(ev::io& watcher, int revents)
{
	// L_CALL("Raft::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d, state:%s}", revents, readable_revents(revents), watcher.fd, XapiandManager::StateNames(XapiandManager::state()));

	L_EV_BEGIN("Raft::io_accept_cb:BEGIN {state:%s}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Raft::io_accept_cb:END {state:%s}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(watcher);
	ASSERT(sock == -1 || sock == watcher.fd);

	if (closed) {
		return;
	}

	L_DEBUG_HOOK("Raft::io_accept_cb", "Raft::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d}", revents, readable_revents(revents), watcher.fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid raft event {sock:%d}: %s (%d): %s", watcher.fd, error::name(errno), errno, error::description(errno));
		return;
	}

	if (revents & EV_READ) {
		while (
			XapiandManager::state() == XapiandManager::State::JOINING ||
			XapiandManager::state() == XapiandManager::State::SETUP ||
			XapiandManager::state() == XapiandManager::State::READY
		) {
			try {
				std::string message;
				auto raw_type = get_message(message, static_cast<char>(Message::MAX));
				if (raw_type == '\xff') {
					break;  // no message
				}
				Message type = static_cast<Message>(raw_type);
				L_RAFT_PROTO(">> get_message (%s): %s", MessageNames(type), repr(message));
				raft_server(type, message);
			} catch (const BaseException& exc) {
				L_WARNING("WARNING: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
				break;
			}
		}
	}
}


void
Raft::raft_server(Message type, const std::string& message)
{
	L_CALL("Raft::raft_server(%s, <message>)", MessageNames(type));

	L_EV_BEGIN("Raft::raft_server:BEGIN {state:%s, type:%s}", XapiandManager::StateNames(XapiandManager::state()), MessageNames(type));
	L_EV_END("Raft::raft_server:END {state:%s, type:%s}", XapiandManager::StateNames(XapiandManager::state()), MessageNames(type));

	switch (type) {
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
		default: {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
	}
}


void
Raft::raft_request_vote(Message type, const std::string& message)
{
	L_CALL("Raft::raft_request_vote(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node);
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
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

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	auto granted = false;
	if (term == raft_current_term) {
		if (raft_voted_for.empty()) {
			if (Node::is_local(node)) {
				raft_voted_for = *node;
				L_RAFT("I vote for %s (1)", raft_voted_for.name());
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
					L_RAFT("I vote for %s (raft_log term is newer)", raft_voted_for.name());
				} else if (last_log_term == remote_last_log_term) {
					// If the logs end with the same term, then whichever
					// raft_log is longer is more up-to-date.
					if (raft_log.size() <= remote_last_log_index) {
						raft_voted_for = *node;
						L_RAFT("I vote for %s (raft_log index size concurs)", raft_voted_for.name());
					} else {
						L_RAFT("I don't vote for %s (raft_log index is shorter)", raft_voted_for.name());
					}
				} else {
					L_RAFT("I don't vote for %s (raft_log term is older)", raft_voted_for.name());
				}
			}
		} else {
			L_RAFT("I already voted for %s", raft_voted_for.name());
		}
		granted = raft_voted_for == *node;
	}

	L_RAFT("   << REQUEST_VOTE_RESPONSE {node:%s, term:%llu, granted:%s}", node->name(), term, granted ? "true" : "false");
	send_message(Message::RAFT_REQUEST_VOTE_RESPONSE,
		node->serialise() +
		serialise_length(term) +
		serialise_length(granted));
}


void
Raft::raft_request_vote_response(Message type, const std::string& message)
{
	L_CALL("Raft::raft_request_vote_response(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (raft_role != Role::RAFT_CANDIDATE) {
		return;
	}

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node);
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
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

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	if (term == raft_current_term) {
		if (Node::is_equal(node, local_node)) {
			bool granted = unserialise_length(&p, p_end);
			if (granted) {
				++raft_votes_granted;
			} else {
				++raft_votes_denied;
			}
			L_RAFT("Number of servers: %d; Votes granted: %d; Votes denied: %d", Node::active_nodes(), raft_votes_granted, raft_votes_denied);
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

					L_RAFT("   << HEARTBEAT {node:%s, term:%llu, prev_log_term:%llu, prev_log_index:%zu, raft_commit_index:%zu}",
						local_node->name(), raft_current_term, prev_log_term, prev_log_index, raft_commit_index);
					send_message(Message::RAFT_HEARTBEAT,
						local_node->serialise() +
						serialise_length(raft_current_term) +
						serialise_length(prev_log_index) +
						serialise_length(prev_log_term) +
						serialise_length(raft_commit_index));

					// First time we elect a leader's, we setup node
					auto joining = XapiandManager::State::JOINING;
					if (XapiandManager::state().compare_exchange_strong(joining, XapiandManager::State::SETUP)) {
						// L_DEBUG("Role changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
						XapiandManager::setup_node();
					}
				}
			}
		}
	}
}


void
Raft::raft_append_entries(Message type, const std::string& message)
{
	L_CALL("Raft::raft_append_entries(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node);
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
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
		if (!Node::is_equal(node, local_node)) {
			// If another leader is around, immediately run for election
			_raft_request_vote(true);
		}
		return;
	}

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

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
		// L_RAFT("   {entry_index:%zu, prev_log_index:%zu, last_index:%zu, prev_log_term:%llu}", entry_index, prev_log_index, last_index, prev_log_term);
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
					L_RAFT("committed {raft_commit_index:%zu}", raft_commit_index);

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
				auto joining = XapiandManager::State::JOINING;
				if (XapiandManager::state().compare_exchange_strong(joining, XapiandManager::State::SETUP)) {
					// L_DEBUG("Role changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::state().load()));
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
	L_RAFT("   << %s {node:%s, term:%llu, success:%s}",
		MessageNames(response_type), local_node->name(), term, success ? "true" : "false");
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
		L_RAFT_LOG("   %s raft_log[%zu] -> {term:%llu, command:%s}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
	}
#endif
}


void
Raft::raft_append_entries_response(Message type, const std::string& message)
{
	L_CALL("Raft::raft_append_entries_response(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node);
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
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

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == raft_current_term ? "" : " (wrong term)");

	if (term == raft_current_term) {
		bool success = unserialise_length(&p, p_end);
		if (success) {
			// If successful:
			// update nextIndex and matchIndex for follower
			size_t next_index = unserialise_length(&p, p_end);
			size_t match_index = unserialise_length(&p, p_end);
			raft_next_indexes[node->lower_name()] = next_index;
			raft_match_indexes[node->lower_name()] = match_index;
			L_RAFT("   {success:%s, next_index:%zu, match_index:%zu}", success ? "true" : "false", next_index, match_index);
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
			L_RAFT("   {success:%s, next_index:%zu}", success ? "true" : "false", next_index);
		}
		_raft_commit_log();

#ifdef L_RAFT_LOG
		for (size_t i = 0; i < raft_log.size(); ++i) {
			L_RAFT_LOG("%s raft_log[%zu] -> {term:%llu, command:%s}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
		}
#endif
	}
}


void
Raft::raft_add_command(Message type, const std::string& message)
{
	L_CALL("Raft::raft_add_command(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
	ignore_unused(type);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::state().load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node);
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	if (raft_role != Role::RAFT_LEADER) {
		return;
	}

	auto command = std::string(unserialise_string(&p, p_end));
	raft_add_command(command);
}


void
Raft::raft_leader_election_timeout_cb(ev::timer&, int revents)
{
	L_CALL("Raft::raft_leader_election_timeout_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::state().load()));

	L_EV_BEGIN("Raft::raft_leader_election_timeout_cb:BEGIN");
	L_EV_END("Raft::raft_leader_election_timeout_cb:END");

	ignore_unused(revents);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT("   << LEADER_ELECTION (invalid state: %s)", XapiandManager::StateNames(XapiandManager::state().load()));
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
Raft::raft_leader_heartbeat_cb(ev::timer&, int revents)
{
	// L_CALL("Raft::raft_leader_heartbeat_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::state().load()));

	L_EV_BEGIN("Raft::raft_leader_heartbeat_cb:BEGIN");
	L_EV_END("Raft::raft_leader_heartbeat_cb:END");

	ignore_unused(revents);

	if (XapiandManager::state() != XapiandManager::State::JOINING &&
		XapiandManager::state() != XapiandManager::State::SETUP &&
		XapiandManager::state() != XapiandManager::State::READY) {
		L_RAFT("   << HEARTBEAT (invalid state: %s)", XapiandManager::StateNames(XapiandManager::state().load()));
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
			L_RAFT("   << APPEND_ENTRIES {raft_current_term:%llu, prev_log_index:%zu, prev_log_term:%llu, last_log_index:%zu, entry_term:%llu, entry_command:%s, raft_commit_index:%zu}",
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
	L_RAFT("   << HEARTBEAT {last_log_term:%llu, last_log_index:%zu, raft_commit_index:%zu}", last_log_term, last_log_index, raft_commit_index);
	send_message(Message::RAFT_HEARTBEAT,
		local_node->serialise() +
		serialise_length(raft_current_term) +
		serialise_length(last_log_index) +
		serialise_length(last_log_term) +
		serialise_length(raft_commit_index));
}


void
Raft::_raft_leader_heartbeat_start(double min, double max)
{
	L_CALL("Raft::_raft_leader_heartbeat_start()");

	raft_leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	raft_leader_heartbeat.repeat = random_real(min, max);
	raft_leader_heartbeat.again();
	L_EV("Restart raft's leader heartbeat event (%g)", raft_leader_heartbeat.repeat);
}


void
Raft::_raft_leader_election_timeout_reset(double min, double max)
{
	L_CALL("Raft::_raft_leader_election_timeout_reset(%g, %g)", min, max);

	raft_leader_election_timeout.repeat = random_real(min, max);
	raft_leader_election_timeout.again();
	L_EV("Restart raft's leader election timeout event (%g)", raft_leader_election_timeout.repeat);

	raft_leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");
}


void
Raft::_raft_set_leader_node(const std::shared_ptr<const Node>& node)
{
	L_CALL("Raft::_raft_set_leader_node(%s)", repr(node->name()));

	auto leader_node = Node::leader_node();
	L_CALL("leader_node -> %s", leader_node->__repr__());
	if (!Node::is_equal(node, leader_node)) {
		Node::leader_node(node);
		XapiandManager::new_leader();
	}
}


void
Raft::_raft_apply(const std::string& command)
{
	L_CALL("Raft::_raft_apply(%s)", repr(command));

	const char *p = command.data();
	const char *p_end = p + command.size();

	size_t idx = unserialise_length(&p, p_end);
	auto node_name = unserialise_string(&p, p_end);

	auto node = Node::get_node(node_name);
	if (node) {
		auto node_copy = std::make_unique<Node>(*node);
		node_copy->idx = idx;
		node = std::shared_ptr<const Node>(node_copy.release());
	} else {
		auto node_copy = std::make_unique<Node>();
		node_copy->name(std::string(node_name));
		node_copy->idx = idx;
		node = std::shared_ptr<const Node>(node_copy.release());
	}

	auto put = Node::put_node(node, false);

	if (put.first == nullptr) {
		L_DEBUG("Denied node: %s[%zu] %s", node->col().ansi(), node->idx, node->name());
	} else {
		node = put.first;
		L_DEBUG("Added node: %s[%zu] %s", node->col().ansi(), node->idx, node->name());
	}
}


void
Raft::_raft_commit_log()
{
	L_CALL("Raft::_raft_commit_log()");

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
				L_RAFT("committed {matches:%zu, active_nodes:%zu, raft_commit_index:%zu}",
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
				L_RAFT("not committed {matches:%zu, active_nodes:%zu, raft_commit_index:%zu}",
					matches, Node::active_nodes(), raft_commit_index);
			}
		}
	}
}


void
Raft::_raft_request_vote(bool immediate)
{
	L_CALL("Raft::_raft_request_vote(%s)", immediate ? "true" : "false");

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
		L_RAFT("   << REQUEST_VOTE { node:%s, term:%llu, last_log_term:%llu, last_log_index:%zu, state:%s, timeout:%f, active_nodes:%zu, leader:%s }",
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
Raft::raft_request_vote()
{
	L_CALL("Raft::raft_request_vote()");

	raft_request_vote_async.send();
}


void
Raft::raft_request_vote_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::raft_request_vote_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	L_EV_BEGIN("Raft::raft_request_vote_async_cb:BEGIN {state:%s}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Raft::raft_request_vote_async_cb:END {state:%s}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

	_raft_request_vote(false);
}


void
Raft::_raft_add_command(const std::string& command)
{
	L_CALL("Raft::_raft_add_command(%s)", repr(command));

	if (raft_role == Role::RAFT_LEADER) {
		raft_log.push_back({
			raft_current_term,
			command,
		});

		_raft_commit_log();

#ifdef L_RAFT_LOG
		for (size_t i = 0; i < raft_log.size(); ++i) {
			L_RAFT_LOG("%s raft_log[%zu] -> {term:%llu, command:%s}", i + 1 <= raft_commit_index ? "*" : i + 1 <= raft_last_applied ? "+" : " ", i + 1, raft_log[i].term, repr(raft_log[i].command));
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
Raft::raft_add_command_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::raft_add_command_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	L_EV_BEGIN("Raft::raft_add_command_async_cb:BEGIN {state:%s}", XapiandManager::StateNames(XapiandManager::state()));
	L_EV_END("Raft::raft_add_command_async_cb:END {state:%s}", XapiandManager::StateNames(XapiandManager::state()));

	ignore_unused(revents);

	std::string command;
	while (raft_add_command_args.try_dequeue(command)) {
		_raft_add_command(command);
	}
}


void
Raft::raft_add_command(const std::string& command)
{
	L_CALL("Raft::raft_add_command(%s)", repr(command));

	raft_add_command_args.enqueue(command);

	raft_add_command_async.send();
}


std::string
Raft::getDescription() const
{
	L_CALL("Raft::getDescription()");

	return string::format("UDP %s:%d (%s v%d.%d)", addr.sin_addr.s_addr ? fast_inet_ntop4(addr.sin_addr) : "", ntohs(addr.sin_port), description, XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION, XAPIAND_RAFT_PROTOCOL_MINOR_VERSION);
}


std::string
Raft::__repr__() const
{
	return string::format("<Raft (%s) {cnt:%ld, sock:%d}%s%s%s>",
		RoleNames(raft_role),
		use_count(),
		sock,
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}

#endif
