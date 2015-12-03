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

#include "../endpoint.h"

#include <assert.h>


Raft::Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string &group_)
	: BaseUDP(manager_, loop_, port_, "Raft", group_),
	  term(0),
	  running(false),
	  election_leader(*loop),
	  heartbeat(*loop),
	  state(State::FOLLOWER),
	  number_servers(0)
{
	election_timeout = random_real(ELECTION_LEADER_MIN, ELECTION_LEADER_MAX);
	election_leader.set<Raft, &Raft::leader_election_cb>(this);
	election_leader.set(election_timeout, 0.);
	LOG_EV(this, "\tStart election leader event\n");
	last_activity = ev::now(*loop);

	heartbeat.set<Raft, &Raft::heartbeat_cb>(this);

	LOG_OBJ(this, "CREATED RAFT CONSENSUS\n");
}


Raft::~Raft()
{
	election_leader.stop();

	if (state == State::LEADER) {
		heartbeat.stop();
	}

	LOG_OBJ(this, "DELETED RAFT CONSENSUS\n");
}


void
Raft::reset()
{
	if (state == State::LEADER) {
		heartbeat.stop();
	}

	state = State::FOLLOWER;

	LOG_RAFT(this, "Raft was restarted!\n");
}


void
Raft::leader_election_cb(ev::timer &, int)
{
	LOG_EV_BEGIN(this, "Raft::leader_election_cb:BEGIN\n");
	if (manager->state == XapiandManager::State::READY) {
		// calculate when the timeout would happen
		ev::tstamp remaining_time = last_activity + election_timeout - ev::now(*loop);
		LOG_RAFT(this, "Raft { Reg: %d; State: %d; Rem_t: %f; Elec_t: %f; Term: %llu; #ser: %zu; Lead: %s }\n",
			local_node.region.load(), state, remaining_time, election_timeout, term, number_servers.load(), leader.c_str());

		if (remaining_time < 0. && state != State::LEADER) {
			state = State::CANDIDATE;
			term++;
			votes = 0;
			votedFor.clear();
			send_message(Message::REQUEST_VOTE, local_node.serialise() + serialise_string(std::to_string(term)));
			election_timeout = random_real(ELECTION_LEADER_MIN, ELECTION_LEADER_MAX);
		}
	} else {
		LOG_RAFT(this, "Waiting manager get ready!!\n");
	}

	// Start the timer again.
	election_leader.set(election_timeout, 0.);
	election_leader.start();
	LOG_EV_END(this, "Raft::leader_election_cb:END\n");
}


void
Raft::heartbeat_cb(ev::timer &, int)
{
	LOG_EV_BEGIN(this, "Raft::heartbeat_cb:BEGIN\n");
	send_message(Message::HEARTBEAT_LEADER, local_node.serialise());
	LOG_EV_END(this, "Raft::heartbeat_cb:END\n");
}


void
Raft::start_heartbeat()
{
	send_message(Message::LEADER, local_node.serialise() +
		serialise_string(std::to_string(number_servers.load())) +
		serialise_string(std::to_string(term)));

	heartbeat.repeat = random_real(HEARTBEAT_MIN, HEARTBEAT_MAX);
	heartbeat.again();
	LOG_RAFT(this, "\tSet heartbeat timeout event %f\n", heartbeat.repeat);

	leader = stringtolower(local_node.name);
}


void
Raft::send_message(Message type, const std::string &content)
{
	if (!content.empty()) {
		std::string message(1, toUType(type));
		message.append(std::string((const char *)&XAPIAND_RAFT_PROTOCOL_VERSION, sizeof(uint16_t)));
		message.append(serialise_string(manager->cluster_name));
		message.append(content);
		sending_message(message);
	}
}


void
Raft::register_activity()
{
	last_activity = ev::now(*loop);
}


std::string
Raft::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}
