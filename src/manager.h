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

#include "config.h"

#include <cassert>                            // for assert
#include <chrono>                             // for std::chrono
#include <atomic>                             // for std::atomic, std::atomic_int
#include <mutex>                              // for std::mutex
#include <string>                             // for std::string
#include <string_view>                        // for std::string_view
#include <vector>                             // for std::vector

#include "base_x.hh"                          // for Base62
#include "database/utils.h"                   // for UNKNOWN_REVISION
#include "debouncer.h"                        // for Debouncer
#include "endpoint.h"                         // for Endpoint
#include "enum.h"                             // for ENUM_CLASS
#include "ev/ev++.h"                          // for ev::loop_ref
#include "length.h"                           // for serialise_length
#include "opts.h"                             // for opts::*
#include "node.h"                             // for Node, local_node
#include "thread.hh"                          // for ThreadPolicyType::*
#include "threadpool.hh"                      // for ThreadPool
#include "worker.h"                           // for Worker
#include "xapian.h"                           // for Xapian::*


class Http;
#ifdef XAPIAND_CLUSTERING
class Discovery;
class RemoteProtocol;
class RemoteProtocolClient;
class RemoteProtocolServer;
class ReplicationProtocol;
class ReplicationProtocolClient;
class ReplicationProtocolServer;
#endif

class DocMatcher;
class DocPreparer;
class DocIndexer;

class MsgPack;
class HttpClient;
class HttpServer;
class DatabasePool;
class DatabaseWALWriter;
class DatabaseCleanup;
class SchemasLRU;

extern void sig_exit(int sig);


inline std::string serialise_node_id(uint64_t node_id) {
	return Base62::inverted().encode(serialise_length(node_id));
}


inline uint64_t unserialise_node_id(std::string_view node_id_str) {
	auto serialized = Base62::inverted().decode(node_id_str);
	const char *p = serialized.data();
	const char *p_end = p + serialized.size();
	return unserialise_length(&p, p_end);
}


ENUM_CLASS(XapiandManagerState, int,
	BAD,
	READY,
	SETUP,
	JOINING,
	WAITING_MORE,
	WAITING,
	RESET
)


struct IndexSettingsShard {
	Xapian::rev version;
	bool modified;

	std::vector<std::string> nodes;

	IndexSettingsShard();
};

ENUM_CLASS(XapiandManagerCommand, int,
	RAFT_APPLY_COMMAND,
	RAFT_SET_LEADER_NODE,
	ELECT_PRIMARY,
	ASYNC_PRIMARY_UPDATED,
	ASYNC_ELECT_PRIMARY,
	ASYNC_ELECT_PRIMARY_RESPONSE
)

struct IndexSettings {
	Xapian::rev version;
	bool loaded;
	bool saved;
	bool modified;

	std::chrono::steady_clock::time_point stalled;

	size_t num_shards;
	size_t num_replicas_plus_master;
	std::vector<IndexSettingsShard> shards;

	IndexSettings();

	IndexSettings(Xapian::rev version, bool loaded, bool saved, bool modified, std::chrono::steady_clock::time_point stalled, size_t num_shards, size_t num_replicas_plus_master, const std::vector<IndexSettingsShard>& shards);

	std::string __repr__() const;
};


class XapiandManager : public Worker  {
	friend Worker;

	XapiandManager();
	XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::steady_clock::time_point process_start_ = std::chrono::steady_clock::now());
	~XapiandManager() noexcept;

	std::pair<struct sockaddr_in, std::string> host_address();

	void shutdown_impl(long long asap, long long now) override;

	void _get_stats_time(MsgPack& stats, int start, int end, int increment);

	std::string load_node_name();
	void save_node_name(std::string_view name);
	std::string set_node_name(std::string_view name);

	void start_discovery();
	void make_servers();

public:
	using State = XapiandManagerState;
	using Command = XapiandManagerCommand;

	std::atomic_int total_clients;
	std::atomic_int http_clients;
	std::atomic_int remote_clients;
	std::atomic_int replication_clients;

	std::shared_ptr<Http> http;
#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<RemoteProtocol> remote;
	std::shared_ptr<ReplicationProtocol> replication;
	std::shared_ptr<Discovery> discovery;
#endif

	std::shared_ptr<DatabaseCleanup> database_cleanup;

	std::unique_ptr<SchemasLRU> schemas;
	std::unique_ptr<DatabasePool> database_pool;
	std::unique_ptr<DatabaseWALWriter> wal_writer;

	std::unique_ptr<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>> http_client_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>> http_server_pool;
#ifdef XAPIAND_CLUSTERING
	std::unique_ptr<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>> remote_client_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>> remote_server_pool;

	std::unique_ptr<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>> replication_client_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>> replication_server_pool;
#endif
	std::unique_ptr<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>> doc_preparer_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>> doc_indexer_pool;

	std::atomic<State> state;
	std::string node_name;

