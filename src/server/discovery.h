/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"  // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include <string>                           // for std::string
#include <unordered_map>                    // for std::unordered_map
#include <vector>                           // for std::vector

#include "concurrent_queue.h"               // for ConcurrentQueue
#include "debouncer.h"                      // for make_debouncer
#include "node.h"                           // for Node
#include "opts.h"                           // for opts::*
#include "thread.hh"                        // for Thread, ThreadPolicyType::*
#include "udp.h"                            // for UDP
#include "worker.h"                         // for Worker


// Values in seconds
constexpr double CLUSTER_DISCOVERY_WAITING_FAST  = 0.200;
constexpr double CLUSTER_DISCOVERY_WAITING_SLOW  = 0.600;

constexpr double RAFT_HEARTBEAT_LEADER_MIN = 0.150;
constexpr double RAFT_HEARTBEAT_LEADER_MAX = 0.300;

constexpr double RAFT_LEADER_ELECTION_MIN = 2.5 * RAFT_HEARTBEAT_LEADER_MAX;
constexpr double RAFT_LEADER_ELECTION_MAX = 5.0 * RAFT_HEARTBEAT_LEADER_MAX;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;


struct DatabaseUpdate;
class UUID;


struct RaftLogEntry {
	uint64_t term;
	std::string command;
};

// Discovery for nodes and databases
class Discovery : public UDP, public Worker, public Thread<Discovery, ThreadPolicyType::regular> {
public:
	enum class Role {
		RAFT_FOLLOWER,
		RAFT_CANDIDATE,
		RAFT_LEADER,
	};

	static const std::string& RoleNames(Role type) {
		static const std::string _[] = {
			"RAFT_FOLLOWER", "RAFT_CANDIDATE", "RAFT_LEADER",
		};
		auto idx = static_cast<size_t>(type);
		if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
			return _[idx];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

	enum class Message {
		CLUSTER_HELLO,                // New node saying hello
		CLUSTER_WAVE,                 // Nodes telling the client they do agree with the new node's name
		CLUSTER_SNEER,                // Nodes telling the client they don't agree with the new node's name
		CLUSTER_ENTER,                // Node enters the room
		CLUSTER_BYE,                  // Node says goodbye
		RAFT_HEARTBEAT,               // same as RAFT_APPEND_ENTRIES
		RAFT_HEARTBEAT_RESPONSE,      // same as RAFT_APPEND_ENTRIES_RESPONSE
		RAFT_APPEND_ENTRIES,          // Node saying hello when it become leader
		RAFT_APPEND_ENTRIES_RESPONSE, // Request information from leader
		RAFT_REQUEST_VOTE,            // Invoked by candidates to gather votes
		RAFT_REQUEST_VOTE_RESPONSE,   // Gather votes
		RAFT_ADD_COMMAND,             //
		DB_UPDATED,                   //
		MAX,                          //
	};

	static const std::string& MessageNames(Message type) {
		static const std::string _[] = {
			"CLUSTER_HELLO", "CLUSTER_WAVE", "CLUSTER_SNEER",
			"CLUSTER_ENTER", "CLUSTER_BYE",
			"RAFT_HEARTBEAT", "RAFT_HEARTBEAT_RESPONSE",
			"RAFT_APPEND_ENTRIES", "RAFT_APPEND_ENTRIES_RESPONSE",
			"RAFT_REQUEST_VOTE", "RAFT_REQUEST_VOTE_RESPONSE",
			"RAFT_ADD_COMMAND",
			"DB_UPDATED",
		};
		auto idx = static_cast<size_t>(type);
		if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
			return _[idx];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

private:
	ev::io io;

	ev::timer cluster_discovery;

	ev::async cluster_enter_async;

	ev::timer raft_leader_election_timeout;
	ev::timer raft_leader_heartbeat;

	ev::async raft_request_vote_async;

	ev::async raft_add_command_async;
	ConcurrentQueue<std::string> raft_add_command_args;

	ev::async db_update_send_async;

	Role raft_role;
	size_t raft_votes_granted;
	size_t raft_votes_denied;

	uint64_t raft_current_term;
	Node raft_voted_for;
	std::vector<RaftLogEntry> raft_log;

	size_t raft_commit_index;
	size_t raft_last_applied;

	std::unordered_map<std::string, size_t> raft_next_indexes;
	std::unordered_map<std::string, size_t> raft_match_indexes;

	ConcurrentQueue<std::string> db_update_send_args;

	void cluster_enter_async_cb(ev::async& watcher, int revents);

	void _db_update_send(const std::string& path);
	void db_update_send_async_cb(ev::async& watcher, int revents);

	void send_message(Message type, const std::string& message);
	void io_accept_cb(ev::io& watcher, int revents);
	void discovery_server(Discovery::Message type, const std::string& message);

	void cluster_hello(Message type, const std::string& message);
	void cluster_wave(Message type, const std::string& message);
	void cluster_sneer(Message type, const std::string& message);
	void cluster_enter(Message type, const std::string& message);
	void cluster_bye(Message type, const std::string& message);
	void raft_request_vote(Message type, const std::string& message);
	void raft_request_vote_response(Message type, const std::string& message);
	void raft_append_entries(Message type, const std::string& message);
	void raft_append_entries_response(Message type, const std::string& message);
	void raft_add_command(Message type, const std::string& message);
	void db_updated(Message type, const std::string& message);

	void cluster_discovery_cb(ev::timer& watcher, int revents);

	void raft_leader_election_timeout_cb(ev::timer& watcher, int revents);
	void raft_leader_heartbeat_cb(ev::timer& watcher, int revents);

	void _raft_leader_heartbeat_start(double min = RAFT_HEARTBEAT_LEADER_MIN, double max = RAFT_HEARTBEAT_LEADER_MAX);
	void _raft_leader_election_timeout_reset(double min = RAFT_LEADER_ELECTION_MIN, double max = RAFT_LEADER_ELECTION_MAX);
	void _raft_set_leader_node(const std::shared_ptr<const Node>& node);

	void _raft_apply(const std::string& command);
	void _raft_commit_log();

	void _raft_request_vote(bool immediate);

	void raft_request_vote_async_cb(ev::async& watcher, int revents);

	void _raft_add_command(const std::string& command);
	void raft_add_command_async_cb(ev::async& watcher, int revents);

	void shutdown_impl(long long asap, long long now) override;
	void destroy_impl() override;
	void start_impl() override;
	void stop_impl() override;

	// No copy constructor
	Discovery(const Discovery&) = delete;
	Discovery& operator=(const Discovery&) = delete;

public:
	Discovery(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* hostname, unsigned int serv);
	~Discovery() noexcept;

	const char* name() const noexcept {
		return "DISC";
	}

	void operator()();

	void cluster_enter();
	void raft_add_command(const std::string& command);
	void raft_request_vote();
	void db_update_send(const std::string& path);

	std::string __repr__() const override;

	std::string getDescription() const;
};

void db_updater_send(std::string path);

inline auto& db_updater(bool create = true) {
	static auto db_updater = create ? make_unique_debouncer<std::string, 3000, 6000, 12000, ThreadPolicyType::updaters>("DU--", "DU{:02}", opts.num_discoverers, db_updater_send) : nullptr;
	ASSERT(!create || db_updater);
	return db_updater;
}

#endif
