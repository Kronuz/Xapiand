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

#include "endpoint.h"
#include "ignore_unused.h"
#include "length.h"
#include "log.h"
#include "manager.h"


using dispatch_func = void (Raft::*)(Raft::Message type, const std::string&);


static inline bool has_consensus(size_t votes) {
	auto active_nodes = Node::active_nodes.load();
	return active_nodes == 1 || votes > active_nodes / 2;
}


Raft::Raft(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: UDP(port_, "Raft", XAPIAND_RAFT_PROTOCOL_VERSION, group_),
	  Worker(parent_, ev_loop_, ev_flags_),
	  leader_election_timeout(*ev_loop),
	  leader_heartbeat(*ev_loop),
	  state(State::FOLLOWER),
	  votes_granted(0),
	  votes_denied(0),
	  current_term(0),
	  commit_index(0),
	  last_applied(0)
{
	io.set<Raft, &Raft::io_accept_cb>(this);

	leader_election_timeout.set<Raft, &Raft::leader_election_timeout_cb>(this);
	leader_heartbeat.set<Raft, &Raft::leader_heartbeat_cb>(this);
	send_missing_entries.set<Raft, &Raft::send_missing_entries_cb>(this);

	L_OBJ("CREATED RAFT CONSENSUS");
}


Raft::~Raft()
{
	destroyer();

	L_OBJ("DELETED RAFT CONSENSUS");
}


void
Raft::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("Raft::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
	}
}


void
Raft::destroy_impl()
{
	destroyer();
}


void
Raft::destroyer()
{
	L_CALL("Raft::destroyer()");

	leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	send_missing_entries.stop();
	L_EV("Stop raft's send missing event");

	io.stop();
	L_EV("Stop raft's io event");
}


void
Raft::send_message(Message type, const std::string& message)
{
	L_CALL("Raft::send_message(%s, <message>)", MessageNames(type));

	if (type != Message::HEARTBEAT && type != Message::HEARTBEAT_RESPONSE) {
		L_RAFT_PROTO("<< send_message (%s): %s", MessageNames(type), repr(message));
	}

	UDP::send_message(toUType(type), message);
}


void
Raft::io_accept_cb(ev::io& watcher, int revents)
{
	L_CALL("Raft::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d, fd:%d}", revents, readable_revents(revents), sock, watcher.fd);

	int fd = sock;
	if (fd == -1) {
		return;
	}
	ignore_unused(watcher);
	assert(fd == watcher.fd || fd == -1);

	L_DEBUG_HOOK("Raft::io_accept_cb", "Raft::io_accept_cb(<watcher>, 0x%x (%s)) {fd:%d}", revents, readable_revents(revents), fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid raft event {fd:%d}: %s", fd, strerror(errno));
		return;
	}

	L_EV_BEGIN("Raft::io_accept_cb:BEGIN");

	if (revents & EV_READ) {
		while (
			XapiandManager::manager->state == XapiandManager::State::JOINING ||
			XapiandManager::manager->state == XapiandManager::State::SETUP ||
			XapiandManager::manager->state == XapiandManager::State::READY
		) {
			try {
				std::string message;
				auto raw_type = get_message(message, static_cast<char>(Message::MAX));
				if (raw_type == '\xff') {
					break;  // no message
				}
				Message type = static_cast<Message>(raw_type);
				if (type != Message::HEARTBEAT && type != Message::HEARTBEAT_RESPONSE) {
					L_RAFT_PROTO(">> get_message (%s): %s", MessageNames(type), repr(message));
				}
				raft_server(type, message);
			} catch (const BaseException& exc) {
				L_WARNING("WARNING: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
				break;
			} catch (...) {
				L_EV_END("Raft::io_accept_cb:END %lld", SchedulerQueue::now);
				throw;
			}
		}
	}

	L_EV_END("Raft::io_accept_cb:END %lld", SchedulerQueue::now);
}


void
Raft::raft_server(Message type, const std::string& message)
{
	L_CALL("Raft::raft_server(%s, <message>)", MessageNames(type));

	static const dispatch_func dispatch[] = {
		&Raft::append_entries,
		&Raft::append_entries_response,
		&Raft::append_entries,
		&Raft::append_entries_response,
		&Raft::request_vote,
		&Raft::request_vote_response,
		&Raft::add_command,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[toUType(type)]))(type, message);
}


