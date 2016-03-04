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

#pragma once

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "udp_base.h"

// Values in seconds
#define HEARTBEAT_LEADER_MIN 0.150
#define HEARTBEAT_LEADER_MAX 0.300

#define ELECTION_LEADER_MIN (4.0 * HEARTBEAT_LEADER_MIN)
#define ELECTION_LEADER_MAX (4.5 * HEARTBEAT_LEADER_MAX)

#define XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION 1
#define XAPIAND_RAFT_PROTOCOL_MINOR_VERSION 0

constexpr uint16_t XAPIAND_RAFT_PROTOCOL_VERSION = XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION | XAPIAND_RAFT_PROTOCOL_MINOR_VERSION << 8;


// The Raft consensus algorithm
class Raft : public BaseUDP {
private:
	enum class State {
		LEADER,
		FOLLOWER,
		CANDIDATE,
	};

	static constexpr const char* const StateNames[] = {
		"LEADER", "FOLLOWER", "CANDIDATE",
	};

	enum class Message {
		HEARTBEAT_LEADER,   // Only leader send heartbeats to its follower servers
		REQUEST_VOTE,       // Invoked by candidates to gather votes
		RESPONSE_VOTE,      // Gather votes
		LEADER,             // Node saying hello when it become leader
		REQUEST_DATA,       // Request information from leader
		RESPONSE_DATA,      // Receive information from leader
		RESET,              // Force reset a node
		MAX,
	};

	static constexpr const char* const MessageNames[] = {
		"HEARTBEAT_LEADER", "REQUEST_VOTE", "RESPONSE_VOTE", "LEADER",
		"REQUEST_DATA", "RESPONSE_DATA", "RESET",
	};

	uint64_t term;
	size_t votes;

	std::atomic_bool running;

	ev::timer election_leader;
	ev::tstamp election_timeout;

	ev::timer heartbeat;
	ev::tstamp last_activity;

	std::string votedFor;
	std::string leader;

	State state;
	std::atomic_size_t number_servers;

	void leader_election_cb(ev::timer& watcher, int revents);
	void heartbeat_cb(ev::timer& watcher, int revents);

	void start_heartbeat();

	friend class RaftServer;

public:
	Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& group_);
	~Raft();

	void reset();

	inline void send_message(Message type, const std::string& message) {
		if (type != Raft::Message::HEARTBEAT_LEADER) {
			L_RAFT(this, "<< send_message(%s)", MessageNames[static_cast<int>(type)]);
		}
		L_RAFT_PROTO(this, "message: '%s'", repr(message).c_str());
		BaseUDP::send_message(toUType(type), message);
	}

	void register_activity();

	std::string getDescription() const noexcept override;

	inline void start() {
		if (!running.exchange(true)) {
			election_leader.start();
			L_RAFT(this, "Raft was started!");
		}
		number_servers.store(manager()->get_nodes_by_region(local_node.region.load()) + 1);
	}

	inline void stop() {
		if (running.exchange(false)) {
			election_leader.stop();
			if (state == State::LEADER) {
				heartbeat.stop();
			}
			state = State::FOLLOWER;
			number_servers.store(1);
			L_RAFT(this, "Raft was stopped!");
		}
		number_servers.store(manager()->get_nodes_by_region(local_node.region.load()) + 1);
	}
};

#endif
