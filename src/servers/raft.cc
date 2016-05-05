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

#include "raft.h"

#ifdef XAPIAND_CLUSTERING

#include "../endpoint.h"

#include <assert.h>


constexpr const char* const Raft::MessageNames[];
constexpr const char* const Raft::StateNames[];


Raft::Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: BaseUDP(manager_, ev_loop_, ev_flags_, port_, "Raft", XAPIAND_RAFT_PROTOCOL_VERSION, group_),
	  term(0),
	  votes(0),
	  state(State::FOLLOWER),
	  number_servers(1),
	  leader_election_timeout(*ev_loop),
	  leader_heartbeat(*ev_loop),
	  async_reset_leader_election_timeout(*ev_loop),
	  async_reset(*ev_loop)
{
	leader_election_timeout.set<Raft, &Raft::leader_election_timeout_cb>(this);

	leader_heartbeat.set<Raft, &Raft::leader_heartbeat_cb>(this);

	async_start_leader_heartbeat.set<Raft, &Raft::async_start_leader_heartbeat_cb>(this);
	async_start_leader_heartbeat.start();
	L_EV(this, "Start raft's async start leader heartbeat event");

	async_reset_leader_election_timeout.set<Raft, &Raft::async_reset_leader_election_timeout_cb>(this);
	async_reset_leader_election_timeout.start();
	L_EV(this, "Start raft's async reset leader election timeout event");

	async_reset.set<Raft, &Raft::async_reset_cb>(this);
	async_reset.start();
	L_EV(this, "Start raft's async reset event");

	L_OBJ(this, "CREATED RAFT CONSENSUS");
}


Raft::~Raft()
{
	leader_election_timeout.stop();
	L_EV(this, "Stop raft's leader election event");

	leader_heartbeat.stop();
	L_EV(this, "Stop raft's leader heartbeat event");

	L_OBJ(this, "DELETED RAFT CONSENSUS");
}


void
Raft::start()
{
	_reset_leader_election_timeout();

	L_RAFT(this, "Raft was started!");
}


void
Raft::stop()
{
	leader_election_timeout.stop();
	L_EV(this, "Stop raft's leader election event");

	leader_heartbeat.stop();
	L_EV(this, "Stop raft's leader heartbeat event");

	state = State::FOLLOWER;
	number_servers = 1;

	L_RAFT(this, "Raft was stopped!");
}


void
Raft::async_start_leader_heartbeat_cb(ev::async&, int)
{
	_start_leader_heartbeat();
}


void
Raft::async_reset_leader_election_timeout_cb(ev::async&, int)
{
	_reset_leader_election_timeout();
}


void
Raft::async_reset_cb(ev::async&, int)
{
	_reset();
}


void
Raft::_reset()
{
	leader_heartbeat.stop();
	L_EV(this, "Stop raft's leader heartbeat event");

	state = State::FOLLOWER;

	_reset_leader_election_timeout();

	L_RAFT(this, "Raft was restarted!");
}


void
Raft::leader_election_timeout_cb(ev::timer&, int)
{
	L_EV(this, "Raft::leader_election_timeout_cb");

	L_EV_BEGIN(this, "Raft::leader_election_timeout_cb:BEGIN");

	if (XapiandManager::manager->state != XapiandManager::State::READY) {
		L_EV_END(this, "Raft::leader_election_timeout_cb:END");
		return;
	}

	auto local_node_ = std::atomic_load(&local_node);
	L_RAFT_PROTO(this, "Raft { Reg: %d; State: %d; Elec_t: %f; Term: %llu; #ser: %zu; Lead: %s }",
		local_node_.region, state, leader_election_timeout.repeat, term, number_servers, leader.c_str());

	if (state != State::LEADER) {
		state = State::CANDIDATE;
		++term;
		votes = 0;
		votedFor.clear();
		send_message(Message::REQUEST_VOTE, local_node_->serialise() + serialise_length(term));
	}

	_reset_leader_election_timeout();

	L_EV_END(this, "Raft::leader_election_timeout_cb:END");
}


void
Raft::_reset_leader_election_timeout()
{
	auto node = std::atomic_load(&local_node);
	number_servers = XapiandManager::manager->get_nodes_by_region(node->region) + 1;

	leader_election_timeout.repeat = random_real(LEADER_ELECTION_MIN, LEADER_ELECTION_MAX);
	leader_election_timeout.again();
	L_EV(this, "Restart raft's leader election event (%g)", leader_election_timeout.repeat);
}


void
Raft::leader_heartbeat_cb(ev::timer&, int)
{
	L_EV(this, "Raft::leader_heartbeat_cb");

	L_EV_BEGIN(this, "Raft::leader_heartbeat_cb:BEGIN");

	if (XapiandManager::manager->state != XapiandManager::State::READY) {
		L_EV_END(this, "Raft::leader_heartbeat_cb:END");
		return;
	}

	auto node = std::atomic_load(&local_node);
	send_message(Message::HEARTBEAT_LEADER, node->serialise());

	L_EV_END(this, "Raft::leader_heartbeat_cb:END");
}


void
Raft::_start_leader_heartbeat()
{
	auto node = std::atomic_load(&local_node);
	assert(leader == *node);

	leader_heartbeat.repeat = random_real(HEARTBEAT_LEADER_MIN, HEARTBEAT_LEADER_MAX);
	leader_heartbeat.again();
	L_EV(this, "Restart raft's leader heartbeat event (%g)", leader_heartbeat.repeat);

	send_message(Message::LEADER, node->serialise() +
		serialise_length(number_servers) +
		serialise_length(term));
}


std::string
Raft::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
