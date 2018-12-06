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

#include "manager.h"

#include <algorithm>                          // for std::min, std::find_if
#include <cctype>                             // for isspace
#include <chrono>                             // for std::chrono, std::chrono::system_clock
#include <cstdlib>                            // for size_t, exit
#include <errno.h>                            // for errno
#include <exception>                          // for exception
#include <fcntl.h>                            // for O_CLOEXEC, O_CREAT, O_RD...
#include <ifaddrs.h>                          // for ifaddrs, freeifaddrs
#include <memory>                             // for std::shared_ptr
#include <mutex>                              // for mutex, lock_guard, uniqu...
#include <net/if.h>                           // for IFF_LOOPBACK
#include <netinet/in.h>                       // for sockaddr_in, INET_ADDRST...
#include <regex>                              // for std::regex
#include <signal.h>                           // for SIGTERM, SIGINT
#include <string>                             // for std::string, std::to_string
#include <sys/socket.h>                       // for AF_INET, sockaddr
#include <sysexits.h>                         // for EX_IOERR, EX_NOINPUT, EX_SOFTWARE
#include <unistd.h>                           // for ssize_t, getpid
#include <utility>                            // for std::move
#include <vector>                             // for std::vector

#if defined(XAPIAND_V8)
#include <v8-version.h>                       // for V8_MAJOR_VERSION, V8_MINOR_VERSION
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include <chaiscript/chaiscript_defines.hpp>  // for chaiscript::Build_Info
#endif

#include "allocator.h"                        // for allocator::total_allocated
#include "cassert.h"                          // for ASSERT
#include "color_tools.hh"                     // for color
#include "database_cleanup.h"                 // for DatabaseCleanup
#include "database_handler.h"                 // for DatabaseHandler, committer
#include "database_pool.h"                    // for DatabasePool
#include "database_utils.h"                   // for RESERVED_TYPE
#include "epoch.hh"                           // for epoch::now
#include "error.hh"                           // for error:name, error::description
#include "ev/ev++.h"                          // for ev::async, ev::loop_ref
#include "exception.h"                        // for SystemExit, Excep...
#include "hashes.hh"                          // for jump_consistent_hash
#include "ignore_unused.h"                    // for ignore_unused
#include "io.hh"                              // for io::*
#include "length.h"                           // for serialise_length
#include "log.h"                              // for L_CALL, L_DEBUG
#include "lru.h"                              // for LRU
#include "memory_stats.h"                     // for get_total_ram, get_total_virtual_memor...
#include "metrics.h"                          // for Metrics::metrics
#include "msgpack.h"                          // for MsgPack, object::object
#include "namegen.h"                          // for name_generator
#include "net.hh"                             // for fast_inet_ntop4
#include "opts.h"                             // for opts::*
#include "package.h"                          // for Package
#include "readable_revents.hh"                // for readable_revents
#include "serialise.h"                        // for KEYWORD_STR
#include "server/http.h"                      // for Http
#include "server/http_client.h"               // for HttpClient
#include "server/http_server.h"               // for HttpServer
#include "storage.h"                          // for Storage
#include "system.hh"                          // for get_open_files_per_proc, get_max_files_per_proc

#ifdef XAPIAND_CLUSTERING
#include "server/binary.h"                    // for Binary
#include "server/binary_server.h"             // for BinaryServer
#include "server/binary_client.h"             // for BinaryClient
#include "server/discovery.h"                 // for Discovery
#include "server/raft.h"                      // for Raft
#endif


#define L_MANAGER L_NOTHING

// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_MANAGER
// #define L_MANAGER L_DARK_CYAN


#define NODE_LABEL "node"
#define CLUSTER_LABEL "cluster"


static const std::regex time_re("(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)(\\.\\.(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)?)?", std::regex::icase | std::regex::optimize);


std::shared_ptr<XapiandManager> XapiandManager::manager;
static ev::loop_ref* loop_ref_nil = nullptr;

void sig_exit(int sig) {
	if (XapiandManager::manager) {
		XapiandManager::manager->signal_sig(sig);
	} else if (sig < 0) {
		throw SystemExit(-sig);
	} else {
		if (sig == SIGTERM || sig == SIGINT) {
			throw SystemExit(EX_SOFTWARE);
		}
	}
}


