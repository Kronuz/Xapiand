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

#pragma once

#include "xapiand.h"


#ifdef XAPIAND_CLUSTERING

#include "udp_base.h"  // for BaseUDP
#include "endpoint.h"  // for Node


// Values in seconds
constexpr double HEARTBEAT_LEADER_MIN = 1.5;
constexpr double HEARTBEAT_LEADER_MAX = 3.0;

constexpr double LEADER_ELECTION_MIN = 2.5 * HEARTBEAT_LEADER_MAX;
constexpr double LEADER_ELECTION_MAX = 5.0 * HEARTBEAT_LEADER_MAX;

constexpr uint16_t XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_RAFT_PROTOCOL_MINOR_VERSION = 0;

constexpr uint16_t XAPIAND_RAFT_PROTOCOL_VERSION = XAPIAND_RAFT_PROTOCOL_MAJOR_VERSION | XAPIAND_RAFT_PROTOCOL_MINOR_VERSION << 8;

class RaftServer;

// The Raft consensus algorithm
class Raft : public BaseUDP {
	friend RaftServer;
public:
	enum class State {
		LEADER,
		FOLLOWER,
		CANDIDATE,
		MAX,
	};

	static const std::string& StateNames(State type) {
		static const std::string StateNames[] = {
			"LEADER", "FOLLOWER", "CANDIDATE",
		};

		auto type_int = static_cast<int>(type);
		if (type_int >= 0 || type_int < static_cast<int>(State::MAX)) {
			return StateNames[type_int];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

	enum class Message {
		HEARTBEAT_LEADER,   // Only leader send heartbeats to its follower servers
		REQUEST_VOTE,       // Invoked by candidates to gather votes
		RESPONSE_VOTE,      // Gather votes
		LEADER,             // Node saying hello when it become leader
		LEADERSHIP,         // Request information from leader
		RESET,              // Force reset a node
		MAX,
	};

	static const std::string& MessageNames(Message type) {
		static const std::string MessageNames[] = {
			"HEARTBEAT_LEADER", "REQUEST_VOTE", "RESPONSE_VOTE", "LEADER",
			"LEADERSHIP", "RESET",
		};

		auto type_int = static_cast<int>(type);
		if (type_int >= 0 || type_int < static_cast<int>(Message::MAX)) {
			return MessageNames[type_int];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

	size_t votes;
	uint64_t term;
	Node voted_for;
	Node leader;

	State state;
	std::atomic_size_t number_servers;

private:

	ev::timer leader_election_timeout;
	ev::timer leader_heartbeat;
	ev::async start_leader_heartbeat_async;
	ev::async reset_leader_election_timeout_async;
	ev::async reset_async;

	void leader_election_timeout_cb(ev::timer& watcher, int revents);
	void leader_heartbeat_cb(ev::timer& watcher, int revents);
	void start_leader_heartbeat_async_cb(ev::async& watcher, int revents);
	void reset_leader_election_timeout_async_cb(ev::async& watcher, int revents);
	void reset_async_cb(ev::async& watcher, int revents);

	void _request_vote();
	void _start_leader_heartbeat();
	void _reset_leader_election_timeout();
	void _reset();

public:
	std::string __repr__() const override {
		return Worker::__repr__("Raft");
	}

	Raft(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_);
	~Raft();

	inline void start_leader_heartbeat() {
		start_leader_heartbeat_async.send();
	}

	inline void reset_leader_election_timeout() {
		reset_leader_election_timeout_async.send();
	}

	inline void reset() {
		reset_async.send();
	}

	void start();
	void stop();

	void send_message(Message type, const std::string& message);
	std::string getDescription() const noexcept override;
};

#endif
