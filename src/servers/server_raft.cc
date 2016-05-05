/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "server_raft.h"

#ifdef XAPIAND_CLUSTERING

#include "server.h"

#include <assert.h>


using dispatch_func = void (RaftServer::*)(const std::string&);


RaftServer::RaftServer(const std::shared_ptr<XapiandServer>& server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const std::shared_ptr<Raft>& raft_)
	: BaseServer(server_, ev_loop_, ev_flags_),
	  raft(raft_)
{
	io.start(raft->get_socket(), ev::READ);
	L_EV(this, "Start raft's server accept event (sock=%d)", raft->get_socket());

	L_OBJ(this, "CREATED RAFT SERVER!");
}


RaftServer::~RaftServer()
{
	L_OBJ(this, "DELETED RAFT SERVER!");
}


void
RaftServer::raft_server(Raft::Message type, const std::string& message)
{
	static const dispatch_func dispatch[] = {
		&RaftServer::heartbeat_leader,
		&RaftServer::request_vote,
		&RaftServer::response_vote,
		&RaftServer::leader,
		&RaftServer::leadership,
		&RaftServer::reset,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		throw MSG_InvalidArgumentError(errmsg);
	}
	(this->*(dispatch[toUType(type)]))(message);
}


void
RaftServer::heartbeat_leader(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	raft->reset_leader_election_timeout();

	if (raft->leader != remote_node) {
		L_RAFT(this, "Request the raft server's configuration!");
		raft->send_message(Raft::Message::LEADERSHIP, local_node_->serialise());
	}
	L_RAFT_PROTO(this, "Listening %s's heartbeat in timestamp: %f!", remote_node.name.c_str(), raft->last_activity);
}


void
RaftServer::request_vote(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	uint64_t remote_term = unserialise_length(&p, p_end);

	L_RAFT(this, "remote_term: %llu  local_term: %llu", remote_term, raft->term);

	if (remote_term > raft->term) {
		if (raft->state == Raft::State::LEADER && remote_node != *local_node_) {
			L_ERR(this, "ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, this node will reset!", remote_node.name.c_str(), remote_term, raft->term);
			raft->reset();
		}

		raft->votedFor = remote_node;
		raft->term = remote_term;

		L_RAFT(this, "It Vote for %s", raft->votedFor.name.c_str());
		raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
			serialise_length(true) + serialise_length(remote_term));
	} else {
		if (raft->state == Raft::State::LEADER && remote_node != *local_node_) {
			L_ERR(this, "ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, remote node will reset!", remote_node.name.c_str(), remote_term, raft->term);
			raft->send_message(Raft::Message::RESET, remote_node.serialise());
			return;
		}

		if (remote_term < raft->term) {
			L_RAFT(this, "Vote for %s", raft->votedFor.name.c_str());
			raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
				serialise_length(false) + serialise_length(raft->term));
		} else if (raft->votedFor.empty()) {
			raft->votedFor = remote_node;
			L_RAFT(this, "Vote for %s", raft->votedFor.name.c_str());
			raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
				serialise_length(true) + serialise_length(raft->term));
		} else {
			L_RAFT(this, "Vote for %s", raft->votedFor.name.c_str());
			raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
				serialise_length(false) + serialise_length(raft->term));
		}
	}
}


void
RaftServer::response_vote(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	if (remote_node == *local_node_ && raft->state == Raft::State::CANDIDATE) {
		bool vote = unserialise_length(&p, p_end);

		if (vote) {
			++raft->votes;
			L_RAFT(this, "Number of servers: %d;  Votes received: %d", raft->number_servers.load(), raft->votes);
			if (raft->votes > raft->number_servers / 2) {
				raft->state = Raft::State::LEADER;

				if (raft->leader != *local_node_) {
					raft->leader = *local_node_;
					L_NOTICE(this, "Raft: New leader for region %d is %s (1)", local_node_->region, raft->leader.name.c_str());
				}

				raft->start_leader_heartbeat();
			}
			return;
		}

		uint64_t remote_term = unserialise_length(&p, p_end);
		if (raft->term < remote_term) {
			raft->term = remote_term;
			raft->state = Raft::State::FOLLOWER;
			return;
		}
	}
}


void
RaftServer::leader(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	if (raft->state == Raft::State::LEADER) {
		if (remote_node != *local_node_) {
			L_CRIT(this, "I'm leader, other responded as leader!");
			raft->reset();
		}
		return;
	}

	raft->state = Raft::State::FOLLOWER;
	raft->number_servers.store(unserialise_length(&p, p_end));
	raft->term = unserialise_length(&p, p_end);

	if (raft->leader != remote_node) {
		raft->leader = remote_node;
		L_NOTICE(this, "Raft: New leader for region %d is %s (2)", local_node_->region, raft->leader.name.c_str());
	}

	raft->reset_leader_election_timeout();
}


void
RaftServer::leadership(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	if (raft->state == Raft::State::LEADER) {
		L_DEBUG(this, "Sending Data!");
		raft->send_message(Raft::Message::LEADER, local_node_->serialise() +
			serialise_length(raft->number_servers) +
			serialise_length(raft->term));
	}
}


void
RaftServer::reset(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = std::atomic_load(&local_node);
	if (local_node_->region != remote_node.region) {
		return;
	}

	if (*local_node_ == remote_node) {
		raft->reset();
	}
}


void
RaftServer::io_accept_cb(ev::io& watcher, int revents)
{
	int fd = watcher.fd;
	int sock = raft->get_socket();

	L_EV_BEGIN(this, "RaftServer::io_accept_cb:BEGIN");
	if (EV_ERROR & revents) {
		L_EV(this, "ERROR: got invalid raft event (sock=%d, fd=%d): %s", sock, fd, strerror(errno));
		L_EV_END(this, "RaftServer::io_accept_cb:END");
		return;
	}

	assert(sock == fd || sock == -1);

	if (revents & EV_READ) {
		while (XapiandManager::manager->state == XapiandManager::State::READY) {
			try {
				std::string message;
				Raft::Message type = static_cast<Raft::Message>(raft->get_message(message, static_cast<char>(Raft::Message::MAX)));
				if (type != Raft::Message::HEARTBEAT_LEADER) {
					L_RAFT(this, ">> get_message(%s)", Raft::MessageNames[static_cast<int>(type)]);
				}
				L_RAFT_PROTO(this, "message: '%s'", repr(message).c_str());

				raft_server(type, message);
			} catch (const DummyException&) {
				break;  // no message
			} catch (const Exception& exc) {
				L_WARNING(this, "WARNING: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
				break;
			} catch (...) {
				L_EV_END(this, "RaftServer::io_accept_cb:END %lld", now);
				throw;
			}
		}
	}

	L_EV_END(this, "RaftServer::io_accept_cb:END %lld", now);
}

#endif
