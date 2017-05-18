/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <algorithm>                         // for move
#include <arpa/inet.h>                       // for inet_ntop
#include <atomic>                            // for atomic, atomic_int
#include <chrono>                            // for duration, system_clock
#include <cstdint>                           // for uint64_t, UINT64_MAX
#include <ctime>                             // for time_t, ctime, NULL
#include <ctype.h>                           // for isspace
#include <exception>                         // for exception
#include <functional>                        // for __base
#include <ifaddrs.h>                         // for ifaddrs, freeifaddrs
#include <memory>                            // for allocator, shared_ptr
#include <mutex>                             // for mutex, lock_guard, uniqu...
#include <net/if.h>                          // for IFF_LOOPBACK
#include <netinet/in.h>                      // for sockaddr_in, INET_ADDRST...
#include <ratio>                             // for milli
#include <regex>                             // for smatch, regex, operator|
#include <stdlib.h>                          // for size_t, exit
#include <string.h>                          // for strerror
#include <string>                            // for string, basic_string
#include <sys/errno.h>                       // for __error, errno
#include <sys/fcntl.h>                       // for O_CLOEXEC, O_CREAT, O_RD...
#include <sys/signal.h>                      // for SIGTERM, SIGINT
#include <sys/socket.h>                      // for AF_INET, sockaddr
#include <sys/types.h>                       // for uint64_t
#include <sysexits.h>                        // for EX_IOERR, EX_NOINPUT
#include <unistd.h>                          // for ssize_t, getpid
#include <unordered_map>                     // for __hash_map_const_iterator
#include <utility>                           // for pair
#include <vector>                            // for vector
#include <xapian.h>                          // for Error

#include "async_fsync.h"                     // for AsyncFsync
#include "atomic_shared_ptr.h"               // for atomic_shared_ptr
#include "database.h"                        // for DatabasePool
#include "database_autocommit.h"             // for DatabaseAutocommit
#include "database_handler.h"                // for DatabaseHandler
#include "database_utils.h"                  // for RESERVED_TYPE, DB_NOWAL
#include "endpoint.h"                        // for Node, Endpoint, local_node
#include "ev/ev++.h"                         // for async, loop_ref (ptr only)
#include "exception.h"                       // for Exit, ClientError, Excep...
#include "http_parser.h"                     // for http_method
#include "io_utils.h"                        // for close, open, read, write
#include "log.h"                             // for L_CALL, L_DEBUG
#include "memory_stats.h"                    // for get_total_ram, get_total_virtual_memor...
#include "msgpack.h"                         // for MsgPack, object::object
#include "serialise.h"                       // for TERM_STR
#include "servers/http.h"                    // for Http
#include "servers/server.h"                  // for XapiandServer, XapiandSe...
#include "servers/server_http.h"             // for HttpServer
#include "threadpool.h"                      // for ThreadPool
#include "utils.h"                           // for Stats::Pos, SLOT_TIME_SE...
#include "worker.h"                          // for Worker, enable_make_shared


#if defined(XAPIAND_V8)
#include "v8pp/v8pp.h"
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include "chaipp/chaipp.h"
#endif

#ifndef L_MANAGER
#define L_MANAGER_DEFINED
#define L_MANAGER L_TEST
#endif


static const std::regex time_re("(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)(\\.\\.(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)?)?", std::regex::icase | std::regex::optimize);


constexpr const char* const XapiandManager::StateNames[];


std::shared_ptr<XapiandManager> XapiandManager::manager;


void sig_exit(int sig) {
	if (sig < 0) {
		exit(-sig);
	} else if (XapiandManager::manager) {
		XapiandManager::manager->signal_sig(sig);
	}
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, const opts_t& o)
	: Worker(nullptr, ev_loop_, ev_flags_),
	  database_pool(o.dbpool_size),
	  schemas(o.dbpool_size),
	  thread_pool("W%02zu", o.threadpool_size),
	  server_pool("S%02zu", o.num_servers),
#ifdef XAPIAND_CLUSTERING
	  replicator_pool("R%02zu", o.num_replicators),
	  endp_r(o.endpoints_list_size),
#endif
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  cluster_name(o.cluster_name),
	  node_name(o.node_name),
#ifdef XAPIAND_CLUSTERING
	  solo(o.solo),
#else
	  solo(true),
