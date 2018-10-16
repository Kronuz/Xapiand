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


using dispatch_func = void (Raft::*)(const std::string&);


Raft::Raft(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: UDP(port_, "Raft", XAPIAND_RAFT_PROTOCOL_VERSION, group_),
	  Worker(parent_, ev_loop_, ev_flags_),
	  leader_election_timeout(*ev_loop),
	  leader_heartbeat(*ev_loop),
	  state(State::FOLLOWER),
	  votes_granted(0),
	  votes_denied(0),
	  num_servers(1),
	  current_term(0),
	  commit_index(0),
	  last_applied(0)
{
	io.set<Raft, &Raft::io_accept_cb>(this);

	leader_election_timeout.set<Raft, &Raft::leader_election_timeout_cb>(this);
	leader_heartbeat.set<Raft, &Raft::leader_heartbeat_cb>(this);

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

	io.stop();
	L_EV("Stop raft's io event");
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
				L_RAFT_PROTO(">> get_message (%s): %s", MessageNames(type), repr(message));
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
		&Raft::request_vote,
		&Raft::request_vote_response,
		&Raft::append_entries,
		&Raft::append_entries_response,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[toUType(type)]))(message);
}


void
Raft::request_vote(const std::string& message)
{
	L_CALL("Raft::request_vote(<message>) {state:%s}", XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node candidate_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (local_node_->region != candidate_node.region) {
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

		_reset_leader_election_timeout();
		_set_master_node(candidate_node);
	}

	L_RAFT(">> REQUEST_VOTE [%s]", candidate_node.name());

	auto granted = false;
	if (term == current_term) {
		if (voted_for.empty()) {
			if (candidate_node == *local_node_) {
				voted_for = candidate_node;
				L_RAFT("I vote for %s (1)", voted_for.name());
			} else if (state == State::FOLLOWER) {
				size_t remote_last_log_index = unserialise_length(&p, p_end);
				uint64_t remote_last_log_term = unserialise_length(&p, p_end);
				// §5.4.1
				auto last_log_term = log.empty() ? 0 : log.back().term;
				if (last_log_term < remote_last_log_term) {
					// If the logs have last entries with different terms, then the
					// log with the later term is more up-to-date.
					voted_for = candidate_node;
					L_RAFT("I vote for %s (2)", voted_for.name());
				} else if (last_log_term == remote_last_log_term) {
					// If the logs end with the same term, then whichever
					// log is longer is more up-to-date.
					if (log.size() < remote_last_log_index) {
						voted_for = candidate_node;
						L_RAFT("I vote for %s (3)", voted_for.name());
					}
				}
			}
		}
		granted = voted_for == candidate_node;
	}

	send_message(Message::REQUEST_VOTE_RESPONSE,
		candidate_node.serialise() +
		serialise_length(term) +
		serialise_length(granted));
}


void
Raft::request_vote_response(const std::string& message)
{
	L_CALL("Raft::request_vote_response(<message>) {state:%s}", XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node candidate_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (local_node_->region != candidate_node.region) {
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

		_reset_leader_election_timeout();
		_set_master_node(candidate_node);
	}

	if (state != State::CANDIDATE) {
		return;
	}

	L_RAFT(">> REQUEST_VOTE_RESPONSE [%s]", candidate_node.name());

	if (term == current_term) {
		if (candidate_node == *local_node_) {
			bool granted = unserialise_length(&p, p_end);
			if (granted) {
				++votes_granted;
			} else {
				++votes_denied;
			}
			L_RAFT("Number of servers: %d; Votes granted: %d; Votes denied: %d", num_servers.load(), votes_granted, votes_denied);
			if (votes_granted + votes_denied > num_servers / 2) {
				if (votes_granted > votes_denied) {
					state = State::LEADER;
					voted_for.clear();

					_start_leader_heartbeat();
					_set_master_node(candidate_node);

					send_message(Message::APPEND_ENTRIES,
						local_node_->serialise() +
						serialise_length(current_term) +
						serialise_length(log.size()) +
						serialise_length(log.empty() ? 0 : log.back().term) +
						serialise_string("") +
						serialise_length(commit_index));
				}
			}
		}
	}
}


void
Raft::append_entries(const std::string& message)
{
	L_CALL("Raft::append_entries(<message>) {state:%s}", XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (local_node_->region != remote_node.region) {
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

		// _reset_leader_election_timeout();  // resetted below!
		// _set_master_node(remote_node);
	}

	if (state != State::FOLLOWER) {
		return;
	}

	L_RAFT(">> APPEND_ENTRIES [%s]", remote_node.name());

	auto success = false;
	if (term == current_term) {
		_reset_leader_election_timeout();
		_set_master_node(remote_node);

		// If commitIndex > lastApplied:
		if (commit_index > last_applied) {
			// increment lastApplied,
			++last_applied;
			// apply log[lastApplied] to state machine
			_apply(last_applied);
		}

		// ...
	}

	send_message(Message::APPEND_ENTRIES_RESPONSE,
		local_node_->serialise() +
		serialise_length(term) +
		serialise_length(success));
}


void
Raft::append_entries_response(const std::string& message)
{
	L_CALL("Raft::append_entries_response(<message>) {state:%s}", XapiandManager::StateNames(XapiandManager::manager->state.load()));

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node candidate_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (local_node_->region != candidate_node.region) {
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

		_reset_leader_election_timeout();
		_set_master_node(candidate_node);
	}

	if (state != State::LEADER) {
		return;
	}

	L_RAFT(">> APPEND_ENTRIES_RESPONSE [%s]", candidate_node.name());

	if (term == current_term) {
		bool success = unserialise_length(&p, p_end);
		if (success) {
			// ...
		}
	}
}

void
Raft::leader_election_timeout_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_election_timeout_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));
	ignore_unused(revents);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	L_EV_BEGIN("Raft::leader_election_timeout_cb:BEGIN");

	if (state == State::LEADER) {
		// We're a leader, we shouldn't be here!
		return;
	}

	// If election timeout elapses without receiving AppendEntries
	// RPC from current leader or granting vote to candidate: convert to candidate
	if (!voted_for.empty()) {
		return;
	}

	++current_term;
	state = State::CANDIDATE;
	voted_for.clear();
	votes_granted = 0;
	votes_denied = 0;

	_reset_leader_election_timeout();

	auto local_node_ = local_node.load();
	send_message(Message::REQUEST_VOTE,
		local_node_->serialise() +
		serialise_length(current_term) +
		serialise_length(log.size()) +
		serialise_length(log.empty() ? 0 : log.back().term));

	L_RAFT("request_vote { region: %d; state: %s; timeout: %f; current_term: %llu; num_servers: %zu; leader: %s }",
		local_node_->region, StateNames(state), leader_election_timeout.repeat, current_term, num_servers, master_node.load()->empty() ? "<none>" : master_node.load()->name());

	L_EV_END("Raft::leader_election_timeout_cb:END");
}


