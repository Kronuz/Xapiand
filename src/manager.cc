/*
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

#include "manager.h"

#include <algorithm>                             // for std::min, std::find_if
#include <arpa/inet.h>                           // for inet_aton
#include <cassert>                               // for assert
#include <cctype>                                // for isspace
#include <chrono>                                // for std::chrono, std::chrono::steady_clock
#include <cstdlib>                               // for size_t, exit
#include <errno.h>                               // for errno
#include <exception>                             // for exception
#include <fcntl.h>                               // for O_CLOEXEC, O_CREAT, O_RD...
#include <ifaddrs.h>                             // for ifaddrs, freeifaddrs
#include <memory>                                // for std::shared_ptr
#include <mutex>                                 // for mutex, lock_guard, uniqu...
#include <net/if.h>                              // for IFF_LOOPBACK
#include <netinet/in.h>                          // for sockaddr_in, INET_ADDRST...
#include <regex>                                 // for std::regex
#include <signal.h>                              // for SIGTERM, SIGINT
#include <string>                                // for std::string, std::to_string
#include <sys/socket.h>                          // for AF_INET, sockaddr
#include <sysexits.h>                            // for EX_IOERR, EX_NOINPUT, EX_SOFTWARE
#include <unistd.h>                              // for ssize_t
#include <utility>                               // for std::move
#include <vector>                                // for std::vector

#ifdef XAPIAND_CHAISCRIPT
#include "chaiscript/chaiscript_defines.hpp"     // for chaiscript::Build_Info
#endif

#include "allocators.h"                          // for allocators::total_allocated
#include "color_tools.hh"                        // for color
#include "database/cleanup.h"                    // for DatabaseCleanup
#include "database/handler.h"                    // for DatabaseHandler, DocPreparer, DocIndexer, committer
#include "database/pool.h"                       // for DatabasePool
#include "database/schemas_lru.h"                // for SchemasLRU
#include "database/utils.h"                      // for RESERVED_TYPE, unsharded_path
#include "database/wal.h"                        // for DatabaseWALWriter
#include "epoch.hh"                              // for epoch::now
#include "error.hh"                              // for error:name, error::description
#include "ev/ev++.h"                             // for ev::async, ev::loop_ref
#include "exception.h"                           // for SystemExit, Excep...
#include "hashes.hh"                             // for jump_consistent_hash
#include "io.hh"                                 // for io::*
#include "length.h"                              // for serialise_length
#include "log.h"                                 // for L_CALL, L_DEBUG
#include "lru.h"                                 // for lru::lru
#include "memory_stats.h"                        // for get_total_ram, get_total_virtual_memor...
#include "metrics.h"                             // for Metrics::metrics
#include "msgpack.h"                             // for MsgPack, object::object
#include "namegen.h"                             // for name_generator
#include "net.hh"                                // for inet_ntop
#include "package.h"                             // for Package
#include "readable_revents.hh"                   // for readable_revents
#include "reserved/schema.h"                     // for RESERVED_INDEX, RESERVED_TYPE, ...
#include "serialise.h"                           // for KEYWORD_STR
#include "serialise_list.h"                      // for StringList
#include "server/http.h"                         // for Http
#include "server/http_client.h"                  // for HttpClient
#include "server/http_server.h"                  // for HttpServer
#include "storage.h"                             // for Storage
#include "strict_stox.hh"                        // for strict_stoll
#include "system.hh"                             // for get_open_files_per_proc, get_max_files_per_proc
#include "thread.hh"                             // for callstacks_snapshot, dump_callstacks

#ifdef XAPIAND_CLUSTERING
#include "server/remote_protocol.h"              // for RemoteProtocol
#include "server/remote_protocol_server.h"       // for RemoteProtocolServer
#include "server/remote_protocol_client.h"       // for RemoteProtocolClient
#include "server/replication_protocol.h"         // for ReplicationProtocol
#include "server/replication_protocol_client.h"  // for ReplicationProtocolClient
#include "server/replication_protocol_server.h"  // for ReplicationProtocolServer
#include "server/discovery.h"                    // for Discovery
#endif

#define L_MANAGER L_NOTHING
#define L_SHARDS L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_MANAGER
// #define L_MANAGER L_DARK_CYAN
// #undef L_SHARDS
// #define L_SHARDS L_DARK_SLATE_BLUE


#define NODE_LABEL "node"
#define CLUSTER_LABEL "cluster"

#define L_MANAGER_TIMED_CLEAR() { \
	if (log) { \
		log->clear(); \
		log.reset(); \
	} \
}

#define L_MANAGER_TIMED(delay, format_timeout, format_done, ...) { \
	if (log) { \
		log->clear(); \
		log.reset(); \
	} \
	auto __log_timed = L_DELAYED(true, (delay), LOG_WARNING, WARNING_COL, (format_timeout), ##__VA_ARGS__); \
	__log_timed.L_DELAYED_UNLOG(LOG_NOTICE, NOTICE_COL, (format_done), ##__VA_ARGS__); \
	log = __log_timed.release(); \
}

constexpr int CONFLICT_RETRIES = 10;   // Number of tries for resolving version conflicts

static const std::regex time_re("(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)(\\.\\.(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)?)?", std::regex::icase | std::regex::optimize);

std::shared_ptr<XapiandManager> XapiandManager::_manager;

static ev::loop_ref* loop_ref_nil = nullptr;

void sig_exit(int sig) {
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->signal_sig(sig);
	} else if (sig < 0) {
		throw SystemExit(-sig);
	} else {
		if (sig == SIGTERM || sig == SIGINT) {
			throw SystemExit(EX_SOFTWARE);
		}
	}
}


XapiandManager::XapiandManager()
	: Worker(std::weak_ptr<Worker>{}, loop_ref_nil, 0),
	  _total_clients(0),
	  _http_clients(0),
	  _remote_clients(0),
	  _replication_clients(0),
	  _schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size)),
	  _database_pool(std::make_unique<DatabasePool>(opts.database_pool_size, opts.max_database_readers)),
	  _wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  _http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  _http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_http_servers)),
#ifdef XAPIAND_CLUSTERING
	  _remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  _remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_remote_servers)),
	  _replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  _replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_replication_servers)),
#endif
	  _doc_matcher_pool(std::make_unique<ThreadPool<std::shared_ptr<DocMatcher>, ThreadPolicyType::doc_matchers>>("DM{:02}", opts.num_doc_matchers)),
	  _doc_preparer_pool(std::make_unique<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>>("DP{:02}", opts.num_doc_preparers)),
	  _doc_indexer_pool(std::make_unique<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>>("DI{:02}", opts.num_doc_indexers)),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _state(State::RESET),
	  _node_name(opts.node_name),
	  _new_cluster(0),
	  _process_start(std::chrono::steady_clock::now()),
	  atom_sig(0)
{
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::time_point<std::chrono::steady_clock> process_start_)
	: Worker(std::weak_ptr<Worker>{}, ev_loop_, ev_flags_),
	  _total_clients(0),
	  _http_clients(0),
	  _remote_clients(0),
	  _replication_clients(0),
	  _schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size)),
	  _database_pool(std::make_unique<DatabasePool>(opts.database_pool_size, opts.max_database_readers)),
	  _wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  _http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  _http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_http_servers)),
#ifdef XAPIAND_CLUSTERING
	  _remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  _remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_remote_servers)),
	  _replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  _replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_replication_servers)),
#endif
	  _doc_matcher_pool(std::make_unique<ThreadPool<std::shared_ptr<DocMatcher>, ThreadPolicyType::doc_matchers>>("DM{:02}", opts.num_doc_matchers)),
	  _doc_preparer_pool(std::make_unique<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>>("DP{:02}", opts.num_doc_preparers)),
	  _doc_indexer_pool(std::make_unique<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>>("DI{:02}", opts.num_doc_indexers)),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _state(State::RESET),
	  _node_name(opts.node_name),
	  _new_cluster(0),
	  _process_start(process_start_),
	  signal_sig_async(*ev_loop),
	  setup_node_async(*ev_loop),
	  set_cluster_database_ready_async(*ev_loop),
#ifdef XAPIAND_CLUSTERING
	  new_leader_async(*ev_loop),
	  raft_apply_command_async(*ev_loop),
#endif
	  atom_sig(0)
{
	signal_sig_async.set<XapiandManager, &XapiandManager::signal_sig_async_cb>(this);
	signal_sig_async.start();

	setup_node_async.set<XapiandManager, &XapiandManager::setup_node_async_cb>(this);
	setup_node_async.start();

	set_cluster_database_ready_async.set<XapiandManager, &XapiandManager::set_cluster_database_ready_async_cb>(this);
	set_cluster_database_ready_async.start();

#ifdef XAPIAND_CLUSTERING
	new_leader_async.set<XapiandManager, &XapiandManager::new_leader_async_cb>(this);
	new_leader_async.start();

	raft_apply_command_async.set<XapiandManager, &XapiandManager::raft_apply_command_async_cb>(this);
	raft_apply_command_async.start();
#endif
}


XapiandManager::~XapiandManager() noexcept
{
	try {
		Worker::deinit();

		join();

		if (log) {
			log->clear();
			log.reset();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


std::string
XapiandManager::load_node_name()
{
	L_CALL("XapiandManager::load_node_name()");

	ssize_t length = 0;
	char buf[512];
	int fd = io::open(".xapiand/node", O_RDONLY | O_CLOEXEC);
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
XapiandManager::save_node_name(std::string_view node_name)
{
	L_CALL("XapiandManager::save_node_name({})", node_name);

	int fd = io::open(".xapiand/node", O_WRONLY | O_CREAT, 0644);
	if (fd != -1) {
		if (io::write(fd, node_name.data(), node_name.size()) != static_cast<ssize_t>(node_name.size())) {
			THROW(Error, "Cannot write in node file");
		}
		io::close(fd);
	} else {
		THROW(Error, "Cannot open or create the node file");
	}
}


std::string
XapiandManager::set_node_name(std::string_view node_name)
{
	L_CALL("XapiandManager::set_node_name({})", node_name);

	_node_name = load_node_name();

	if (_node_name.empty()) {
		if (!node_name.empty()) {
			// Ignore empty _node_name
			_node_name = node_name;
			save_node_name(_node_name);
		}
	}

	return _node_name;
}


std::pair<struct sockaddr_in, std::string>
XapiandManager::host_address()
{
	L_CALL("XapiandManager::host_address()");

	struct sockaddr_in addr{};

	auto hostname = opts.bind_address.empty() ? nullptr : opts.bind_address.c_str();
	if (hostname) {
		if (inet_aton(hostname, &addr.sin_addr) == -1) {
			L_CRIT("ERROR: inet_aton {}: {} ({}): {}", hostname, error::name(errno), errno, error::description(errno));
			sig_exit(-EX_CONFIG);
			return {{}, ""};
		}
	}

	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) == -1) {
		L_CRIT("ERROR: getifaddrs: {} ({}): {}", error::name(errno), errno, error::description(errno));
		sig_exit(-EX_CONFIG);
		return {{}, ""};
	}

	for (struct ifaddrs *ifa = if_addr_struct; ifa != nullptr; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			auto ifa_addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
			if ((!addr.sin_addr.s_addr && ((ifa->ifa_flags & IFF_LOOPBACK) == 0u)) || addr.sin_addr.s_addr == ifa_addr->sin_addr.s_addr) {
				addr = *ifa_addr;
				std::string ifa_name = ifa->ifa_name;
				freeifaddrs(if_addr_struct);
				return {std::move(addr), std::move(ifa_name)};
			}
		}
	}

	freeifaddrs(if_addr_struct);
	L_CRIT("ERROR: host_address: Cannot find the node's IP address!");
	sig_exit(-EX_CONFIG);
	return {{}, ""};
}


void
XapiandManager::signal_sig(int sig)
{
	atom_sig = sig;
	if (is_running_loop()) {
		signal_sig_async.send();
	} else {
		signal_sig_impl();
	}
}


void
XapiandManager::signal_sig_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("XapiandManager::signal_sig_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	signal_sig_impl();
}


void
XapiandManager::signal_sig_impl()
{
	L_CALL("XapiandManager::signal_sig_impl()");

	int sig = atom_sig;

	if (sig < 0) {
		shutdown_sig(sig, is_running_loop());
	}

	switch (sig) {
		case SIGTERM:
		case SIGINT:
			shutdown_sig(sig, is_running_loop());
			break;
		case SIGUSR1:
#if defined(__APPLE__) || defined(__FreeBSD__)
		case SIGINFO:
#endif
#ifdef XAPIAND_CLUSTERING
			print(DARK_STEEL_BLUE + "Threads:\n{}" + DARK_STEEL_BLUE + "Workers:\n{}" + DARK_STEEL_BLUE + "Databases:\n{}" + DARK_STEEL_BLUE + "Schemas:\n{}" + DARK_STEEL_BLUE + "Nodes:\n{}", dump_callstacks(), dump_tree(), _database_pool->dump_databases(), _schemas->dump_schemas(), Node::dump_nodes());
#else
			print(DARK_STEEL_BLUE + "Threads:\n{}" + DARK_STEEL_BLUE + "Workers:\n{}" + DARK_STEEL_BLUE + "Databases:\n{}" + DARK_STEEL_BLUE + "Schemas:\n{}", dump_callstacks(), dump_tree(), _database_pool->dump_databases(), _schemas->dump_schemas());
#endif
			break;
	}
}


void
XapiandManager::shutdown_sig(int sig, bool async)
{
	L_CALL("XapiandManager::shutdown_sig({}, {})", sig, async);

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
	if (_shutdown_now != 0) {
		if (now >= _shutdown_now + 800) {
			if (now <= _shutdown_now + 3000) {
				io::ignore_eintr().store(false);
				atom_sig = sig = -EX_SOFTWARE;
				if (is_runner() && is_running_loop()) {
					L_WARNING("Trying breaking the loop.");
					break_loop();
				} else {
					L_WARNING("You insisted... {} exiting now!", Package::NAME);
					throw SystemExit(-sig);
				}
				return;
			}
			_shutdown_now = now;
		}
	} else if (_shutdown_asap != 0) {
		if (now >= _shutdown_asap + 800) {
			if (now <= _shutdown_asap + 3000) {
				_shutdown_now = now;
				io::ignore_eintr().store(false);
				L_INFO("Trying immediate shutdown.");
			}
			_shutdown_asap = now;
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

	if (now >= _shutdown_asap + 800) {
		_shutdown_asap = now;
	}

	if (_total_clients <= 0) {
		_shutdown_now = now;
	}

	shutdown(_shutdown_asap, _shutdown_now, async);
}


void
XapiandManager::shutdown_impl(long long asap, long long now)
{
	L_CALL("XapiandManager::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		L_MANAGER_TIMED(3s, "Is taking too long to start shutting down, perhaps there are active clients still connected...", "Starting shutdown process!");

		stop(false);
		destroy(false);

		if (now != 0) {
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


void
XapiandManager::init()
{
	L_CALL("XapiandManager::init()");

	bool snooping = (
		!opts.dump_documents.empty() ||
		!opts.restore_documents.empty()
	);

	// Set the id in local node.
	auto local_node = Node::get_local_node();
	auto node_copy = std::make_unique<Node>(*local_node);

	// Setup node from node database directory
	std::string node_name(load_node_name());
	if (!node_name.empty()) {
		if (!_node_name.empty() && strings::lower(_node_name) != strings::lower(node_name)) {
			_node_name = "~";
		} else {
			_node_name = node_name;
		}
	}
	if (opts.solo) {
		if (_node_name.empty()) {
			_node_name = name_generator();
		}
	}
	node_copy->name(_node_name);

	if (!snooping) {
		// Set addr in local node
		auto address = host_address();
		node_copy->addr(address.first);
		L(-LOG_NOTICE, NOTICE_COL, "Node IP address is {} on interface {}", inet_ntop(address.first), address.second);
	}

	Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));
	local_node = Node::get_local_node();

	// If restoring documents, fill all the nodes from the cluster database:
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		if (snooping) {
			try {
				DatabaseHandler db_handler(Endpoints{Endpoint{".xapiand/nodes"}});
				if (!db_handler.get_metadata(std::string_view(RESERVED_SCHEMA)).empty()) {
					auto mset = db_handler.get_mset();
					const auto m_e = mset.end();
					for (auto m = mset.begin(); m != m_e; ++m) {
						auto did = *m;
						auto document = db_handler.get_document(did);
						auto obj = document.get_obj();
						Node node;
						node.name(obj["name"].str_view());
						Node::touch_node(node, false);
					}
				}
			} catch (const Xapian::DocNotFoundError&) {
			} catch (const Xapian::DatabaseNotFoundError&) {}
		}
	} else
#endif
	{
		Node::set_leader_node(local_node);
		Node::touch_node(*local_node, true);
	}
}


void
XapiandManager::run()
{
	L_CALL("XapiandManager::run()");

	try {
		if (_node_name == "~") {
			L_CRIT("Node name {} doesn't match with the one in the cluster's database!", opts.node_name);
			throw SystemExit(EX_CONFIG);
		}

		start_discovery();

		if (opts.solo) {
			setup_node();
		}

		L_MANAGER("Entered manager loop...");
		run_loop();
		L_MANAGER("Manager loop ended!");

		int sig = atom_sig;
		if (sig < 0) {
			throw SystemExit(-sig);
		}

		shutdown_sig(0, false);
		join();

	} catch (...) {
		L_EXC("Exception");
		shutdown_sig(0, false);
		join();
		sig_exit(-EX_SOFTWARE);
	}
}


void
XapiandManager::start_discovery()
{
	L_CALL("XapiandManager::start_discovery()");

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		auto msg = strings::format("Discovering cluster {} by listening on ", repr(opts.cluster_name));

		int discovery_port = opts.discovery_port ? opts.discovery_port : XAPIAND_DISCOVERY_SERVERPORT;
		_discovery = Worker::make_shared<Discovery>(shared_from_this(), nullptr, ev_flags, opts.discovery_group.c_str(), discovery_port);
		msg += _discovery->getDescription();
		_discovery->run();

		_discovery->start();

		L_NOTICE(msg);
	}
#endif
}


void
XapiandManager::setup_node_impl()
{
	setup_node_async.send();
}


void
XapiandManager::setup_node_async_cb(ev::async&, int)
{
	L_CALL("XapiandManager::setup_node_async_cb(...)");

	L_MANAGER("Setup Node!");

	make_servers();

	// Once all threads have started, get a callstacks snapshot:
	callstacks_snapshot();

	std::shared_ptr<const Node> local_node;
	std::shared_ptr<const Node> leader_node;
	Endpoint cluster_endpoint;

	_new_cluster = 0;
	bool found = false;

	for (int t = 10; !found && t >= 0; --t) {
		nanosleep(100000000);  // sleep for 100 miliseconds
		local_node = Node::get_local_node();
		leader_node = Node::get_leader_node();
		cluster_endpoint = Endpoint{".xapiand/nodes", leader_node};

		try {
			DatabaseHandler db_handler(Endpoints{cluster_endpoint});
			if (!db_handler.get_metadata(std::string_view(RESERVED_SCHEMA)).empty()) {
#ifdef XAPIAND_CLUSTERING
				if (!opts.solo) {
					auto mset = db_handler.get_mset();
					const auto m_e = mset.end();
					for (auto m = mset.begin(); m != m_e; ++m) {
						auto did = *m;
						auto document = db_handler.get_document(did);
						if (document.get_value(DB_SLOT_ID) == local_node->lower_name()) {
							found = true;
						}
						auto obj = document.get_obj();
						node_added(obj["name"].str_view());
					}
				} else
#endif
				{
					db_handler.get_document(local_node->lower_name());
					found = true;
				}
			}
		} catch (const Xapian::DatabaseNotAvailableError&) {
			if (t == 0) { throw; }
			continue;
		} catch (const Xapian::DocNotFoundError&) {
		} catch (const Xapian::DatabaseNotFoundError&) { }

		if (!found) {
			try {
				DatabaseHandler db_handler(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
				MsgPack obj({
					{ ID_FIELD_NAME, {
						{ RESERVED_TYPE,  KEYWORD_STR },
					} },
					{ "name", {
						{ RESERVED_INDEX, "none" },
						{ RESERVED_TYPE,  KEYWORD_STR },
						{ RESERVED_VALUE, local_node->name() },
					} },
				});
				db_handler.update(local_node->lower_name(), UNKNOWN_REVISION, false, obj, false, msgpack_type);
				_new_cluster = 1;
#ifdef XAPIAND_CLUSTERING
				if (!opts.solo) {
					add_node(local_node->name());

					if (Node::is_superset(local_node, leader_node)) {
						L_INFO("Cluster database doesn't exist. Generating database...");
					} else {
						if (!leader_node->is_active()) {
							throw Xapian::NetworkError("Endpoint node is inactive");
						}
						auto port = leader_node->remote_port;
						if (port == 0) {
							throw Xapian::NetworkError("Endpoint node without a valid port");
						}
						auto& host = leader_node->host();
						if (host.empty()) {
							throw Xapian::NetworkError("Endpoint node without a valid host");
						}
					}
				}
#endif
				break;
			} catch (...) {
				if (t == 0) { throw; }
			}
		}
	}

	// Set node as ready!
	_node_name = set_node_name(local_node->name());
	if (strings::lower(_node_name) != local_node->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node);
		local_node_copy->name(_node_name);
		Node::set_local_node(std::shared_ptr<const Node>(local_node_copy.release()));
		local_node = Node::get_local_node();
	}

	Metrics::metrics({{NODE_LABEL, _node_name}, {CLUSTER_LABEL, opts.cluster_name}});

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		if (Node::is_superset(local_node, leader_node)) {
			// The local node is the leader
			load_nodes();
			set_cluster_database_ready_impl();
		} else {
			L_INFO("Synchronizing cluster from {}{}" + INFO_COL + "...", leader_node->col().ansi(), leader_node->to_string());
			_new_cluster = 2;
			// Replicate cluster database from the leader
			_replication->trigger_replication({cluster_endpoint, Endpoint{".xapiand/nodes"}, true});
			// Request updates from indices database shards
			auto endpoints = XapiandManager::resolve_index_endpoints(Endpoint{".xapiand/indices"}, true);
			assert(!endpoints.empty());
			for (auto& endpoint : endpoints) {
				Endpoint remote_endpoint{endpoint.path, leader_node};
				Endpoint local_endpoint{endpoint.path};
				_replication->trigger_replication({remote_endpoint, local_endpoint, false});
			}
		}
	} else
#endif
	{
		set_cluster_database_ready_impl();
	}
}


void
XapiandManager::make_servers()
{
	L_CALL("XapiandManager::make_servers()");

	// Try to find suitable ports for the servers.

#if defined(__linux) || defined(__linux__) || defined(linux) || defined(SO_REUSEPORT_LB)
	// In Linux, accept(2) on sockets using SO_REUSEPORT do a load balancing
	// of the incoming clients. It's not the case in other systems; FreeBSD is
	// adding SO_REUSEPORT_LB for that.
	bool reuse_ports = true;
#else
	bool reuse_ports = false;
#endif

	int http_tries = opts.http_port ? 1 : 10;
	int http_port = opts.http_port ? opts.http_port : XAPIAND_HTTP_SERVERPORT;
	int remote_tries = opts.remote_port ? 1 : 10;
	int remote_port = opts.remote_port ? opts.remote_port : XAPIAND_REMOTE_SERVERPORT;
	int replication_tries = opts.replication_port ? 1 : 10;
	int replication_port = opts.replication_port ? opts.replication_port : XAPIAND_REPLICATION_SERVERPORT;

	auto local_node = Node::get_local_node();
#ifdef XAPIAND_CLUSTERING
	auto nodes = Node::nodes();
	for (auto it = nodes.begin(); it != nodes.end();) {
		const auto& node = *it;
		if (!node->is_local()) {
			if (node->addr().sin_addr.s_addr == local_node->addr().sin_addr.s_addr) {
				if (node->http_port == http_port) {
					if (--http_tries == 0) {
						THROW(Error, "Cannot use port {}, it's already in use!", http_port);
					}
					++http_port;
					it = nodes.begin();
					continue;
				}
				if (node->remote_port == remote_port) {
					if (--remote_tries == 0) {
						THROW(Error, "Cannot use port {}, it's already in use!", remote_port);
					}
					++remote_port;
					it = nodes.begin();
					continue;
				}
				if (node->replication_port == replication_port) {
					if (--replication_tries == 0) {
						THROW(Error, "Cannot use port {}, it's already in use!", replication_port);
					}
					++replication_port;
					it = nodes.begin();
					continue;
				}
			}
		}
		++it;
	}
#endif

	// Create and initialize servers.

	auto local_node_addr = inet_ntop(local_node->addr());

	_http = Worker::make_shared<Http>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? 0 : http_tries);

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		_remote = Worker::make_shared<RemoteProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? 0 : remote_tries);
        _replication = Worker::make_shared<ReplicationProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? 0 : replication_tries);
	}
#endif

	for (ssize_t i = 0; i < opts.num_http_servers; ++i) {
		auto _http_server = Worker::make_shared<HttpServer>(_http, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? http_tries : 0);
		if (_http_server->addr.sin_family) {
			_http->addr = _http_server->addr;
		}
		_http_server_pool->enqueue(std::move(_http_server));
	}

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		for (ssize_t i = 0; i < opts.num_remote_servers; ++i) {
			auto _remote_server = Worker::make_shared<RemoteProtocolServer>(_remote, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? remote_tries : 0);
			if (_remote_server->addr.sin_family) {
				_remote->addr = _remote_server->addr;
			}
			_remote_server_pool->enqueue(std::move(_remote_server));
		}

		for (ssize_t i = 0; i < opts.num_replication_servers; ++i) {
			auto _replication_server = Worker::make_shared<ReplicationProtocolServer>(_replication, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? replication_tries : 0);
			if (_replication_server->addr.sin_family) {
				_replication->addr = _replication_server->addr;
			}
			_replication_server_pool->enqueue(std::move(_replication_server));
		}
	}
#endif

	// Setup local node ports.
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->http_port = ntohs(_http->addr.sin_port);
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		node_copy->remote_port = ntohs(_remote->addr.sin_port);
		node_copy->replication_port = ntohs(_replication->addr.sin_port);
	}
#endif
	Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));

	std::string msg("Servers listening on ");
	msg += _http->getDescription();
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		msg += ", " + _remote->getDescription();
		msg += " and " + _replication->getDescription();
	}
#endif
	L(-LOG_NOTICE, NOTICE_COL, msg);

	// Setup database cleanup thread.
	_database_cleanup = Worker::make_shared<DatabaseCleanup>(shared_from_this(), nullptr, ev_flags);
	_database_cleanup->run();
	_database_cleanup->start();

	// Start committers, fsynchers, database updaters and replicator triggers.
	committer();
	fsyncher();
#ifdef XAPIAND_CLUSTERING
	db_updater();
	schema_updater();
	primary_updater();
	trigger_replication();
#endif
}


void
XapiandManager::set_cluster_database_ready_impl()
{
	set_cluster_database_ready_async.send();
}


void
XapiandManager::set_cluster_database_ready_async_cb(ev::async&, int)
{
	L_CALL("XapiandManager::set_cluster_database_ready_async_cb(...)");

	auto local_node = Node::get_local_node();
	assert(local_node->is_active());

	exchange_state(_state.load(), State::READY);

	_http->start();

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		_remote->start();
		_replication->start();
		_discovery->cluster_enter();
	}
#endif

	L(-LOG_NOTICE, SEA_GREEN, "Node {}{}" + SEA_GREEN + " is Ready to Rock!", local_node->col().ansi(), local_node->to_string());

	if (opts.solo) {
		switch (_new_cluster) {
			case 0:
				L_INFO("Using solo cluster {}", repr(opts.cluster_name));
				break;
			case 1:
				L_INFO("Using new solo cluster {}", repr(opts.cluster_name));
				break;
		}
	} else {
		std::vector<std::string> nodes;
		for (const auto& node : Node::nodes()) {
			nodes.push_back(strings::format("{}{}" + INFO_COL, node->col().ansi(), node->to_string()));
		}
		switch (_new_cluster) {
			case 0:
				L_INFO("Opened cluster {} with {} {}.", repr(opts.cluster_name), nodes.size() == 1 ? "node" : "nodes", strings::join(nodes, ", ", " and "));
				break;
			case 1:
				L_INFO("Created cluster {} with {} {}.", repr(opts.cluster_name), nodes.size() == 1 ? "node" : "nodes", strings::join(nodes, ", ", " and "));
				break;
			case 2:
				L_INFO("Joined cluster {} with {} {}.", repr(opts.cluster_name), nodes.size() == 1 ? "node" : "nodes", strings::join(nodes, ", ", " and "));
				break;
		}
	}
}


void
XapiandManager::stop_impl()
{
	L_CALL("XapiandManager::stop_impl()");

	Worker::stop_impl();

	// During stop, finish HTTP servers and clients, but not binary
	// as those may still be needed by the other end to start the
	// shutting down process.

	L_MANAGER("Finishing http servers pool!");
	_http_server_pool->finish();

	L_MANAGER("Finishing http client threads pool!");
	_http_client_pool->finish();
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	if (!_database_pool) {
		return;  // already joined!
	}

	// This method should finish and wait for all objects and threads to finish
	// their work. Order of waiting for objects here matters!
	L_MANAGER(STEEL_BLUE + "Workers:\n{}Databases:\n{}Nodes:\n{}", dump_tree(), _database_pool->dump_databases(), Node::dump_nodes());

#ifdef XAPIAND_CLUSTERING
	if (_discovery) {
		_discovery->stop();
	}
#endif

	////////////////////////////////////////////////////////////////////
	if (_http_server_pool) {
		L_MANAGER("Finishing http servers pool!");
		_http_server_pool->finish();

		L_MANAGER("Waiting for {} http server{}...", _http_server_pool->running_size(), (_http_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP servers...", "HTTP servers finished!");
		while (!_http_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_http_client_pool) {
		L_MANAGER("Finishing http client threads pool!");
		_http_client_pool->finish();

		L_MANAGER("Waiting for {} http client thread{}...", _http_client_pool->running_size(), (_http_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP clients...", "HTTP clients finished!");
		while (!_http_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_doc_matcher_pool) {
		L_MANAGER("Finishing parallel document matcher threads pool!");
		_doc_matcher_pool->finish();

		L_MANAGER("Waiting for {} parallel document matcher thread{}...", _doc_matcher_pool->running_size(), (_doc_matcher_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the parallel document matchers...", "Parallel document matchers finished!");
		while (!_doc_matcher_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_doc_preparer_pool) {
		L_MANAGER("Finishing bulk document preparer threads pool!");
		_doc_preparer_pool->finish();

		L_MANAGER("Waiting for {} bulk document preparer thread{}...", _doc_preparer_pool->running_size(), (_doc_preparer_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the bulk document preparers...", "Bulk document preparers finished!");
		while (!_doc_preparer_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_doc_indexer_pool) {
		L_MANAGER("Finishing bulk document indexer threads pool!");
		_doc_indexer_pool->finish();

		L_MANAGER("Waiting for {} bulk document indexer thread{}...", _doc_indexer_pool->running_size(), (_doc_indexer_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the bulk document indexers...", "Bulk document indexers finished!");
		while (!_doc_indexer_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#ifdef XAPIAND_CLUSTERING

	////////////////////////////////////////////////////////////////////
	auto& trigger_replication_obj = trigger_replication(false);
	if (trigger_replication_obj) {
		L_MANAGER("Finishing replication scheduler!");
		trigger_replication_obj->finish();

		L_MANAGER("Waiting for {} replication scheduler{}...", trigger_replication_obj->running_size(), (trigger_replication_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the replication schedulers...", "Replication Protocol schedulers finished!");
		while (!trigger_replication_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_remote_server_pool) {
		L_MANAGER("Finishing remote protocol servers pool!");
		_remote_server_pool->finish();

		L_MANAGER("Waiting for {} remote protocol server{}...", _remote_server_pool->running_size(), (_remote_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the remote protocol servers...", "Remote Protocol servers finished!");
		while (!_remote_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_remote_client_pool) {
		L_MANAGER("Finishing remote protocol threads pool!");
		_remote_client_pool->finish();

		L_MANAGER("Waiting for {} remote protocol thread{}...", _remote_client_pool->running_size(), (_remote_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the remote protocol threads...", "Remote Protocol threads finished!");
		while (!_remote_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_replication_server_pool) {
		L_MANAGER("Finishing replication protocol servers pool!");
		_replication_server_pool->finish();

		L_MANAGER("Waiting for {} replication server{}...", _replication_server_pool->running_size(), (_replication_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the replication protocol servers...", "Replication Protocol servers finished!");
		while (!_replication_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (_replication_client_pool) {
		L_MANAGER("Finishing replication protocol threads pool!");
		_replication_client_pool->finish();

		L_MANAGER("Waiting for {} replication protocol thread{}...", _replication_client_pool->running_size(), (_replication_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the replication protocol threads...", "Replication Protocol threads finished!");
		while (!_replication_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
	if (_database_pool) {
		L_MANAGER("Finishing database pool!");
		_database_pool->finish();

		L_MANAGER("Clearing and waiting for database pool!");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database pool...", "Database pool finished!");
		while (!_database_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	auto& committer_obj = committer(false);
	if (committer_obj) {
		L_MANAGER("Finishing autocommitter scheduler!");
		committer_obj->finish();

		L_MANAGER("Waiting for {} autocommitter{}...", committer_obj->running_size(), (committer_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the autocommit schedulers...", "Autocommit schedulers finished!");
		while (!committer_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#if XAPIAND_DATABASE_WAL

	////////////////////////////////////////////////////////////////////
	if (_wal_writer) {
		L_MANAGER("Finishing WAL writers!");
		_wal_writer->finish();

		L_MANAGER("Waiting for {} WAL writer{}...", _wal_writer->running_size(), (_wal_writer->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the WAL writers...", "WAL writers finished!");
		while (!_wal_writer->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
#ifdef XAPIAND_CLUSTERING

	auto& db_updater_obj = db_updater(false);
	if (db_updater_obj) {
		L_MANAGER("Finishing database updater!");
		db_updater_obj->finish();

		L_MANAGER("Waiting for {} database updater{}...", db_updater_obj->running_size(), (db_updater_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database updaters...", "Database updaters finished!");
		while (!db_updater_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	auto& schema_updater_obj = schema_updater(false);
	if (schema_updater_obj) {
		L_MANAGER("Finishing schema updater!");
		schema_updater_obj->finish();

		L_MANAGER("Waiting for {} schema updater{}...", schema_updater_obj->running_size(), (schema_updater_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the schema updaters...", "Schema updaters finished!");
		while (!schema_updater_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	auto& primary_updater_obj = primary_updater(false);
	if (primary_updater_obj) {
		L_MANAGER("Finishing primary shard updater!");
		primary_updater_obj->finish();

		L_MANAGER("Waiting for {} primary shard updater{}...", primary_updater_obj->running_size(), (primary_updater_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the primary shard updaters...", "Primary shard updaters finished!");
		while (!primary_updater_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
	auto& fsyncher_obj = fsyncher(false);
	if (fsyncher_obj) {
		L_MANAGER("Finishing async fsync threads pool!");
		fsyncher_obj->finish();

		L_MANAGER("Waiting for {} async fsync{}...", fsyncher_obj->running_size(), (fsyncher_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the async fsync threads...", "Async fsync threads finished!");
		while (!fsyncher_obj->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#if XAPIAND_CLUSTERING

	////////////////////////////////////////////////////////////////////
	if (_discovery) {
		L_MANAGER("Finishing Discovery loop!");
		_discovery->finish();

		L_MANAGER("Waiting for Discovery...");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the discovery protocol...", "Discovery protocol finished!");
		while (!_discovery->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
	if (_database_cleanup) {
		L_MANAGER("Finishing Database Cleanup loop!");
		_database_cleanup->finish();

		L_MANAGER("Waiting for Database Cleanup...");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database cleanup worker...", "Database cleanup worker finished!");
		while (!_database_cleanup->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////

	L_MANAGER_TIMED(1s, "Is taking too long to reset manager...", "Manager reset finished!");

	_http.reset();
#ifdef XAPIAND_CLUSTERING
	_remote.reset();
	_replication.reset();
	_discovery.reset();
#endif

	_database_pool.reset();

	_wal_writer.reset();

	_http_client_pool.reset();
	_http_server_pool.reset();

	_doc_matcher_pool.reset();
	_doc_indexer_pool.reset();
	_doc_preparer_pool.reset();

#ifdef XAPIAND_CLUSTERING
	_remote_client_pool.reset();
	_remote_server_pool.reset();
	_replication_client_pool.reset();
	_replication_server_pool.reset();
#endif

	_database_cleanup.reset();

#ifdef XAPIAND_CLUSTERING
	trigger_replication_obj.reset();
	db_updater_obj.reset();
#endif
	committer_obj.reset();
	fsyncher_obj.reset();

	_schemas.reset();

	////////////////////////////////////////////////////////////////////
	L_MANAGER("Server ended!");
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::reset_state_impl()
{
	L_CALL("XapiandManager::reset_state_impl()");

	if (exchange_state(_state.load(), State::RESET, 3s, "Node resetting is taking too long...", "Node reset done!")) {
		Node::reset();
		_discovery->start();
	}
}


void
XapiandManager::join_cluster_impl()
{
	L_CALL("XapiandManager::join_cluster_impl()");

	L_INFO("Joining cluster {}...", repr(opts.cluster_name));
	_discovery->raft_request_vote();
}


void
XapiandManager::renew_leader_impl()
{
	L_CALL("XapiandManager::renew_leader_impl()");

	_discovery->raft_request_vote();
}


void
XapiandManager::new_leader_impl()
{
	L_CALL("XapiandManager::new_leader_impl()");

	new_leader_async.send();
}


void
XapiandManager::new_leader_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("XapiandManager::new_leader_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	auto leader_node = Node::get_leader_node();
	L_INFO("New leader of cluster {} is {}{}", repr(opts.cluster_name), leader_node->col().ansi(), leader_node->to_string());

	if (_state == State::READY && leader_node->is_local()) {
		try {
			// If we get promoted to leader, we immediately try to load the nodes.
			load_nodes();
		} catch (...) {
			L_EXC("ERROR: Cannot load local nodes!");
		}
	}
}


void
XapiandManager::load_nodes()
{
	L_CALL("XapiandManager::load_nodes()");

	// See if our local database has all nodes currently commited.
	// If any is missing, it gets added.

	auto leader_node = Node::get_leader_node();
	Endpoint cluster_endpoint{".xapiand/nodes", leader_node};
	DatabaseHandler db_handler(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
	auto mset = db_handler.get_mset();

	std::vector<std::string> db_nodes;
	for (auto it = mset.begin(), it_e = mset.end(); it != it_e; ++it) {
		auto document = db_handler.get_document(*it);
		db_nodes.push_back(document.get_value(DB_SLOT_ID));
	}

	for (const auto& node : Node::nodes()) {
		if (std::find_if(db_nodes.begin(), db_nodes.end(), [&](const std::string& node_lower_name) {
			return node_lower_name == node->lower_name();
		}) == db_nodes.end()) {
			// Node is not in our local database, add it now!
			L_WARNING("Adding missing node: {}{}", node->col().ansi(), node->to_string());
			MsgPack obj({
				{ ID_FIELD_NAME, {
					{ RESERVED_TYPE,  KEYWORD_STR },
				} },
				{ "name", {
					{ RESERVED_INDEX, "none" },
					{ RESERVED_TYPE,  KEYWORD_STR },
					{ RESERVED_VALUE, node->name() },
				} },
			});
			auto prepared = db_handler.prepare(node->lower_name(), 0, false, obj, msgpack_type);
			auto& doc = std::get<1>(prepared);
			db_handler.replace_document(node->lower_name(), std::move(doc), false);
		}
		add_node(node->name());
	}
}


void
XapiandManager::raft_apply_command_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("XapiandManager::raft_apply_command_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	std::string command;
	while (raft_apply_command_args.try_dequeue(command)) {
		_raft_apply_command(command);
	}
}


void
XapiandManager::raft_apply_command_impl(const std::string& command)
{
	L_CALL("XapiandManager::raft_apply_command_impl({})", repr(command));

	raft_apply_command_args.enqueue(command);

	raft_apply_command_async.send();
}


void
XapiandManager::_raft_apply_command(const std::string& command)
{
	L_CALL("XapiandManager::_raft_apply_command({})", repr(command));

	const char *p = command.data();
	const char *p_end = p + command.size();
	if (p == p_end) {
		L_ERR("Empty raft command ignored");
		return;
	}

	node_added(std::string_view(p, p_end - p));
}


void
XapiandManager::add_node(std::string_view name)
{
	L_CALL("XapiandManager::add_node({})", name);

	node_added(name);

	_discovery->raft_add_command(std::string(name));
}


void
XapiandManager::node_added(std::string_view name)
{
	L_CALL("XapiandManager::node_added({})", name);

	Node node_copy;

	auto node = Node::get_node(name);
	if (node) {
		node_copy = *node;
	} else {
		node_copy.name(std::string(name));
	}

	auto put = Node::touch_node(node_copy, false, false);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node_copy.col().ansi(), node_copy.to_string(), node_copy.host(), node_copy.http_port, node_copy.remote_port, node_copy.replication_port);
	} else {
		node = put.first;
		L_DEBUG("Added node {}{}" + INFO_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
	}
}


void
XapiandManager::sweep_primary_impl(size_t shards, const std::string& normalized_path)
{
	L_CALL("XapiandManager::sweep_primary_impl(<shards>, {})", repr(normalized_path));

	if (shards > 1) {
		for (size_t shard_num = 0; shard_num < shards; ++shard_num) {
			auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, ++shard_num);
			resolve_index_nodes_impl(shard_normalized_path, false, false,  nullptr, false, false, true);
		}
	}
	resolve_index_nodes_impl(normalized_path, false, false,  nullptr, false, false, true);
}

#endif


struct NodeSettingsShard {
	Xapian::rev version;
	bool modified;

	std::vector<std::string> nodes;

	NodeSettingsShard() : version(UNKNOWN_REVISION), modified(false) { }
};


struct NodeSettings {
	Xapian::rev version;
	bool modified;

	std::chrono::time_point<std::chrono::steady_clock> stalled;

	size_t num_shards;
	size_t num_replicas_plus_master;
	std::vector<NodeSettingsShard> shards;

	NodeSettings() : version(UNKNOWN_REVISION), modified(false), stalled(std::chrono::steady_clock::time_point::min()), num_shards(0), num_replicas_plus_master(0) { }

	NodeSettings(Xapian::rev version, bool modified, const std::chrono::time_point<std::chrono::steady_clock>& stalled, size_t num_shards, size_t num_replicas_plus_master, const std::vector<NodeSettingsShard>& shards) :
		version(version),
		modified(modified),
		stalled(stalled),
		num_shards(num_shards),
		num_replicas_plus_master(num_replicas_plus_master),
		shards(shards) {
#ifndef NDEBUG
		size_t replicas_size = 0;
		for (auto& shard : shards) {
			auto replicas_size_ = shard.nodes.size();
			assert(replicas_size_ != 0 && (!replicas_size || replicas_size == replicas_size_));
			replicas_size = replicas_size_;
		}
#endif
	}
};


void
settle_replicas(std::vector<NodeSettingsShard>& shards, std::vector<std::shared_ptr<const Node>>& nodes, size_t num_replicas_plus_master)
{
	L_CALL("settle_replicas(<shards>, {})", num_replicas_plus_master);

	size_t total_nodes = Node::total_nodes();
	if (num_replicas_plus_master > total_nodes) {
		num_replicas_plus_master = total_nodes;
	}
	for (auto& shard : shards) {
		auto shard_nodes_size = shard.nodes.size();
		assert(shard_nodes_size);
		if (shard_nodes_size < num_replicas_plus_master) {
			std::unordered_set<std::string> used;
			for (size_t i = 0; i < shard_nodes_size; ++i) {
				used.insert(strings::lower(shard.nodes[i]));
			}
			if (nodes.empty()) {
				nodes = Node::nodes();
			}
			auto primary = strings::lower(shard.nodes[0]);
			size_t idx = 0;
			for (const auto& node : nodes) {
				++idx;
				if (node->lower_name() == primary) {
					break;
				}
			}
			for (auto n = shard_nodes_size; n < num_replicas_plus_master; ++n) {
				auto node = nodes[idx % nodes.size()];
				while (used.count(node->lower_name())) {
					node = nodes[++idx % nodes.size()];
					assert(idx < nodes.size() * 2);
				}
				shard.nodes.push_back(node->name());
			}
			shard.modified = true;
		} else if (shard_nodes_size > num_replicas_plus_master) {
			assert(num_replicas_plus_master);
			shard.nodes.resize(num_replicas_plus_master);
			shard.modified = true;
		}
	}
}


std::vector<NodeSettingsShard>
calculate_shards(size_t routing_key, std::vector<std::shared_ptr<const Node>>& nodes, size_t num_shards)
{
	L_CALL("calculate_shards({}, {})", routing_key, num_shards);

	std::vector<NodeSettingsShard> shards;
	if (Node::total_nodes()) {
		if (routing_key < num_shards) {
			routing_key += num_shards;
		}
		for (size_t s = 0; s < num_shards; ++s) {
			NodeSettingsShard shard;
			if (nodes.empty()) {
				nodes = Node::nodes();
			}
			size_t idx = (routing_key - s) % nodes.size();
			auto node = nodes[idx];
			shard.nodes.push_back(node->name());
			shards.push_back(std::move(shard));
		}
	}
	return shards;
}


void
update_primary(const std::string& normalized_path, NodeSettings& node_settings)
{
	L_CALL("update_primary({}, <node_settings>)", repr(normalized_path));

	auto now = std::chrono::steady_clock::now();

	if (node_settings.stalled > now) {
		return;
	}

	bool updated = false;
	size_t shard_num = 0;
	for (auto& shard : node_settings.shards) {
		++shard_num;
		auto it_b = shard.nodes.begin();
		auto it_e = shard.nodes.end();
		auto it = it_b;
		for (; it != it_e; ++it) {
			auto node = Node::get_node(*it);
			if (node && node->is_active()) {
				break;
			}
		}
		if (it != it_b && it != it_e) {
			if (node_settings.stalled == std::chrono::steady_clock::time_point::min()) {
				node_settings.stalled = now + std::chrono::milliseconds(opts.database_stall_time);
				break;
			} else if (node_settings.stalled <= now) {
				auto from_node = Node::get_node(*it_b);
				auto to_node = Node::get_node(*it);
				auto path = node_settings.shards.size() > 1 ? strings::format("{}/.__{}", normalized_path, shard_num) : normalized_path;
				L_INFO("Primary shard {} moved from node {}{}" + INFO_COL + " to {}{}",
					repr(path),
					from_node->col().ansi(), from_node->name(),
					to_node->col().ansi(), to_node->name());
				std::swap(*it, *it_b);
				updated = true;
				shard.modified = true;
			}
		}
	}

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		if (updated) {
			node_settings.stalled = std::chrono::steady_clock::time_point::min();
			primary_updater()->debounce(normalized_path, node_settings.shards.size(), normalized_path);
		}
	}
#endif
}


void
index_replicas(const std::string& normalized_path, size_t num_replicas_plus_master, NodeSettingsShard& shard)
{
	L_CALL("index_replicas(<shard>)");

	if (shard.modified) {
		Endpoint endpoint(".xapiand/indices");
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		assert(!endpoints.empty());
		DatabaseHandler db_handler(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN);
		MsgPack obj({
			{ RESERVED_IGNORE, SCHEMA_FIELD_NAME },
			{ ID_FIELD_NAME, {
				{ RESERVED_TYPE,  KEYWORD_STR },
			} },
			{ "number_of_shards", {
				{ RESERVED_INDEX, "none" },
				{ RESERVED_TYPE,  "positive" },
			} },
			{ "number_of_replicas", {
				{ RESERVED_INDEX, "none" },
				{ RESERVED_TYPE,  "positive" },
				{ RESERVED_VALUE, num_replicas_plus_master - 1 },
			} },
			{ "shards", {
				{ RESERVED_INDEX, "field_terms" },
				{ RESERVED_TYPE,  "array/keyword" },
				{ RESERVED_VALUE, shard.nodes },
			} },
		});
		auto info = db_handler.update(normalized_path, shard.version, false, obj, false, msgpack_type).first;
		shard.version = info.version;
		shard.modified = false;
	}
}


void
index_settings(const std::string& normalized_path, NodeSettings& node_settings)
{
	L_CALL("index_settings(<node_settings>)");

	assert(node_settings.shards.size() == node_settings.num_shards);

	if (node_settings.num_shards == 1) {
		index_replicas(normalized_path, node_settings.num_replicas_plus_master, node_settings.shards[0]);
	} else if (node_settings.num_shards != 0) {
		if (!node_settings.shards[0].nodes.empty()) {
			if (node_settings.modified) {
				Endpoint endpoint(".xapiand/indices");
				auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
				assert(!endpoints.empty());
				DatabaseHandler db_handler(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN);
				MsgPack obj({
					{ RESERVED_IGNORE, SCHEMA_FIELD_NAME },
					{ ID_FIELD_NAME, {
						{ RESERVED_TYPE,  KEYWORD_STR },
					} },
					{ "number_of_shards", {
						{ RESERVED_INDEX, "none" },
						{ RESERVED_TYPE,  "positive" },
						{ RESERVED_VALUE, node_settings.num_shards },
					} },
					{ "number_of_replicas", {
						{ RESERVED_INDEX, "none" },
						{ RESERVED_TYPE,  "positive" },
						{ RESERVED_VALUE, node_settings.num_replicas_plus_master - 1 },
					} },
					{ "shards", {
						{ RESERVED_INDEX, "field_terms" },
						{ RESERVED_TYPE,  "array/keyword" },
					} },
				});
				auto info = db_handler.update(normalized_path, node_settings.version, false, obj, false, msgpack_type).first;
				node_settings.version = info.version;
				node_settings.modified = false;
			}
		}
		size_t shard_num = 0;
		for (auto& shard : node_settings.shards) {
			if (!shard.nodes.empty()) {
				auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, ++shard_num);
				index_replicas(shard_normalized_path, node_settings.num_replicas_plus_master, shard);
			}
		}
	}
}


NodeSettingsShard
load_replicas(const Endpoint& endpoint, const MsgPack& obj)
{
	L_CALL("load_replicas(<obj>)");

	NodeSettingsShard shard;

	auto it = obj.find(VERSION_FIELD_NAME);
	if (it != obj.end()) {
		auto& version_val = it.value();
		if (!version_val.is_number()) {
			THROW(Error, "Inconsistency in '{}' configured for {}: Invalid version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
		}
		shard.version = version_val.u64();
	}

	it = obj.find("shards");
	if (it != obj.end()) {
		auto& replicas_val = it.value();
		if (!replicas_val.is_array()) {
			THROW(Error, "Inconsistency in 'shards' configured for {}: Invalid array", repr(endpoint.to_string()));
		}
		for (auto& node_name_val : replicas_val) {
			if (!node_name_val.is_string()) {
				THROW(Error, "Inconsistency in 'shards' configured for {}: Invalid node name", repr(endpoint.to_string()));
			}
			shard.nodes.push_back(node_name_val.str());
		}
	}

	return shard;
}


NodeSettings
load_settings(const std::string& normalized_path)
{
	L_CALL("load_settings(<index_endpoints>, {})", repr(normalized_path));

	auto nodes = Node::nodes();
	assert(!nodes.empty());

	try {
		NodeSettings settings;

		Endpoint endpoint(".xapiand/indices");
		auto endpoints = XapiandManager::resolve_index_endpoints(endpoint, true);
		assert(!endpoints.empty());
		DatabaseHandler db_handler(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN);
		auto document = db_handler.get_document(normalized_path);
		auto obj = document.get_obj();

		auto it = obj.find(VERSION_FIELD_NAME);
		if (it != obj.end()) {
			auto& version_val = it.value();
			if (!version_val.is_number()) {
				THROW(Error, "Inconsistency in '{}' configured for {}: Invalid version number", VERSION_FIELD_NAME, repr(endpoint.to_string()));
			}
			settings.version = version_val.u64();
		}

		it = obj.find("number_of_replicas");
		if (it != obj.end()) {
			auto& n_replicas_val = it.value();
			if (!n_replicas_val.is_number()) {
				THROW(Error, "Inconsistency in 'number_of_replicas' configured for {}: Invalid number", repr(endpoint.to_string()));
			}
			settings.num_replicas_plus_master = n_replicas_val.u64() + 1;
		}

		it = obj.find("number_of_shards");
		if (it != obj.end()) {
			auto& n_shards_val = it.value();
			if (!n_shards_val.is_number()) {
				THROW(Error, "Inconsistency in 'number_of_shards' configured for {}: Invalid number", repr(endpoint.to_string()));
			}
			settings.num_shards = n_shards_val.u64();
			size_t replicas_size = 0;
			for (size_t shard_num = 1; shard_num <= settings.num_shards; ++shard_num) {
				auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, shard_num);
				auto replica_document = db_handler.get_document(shard_normalized_path);
				auto shard = load_replicas(endpoint, replica_document.get_obj());
				auto replicas_size_ = shard.nodes.size();
				if (replicas_size_ == 0 || replicas_size_ > settings.num_replicas_plus_master || (replicas_size && replicas_size != replicas_size_)) {
					THROW(Error, "Inconsistency in number of replicas configured for {}", repr(endpoint.to_string()));
				}
				replicas_size = replicas_size_;
				settings.shards.push_back(std::move(shard));
			}
		}

		if (!settings.num_shards) {
			auto shard = load_replicas(endpoint, obj);
			auto replicas_size_ = shard.nodes.size();
			if (replicas_size_ == 0 || replicas_size_ > settings.num_replicas_plus_master) {
				THROW(Error, "Inconsistency in number of replicas configured for {}", repr(endpoint.to_string()));
			}
			settings.shards.push_back(std::move(shard));
		}

		return settings;
	} catch (const Xapian::DocNotFoundError&) {
	} catch (const Xapian::DatabaseNotFoundError&) {
	}

	return {};
}


MsgPack
shards_to_obj(const std::vector<NodeSettingsShard>& shards)
{
	MsgPack nodes = MsgPack::ARRAY();
	for (auto& shard : shards) {
		MsgPack node_replicas = MsgPack::ARRAY();
		for (auto name : shard.nodes) {
			auto node = Node::get_node(name);
			node_replicas.append(MsgPack({
				{ "node", node ? MsgPack(node->name()) : MsgPack::NIL() },
			}));
		}
		nodes.append(std::move(node_replicas));
	}
	return nodes;
}


std::vector<std::vector<std::shared_ptr<const Node>>>
shards_to_nodes(const std::vector<NodeSettingsShard>& shards)
{
	L_CALL("shards_to_nodes({})", shards_to_obj(shards).to_string());

	std::vector<std::vector<std::shared_ptr<const Node>>> nodes;
	for (auto& shard : shards) {
		std::vector<std::shared_ptr<const Node>> node_replicas;
		for (auto name : shard.nodes) {
			auto node = Node::get_node(name);
			node_replicas.push_back(std::move(node));
		}
		nodes.push_back(std::move(node_replicas));
	}
	return nodes;
}


std::vector<std::vector<std::shared_ptr<const Node>>>
XapiandManager::resolve_index_nodes_impl([[maybe_unused]] const std::string& normalized_path, [[maybe_unused]] bool writable, [[maybe_unused]] bool primary, [[maybe_unused]] const MsgPack* settings, bool reload, bool rebuild, bool clear)
{
	L_CALL("XapiandManager::resolve_index_nodes_impl({}, {}, {}, {}, {}, {})", repr(normalized_path), writable, primary, settings ? settings->to_string() : "null", rebuild, clear);

	std::vector<std::vector<std::shared_ptr<const Node>>> nodes;

	if (strings::startswith(normalized_path, ".xapiand/")) {
		// Everything inside .xapiand has the primary shard inside
		// the current leader and replicas everywhere.
		if (settings && settings->is_map()) {
			THROW(ClientError, "Cannot modify settings of cluster indices.");
		}

		// Primary databases in .xapiand are always in the master
		std::vector<std::shared_ptr<const Node>> node_replicas;
		node_replicas.push_back(Node::get_leader_node());
		node_replicas.push_back(Node::get_local_node());

		if (normalized_path == ".xapiand/indices") {
			// .xapiand/indices have the default number of shards
			for (size_t i = 0; i < opts.num_shards; ++i) {
				nodes.push_back(node_replicas);
			}
		} else {
			// Everything else inside .xapiand has a single shard
			// (.xapiand/nodes, .xapiand/indices/.__N, .xapiand/* etc.)
			nodes.push_back(node_replicas);
		}

		return nodes;
	}

	static std::mutex resolve_index_lru_mtx;
	static lru::lru<std::string, NodeSettings> resolve_index_lru(opts.resolver_cache_size);

	NodeSettings node_settings;

	std::unique_lock<std::mutex> lk(resolve_index_lru_mtx);
	auto it = reload ? resolve_index_lru.end() : resolve_index_lru.find(normalized_path);
	if (it != resolve_index_lru.end()) {
		if (clear) {
			resolve_index_lru.erase(it);
			return nodes;
		}
		node_settings = it->second;
		lk.unlock();
		nodes = shards_to_nodes(node_settings.shards);
		L_SHARDS("Node settings for {} loaded from LRU", normalized_path);
	} else {
		lk.unlock();
		node_settings = load_settings(normalized_path);
		if (!node_settings.shards.empty()) {
			nodes = shards_to_nodes(node_settings.shards);
			lk.lock();
			size_t shard_num = 0;
			for (auto& shard : node_settings.shards) {
				if (shard.nodes.empty()) {
					nodes.clear();  // There were missing replicas, abort!
					break;
				}
				auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, ++shard_num);
				std::vector<NodeSettingsShard> shard_shards;
				shard_shards.push_back(shard);
				resolve_index_lru[shard_normalized_path] = NodeSettings(
					shard.version,
					shard.modified,
					node_settings.stalled,
					1,
					node_settings.num_replicas_plus_master,
					shard_shards);
			}
			if (!nodes.empty()) {
				resolve_index_lru[normalized_path] = NodeSettings(
					node_settings.version,
					node_settings.modified,
					node_settings.stalled,
					node_settings.num_shards,
					node_settings.num_replicas_plus_master,
					node_settings.shards);
			}
			lk.unlock();
			L_SHARDS("Node settings for {} loaded", normalized_path);
		} else {
			node_settings.num_shards = opts.num_shards;
			node_settings.num_replicas_plus_master = opts.num_replicas + 1;
			node_settings.modified = true;
			L_SHARDS("Node settings for {} initialized", normalized_path);
		}
	}

	assert(Node::total_nodes());

	if (settings && settings->is_map()) {
		auto num_shards = node_settings.num_shards;
		auto num_replicas_plus_master = node_settings.num_replicas_plus_master;

		auto num_shards_it = settings->find("number_of_shards");
		if (num_shards_it != settings->end()) {
			auto& num_shards_val = num_shards_it.value();
			if (num_shards_val.is_number()) {
				num_shards = num_shards_val.u64();
				if (num_shards == 0 || num_shards > 9999UL) {
					THROW(ClientError, "Invalid 'number_of_shards' setting.");
				}
			}
		}

		auto num_replicas_it = settings->find("number_of_replicas");
		if (num_replicas_it != settings->end()) {
			auto& num_replicas_val = num_replicas_it.value();
			if (num_replicas_val.is_number()) {
				num_replicas_plus_master = num_replicas_val.u64() + 1;
				if (num_replicas_plus_master == 0 || num_replicas_plus_master > 9999UL) {
					THROW(ClientError, "Invalid 'number_of_replicas' setting.");
				}
			}
		}

		if (!nodes.empty()) {
			if (num_shards != node_settings.num_shards) {
				THROW(ClientError, "It is not allowed to change 'number_of_shards' setting.");
			}
			if (num_replicas_plus_master != node_settings.num_replicas_plus_master) {
				nodes.clear();
			}
		}

		if (node_settings.num_replicas_plus_master != num_replicas_plus_master) {
			node_settings.num_replicas_plus_master = num_replicas_plus_master;
			node_settings.modified = true;
		}

		if (node_settings.num_shards != num_shards) {
			node_settings.num_shards = num_shards;
			node_settings.modified = true;
		}
	}

	if (nodes.empty() || rebuild) {
		L_SHARDS("    Configuring {} replicas for {} shards", node_settings.num_replicas_plus_master - 1, node_settings.num_shards);

		std::vector<std::shared_ptr<const Node>> node_nodes;
		if (node_settings.shards.empty()) {
			size_t routing_key = jump_consistent_hash(normalized_path, Node::total_nodes());
			node_settings.shards = calculate_shards(routing_key, node_nodes, node_settings.num_shards);
			assert(!node_settings.shards.empty());
			node_settings.modified = true;
		}
		settle_replicas(node_settings.shards, node_nodes, node_settings.num_replicas_plus_master);

		if (writable) {
			update_primary(normalized_path, node_settings);
			index_settings(normalized_path, node_settings);
		}

		nodes = shards_to_nodes(node_settings.shards);

		lk.lock();
		size_t shard_num = 0;
		for (auto& shard : node_settings.shards) {
			assert(!shard.nodes.empty());
			auto shard_normalized_path = strings::format("{}/.__{}", normalized_path, ++shard_num);
			std::vector<NodeSettingsShard> shard_shards;
			shard_shards.push_back(shard);
			resolve_index_lru[shard_normalized_path] = NodeSettings(
				shard.version,
				shard.modified,
				node_settings.stalled,
				1,
				node_settings.num_replicas_plus_master,
				shard_shards);
		}
		if (!nodes.empty()) {
			resolve_index_lru[normalized_path] = NodeSettings(
				node_settings.version,
				node_settings.modified,
				node_settings.stalled,
				node_settings.num_shards,
				node_settings.num_replicas_plus_master,
				node_settings.shards);
		}
		lk.unlock();
	}

	return nodes;
}


Endpoints
XapiandManager::resolve_index_endpoints_impl(const Endpoint& endpoint, bool writable, bool primary, const MsgPack* settings)
{
	L_CALL("XapiandManager::resolve_index_endpoints_impl({}, {}, {}, {})", repr(endpoint.to_string()), writable, primary, settings ? settings->to_string() : "null");

	auto unsharded = unsharded_path(endpoint.path);
	std::string unsharded_endpoint_path;
	if (unsharded.second) {
		unsharded_endpoint_path = std::string(unsharded.first);
	}
	auto& endpoint_path = unsharded.second ? unsharded_endpoint_path : endpoint.path;

	bool rebuild = false;
	int t = CONFLICT_RETRIES;
	while (true) {
		try {
			Endpoints endpoints;

			auto nodes = resolve_index_nodes_impl(endpoint_path, writable, primary, settings, t != CONFLICT_RETRIES, rebuild, false);
			bool retry = !rebuild;
			rebuild = false;

			int n_shards = nodes.size();
			size_t shard_num = 0;
			for (const auto& shard_nodes : nodes) {
				auto path = n_shards == 1 ? endpoint_path : strings::format("{}/.__{}", endpoint_path, ++shard_num);
				if (!unsharded.second || path == endpoint.path) {
					Endpoint node_endpoint;
					for (const auto& node : shard_nodes) {
						node_endpoint = Endpoint(path, node);
						if (writable) {
							if (Node::is_active(node)) {
								L_SHARDS("Active writable node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							rebuild = retry;
							break;
						} else {
							if (Node::is_active(node)) {
								L_SHARDS("Active node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							if (primary) {
								L_SHARDS("Inactive primary node used (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
								break;
							}
							L_SHARDS("Inactive node ignored (of {} nodes) {}", Node::total_nodes(), node ? node->__repr__() : "null");
						}
					}
					endpoints.add(node_endpoint);
					if (rebuild || unsharded.second) {
						break;
					}
				}
			}

			if (!rebuild) {
				return endpoints;
			}
		} catch (const Xapian::DocVersionConflictError&) {
			if (--t == 0) { throw; }
		}
	}
}


std::string
XapiandManager::server_metrics_impl()
{
	L_CALL("XapiandManager::server_metrics_impl()");

	auto& metrics = Metrics::metrics();

	metrics.xapiand_uptime.Set(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _process_start).count());

	// http client tasks:
	metrics.xapiand_http_clients_running.Set(_http_client_pool->running_size());
	metrics.xapiand_http_clients_queue_size.Set(_http_client_pool->size());
	metrics.xapiand_http_clients_pool_size.Set(_http_client_pool->threadpool_size());
	metrics.xapiand_http_clients_capacity.Set(_http_client_pool->threadpool_capacity());

#ifdef XAPIAND_CLUSTERING
	// remote protocol client tasks:
	metrics.xapiand_remote_clients_running.Set(_remote_client_pool->running_size());
	metrics.xapiand_remote_clients_queue_size.Set(_remote_client_pool->size());
	metrics.xapiand_remote_clients_pool_size.Set(_remote_client_pool->threadpool_size());
	metrics.xapiand_remote_clients_capacity.Set(_remote_client_pool->threadpool_capacity());
#endif

	// servers_threads:
	metrics.xapiand_servers_running.Set(_http_server_pool->running_size());
	metrics.xapiand_servers_queue_size.Set(_http_server_pool->size());
	metrics.xapiand_servers_pool_size.Set(_http_server_pool->threadpool_size());
	metrics.xapiand_servers_capacity.Set(_http_server_pool->threadpool_capacity());

	// committers_threads:
	metrics.xapiand_committers_running.Set(committer()->running_size());
	metrics.xapiand_committers_queue_size.Set(committer()->size());
	metrics.xapiand_committers_pool_size.Set(committer()->threadpool_size());
	metrics.xapiand_committers_capacity.Set(committer()->threadpool_capacity());

	// fsync_threads:
	metrics.xapiand_fsync_running.Set(fsyncher()->running_size());
	metrics.xapiand_fsync_queue_size.Set(fsyncher()->size());
	metrics.xapiand_fsync_pool_size.Set(fsyncher()->threadpool_size());
	metrics.xapiand_fsync_capacity.Set(fsyncher()->threadpool_capacity());

	metrics.xapiand_http_current_connections.Set(_http_clients.load());
#ifdef XAPIAND_CLUSTERING
	// current connections:
	metrics.xapiand_remote_current_connections.Set(_remote_clients.load());
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
	metrics.xapiand_tracked_memory_bytes.Set(allocators::total_allocated());
#endif
	metrics.xapiand_total_memory_system_bytes.Set(get_total_ram());
	metrics.xapiand_total_virtual_memory_used.Set(get_total_virtual_memory());
	metrics.xapiand_total_disk_bytes.Set(get_total_disk_size());
	metrics.xapiand_free_disk_bytes.Set(get_free_disk_size());

	// databases:
	auto count = _database_pool->count();
	metrics.xapiand_endpoints.Set(count.first);
	metrics.xapiand_databases.Set(count.second);

	return metrics.serialise();
}


bool
XapiandManager::exchange_state(State from, State to, std::chrono::milliseconds timeout, std::string_view format_timeout, std::string_view format_done)
{
	assert(_manager);
	if (from != to) {
		if (_manager->_state.compare_exchange_strong(from, to)) {
			auto& log = _manager->log;
			if (timeout == 0s) {
				L_MANAGER_TIMED_CLEAR();
			} else {
				L_MANAGER_TIMED(timeout, format_timeout, format_done);
			}
			return true;
		}
	}
	return false;
}


std::string
XapiandManager::__repr__() const
{
	return strings::format(STEEL_BLUE + "<XapiandManager ({}) {{cnt:{}}}{}{}{}>",
		enum_name(_state.load()),
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}


#ifdef XAPIAND_CLUSTERING

void
trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint)
{
	XapiandManager::replication()->trigger_replication({src_endpoint, dst_endpoint, false});
}

#endif