#endif
	  atom_sig(0),
	  signal_sig_async(*ev_loop)
{
	// Set the id in local node.
	auto local_node_ = local_node.load();
	auto node_copy = std::make_unique<Node>(*local_node_);
	node_copy->id = get_node_id();

	// Setup node from node database directory
	std::string node_name_(load_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && lower_string(node_name) != lower_string(node_name_)) {
			node_name = "~";
		} else {
			node_name = node_name_;
		}
	}
	if (solo) {
		if (node_name.empty()) {
			node_name = name_generator();
		}
	}
	node_copy->name = node_name;

	// Set addr in local node
	node_copy->addr = host_address();

	local_node = std::shared_ptr<const Node>(node_copy.release());

	signal_sig_async.set<XapiandManager, &XapiandManager::signal_sig_async_cb>(this);
	signal_sig_async.start();

	L_OBJ(this, "CREATED XAPIAN MANAGER!");
}


XapiandManager::~XapiandManager()
{
	destroyer();

	L_OBJ(this, "DELETED XAPIAN MANAGER!");
}


std::string
XapiandManager::load_node_name()
{
	L_CALL(this, "XapiandManager::load_node_name()");

	ssize_t length = 0;
	char buf[512];
	int fd = io::open("nodename", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = io::read(fd, buf, sizeof(buf) - 1);
		io::close(fd);
		if (length < 0) length = 0;
		buf[length] = '\0';
		for (size_t i = 0, j = 0; (buf[j] = buf[i]); j += !isspace(buf[i++]));

	}
	return std::string(buf, length);
}


void
XapiandManager::save_node_name(const std::string& node_name)
{
	L_CALL(this, "XapiandManager::save_node_name(%s)", node_name.c_str());

	int fd = io::open("nodename", O_WRONLY | O_CREAT, 0644);
	if (fd >= 0) {
		if (io::write(fd, node_name.c_str(), node_name.size()) != static_cast<ssize_t>(node_name.size())) {
			L_CRIT(nullptr, "Cannot write in nodename file");
			sig_exit(-EX_IOERR);
		}
		io::close(fd);
	} else {
		L_CRIT(nullptr, "Cannot open or create the nodename file");
		sig_exit(-EX_NOINPUT);
	}
}


std::string
XapiandManager::set_node_name(const std::string& node_name_)
{
	L_CALL(this, "XapiandManager::set_node_name(%s)", node_name_.c_str());

	node_name = load_node_name();

	if (node_name.empty()) {
		if (!node_name_.empty()) {
			// Ignore empty node_name
			node_name = node_name_;
			save_node_name(node_name);
		}
	}

	return node_name;
}


uint64_t
XapiandManager::load_node_id()
{
	L_CALL(this, "XapiandManager::load_node_id()");

	uint64_t node_id = 0;
	ssize_t length = 0;
	char buf[512];
	int fd = io::open("node", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = io::read(fd, buf, sizeof(buf) - 1);
		io::close(fd);
		if (length < 0) length = 0;
		buf[length] = '\0';
		for (size_t i = 0, j = 0; (buf[j] = buf[i]); j += !isspace(buf[i++]));
		try {
			node_id = unserialise_node_id(std::string(buf, length));
		} catch (...) {
			L_CRIT(nullptr, "Cannot load node_id!");
			sig_exit(-EX_IOERR);
		}
	}
	return node_id;
}


void
XapiandManager::save_node_id(uint64_t node_id)
{
	L_CALL(this, "XapiandManager::save_node_id(%llu)", node_id);

	int fd = io::open("node", O_WRONLY | O_CREAT, 0644);
	if (fd >= 0) {
		auto node_id_str = serialise_node_id(node_id);
		if (io::write(fd, node_id_str.data(), node_id_str.size()) != static_cast<ssize_t>(node_id_str.size())) {
			L_CRIT(nullptr, "Cannot write in node file");
			sig_exit(-EX_IOERR);
		}
		io::close(fd);
	} else {
		L_CRIT(nullptr, "Cannot open or create the node file");
		sig_exit(-EX_NOINPUT);
	}
}


uint64_t
XapiandManager::get_node_id()
{
	L_CALL(this, "XapiandManager::get_node_id()");

	uint64_t node_id = load_node_id();

	if (!node_id) {
		node_id = random_int(1, UINT64_MAX - 1);
		save_node_id(node_id);
	}

	return node_id;
}


void
XapiandManager::setup_node()
{
	L_CALL(this, "XapiandManager::setup_node()");

	for (const auto& weak_server : servers_weak) {
		if (auto server = weak_server.lock()) {
			server->setup_node_async.send();
			return;
		}
	}
	L_WARNING(this, "Cannot setup node: No servers!");
}


