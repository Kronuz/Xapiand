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

#pragma once

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "udp_base.h"

// Values in seconds
#define HEARTBEAT_LEADER_MIN 0.150
#define HEARTBEAT_LEADER_MAX 0.300

#define LEADER_ELECTION_MIN (2.5 * HEARTBEAT_LEADER_MAX)
#define LEADER_ELECTION_MAX (5.0 * HEARTBEAT_LEADER_MAX)

#define XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION 1
#define XAPIAND_RAFT_PROTOCOL_MINOR_VERSION 0

constexpr uint16_t XAPIAND_RAFT_PROTOCOL_VERSION = XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION | XAPIAND_RAFT_PROTOCOL_MINOR_VERSION << 8;


// The Raft consensus algorithm
class Raft : public BaseUDP {
public:
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
		LEADERSHIP,         // Request information from leader
		RESET,              // Force reset a node
		MAX,
	};

	static constexpr const char* const MessageNames[] = {
		"HEARTBEAT_LEADER", "REQUEST_VOTE", "RESPONSE_VOTE", "LEADER",
		"LEADERSHIP", "RESET",
	};

	uint64_t term;
	size_t votes;

	Node votedFor;
	Node leader;

	State state;
	std::atomic_size_t number_servers;

private:

	ev::timer leader_election_timeout;
	ev::timer leader_heartbeat;
	ev::async async_start_leader_heartbeat;
	ev::async async_reset_leader_election_timeout;
	ev::async async_reset;
	ev::async async_start;
	ev::async async_stop;

	void leader_election_timeout_cb(ev::timer& watcher, int revents);
	void leader_heartbeat_cb(ev::timer& watcher, int revents);
	void async_start_leader_heartbeat_cb(ev::async &watcher, int revents);
	void async_reset_leader_election_timeout_cb(ev::async &watcher, int revents);
	void async_reset_cb(ev::async &watcher, int revents);
	void async_start_cb(ev::async &watcher, int revents);
	void async_stop_cb(ev::async &watcher, int revents);

	void _start_leader_heartbeat();
	void _reset_leader_election_timeout();
	void _reset();
	void _start();
	void _stop();

public:
	Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& group_);
	~Raft();

	inline void start_leader_heartbeat() {
		async_start_leader_heartbeat.send();
	}
	inline void reset_leader_election_timeout() {
		async_reset_leader_election_timeout.send();
	}
	inline void reset() {
		async_reset.send();
	}
	inline void start() {
		async_start.send();
	}
	inline void stop() {
		async_stop.send();
	}

	inline void send_message(Message type, const std::string& message) {
		if (type != Raft::Message::HEARTBEAT_LEADER) {
			L_RAFT(this, "<< send_message(%s)", MessageNames[toUType(type)]);
		}
		L_RAFT_PROTO(this, "message: '%s'", repr(message).c_str());
		BaseUDP::send_message(toUType(type), message);
	}

	std::string getDescription() const noexcept override;
};

#endif
