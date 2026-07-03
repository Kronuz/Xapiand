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
#include "debouncer.h"                      // for make_debouncer (db/schema/settings updaters)
#include "enum.h"                           // for ENUM_CLASS
#include "lru.h"                            // for lru::aging_lru
#include "opts.h"                           // for opts::*
#include "thread.hh"                        // for ThreadPolicyType::* (the updater debouncers)
#include "xapian.h"                         // for Xapian::rev

// IMPORTANT include order (EV_ERROR clash): the Xapiand headers pull in libev (ev/ev.h),
// whose `enum EV_ERROR` collides with macOS <sys/event.h>'s `#define EV_ERROR` that Asio
// pulls in. libev must be included FIRST. node.h + raft_delegate.h (which pulls node.h and
// manager.h, both libev) come BEFORE the Asio-only cluster/reactor headers.
#include "node.h"                           // for Node (libev)
#include "raft_delegate.h"                  // for XapiandRaftDelegate (pulls manager.h; ev before raft.h)

#include "bus.h"                            // for cluster::Bus (Asio)
#include "raft.h"                           // for cluster::Raft / cluster::RaftMessage (Asio)
#include "reactor_events.h"                 // for reactor::PeriodicTimer / reactor::Signal (Asio)


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


// Discovery for nodes and databases -- rebuilt on the Kronuz/cluster substrate: a
// cluster::Bus (multicast transport, replacing the libev UDP + ev::io) carries the
// membership gossip and the app database/primary-election messages, while a cluster::Raft
// (consensus, replacing all the former raft_* state/handlers/timers) rides the same Bus
// through XapiandRaftDelegate. Discovery owns its Bus reactor thread, so it is no longer a
// libev Worker/Thread; the manager drives it via run()/start()/stop()/finish()/join().
class Discovery {
public:
	using Message = DiscoveryMessage;

private:
	unsigned short port_;
	std::string group_;                                // multicast group (for getDescription)

	cluster::Bus bus_;                                 // transport (owns the reactor thread)
	XapiandRaftDelegate delegate_;                     // the ~16 raft seams over Xapiand
	cluster::Raft<std::shared_ptr<const Node>> raft_;  // consensus (rides bus_)

	reactor::PeriodicTimer cluster_discovery_;         // exploration cadence (ev::timer replacement)
	reactor::Signal cluster_enter_signal_;             // cross-thread cluster_enter (ev::async replacement)
	reactor::Signal message_send_signal_;              // cross-thread app message send (ev::async replacement)

	struct PrimaryShardVoter {
		std::string uuid;
		Xapian::rev revision;
		bool eligible;
	};

	lru::aging_lru<std::string, std::unordered_map<std::string, PrimaryShardVoter>> _ASYNC_elected_primaries;

	ConcurrentQueue<std::pair<Message, std::string>> message_send_args;

	// transport: the Bus on_message router (runs on the bus reactor thread) + framed send
	void on_message(int wire_type, std::string_view content, const asio::ip::udp::endpoint& from);
	void send_message(Message type, const std::string& message);

	// membership gossip (ride the Bus)
	void cluster_hello(Message type, const std::string& message);
	void cluster_wave(Message type, const std::string& message);
	void cluster_sneer(Message type, const std::string& message);
	void cluster_enter(Message type, const std::string& message);
	void cluster_bye(Message type, const std::string& message);

	// app database/schema/settings messages
	void db_updated(Message type, const std::string& message);
	void schema_updated(Message type, const std::string& message);
	void index_settings_updated(Message type, const std::string& message);

	// discovery exploration timer + cross-thread signals (all run on the bus reactor thread)
	void cluster_discovery_cb();
	void cluster_enter_signal_cb();
	void _message_send(Message type, const std::string& path);
	void message_send_signal_cb();

	// No copy constructor
	Discovery(const Discovery&) = delete;
	Discovery& operator=(const Discovery&) = delete;

public:
	Discovery(const char* group, unsigned int port);
	~Discovery() noexcept;

	const char* name() const noexcept {
		return "DISC";
	}

	// lifecycle (the manager drives these; the Bus owns the reactor thread)
	void run();                                       // bind + join the group + start the receive loop
	void start();                                     // arm the exploration timer
	void stop();                                      // relinquish leadership + wave goodbye + stop timers
	void finish();                                    // stop the Bus reactor loop
	bool join(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));  // wait for the loop to end

	void cluster_enter();
	void raft_add_command(const std::string& command);
	void raft_request_vote();
	void raft_relinquish_leadership();
	void db_updated_send(Xapian::rev revision, std::string_view path);
	void schema_updated_send(Xapian::rev revision, std::string_view path);
	void settings_updated_send(Xapian::rev revision, std::string_view path);
	void primary_updated_send(size_t shards, std::string_view path);

	// Messages executed asynchronously from MAIN thread
	void _ASYNC_primary_updated(const std::string& message);
	void _ASYNC_elect_primary(const std::string& message);
	void _ASYNC_elect_primary_response(const std::string& message);
	void _ASYNC_elect_primary_send(const std::string& normalized_path);

	std::string __repr__() const;

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

void settings_updated_send(Xapian::rev revision, std::string path);

inline auto& settings_updater(bool create = true) {
	static auto settings_updater = create ? make_unique_debouncer<std::string, ThreadPolicyType::updaters>("IU--", "SE{:02}", opts.num_discoverers, settings_updated_send, std::chrono::milliseconds(opts.db_updater_throttle_time), std::chrono::milliseconds(opts.db_updater_debounce_timeout), std::chrono::milliseconds(opts.db_updater_debounce_busy_timeout), std::chrono::milliseconds(opts.db_updater_debounce_min_force_timeout), std::chrono::milliseconds(opts.db_updater_debounce_max_force_timeout)) : nullptr;
	assert(!create || settings_updater);
	return settings_updater;
}

void primary_updated_send(size_t shards, std::string path);

inline auto& primary_updater(bool create = true) {
	static auto primary_updater = create ? make_unique_debouncer<std::string, ThreadPolicyType::updaters>("PU--", "PU{:02}", opts.num_discoverers, primary_updated_send, std::chrono::milliseconds(opts.db_updater_throttle_time), std::chrono::milliseconds(opts.db_updater_debounce_timeout), std::chrono::milliseconds(opts.db_updater_debounce_busy_timeout), std::chrono::milliseconds(opts.db_updater_debounce_min_force_timeout), std::chrono::milliseconds(opts.db_updater_debounce_max_force_timeout)) : nullptr;
	assert(!create || primary_updater);
	return primary_updater;
}

#endif
