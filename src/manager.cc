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

#include "manager.h"

#include "utils.h"
#include "replicator.h"
#include "endpoint.h"
#include "servers/server.h"
#include "client_binary.h"
#include "servers/http.h"
#include "servers/binary.h"
#include "servers/discovery.h"
#include "servers/raft.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <net/if.h> /* for IFF_LOOPBACK */
#include <ifaddrs.h>
#include <unistd.h>


std::regex XapiandManager::time_re = std::regex("((([01]?[0-9]|2[0-3])h)?(([0-5]?[0-9])m)?(([0-5]?[0-9])s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?(([0-5]?[0-9])m)?(([0-5]?[0-9])s)?)?", std::regex::icase | std::regex::optimize);


XapiandManager::XapiandManager(ev::loop_ref *loop_, const opts_t &o)
	: Worker(nullptr, loop_),
	  database_pool(o.dbpool_size),
	  thread_pool(o.threadpool_size),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  endp_r(o.endpoints_list_size),
	  state(State::RESET),
	  cluster_name(o.cluster_name),
	  node_name(o.node_name)
{
	// Setup node from node database directory
	std::string node_name_(get_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && stringtolower(node_name) != stringtolower(node_name_)) {
			LOG_ERR(this, "Node name %s doesn't match with the one in the cluster's database: %s!\n", node_name.c_str(), node_name_.c_str());
			assert(false);
		}
		node_name = node_name_;
	}

	// Set the id in local node.
	local_node.id = get_node_id();

	// Set addr in local node
	local_node.addr = host_address();

	async_shutdown.set<XapiandManager, &XapiandManager::async_shutdown_cb>(this);
	async_shutdown.start();
	LOG_EV(this, "\tStart async shutdown event\n");

	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	discovery->send_message(Discovery::Message::BYE, local_node.serialise());

	destroy();

	async_shutdown.stop();
	LOG_EV(this, "\tStop async shutdown event\n");

	LOG_OBJ(this, "DELETED MANAGER!\n");
}


std::string
XapiandManager::get_node_name()
{
	size_t length = 0;
	unsigned char buf[512];
	int fd = open("nodename", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = read(fd, (char *)buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			for (size_t i = 0, j = 0; (buf[j] = buf[i]); j += !isspace(buf[i++]));
		}
		close(fd);
	}
	return std::string((const char *)buf, length);
}


bool
XapiandManager::set_node_name(const std::string &node_name_, std::unique_lock<std::mutex> &lk)
{
	if (node_name_.empty()) {
		lk.unlock();
		return false;
	}

	node_name = get_node_name();
	if (!node_name.empty() && stringtolower(node_name) != stringtolower(node_name_)) {
		lk.unlock();
		return false;
	}

	if (stringtolower(node_name) != stringtolower(node_name_)) {
		node_name = node_name_;

		int fd = open("nodename", O_WRONLY | O_CREAT, 0644);
		if (fd >= 0) {
			if (write(fd, node_name.c_str(), node_name.size()) != static_cast<ssize_t>(node_name.size())) {
				assert(false);
			}
			close(fd);
		} else {
			assert(false);
		}
	}

	INFO(this, "Node %s accepted to the party!\n", node_name.c_str());
	return true;
}


uint64_t
XapiandManager::get_node_id()
{
	return random_int(0, UINT64_MAX);
}


bool
XapiandManager::set_node_id()
{
	if (random_real(0, 1.0) < 0.5) return false;

	// std::lock_guard<std::mutex> lk(qmtx);
	local_node.id = get_node_id();

	return true;
}


void
XapiandManager::setup_node()
{
	std::shared_ptr<XapiandServer> server = std::static_pointer_cast<XapiandServer>(*_children.begin());
	server->async_setup_node.send();
}