void
Raft::request_vote(Message type, const std::string& message)
{
	L_CALL("Raft::request_vote(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	auto local_node_ = Node::local_node();
	auto node = Node::touch_node(remote_node.name());
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > current_term) {
		// set currentTerm = T,
		current_term = term;
		// convert to follower
		state = State::FOLLOWER;
		voted_for.clear();
		next_indexes.clear();
		match_indexes.clear();

		_reset_leader_election_timeout();
		_set_master_node(node);
	}

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == current_term ? "" : " (wrong term)");

	auto granted = false;
	if (term == current_term) {
		if (voted_for.empty()) {
			if (*node == *local_node_) {
				voted_for = *node;
				L_RAFT("I vote for %s (1)", voted_for.name());
			} else if (state == State::FOLLOWER) {
				uint64_t remote_last_log_term = unserialise_length(&p, p_end);
				size_t remote_last_log_index = unserialise_length(&p, p_end);
				// §5.4.1
				auto last_log_index = log.size();
				auto last_log_term = last_log_index > 0 ? log[last_log_index - 1].term : 0;
				if (last_log_term < remote_last_log_term) {
					// If the logs have last entries with different terms, then the
					// log with the later term is more up-to-date.
					voted_for = *node;
					L_RAFT("I vote for %s (2)", voted_for.name());
				} else if (last_log_term == remote_last_log_term) {
					// If the logs end with the same term, then whichever
					// log is longer is more up-to-date.
					if (log.size() < remote_last_log_index) {
						voted_for = *node;
						L_RAFT("I vote for %s (3)", voted_for.name());
					}
				}
			}
		}
		granted = voted_for == *node;
	}

	send_message(Message::REQUEST_VOTE_RESPONSE,
		node->serialise() +
		serialise_length(term) +
		serialise_length(granted));
}


void
Raft::request_vote_response(Message type, const std::string& message)
{
	L_CALL("Raft::request_vote_response(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (state != State::CANDIDATE) {
		return;
	}

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	auto local_node_ = Node::local_node();
	auto node = Node::touch_node(remote_node.name());
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > current_term) {
		// set currentTerm = T,
		current_term = term;
		// convert to follower
		state = State::FOLLOWER;
		voted_for.clear();
		next_indexes.clear();
		match_indexes.clear();

		_reset_leader_election_timeout();
		_set_master_node(node);
	}

	L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == current_term ? "" : " (wrong term)");

	if (term == current_term) {
		if (*node == *local_node_) {
			bool granted = unserialise_length(&p, p_end);
			if (granted) {
				++votes_granted;
			} else {
				++votes_denied;
			}
			L_RAFT("Number of servers: %d; Votes granted: %d; Votes denied: %d", Node::active_nodes, votes_granted, votes_denied);
			if (has_consensus(votes_granted + votes_denied)) {
				if (votes_granted > votes_denied) {
					state = State::LEADER;
					voted_for.clear();
					next_indexes.clear();
					match_indexes.clear();

					_start_leader_heartbeat();
					_set_master_node(node);

					auto entry_index = log.size();
					auto prev_log_index = entry_index - 1;
					auto prev_log_term = entry_index > 1 ? log[prev_log_index - 1].term : 0;

					// L_RAFT("   << HEARTBEAT {prev_log_term:%llu, prev_log_index:%zu, commit_index:%zu}", prev_log_term, prev_log_index, commit_index);
					send_message(Message::HEARTBEAT,
						local_node_->serialise() +
						serialise_length(current_term) +
						serialise_length(prev_log_index) +
						serialise_length(prev_log_term) +
						serialise_length(commit_index));
				}
			}
		}
	}
}


