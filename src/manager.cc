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
#include <cstring>                            // for strerror
#include <ctime>                              // for time_t, ctime, NULL
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
#include "async_fsync.h"                      // for AsyncFsync
#include "color_tools.hh"                     // for color
#include "database_pool.h"                    // for DatabasePool
#include "database_autocommit.h"              // for DatabaseAutocommit
#include "database_handler.h"                 // for DatabaseHandler
#include "database_utils.h"                   // for RESERVED_TYPE
#include "database_wal.h"                     // for DatabaseWALWriter
#include "epoch.hh"                           // for epoch::now
#include "ev/ev++.h"                          // for ev::async, ev::loop_ref
#include "exception.h"                        // for Exit, Excep...
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
#include "readable_revents.hh"                // for readable_revents
#include "serialise.h"                        // for KEYWORD_STR
#include "server/http.h"                      // for Http
#include "server/http_server.h"               // for HttpServer
#include "server/server.h"                    // for XapiandServer, XapiandServer::total_clients
#include "system.hh"                          // for get_open_files_per_proc, get_max_files_per_proc

#ifdef XAPIAND_CLUSTERING
#include "server/binary.h"                    // for Binary
#include "server/binary_server.h"             // for BinaryServer
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
		exit(-sig);
	}
}


XapiandManager::XapiandManager()
	: Worker(nullptr, loop_ref_nil, 0),
	  database_pool(opts.dbpool_size, opts.max_databases),
	  schemas(opts.dbpool_size * 3),
	  thread_pool("W%02zu", opts.threadpool_size),
	  client_pool("C%02zu", opts.tasks_size),
	  server_pool("S%02zu", opts.num_servers),
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0)
{
	std::vector<std::string> values({
		std::to_string(opts.tasks_size) +( (opts.tasks_size == 1) ? " async task" : " async tasks"),
		std::to_string(opts.threadpool_size) +( (opts.threadpool_size == 1) ? " worker thread" : " worker threads"),
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::time_point<std::chrono::system_clock> process_start_)
	: Worker(nullptr, ev_loop_, ev_flags_),
	  database_pool(opts.dbpool_size, opts.max_databases),
	  schemas(opts.dbpool_size),
	  thread_pool("W%02zu", opts.threadpool_size),
	  client_pool("C%02zu", opts.tasks_size),
	  server_pool("S%02zu", opts.num_servers),
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0),
	  signal_sig_async(*ev_loop),
	  setup_node_async(*ev_loop),
	  cluster_database_ready_async(*ev_loop),
	  process_start(process_start_),
	  cleanup(*ev_loop)
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

	cleanup.set<XapiandManager, &XapiandManager::cleanup_cb>(this);
	cleanup.repeat = 60.0;
	cleanup.again();
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
			L_CRIT("Cannot write in node file");
			sig_exit(-EX_IOERR);
		}
		io::close(fd);
	} else {
		L_CRIT("Cannot open or create the node file");
		sig_exit(-EX_NOINPUT);
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

	#ifdef XAPIAND_CLUSTERING
	auto raft = weak_raft.lock();
	if (!opts.solo && !raft) {
		L_CRIT("Raft not available");
		sig_exit(-EX_SOFTWARE);
		return;
	}
	#endif

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
					raft->add_command(serialise_length(did) + serialise_string(obj["name"].as_str()));
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
		try {
			L_INFO("Cluster database doesn't exist. Generating database...");
			DatabaseHandler db_handler(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
			auto did = db_handler.index(local_node->lower_name(), false, {
				{ RESERVED_INDEX, "field_all" },
				{ ID_FIELD_NAME,  { { RESERVED_TYPE,  KEYWORD_STR } } },
				{ "name",         { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_VALUE, local_node->name() } } },
			}, true, msgpack_type).first;
			new_cluster = 1;
			#ifdef XAPIAND_CLUSTERING
			if (!opts.solo) {
				raft->add_command(serialise_length(did) + serialise_string(local_node->name()));
			}
			#endif
		} catch (const CheckoutError&) {
			L_CRIT("Cannot generate cluster database");
			sig_exit(-EX_CANTCREAT);
			return;
		}
	} catch (const Exception& exc) {
		L_CRIT("Exception: %s", exc.get_message());
		sig_exit(-EX_SOFTWARE);
		return;
	} catch (const Xapian::Error& exc) {
		L_CRIT("Exception: %s", exc.get_description());
		sig_exit(-EX_SOFTWARE);
		return;
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
			assert(!cluster_endpoint.is_local());
			Endpoint local_endpoint(".");
			L_INFO("Synchronizing cluster database from %s...", leader_node->name());
			new_cluster = 2;
			trigger_replication(cluster_endpoint, local_endpoint, true);
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
#endif
}