void
XapiandManager::setup_node(std::shared_ptr<XapiandServer>&& server)
{
	int new_cluster = 0;

	std::unique_lock<std::mutex> lk(qmtx);

	// Open cluster database
	cluster_endpoints.clear();
	Endpoint cluster_endpoint(".");
	cluster_endpoints.insert(cluster_endpoint);

	std::shared_ptr<Database> cluster_database;
	if (!database_pool.checkout(cluster_database, cluster_endpoints, DB_WRITABLE | DB_PERSISTENT)) {
		new_cluster = 1;
		INFO(this, "Cluster database doesn't exist. Generating database...\n");
		if (!database_pool.checkout(cluster_database, cluster_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
			assert(false);
		}
	}
	database_pool.checkin(cluster_database);

	// Get a node (any node)
	std::unique_lock<std::mutex> lk_n(nodes_mtx);

	nodes_map_t::const_iterator it(nodes.cbegin());
	for ( ; it != nodes.cend(); it++) {
		const Node &node = it->second;
		Endpoint remote_endpoint(".", &node);
		// Replicate database from the other node
#ifdef HAVE_REMOTE_PROTOCOL
		INFO(this, "Syncing cluster data from %s...\n", node.name.c_str());
		if (binary->trigger_replication(remote_endpoint, *cluster_endpoints.begin(), std::move(server))) {
			INFO(this, "Cluster data being synchronized from %s...\n", node.name.c_str());
			new_cluster = 2;
			break;
		}
#endif
	}

	lk_n.unlock();

	// Set node as ready!
	set_node_name(local_node.name, lk);
	state = State::READY;

	switch (new_cluster) {
		case 0:
			INFO(this, "Joined cluster %s: It is now online!\n", cluster_name.c_str());
			break;
		case 1:
			INFO(this, "Joined new cluster %s: It is now online!\n", cluster_name.c_str());
			break;
		case 2:
			INFO(this, "Joined cluster %s: It was already online!\n", cluster_name.c_str());
			break;
	}
}


void
XapiandManager::reset_state()
{
	if (state != State::RESET) {
		state = State::RESET;
		discovery->start();
	}
}


bool
XapiandManager::put_node(const Node &node)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	std::string node_name_lower(stringtolower(node.name));
	if (node_name_lower == stringtolower(local_node.name)) {
		local_node.touched = epoch::now();
		return false;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			if (node == node_ref) {
				node_ref.touched = epoch::now();
			}
		} catch (const std::out_of_range &err) {
			Node &node_ref = nodes[node_name_lower];
			node_ref = node;
			node_ref.touched = epoch::now();
			return true;
		} catch (...) {
			throw;
		}
	}
	return false;
}


bool
XapiandManager::get_node(const std::string &node_name, const Node **node)
{
	try {
		std::string node_name_lower(stringtolower(node_name));
		const Node &node_ref = nodes.at(node_name_lower);
		*node = &node_ref;
		return true;
	} catch (const std::out_of_range &err) {
		return false;
	}
}


bool
XapiandManager::touch_node(const std::string &node_name, int region, const Node **node)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	std::string node_name_lower(stringtolower(node_name));
	if (node_name_lower == stringtolower(local_node.name)) {
		local_node.touched = epoch::now();
		if (region != UNKNOWN_REGION) {
			local_node.region.store(region);
		}
		if (node) *node = &local_node;
		return true;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			node_ref.touched = epoch::now();
			if (region != UNKNOWN_REGION) {
				node_ref.region.store(region);
			}
			if (node) *node = &node_ref;
			return true;
		} catch (const std::out_of_range &err) {
		} catch (...) {
			throw;
		}
	}
	return false;
}


void
XapiandManager::drop_node(const std::string &node_name)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	nodes.erase(stringtolower(node_name));
}


int
XapiandManager::get_nodes_by_region(int region)
{
	int cont = 0;
	std::lock_guard<std::mutex> lk(nodes_mtx);
	for (auto it(nodes.begin()); it != nodes.end(); it++) {
		if (it->second.region.load() == region) cont++;
	}
	return cont;
}