private:
	static std::shared_ptr<XapiandManager> _manager;

	std::shared_ptr<Logging> log;

	std::atomic_llong _shutdown_asap;
	std::atomic_llong _shutdown_now;

	int _new_cluster;
	std::chrono::steady_clock::time_point _process_start;

	int _try_shutdown;
	ev::timer try_shutdown_timer;
	ev::async signal_sig_async;
	ev::async setup_node_async;
	ev::async set_cluster_database_ready_async;

	ev::async dispatch_command_async;
	ConcurrentQueue<std::pair<Command, std::string>> dispatch_command_args;

	void try_shutdown_timer_cb(ev::timer& watcher, int revents);
	void signal_sig_async_cb(ev::async&, int);
	void signal_sig_impl();

	void shutdown_sig(int sig, bool async);

	void setup_node_async_cb(ev::async&, int);
	void setup_node_impl();

	void set_cluster_database_ready_async_cb(ev::async&, int);
	void set_cluster_database_ready_impl();

	void dispatch_command_async_cb(ev::async& watcher, int revents);
	void dispatch_command_impl(Command command, const std::string& data);
	void _dispatch_command(Command command, const std::string& data);

#ifdef XAPIAND_CLUSTERING
	void load_nodes();
	void new_leader();

	void add_node(const std::string& name);
	void node_added(const std::string& name);
#endif

	IndexSettings resolve_index_settings_impl(std::string_view normalized_slashed_path, bool writable = false, bool primary = false, const MsgPack* settings = nullptr, std::shared_ptr<const Node> primary_node = nullptr, bool reload = false, bool rebuild = false, bool clear = false);
	Endpoints resolve_index_endpoints_impl(const Endpoint& endpoint, bool writable = false, bool primary = false, const MsgPack* settings = nullptr);

	std::string server_metrics_impl();

	void try_shutdown_impl(bool always) {
		if (always || _shutdown_asap) {
			shutdown_sig(0, true);
		}
	}

	void init();

public:
	std::atomic_int atom_sig;

	void signal_sig(int sig);

	void run();
	void join();

	bool ready_to_end_http(bool notify = false);
	bool ready_to_end_remote(bool notify = false);
	bool ready_to_end_replication(bool notify = false);
	bool ready_to_end_database_cleanup(bool notify = false);
	bool ready_to_end_discovery(bool notify = false);
	bool ready_to_end(bool notify = false);

	std::string __repr__() const override;

	template<typename... Args>
	static auto& make(Args&&... args) {
		assert(!_manager);
		_manager = Worker::make_shared<XapiandManager>(std::forward<Args>(args)...);
		_manager->init();
		return _manager;
	}

	static const auto& manager([[maybe_unused]] bool check = false) {
		if (check && !_manager) {
			throw Xapian::AssertionError("No manager");
		}
		return _manager;
	}

	static void reset() {
		_manager.reset();
	}

	static IndexSettings resolve_index_settings(std::string_view normalized_path, bool writable = false, bool primary = false, const MsgPack* settings = nullptr, std::shared_ptr<const Node> primary_node = nullptr, bool reload = false, bool rebuild = false, bool clear = false) {
		return manager(true)->resolve_index_settings_impl(normalized_path, writable, primary, settings, primary_node, reload, rebuild, clear);
	}

	static std::vector<std::vector<std::shared_ptr<const Node>>> resolve_nodes(const IndexSettings& index_settings);

	static Endpoints resolve_index_endpoints(const Endpoint& endpoint, bool writable = false, bool primary = false, const MsgPack* settings = nullptr) {
		return manager(true)->resolve_index_endpoints_impl(endpoint, writable, primary, settings);
	}

	static void setup_node() {
		manager(true)->setup_node_impl();
	}

	static void dispatch_command(Command command, const std::string& data = "") {
		manager(true)->dispatch_command_impl(command, data);
	}

	static std::string server_metrics() {
		return manager(true)->server_metrics_impl();
	}

	static void try_shutdown(bool always = false) {
		if (_manager) {
			_manager->try_shutdown_impl(always);
		}
	}

	static void set_cluster_database_ready() {
		manager(true)->set_cluster_database_ready_impl();
	}

	static bool exchange_state(State from, State to, std::chrono::milliseconds timeout = 0s, std::string_view format_timeout = "", std::string_view format_done = "");

	static State get_state() {
		return _manager ? _manager->state.load() : State::BAD;
	}

	static void set_state(State state) {
		if (_manager) {
			_manager->state.store(state);
		}
	}
};


#ifdef XAPIAND_CLUSTERING

void trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint);

inline auto& trigger_replication(bool create = true) {
	static auto trigger_replication = create ? make_unique_debouncer<std::string, ThreadPolicyType::replication>("TR--", "TR{:02}", opts.num_replicators, trigger_replication_trigger, std::chrono::milliseconds(opts.trigger_replication_throttle_time), std::chrono::milliseconds(opts.trigger_replication_debounce_timeout), std::chrono::milliseconds(opts.trigger_replication_debounce_busy_timeout), std::chrono::milliseconds(opts.trigger_replication_debounce_min_force_timeout), std::chrono::milliseconds(opts.trigger_replication_debounce_max_force_timeout)) : nullptr;
	assert(!create || trigger_replication);
	return trigger_replication;
}

#endif
