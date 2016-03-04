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
#include "database_autocommit.h"
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
#include <sysexits.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <net/if.h> /* for IFF_LOOPBACK */
#include <ifaddrs.h>
#include <unistd.h>

#define NANOSEC 1e-9


static const std::regex time_re("((([01]?[0-9]|2[0-3])h)?(([0-5]?[0-9])m)?(([0-5]?[0-9])s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?(([0-5]?[0-9])m)?(([0-5]?[0-9])s)?)?", std::regex::icase | std::regex::optimize);


std::atomic<time_t> XapiandManager::initialized(0);
std::atomic<time_t> XapiandManager::shutdown_asap(0);
std::atomic<time_t> XapiandManager::shutdown_now(0);


XapiandManager::XapiandManager(ev::loop_ref* loop_, const opts_t& o)
	: Worker(nullptr, loop_),
	  database_pool(o.dbpool_size),
	  thread_pool("W%02zu", o.threadpool_size),
	  server_pool("S%02zu", o.num_servers),
	  autocommit_pool("C%02zu", o.num_committers),
#ifdef XAPIAND_CLUSTERING
	  replicator_pool("R%02zu", o.num_replicators),
	  endp_r(o.endpoints_list_size),
#endif
	  async_shutdown(*loop),
	  state(State::RESET),
	  cluster_name(o.cluster_name),
	  node_name(o.node_name),
	  solo(o.solo)
{
	// Setup node from node database directory
	std::string node_name_(get_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && lower_string(node_name) != lower_string(node_name_)) {
			L_CRIT(this, "Node name %s doesn't match with the one in the cluster's database: %s!", node_name.c_str(), node_name_.c_str());
			exit(EX_CONFIG);
		}
		node_name = node_name_;
	}

	// Set the id in local node.
	local_node.id = get_node_id();

	// Set addr in local node
	local_node.addr = host_address();

	async_shutdown.set<XapiandManager, &XapiandManager::async_shutdown_cb>(this);
	async_shutdown.start();
	L_EV(this, "Start manager's async shutdown event");

	L_OBJ(this, "CREATED XAPIAN MANAGER!");
}