void
XapiandManager::setup_node(std::shared_ptr<XapiandServer>&& /*server*/)
{
	L_CALL(this, "XapiandManager::setup_node(...)");

	L_DISCOVERY(this, "Setup Node!");

	int new_cluster = 0;

	std::lock_guard<std::mutex> lk(qmtx);

	// Open cluster database
	Endpoints cluster_endpoints(Endpoint("."));

	DatabaseHandler db_handler(cluster_endpoints, DB_WRITABLE | DB_PERSISTENT | DB_NOWAL);
	auto local_node_ = local_node.load();
	try {
		db_handler.get_document(serialise_node_id(local_node_->id));
	} catch (const CheckoutError&) {
		new_cluster = 1;
		L_INFO(this, "Cluster database doesn't exist. Generating database...");
		try {
			db_handler.reset(cluster_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);
			db_handler.index(serialise_node_id(local_node_->id), false, {
				{ RESERVED_INDEX, "field_all" },
				{ ID_FIELD_NAME,  { { RESERVED_TYPE,  TERM_STR } } },
				{ "name",         { { RESERVED_TYPE,  TERM_STR }, { RESERVED_VALUE, local_node_->name } } },
				{ "tagline",      { { RESERVED_TYPE,  TERM_STR }, { RESERVED_INDEX, "none" }, { RESERVED_VALUE, XAPIAND_TAGLINE } } },
			}, true, MSGPACK_CONTENT_TYPE);
		} catch (const CheckoutError&) {
			L_CRIT(this, "Cannot generate cluster database");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (const DocNotFoundError&) {
		L_CRIT(this, "Cluster database is corrupt");
		sig_exit(-EX_DATAERR);
	} catch (const Exception& e) {
		L_CRIT(this, "Exception: %s", e.what());
		sig_exit(-EX_SOFTWARE);
	}

	// Set node as ready!
	auto node_name_ = local_node_->name;
	node_name = set_node_name(node_name_);
	if (lower_string(node_name) != lower_string(node_name_)) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->name = node_name;
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
	}
	L_INFO(this, "Node %s accepted to the party!", node_name.c_str());

	{
		// Get a node (any node)
		std::lock_guard<std::mutex> lk_n(nodes_mtx);
		for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
			auto& node = it->second;
			Endpoint remote_endpoint(".", node.get());
			// Replicate database from the other node
	#ifdef XAPIAND_CLUSTERING
			if (!solo) {
				L_INFO(this, "Syncing cluster data from %s...", node->name.c_str());

				auto ret = trigger_replication(remote_endpoint, cluster_endpoints[0]);
				if (ret.get()) {
					L_INFO(this, "Cluster data being synchronized from %s...", node->name.c_str());
					new_cluster = 2;
					break;
				}
			}
	#endif
		}
	}

	state = State::READY;

#ifdef XAPIAND_CLUSTERING
	if (!is_single_node()) {
		if (auto raft = weak_raft.lock()) {
			raft->start();
		}
	}

	if (auto discovery = weak_discovery.lock()) {
		discovery->enter();
	}
#endif

	if (solo) {
		switch (new_cluster) {
			case 0:
				L_NOTICE(this, "Using solo cluster: %s.", cluster_name.c_str());
				break;
			case 1:
				L_NOTICE(this, "Using new solo cluster: %s.", cluster_name.c_str());
				break;
		}
	} else {
		switch (new_cluster) {
			case 0:
				L_NOTICE(this, "Joined cluster: %s. It is now online!", cluster_name.c_str());
				break;
			case 1:
				L_NOTICE(this, "Joined new cluster: %s. It is now online!", cluster_name.c_str());
				break;
			case 2:
				L_NOTICE(this, "Joined cluster: %s. It was already online!", cluster_name.c_str());
				break;
		}
	}
}


