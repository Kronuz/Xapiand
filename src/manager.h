/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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
#include <functional>                         // for std::function
#include <atomic>                             // for std::atomic, std::atomic_int
#include <memory>                             // for std::unique_ptr, std::shared_ptr
#include <mutex>                              // for std::mutex
#include <string>                             // for std::string
#include <string_view>                        // for std::string_view

#include "base_x.hh"                          // for Base62
#include "database/utils.h"                   // for UNKNOWN_REVISION
#include "debouncer.h"                        // for Debouncer
#include "endpoint.h"                         // for Endpoint
#include "enum.h"                             // for ENUM_CLASS
#include "length.h"                           // for serialise_length
#include "opts.h"                             // for opts::*
#include "node.h"                             // for Node, local_node
#include "thread.hh"                          // for ThreadPolicyType::*
#include "threadpool.hh"                      // for ThreadPool
#include "engine/error.h"                    // for engine::Error



class DocPreparer;
class DocIndexer;

class MsgPack;

namespace http { class HttpAsioService; }
class DatabasePool;
class DatabaseWALWriter;
class DatabaseCleanup;
class SchemasLRU;
class IndexResolverLRU;
struct IndexSettings;

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
	RESET
)


class XapiandManager {

	explicit XapiandManager(std::chrono::steady_clock::time_point process_start_ = std::chrono::steady_clock::now());
	~XapiandManager() noexcept;

	std::pair<struct sockaddr_in, std::string> host_address();
	std::pair<struct sockaddr_in, std::string> host_address(const char *hostname);

	void shutdown_impl(long long asap, long long now);

	void _get_stats_time(MsgPack& stats, int start, int end, int increment);

	std::string load_node_name();
	void save_node_name(std::string_view name);
	std::string set_node_name(std::string_view name);

	void make_servers();

public:
	using State = XapiandManagerState;

	std::atomic_int total_clients;
	std::atomic_int http_clients;

	std::unique_ptr<http::HttpAsioService> http_service;
	int http_port = 0;

	std::shared_ptr<DatabaseCleanup> database_cleanup;

	std::unique_ptr<SchemasLRU> schemas;
	std::unique_ptr<DatabasePool> database_pool;
	std::unique_ptr<DatabaseWALWriter> wal_writer;
	std::unique_ptr<IndexResolverLRU> index_settings_resolver;

	std::unique_ptr<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>> doc_preparer_pool;
	std::unique_ptr<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>> doc_indexer_pool;

	std::atomic<State> state;
	std::string node_name;

private:
	static std::shared_ptr<XapiandManager> _manager;

	std::shared_ptr<Logging> log;

	std::atomic_llong _shutdown_asap;
	std::atomic_llong _shutdown_now;

	std::chrono::steady_clock::time_point _process_start;

	std::atomic_int _try_shutdown;
	std::atomic_bool _running_loop;
	std::atomic_bool _deinited;
	std::atomic_llong _asap;      // snapshot for the async shutdown hand-off
	std::atomic_llong _now;

	// All the Asio/reactor control-plane state (the loop, the timers/signals, the
	// async-signal-safe self-pipe) lives in this pimpl so this header stays free of
	// Asio -- mirroring how the *AsioService members are forward-declared + unique_ptr'd.
	struct Reactors;
	std::unique_ptr<Reactors> _rx;

	void run_loop();
	void break_loop();
	// Runs work on the control plane's offload worker, then `then` on the loop.
	void control_offload(std::function<void()> work, std::function<void()> then = {});
	void arm_signal_reader();

	void try_shutdown_timer_cb();
	void signal_sig_impl();

	void shutdown_async_cb();
	void shutdown_sig(int sig, bool async);

	void setup_node_async_cb();
	void setup_node_impl();

	void set_ready_impl();

	bool is_running_loop() const { return _running_loop.load(std::memory_order_relaxed); }


	IndexSettings resolve_index_settings_impl(std::string_view normalized_slashed_path, bool writable = false, const MsgPack* settings = nullptr, bool reload = false, bool rebuild = false, bool clear = false);
	Endpoints resolve_index_endpoints_impl(const Endpoint& endpoint, bool writable = false, const MsgPack* settings = nullptr);
	void invalidate_settings_impl(const std::string& uri);

	std::string server_metrics_impl();
	std::string reactor_lag_metrics() const;

	void try_shutdown_impl(bool always) {
		if (always || _shutdown_asap) {
			shutdown_sig(0, true);
		}
	}

	void init();

public:
	std::atomic_int atom_sig;

	void signal_sig(int sig);

	bool is_deinited() const { return _deinited.load(std::memory_order_relaxed); }

	// Retained for callers (e.g. the restore loop) that poll "is the manager going away?".
	// Worker set _detaching during shutdown; the equivalent now is: being torn down, or a
	// signal (shutdown/exit) has arrived.
	bool is_detaching() const {
		return _deinited.load(std::memory_order_relaxed) || atom_sig.load(std::memory_order_relaxed) != 0;
	}

	void run();
	void join();

	void shutdown(long long asap, long long now, bool async = true);

	bool ready_to_end_http(bool notify = false);
	bool ready_to_end(bool notify = false);

	std::string __repr__() const;

	template<typename... Args>
	static auto& make(Args&&... args) {
		assert(!_manager);
		// std::make_shared needs a public ctor; the manager's are private, so use the
		// same trick Worker::make_shared did -- a local subclass that exposes the ctor.
		struct enable_make_shared : XapiandManager {
			explicit enable_make_shared(Args&&... _args) : XapiandManager(std::forward<Args>(_args)...) {}
		};
		_manager = std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
		_manager->init();
		return _manager;
	}

	static const auto& manager([[maybe_unused]] bool check = false) {
		if (check && !_manager) {
			throw engine::Error(engine::errc::internal, "No manager");
		}
		return _manager;
	}

	static void reset() {
		_manager.reset();
	}

	static IndexSettings resolve_index_settings(std::string_view normalized_path, bool writable = false, const MsgPack* settings = nullptr, bool reload = false, bool rebuild = false, bool clear = false);

	static Endpoints resolve_index_endpoints(const Endpoint& endpoint, bool writable = false, const MsgPack* settings = nullptr) {
		return manager(true)->resolve_index_endpoints_impl(endpoint, writable, settings);
	}

	static void invalidate_settings(const std::string& uri) {
		return manager(true)->invalidate_settings_impl(uri);
	}

	static void setup_node() {
		manager(true)->setup_node_impl();
	}

	static std::string server_metrics() {
		return manager(true)->server_metrics_impl();
	}

	static void try_shutdown(bool always = false) {
		if (_manager) {
			_manager->try_shutdown_impl(always);
		}
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