struct sockaddr_in
XapiandManager::host_address()
{
	L_CALL("XapiandManager::host_address()");

	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		L_ERR("ERROR: getifaddrs: %s", strerror(errno));
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
XapiandManager::cleanup_cb(ev::timer& /*unused*/, int revents)
{
	L_CALL("XapiandManager::cleanup_cb(<timer>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	database_pool.cleanup();
}


void
XapiandManager::signal_sig(int sig)
{
	atom_sig = sig;
	signal_sig_async.send();
}


void
XapiandManager::signal_sig_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("XapiandManager::signal_sig_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

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
			print(STEEL_BLUE + "Workers: %s", dump_tree());
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
	auto now = epoch::now<>();

	if (sig < 0) {
		atom_sig = sig;
		break_loop();
		return;
	}
	if ((shutdown_now != 0) && sig != SIGTERM) {
		if ((sig != 0) && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			io::ignore_eintr().store(false);
			L_WARNING("You insisted... Xapiand exiting now!");
			atom_sig = -1;
			break_loop();
			return;
		}
	} else if ((shutdown_asap != 0) && sig != SIGTERM) {
		if ((sig != 0) && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			shutdown_now = now;
			io::ignore_eintr().store(false);
			L_INFO("Trying immediate shutdown.");
		} else if (sig == 0) {
			shutdown_now = now;
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

	if (now > shutdown_asap + 1) {
		shutdown_asap = now;
	}

	if (XapiandServer::total_clients <= 0) {
		shutdown_now = now;
	}

	shutdown(shutdown_asap, shutdown_now);
}


void
XapiandManager::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("XapiandManager::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
		break_loop();
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

		discovery = Worker::make_shared<Discovery>(XapiandManager::manager, ev_loop, ev_flags, opts.discovery_port, opts.discovery_group);
		msg += discovery->getDescription() + ", ";

		raft = Worker::make_shared<Raft>(XapiandManager::manager, ev_loop, ev_flags, opts.raft_port, opts.raft_group);
		msg += raft->getDescription() + ", ";
	}
#endif

	msg += "at pid:" + std::to_string(getpid()) + " ...";
	L_NOTICE(msg);

	for (ssize_t i = 0; i < opts.num_servers; ++i) {
		std::shared_ptr<XapiandServer> server = Worker::make_shared<XapiandServer>(XapiandManager::manager, nullptr, ev_flags);
		servers_weak.push_back(server);

		auto http_server = Worker::make_shared<HttpServer>(server, server->ev_loop, ev_flags, http);
		http->add_server(http_server);

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			auto binary_server = Worker::make_shared<BinaryServer>(server, server->ev_loop, ev_flags, binary);
			binary->add_server(binary_server);
		}
#endif
		server_pool.enqueue([task = std::move(server)]{
			task->run();
		});
	}

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
		join();
		throw Exit(EX_CONFIG);
	}

	make_servers();

	DatabaseAutocommit::scheduler(opts.num_committers);

#if XAPIAND_DATABASE_WAL
	DatabaseWALWriter::start(opts.num_async_wal_writers);
#endif

	AsyncFsync::scheduler(opts.num_fsynchers);

	std::vector<std::string> values({
		std::to_string(opts.num_servers) + ((opts.num_servers == 1) ? " server" : " servers"),
		std::to_string(opts.tasks_size) +( (opts.tasks_size == 1) ? " async task" : " async tasks"),
		std::to_string(opts.threadpool_size) +( (opts.threadpool_size == 1) ? " worker thread" : " worker threads"),
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
		stop();
		destroy();
		detach();
		throw Exit(-sig);
	}

	stop();
	join();

	destroy();
	detach();
	detach();  // detach a second time so it cleans lingering protocols
}


void
XapiandManager::finish()
{
	L_CALL("XapiandManager::finish()");

	L_MANAGER("Finishing servers pool!");
	server_pool.finish();

	L_MANAGER("Finishing client threads pool!");
	client_pool.finish();
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	L_MANAGER("Workers:" + STEEL_BLUE + "%s", dump_tree());

	finish();

	L_MANAGER("Waiting for %zu server%s...", server_pool.running_size(), (server_pool.running_size() == 1) ? "" : "s");
	server_pool.join();

	L_MANAGER("Waiting for %zu client thread%s...", client_pool.running_size(), (client_pool.running_size() == 1) ? "" : "s");
	client_pool.join();

	L_MANAGER("Finishing thread pool!");
	thread_pool.finish();

	L_MANAGER("Waiting for %zu worker thread%s...", thread_pool.running_size(), (thread_pool.running_size() == 1) ? "" : "s");
	thread_pool.join();

	L_MANAGER("Finishing database pool!");
	database_pool.finish();

	L_MANAGER("Finishing autocommitter scheduler!");
	DatabaseAutocommit::finish();

	L_MANAGER("Waiting for %zu autocommitter%s...", DatabaseAutocommit::running_size(), (DatabaseAutocommit::running_size() == 1) ? "" : "s");
	DatabaseAutocommit::join();

#if XAPIAND_DATABASE_WAL
	if (DatabaseWALWriter::running_size()) {
		L_MANAGER("Finishing WAL writers!");
		DatabaseWALWriter::finish();

		L_MANAGER("Waiting for %zu WAL writer%s...", DatabaseWALWriter::running_size(), (DatabaseWALWriter::running_size() == 1) ? "" : "s");
		DatabaseWALWriter::join();
	}
#endif

	L_MANAGER("Finishing async fsync threads pool!");
	AsyncFsync::finish();

	L_MANAGER("Waiting for %zu async fsync%s...", AsyncFsync::running_size(), (AsyncFsync::running_size() == 1) ? "" : "s");
	AsyncFsync::join();

	L_MANAGER("Clearing database pool!");
	database_pool.clear();

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
		try {
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
						db_handler.replace_document(node->idx, doc, true);
					}
				}
			}
		} catch (const Exception& exc) {
			L_CRIT("Exception: %s", exc.get_message());
			sig_exit(-EX_SOFTWARE);
			return;
		} catch (const Xapian::Error& exc) {
			L_CRIT("Exception: %s", exc.get_description());
			sig_exit(-EX_SOFTWARE);
			return;
		}
	}
}