XapiandManager::~XapiandManager()
{
	async_shutdown.stop();
	L_EV(this, "Stop async shutdown event");

	destroy();

	L_OBJ(this, "DELETED XAPIAN MANAGER!");
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
XapiandManager::set_node_name(const std::string& node_name_, std::unique_lock<std::mutex>& lk)
{
	if (node_name_.empty()) {
		lk.unlock();
		return false;
	}

	node_name = get_node_name();

	std::string lower_node_name = lower_string(node_name);
	std::string _lower_node_name = lower_string(node_name_);

	if (!node_name.empty() && lower_node_name != _lower_node_name) {
		lk.unlock();
		return false;
	}

	if (lower_node_name != _lower_node_name) {
		node_name = node_name_;

		int fd = open("nodename", O_WRONLY | O_CREAT, 0644);
		if (fd >= 0) {
			if (write(fd, node_name.c_str(), node_name.size()) != static_cast<ssize_t>(node_name.size())) {
				L_CRIT(nullptr, "Cannot write in nodename file");
				exit(EX_IOERR);
			}
			close(fd);
		} else {
			L_CRIT(nullptr, "Cannot open or create the nodename file");
			exit(EX_NOINPUT);
		}
	}

	L_NOTICE(this, "Node %s accepted to the party!", node_name.c_str());
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
	for (auto& s : servers) {
		if (auto server = s.lock()) {
			server->async_setup_node.send();
			return;
		}
	}
}


void
XapiandManager::setup_node(std::shared_ptr<XapiandServer>&& server)
{
	int new_cluster = 0;

	std::unique_lock<std::mutex> lk(qmtx);

	// Open cluster database
	cluster_endpoints.clear();
	Endpoint cluster_endpoint(".");
	cluster_endpoints.add(cluster_endpoint);

	std::shared_ptr<Database> cluster_database;
	if (!database_pool.checkout(cluster_database, cluster_endpoints, DB_WRITABLE | DB_PERSISTENT | DB_NOWAL)) {
		new_cluster = 1;
		L_INFO(this, "Cluster database doesn't exist. Generating database...");
		if (!database_pool.checkout(cluster_database, cluster_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL)) {
			L_CRIT(nullptr, "Cannot generate cluster database");
			exit(EX_CANTCREAT);
		}
	}
	database_pool.checkin(cluster_database);

	// Get a node (any node)
	std::unique_lock<std::mutex> lk_n(nodes_mtx);

	for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
		const Node &node = it->second;
		Endpoint remote_endpoint(".", &node);
		// Replicate database from the other node
#ifdef XAPIAND_CLUSTERING
		L_INFO(this, "Syncing cluster data from %s...", node.name.c_str());

		auto ret = trigger_replication(remote_endpoint, cluster_endpoints[0]);
		if (ret.get()) {
			L_INFO(this, "Cluster data being synchronized from %s...", node.name.c_str());
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
			L_NOTICE(this, "Joined cluster %s: It is now online!", cluster_name.c_str());
			break;
		case 1:
			L_NOTICE(this, "Joined new cluster %s: It is now online!", cluster_name.c_str());
			break;
		case 2:
			L_NOTICE(this, "Joined cluster %s: It was already online!", cluster_name.c_str());
			break;
	}
}

struct sockaddr_in
XapiandManager::host_address()
{
	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		L_ERR(this, "ERROR: getifaddrs: %s", strerror(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) { // check it is IP4
				char ip[INET_ADDRSTRLEN];
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
				L_NOTICE(this, "Node IP address is %s on interface %s", ip, ifa->ifa_name);
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
	if (!XapiandManager::initialized) {
		return;
	}
	XapiandManager::initialized = 0;

	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	auto now = epoch::now<>();

	if (XapiandManager::shutdown_now && sig != SIGTERM) {
		if (sig && now > XapiandManager::shutdown_asap + 1 && now < XapiandManager::shutdown_asap + 4) {
			L_WARNING(this, "You insisted... Xapiand exiting now!");
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (XapiandManager::shutdown_asap && sig != SIGTERM) {
		if (sig && now > XapiandManager::shutdown_asap + 1 && now < XapiandManager::shutdown_asap + 4) {
			XapiandManager::shutdown_now = now;
			L_INFO(this, "Trying immediate shutdown.");
		} else if (sig == 0) {
			XapiandManager::shutdown_now = now;
		}
	} else {
		switch (sig) {
			case SIGINT:
				L_INFO(this, "Received SIGINT scheduling shutdown...");
				break;
			case SIGTERM:
				L_INFO(this, "Received SIGTERM scheduling shutdown...");
				break;
			default:
				L_INFO(this, "Received shutdown signal, scheduling shutdown...");
		};
	}

	if (now > XapiandManager::shutdown_asap + 1) {
		XapiandManager::shutdown_asap = now;
	}

	if (XapiandServer::http_clients <= 0) {
		XapiandManager::shutdown_now = now;
	}

	bool shutdown_asap = bool(XapiandManager::shutdown_asap);
	bool shutdown_now = bool(XapiandManager::shutdown_now);

	shutdown(shutdown_asap, shutdown_now);

	XapiandManager::initialized = now;
}


void
XapiandManager::destroy()
{
	L_OBJ(this, "DESTROYING XAPIAN MANAGER!");

	if (auto discovery = proto_discovery.lock()) {
		L_INFO(this, "Waving goodbye to cluster %s!", cluster_name.c_str());
		discovery->stop();
	}

	L_DEBUG(this, "Finishing servers pool!");
	server_pool.finish();

#ifdef XAPIAND_CLUSTERING
	L_DEBUG(this, "Finishing replicators pool!");
	replicator_pool.finish();
#endif

	L_DEBUG(this, "Finishing commiters pool!");
	autocommit_pool.finish();

	L_OBJ(this, "DESTROYED XAPIAN MANAGER!");
}


void
XapiandManager::async_shutdown_cb(ev::async&, int)
{
	L_OBJ(this , "ASYNC SHUTDOWN XAPIAN MANAGER!");

	L_EV_BEGIN(this, "XapiandManager::async_shutdown_cb:BEGIN");
	L_EV(this, "Async shutdown event received!");

	sig_shutdown_handler(0);
	L_EV_END(this, "XapiandManager::async_shutdown_cb:END");
}


void
XapiandManager::shutdown(bool asap, bool now)
{
	L_OBJ(this , "SHUTDOWN XAPIAN MANAGER! (%d %d)", asap, now);

	destroy();

	Worker::shutdown(asap, now);

	if (now) {
		L_EV(this, "Breaking Manager loop!");
		break_loop();
	}
}


void
XapiandManager::run(const opts_t& o)
{
	std::string msg("Listening on ");

	auto manager = share_this<XapiandManager>();

	auto http = Worker::make_shared<Http>(manager, loop, o.http_port);
	msg += http->getDescription() + ", ";

#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<Binary> binary;
	std::shared_ptr<Discovery> discovery;
	std::shared_ptr<Raft> raft;
	if (!solo) {
		binary = Worker::make_shared<Binary>(manager, loop, o.binary_port);
		msg += binary->getDescription() + ", ";

		discovery = Worker::make_shared<Discovery>(manager, loop, o.discovery_port, o.discovery_group);
		msg += discovery->getDescription() + ", ";

		raft = Worker::make_shared<Raft>(manager, loop, o.raft_port, o.raft_group);
		msg += raft->getDescription() + ", ";
	}
#endif

	msg += "at pid:" + std::to_string(getpid()) + " ...";

	L_NOTICE(this, msg.c_str());

	for (size_t i = 0; i < o.num_servers; ++i) {
		std::shared_ptr<XapiandServer> server = Worker::make_shared<XapiandServer>(manager, nullptr);
		servers.push_back(server);
		Worker::make_shared<HttpServer>(server, server->loop, http);
#ifdef XAPIAND_CLUSTERING
		if (!solo) {
			binary->add_server(Worker::make_shared<BinaryServer>(server, server->loop, binary));
			Worker::make_shared<DiscoveryServer>(server, server->loop, discovery);
			Worker::make_shared<RaftServer>(server, server->loop, raft);
		}
#endif
		server_pool.enqueue(std::move(server));
	}

#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		for (size_t i = 0; i < o.num_replicators; ++i) {
			replicator_pool.enqueue(Worker::make_shared<XapiandReplicator>(manager, nullptr));
		}
	}
#endif

	for (size_t i = 0; i < o.num_committers; ++i) {
		auto dbcommit = Worker::make_shared<DatabaseAutocommit>(manager, nullptr);
		autocommit_pool.enqueue(dbcommit);
	}

	// Make server protocols weak:
	proto_http = std::move(http);
#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		L_INFO(this, "Joining cluster %s...", cluster_name.c_str());
		discovery->start();

		proto_binary = std::move(binary);
		proto_discovery = std::move(discovery);
		proto_raft = std::move(raft);
	}
#endif

	msg = "Started " + std::to_string(o.num_servers) + ((o.num_servers == 1) ? " server" : " servers");
	msg += ", " + std::to_string(o.threadpool_size) +( (o.threadpool_size == 1) ? " worker thread" : " worker threads");
#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		msg += ", " + std::to_string(o.num_replicators) + ((o.num_replicators == 1) ? " replicator" : " replicators");
	}
#endif
	msg += ", " + std::to_string(o.num_committers) + ((o.num_committers == 1) ? " autocommitter" : " autocommitters");
	L_NOTICE(this, msg.c_str());

	XapiandManager::initialized = epoch::now<>();

	L_EV(this, "Starting manager loop...");
	loop->run();
	L_EV(this, "Manager loop ended!");

	L_DEBUG(this, "Waiting for %zu server%s...", server_pool.size(), (server_pool.size() == 1) ? "" : "s");
	server_pool.join();

#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		L_DEBUG(this, "Waiting for %zu replicator%s...", replicator_pool.size(), (replicator_pool.size() == 1) ? "" : "s");
		replicator_pool.join();
	}
#endif

	L_DEBUG(this, "Waiting for %zu committer%s...", autocommit_pool.size(), (autocommit_pool.size() == 1) ? "" : "s");
	autocommit_pool.join();

	L_DEBUG(this, "Finishing worker threads pool!");
	thread_pool.finish();
	L_DEBUG(this, "Waiting for %zu worker thread%s...", thread_pool.size(), (thread_pool.size() == 1) ? "" : "s");
	thread_pool.join();

	L_DEBUG(this, "Server ended!");
}


