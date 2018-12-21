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

#include "config.h"

#include <atomic>                             // for std::atomic, std::atomic_int
#include <mutex>                              // for std::mutex
#include <string>                             // for std::string
#include "string_view.hh"                     // for std::string_view
#include <vector>                             // for std::vector

#include "base_x.hh"                          // for Base62
#include "debouncer.h"                        // for Debouncer
#include "endpoint.h"                         // for Endpoint
#include "ev/ev++.h"                          // for ev::loop_ref
#include "ignore_unused.h"                    // for ignore_unused
#include "length.h"                           // for serialise_length
#include "node.h"                             // for Node, local_node
#include "thread.hh"                          // for ThreadPolicyType::*
#include "threadpool.hh"                      // for ThreadPool
#include "worker.h"                           // for Worker


class Http;
#ifdef XAPIAND_CLUSTERING
class Binary;
class Discovery;
class BinaryClient;
class BinaryServer;
#endif

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


class XapiandManager : public Worker  {
	friend Worker;

	XapiandManager();
	XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::time_point<std::chrono::system_clock> process_start_ = std::chrono::system_clock::now());
	~XapiandManager() noexcept;

	std::pair<struct sockaddr_in, std::string> host_address();

	void shutdown_impl(long long asap, long long now) override;
	void stop_impl() override;

	void _get_stats_time(MsgPack& stats, int start, int end, int increment);

	std::string load_node_name();
	void save_node_name(std::string_view node_name);
	std::string set_node_name(std::string_view node_name);

	void start_discovery();
	void make_servers();