struct sockaddr_in
XapiandManager::host_address()
{
	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		LOG_ERR(this, "ERROR: getifaddrs: %s\n", strerror(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) { // check it is IP4
				char ip[INET_ADDRSTRLEN];
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
				LOG_DISCOVERY(this, "Node IP address is %s on interface %s\n", ip, ifa->ifa_name);
				break;
			}
		}
		freeifaddrs(if_addr_struct);
	}
	return addr;
}


void
XapiandManager::sig_shutdown_handler(int sig)
{
	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	auto now = epoch::now();
	if (shutdown_now && sig != SIGTERM) {
		if (sig && shutdown_now + 1 < now) {
			INFO(this, "You insist... exiting now.\n");
			// remove pid file here, use: getpid();
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (shutdown_asap && sig != SIGTERM) {
		if (shutdown_asap + 1 < now) {
			shutdown_now = now;
			INFO(this, "Trying immediate shutdown.\n");
		}
	} else {
		shutdown_asap = now;
		switch (sig) {
			case SIGINT:
				INFO(this, "Received SIGINT scheduling shutdown...\n");
				break;
			case SIGTERM:
				INFO(this, "Received SIGTERM scheduling shutdown...\n");
				break;
			default:
				INFO(this, "Received shutdown signal, scheduling shutdown...\n");
		};
	}
	shutdown();
}


void
XapiandManager::destroy()
{
	std::lock_guard<std::mutex> lk(qmtx);

	LOG_OBJ(this, "DESTROYED MANAGER!\n");
}


void
XapiandManager::async_shutdown_cb(ev::async &, int)
{
	LOG_EV(this, "\tAsync shutdown event received!\n");

	sig_shutdown_handler(0);
}


void
XapiandManager::shutdown()
{
	Worker::shutdown();

	if (shutdown_asap) {
		discovery->send_message(Discovery::Message::BYE, local_node.serialise());
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop();
	}
}


void
XapiandManager::run(const opts_t &o)
{
	std::string msg("Listening on ");

	http = std::make_unique<Http>(share_this<XapiandManager>(), o.http_port);
	msg += http->getDescription() + ", ";

#ifdef HAVE_REMOTE_PROTOCOL
	binary = std::make_unique<Binary>(share_this<XapiandManager>(), o.binary_port);
	msg += binary->getDescription() + ", ";
#endif

	discovery = std::make_unique<Discovery>(share_this<XapiandManager>(), loop, o.discovery_port, o.discovery_group);
	msg += discovery->getDescription() + ", ";

	raft = std::make_unique<Raft>(share_this<XapiandManager>(), loop, o.raft_port, o.raft_group);
	msg += raft->getDescription() + ", ";

	msg += "at pid:" + std::to_string(getpid()) + "...\n";

	INFO(this, msg.c_str());

	INFO(this, "Starting %d server worker thread%s and %d replicator%s.\n", o.num_servers, (o.num_servers == 1) ? "" : "s", o.num_replicators, (o.num_replicators == 1) ? "" : "s");

	ThreadPool server_pool(o.num_servers);
	for (size_t i = 0; i < o.num_servers; i++) {
		std::shared_ptr<XapiandServer> server = Worker::create<XapiandServer>(share_this<XapiandManager>(), nullptr);
		server->register_server(std::make_unique<HttpServer>(server->share_this<XapiandServer>(), server->loop, http));
#ifdef HAVE_REMOTE_PROTOCOL
		server->register_server(std::make_unique<BinaryServer>(server->share_this<XapiandServer>(), server->loop, binary));
#endif
		server->register_server(std::make_unique<DiscoveryServer>(server->share_this<XapiandServer>(), server->loop, discovery));
		server->register_server(std::make_unique<RaftServer>(server->share_this<XapiandServer>(), server->loop, raft));
		server_pool.enqueue(std::move(server));
	}

	ThreadPool replicator_pool(o.num_replicators);
	for (size_t i = 0; i < o.num_replicators; i++) {
		replicator_pool.enqueue(Worker::create<XapiandReplicator>(share_this<XapiandManager>(), nullptr));
	}

	INFO(this, "Joining cluster %s...\n", cluster_name.c_str());

	discovery->start();
	raft->start();

	LOG_EV(this, "\tStarting manager loop...\n");
	loop->run();
	LOG_EV(this, "\tManager loop ended!\n");

	LOG_OBJ(this, "Waiting for servers...\n");
	server_pool.finish();
	server_pool.join();

	LOG_OBJ(this, "Waiting for replicators...\n");
	replicator_pool.finish();
	replicator_pool.join();

	LOG_OBJ(this, "Server ended!\n");
}


int
XapiandManager::get_region(const std::string &db_name)
{
	if (local_node.regions.load() == -1) {
		local_node.regions.store(sqrt(nodes.size()));
		LOG(this, "Regions: %d\n", local_node.regions.load());
	}
	std::hash<std::string> hash_fn;
	return jump_consistent_hash(hash_fn(db_name), local_node.regions.load());
}


int
XapiandManager::get_region()
{
	LOG(this, "Get_region()\n");
	if (local_node.regions.load() == -1) {
		local_node.regions.store(sqrt(nodes.size()));
		int region = jump_consistent_hash(local_node.id, local_node.regions.load());
		if (local_node.region.load() != region) {
			local_node.region.store(region);
			raft->reset();
		}
		LOG(this, "Regions: %d Region: %d\n", local_node.regions.load(), local_node.region.load());
	}
	return local_node.region.load();
}


unique_cJSON
XapiandManager::server_status()
{
	unique_cJSON root_status(cJSON_CreateObject(), cJSON_Delete);
	std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status.get(), "Connections", XapiandServer::total_clients);
	cJSON_AddNumberToObject(root_status.get(), "Http connections", XapiandServer::http_clients);
	cJSON_AddNumberToObject(root_status.get(), "Xapian remote connections", XapiandServer::binary_clients);
	cJSON_AddNumberToObject(root_status.get(), "Size thread pool", thread_pool.size());
	return root_status;
}


unique_cJSON
XapiandManager::get_stats_time(const std::string &time_req)
{
	std::smatch m;
	if (std::regex_match(time_req, m, time_re) && static_cast<size_t>(m.length()) == time_req.size() && m.length(1) != 0) {
		pos_time_t first_time, second_time;
		first_time.minute = SLOT_TIME_SECOND * (m.length(3) != 0 ? strtol(m.str(3)) : 0);
		first_time.minute += m.length(5) != 0 ? strtol(m.str(5)) : 0;
		first_time.second = m.length(7) != 0 ? strtol(m.str(7)) : 0;
		if (m.length(8) != 0) {
			second_time.minute = SLOT_TIME_SECOND * (m.length(10) != 0 ? strtol(m.str(10)) : 0);
			second_time.minute += m.length(12) != 0 ? strtol(m.str(12)) : 0;
			second_time.second = m.length(14) != 0 ? strtol(m.str(14)) : 0;
		} else {
			second_time.minute = 0;
			second_time.second = 0;
		}
		return get_stats_json(first_time, second_time);
	}

	unique_cJSON root_stats(cJSON_CreateObject(), cJSON_Delete);
	cJSON_AddStringToObject(root_stats.get(), "Error in time argument input", "Incorrect input.");
	return root_stats;
}


unique_cJSON
XapiandManager::get_stats_json(pos_time_t &first_time, pos_time_t &second_time)
{
	unique_cJSON root_stats(cJSON_CreateObject(), cJSON_Delete);
	unique_cJSON time_period(cJSON_CreateObject(), cJSON_Delete);

	std::unique_lock<std::mutex> lk(XapiandServer::static_mutex);
	update_pos_time();
	auto now_time = std::chrono::system_clock::to_time_t(init_time);
	auto b_time_cpy = b_time;
	auto stats_cnt_cpy = stats_cnt;
	lk.unlock();

	auto seconds = first_time.minute == 0 ? true : false;
	uint16_t start, end;
	if (second_time.minute == 0 && second_time.second == 0) {
		start = first_time.minute * SLOT_TIME_SECOND + first_time.second;
		end = 0;
		first_time.minute = (first_time.minute > b_time_cpy.minute ? SLOT_TIME_MINUTE + b_time_cpy.minute : b_time_cpy.minute) - first_time.minute;
		first_time.second = (first_time.second > b_time_cpy.second ? SLOT_TIME_SECOND + b_time_cpy.second : b_time_cpy.second) - first_time.second;
	} else {
		start = second_time.minute * SLOT_TIME_SECOND + second_time.second;
		end = first_time.minute * SLOT_TIME_SECOND + first_time.second;
		first_time.minute = (second_time.minute > b_time_cpy.minute ? SLOT_TIME_MINUTE + b_time_cpy.minute : b_time_cpy.minute) - second_time.minute;
		first_time.second = (second_time.second > b_time_cpy.second ? SLOT_TIME_SECOND + b_time_cpy.second : b_time_cpy.second) - second_time.second;
	}

	if (end > start) {
		cJSON_AddStringToObject(root_stats.get(), "Error in time argument input", "First argument must be less or equal than the second.");
	} else {
		std::vector<uint64_t> cnt{0, 0, 0};
		std::vector<double> tm_cnt{0.0, 0.0, 0.0};
		cJSON_AddStringToObject(time_period.get(), "System time", ctime(&now_time));
		if (seconds) {
			auto aux = first_time.second + start - end;
			if (aux < SLOT_TIME_SECOND) {
				add_stats_sec(first_time.second, aux, cnt, tm_cnt, stats_cnt_cpy);
			} else {
				add_stats_sec(first_time.second, SLOT_TIME_SECOND - 1, cnt, tm_cnt, stats_cnt_cpy);
				add_stats_sec(0, aux % SLOT_TIME_SECOND, cnt, tm_cnt, stats_cnt_cpy);
			}
		} else {
			auto aux = first_time.minute + (start - end) / SLOT_TIME_SECOND;
			if (aux < SLOT_TIME_MINUTE) {
				add_stats_min(first_time.minute, aux, cnt, tm_cnt, stats_cnt_cpy);
			} else {
				add_stats_min(first_time.second, SLOT_TIME_MINUTE - 1, cnt, tm_cnt, stats_cnt_cpy);
				add_stats_min(0, aux % SLOT_TIME_MINUTE, cnt, tm_cnt, stats_cnt_cpy);
			}
		}
		auto p_time = now_time - start;
		cJSON_AddStringToObject(time_period.get(), "Period start", ctime(&p_time));
		p_time = now_time - end;
		cJSON_AddStringToObject(time_period.get(), "Period end", ctime(&p_time));
		cJSON_AddItemToObject(root_stats.get(), "Time", time_period.release());
		cJSON_AddNumberToObject(root_stats.get(), "Docs index", cnt[0]);
		cJSON_AddNumberToObject(root_stats.get(), "Number searches", cnt[1]);
		cJSON_AddNumberToObject(root_stats.get(), "Docs deleted", cnt[2]);
		cJSON_AddNumberToObject(root_stats.get(), "Average time indexing", cnt[0] == 0 ? 0 : tm_cnt[0] / cnt[0]);
		cJSON_AddNumberToObject(root_stats.get(), "Average search time", cnt[1] == 0 ? 0 : tm_cnt[1] / cnt[1]);
		cJSON_AddNumberToObject(root_stats.get(), "Average deletion time", cnt[2] == 0 ? 0 : tm_cnt[2] / cnt[2]);
	}

	return root_stats;
}