XapiandManager::XapiandManager()
	: Worker(nullptr, loop_ref_nil, 0),
	  total_clients(0),
	  http_clients(0),
	  binary_clients(0),
	  database_pool(std::make_shared<DatabasePool>(opts.dbpool_size, opts.max_databases)),
	  schemas(opts.dbpool_size * 3),
	  wal_writer("WW%02zu", opts.num_async_wal_writers),
	  http_client_pool("CH%02zu", opts.num_http_clients),
	  http_server_pool("SH%02zu", opts.num_servers),
#ifdef XAPIAND_CLUSTERING
	  binary_client_pool("CB%02zu", opts.num_binary_clients),
	  binary_server_pool("SB%02zu", opts.num_servers),
#endif
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0),
	  process_start(std::chrono::system_clock::now())
{
	std::vector<std::string> values({
		std::to_string(opts.num_http_clients) +( (opts.num_http_clients == 1) ? " http client thread" : " http client threads"),
#ifdef XAPIAND_CLUSTERING
		std::to_string(opts.num_binary_clients) +( (opts.num_binary_clients == 1) ? " binary client thread" : " binary client threads"),
#endif
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::time_point<std::chrono::system_clock> process_start_)
	: Worker(nullptr, ev_loop_, ev_flags_),
	  total_clients(0),
	  http_clients(0),
	  binary_clients(0),
	  database_pool(std::make_shared<DatabasePool>(opts.dbpool_size, opts.max_databases)),
	  schemas(opts.dbpool_size * 3),
	  wal_writer("WW%02zu", opts.num_async_wal_writers),
	  http_client_pool("CH%02zu", opts.num_http_clients),
	  http_server_pool("SH%02zu", opts.num_servers),
#ifdef XAPIAND_CLUSTERING
	  binary_client_pool("CB%02zu", opts.num_binary_clients),
	  binary_server_pool("SB%02zu", opts.num_servers),
#endif
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0),
	  signal_sig_async(*ev_loop),
	  setup_node_async(*ev_loop),
	  cluster_database_ready_async(*ev_loop),
	  process_start(process_start_)
{
	// Set the id in local node.
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);

	// Setup node from node database directory
	std::string node_name_(load_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && string::lower(node_name) != string::lower(node_name_)) {
			node_name = "~";
		} else {
			node_name = node_name_;
		}
	}
	if (opts.solo) {
		if (node_name.empty()) {
			node_name = name_generator();
		}
	}
	node_copy->name(node_name);

	// Set addr in local node
	node_copy->addr(host_address());

	local_node = std::shared_ptr<const Node>(node_copy.release());
	local_node = Node::local_node(local_node);

	if (opts.solo) {
		Node::leader_node(local_node);
	}

	signal_sig_async.set<XapiandManager, &XapiandManager::signal_sig_async_cb>(this);
	signal_sig_async.start();

	setup_node_async.set<XapiandManager, &XapiandManager::setup_node_async_cb>(this);
	setup_node_async.start();

	cluster_database_ready_async.set<XapiandManager, &XapiandManager::cluster_database_ready_async_cb>(this);
	cluster_database_ready_async.start();
}


XapiandManager::~XapiandManager()
{
	Worker::deinit();
}


std::string
XapiandManager::load_node_name()
{
	L_CALL("XapiandManager::load_node_name()");

	ssize_t length = 0;
	char buf[512];
	int fd = io::open("node", O_RDONLY | O_CLOEXEC);
	if (fd != -1) {
		length = io::read(fd, buf, sizeof(buf) - 1);
		io::close(fd);
		if (length < 0) { length = 0; }
		buf[length] = '\0';
		for (size_t i = 0, j = 0; (buf[j] = buf[i]) != 0; j += static_cast<unsigned long>(isspace(buf[i++]) == 0)) { }
	}
	return std::string(buf, length);
}