public:
	enum class State {
		BAD,
		READY,
		SETUP,
		JOINING,
		WAITING_MORE,
		WAITING,
		RESET,
	};

	static const std::string& StateNames(State type) {
		static const std::string _[] = {
			"BAD", "READY", "SETUP", "JOINING", "WAITING_MORE", "WAITING", "RESET",
		};
		auto idx = static_cast<size_t>(type);
		if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
			return _[idx];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

private:
	static std::shared_ptr<XapiandManager> _manager;

	std::shared_ptr<Logging> log;

	std::atomic_int _total_clients;
	std::atomic_int _http_clients;
	std::atomic_int _binary_clients;

	std::shared_ptr<Http> _http;
#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<Binary> _binary;
	std::shared_ptr<Discovery> _discovery;
#endif

	std::shared_ptr<DatabaseCleanup> _database_cleanup;

	std::unique_ptr<SchemasLRU> _schemas;
	std::unique_ptr<DatabasePool> _database_pool;
	std::unique_ptr<DatabaseWALWriter> _wal_writer;

	std::unique_ptr<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::binary_clients>> _http_client_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::binary_servers>> _http_server_pool;
#ifdef XAPIAND_CLUSTERING
	std::unique_ptr<ThreadPool<std::shared_ptr<BinaryClient>, ThreadPolicyType::http_clients>> _binary_client_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<BinaryServer>, ThreadPolicyType::http_servers>> _binary_server_pool;
#endif

	std::atomic_llong _shutdown_asap;
	std::atomic_llong _shutdown_now;

	std::atomic<State> _state;
	std::string _node_name;
	int _new_cluster;
	std::chrono::time_point<std::chrono::system_clock> _process_start;

	ev::async signal_sig_async;
	ev::async setup_node_async;
	ev::async set_cluster_database_ready_async;
	ev::async shutdown_sig_async;

#ifdef XAPIAND_CLUSTERING
	ev::async new_leader_async;
#endif

	void signal_sig_async_cb(ev::async&, int);
	void signal_sig_impl();

	void shutdown_sig(int sig);

	void setup_node_async_cb(ev::async&, int);
	void setup_node_impl();

	void set_cluster_database_ready_async_cb(ev::async&, int);
	void set_cluster_database_ready_impl();

#ifdef XAPIAND_CLUSTERING
	void load_nodes();
	void new_leader_async_cb(ev::async&, int);
	void new_leader_impl();
	void renew_leader_impl();
	void reset_state_impl();
	void join_cluster_impl();
#endif

	std::vector<std::shared_ptr<const Node>> resolve_index_nodes_impl(const std::string& normalized_slashed_path);
	Endpoint resolve_index_endpoint_impl(const Endpoint& endpoint, bool master);

	std::string server_metrics_impl();

	void try_shutdown_impl(bool always) {
		if (always || (_shutdown_asap != 0 && _total_clients == 0)) {
			shutdown_sig(0);
		}
	}

	void init();

public:
	std::atomic_int atom_sig;

	void signal_sig(int sig);

	void run();
	void join();

	std::string __repr__() const override;

	template<typename... Args>
	static auto& make(Args&&... args) {
		_manager = Worker::make_shared<XapiandManager>(std::forward<Args>(args)...);
		_manager->init();
		return _manager;
	}

	static const auto& manager(bool check = true) {
		ASSERT(!check || _manager);
		ignore_unused(check);
		return _manager;
	}

	static void reset() {
		ASSERT(_manager);
		_manager.reset();
	}

	static std::vector<std::shared_ptr<const Node>> resolve_index_nodes(const std::string& normalized_slashed_path) {
		ASSERT(_manager);
		return _manager->resolve_index_nodes_impl(normalized_slashed_path);
	}

	static Endpoint resolve_index_endpoint(const Endpoint& endpoint, bool master) {
		ASSERT(_manager);
		return _manager->resolve_index_endpoint_impl(endpoint, master);
	}

	static void setup_node() {
		ASSERT(_manager);
		_manager->setup_node_impl();
	}

	static void new_leader() {
		ASSERT(_manager);
		_manager->new_leader_impl();
	}

	static void renew_leader() {
		ASSERT(_manager);
		_manager->renew_leader_impl();
	}

	static void reset_state() {
		ASSERT(_manager);
		_manager->reset_state_impl();
	}

	static void join_cluster() {
		ASSERT(_manager);
		_manager->join_cluster_impl();
	}

	static std::string server_metrics() {
		ASSERT(_manager);
		return _manager->server_metrics_impl();
	}

	static void try_shutdown(bool always = false) {
		ASSERT(_manager);
		_manager->try_shutdown_impl(always);
	}

	static void set_cluster_database_ready() {
		ASSERT(_manager);
		_manager->set_cluster_database_ready_impl();
	}

	static auto& node_name() {
		ASSERT(_manager);
		return _manager->_node_name;
	}

	static auto& state() {
		ASSERT(_manager);
		return _manager->_state;
	}

	static bool exchange_state(State from, State to, std::chrono::milliseconds timeout = 0s, std::string_view format_timeout = "", std::string_view format_done = "");

	static auto& total_clients() {
		ASSERT(_manager);
		return _manager->_total_clients;
	}
	static auto& http_clients() {
		ASSERT(_manager);
		return _manager->_http_clients;
	}
	static auto& binary_clients() {
		ASSERT(_manager);
		return _manager->_binary_clients;
	}

	static auto& database_pool() {
		ASSERT(_manager);
		ASSERT(_manager->_database_pool);
		return _manager->_database_pool;
	}
	static auto& http_client_pool() {
		ASSERT(_manager);
		ASSERT(_manager->_http_client_pool);
		return _manager->_http_client_pool;
	}
#ifdef XAPIAND_CLUSTERING
	static auto& binary_client_pool() {
		ASSERT(_manager);
		ASSERT(_manager->_binary_client_pool);
		return _manager->_binary_client_pool;
	}
	static auto& discovery() {
		ASSERT(_manager);
		ASSERT(_manager->_discovery);
		return _manager->_discovery;
	}
	static auto& binary() {
		ASSERT(_manager);
		ASSERT(_manager->_binary);
		return _manager->_binary;
	}
#endif

	static auto& http() {
		ASSERT(_manager);
		ASSERT(_manager->_http);
		return _manager->_http;
	}

	static auto& wal_writer() {
		ASSERT(_manager);
		ASSERT(_manager->_wal_writer);
		return _manager->_wal_writer;
	}
	static auto& schemas() {
		ASSERT(_manager);
		ASSERT(_manager->_schemas);
		return _manager->_schemas;
	}
};

#ifdef XAPIAND_CLUSTERING
void trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint);

inline auto& trigger_replication(bool create = true) {
	static auto trigger_replication = create ? make_unique_debouncer<std::string, 3000, 6000, 12000, ThreadPolicyType::replication>("TR--", "TR%02zu", 3, trigger_replication_trigger) : nullptr;
	ASSERT(!create || trigger_replication);
	return trigger_replication;
}
#endif