bool
XapiandManager::is_single_node()
{
	return solo || (nodes.size() == 0);
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::reset_state()
{
	if (state != State::RESET) {
		state = State::RESET;
		if (auto discovery = proto_discovery.lock()) {
			discovery->start();
		}
	}
}


bool
XapiandManager::put_node(const Node& node)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	std::string lower_node_name(lower_string(node.name));
	if (lower_node_name == lower_string(local_node.name)) {
		local_node.touched = epoch::now<>();
		return false;
	} else {
		try {
			Node &node_ref = nodes.at(lower_node_name);
			if (node == node_ref) {
				node_ref.touched = epoch::now<>();
			}
		} catch (const std::out_of_range &err) {
			Node &node_ref = nodes[lower_node_name];
			node_ref = node;
			node_ref.touched = epoch::now<>();
			return true;
		} catch (...) {
			throw;
		}
	}
	return false;
}


bool
XapiandManager::get_node(const std::string& node_name, const Node** node)
{
	try {
		const Node &node_ref = nodes.at(lower_string(node_name));
		*node = &node_ref;
		return true;
	} catch (const std::out_of_range &err) {
		return false;
	}
}


bool
XapiandManager::touch_node(const std::string& node_name, int region, const Node** node)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	std::string lower_node_name(lower_string(node_name));
	if (lower_node_name == lower_string(local_node.name)) {
		local_node.touched = epoch::now<>();
		if (region != UNKNOWN_REGION) {
			local_node.region.store(region);
		}
		if (node) *node = &local_node;
		return true;
	} else {
		try {
			Node &node_ref = nodes.at(lower_node_name);
			node_ref.touched = epoch::now<>();
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
XapiandManager::drop_node(const std::string& node_name)
{
	std::lock_guard<std::mutex> lk(nodes_mtx);
	nodes.erase(lower_string(node_name));
}


size_t
XapiandManager::get_nodes_by_region(int region)
{
	size_t cont = 0;
	std::lock_guard<std::mutex> lk(nodes_mtx);
	for (auto it(nodes.begin()); it != nodes.end(); ++it) {
		if (it->second.region.load() == region) ++cont;
	}
	return cont;
}


int
XapiandManager::get_region(const std::string& db_name)
{
	if (local_node.regions.load() == -1) {
		local_node.regions.store(sqrt(nodes.size()));
	}
	std::hash<std::string> hash_fn;
	return jump_consistent_hash(hash_fn(db_name), local_node.regions.load());
}


int
XapiandManager::get_region()
{
	if (auto raft = proto_raft.lock()) {
		if (local_node.regions.load() == -1) {
			if (is_single_node()) {
				local_node.regions.store(1);
				local_node.region.store(0);
				raft->stop();
			} else {
				raft->start();
				local_node.regions.store(sqrt(nodes.size() + 1));
				int region = jump_consistent_hash(local_node.id, local_node.regions.load());
				if (local_node.region.load() != region) {
					local_node.region.store(region);
					raft->reset();
				}
			}
			L_RAFT(this, "Regions: %d Region: %d", local_node.regions.load(), local_node.region.load());
		}
	}
	return local_node.region.load();
}


std::future<bool>
XapiandManager::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint)
{
	if (auto binary = proto_binary.lock()) {
		return binary->trigger_replication(src_endpoint, dst_endpoint);
	}
	return std::future<bool>();
}

#endif

bool
XapiandManager::resolve_index_endpoint(const std::string &path, std::vector<Endpoint> &endpv, size_t n_endps, duration<double, std::milli> timeout)
{
#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		return endp_r.resolve_index_endpoint(path, share_this<XapiandManager>(), endpv, n_endps, timeout);
	}
	else
#endif
	{
		endpv.push_back(Endpoint(path));
		return true;
	}
}