void
Raft::append_entries(Message type, const std::string& message)
{
	L_CALL("Raft::append_entries(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		if (type != Message::HEARTBEAT) {
			L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
		}
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	auto local_node_ = Node::local_node();
	auto node = Node::touch_node(remote_node.name());
	if (!node) {
		if (type != Message::HEARTBEAT) {
			L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		}
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > current_term) {
		// set currentTerm = T,
		current_term = term;
		// convert to follower
		state = State::FOLLOWER;
		voted_for.clear();
		next_indexes.clear();
		match_indexes.clear();

		// _reset_leader_election_timeout();  // resetted below!
		// _set_master_node(node);
	}

	if (state == State::LEADER) {
		return;
	}

	if (type != Message::HEARTBEAT) {
		L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == current_term ? "" : " (wrong term)");
	}

	size_t next_index;
	size_t match_index;
	bool succeeded = false;

	if (term == current_term) {
		size_t prev_log_index = unserialise_length(&p, p_end);
		uint64_t prev_log_term = unserialise_length(&p, p_end);

		if (state == State::CANDIDATE) {
			// If AppendEntries RPC received from new leader:
			// convert to follower
			state = State::FOLLOWER;
			voted_for.clear();
			next_indexes.clear();
			match_indexes.clear();
		}

		_reset_leader_election_timeout();
		_set_master_node(node);

		// Reply false if log doesn’t contain an entry at
		// prevLogIndex whose term matches prevLogTerm
		auto last_index = log.size();
		auto entry_index = prev_log_index + 1;
		// if (type != Message::HEARTBEAT) {
		// 	for (size_t i = 0; i < log.size(); ++i) {
		// 		L_DEBUG("   %s log[%zu] -> {term:%llu, command:%s}", i + 1 <= commit_index ? "*" : i + 1 <= last_applied ? "+" : " ", i + 1, log[i].term, repr(log[i].command));
		// 	}
		// 	L_DEBUG("   {prev_log_index:%zu, last_index:%zu, prev_log_term:%llu}", prev_log_index, last_index, prev_log_term);
		// }
		if (entry_index <= 1 || (prev_log_index <= last_index && log[prev_log_index - 1].term == prev_log_term)) {
			if (type == Message::APPEND_ENTRIES) {
				uint64_t entry_term = unserialise_length(&p, p_end);
				auto entry_command = unserialise_string(&p, p_end);
				if (entry_index <= last_index) {
					if (entry_index > 1 && log[prev_log_index - 1].term != entry_term) {
						// If an existing entry conflicts with a new one (same
						// index but different terms),
						// delete the existing entry and all that follow it
						// and append new entries
						log.resize(entry_index);
						log.push_back({
							entry_term,
							std::string(entry_command),
						});
						last_index = log.size();
					} else {
						// If a valid existing entry already exists,
						// just ignore the message
						return;
					}
				} else {
					// Append any new entries not already in the log
					log.push_back({
						entry_term,
						std::string(entry_command),
					});
					last_index = log.size();
				}
			}

			// If leaderCommit > commitIndex,
			// set commitIndex = min(leaderCommit, index of last new entry)
			size_t leader_commit = unserialise_length(&p, p_end);
			if (leader_commit > commit_index) {
				commit_index = std::min(leader_commit, entry_index);
				L_RAFT("committed {commit_index:%zu}", commit_index);

				// If commitIndex > lastApplied:
				while (commit_index > last_applied) {
					// increment lastApplied,
					++last_applied;
					// apply log[lastApplied] to state machine
					const auto& command = log[last_applied - 1].command;
					_apply(command);
				}
			}

			next_index = last_index + 1;
			match_index = entry_index;
			succeeded = true;
		}
	}

	Message response_type;
	if (type != Message::HEARTBEAT) {
		L_RAFT("   << APPEND_ENTRIES_RESPONSE {term:%llu, success:false}", term);
		response_type = Message::APPEND_ENTRIES_RESPONSE;
	} else {
		response_type = Message::HEARTBEAT_RESPONSE;
	}
	send_message(response_type,
		local_node_->serialise() +
		serialise_length(term) +
		serialise_length(succeeded) +
		(succeeded
			? serialise_length(next_index) +
			  serialise_length(match_index)
			: ""
		));