void
XapiandManager::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint, bool cluster_database)
{
	L_CALL("XapiandManager::trigger_replication(%s, %s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()), cluster_database ? "true" : "false");

	if (auto binary = weak_binary.lock()) {
		binary->trigger_replication(src_endpoint, dst_endpoint, cluster_database);
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
		static std::mutex lru_mtx;
		static lru::LRU<std::string, std::string> lru(1000);

		DatabaseHandler db_handler;

		std::string serialised;
		auto key = std::string(path);
		key.push_back('/');

		std::unique_lock<std::mutex> lk(lru_mtx);
		auto it = lru.find(key);
		if (it != lru.end()) {
			serialised = it->second;
			lk.unlock();
		} else {
			lk.unlock();
			db_handler.reset(Endpoints{Endpoint{"."}});
			serialised = db_handler.get_metadata(key);
			if (!serialised.empty()) {
				lk.lock();
				lru.insert(std::make_pair(key, serialised));
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
					assert(node);
					nodes.push_back(std::move(node));
					consistent_hash = idx % indexed_nodes;
					serialised.append(serialise_length(idx));
				}
				assert(!serialised.empty());
				lk.lock();
				lru.insert(std::make_pair(key, serialised));
				lk.unlock();
				auto leader_node = Node::leader_node();
				Endpoint cluster_endpoint(".", leader_node.get());
				db_handler.reset(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
				db_handler.set_metadata(key, serialised, true);
			}
		} else {
			const char *p = serialised.data();
			const char *p_end = p + serialised.size();
			do {
				auto idx = unserialise_length(&p, p_end);
				auto node = Node::get_node(idx);
				assert(node);
				nodes.push_back(std::move(node));
			} while (p != p_end);
		}
	}
	else
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

	// clients_tasks:
	metrics.xapiand_clients_running.Set(client_pool.running_size());
	metrics.xapiand_clients_queue_size.Set(client_pool.size());
	metrics.xapiand_clients_pool_size.Set(client_pool.threadpool_size());
	metrics.xapiand_clients_capacity.Set(client_pool.threadpool_capacity());

	// servers_threads:
	metrics.xapiand_servers_running.Set(server_pool.running_size());
	metrics.xapiand_servers_queue_size.Set(server_pool.size());
	metrics.xapiand_servers_pool_size.Set(server_pool.threadpool_size());
	metrics.xapiand_servers_capacity.Set(server_pool.threadpool_capacity());

	// committers_threads:
	metrics.xapiand_committers_running.Set(DatabaseAutocommit::running_size());
	metrics.xapiand_committers_queue_size.Set(DatabaseAutocommit::size());
	metrics.xapiand_committers_pool_size.Set(DatabaseAutocommit::threadpool_size());
	metrics.xapiand_committers_capacity.Set(DatabaseAutocommit::threadpool_capacity());

	// fsync_threads:
	metrics.xapiand_fsync_running.Set(AsyncFsync::running_size());
	metrics.xapiand_fsync_queue_size.Set(AsyncFsync::size());
	metrics.xapiand_fsync_pool_size.Set(AsyncFsync::threadpool_size());
	metrics.xapiand_fsync_capacity.Set(AsyncFsync::threadpool_capacity());

	// current connections:
	metrics.xapiand_http_current_connections.Set(XapiandServer::http_clients.load());
	metrics.xapiand_binary_current_connections.Set(XapiandServer::binary_clients.load());

	// file_descriptors:
	metrics.xapiand_file_descriptors.Set(get_open_files_per_proc());
	metrics.xapiand_max_file_descriptors.Set(get_max_files_per_proc());

	// inodes:
	metrics.xapiand_free_inodes.Set(get_free_inodes());
	metrics.xapiand_max_inodes.Set(get_total_inodes());

	// memory:
	metrics.xapiand_resident_memory_bytes.Set(get_current_memory_by_process());
	metrics.xapiand_virtual_memory_bytes.Set(get_current_memory_by_process(false));
	metrics.xapiand_used_memory_bytes.Set(allocator::total_allocated());
	metrics.xapiand_total_memory_system_bytes.Set(get_total_ram());
	metrics.xapiand_total_virtual_memory_used.Set(get_total_virtual_memory());
	metrics.xapiand_total_disk_bytes.Set(get_total_disk_size());
	metrics.xapiand_free_disk_bytes.Set(get_free_disk_size());

	// databases:
	auto wdb = database_pool.total_writable_databases();
	auto rdb = database_pool.total_readable_databases();
	metrics.xapiand_readable_db_queues.Set(rdb.queues);
	metrics.xapiand_readable_db.Set(rdb.count);
	metrics.xapiand_writable_db_queues.Set(wdb.queues);
	metrics.xapiand_writable_db.Set(wdb.count);
	metrics.xapiand_db_queues.Set(rdb.queues + wdb.queues);
	metrics.xapiand_db.Set(rdb.count + wdb.count);

	return metrics.serialise();
}
