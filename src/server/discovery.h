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

#include <cassert>                          // for assert
#include <chrono>                           // for std::chrono
#include <string>                           // for std::string
#include <string_view>                      // for std::string_view
#include <unordered_map>                    // for std::unordered_map
#include <vector>                           // for std::vector

#include "concurrent_queue.h"               // for ConcurrentQueue
#include "debouncer.h"                      // for make_debouncer
#include "enum.h"                           // for ENUM_CLASS
#include "lru.h"                            // for lru::aging_lru
#include "node.h"                           // for Node
#include "opts.h"                           // for opts::*
#include "thread.hh"                        // for Thread, ThreadPolicyType::*
#include "udp.h"                            // for UDP
#include "worker.h"                         // for Worker
#include "xapian.h"                         // for Xapian::rev


struct DatabaseUpdate;
class UUID;


struct RaftLogEntry {
	uint64_t term;
	std::string command;
};


ENUM_CLASS(DiscoveryRole, int,
	RAFT_FOLLOWER,
	RAFT_CANDIDATE,
	RAFT_LEADER
)


ENUM_CLASS(DiscoveryMessage, int,
	CLUSTER_HELLO,                // New node saying hello
	CLUSTER_WAVE,                 // Nodes telling the client they do agree with the new node's name
	CLUSTER_SNEER,                // Nodes telling the client they don't agree with the new node's name
	CLUSTER_ENTER,                // Node enters the room
	CLUSTER_BYE,                  // Node says goodbye
	RAFT_HEARTBEAT,               // Same as RAFT_APPEND_ENTRIES
	RAFT_HEARTBEAT_RESPONSE,      // Same as RAFT_APPEND_ENTRIES_RESPONSE
	RAFT_APPEND_ENTRIES,          // Node saying hello when it become leader
	RAFT_APPEND_ENTRIES_RESPONSE, // Request information from leader
	RAFT_REQUEST_VOTE,            // Invoked by candidates to gather votes
	RAFT_REQUEST_VOTE_RESPONSE,   // Gather votes
	RAFT_ADD_COMMAND,             // Tell the leader to add a command to the log
	DB_UPDATED,                   // Database has been updated, trigger replication
	SCHEMA_UPDATED,               // Schema has been updated, invalidate schema from LRU
	INDEX_SETTINGS_UPDATED,       // Schema has been updated, invalidate cache from index settings LRU
	PRIMARY_UPDATED,              // Primary shard has been updated, invalidate index from LRU
	ELECT_PRIMARY,                // Invoked by leader to gather votes to promote a primary shard
	ELECT_PRIMARY_RESPONSE,       // Gather primary shard votes
	MAX                           //
)


// Discovery for nodes and databases
class Discovery : public UDP, public Worker, public Thread<Discovery, ThreadPolicyType::regular> {
public:
	using Role = DiscoveryRole;
	using Message = DiscoveryMessage;

private:
	ev::io io;

	ev::timer cluster_discovery;

	ev::async cluster_enter_async;

	ev::timer raft_leader_election_timeout;
	ev::timer raft_leader_heartbeat;

	ev::async raft_request_vote_async;
	ev::async raft_relinquish_leadership_async;

	ev::async raft_add_command_async;
	ConcurrentQueue<std::string> raft_add_command_args;

	ev::async message_send_async;

	Role raft_role;
	size_t raft_votes_granted;
	size_t raft_votes_denied;
	std::unordered_set<std::string> raft_voters;

	uint64_t raft_current_term;
	Node raft_voted_for;
	std::vector<RaftLogEntry> raft_log;

	size_t raft_commit_index;
	size_t raft_last_applied;

	bool raft_eligible;

	struct PrimaryShardVoter {
		std::string uuid;
		Xapian::rev revision;
		bool eligible;
	};

	lru::aging_lru<std::string, std::unordered_map<std::string, PrimaryShardVoter>> _ASYNC_elected_primaries;

	std::unordered_map<std::string, size_t> raft_next_indexes;
	std::unordered_map<std::string, size_t> raft_match_indexes;