struct sockaddr_in
XapiandManager::host_address()
{
	L_CALL(this, "XapiandManager::host_address()");

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
XapiandManager::signal_sig(int sig)
{
	atom_sig = sig;
	signal_sig_async.send();
}


void
XapiandManager::signal_sig_async_cb(ev::async&, int revents)
{
	L_CALL(this, "XapiandManager::signal_sig_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents).c_str()); (void)revents;

	int sig = atom_sig;
	switch (sig) {
		case SIGTERM:
		case SIGINT:
			shutdown_sig(sig);
			break;
#if defined(__APPLE__) || defined(__FreeBSD__)
		case SIGINFO:
			print(BLUE + "Workers: %s", dump_tree().c_str());
			break;
#endif
	}
}


void
XapiandManager::shutdown_sig(int sig)
{
	L_CALL(this, "XapiandManager::shutdown_sig(%d)", sig);

	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	auto now = epoch::now<>();

	if (sig < 0) {
		throw Exit(-sig);
	}
	if (shutdown_now && sig != SIGTERM) {
		if (sig && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			L_WARNING(this, "You insisted... Xapiand exiting now!");
			throw Exit(1);
		}
	} else if (shutdown_asap && sig != SIGTERM) {
		if (sig && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			shutdown_now = now;
			L_INFO(this, "Trying immediate shutdown.");
		} else if (sig == 0) {
			shutdown_now = now;
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

	if (now > shutdown_asap + 1) {
		shutdown_asap = now;
	}

	if (XapiandServer::http_clients <= 0) {
		shutdown_now = now;
	}

	shutdown(shutdown_asap, shutdown_now);
}


void
XapiandManager::destroy_impl()
{
	destroyer();
}


void
XapiandManager::destroyer() {
	L_CALL(this, "XapiandManager::destroyer()");

#ifdef XAPIAND_CLUSTERING
	if (auto discovery = weak_discovery.lock()) {
		L_INFO(this, "Waving goodbye to cluster %s!", cluster_name.c_str());
		discovery->stop();
	}
#endif

	finish();
}


void
XapiandManager::shutdown_impl(time_t asap, time_t now)
{
	L_CALL(this, "XapiandManager::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now) {
		detach();
		break_loop();
	}
}


void
XapiandManager::make_servers(const opts_t& o)
{
	L_CALL(this, "XapiandManager::make_servers()");

	std::string msg("Listening on ");

	auto http = Worker::make_shared<Http>(XapiandManager::manager, ev_loop, ev_flags, o.http_port);
	msg += http->getDescription() + ", ";

#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<Binary> binary;
	std::shared_ptr<Discovery> discovery;
	std::shared_ptr<Raft> raft;

	if (!solo) {
		binary = Worker::make_shared<Binary>(XapiandManager::manager, ev_loop, ev_flags, o.binary_port);
		msg += binary->getDescription() + ", ";

		discovery = Worker::make_shared<Discovery>(XapiandManager::manager, ev_loop, ev_flags, o.discovery_port, o.discovery_group);
		msg += discovery->getDescription() + ", ";

		raft = Worker::make_shared<Raft>(XapiandManager::manager, ev_loop, ev_flags, o.raft_port, o.raft_group);
		msg += raft->getDescription() + ", ";
	}
#endif

	msg += "at pid:" + std::to_string(getpid()) + " ...";
	L_NOTICE(this, msg.c_str());


	for (ssize_t i = 0; i < o.num_servers; ++i) {
		std::shared_ptr<XapiandServer> server = Worker::make_shared<XapiandServer>(XapiandManager::manager, nullptr, ev_flags);
		servers_weak.push_back(server);

		Worker::make_shared<HttpServer>(server, server->ev_loop, ev_flags, http);

#ifdef XAPIAND_CLUSTERING
		if (!solo) {
			auto binary_server = Worker::make_shared<BinaryServer>(server, server->ev_loop, ev_flags, binary);
			binary->add_server(binary_server);

			Worker::make_shared<DiscoveryServer>(server, server->ev_loop, ev_flags, discovery);

			Worker::make_shared<RaftServer>(server, server->ev_loop, ev_flags, raft);
		}
#endif
		server_pool.enqueue(std::move(server));
	}

	// Make server protocols weak:
	weak_http = std::move(http);
#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		L_INFO(this, "Joining cluster %s...", cluster_name.c_str());
		discovery->start();

		weak_binary = std::move(binary);
		weak_discovery = std::move(discovery);
		weak_raft = std::move(raft);
	}
#endif
}


void
XapiandManager::make_replicators(const opts_t& o)
{
#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		for (size_t i = 0; i < o.num_replicators; ++i) {
			auto obj = Worker::make_shared<XapiandReplicator>(XapiandManager::manager, nullptr, ev_flags);
			replicator_pool.enqueue(std::move(obj));
		}
	}
#else
	(void)o;  // silence -Wunused-parameter
#endif
}


void
XapiandManager::run(const opts_t& o)
{
	L_CALL(this, "XapiandManager::run()");

	if (node_name == "~") {
		L_CRIT(this, "Node name %s doesn't match with the one in the cluster's database!", o.node_name.c_str());
		join();
		throw Exit(EX_CONFIG);
	}

	make_servers(o);

	make_replicators(o);

#if defined(XAPIAND_V8)
	v8pp::Processor::engine(SCRIPTS_CACHE_SIZE);
#endif
#if defined(XAPIAND_CHAISCRIPT)
	chaipp::Processor::engine(SCRIPTS_CACHE_SIZE);
#endif

	DatabaseAutocommit::scheduler(o.num_committers);

	AsyncFsync::scheduler(o.num_fsynchers);

	L_NOTICE(this, "Started " + join_string(std::vector<std::string>{
		std::to_string(o.num_servers) + ((o.num_servers == 1) ? " server" : " servers"),
		std::to_string(o.threadpool_size) +( (o.threadpool_size == 1) ? " worker thread" : " worker threads"),
#ifdef XAPIAND_CLUSTERING
		solo ? "" : std::to_string(o.num_replicators) + ((o.num_replicators == 1) ? " replicator" : " replicators"),
#endif
		std::to_string(o.num_committers) + ((o.num_committers == 1) ? " autocommitter" : " autocommitters"),
		std::to_string(o.num_fsynchers) + ((o.num_fsynchers == 1) ? " fsyncher" : " fsynchers"),
	}, ", ", " and ", [](const auto& s) { return s.empty(); }));

	if (solo) {
		setup_node();
	}

	try {
		L_EV(this, "Entered manager loop...");
		run_loop();
		L_EV(this, "Manager loop ended!");
	} catch (...) {
		join();
		throw;
	}

	join();

	detach();
	detach();  // detach a second time so it cleans lingering protocols
}


void
XapiandManager::finish()
{
	L_CALL(this, "XapiandManager::finish()");

	L_MANAGER(this, "Finishing servers pool!");
	server_pool.finish();

#ifdef XAPIAND_CLUSTERING
	L_MANAGER(this, "Finishing replicators pool!");
	replicator_pool.finish();
#endif

	L_MANAGER(this, "Finishing commiters pool!");
	DatabaseAutocommit::finish();

	L_MANAGER(this, "Finishing async fsync pool!");
	AsyncFsync::finish();
}


void
XapiandManager::join()
{
	L_CALL(this, "XapiandManager::join()");

	L_MANAGER(this, "Workers:" BLUE "%s", dump_tree().c_str());

	finish();

	L_MANAGER(this, "Waiting for %zu server%s...", server_pool.running_size(), (server_pool.running_size() == 1) ? "" : "s");
	server_pool.join();

#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		L_MANAGER(this, "Waiting for %zu replicator%s...", replicator_pool.running_size(), (replicator_pool.running_size() == 1) ? "" : "s");
		replicator_pool.join();
	}
#endif

	L_MANAGER(this, "Finishing autocommitter scheduler!");
	DatabaseAutocommit::finish();

	L_MANAGER(this, "Waiting for %zu autocommitter%s...", DatabaseAutocommit::running_size(), (DatabaseAutocommit::running_size() == 1) ? "" : "s");
	DatabaseAutocommit::join();

	L_MANAGER(this, "Finishing async fsync threads pool!");
	AsyncFsync::finish();

	L_MANAGER(this, "Waiting for %zu async fsync%s...", AsyncFsync::running_size(), (AsyncFsync::running_size() == 1) ? "" : "s");
	AsyncFsync::join();

	L_MANAGER(this, "Finishing worker threads pool!");
	thread_pool.finish();

	L_MANAGER(this, "Finishing database pool!");
	database_pool.finish();

	L_MANAGER(this, "Waiting for %zu worker thread%s...", thread_pool.running_size(), (thread_pool.running_size() == 1) ? "" : "s");
	thread_pool.join();

	L_MANAGER(this, "Server ended!");
}


