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
#include "index_resolver_lru.h"                  // for IndexResolverLRU
#include "length.h"                              // for serialise_length
#include "log.h"                                 // for L_CALL, L_DEBUG
#include "lru.h"                                 // for lru::lru
#include "memory_stats.h"                        // for get_total_ram, get_total_virtual_memor...
#include "metrics.h"                             // for Metrics::metrics
#include "msgpack.h"                             // for MsgPack, object::object
#include "namegen.h"                             // for name_generator
#include "nanosleep.h"                           // for nanosleep
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


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_MANAGER
// #define L_MANAGER L_DARK_CYAN


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
	  total_clients(0),
	  http_clients(0),
	  remote_clients(0),
	  replication_clients(0),
	  schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size)),
	  database_pool(std::make_unique<DatabasePool>(opts.database_pool_size, opts.max_database_readers)),
	  wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  index_settings_resolver(),
	  http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_http_servers)),
#ifdef XAPIAND_CLUSTERING
	  remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_remote_servers)),
	  replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_replication_servers)),
#endif
	  doc_preparer_pool(std::make_unique<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>>("DP{:02}", opts.num_doc_preparers)),
	  doc_indexer_pool(std::make_unique<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>>("DI{:02}", opts.num_doc_indexers)),
	  state(State::RESET),
	  node_name(opts.node_name),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _new_cluster(0),
	  _process_start(std::chrono::steady_clock::now()),
	  _try_shutdown(0),
	  atom_sig(0)
{
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::steady_clock::time_point process_start_)
	: Worker(std::weak_ptr<Worker>{}, ev_loop_, ev_flags_),
	  total_clients(0),
	  http_clients(0),
	  remote_clients(0),
	  replication_clients(0),
	  schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size)),
	  database_pool(std::make_unique<DatabasePool>(opts.database_pool_size, opts.max_database_readers)),
	  wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_http_servers)),
#ifdef XAPIAND_CLUSTERING
	  remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_remote_servers)),
	  replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_replication_servers)),