void
Raft::leader_heartbeat_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_heartbeat_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	L_EV_BEGIN("Raft::leader_heartbeat_cb:BEGIN");

	if (state != State::LEADER) {
		return;
	}

	auto local_node_ = local_node.load();
	send_message(Message::APPEND_ENTRIES,
		local_node_->serialise() +
		serialise_length(current_term) +
		serialise_length(log.size()) +
		serialise_length(log.empty() ? 0 : log.back().term) +
		serialise_string("") +
		serialise_length(commit_index));

	L_EV_END("Raft::leader_heartbeat_cb:END");
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
	L_CALL("Raft::_reset_leader_election_timeout()");

	auto local_node_ = local_node.load();
	num_servers = XapiandManager::manager->get_nodes_by_region(local_node_->region) + 1;

	leader_election_timeout.repeat = random_real(min, max);
	leader_election_timeout.again();
	L_EV("Restart raft's leader election timeout event (%g)", leader_election_timeout.repeat);

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");
}


void
Raft::_set_master_node(const Node& remote_node)
{
	L_CALL("Raft::_set_master_node(%s)", repr(remote_node.name()));

	auto node = XapiandManager::manager->touch_node(remote_node.name(), remote_node.region);

	if (node) {
		auto master_node_ = master_node.load();
		if (*master_node_ != *node) {
			if (master_node_->empty()) {
				L_NOTICE("Raft: Leader for region %d is %s", node->region, node->name());
			} else {
				L_NOTICE("Raft: New leader for region %d is %s", node->region, node->name());
			}
			master_node = node;
			auto joining = XapiandManager::State::JOINING;
			if (XapiandManager::manager->state.compare_exchange_strong(joining, XapiandManager::State::SETUP)) {
				XapiandManager::manager->setup_node();
			}
		}
	}
}


void
Raft::_apply(size_t idx)
{
	L_CALL("Raft::_apply()");
	ignore_unused(idx);
}


void
Raft::request_vote()
{
	L_CALL("Raft::request_vote()");

	auto local_node_ = local_node.load();
	num_servers = XapiandManager::manager->get_nodes_by_region(local_node_->region) + 1;

	state = State::FOLLOWER;
	voted_for.clear();

	_reset_leader_election_timeout(0, LEADER_ELECTION_MAX - LEADER_ELECTION_MIN);
}


void
Raft::start()
{
	L_CALL("Raft::start()");

	auto local_node_ = local_node.load();
	num_servers = XapiandManager::manager->get_nodes_by_region(local_node_->region) + 1;

	state = State::FOLLOWER;
	voted_for.clear();

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