size_t
XapiandManager::nodes_size()
{
	L_CALL(this, "XapiandManager::nodes_size()");

	std::unique_lock<std::mutex> lk_n(nodes_mtx);
	return nodes.size();
}


bool
XapiandManager::is_single_node()
{
	L_CALL(this, "XapiandManager::is_single_node()");

	return solo || (nodes_size() == 0);
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::reset_state()
{
	L_CALL(this, "XapiandManager::reset_state()");

	if (state != State::RESET) {
		state = State::RESET;
		if (auto discovery = weak_discovery.lock()) {
			discovery->start();
		}
	}
}


bool
XapiandManager::put_node(std::shared_ptr<const Node> node)
{
	L_CALL(this, "XapiandManager::put_node(%s)", repr(node->to_string()).c_str());

	auto local_node_ = local_node.load();
	std::string lower_node_name(lower_string(node->name));
	if (lower_node_name == lower_string(local_node_->name)) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->touched = epoch::now<>();
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
	} else {
		std::lock_guard<std::mutex> lk(nodes_mtx);
		try {
			auto& node_ref = nodes.at(lower_node_name);
			if (*node == *node_ref) {
				auto node_copy = std::make_unique<Node>(*node_ref);
				node_copy->touched = epoch::now<>();
				node_ref = std::shared_ptr<const Node>(node_copy.release());
			}
		} catch (const std::out_of_range &err) {
			auto node_copy = std::make_unique<Node>(*node);
			node_copy->touched = epoch::now<>();
			nodes[lower_node_name] = std::shared_ptr<const Node>(node_copy.release());
			return true;
		} catch (...) {
			throw;
		}
	}
	return false;
}