#ifdef L_RAFT_LOG
	if (type != Message::HEARTBEAT) {
		for (size_t i = 0; i < log.size(); ++i) {
			L_RAFT_LOG("   %s log[%zu] -> {term:%llu, command:%s}", i + 1 <= commit_index ? "*" : i + 1 <= last_applied ? "+" : " ", i + 1, log[i].term, repr(log[i].command));
		}
	}
#endif
}


void
Raft::append_entries_response(Message type, const std::string& message)
{
	L_CALL("Raft::append_entries_response(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (state != State::LEADER) {
		return;
	}

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		if (type != Message::HEARTBEAT_RESPONSE) {
			L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
		}
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node.name());
	if (!node) {
		if (type != Message::HEARTBEAT_RESPONSE) {
			L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		}
		return;
	}

	// If RPC request or response contains term T > currentTerm:
	uint64_t term = unserialise_length(&p, p_end);
	if (term > current_term) {
		// set currentTerm = T,
		current_term = term;
		// convert to follower
		state = State::FOLLOWER;
		voted_for.clear();
		next_indexes.clear();
		match_indexes.clear();

		_reset_leader_election_timeout();
		_set_master_node(node);
	}

	if (type != Message::HEARTBEAT_RESPONSE) {
		L_RAFT(">> %s [from %s]%s", MessageNames(type), node->name(), term == current_term ? "" : " (wrong term)");
	}

	if (term == current_term) {
		bool success = unserialise_length(&p, p_end);
		if (success) {
			// If successful:
			// update nextIndex and matchIndex for follower
			size_t next_index = unserialise_length(&p, p_end);
			size_t match_index = unserialise_length(&p, p_end);
			next_indexes[remote_node.lower_name()] = next_index;
			match_indexes[remote_node.lower_name()] = match_index;
			if (type != Message::HEARTBEAT_RESPONSE) {
				L_RAFT("   {success:true, next_index:%zu, match_index:%zu}", next_index, match_index);
			}
		} else {
			// If AppendEntries fails because of log inconsistency:
			// decrement nextIndex and retry
			auto it = next_indexes.find(remote_node.lower_name());
			auto& next_index = it == next_indexes.end()
				? next_indexes[remote_node.lower_name()] = log.size() + 2
				: it->second;
			if (next_index > 1) {
				--next_index;
			}
			if (type != Message::HEARTBEAT_RESPONSE) {
				L_RAFT("   {success:false, next_index:%zu}", next_index);
			}
		}
		_commit_log();
		_send_missing_entries();

#ifdef L_RAFT_LOG
		if (type != Message::HEARTBEAT_RESPONSE) {
			for (size_t i = 0; i < log.size(); ++i) {
				L_RAFT_LOG("%s log[%zu] -> {term:%llu, command:%s}", i + 1 <= commit_index ? "*" : i + 1 <= last_applied ? "+" : " ", i + 1, log[i].term, repr(log[i].command));
			}
		}
#endif
	}
}


