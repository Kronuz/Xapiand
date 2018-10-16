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
	  reset_leader_election_timeout_async(*ev_loop),
	  state(State::FOLLOWER),
	  votes(0),
	  current_term(0),
	  commit_index(0),
	  last_applied(0),
	  number_servers(1)
{
	io.set<Raft, &Raft::io_accept_cb>(this);

	leader_election_timeout.set<Raft, &Raft::leader_election_timeout_cb>(this);
	leader_heartbeat.set<Raft, &Raft::leader_heartbeat_cb>(this);

	start_leader_heartbeat_async.set<Raft, &Raft::start_leader_heartbeat_async_cb>(this);
	reset_leader_election_timeout_async.set<Raft, &Raft::reset_leader_election_timeout_async_cb>(this);

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
	L_CALL("Raft::request_vote(<message>)");
	ignore_unused(message);

	ignore_unused(commit_index);
	ignore_unused(last_applied);

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_RAFT(">> REQUEST_VOTE [%s]", remote_node.name());

	auto local_node_ = local_node.load();
	if (local_node_->region != remote_node.region) {
		return;
	}

	uint64_t term = unserialise_length(&p, p_end);
	if (term > current_term) {
		state = Raft::State::FOLLOWER;
	}

	// if (term >= current_term) {
	// 	size_t last_log_index = unserialise_length(&p, p_end);
	// 	uint64_t last_log_term = unserialise_length(&p, p_end);
	// }


	// L_RAFT("term: %llu  local_term: %llu", term, current_term);

	// if (term > current_term) {
	// 	if (state == Raft::State::LEADER && remote_node != *local_node_) {
	// 		L_ERR("ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, this node will reset!", remote_node.name(), term, current_term);
	// 		reset();
	// 	}

	// 	voted_for = remote_node;
	// 	current_term = term;

	// 	L_RAFT("It Vote for %s", voted_for.name());
	// 	send_message(Message::RESPONSE_VOTE, remote_node.serialise() +
	// 		serialise_length(true) + serialise_length(term));
	// } else {
	// 	if (state == Raft::State::LEADER && remote_node != *local_node_) {
	// 		L_ERR("ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, remote node will reset!", remote_node.name(), term, current_term);
	// 		send_message(Message::RESET, remote_node.serialise());
	// 		return;
	// 	}

	// 	if (term < current_term) {
	// 		L_RAFT("Vote for %s", voted_for.name());
	// 		send_message(Message::RESPONSE_VOTE, remote_node.serialise() +
	// 			serialise_length(false) + serialise_length(current_term));
	// 	} else if (voted_for.empty()) {
	// 		voted_for = remote_node;
	// 		L_RAFT("Vote for %s", voted_for.name());
	// 		send_message(Message::RESPONSE_VOTE, remote_node.serialise() +
	// 			serialise_length(true) + serialise_length(current_term));
	// 	} else {
	// 		L_RAFT("Vote for %s", voted_for.name());
	// 		send_message(Message::RESPONSE_VOTE, remote_node.serialise() +
	// 			serialise_length(false) + serialise_length(current_term));
	// 	}
	// }
}


void
Raft::request_vote_response(const std::string& message)
{
	L_CALL("Raft::request_vote_response(<message>)");
	ignore_unused(message);

	// const char *p = message.data();
	// const char *p_end = p + message.size();

	// Node remote_node = Node::unserialise(&p, p_end);
	// L_RAFT(">> REQUEST_VOTE_RESPONSE [%s]", remote_node.name());

	// auto local_node_ = local_node.load();
	// if (local_node_->region != remote_node.region) {
	// 	return;
	// }

	// if (remote_node == *local_node_ && state == Raft::State::CANDIDATE) {
	// 	bool vote = unserialise_length(&p, p_end);

	// 	if (vote) {
	// 		++votes;
	// 		L_RAFT("Number of servers: %d; Votes received: %d", number_servers.load(), votes);
	// 		if (votes > number_servers / 2) {
	// 			state = Raft::State::LEADER;
	// 			start_leader_heartbeat();

	// 			auto master_node_ = master_node.load();
	// 			if (*master_node_ != *local_node_) {
	// 				if (master_node_->empty()) {
	// 					L_NOTICE("Raft: Leader for region %d is %s (me)", local_node_->region, local_node_->name());
	// 				} else {
	// 					L_NOTICE("Raft: New leader for region %d is %s (me)", local_node_->region, local_node_->name());
	// 				}
	// 				master_node = local_node_;
	// 				auto joining = XapiandManager::State::JOINING;
	// 				XapiandManager::manager->state.compare_exchange_strong(joining, XapiandManager::State::SETUP);
	// 				XapiandManager::manager->setup_node();
	// 			}
	// 		}
	// 		return;
	// 	}

	// 	uint64_t term = unserialise_length(&p, p_end);
	// 	if (current_term < term) {
	// 		current_term = term;
	// 		state = Raft::State::FOLLOWER;
	// 		reset_leader_election_timeout();
	// 		return;
	// 	}
	// }
}