void
XapiandManager::save_node_name(std::string_view _node_name)
{
	L_CALL("XapiandManager::save_node_name(%s)", _node_name);

	int fd = io::open("node", O_WRONLY | O_CREAT, 0644);
	if (fd != -1) {
		if (io::write(fd, _node_name.data(), _node_name.size()) != static_cast<ssize_t>(_node_name.size())) {
			THROW(Error, "Cannot write in node file");
		}
		io::close(fd);
	} else {
		THROW(Error, "Cannot open or create the node file");
	}
}


std::string
XapiandManager::set_node_name(std::string_view node_name_)
{
	L_CALL("XapiandManager::set_node_name(%s)", node_name_);

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


void
XapiandManager::setup_node()
{
	setup_node_async.send();
}


void
XapiandManager::setup_node_async_cb(ev::async&, int)
{
	L_CALL("XapiandManager::setup_node_async_cb(...)");

	L_MANAGER("Setup Node!");

	auto leader_node = Node::leader_node();
	auto local_node = Node::local_node();
	auto is_leader = Node::is_equal(leader_node, local_node);

	new_cluster = 0;
	Endpoint cluster_endpoint(".", leader_node.get());
	try {
		bool found = false;
		if (is_leader) {
			DatabaseHandler db_handler(Endpoints{cluster_endpoint});
			auto mset = db_handler.get_all_mset();
			const auto m_e = mset.end();
			for (auto m = mset.begin(); m != m_e; ++m) {
				auto did = *m;
				auto document = db_handler.get_document(did);
				auto obj = document.get_obj();
				if (obj[ID_FIELD_NAME] == local_node->lower_name()) {
					found = true;
				}
				#ifdef XAPIAND_CLUSTERING
				if (!opts.solo) {
					if (auto raft = weak_raft.lock()) {
						raft->add_command(serialise_length(did) + serialise_string(obj["name"].as_str()));
					}
				}
				#endif
			}
		} else {
			#ifdef XAPIAND_CLUSTERING
			auto node = Node::get_node(local_node->lower_name());
			found = node && !!node->idx;
			#endif
		}
		if (!found) {
			THROW(NotFoundError);
		}
	} catch (const NotFoundError&) {
		L_INFO("Cluster database doesn't exist. Generating database...");
		DatabaseHandler db_handler(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
		auto did = db_handler.index(local_node->lower_name(), false, {
			{ RESERVED_INDEX, "field_all" },
			{ ID_FIELD_NAME,  { { RESERVED_TYPE,  KEYWORD_STR } } },
			{ "name",         { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_VALUE, local_node->name() } } },
		}, false, msgpack_type).first;
		new_cluster = 1;
		#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			if (auto raft = weak_raft.lock()) {
				raft->add_command(serialise_length(did) + serialise_string(local_node->name()));
			}
		}
		#else
			ignore_unused(did);
		#endif
	}

	// Set node as ready!
	node_name = set_node_name(local_node->name());
	if (string::lower(node_name) != local_node->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node);
		local_node_copy->name(node_name);
		local_node = Node::local_node(std::shared_ptr<const Node>(local_node_copy.release()));
	}

	Metrics::metrics({{NODE_LABEL, node_name}, {CLUSTER_LABEL, opts.cluster_name}});

	#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		// Replicate database from the leader
		if (!is_leader) {
			ASSERT(!cluster_endpoint.is_local());
			Endpoint local_endpoint(".");
			L_INFO("Synchronizing cluster database from %s%s" + INFO_COL + "...", leader_node->col().ansi(), leader_node->name());
			new_cluster = 2;
			if (auto binary = weak_binary.lock()) {
				binary->trigger_replication({cluster_endpoint, local_endpoint, true});
			}
		} else {
			cluster_database_ready();
		}
	} else
	#endif
	{
		cluster_database_ready();
	}
}


void
XapiandManager::cluster_database_ready()
{
	cluster_database_ready_async.send();
}


