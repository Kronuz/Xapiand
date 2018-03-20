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

#include <list>
#include <mutex>
#include <regex>
#include <unordered_map>

#include "base_x.hh"
#include "database.h"
#include "endpoint_resolver.h"
#include "ev/ev++.h"
#include "length.h"
#include "opts.h"
#include "schemas_lru.h"
#include "stats.h"
#include "threadpool.h"
#include "worker.h"
#include "serialise.h"


#define UNKNOWN_REGION -1


class Http;
#ifdef XAPIAND_CLUSTERING
class Binary;
class Discovery;
class Raft;
#endif
class XapiandServer;


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

	using nodes_map_t = std::unordered_map<std::string, std::shared_ptr<const Node>>;

	std::mutex qmtx;

	XapiandManager();
	XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_);

	struct sockaddr_in host_address();

	void destroyer();

	void destroy_impl() override;
	void shutdown_impl(time_t asap, time_t now) override;

	void finish();
	void join();

	void _get_stats_time(MsgPack& stats, int start, int end, int increment);

protected:
	std::mutex nodes_mtx;
	nodes_map_t nodes;

	size_t nodes_size();
	std::string load_node_name();
	void save_node_name(std::string_view node_name);
	std::string set_node_name(std::string_view node_name_);

	uint64_t load_node_id();
	void save_node_id(uint64_t node_id);
	uint64_t get_node_id();

	void make_servers();
	void make_replicators();

public:
	std::string __repr__() const override {
		return Worker::__repr__("XapiandManager");
	}

	enum class State {
		BAD,
		READY,
		SETUP,
		WAITING_,
		WAITING,
		RESET,
	};

	static constexpr const char* const StateNames[] = {
		"BAD", "READY", "SETUP", "WAITING_", "WAITING", "RESET",
	};

	static std::shared_ptr<XapiandManager> manager;

	std::vector<std::weak_ptr<XapiandServer>> servers_weak;
	std::weak_ptr<Http> weak_http;
#ifdef XAPIAND_CLUSTERING
	std::weak_ptr<Binary> weak_binary;
	std::weak_ptr<Discovery> weak_discovery;
	std::weak_ptr<Raft> weak_raft;
#endif

	DatabasePool database_pool;
	SchemasLRU schemas;

	ThreadPool<> thread_pool;
	ThreadPool<> server_pool;
#ifdef XAPIAND_CLUSTERING
	ThreadPool<> replicator_pool;
#endif
#ifdef XAPIAND_CLUSTERING
	EndpointResolver endp_r;
#endif

	std::atomic<time_t> shutdown_asap;
	std::atomic<time_t> shutdown_now;

	std::atomic<State> state;
	std::string node_name;

	std::atomic_int atom_sig;
	ev::async signal_sig_async;
	ev::async shutdown_sig_async;
	void signal_sig(int sig);
	void signal_sig_async_cb(ev::async&, int);

	void shutdown_sig(int sig);

	~XapiandManager();

	void setup_node();

	void setup_node(std::shared_ptr<XapiandServer>&& server);

	void run();

	bool is_single_node();

#ifdef XAPIAND_CLUSTERING
	void reset_state();

	bool put_node(std::shared_ptr<const Node> node);
	std::shared_ptr<const Node> get_node(std::string_view node_name);
	std::shared_ptr<const Node> touch_node(std::string_view node_name, int32_t region);
	void drop_node(std::string_view node_name);

	size_t get_nodes_by_region(int32_t region);

	// Return the region to which db name belongs
	int32_t get_region(std::string_view db_name);
	// Return the region to which local_node belongs
	int32_t get_region();

	std::shared_future<bool> trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint);
#endif

	bool resolve_index_endpoint(const std::string &path, std::vector<Endpoint> &endpv, size_t n_endps=1, std::chrono::duration<double, std::milli> timeout=1s);

	void server_status(MsgPack& stats);
	void get_stats_time(MsgPack& stats, const std::string& time_req, const std::string& gran_req);

	inline decltype(auto) get_lock() noexcept {
		return std::unique_lock<std::mutex>(qmtx);
	}
};
