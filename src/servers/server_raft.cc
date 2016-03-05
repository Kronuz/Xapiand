/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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


RaftServer::RaftServer(const std::shared_ptr<XapiandServer>& server_, ev::loop_ref *loop_, const std::shared_ptr<Raft>& raft_)
	: BaseServer(server_, loop_, raft_->sock),
	  raft(raft_)
{
	// accept event actually started in BaseServer::BaseServer
	L_EV(this, "Start raft's server accept event (sock=%d)", raft->sock);

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
		&RaftServer::request_data,
		&RaftServer::response_data,
		&RaftServer::reset,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		throw MSG_InvalidArgumentError(errmsg);
	}

	raft->register_activity();

	(this->*(dispatch[toUType(type)]))(message);
}


void
RaftServer::request_vote(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	uint64_t remote_term = unserialise_length(&p, p_end);

	L_RAFT(this, "remote_term: %llu  local_term: %llu", remote_term, raft->term);

	if (remote_term > raft->term) {
		if (raft->state == Raft::State::LEADER && remote_node != local_node) {
			L_ERR(this, "ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, this node will reset!", remote_node.name.c_str(), remote_term, raft->term);
			raft->reset();
		}

		raft->votedFor = lower_string(remote_node.name);
		raft->term = remote_term;

		L_RAFT(this, "It Vote for %s", raft->votedFor.c_str());
		raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
			serialise_length(true) + serialise_length(remote_term));
	} else {
		if (raft->state == Raft::State::LEADER && remote_node != local_node) {
			L_ERR(this, "ERROR: Remote node %s with term: %llu does not recognize this node with term: %llu as a leader. Therefore, remote node will reset!", remote_node.name.c_str(), remote_term, raft->term);
			raft->send_message(Raft::Message::RESET, remote_node.serialise());
			return;
		}

		if (remote_term < raft->term) {
			L_RAFT(this, "Vote for %s", raft->votedFor.c_str());
			raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
				serialise_length(false) + serialise_length(raft->term));
		} else if (raft->votedFor.empty()) {
			raft->votedFor = lower_string(remote_node.name);
			L_RAFT(this, "Vote for %s", raft->votedFor.c_str());
			raft->send_message(Raft::Message::RESPONSE_VOTE, remote_node.serialise() +
				serialise_length(true) + serialise_length(raft->term));
		} else {
			L_RAFT(this, "Vote for %s", raft->votedFor.c_str());
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

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	if (remote_node == local_node && raft->state == Raft::State::CANDIDATE) {
		bool vote = unserialise_length(&p, p_end);

		if (vote) {
			++raft->votes;
			L_RAFT(this, "Number of servers: %d;  Votes received: %d", raft->number_servers.load(), raft->votes);
			if (raft->votes > raft->number_servers / 2) {
				L_RAFT(this, "It becomes the leader for region: %d", local_node.region.load());

				raft->state = Raft::State::LEADER;
				raft->leader = lower_string(local_node.name);

				L_INFO(this, "Raft: New leader is %s (1)", raft->leader.c_str());

				raft->start_heartbeat();
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
RaftServer::heartbeat_leader(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	if (raft->leader != lower_string(remote_node.name)) {
		L_RAFT(this, "Request the raft server's configuration!");
		raft->send_message(Raft::Message::REQUEST_DATA, local_node.serialise());
	}
	L_RAFT_PROTO(this, "Listening %s's heartbeat in timestamp: %f!", remote_node.name.c_str(), raft->last_activity);
}


void
RaftServer::leader(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	if (raft->state == Raft::State::LEADER) {
		assert(remote_node == local_node);
		return;
	}

	raft->number_servers.store(unserialise_length(&p, p_end));
	raft->term = unserialise_length(&p, p_end);

	raft->leader = lower_string(remote_node.name);
	raft->state = Raft::State::FOLLOWER;

	L_INFO(this, "Raft: New leader is %s (2)", raft->leader.c_str());
}


void
RaftServer::request_data(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	if (raft->state == Raft::State::LEADER) {
		L_DEBUG(this, "Sending Data!");
		raft->send_message(Raft::Message::RESPONSE_DATA, local_node.serialise() +
			serialise_length(raft->number_servers) +
			serialise_length(raft->term));
	}
}


void
RaftServer::response_data(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	if (raft->state == Raft::State::LEADER) {
		L_CRIT(this, "I'm leader, other responded as leader!");
		raft->reset();
		return;
	}

	L_DEBUG(this, "Receiving Data!");

	raft->number_servers.store(unserialise_length(&p, p_end));
	raft->term = unserialise_length(&p, p_end);

	raft->leader = lower_string(remote_node.name);

	L_INFO(this, "Raft: New leader is %s (3)", raft->leader.c_str());
}


void
RaftServer::reset(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	if (local_node.region.load() != remote_node.region.load()) {
		return;
	}

	raft->register_activity();

	if (local_node == remote_node) {
		raft->reset();
	}
}


void
RaftServer::io_accept_cb(ev::io& watcher, int revents)
{
	L_EV_BEGIN(this, "RaftServer::io_accept_cb:BEGIN");
	if (EV_ERROR & revents) {
		L_EV(this, "ERROR: got invalid raft event (sock=%d): %s", raft->sock, strerror(errno));
		L_EV_END(this, "RaftServer::io_accept_cb:END");
		return;
	}

	assert(raft->sock == watcher.fd || raft->sock == -1);

	if (revents & EV_READ) {
		while (manager()->state == XapiandManager::State::READY) {
			try {
				std::string message;
				Raft::Message type = static_cast<Raft::Message>(raft->get_message(message, static_cast<char>(Raft::Message::MAX)));
				if (type != Raft::Message::HEARTBEAT_LEADER) {
					L_RAFT(this, ">> get_message(%s)", Raft::MessageNames[static_cast<int>(type)]);
				}
				L_RAFT_PROTO(this, "message: '%s'", repr(message).c_str());

				raft_server(type, message);
			} catch (DummyException) {
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