#endif
	  doc_preparer_pool(std::make_unique<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>>("DP{:02}", opts.num_doc_preparers)),
	  doc_indexer_pool(std::make_unique<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>>("DI{:02}", opts.num_doc_indexers)),
	  state(State::RESET),
	  node_name(opts.node_name),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _new_cluster(0),
	  _process_start(process_start_),
	  _try_shutdown(0),
	  try_shutdown_timer(*ev_loop),
	  signal_sig_async(*ev_loop),
	  setup_node_async(*ev_loop),
	  set_cluster_database_ready_async(*ev_loop),
	  dispatch_command_async(*ev_loop),
	  atom_sig(0)
{
	try_shutdown_timer.set<XapiandManager, &XapiandManager::try_shutdown_timer_cb>(this);

	signal_sig_async.set<XapiandManager, &XapiandManager::signal_sig_async_cb>(this);
	signal_sig_async.start();

	setup_node_async.set<XapiandManager, &XapiandManager::setup_node_async_cb>(this);
	setup_node_async.start();

	set_cluster_database_ready_async.set<XapiandManager, &XapiandManager::set_cluster_database_ready_async_cb>(this);
	set_cluster_database_ready_async.start();

	dispatch_command_async.set<XapiandManager, &XapiandManager::dispatch_command_async_cb>(this);
	dispatch_command_async.start();
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


IndexSettings
XapiandManager::resolve_index_settings(std::string_view normalized_path, bool writable, bool primary, const MsgPack* settings, std::shared_ptr<const Node> primary_node, bool reload, bool rebuild, bool clear)
{
	return manager(true)->resolve_index_settings_impl(normalized_path, writable, primary, settings, primary_node, reload, rebuild, clear);
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
XapiandManager::save_node_name(std::string_view name)
{
	L_CALL("XapiandManager::save_node_name({})", name);

	int fd = io::open(".xapiand/node", O_WRONLY | O_CREAT, 0644);
	if (fd != -1) {
		if (io::write(fd, name.data(), name.size()) != static_cast<ssize_t>(name.size())) {
			THROW(Error, "Cannot write in node file");
		}
		io::close(fd);
	} else {
		THROW(Error, "Cannot open or create the node file");
	}
}


std::string
XapiandManager::set_node_name(std::string_view name)
{
	L_CALL("XapiandManager::set_node_name({})", name);

	node_name = load_node_name();

	if (node_name.empty()) {
		if (!name.empty()) {
			// Ignore empty node_name
			node_name = name;
			save_node_name(node_name);
		}
	}

	return node_name;
}


std::pair<struct sockaddr_in, std::string>
XapiandManager::host_address()
{
	L_CALL("XapiandManager::host_address()");

	auto hostname = opts.bind_address.empty() ? nullptr : opts.bind_address.c_str();

	auto address = host_address(hostname);
	if (address.first.sin_addr.s_addr && !address.second.empty()) {
		return address;
	} else {
		L_WARNING("WARNING: host_address: Cannot find the node's IP address!");
	}

	L_WARNING("WARNING: host_address: Fallback to loopback IP address!");

	address = host_address("127.0.0.1");
	if (address.first.sin_addr.s_addr && !address.second.empty()) {
		return address;
	} else {
		L_CRIT("ERROR: host_address: Cannot fallback to loopback IP address!");
	}

	sig_exit(-EX_CONFIG);
	return {{}, ""};
}


std::pair<struct sockaddr_in, std::string>
XapiandManager::host_address(const char *hostname)
{
	L_CALL("XapiandManager::host_address(<hostname>)");

	struct sockaddr_in addr{};

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
	return {{}, ""};
}


void
XapiandManager::signal_sig(int sig)
{
	atom_sig = sig;
	signal_sig_async.send();
}


void
XapiandManager::try_shutdown_timer_cb(ev::timer& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("XapiandManager::try_shutdown_timer_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	L_EV_BEGIN("XapiandManager::try_shutdown_timer_cb:BEGIN");
	L_EV_END("XapiandManager::try_shutdown_timer_cb:END");

	try_shutdown();
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
		shutdown_sig(sig, true);
	}

	switch (sig) {
		case SIGTERM:
		case SIGINT:
			shutdown_sig(sig, true);
			break;
		case SIGUSR1:
#if defined(__APPLE__) || defined(__FreeBSD__)
		case SIGINFO:
#endif
#ifdef XAPIAND_CLUSTERING
			print(DARK_STEEL_BLUE + "Threads:\n{}" + DARK_STEEL_BLUE + "Workers:\n{}" + DARK_STEEL_BLUE + "Databases:\n{}" + DARK_STEEL_BLUE + "Schemas:\n{}" + DARK_STEEL_BLUE + "Nodes:\n{}", dump_callstacks(), dump_tree(), database_pool->dump_databases(), schemas->dump_schemas(), Node::dump_nodes());
#else
			print(DARK_STEEL_BLUE + "Threads:\n{}" + DARK_STEEL_BLUE + "Workers:\n{}" + DARK_STEEL_BLUE + "Databases:\n{}" + DARK_STEEL_BLUE + "Schemas:\n{}", dump_callstacks(), dump_tree(), database_pool->dump_databases(), schemas->dump_schemas());
#endif
			break;
	}
}


void
XapiandManager::shutdown_sig(int sig, bool async)
{
	L_CALL("XapiandManager::shutdown_sig({}, {})", sig, async);

	if (sig) {
		if (sig < 0) {
			// System Exit with error code (-sig)
			atom_sig = sig;
			if (is_runner() && is_running_loop()) {
				break_loop();
			} else {
				throw SystemExit(-sig);
			}
			return;
		}

		auto now = epoch::now<std::chrono::milliseconds>();

		if (_shutdown_now != 0) {
			if (now >= _shutdown_now + 200) {
				if (now <= _shutdown_now + 1000) {
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
			if (now >= _shutdown_asap + 200) {
				if (now <= _shutdown_asap + 1000) {
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

		if (now >= _shutdown_asap + 200) {
			_shutdown_asap = now;
		}
	} else {
		if (_try_shutdown++ > 6) {
			auto now = epoch::now<std::chrono::milliseconds>();
			_shutdown_now = now;
		}
	}

	shutdown(_shutdown_asap, _shutdown_now, async);
}


void
XapiandManager::shutdown_impl(long long asap, long long now)
{
	L_CALL("XapiandManager::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		if (!ready_to_end_http()) {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: HTTP is busy...", "Continuing shutdown process!");
		} else if (database_pool->is_pending()) {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: Waiting for replicators...", "Continuing shutdown process!");
		} else if (!ready_to_end_remote()) {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: Remote Protocol is busy...", "Continuing shutdown process!");
		} else if (!ready_to_end_replication()) {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: Replication Protocol is busy...", "Continuing shutdown process!");
		} else {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down...", "Starting shutdown process!");
		}

		try_shutdown_timer.repeat = 5.0;
		try_shutdown_timer.again();
		L_EV("Configured try shutdown timer ({})", try_shutdown_timer.repeat);

		if (now != 0 || ready_to_end(true)) {
			stop(false);
			destroy(false);
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
	std::string name = load_node_name();
	if (!name.empty()) {
		if (!node_name.empty() && strings::lower(node_name) != strings::lower(name)) {
			node_name = "~";
		} else {
			node_name = name;
		}
	}
	if (opts.solo) {
		if (node_name.empty()) {
			node_name = name_generator();
		}
	}
	node_copy->name(node_name);

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
		if (node_name == "~") {
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
		int sig = 0;
		atom_sig.compare_exchange_strong(sig, -EX_SOFTWARE);
		shutdown_sig(atom_sig.load(), false);
		join();
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
		discovery = Worker::make_shared<Discovery>(shared_from_this(), nullptr, ev_flags, opts.discovery_group.c_str(), discovery_port);
		msg += discovery->getDescription();
		discovery->run();

		discovery->start();

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
	std::shared_ptr<const Node> primary_node;
	Endpoint primary_endpoint;

	_new_cluster = 0;
	bool found = false;

	for (int t = 10; !found && t >= 0; --t) {
		nanosleep(100000000);  // sleep for 100 miliseconds
		local_node = Node::get_local_node();
		primary_node = Node::get_primary_node();
		primary_endpoint = Endpoint{".xapiand/nodes", primary_node};

		try {
			DatabaseHandler db_handler(Endpoints{primary_endpoint});
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
						node_added(obj["name"].str());
					}
				} else
#endif
				{
					db_handler.get_document(local_node->lower_name());
					found = true;
				}
			}
		} catch (const Xapian::DatabaseNotAvailableError&) {
			if (t == 0) {
				if (!primary_node->is_active()) {
					L_WARNING("Primary node {}{}" + WARNING_COL + " is not active!", primary_node->col().ansi(), primary_node->to_string());
				}
				throw;
			}
			continue;
		} catch (const Xapian::DocNotFoundError&) {
		} catch (const Xapian::DatabaseNotFoundError&) { }

		if (!found) {
			try {
				DatabaseHandler db_handler(Endpoints{primary_endpoint}, DB_CREATE_OR_OPEN | DB_WRITABLE);
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
				db_handler.update(local_node->lower_name(), UNKNOWN_REVISION, false, true, obj, false, msgpack_type);
				_new_cluster = 1;
#ifdef XAPIAND_CLUSTERING
				if (!opts.solo) {
					add_node(local_node->name());

					if (Node::is_superset(local_node, primary_node)) {
						L_INFO("Cluster database doesn't exist. Generating database...");
					} else {
						if (!primary_node || primary_node->empty()) {
							throw Xapian::NetworkError("Endpoint node is invalid");
						}
						if (!primary_node->is_active()) {
							throw Xapian::NetworkError("Endpoint node is inactive");
						}
						auto port = primary_node->remote_port;
						if (port == 0) {
							throw Xapian::NetworkError("Endpoint node without a valid port");
						}
						auto& host = primary_node->host();
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
	node_name = set_node_name(local_node->name());
	if (strings::lower(node_name) != local_node->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node);
		local_node_copy->name(node_name);
		Node::set_local_node(std::shared_ptr<const Node>(local_node_copy.release()));
		local_node = Node::get_local_node();
	}

	Metrics::metrics({{NODE_LABEL, node_name}, {CLUSTER_LABEL, opts.cluster_name}});

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		auto leader_node = Node::get_leader_node();
		if (local_node->is_leader()) {
			// The local node is the leader
			load_nodes();
		}
		if (Node::is_superset(local_node, primary_node)) {
			set_cluster_database_ready_impl();
		} else {
			L_INFO("Synchronizing cluster from {}{}" + INFO_COL + "...", primary_node->col().ansi(), primary_node->to_string());
			_new_cluster = 2;
			// Replicate cluster database from the leader
			replication->trigger_replication({primary_endpoint, Endpoint{".xapiand/nodes"}, true});
			// Request updates from indices database shards
			auto endpoints = XapiandManager::resolve_index_endpoints(Endpoint{".xapiand/indices"}, true);
			assert(!endpoints.empty());
			for (auto& endpoint : endpoints) {
				Endpoint remote_endpoint{endpoint.path, primary_node};
				Endpoint local_endpoint{endpoint.path};
				replication->trigger_replication({remote_endpoint, local_endpoint, false});
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

	auto local_node = Node::get_local_node();

	int http_tries = opts.http_port ? 1 : 10;
	int http_port = opts.http_port ? opts.http_port : XAPIAND_HTTP_SERVERPORT;

#ifdef XAPIAND_CLUSTERING
	int remote_tries = opts.remote_port ? 1 : 10;
	int remote_port = opts.remote_port ? opts.remote_port : XAPIAND_REMOTE_SERVERPORT;
	int replication_tries = opts.replication_port ? 1 : 10;
	int replication_port = opts.replication_port ? opts.replication_port : XAPIAND_REPLICATION_SERVERPORT;

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

	http = Worker::make_shared<Http>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? 0 : http_tries);

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		remote = Worker::make_shared<RemoteProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? 0 : remote_tries);
        replication = Worker::make_shared<ReplicationProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? 0 : replication_tries);
	}
#endif

	for (ssize_t i = 0; i < opts.num_http_servers; ++i) {
		auto _http_server = Worker::make_shared<HttpServer>(http, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? http_tries : 0);
		if (_http_server->addr.sin_family) {
			http->addr = _http_server->addr;
		}
		http_server_pool->enqueue(std::move(_http_server));
	}

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		for (ssize_t i = 0; i < opts.num_remote_servers; ++i) {
			auto _remote_server = Worker::make_shared<RemoteProtocolServer>(remote, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? remote_tries : 0);
			if (_remote_server->addr.sin_family) {
				remote->addr = _remote_server->addr;
			}
			remote_server_pool->enqueue(std::move(_remote_server));
		}

		for (ssize_t i = 0; i < opts.num_replication_servers; ++i) {
			auto _replication_server = Worker::make_shared<ReplicationProtocolServer>(replication, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? replication_tries : 0);
			if (_replication_server->addr.sin_family) {
				replication->addr = _replication_server->addr;
			}
			replication_server_pool->enqueue(std::move(_replication_server));
		}
	}
#endif

	// Setup local node ports.
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->http_port = ntohs(http->addr.sin_port);
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		node_copy->remote_port = ntohs(remote->addr.sin_port);
		node_copy->replication_port = ntohs(replication->addr.sin_port);
	}
#endif
	Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));

	std::string msg("Servers listening on ");
	msg += http->getDescription();
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		msg += ", " + remote->getDescription();
		msg += " and " + replication->getDescription();
	}
#endif
	L(-LOG_NOTICE, NOTICE_COL, msg);

	// Setup database cleanup thread.
	database_cleanup = Worker::make_shared<DatabaseCleanup>(shared_from_this(), nullptr, ev_flags);
	database_cleanup->run();
	database_cleanup->start();

	// Start committers, fsynchers, database updaters and replicator triggers.
	committer();
	fsyncher();
#ifdef XAPIAND_CLUSTERING
	db_updater();
	schema_updater();
	settings_updater();
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

	exchange_state(state.load(), State::READY);

	http->start();

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		remote->start();
		replication->start();
		discovery->cluster_enter();
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
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	if (!database_pool) {
		return;  // already joined!
	}

	// This method should finish and wait for all objects and threads to finish
	// their work. Order of waiting for objects here matters!
	L_MANAGER(STEEL_BLUE + "Workers:\n{}Databases:\n{}Nodes:\n{}", dump_tree(), database_pool->dump_databases(), Node::dump_nodes());

	////////////////////////////////////////////////////////////////////
	if (http_server_pool) {
		L_MANAGER("Finishing http servers pool!");
		http_server_pool->finish();

		L_MANAGER("Waiting for {} http server{}...", http_server_pool->running_size(), (http_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP servers...", "HTTP servers finished!");
		while (!http_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (http_client_pool) {
		L_MANAGER("Finishing http client threads pool!");
		http_client_pool->finish();

		L_MANAGER("Waiting for {} http client thread{}...", http_client_pool->running_size(), (http_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP clients...", "HTTP clients finished!");
		while (!http_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (doc_preparer_pool) {
		L_MANAGER("Finishing bulk document preparer threads pool!");
		doc_preparer_pool->finish();

		L_MANAGER("Waiting for {} bulk document preparer thread{}...", doc_preparer_pool->running_size(), (doc_preparer_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the bulk document preparers...", "Bulk document preparers finished!");
		while (!doc_preparer_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (doc_indexer_pool) {
		L_MANAGER("Finishing bulk document indexer threads pool!");
		doc_indexer_pool->finish();

		L_MANAGER("Waiting for {} bulk document indexer thread{}...", doc_indexer_pool->running_size(), (doc_indexer_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the bulk document indexers...", "Bulk document indexers finished!");
		while (!doc_indexer_pool->join(500ms)) {
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
	if (remote_server_pool) {
		L_MANAGER("Finishing remote protocol servers pool!");
		remote_server_pool->finish();

		L_MANAGER("Waiting for {} remote protocol server{}...", remote_server_pool->running_size(), (remote_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the remote protocol servers...", "Remote Protocol servers finished!");
		while (!remote_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (remote_client_pool) {
		L_MANAGER("Finishing remote protocol threads pool!");
		remote_client_pool->finish();

		L_MANAGER("Waiting for {} remote protocol thread{}...", remote_client_pool->running_size(), (remote_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the remote protocol threads...", "Remote Protocol threads finished!");
		while (!remote_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}
#endif

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

	auto& settings_updater_obj = schema_updater(false);
	if (settings_updater_obj) {
		L_MANAGER("Finishing settings updater!");
		settings_updater_obj->finish();

		L_MANAGER("Waiting for {} settings updater{}...", settings_updater_obj->running_size(), (settings_updater_obj->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the settings updaters...", "Settings updaters finished!");
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

	////////////////////////////////////////////////////////////////////
	if (replication_server_pool) {
		L_MANAGER("Finishing replication protocol servers pool!");
		replication_server_pool->finish();

		L_MANAGER("Waiting for {} replication server{}...", replication_server_pool->running_size(), (replication_server_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the replication protocol servers...", "Replication Protocol servers finished!");
		while (!replication_server_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	if (replication_client_pool) {
		L_MANAGER("Finishing replication protocol threads pool!");
		replication_client_pool->finish();

		L_MANAGER("Waiting for {} replication protocol thread{}...", replication_client_pool->running_size(), (replication_client_pool->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the replication protocol threads...", "Replication Protocol threads finished!");
		while (!replication_client_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
	if (database_pool) {
		L_MANAGER("Finishing database pool!");
		database_pool->finish();

		L_MANAGER("Clearing and waiting for database pool!");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database pool...", "Database pool finished!");
		while (!database_pool->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#if XAPIAND_DATABASE_WAL

	////////////////////////////////////////////////////////////////////
	if (wal_writer) {
		L_MANAGER("Finishing WAL writers!");
		wal_writer->finish();

		L_MANAGER("Waiting for {} WAL writer{}...", wal_writer->running_size(), (wal_writer->running_size() == 1) ? "" : "s");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the WAL writers...", "WAL writers finished!");
		while (!wal_writer->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

#if XAPIAND_CLUSTERING

	////////////////////////////////////////////////////////////////////
	if (discovery) {
		discovery->stop();

		L_MANAGER("Finishing Discovery loop!");
		discovery->finish();

		L_MANAGER("Waiting for Discovery...");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the discovery protocol...", "Discovery protocol finished!");
		while (!discovery->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

#endif

	////////////////////////////////////////////////////////////////////
	if (database_cleanup) {
		L_MANAGER("Finishing Database Cleanup loop!");
		database_cleanup->finish();

		L_MANAGER("Waiting for Database Cleanup...");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database cleanup worker...", "Database cleanup worker finished!");
		while (!database_cleanup->join(500ms)) {
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
		}
	}

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

	////////////////////////////////////////////////////////////////////

	L_MANAGER_TIMED(1s, "Is taking too long to reset manager...", "Manager reset finished!");

	http.reset();
#ifdef XAPIAND_CLUSTERING
	remote.reset();
	replication.reset();
	discovery.reset();
#endif

	database_pool.reset();

	wal_writer.reset();

	http_client_pool.reset();
	http_server_pool.reset();

	doc_indexer_pool.reset();
	doc_preparer_pool.reset();

#ifdef XAPIAND_CLUSTERING
	remote_client_pool.reset();
	remote_server_pool.reset();
	replication_client_pool.reset();
	replication_server_pool.reset();
#endif

	database_cleanup.reset();

#ifdef XAPIAND_CLUSTERING
	trigger_replication_obj.reset();
	db_updater_obj.reset();
#endif
	committer_obj.reset();
	fsyncher_obj.reset();

	schemas.reset();

	////////////////////////////////////////////////////////////////////
	L_MANAGER("Server ended!");
}


bool
XapiandManager::ready_to_end_http(bool /*notify*/)
{
	return (
		!http_clients
	);
}


bool
XapiandManager::ready_to_end_remote(bool notify)
{
	return (
		!remote_clients &&
		!database_pool->is_pending(notify)
	);
}


bool
XapiandManager::ready_to_end_replication(bool notify)
{
	return (
		!replication_clients &&
		!database_pool->is_pending(notify)
	);
}


bool
XapiandManager::ready_to_end_database_cleanup(bool notify)
{
	return (
		ready_to_end_http(notify) &&
		ready_to_end_remote(notify) &&
		ready_to_end_replication(notify)
	);
}


bool
XapiandManager::ready_to_end_discovery(bool notify)
{
	return (
		ready_to_end_http(notify) &&
		ready_to_end_remote(notify) &&
		ready_to_end_replication(notify)
	);
}


bool
XapiandManager::ready_to_end(bool notify)
{
	return (
		ready_to_end_http(notify) &&
		ready_to_end_remote(notify) &&
		ready_to_end_replication(notify) &&
		ready_to_end_database_cleanup(notify) &&
		ready_to_end_discovery(notify)
	);
}



void
XapiandManager::dispatch_command_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("XapiandManager::dispatch_command_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	std::pair<Command, std::string> command;
	while (dispatch_command_args.try_dequeue(command)) {
		_dispatch_command(command.first, command.second);
	}
}


void
XapiandManager::dispatch_command_impl(Command command, const std::string& data)
{
	L_CALL("XapiandManager::dispatch_command_impl({}, {})", enum_name(command), repr(data));

	dispatch_command_args.enqueue(std::make_pair(command, data));

	dispatch_command_async.send();
}


void
XapiandManager::_dispatch_command(Command command, [[maybe_unused]] const std::string& data)
{
	L_CALL("XapiandManager::_dispatch_command({}, {})", enum_name(command), repr(data));


	switch (command) {
#ifdef XAPIAND_CLUSTERING
		case Command::RAFT_APPLY_COMMAND:
			node_added(data);
			break;
		case Command::RAFT_SET_LEADER_NODE:
			new_leader();
			break;
		case Command::ELECT_PRIMARY:
			discovery->_ASYNC_elect_primary_send(data);
			break;
		case Command::ASYNC_PRIMARY_UPDATED:
			discovery->_ASYNC_primary_updated(data);
			break;
		case Command::ASYNC_ELECT_PRIMARY:
			discovery->_ASYNC_elect_primary(data);
			break;
		case Command::ASYNC_ELECT_PRIMARY_RESPONSE:
			discovery->_ASYNC_elect_primary_response(data);
			break;
#endif
		default:
			break;
	}
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::load_nodes()
{
	L_CALL("XapiandManager::load_nodes()");

	// See if our local database has all nodes currently commited.
	// If any is missing, it gets added.

	auto primary_node = Node::get_primary_node();
	if (!primary_node->is_active()) {
		L_WARNING("Primary node {}{}" + WARNING_COL + " is not active!", primary_node->col().ansi(), primary_node->to_string());
	}
	Endpoint primary_endpoint{".xapiand/nodes", primary_node};
	DatabaseHandler db_handler(Endpoints{primary_endpoint}, DB_CREATE_OR_OPEN | DB_WRITABLE);
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
XapiandManager::new_leader()
{
	L_CALL("XapiandManager::new_leader()");

	auto leader_node = Node::get_leader_node();
	if (leader_node && !leader_node->empty()) {
		L_INFO("New leader of cluster {} is {}{}", repr(opts.cluster_name), leader_node->col().ansi(), leader_node->to_string());
		if (leader_node->is_local()) {
			if (state == State::READY) {
				try {
					// If we get promoted to leader, we immediately try to load the nodes.
					load_nodes();
				} catch (...) {
					L_EXC("ERROR: Cannot load local nodes!");
				}
			}
		}
	} else {
		L_INFO("There is currently no leader for cluster {}!", repr(opts.cluster_name));
	}
}


void
XapiandManager::add_node(const std::string& name)
{
	L_CALL("XapiandManager::add_node({})", name);

	node_added(name);

	discovery->raft_add_command(name);
}


void
XapiandManager::node_added(const std::string& name)
{
	L_CALL("XapiandManager::node_added({})", name);

	Node node_copy;

	auto node = Node::get_node(name);
	if (node) {
		node_copy = *node;
	} else {
		node_copy.name(name);
	}

	auto put = Node::touch_node(node_copy, false, false);
	if (put.first == nullptr) {
		L_ERR("Denied node {}{}" + ERR_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node_copy.col().ansi(), node_copy.to_string(), node_copy.host(), node_copy.http_port, node_copy.remote_port, node_copy.replication_port);
	} else {
		node = put.first;
		L_DEBUG("Added node {}{}" + INFO_COL + "! (ip:{}, http_port:{}, remote_port:{}, replication_port:{})", node->col().ansi(), node->to_string(), node->host(), node->http_port, node->remote_port, node->replication_port);
	}
}


#endif


IndexSettings
XapiandManager::resolve_index_settings_impl(std::string_view normalized_path, bool writable, [[maybe_unused]] bool primary, const MsgPack* settings, std::shared_ptr<const Node> primary_node, bool reload, bool rebuild, bool clear)
{
	L_CALL("XapiandManager::resolve_index_settings_impl()");

	return index_settings_resolver->resolve_index_settings(normalized_path, writable, primary, settings, primary_node, reload, rebuild, clear);
}


Endpoints
XapiandManager::resolve_index_endpoints_impl(const Endpoint& endpoint, bool writable, bool primary, const MsgPack* settings)
{
	L_CALL("XapiandManager::resolve_index_endpoints_impl()");

	return index_settings_resolver->resolve_index_endpoints(endpoint, writable, primary, settings);
}


std::string
XapiandManager::server_metrics_impl()
{
	L_CALL("XapiandManager::server_metrics_impl()");

	auto& metrics = Metrics::metrics();

	metrics.xapiand_uptime.Set(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _process_start).count());

	// http client tasks:
	metrics.xapiand_http_clients_running.Set(http_client_pool->running_size());
	metrics.xapiand_http_clients_queue_size.Set(http_client_pool->size());
	metrics.xapiand_http_clients_pool_size.Set(http_client_pool->threadpool_size());
	metrics.xapiand_http_clients_capacity.Set(http_client_pool->threadpool_capacity());

#ifdef XAPIAND_CLUSTERING
	// remote protocol client tasks:
	metrics.xapiand_remote_clients_running.Set(remote_client_pool->running_size());
	metrics.xapiand_remote_clients_queue_size.Set(remote_client_pool->size());
	metrics.xapiand_remote_clients_pool_size.Set(remote_client_pool->threadpool_size());
	metrics.xapiand_remote_clients_capacity.Set(remote_client_pool->threadpool_capacity());
#endif

	// servers_threads:
	metrics.xapiand_servers_running.Set(http_server_pool->running_size());
	metrics.xapiand_servers_queue_size.Set(http_server_pool->size());
	metrics.xapiand_servers_pool_size.Set(http_server_pool->threadpool_size());
	metrics.xapiand_servers_capacity.Set(http_server_pool->threadpool_capacity());

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

	metrics.xapiand_http_current_connections.Set(http_clients.load());
#ifdef XAPIAND_CLUSTERING
	// current connections:
	metrics.xapiand_remote_current_connections.Set(remote_clients.load());
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
	auto count = database_pool->count();
	metrics.xapiand_endpoints.Set(count.first);
	metrics.xapiand_databases.Set(count.second);

	return metrics.serialise();
}


bool
XapiandManager::exchange_state(State from, State to, std::chrono::milliseconds timeout, std::string_view format_timeout, std::string_view format_done)
{
	if (_manager && from != to) {
		if (_manager->state.compare_exchange_strong(from, to)) {
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
		enum_name(state.load()),
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}


#ifdef XAPIAND_CLUSTERING

void
trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint)
{
	if (auto manager = XapiandManager::manager()) {
		manager->replication->trigger_replication({src_endpoint, dst_endpoint, false});
	}
}

#endif