void
XapiandManager::server_status(MsgPack&& stats)
{
	std::lock_guard<std::mutex> lk(XapiandServer::static_mutex);
	stats["connections"] = XapiandServer::total_clients.load();
	stats["http_connections"] = XapiandServer::http_clients.load();
	stats["workers_pool_size"] = thread_pool.size();
	stats["servers_pool_size"] = server_pool.size();
	stats["committers_pool_size"] = autocommit_pool.size();
#ifdef XAPIAND_CLUSTERING
	if(!solo) {
		stats["binary_connections"] = XapiandServer::binary_clients.load();
		stats["replicators_pool_size"] = replicator_pool.size();
	}
#endif
}


void
XapiandManager::get_stats_time(MsgPack&& stats, const std::string& time_req)
{
	std::smatch m;
	if (std::regex_match(time_req, m, time_re) && static_cast<size_t>(m.length()) == time_req.size() && m.length(1) != 0) {
		pos_time_t first_time, second_time;
		if (m.length(8) != 0) {
			first_time.minute = SLOT_TIME_SECOND * (m.length(3) != 0 ? std::stoi(m.str(3)) : 0);
			first_time.minute += m.length(5) != 0 ? std::stoi(m.str(5)) : 0;
			first_time.second = m.length(7) != 0 ? std::stoi(m.str(7)) : 0;
			second_time.minute = SLOT_TIME_SECOND * (m.length(10) != 0 ? std::stoi(m.str(10)) : 0);
			second_time.minute += m.length(12) != 0 ? std::stoi(m.str(12)) : 0;
			second_time.second = m.length(14) != 0 ? std::stoi(m.str(14)) : 0;
		} else {
			first_time.minute = 0;
			first_time.second = 0;
			second_time.minute = SLOT_TIME_SECOND * (m.length(3) != 0 ? std::stoi(m.str(3)) : 0);
			second_time.minute += m.length(5) != 0 ? std::stoi(m.str(5)) : 0;
			second_time.second = m.length(7) != 0 ? std::stoi(m.str(7)) : 0;
		}
		return _get_stats_time(std::move(stats), first_time, second_time);
	}
	throw MSG_ClientError("Incorrect input: %s", time_req.c_str());
}


