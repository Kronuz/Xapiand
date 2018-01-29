/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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

#include "endpoint.h"
#include "ignore_unused.h"
#include "log.h"
#include "manager.h"


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
	  reset_leader_election_timeout_async(*ev_loop),
	  reset_async(*ev_loop)
{
	leader_election_timeout.set<Raft, &Raft::leader_election_timeout_cb>(this);

	leader_heartbeat.set<Raft, &Raft::leader_heartbeat_cb>(this);

	start_leader_heartbeat_async.set<Raft, &Raft::start_leader_heartbeat_async_cb>(this);
	start_leader_heartbeat_async.start();
	L_EV("Start raft's async start leader heartbeat event");

	reset_leader_election_timeout_async.set<Raft, &Raft::reset_leader_election_timeout_async_cb>(this);
	reset_leader_election_timeout_async.start();
	L_EV("Start raft's async reset leader election timeout event");

	reset_async.set<Raft, &Raft::reset_async_cb>(this);
	reset_async.start();
	L_EV("Start raft's async reset event");

	L_OBJ("CREATED RAFT CONSENSUS");
}


Raft::~Raft()
{
	leader_election_timeout.stop();
	L_EV("Stop raft's leader election event");

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	L_OBJ("DELETED RAFT CONSENSUS");
}


void
Raft::start()
{
	_reset_leader_election_timeout();

	L_RAFT("Raft was started!");
}


void
Raft::stop()
{
	leader_election_timeout.stop();
	L_EV("Stop raft's leader election event");

	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	state = State::FOLLOWER;
	number_servers = 1;

	L_RAFT("Raft was stopped!");
}


void
Raft::start_leader_heartbeat_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::start_leader_heartbeat_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str());

	ignore_unused(revents);

	_start_leader_heartbeat();
}


void
Raft::reset_leader_election_timeout_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::reset_leader_election_timeout_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str());

	ignore_unused(revents);

	_reset_leader_election_timeout();
}


void
Raft::reset_async_cb(ev::async&, int revents)
{
	L_CALL("Raft::reset_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str());

	ignore_unused(revents);

	_reset();
}


void
Raft::_reset()
{
	leader_heartbeat.stop();
	L_EV("Stop raft's leader heartbeat event");

	state = State::FOLLOWER;

	_reset_leader_election_timeout();

	L_RAFT("Raft was restarted!");
}


void
Raft::leader_election_timeout_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_election_timeout_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str());

	ignore_unused(revents);

	L_EV_BEGIN("Raft::leader_election_timeout_cb:BEGIN");

	if (XapiandManager::manager->state != XapiandManager::State::READY) {
		L_EV_END("Raft::leader_election_timeout_cb:END");
		return;
	}

	auto local_node_ = local_node.load();
	L_RAFT_PROTO("Raft { Reg: %d; State: %d; Elec_t: %f; Term: %llu; #ser: %zu; Lead: %s }",
		local_node_->region, state, leader_election_timeout.repeat, term, number_servers, leader.c_str());

	if (state != State::LEADER) {
		state = State::CANDIDATE;
		++term;
		votes = 0;
		votedFor.clear();
		send_message(Message::REQUEST_VOTE, local_node_->serialise() + serialise_length(term));
	}

	_reset_leader_election_timeout();

	L_EV_END("Raft::leader_election_timeout_cb:END");
}


void
Raft::_reset_leader_election_timeout()
{
	auto local_node_ = local_node.load();
	number_servers = XapiandManager::manager->get_nodes_by_region(local_node_->region) + 1;

	leader_election_timeout.repeat = random_real(LEADER_ELECTION_MIN, LEADER_ELECTION_MAX);
	leader_election_timeout.again();
	L_EV("Restart raft's leader election event (%g)", leader_election_timeout.repeat);
}


void
Raft::leader_heartbeat_cb(ev::timer&, int revents)
{
	L_CALL("Raft::leader_heartbeat_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str());

	ignore_unused(revents);

	L_EV_BEGIN("Raft::leader_heartbeat_cb:BEGIN");

	if (XapiandManager::manager->state != XapiandManager::State::READY) {
		L_EV_END("Raft::leader_heartbeat_cb:END");
		return;
	}

	auto local_node_ = local_node.load();
	send_message(Message::HEARTBEAT_LEADER, local_node_->serialise());

	L_EV_END("Raft::leader_heartbeat_cb:END");
}


void
Raft::_start_leader_heartbeat()
{
	auto local_node_ = local_node.load();
	assert(leader == *local_node_);

	leader_heartbeat.repeat = random_real(HEARTBEAT_LEADER_MIN, HEARTBEAT_LEADER_MAX);
	leader_heartbeat.again();
	L_EV("Restart raft's leader heartbeat event (%g)", leader_heartbeat.repeat);

	send_message(Message::LEADER, local_node_->serialise() +
		serialise_length(number_servers) +
		serialise_length(term));
}


void
Raft::send_message(Message type, const std::string& message)
{
	if (type != Raft::Message::HEARTBEAT_LEADER) {
		L_RAFT("<< send_message(%s)", MessageNames[toUType(type)]);
	}
	L_RAFT_PROTO("message: %s", repr(message).c_str());
	BaseUDP::send_message(toUType(type), message);
}


std::string
Raft::getDescription() const noexcept
{
	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_RAFT_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