std::shared_ptr<const Node>
XapiandManager::get_node(const std::string& node_name)
{
	L_CALL(this, "XapiandManager::get_node(%s)", node_name.c_str());

	try {
		std::lock_guard<std::mutex> lk(nodes_mtx);
		return nodes.at(lower_string(node_name));
	} catch (const std::out_of_range &err) {
		return nullptr;
	}
}


std::shared_ptr<const Node>
XapiandManager::touch_node(const std::string& node_name, int32_t region)
{
	L_CALL(this, "XapiandManager::touch_node(%s, %x)", node_name.c_str(), region);

	std::string lower_node_name(lower_string(node_name));

	auto local_node_ = local_node.load();
	if (lower_node_name == lower_string(local_node_->name)) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->touched = epoch::now<>();
		if (region != UNKNOWN_REGION) {
			local_node_copy->region = region;
		}
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
		return local_node.load();
	} else {
		std::lock_guard<std::mutex> lk(nodes_mtx);
		try {
			auto& node_ref = nodes.at(lower_node_name);
			auto node_ref_copy = std::make_unique<Node>(*node_ref);
			node_ref_copy->touched = epoch::now<>();
			if (region != UNKNOWN_REGION) {
				node_ref_copy->region = region;
			}
			node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
			return node_ref;
		} catch (const std::out_of_range &err) {
		} catch (...) {
			throw;
		}
	}
	return nullptr;
}


void
XapiandManager::drop_node(const std::string& node_name)
{
	L_CALL(this, "XapiandManager::drop_node(%s)", node_name.c_str());

	std::lock_guard<std::mutex> lk(nodes_mtx);
	nodes.erase(lower_string(node_name));
}


size_t
XapiandManager::get_nodes_by_region(int32_t region)
{
	L_CALL(this, "XapiandManager::get_nodes_by_region(%x)", region);

	std::lock_guard<std::mutex> lk(nodes_mtx);
	size_t cnt = 0;
	for (const auto& node : nodes) {
		if (node.second->region == region) ++cnt;
	}
	return cnt;
}


int32_t
XapiandManager::get_region(const std::string& db_name)
{
	L_CALL(this, "XapiandManager::get_region(%s)", db_name.c_str());

	bool re_load = false;
	auto local_node_ = local_node.load();
	if (local_node_->regions == -1) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->regions = sqrt(nodes_size());
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
		re_load = true;
	}

	if (re_load) {
		local_node_ = local_node.load();
	}

	std::hash<std::string> hash_fn;
	return jump_consistent_hash(hash_fn(db_name), local_node_->regions);
}


int32_t
XapiandManager::get_region()
{
	L_CALL(this, "XapiandManager::get_region()");

	if (auto raft = weak_raft.lock()) {
		auto local_node_ = local_node.load();
		if (local_node_->regions == -1) {
			if (is_single_node()) {
				auto local_node_copy = std::make_unique<Node>(*local_node_);
				local_node_copy->regions = 1;
				local_node_copy->region = 0;
				local_node = std::shared_ptr<const Node>(local_node_copy.release());
				raft->stop();
			} else if (state == State::READY) {
				raft->start();
				auto local_node_copy = std::make_unique<Node>(*local_node_);
				local_node_copy->regions = sqrt(nodes_size() + 1);
				int32_t region = jump_consistent_hash(local_node_copy->id, local_node_copy->regions);
				if (local_node_copy->region != region) {
					local_node_copy->region = region;
					raft->reset();
				}
				local_node = std::shared_ptr<const Node>(local_node_copy.release());
			}
			L_RAFT(this, "Regions: %d Region: %d", local_node_->regions, local_node_->region);
		}
	}

	auto local_node_ = local_node.load();
	return local_node_->region;
}


std::future<bool>
XapiandManager::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint)
{
	L_CALL(this, "XapiandManager::trigger_replication(%s, %s)", repr(src_endpoint.to_string()).c_str(), repr(dst_endpoint.to_string()).c_str());

	if (auto binary = weak_binary.lock()) {
		return binary->trigger_replication(src_endpoint, dst_endpoint);
	}
	return std::future<bool>();
}

#endif


bool
XapiandManager::resolve_index_endpoint(const std::string &path, std::vector<Endpoint> &endpv, size_t n_endps, std::chrono::duration<double, std::milli> timeout)
{
	L_CALL(this, "XapiandManager::resolve_index_endpoint(%s, ...)", path.c_str());

#ifdef XAPIAND_CLUSTERING
	if (!solo) {
		return endp_r.resolve_index_endpoint(path, endpv, n_endps, timeout);
	}
	else
#else
	(void)n_endps;  // silence -Wunused-parameter
	(void)timeout;  // silence -Wunused-parameter
#endif
	{
		endpv.push_back(Endpoint(path));
		return true;
	}
}


