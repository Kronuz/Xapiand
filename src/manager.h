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
	bool daemonize;
	bool chert;
	std::string database;
	std::string cluster_name;
	std::string node_name;
	unsigned int http_port;
	unsigned int binary_port;
	unsigned int discovery_port;
	unsigned int raft_port;
	std::string pidfile;
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


class BaseUDP;
class Binary;
class Discovery;
class Http;
class HttpClient;
class Raft;
class XapiandServer;


class XapiandManager : public Worker  {
	using nodes_map_t = std::unordered_map<std::string, Node>;

	std::mutex qmtx;

	Endpoints cluster_endpoints;

	static std::regex time_re;

	XapiandManager(ev::loop_ref *loop_, const opts_t &o);

	void async_shutdown_cb(ev::async &watcher, int revents);
	struct sockaddr_in host_address();
	void destroy();

	friend Worker;

protected:
	std::mutex nodes_mtx;
	nodes_map_t nodes;

	std::string get_node_name();
	bool set_node_name(const std::string &node_name_, std::unique_lock<std::mutex> &lk);
	uint64_t get_node_id();
	bool set_node_id();

public:
	enum class State {
		BAD,
		READY,
		SETUP,
		WAITING_,
		WAITING,
		RESET
	};

	DatabasePool database_pool;
	ThreadPool<> thread_pool;

	static std::atomic<time_t> shutdown_asap;
	static std::atomic<time_t> shutdown_now;

	ev::async async_shutdown;

	EndpointResolver endp_r;

	std::shared_ptr<Raft> raft;
	std::shared_ptr<Discovery> discovery;
	std::shared_ptr<Binary> binary;

	State state;
	std::string cluster_name;
	std::string node_name;

	~XapiandManager();

	void setup_node();

	void setup_node(std::shared_ptr<XapiandServer>&& server);

	void run(const opts_t &o);
	void sig_shutdown_handler(int sig);
	void shutdown();

	void reset_state();
	bool is_single_node();

	bool put_node(const Node &node);
	bool get_node(const std::string &node_name, const Node **node);
	bool touch_node(const std::string &node_name, int region, const Node **node=nullptr);
	void drop_node(const std::string &node_name);
	size_t get_nodes_by_region(int region);

	// Return the region to which db name belongs
	int get_region(const std::string &db_name);
	// Return the region to which local_node belongs
	int get_region();

	std::future<bool> trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);
	std::future<bool> store(const Endpoints &endpoints, const Xapian::docid &did, const std::string &filename);

	unique_cJSON server_status();
	unique_cJSON get_stats_time(const std::string &time_req);
	unique_cJSON get_stats_json(pos_time_t &first_time, pos_time_t &second_time);

	inline decltype(auto) get_lock() noexcept {
		return std::unique_lock<std::mutex>(qmtx);
	}
};
