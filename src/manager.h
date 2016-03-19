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

#include "database.h"
#include "threadpool.h"
#include "worker.h"
#include "endpoint_resolver.h"
#include "ev/ev++.h"

#include <list>
#include <unordered_map>
#include <mutex>
#include <regex>

#define UNKNOWN_REGION -1


using opts_t = struct opts_s {
	int verbosity;
	bool detach;
	bool chert;
	bool solo;
	std::string database;
	std::string cluster_name;
	std::string node_name;
	unsigned int http_port;
	unsigned int binary_port;
	unsigned int discovery_port;
	unsigned int raft_port;
	std::string pidfile;
	std::string logfile;
	std::string uid;
	std::string gid;
	std::string discovery_group;
	std::string raft_group;
	size_t num_servers;
	size_t dbpool_size;
	size_t num_replicators;
	size_t threadpool_size;
	size_t endpoints_list_size;
	size_t num_committers;
};


class Http;
#ifdef XAPIAND_CLUSTERING
class Binary;
class Discovery;
class Raft;
#endif
class XapiandServer;
class DatabaseAutocommit;


class XapiandManager : public Worker  {
	friend Worker;

	using nodes_map_t = std::unordered_map<std::string, Node>;

	std::mutex qmtx;

	Endpoints cluster_endpoints;

	XapiandManager(ev::loop_ref* loop_, const opts_t& o);

	struct sockaddr_in host_address();

	void shutdown_impl(time_t asap, time_t now) override;
	void destroy_impl() override;
	
	void finish();
	void join();

protected:
	std::mutex nodes_mtx;
	nodes_map_t nodes;

	std::string get_node_name();
	bool set_node_name(const std::string& node_name_, std::unique_lock<std::mutex>& lk);
	uint64_t get_node_id();
	bool set_node_id();

public:
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

	std::vector<std::weak_ptr<XapiandServer>> servers_weak;
	std::weak_ptr<Http> weak_http;
#ifdef XAPIAND_CLUSTERING
	std::weak_ptr<Binary> weak_binary;
	std::weak_ptr<Discovery> weak_discovery;
	std::weak_ptr<Raft> weak_raft;
#endif

	DatabasePool database_pool;
	ThreadPool<> thread_pool;
	ThreadPool<> server_pool;
	ThreadPool<> autocommit_pool;
#ifdef XAPIAND_CLUSTERING
	ThreadPool<> replicator_pool;
#endif
#ifdef XAPIAND_CLUSTERING
	EndpointResolver endp_r;
#endif

	std::atomic<time_t> shutdown_asap;
	std::atomic<time_t> shutdown_now;

	State state;
	std::string cluster_name;
	std::string node_name;
	bool solo;

	ev::async async_shutdown_sig;
	std::atomic_int shutdown_sig_sig;
	void shutdown_sig(int sig);
	void async_shutdown_sig_cb(ev::async&, int);

	~XapiandManager();

	void setup_node();

	void setup_node(std::shared_ptr<XapiandServer>&& server);

	void run(const opts_t& o);

	bool is_single_node();

#ifdef XAPIAND_CLUSTERING
	void reset_state();

	bool put_node(const Node& node);
	bool get_node(const std::string& node_name, const Node** node);
	bool touch_node(const std::string& node_name, int32_t region, const Node** node=nullptr);
	void drop_node(const std::string& node_name);

	size_t get_nodes_by_region(int32_t region);

	// Return the region to which db name belongs
	int32_t get_region(const std::string& db_name);
	// Return the region to which local_node belongs
	int32_t get_region();

	std::future<bool> trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint);
#endif

	bool resolve_index_endpoint(const std::string &path, std::vector<Endpoint> &endpv, size_t n_endps=1, duration<double, std::milli> timeout=1s);

	void server_status(MsgPack&& stats);
	void get_stats_time(MsgPack&& stats, const std::string& time_req);
	void _get_stats_time(MsgPack&& stats, pos_time_t& first_time, pos_time_t& second_time);

	inline decltype(auto) get_lock() noexcept {
		return std::unique_lock<std::mutex>(qmtx);
	}
};