void
Raft::append_entries(const std::string& message)
{
	L_CALL("Raft::append_entries(<message>)");
	ignore_unused(message);

	// const char *p = message.data();
	// const char *p_end = p + message.size();

	// std::shared_ptr<const Node> remote_node = std::make_shared<Node>(Node::unserialise(&p, p_end));
	// L_RAFT(">> APPEND_ENTRIES [%s]", remote_node->name());

	// auto local_node_ = local_node.load();
	// if (local_node_->region != remote_node->region) {
	// 	return;
	// }

	// if (state == Raft::State::LEADER) {
	// 	if (*remote_node != *local_node_) {
	// 		L_CRIT("I'm leader, other responded as leader!");
	// 		reset();
	// 	}
	// 	return;
	// }

	// number_servers.store(unserialise_length(&p, p_end));
	// current_term = unserialise_length(&p, p_end);
	// state = Raft::State::FOLLOWER;
	// reset_leader_election_timeout();

	// auto master_node_ = master_node.load();
	// if (*master_node_ != *remote_node) {
	// 	if (master_node_->empty()) {
	// 		L_NOTICE("Raft: Leader for region %d is %s", local_node_->region, remote_node->name());
	// 	} else {
	// 		L_NOTICE("Raft: New leader for region %d is %s", local_node_->region, remote_node->name());
	// 	}
	// 	auto put = XapiandManager::manager->put_node(remote_node);
	// 	remote_node = put.first;
	// 	if (put.second) {
	// 		L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (5)", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
	// 	}
	// 	master_node = remote_node;
	// 	auto joining = XapiandManager::State::JOINING;
	// 	XapiandManager::manager->state.compare_exchange_strong(joining, XapiandManager::State::SETUP);
	// 	XapiandManager::manager->setup_node();
	// }
}


void
Raft::append_entries_response(const std::string& message)
{
	L_CALL("Raft::append_entries_response(<message>)");
	ignore_unused(message);

	// const char *p = message.data();
	// const char *p_end = p + message.size();

	// Node remote_node = Node::unserialise(&p, p_end);
	// L_RAFT(">> APPEND_ENTRIES_RESPONSE [%s]", remote_node.name());

	// auto local_node_ = local_node.load();
	// if (local_node_->region != remote_node.region) {
	// 	return;
	// }

	// if (state == Raft::State::LEADER) {
	// 	L_DEBUG("Sending Data!");
	// 	send_message(Message::LEADER, local_node_->serialise() +
	// 		serialise_length(number_servers) +
	// 		serialise_length(current_term));
	// }
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

	_request_vote();

	L_EV_END("Raft::leader_election_timeout_cb:END");
}


void
Raft::leader_heartbeat_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_heartbeat_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	// ignore_unused(revents);

	// if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
	// 	XapiandManager::manager->state != XapiandManager::State::SETUP &&
	// 	XapiandManager::manager->state != XapiandManager::State::READY) {
	// 	return;
	// }

	// L_EV_BEGIN("Raft::leader_heartbeat_cb:BEGIN");

	// auto local_node_ = local_node.load();
	// send_message(Message::HEARTBEAT_LEADER, local_node_->serialise());

	// L_EV_END("Raft::leader_heartbeat_cb:END");
}


void
Raft::reset_leader_election_timeout_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::reset_leader_election_timeout_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	_reset_leader_election_timeout();
}


void
Raft::start_leader_heartbeat_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::start_leader_heartbeat_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	// ignore_unused(revents);

	// _start_leader_heartbeat();
}


void
Raft::_request_vote()
{
	L_CALL("Raft::_request_vote()");

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	if (state == State::LEADER) {
		// Already leader, shouldn't even be here!
		return;
	}

	auto local_node_ = local_node.load();
	auto master_node_ = master_node.load();
	L_RAFT("request_vote { region: %d; state: %s; timeout: %f; current_term: %llu; number_servers: %zu; leader: %s }",
		local_node_->region, StateNames(state), leader_election_timeout.repeat, current_term, number_servers, master_node_->empty() ? "<none>" : master_node_->name());

	state = State::CANDIDATE;
	_reset_leader_election_timeout();

	++current_term;
	votes = 0;
	voted_for.clear();

	// RequestVote RPC
	// Arguments:
	//   candidateId  - candidate requesting vote
	//   term         - candidate's term
	//   lastLogIndex - index of candidate’s last log entry
	//   lastLogTerm  - term of candidate’s last log entry
	send_message(Message::REQUEST_VOTE,
		local_node_->serialise() +
		serialise_length(current_term) +
		serialise_length(log.size()) +
		serialise_length(log.empty() ? 0 : log.back().term));
}


void
Raft::_start_leader_heartbeat()
{
	L_CALL("Raft::_start_leader_heartbeat()");

	// auto local_node_ = local_node.load();
	// auto master_node_ = master_node.load();
	// assert(*master_node_ == *local_node_);

	// leader_election_timeout.stop();
	// L_EV("Stop raft's leader election timeout event");

	// leader_heartbeat.repeat = random_real(HEARTBEAT_LEADER_MIN, HEARTBEAT_LEADER_MAX);
	// leader_heartbeat.again();
	// L_EV("Restart raft's leader heartbeat event (%g)", leader_heartbeat.repeat);

	// send_message(Message::LEADER,
	// 	local_node_->serialise() +
	// 	serialise_length(number_servers) +
	// 	serialise_length(current_term));
}


void
Raft::_reset_leader_election_timeout()
{
	L_CALL("Raft::_reset_leader_election_timeout()");

	auto local_node_ = local_node.load();
	number_servers = XapiandManager::manager->get_nodes_by_region(local_node_->region) + 1;

	leader_election_timeout.repeat = random_real(LEADER_ELECTION_MIN, LEADER_ELECTION_MAX);
	leader_election_timeout.again();

	L_EV("Restart raft's leader election timeout event (%g)", leader_election_timeout.repeat);
}


void
Raft::start()
{
	L_CALL("Raft::start()");

	number_servers = 1;

	state = State::FOLLOWER;
	leader_election_timeout.start(HEARTBEAT_LEADER_MAX);
	L_EV("Start raft's leader election timeout event");

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

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