void
XapiandManager::_get_stats_time(MsgPack&& stats, pos_time_t& first_time, pos_time_t& second_time)
{
	std::unique_lock<std::mutex> lk(XapiandServer::static_mutex);
	update_pos_time();
	auto now_time = std::chrono::system_clock::to_time_t(init_time);
	auto b_time_cpy = b_time;
	auto stats_cnt_cpy = stats_cnt;
	lk.unlock();

	unsigned end, start;
	if (first_time.minute == 0 && first_time.second == 0) {
		start = second_time.minute * SLOT_TIME_SECOND + second_time.second;
		end = 0;
		second_time.minute = (second_time.minute > b_time_cpy.minute ? SLOT_TIME_MINUTE + b_time_cpy.minute : b_time_cpy.minute) - second_time.minute;
		second_time.second = (second_time.second > b_time_cpy.second ? SLOT_TIME_SECOND + b_time_cpy.second : b_time_cpy.second) - second_time.second;
	} else {
		start = second_time.minute * SLOT_TIME_SECOND + second_time.second;
		end = first_time.minute * SLOT_TIME_SECOND + first_time.second;
		second_time.minute = (second_time.minute > b_time_cpy.minute ? SLOT_TIME_MINUTE + b_time_cpy.minute : b_time_cpy.minute) - second_time.minute;
		second_time.second = (second_time.second > b_time_cpy.second ? SLOT_TIME_SECOND + b_time_cpy.second : b_time_cpy.second) - second_time.second;
	}

	if (end > start) {
		throw MSG_ClientError("First argument must be less or equal than the second");
	} else {
		std::vector<uint64_t> cnt{ 0, 0, 0, 0 };
		std::vector<double> tm_cnt{ 0.0, 0.0, 0.0, 0.0 };
		if (start < SLOT_TIME_SECOND) {
			auto aux = second_time.second + start - end;
			if (aux < SLOT_TIME_SECOND) {
				add_stats_sec(second_time.second, aux, cnt, tm_cnt, stats_cnt_cpy);
			} else {
				add_stats_sec(second_time.second, SLOT_TIME_SECOND - 1, cnt, tm_cnt, stats_cnt_cpy);
				add_stats_sec(0, aux % SLOT_TIME_SECOND, cnt, tm_cnt, stats_cnt_cpy);
			}
		} else {
			auto aux = second_time.minute + (start - end) / SLOT_TIME_SECOND;
			if (aux < SLOT_TIME_MINUTE) {
				add_stats_min(second_time.minute, aux, cnt, tm_cnt, stats_cnt_cpy);
			} else {
				add_stats_min(second_time.minute, SLOT_TIME_MINUTE - 1, cnt, tm_cnt, stats_cnt_cpy);
				add_stats_min(0, aux % SLOT_TIME_MINUTE, cnt, tm_cnt, stats_cnt_cpy);
			}
		}

		stats["system_time"] = ctime(&now_time);
		auto p_time = now_time - start;
		MsgPack time_period = stats["period"];
		time_period["start"] = ctime(&p_time);
		p_time = now_time - end;
		time_period["end"] = ctime(&p_time);

		stats["docs_indexed"] = cnt[0];
		stats["num_searches"] = cnt[1];
		stats["docs_deleted"] = cnt[2];
		stats["docs_updated"] = cnt[3];
		stats["avg_time_index"]  = cnt[0] == 0 ? 0.0 : (tm_cnt[0] / cnt[0]) * NANOSEC;
		stats["avg_time_search"] = cnt[1] == 0 ? 0.0 : (tm_cnt[1] / cnt[1]) * NANOSEC;
		stats["avg_time_delete"] = cnt[2] == 0 ? 0.0 : (tm_cnt[2] / cnt[2]) * NANOSEC;
		stats["avg_time_update"] = cnt[3] == 0 ? 0.0 : (tm_cnt[3] / cnt[3]) * NANOSEC;
	}
}