void
XapiandManager::cluster_database_ready_async_cb(ev::async&, int)
{
	L_CALL("XapiandManager::cluster_database_ready_async_cb(...)");

	state = State::READY;

	if (auto http = weak_http.lock()) {
		http->start();
	}

#ifdef XAPIAND_CLUSTERING
	if (auto binary = weak_binary.lock()) {
		binary->start();
	}
#endif

	auto local_node = Node::local_node();
	if (opts.solo) {
		switch (new_cluster) {
			case 0:
				L_NOTICE("%s%s" + NOTICE_COL + " using solo cluster %s", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 1:
				L_NOTICE("%s%s" + NOTICE_COL + " using new solo cluster %s", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
		}
	} else {
		switch (new_cluster) {
			case 0:
				L_NOTICE("%s%s" + NOTICE_COL + " joined cluster %s (it is now online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 1:
				L_NOTICE("%s%s" + NOTICE_COL + " joined new cluster %s (it is now online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 2:
				L_NOTICE("%s%s" + NOTICE_COL + " joined cluster %s (it was already online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
		}
	}
}


struct sockaddr_in
XapiandManager::host_address()
{
	L_CALL("XapiandManager::host_address()");

	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		L_ERR("ERROR: getifaddrs: %s (%d): %s", error::name(errno), errno, error::description(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != nullptr; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == AF_INET && ((ifa->ifa_flags & IFF_LOOPBACK) == 0u)) { // check it is IP4
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				L_NOTICE("Node IP address is %s on interface %s", fast_inet_ntop4(addr.sin_addr), ifa->ifa_name);
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
	if (ev_loop->depth()) {
		signal_sig_async.send();
	} else {
		signal_sig_impl();
	}
}


void
XapiandManager::signal_sig_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("XapiandManager::signal_sig_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	signal_sig_impl();
}


void
XapiandManager::signal_sig_impl()
{
	L_CALL("XapiandManager::signal_sig_impl()");

	int sig = atom_sig;

	if (sig < 0) {
		shutdown_sig(sig);
	}

	switch (sig) {
		case SIGTERM:
		case SIGINT:
			shutdown_sig(sig);
			break;
		case SIGUSR1:
		case SIGUSR2:
#if defined(__APPLE__) || defined(__FreeBSD__)
		case SIGINFO:
#endif
			print(STEEL_BLUE + "Workers:\n%sDatabases:\n%sNodes:\n%s", dump_tree(), database_pool->dump_databases(), Node::dump_nodes());
			break;
	}
}


void
XapiandManager::shutdown_sig(int sig)
{
	L_CALL("XapiandManager::shutdown_sig(%d)", sig);

	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	auto now = epoch::now<std::chrono::milliseconds>();

	if (sig < 0) {
		atom_sig = sig;
		if (is_runner() && is_running_loop()) {
			break_loop();
		} else {
			throw SystemExit(-sig);
		}
		return;
	}
	if (shutdown_now != 0) {
		if (now >= shutdown_now + 800) {
			if (now <= shutdown_now + 3000) {
				io::ignore_eintr().store(false);
				atom_sig = sig = -EX_SOFTWARE;
				if (is_runner() && is_running_loop()) {
					L_WARNING("Trying breaking the loop.");
					break_loop();
				} else {
					L_WARNING("You insisted... %s exiting now!", Package::NAME);
					throw SystemExit(-sig);
				}
				return;
			}
			shutdown_now = now;
		}
	} else if (shutdown_asap != 0) {
		if (now >= shutdown_asap + 800) {
			if (now <= shutdown_asap + 3000) {
				shutdown_now = now;
				io::ignore_eintr().store(false);
				L_INFO("Trying immediate shutdown.");
			}
			shutdown_asap = now;
		}
	} else {
		switch (sig) {
			case SIGINT:
				L_INFO("Received SIGINT scheduling shutdown...");
				break;
			case SIGTERM:
				L_INFO("Received SIGTERM scheduling shutdown...");
				break;
			default:
				L_INFO("Received shutdown signal, scheduling shutdown...");
		};
	}

	if (now >= shutdown_asap + 800) {
		shutdown_asap = now;
	}

	if (total_clients <= 0) {
		shutdown_now = now;
	}

	shutdown(shutdown_asap, shutdown_now);
}


void
XapiandManager::shutdown_impl(long long asap, long long now)
{
	L_CALL("XapiandManager::shutdown_impl(%lld, %lld)", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		if (now != 0) {
			detach();
			if (is_runner()) {
				break_loop();
			}
		}
	}
}


void
XapiandManager::stop_impl()
{
	L_CALL("XapiandManager::stop_impl()");

	Worker::stop_impl();

#ifdef XAPIAND_CLUSTERING
	if (auto raft = weak_raft.lock()) {
		raft->stop();
	}
	if (auto discovery = weak_discovery.lock()) {
		discovery->stop();
	}
#endif

	finish();
}


void
XapiandManager::make_servers()
{
	L_CALL("XapiandManager::make_servers()");

	std::string msg("Listening on ");

	auto http = Worker::make_shared<Http>(XapiandManager::manager, ev_loop, ev_flags, opts.http_port);
	msg += http->getDescription() + ", ";

#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<Binary> binary;
	std::shared_ptr<Discovery> discovery;
	std::shared_ptr<Raft> raft;

	if (!opts.solo) {
		binary = Worker::make_shared<Binary>(XapiandManager::manager, ev_loop, ev_flags, opts.binary_port);
		msg += binary->getDescription() + ", ";

		discovery = Worker::make_shared<Discovery>(XapiandManager::manager, nullptr, ev_flags, opts.discovery_port, opts.discovery_group);
		msg += discovery->getDescription() + ", ";
		discovery->run();

		raft = Worker::make_shared<Raft>(XapiandManager::manager, nullptr, ev_flags, opts.raft_port, opts.raft_group);
		msg += raft->getDescription() + ", ";
		raft->run();
	}
#endif

	msg += "at pid:" + std::to_string(getpid()) + " ...";
	L_NOTICE(msg);

	for (ssize_t i = 0; i < opts.num_servers; ++i) {
		auto http_server = Worker::make_shared<HttpServer>(XapiandManager::manager, nullptr, ev_flags, http);
		http->add_server(http_server);
		http_server_pool.enqueue(std::move(http_server));

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			auto binary_server = Worker::make_shared<BinaryServer>(XapiandManager::manager, nullptr, ev_flags, binary);
			binary->add_server(binary_server);
			binary_server_pool.enqueue(std::move(binary_server));
		}
#endif
	}

	auto database_cleanup = Worker::make_shared<DatabaseCleanup>(XapiandManager::manager, nullptr, ev_flags);
	database_cleanup->run();
	database_cleanup->start();

	// Make server protocols weak:
	weak_http = std::move(http);
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		L_INFO("Discovering cluster %s...", opts.cluster_name);
		discovery->start();

		weak_binary = std::move(binary);
		weak_discovery = std::move(discovery);
		weak_raft = std::move(raft);
	}
#endif
}


void
XapiandManager::run()
{
	L_CALL("XapiandManager::run()");

	if (node_name == "~") {
		L_CRIT("Node name %s doesn't match with the one in the cluster's database!", opts.node_name);
		sig_exit(EX_CONFIG);
		join();
		return;
	}

	make_servers();

	std::vector<std::string> values({
		std::to_string(opts.num_servers) + ((opts.num_servers == 1) ? " server" : " servers"),
		std::to_string(opts.num_http_clients) +( (opts.num_http_clients == 1) ? " http client thread" : " http client threads"),
#ifdef XAPIAND_CLUSTERING
		std::to_string(opts.num_binary_clients) +( (opts.num_binary_clients == 1) ? " binary client thread" : " binary client threads"),
#endif
#if XAPIAND_DATABASE_WAL
		std::to_string(opts.num_async_wal_writers) + ((opts.num_async_wal_writers == 1) ? " async wal writer" : " async wal writers"),
#endif
		std::to_string(opts.num_committers) + ((opts.num_committers == 1) ? " autocommitter" : " autocommitters"),
		std::to_string(opts.num_fsynchers) + ((opts.num_fsynchers == 1) ? " fsyncher" : " fsynchers"),
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));

	if (opts.solo) {
		setup_node();
	}

	L_MANAGER("Entered manager loop...");
	run_loop();
	L_MANAGER("Manager loop ended!");

	int sig = atom_sig;
	if (sig < 0) {
		detach();
		throw SystemExit(-sig);
	}

	stop();
	join();
	detach();
}


void
XapiandManager::finish()
{
	L_CALL("XapiandManager::finish()");

	L_MANAGER("Finishing http servers pool!");
	http_server_pool.finish();

	L_MANAGER("Finishing http client threads pool!");
	http_client_pool.finish();

#ifdef XAPIAND_CLUSTERING
	L_MANAGER("Finishing binary servers pool!");
	binary_server_pool.finish();

	L_MANAGER("Finishing binary client threads pool!");
	binary_client_pool.finish();
#endif
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	L_MANAGER(STEEL_BLUE + "Workers:\n%sDatabases:\n%sNodes:\n%s", dump_tree(), database_pool->dump_databases(), Node::dump_nodes());

	finish();

	L_MANAGER("Waiting for %zu http server%s...", http_server_pool.running_size(), (http_server_pool.running_size() == 1) ? "" : "s");
	while (!http_server_pool.join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}

	L_MANAGER("Waiting for %zu http client thread%s...", http_client_pool.running_size(), (http_client_pool.running_size() == 1) ? "" : "s");
	while (!http_client_pool.join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}

#ifdef XAPIAND_CLUSTERING
	L_MANAGER("Waiting for %zu binary server%s...", binary_server_pool.running_size(), (binary_server_pool.running_size() == 1) ? "" : "s");
	while (!binary_server_pool.join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}

	L_MANAGER("Waiting for %zu binary client thread%s...", binary_client_pool.running_size(), (binary_client_pool.running_size() == 1) ? "" : "s");
	while (!binary_client_pool.join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}
#endif

	L_MANAGER("Finishing database pool!");
	database_pool->finish();

	L_MANAGER("Finishing autocommitter scheduler!");
	committer().finish();

	L_MANAGER("Waiting for %zu autocommitter%s...", committer().running_size(), (committer().running_size() == 1) ? "" : "s");
	while (!committer().join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}

	L_MANAGER("Clearing database pool!");
	database_pool->clear();

#if XAPIAND_DATABASE_WAL
	if (wal_writer.running_size()) {
		L_MANAGER("Finishing WAL writers!");
		wal_writer.finish();

		L_MANAGER("Waiting for %zu WAL writer%s...", wal_writer.running_size(), (wal_writer.running_size() == 1) ? "" : "s");
		while (!wal_writer.join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}
#endif

	L_MANAGER("Finishing async fsync threads pool!");
	fsyncher().finish();

	L_MANAGER("Waiting for %zu async fsync%s...", fsyncher().running_size(), (fsyncher().running_size() == 1) ? "" : "s");
	while (!fsyncher().join(500ms)) {
		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}
	}

	L_MANAGER("Server ended!");
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::reset_state()
{
	L_CALL("XapiandManager::reset_state()");

	if (state != State::RESET) {
		if (auto discovery = weak_discovery.lock()) {
			state = State::RESET;
			Node::reset();
			discovery->start();
		}
	}
}


void
XapiandManager::join_cluster()
{
	L_CALL("XapiandManager::join_cluster()");

	L_INFO("Joining cluster %s...", opts.cluster_name);
	if (auto raft = weak_raft.lock()) {
		raft->start();
	}
}


void
XapiandManager::renew_leader()
{
	L_CALL("XapiandManager::renew_leader()");

	if (auto raft = weak_raft.lock()) {
		raft->request_vote();
	}
}


void
XapiandManager::new_leader(std::shared_ptr<const Node>&& leader_node)
{
	L_CALL("XapiandManager::new_leader(%s)", repr(leader_node->name()));

	L_INFO("New leader of cluster %s is %s%s", opts.cluster_name, leader_node->col().ansi(), leader_node->name());

	if (Node::is_local(leader_node)) {
		// If we get promoted to leader, we immediately try to sync the known nodes:
		// See if our local database has all nodes currently commited.
		// If any is missing, it gets added.

		Endpoint cluster_endpoint(".");
		DatabaseHandler db_handler(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
		auto mset = db_handler.get_all_mset();
		const auto m_e = mset.end();

		std::vector<std::pair<size_t, std::string>> db_nodes;
		for (auto m = mset.begin(); m != m_e; ++m) {
			auto did = *m;
			auto document = db_handler.get_document(did);
			auto obj = document.get_obj();
			db_nodes.push_back(std::make_pair(static_cast<size_t>(did), obj[ID_FIELD_NAME].as_str()));
		}

		for (const auto& node : Node::nodes()) {
			if (std::find_if(db_nodes.begin(), db_nodes.end(), [&](std::pair<size_t, std::string> db_node) {
				return db_node.first == node->idx && db_node.second == node->lower_name();
			}) == db_nodes.end()) {
				if (node->idx) {
					// Node is not in our local database, add it now!
					L_WARNING("Adding missing node: [%zu] %s", node->idx, node->name());
					auto prepared = db_handler.prepare(node->lower_name(), false, {
						{ RESERVED_INDEX, "field_all" },
						{ ID_FIELD_NAME,  { { RESERVED_TYPE,  KEYWORD_STR } } },
						{ "name",         { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_VALUE, node->name() } } },
					}, msgpack_type);
					auto& doc = std::get<1>(prepared);
					db_handler.replace_document(node->idx, std::move(doc), false);
				}
			}
		}
	}
}

#endif


std::vector<std::shared_ptr<const Node>>
XapiandManager::resolve_index_nodes(std::string_view path)
{
	L_CALL("XapiandManager::resolve_index_nodes(%s)", repr(path));

	std::vector<std::shared_ptr<const Node>> nodes;

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		DatabaseHandler db_handler;

		std::string serialised;
		auto key = std::string(path);
		key.push_back('/');

		static std::mutex resolve_index_lru_mtx;
		static lru::LRU<std::string, std::string> resolve_index_lru(1000);

		std::unique_lock<std::mutex> lk(resolve_index_lru_mtx);
		auto it = resolve_index_lru.find(key);
		if (it != resolve_index_lru.end()) {
			serialised = it->second;
			lk.unlock();
		} else {
			lk.unlock();
			db_handler.reset(Endpoints{Endpoint{"."}});
			serialised = db_handler.get_metadata(key);
			if (!serialised.empty()) {
				lk.lock();
				resolve_index_lru.insert(std::make_pair(key, serialised));
				lk.unlock();
			}
		}

		if (serialised.empty()) {
			auto indexed_nodes = Node::indexed_nodes();
			if (indexed_nodes) {
				size_t consistent_hash = jump_consistent_hash(path, indexed_nodes);
				for (size_t replicas = std::min(opts.num_replicas, indexed_nodes); replicas; --replicas) {
					auto idx = consistent_hash + 1;
					auto node = Node::get_node(idx);
					ASSERT(node);
					nodes.push_back(std::move(node));
					consistent_hash = idx % indexed_nodes;
					serialised.append(serialise_length(idx));
				}
				ASSERT(!serialised.empty());
				lk.lock();
				resolve_index_lru.insert(std::make_pair(key, serialised));
				lk.unlock();
				auto leader_node = Node::leader_node();
				Endpoint cluster_endpoint(".", leader_node.get());
				db_handler.reset(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
				db_handler.set_metadata(key, serialised, false);
			}
		} else {
			const char *p = serialised.data();
			const char *p_end = p + serialised.size();
			do {
				auto idx = unserialise_length(&p, p_end);
				auto node = Node::get_node(idx);
				ASSERT(node);
				nodes.push_back(std::move(node));
			} while (p != p_end);
		}
	}
	else
#else
	ignore_unused(path);
#endif
	{
		nodes.push_back(Node::local_node());
	}

	return nodes;
}


Endpoint
XapiandManager::resolve_index_endpoint(std::string_view path, bool master)
{
	L_CALL("XapiandManager::resolve_index_endpoint(%s, %s)", repr(path), master ? "true" : "false");

	for (const auto& node : resolve_index_nodes(path)) {
		if (Node::is_active(node)) {
			L_MANAGER("Active node used (of %zu nodes) {idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}", Node::indexed_nodes, node ? node->idx : 0, node ? node->name() : "null", node ? node->http_port : 0, node ? node->binary_port : 0, node ? node->touched : 0);
			return {path, node.get()};
		}
		L_MANAGER("Inactive node ignored (of %zu nodes) {idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}", Node::indexed_nodes, node ? node->idx : 0, node ? node->name() : "null", node ? node->http_port : 0, node ? node->binary_port : 0, node ? node->touched : 0);
		if (master) {
			break;
		}
	}
	THROW(CheckoutErrorEndpointNotAvailable, "Endpoint not available!");
}


std::string
XapiandManager::server_metrics()
{
	L_CALL("XapiandManager::server_metrics()");

	auto& metrics = Metrics::metrics();

	metrics.xapiand_uptime.Set(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - process_start).count());

	// http client tasks:
	metrics.xapiand_http_clients_running.Set(http_client_pool.running_size());
	metrics.xapiand_http_clients_queue_size.Set(http_client_pool.size());
	metrics.xapiand_http_clients_pool_size.Set(http_client_pool.threadpool_size());
	metrics.xapiand_http_clients_capacity.Set(http_client_pool.threadpool_capacity());

#ifdef XAPIAND_CLUSTERING
	// binary client tasks:
	metrics.xapiand_binary_clients_running.Set(binary_client_pool.running_size());
	metrics.xapiand_binary_clients_queue_size.Set(binary_client_pool.size());
	metrics.xapiand_binary_clients_pool_size.Set(binary_client_pool.threadpool_size());
	metrics.xapiand_binary_clients_capacity.Set(binary_client_pool.threadpool_capacity());
#endif

	// servers_threads:
	metrics.xapiand_servers_running.Set(http_server_pool.running_size());
	metrics.xapiand_servers_queue_size.Set(http_server_pool.size());
	metrics.xapiand_servers_pool_size.Set(http_server_pool.threadpool_size());
	metrics.xapiand_servers_capacity.Set(http_server_pool.threadpool_capacity());

	// committers_threads:
	metrics.xapiand_committers_running.Set(committer().running_size());
	metrics.xapiand_committers_queue_size.Set(committer().size());
	metrics.xapiand_committers_pool_size.Set(committer().threadpool_size());
	metrics.xapiand_committers_capacity.Set(committer().threadpool_capacity());

	// fsync_threads:
	metrics.xapiand_fsync_running.Set(fsyncher().running_size());
	metrics.xapiand_fsync_queue_size.Set(fsyncher().size());
	metrics.xapiand_fsync_pool_size.Set(fsyncher().threadpool_size());
	metrics.xapiand_fsync_capacity.Set(fsyncher().threadpool_capacity());

	metrics.xapiand_http_current_connections.Set(http_clients.load());
#ifdef XAPIAND_CLUSTERING
	// current connections:
	metrics.xapiand_binary_current_connections.Set(binary_clients.load());
#endif

	// file_descriptors:
	metrics.xapiand_file_descriptors.Set(get_open_files_per_proc());
	metrics.xapiand_max_file_descriptors.Set(get_max_files_per_proc());

	// inodes:
	metrics.xapiand_free_inodes.Set(get_free_inodes());
	metrics.xapiand_max_inodes.Set(get_total_inodes());

	// memory:
	metrics.xapiand_resident_memory_bytes.Set(get_current_memory_by_process());
	metrics.xapiand_virtual_memory_bytes.Set(get_current_memory_by_process(false));
#ifdef XAPIAND_TRACKED_MEM
	metrics.xapiand_tracked_memory_bytes.Set(allocator::total_allocated());
#endif
	metrics.xapiand_total_memory_system_bytes.Set(get_total_ram());
	metrics.xapiand_total_virtual_memory_used.Set(get_total_virtual_memory());
	metrics.xapiand_total_disk_bytes.Set(get_total_disk_size());
	metrics.xapiand_free_disk_bytes.Set(get_free_disk_size());

	// databases:
	auto count = database_pool->count();
	metrics.xapiand_endpoints.Set(count.first);
	metrics.xapiand_databases.Set(count.second);

	return metrics.serialise();
}


std::string
XapiandManager::__repr__() const
{
	return string::format("<XapiandManager (%s) {cnt:%ld}%s%s%s>",
		StateNames(state),
		use_count(),
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}


#ifdef XAPIAND_CLUSTERING
void
trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint)
{
	if (auto binary = XapiandManager::manager->weak_binary.lock()) {
		binary->trigger_replication({src_endpoint, dst_endpoint, false});
	}
}
#endif