void
XapiandManager::server_status(MsgPack& stats)
{
	// worker_tasks:
	auto& stats_worker_tasks = stats["worker_tasks"];
	stats_worker_tasks["running"] = thread_pool.running_size();
	stats_worker_tasks["enqueued"] = thread_pool.size();
	stats_worker_tasks["capacity"] = thread_pool.threadpool_capacity();
	stats_worker_tasks["pool_size"] = thread_pool.threadpool_size();

	// servers_threads:
	auto& stats_servers_threads = stats["servers_threads"];
	stats_servers_threads["running"] = server_pool.running_size();
	stats_servers_threads["enqueued"] = server_pool.size();
	stats_servers_threads["capacity"] = server_pool.threadpool_capacity();
	stats_servers_threads["pool_size"] = server_pool.threadpool_size();

	// replicator_threads:
#ifdef XAPIAND_CLUSTERING
	if(!solo) {
		auto& stats_replicator_threads = stats["replicator_threads"];
		stats_replicator_threads["running"] = replicator_pool.running_size();
		stats_replicator_threads["enqueued"] = replicator_pool.size();
		stats_replicator_threads["capacity"] = replicator_pool.threadpool_capacity();
		stats_replicator_threads["pool_size"] = replicator_pool.threadpool_size();
	}
#endif

	// committers_threads:
	auto& stats_committers_threads = stats["committers_threads"];
	stats_committers_threads["running"] = DatabaseAutocommit::running_size();
	stats_committers_threads["enqueued"] = DatabaseAutocommit::size();
	stats_committers_threads["capacity"] = DatabaseAutocommit::threadpool_capacity();
	stats_committers_threads["pool_size"] = DatabaseAutocommit::threadpool_size();

	// fsync_threads:
	auto& stats_fsync_threads = stats["fsync_threads"];
	stats_fsync_threads["running"] = AsyncFsync::running_size();
	stats_fsync_threads["enqueued"] = AsyncFsync::size();
	stats_fsync_threads["capacity"] = AsyncFsync::threadpool_capacity();
	stats_fsync_threads["pool_size"] = AsyncFsync::threadpool_size();

	// connections:
	auto& stats_connections = stats["connections"];

	auto& stats_connections_http = stats_connections["http"];
	stats_connections_http["current"] = XapiandServer::http_clients.load();
	stats_connections_http["peak"] = XapiandServer::max_http_clients.load();

#ifdef XAPIAND_CLUSTERING
	if(!solo) {
		auto& stats_connections_binary = stats_connections["binary"];
		stats_connections_binary["current"] = XapiandServer::binary_clients.load();
		stats_connections_binary["peak"] = XapiandServer::max_binary_clients.load();
	}
#endif

	auto& stats_connections_total = stats_connections["total"];
	stats_connections_total["current"] = XapiandServer::total_clients.load();
	stats_connections_total["peak"] = XapiandServer::max_total_clients.load();

	// file_descriptors:
	stats["file_descriptors"] = file_descriptors_cnt();

	// memory:
	auto& stats_memory = stats["memory"];

	auto& stats_memory_used = stats_memory["used"];
	stats_memory_used["resident"] = bytes_string(get_current_memory_by_process());
	stats_memory_used["virtual"] = bytes_string(get_current_memory_by_process(false));

	// auto& stats_memory_total_used = stats_memory["total_used"];
	// stats_memory_total_used["resident"] =  bytes_string(get_current_ram().first);
	// stats_memory_total_used["virtual"] = bytes_string(get_total_virtual_used());

	auto& stats_memory_total = stats_memory["total"];
	stats_memory_total["resident"] =  bytes_string(get_total_ram());
	stats_memory_total["virtual"] = bytes_string(get_total_virtual_memory());

	// databases:
	auto wdb = database_pool.total_writable_databases();
	auto rdb = database_pool.total_readable_databases();

	auto& stats_databases = stats["databases"];

	auto& stats_databases_readable = stats_databases["readable"];
	stats_databases_readable["endpoints"] = rdb.first;
	stats_databases_readable["total"] = rdb.second;

	auto& stats_databases_writable = stats_databases["writable"];
	stats_databases_writable["endpoints"] = wdb.first;
	stats_databases_writable["total"] = wdb.second;

	auto& stats_databases_total = stats_databases["total"];
	stats_databases_total["endpoints"] = rdb.first + wdb.first;
	stats_databases_total["total"] = rdb.second + wdb.second;
}