void
Raft::add_command(Message type, const std::string& message)
{
	L_CALL("Raft::add_command(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		L_RAFT(">> %s (invalid state: %s)", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	auto node = Node::touch_node(remote_node.name());
	if (!node) {
		L_RAFT(">> %s [from %s] (nonexistent node)", MessageNames(type), remote_node.name());
		return;
	}

	if (state != State::LEADER) {
		return;
	}

	auto command = std::string(unserialise_string(&p, p_end));
	add_command(command);
}


void
Raft::leader_election_timeout_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_election_timeout_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(revents);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		L_RAFT("   << LEADER_ELECTION (invalid state: %s)", XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	L_EV_BEGIN("Raft::leader_election_timeout_cb:BEGIN");

	if (state == State::LEADER) {
		// We're a leader, we shouldn't be here!
		return;
	}

	// If election timeout elapses without receiving AppendEntries
	// RPC from current leader or granting vote to candidate:
	// convert to candidate
	++current_term;
	state = State::CANDIDATE;
	voted_for.clear();
	next_indexes.clear();
	match_indexes.clear();
	votes_granted = 0;
	votes_denied = 0;

	_reset_leader_election_timeout();

	auto last_log_index = log.size();
	auto last_log_term = last_log_index > 0 ? log[last_log_index - 1].term : 0;

	auto local_node_ = Node::local_node();
	send_message(Message::REQUEST_VOTE,
		local_node_->serialise() +
		serialise_length(current_term) +
		serialise_length(last_log_term) +
		serialise_length(last_log_index));

	L_RAFT("request_vote { state:%s, timeout:%f, current_term:%llu, active_nodes:%zu, leader:%s }",
		StateNames(state), leader_election_timeout.repeat, current_term, Node::active_nodes, Node::master_node()->empty() ? "<none>" : Node::master_node()->name());

	L_EV_END("Raft::leader_election_timeout_cb:END");
}


void
Raft::leader_heartbeat_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_heartbeat_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::manager->state.load()));

	ignore_unused(revents);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		// L_RAFT("   << HEARTBEAT (invalid state: %s)", XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	L_EV_BEGIN("Raft::leader_heartbeat_cb:BEGIN");

	if (state != State::LEADER) {
		return;
	}

	auto local_node_ = Node::local_node();

	auto last_log_index = log.size();
	auto last_log_term = last_log_index > 0 ? log[last_log_index - 1].term : 0;

	// L_RAFT("   << HEARTBEAT {last_log_term:%llu, last_log_index:%zu, commit_index:%zu}", last_log_term, last_log_index, commit_index);
	send_message(Message::HEARTBEAT,
		local_node_->serialise() +
		serialise_length(current_term) +
		serialise_length(last_log_index) +
		serialise_length(last_log_term) +
		serialise_length(commit_index));

	L_EV_END("Raft::leader_heartbeat_cb:END");
}


void
Raft::send_missing_entries_cb(ev::timer&, int revents)
{
	L_CALL("Raft::send_missing_entries_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(XapiandManager::manager->state.load()));

	ignore_unused(revents);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		// L_RAFT("   << SEND_MISSING_ENTRIES (invalid state: %s)", XapiandManager::StateNames(XapiandManager::manager->state.load()));
		return;
	}

	L_EV_BEGIN("Raft::send_missing_entries_cb:BEGIN");

	if (state != State::LEADER) {
		return;
	}

	// If last log index ≥ nextIndex for a follower:
	// send AppendEntries RPC with log entries starting at nextIndex
	auto last_log_index = log.size();
	if (last_log_index > 0) {
		auto entry_index = last_log_index + 1;
		for (const auto& next_index_pair : next_indexes) {
			if (entry_index > next_index_pair.second) {
				entry_index = next_index_pair.second;
			}
		}
		if (entry_index > 0 && entry_index <= last_log_index) {
			auto local_node_ = Node::local_node();
			auto prev_log_index = entry_index - 1;
			auto prev_log_term = entry_index > 1 ? log[prev_log_index - 1].term : 0;
			auto entry_term = log[entry_index - 1].term;
			auto entry_command = log[entry_index - 1].command;
			L_RAFT("   << APPEND_ENTRIES {prev_log_index:%zu, prev_log_term:%llu, entry_term:%llu, entry_command:%s, commit_index:%zu}",
				prev_log_index, prev_log_term, entry_term, repr(entry_command), commit_index);
			send_message(Message::APPEND_ENTRIES,
				local_node_->serialise() +
				serialise_length(current_term) +
				serialise_length(prev_log_index) +
				serialise_length(prev_log_term) +
				serialise_length(entry_term) +
				serialise_string(entry_command) +
				serialise_length(commit_index));
		}
	}

	L_EV_END("Raft::send_missing_entries_cb:END");
}


void
Raft::_start_leader_heartbeat(double min, double max)
{
	L_CALL("Raft::_start_leader_heartbeat()");

	leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	leader_heartbeat.repeat = random_real(min, max);
	leader_heartbeat.again();
	L_EV("Restart raft's leader heartbeat event (%g)", leader_heartbeat.repeat);
}


