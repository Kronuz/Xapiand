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

#include "raft.h"

#ifdef XAPIAND_CLUSTERING

#include "../endpoint.h"

#include <assert.h>


constexpr const char* const Raft::MessageNames[];
constexpr const char* const Raft::StateNames[];


Raft::Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& group_)
	: BaseUDP(manager_, loop_, port_, "Raft", XAPIAND_RAFT_PROTOCOL_VERSION, group_),
	  term(0),
	  running(false),
	  leader_election(*loop),
	  heartbeat(*loop),
	  state(State::FOLLOWER),
	  number_servers(0)
{
	leader_election.set<Raft, &Raft::leader_election_cb>(this);

	heartbeat.set<Raft, &Raft::heartbeat_cb>(this);

	L_OBJ(this, "CREATED RAFT CONSENSUS");
}


Raft::~Raft()
{
	leader_election.stop();
	L_EV(this, "Stop raft's election leader event");

	heartbeat.stop();
	L_EV(this, "Stop raft's heartbeat event");

	L_OBJ(this, "DELETED RAFT CONSENSUS");
}


void
Raft::start()
{
	if (!running.exchange(true)) {
		reset_leader_election_timeout();
		L_RAFT(this, "Raft was started!");
	}
	number_servers.store(manager()->get_nodes_by_region(local_node.region) + 1);
}


void
Raft::stop()
{
	if (running.exchange(false)) {
		leader_election.stop();
		L_EV(this, "Stop raft's election leader event");

		heartbeat.stop();
		L_EV(this, "Stop raft's heartbeat event");

		state = State::FOLLOWER;
		number_servers.store(1);

		L_RAFT(this, "Raft was stopped!");
	}
	number_servers.store(manager()->get_nodes_by_region(local_node.region) + 1);
}


void
Raft::reset()
{
	heartbeat.stop();
	L_EV(this, "Stop raft's heartbeat event");

	state = State::FOLLOWER;

	reset_leader_election_timeout();

	L_RAFT(this, "Raft was restarted!");
}


void
Raft::leader_election_cb(ev::timer&, int)
{
	L_EV(this, "Raft::leader_election_cb");

	L_EV_BEGIN(this, "Raft::leader_election_cb:BEGIN");

	if (manager()->state != XapiandManager::State::READY) {
		L_EV_END(this, "Raft::leader_election_cb:END");
		return;
	}

	L_RAFT_PROTO(this, "Raft { Reg: %d; State: %d; Elec_t: %f; Term: %llu; #ser: %zu; Lead: %s }",
		local_node.region.load(), state, leader_election.repeat, term, number_servers.load(), leader.c_str());

	if (state != State::LEADER) {
		state = State::CANDIDATE;
		++term;
		votes = 0;
		votedFor.clear();
		send_message(Message::REQUEST_VOTE, local_node.serialise() + serialise_length(term));
	}

	reset_leader_election_timeout();

	L_EV_END(this, "Raft::leader_election_cb:END");
}

void
Raft::reset_leader_election_timeout()
{
	leader_election.repeat = random_real(LEADER_ELECTION_MIN, LEADER_ELECTION_MAX);
	leader_election.again();
	L_EV(this, "Restart raft's election leader event (%g)", leader_election.repeat);
}

void
Raft::heartbeat_cb(ev::timer&, int)
{
	L_EV(this, "Raft::heartbeat_cb");

	L_EV_BEGIN(this, "Raft::heartbeat_cb:BEGIN");

	if (manager()->state != XapiandManager::State::READY) {
		L_EV_END(this, "Raft::heartbeat_cb:END");
		return;
	}

	send_message(Message::HEARTBEAT_LEADER, local_node.serialise());

	L_EV_END(this, "Raft::heartbeat_cb:END");
}


void
Raft::start_heartbeat()
{
	assert(leader == lower_string(local_node.name));

	send_message(Message::LEADER, local_node.serialise() +
		serialise_length(number_servers) +
		serialise_length(term));

	heartbeat.repeat = random_real(HEARTBEAT_LEADER_MIN, HEARTBEAT_LEADER_MAX);
	heartbeat.again();
	L_EV(this, "Restart raft's heartbeat event (%g)", heartbeat.repeat);

	L_RAFT(this, "\tSet raft's heartbeat timeout event %f", heartbeat.repeat);
}


std::string
Raft::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