void
XapiandManager::get_stats_time(MsgPack& stats, const std::string& time_req, const std::string& gran_req)
{
	std::smatch m;
	if (time_req.length() && std::regex_match(time_req, m, time_re) && static_cast<size_t>(m.length()) == time_req.length()) {
		int start = 0, end = 0, increment = 0;
		start += 60 * 60 * (m.length(1) ? std::stoul(m.str(1)) : 0);
		start += 60 * (m.length(2) ? std::stoul(m.str(2)) : 0);
		start += (m.length(3) ? std::stoul(m.str(3)) : 0);
		end += 60 * 60 * (m.length(5) ? std::stoul(m.str(5)) : 0);
		end += 60 * (m.length(6) ? std::stoul(m.str(6)) : 0);
		end += (m.length(7) ? std::stoul(m.str(7)) : 0);

		if (gran_req.length()) {
			if (std::regex_match(gran_req, m, time_re) && static_cast<size_t>(m.length()) == gran_req.length() && m.length(4) == 0) {
				increment += 60 * 60 * (m.length(1) ? std::stoul(m.str(1)) : 0);
				increment += 60 * (m.length(2) ? std::stoul(m.str(2)) : 0);
				increment += (m.length(3) ? std::stoul(m.str(3)) : 0);
			} else {
				THROW(ClientError, "Incorrect input: %s", gran_req.c_str());
			}
		}

		if (start > end) {
			std::swap(end, start);
		}

		return _get_stats_time(stats, start, end, increment);
	}
	THROW(ClientError, "Incorrect input: %s", time_req.c_str());
}


void
XapiandManager::_get_stats_time(MsgPack& stats, int start, int end, int increment)
{
	Stats stats_cnt(Stats::cnt());
	auto current_time = std::chrono::system_clock::to_time_t(stats_cnt.current);

	auto total_inc = end - start;
	if (start >= MAX_TIME_SECOND) {
		return;
	}

	if (total_inc > MAX_TIME_SECOND) {
		total_inc = MAX_TIME_SECOND - start;
		end = MAX_TIME_SECOND;
	}

	int minute = start / 60, second = start % SLOT_TIME_SECOND;
	minute = (minute > stats_cnt.current_pos.minute ? SLOT_TIME_MINUTE + stats_cnt.current_pos.minute : stats_cnt.current_pos.minute) - minute;
	second = (second > stats_cnt.current_pos.second ? SLOT_TIME_SECOND + stats_cnt.current_pos.second : stats_cnt.current_pos.second) - second;

	if (!increment) {
		increment = total_inc;
	}

	for (int offset = 0; offset < total_inc;) {
		auto& stat = stats.push_back(MsgPack());
		// stat["system_time"] = Datetime::iso8601(current_time);
		auto& time_period = stat["period"];

		std::unordered_map<std::string, Stats::Counter::Element> added_counters;
		if (start + offset + increment < SLOT_TIME_SECOND) {
			if (offset + increment > total_inc - 1) {
				increment = total_inc - (offset + 1);
			}
			time_period["start"] = Datetime::iso8601(current_time - (start + offset + increment));
			time_period["end"] = Datetime::iso8601(current_time - (start + offset));
			int end_sec = modulus(second - offset, SLOT_TIME_SECOND);
			int start_sec = modulus(end_sec - increment, SLOT_TIME_SECOND);
			L_DEBUG(this, "sec: %d..%d (pos.second:%u, offset:%d, increment:%d)", start_sec, end_sec, second, offset, increment);
			stats_cnt.add_stats_sec(start_sec, end_sec, added_counters);
			offset += increment + 1;
		} else {
			if (offset + increment > total_inc - 60) {
				increment = total_inc - (offset + 60);
			}
			time_period["start"] = Datetime::iso8601(current_time - (start + offset + increment));
			time_period["end"] = Datetime::iso8601(current_time - (start + offset));
			int end_min = modulus(minute - offset / 60, SLOT_TIME_MINUTE);
			int start_min = modulus(end_min - increment / 60, SLOT_TIME_MINUTE);
			L_DEBUG(this, "min: %d..%d (pos.minute:%u, offset:%d, increment:%d)", start_min, end_min, minute, offset, increment);
			stats_cnt.add_stats_min(start_min, end_min, added_counters);
			offset += increment + 60;
		}

		for (auto& counter : added_counters) {
			if (counter.second.cnt) {
				auto& counter_stats = stat[counter.first];
				counter_stats["cnt"] = counter.second.cnt;
				counter_stats["avg"] = delta_string(counter.second.total / counter.second.cnt);
				counter_stats["min"] = delta_string(counter.second.min);
				counter_stats["max"] = delta_string(counter.second.max);
			}
		}
	}
}


#ifdef L_MANAGER_DEFINED
#undef L_MANAGER_DEFINED
#undef L_MANAGER
#endif