void
Raft::_reset_leader_election_timeout(double min, double max)
{
	L_CALL("Raft::_reset_leader_election_timeout(%g, %g)", min, max);

	leader_election_timeout.repeat = random_real(min, max);
	leader_election_timeout.again();
	L_EV("Restart raft's leader election timeout event (%g)", leader_election_timeout.repeat);

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");
}


void
Raft::_set_master_node(const std::shared_ptr<const Node>& node)
{
	L_CALL("Raft::_set_master_node(%s)", repr(node->name()));

	auto master_node_ = Node::master_node();
	if (*master_node_ != *node) {
		if (master_node_->empty()) {
			L_NOTICE("Raft: Leader is %s", node->name());
		} else {
			L_NOTICE("Raft: New leader is %s", node->name());
		}
		Node::master_node(node);
		auto joining = XapiandManager::State::JOINING;
		if (XapiandManager::manager->state.compare_exchange_strong(joining, XapiandManager::State::SETUP)) {
			XapiandManager::manager->setup_node();
		}
	}
}


void
Raft::_apply(const std::string& command)
{
	L_CALL("Raft::_apply(%s)", repr(command));

	L_RED("APPLY: %s", repr(command));
}


void
Raft::_send_missing_entries()
{
	L_CALL("Raft::_send_missing_entries()");

	send_missing_entries.start(SEND_MISSING_ENTRIES_TIMEOUT);
}


void
Raft::_commit_log()
{
	L_CALL("Raft::_commit_log()");

	// If there exists an N such that N > commitIndex,
	// a majority of matchIndex[i] ≥ N,
	// and log[N].term == currentTerm:
	// set commitIndex = N
	for (size_t index = commit_index + 1; index <= log.size(); ++index) {
		if (log[index - 1].term == current_term) {
			size_t matches = 0;
			for (const auto& match_index_pair : match_indexes) {
				if (match_index_pair.second >= index) {
					++matches;
				}
			}
			if (has_consensus(matches)) {
				commit_index = index;
				L_RAFT("committed {matches:%zu, active_nodes:%zu, commit_index:%zu}",
					matches, Node::active_nodes, commit_index);

				// If commitIndex > lastApplied:
				while (commit_index > last_applied) {
					// increment lastApplied,
					++last_applied;
					// apply log[lastApplied] to state machine
					const auto& command = log[last_applied - 1].command;
					_apply(command);
				}
			}
		}
	}
}


void
Raft::add_command(const std::string& command)
{
	L_CALL("Raft::add_command(%s)", repr(command));

	if (state == State::LEADER) {
		log.push_back({
			current_term,
			command,
		});

		_commit_log();
		_send_missing_entries();

#ifdef L_RAFT_LOG
		for (size_t i = 0; i < log.size(); ++i) {
			L_RAFT_LOG("%s log[%zu] -> {term:%llu, command:%s}", i + 1 <= commit_index ? "*" : i + 1 <= last_applied ? "+" : " ", i + 1, log[i].term, repr(log[i].command));
		}
#endif
	} else {
		auto local_node_ = Node::local_node();
		send_message(Message::ADD_COMMAND,
			local_node_->serialise() +
			serialise_string(command));
	}
}


void
Raft::request_vote()
{
	L_CALL("Raft::request_vote()");

	state = State::FOLLOWER;
	voted_for.clear();
	next_indexes.clear();
	match_indexes.clear();

	_reset_leader_election_timeout(0, LEADER_ELECTION_MAX - LEADER_ELECTION_MIN);
}


void
Raft::start()
{
	L_CALL("Raft::start()");

	state = State::FOLLOWER;
	voted_for.clear();
	next_indexes.clear();
	match_indexes.clear();

	_reset_leader_election_timeout();

	io.start(sock, ev::READ);
	L_EV("Start raft's server accept event (sock=%d)", sock);

	L_RAFT("Raft was started!");
}


void
Raft::stop()
{
	L_CALL("Raft::stop()");

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	leader_election_timeout.stop();
	L_EV("Stop raft's leader election timeout event");

	io.stop();
	L_EV("Stop raft's server accept event");

	L_RAFT("Raft was stopped!");
}


std::string
Raft::getDescription() const noexcept
{
	L_CALL("Raft::getDescription()");

	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