	ConcurrentQueue<std::pair<Message, std::string>> message_send_args;

	void cluster_enter_async_cb(ev::async& watcher, int revents);

	void _message_send(Message type, const std::string& path);
	void message_send_async_cb(ev::async& watcher, int revents);

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
	void schema_updated(Message type, const std::string& message);
	void index_settings_updated(Message type, const std::string& message);

	void cluster_discovery_cb(ev::timer& watcher, int revents);

	void raft_leader_election_timeout_cb(ev::timer& watcher, int revents);
	void raft_leader_heartbeat_cb(ev::timer& watcher, int revents);

	void _raft_leader_heartbeat_reset(double timeout);
	void _raft_leader_election_timeout_reset(double timeout);
	void _raft_set_leader_node(const std::shared_ptr<const Node>& node);

	void _raft_apply_command(const std::string& command);
	void _raft_commit_log();

	void _raft_request_vote(bool immediate);
	void raft_request_vote_async_cb(ev::async& watcher, int revents);

	void raft_relinquish_leadership_async_cb(ev::async& watcher, int revents);

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
	void raft_relinquish_leadership();
	void db_updated_send(Xapian::rev revision, std::string_view path);
	void schema_updated_send(Xapian::rev revision, std::string_view path);
	void primary_updated_send(size_t shards, std::string_view path);

	// Messages executed asynchronously from MAIN thread
	void _ASYNC_primary_updated(const std::string& message);
	void _ASYNC_elect_primary(const std::string& message);
	void _ASYNC_elect_primary_response(const std::string& message);
	void _ASYNC_elect_primary_send(const std::string& normalized_path);

	std::string __repr__() const override;

	std::string getDescription() const;
};

void db_updated_send(Xapian::rev revision, std::string path);

inline auto& db_updater(bool create = true) {
	static auto db_updater = create ? make_unique_debouncer<std::string, ThreadPolicyType::updaters>("DU--", "DU{:02}", opts.num_discoverers, db_updated_send, std::chrono::milliseconds(opts.db_updater_throttle_time), std::chrono::milliseconds(opts.db_updater_debounce_timeout), std::chrono::milliseconds(opts.db_updater_debounce_busy_timeout), std::chrono::milliseconds(opts.db_updater_debounce_min_force_timeout), std::chrono::milliseconds(opts.db_updater_debounce_max_force_timeout)) : nullptr;
	assert(!create || db_updater);
	return db_updater;
}

void schema_updated_send(Xapian::rev revision, std::string path);

inline auto& schema_updater(bool create = true) {
	static auto schema_updater = create ? make_unique_debouncer<std::string, ThreadPolicyType::updaters>("SU--", "SU{:02}", opts.num_discoverers, schema_updated_send, std::chrono::milliseconds(opts.db_updater_throttle_time), std::chrono::milliseconds(opts.db_updater_debounce_timeout), std::chrono::milliseconds(opts.db_updater_debounce_busy_timeout), std::chrono::milliseconds(opts.db_updater_debounce_min_force_timeout), std::chrono::milliseconds(opts.db_updater_debounce_max_force_timeout)) : nullptr;
	assert(!create || schema_updater);
	return schema_updater;
}

void primary_updated_send(size_t shards, std::string path);

inline auto& primary_updater(bool create = true) {
	static auto primary_updater = create ? make_unique_debouncer<std::string, ThreadPolicyType::updaters>("PU--", "PU{:02}", opts.num_discoverers, primary_updated_send, std::chrono::milliseconds(opts.db_updater_throttle_time), std::chrono::milliseconds(opts.db_updater_debounce_timeout), std::chrono::milliseconds(opts.db_updater_debounce_busy_timeout), std::chrono::milliseconds(opts.db_updater_debounce_min_force_timeout), std::chrono::milliseconds(opts.db_updater_debounce_max_force_timeout)) : nullptr;
	assert(!create || primary_updater);
	return primary_updater;
}

#endif
