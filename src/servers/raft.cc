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
	  election_leader(*loop),
	  heartbeat(*loop),
	  state(State::FOLLOWER),
	  number_servers(0)
{
	election_leader.set<Raft, &Raft::leader_election_cb>(this);
	election_timeout = random_real(ELECTION_LEADER_MIN, ELECTION_LEADER_MAX);
	election_leader.set(election_timeout, 0.0);
	L_EV(this, "Set raft's election leader event (%g)", election_timeout);

	last_activity = ev::now(*loop);

	heartbeat.set<Raft, &Raft::heartbeat_cb>(this);

	L_OBJ(this, "CREATED RAFT CONSENSUS");
}


Raft::~Raft()
{
	election_leader.stop();
	L_EV(this, "Stop raft's election leader event");

	if (state == State::LEADER) {
		heartbeat.stop();
		L_EV(this, "Stop raft's heartbeat event");
	}

	L_OBJ(this, "DELETED RAFT CONSENSUS");
}


void
Raft::reset()
{
	if (state == State::LEADER) {
		heartbeat.stop();
		L_EV(this, "Stop raft's heartbeat event");
	}

	state = State::FOLLOWER;

	L_RAFT(this, "Raft was restarted!");
}


void
Raft::leader_election_cb(ev::timer&, int)
{
	L_EV_BEGIN(this, "Raft::leader_election_cb:BEGIN");

	auto m = manager();

	if (m->state == XapiandManager::State::READY) {
		// calculate when the timeout would happen
		ev::tstamp remaining_time = last_activity + election_timeout - ev::now(*loop);
		L_RAFT_PROTO(this, "Raft { Reg: %d; State: %d; Rem_t: %f; Elec_t: %f; Term: %llu; #ser: %zu; Lead: %s }",
			local_node.region.load(), state, remaining_time, election_timeout, term, number_servers.load(), leader.c_str());

		if (remaining_time < 0.0 && state != State::LEADER) {
			state = State::CANDIDATE;
			++term;
			votes = 0;
			votedFor.clear();
			send_message(Message::REQUEST_VOTE, local_node.serialise() + serialise_length(term));
			election_timeout = random_real(ELECTION_LEADER_MIN, ELECTION_LEADER_MAX);
		}
	}

	// Start the timer again.
	election_leader.set(election_timeout, 0.0);
	L_EV(this, "Set raft's election leader event (%g)", election_timeout);

	election_leader.start();
	L_EV(this, "Start raft's election leader event (%g)", election_timeout);

	L_EV_END(this, "Raft::leader_election_cb:END");
}


void
Raft::heartbeat_cb(ev::timer&, int)
{
	L_EV_BEGIN(this, "Raft::heartbeat_cb:BEGIN");
	send_message(Message::HEARTBEAT_LEADER, local_node.serialise());
	L_EV_END(this, "Raft::heartbeat_cb:END");
}


void
Raft::start_heartbeat()
{
	send_message(Message::LEADER, local_node.serialise() +
		serialise_length(number_servers.load()) +
		serialise_length(term));

	heartbeat.repeat = random_real(HEARTBEAT_LEADER_MIN, HEARTBEAT_LEADER_MAX);
	heartbeat.again();
	L_EV(this, "Start raft's heartbeat event (%g)", heartbeat.repeat);

	L_RAFT(this, "\tSet heartbeat timeout event %f", heartbeat.repeat);

	leader = lower_string(local_node.name);
}


void
Raft::register_activity()
{
	L_RAFT_PROTO(this, "Register activity");
	last_activity = ev::now(*loop);
}


std::string
Raft::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
