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
#include <cctype>                                // for isspace
#include <chrono>                                // for std::chrono, std::chrono::system_clock
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
#include <unistd.h>                              // for ssize_t, getpid
#include <utility>                               // for std::move
#include <vector>                                // for std::vector

#if defined(XAPIAND_V8)
#include <v8-version.h>                          // for V8_MAJOR_VERSION, V8_MINOR_VERSION
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include <chaiscript/chaiscript_defines.hpp>     // for chaiscript::Build_Info
#endif

#include "allocator.h"                           // for allocator::total_allocated
#include "cassert.h"                             // for ASSERT
#include "color_tools.hh"                        // for color
#include "database_cleanup.h"                    // for DatabaseCleanup
#include "database_handler.h"                    // for DatabaseHandler, DocPreparer, DocIndexer, committer
#include "database_pool.h"                       // for DatabasePool
#include "database_utils.h"                      // for RESERVED_TYPE
#include "database_wal.h"                        // for DatabaseWALWriter
#include "epoch.hh"                              // for epoch::now
#include "error.hh"                              // for error:name, error::description
#include "ev/ev++.h"                             // for ev::async, ev::loop_ref
#include "exception.h"                           // for SystemExit, Excep...
#include "hashes.hh"                             // for jump_consistent_hash
#include "ignore_unused.h"                       // for ignore_unused
#include "io.hh"                                 // for io::*
#include "length.h"                              // for serialise_length
#include "log.h"                                 // for L_CALL, L_DEBUG
#include "lru.h"                                 // for LRU
#include "memory_stats.h"                        // for get_total_ram, get_total_virtual_memor...
#include "metrics.h"                             // for Metrics::metrics
#include "msgpack.h"                             // for MsgPack, object::object
#include "namegen.h"                             // for name_generator
#include "net.hh"                                // for inet_ntop
#include "opts.h"                                // for opts::*
#include "package.h"                             // for Package
#include "readable_revents.hh"                   // for readable_revents
#include "schemas_lru.h"                         // for SchemasLRU
#include "serialise.h"                           // for KEYWORD_STR
#include "server/http.h"                         // for Http
#include "server/http_client.h"                  // for HttpClient
#include "server/http_server.h"                  // for HttpServer
#include "storage.h"                             // for Storage
#include "system.hh"                             // for get_open_files_per_proc, get_max_files_per_proc

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
	} \
}

#define L_MANAGER_TIMED(delay, format_timeout, format_done, ...) { \
	if (log) { \
		log->clear(); \
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
	  _total_clients(0),
	  _http_clients(0),
	  _remote_clients(0),
	  _replication_clients(0),
	  _schemas(std::make_unique<SchemasLRU>(opts.dbpool_size * 3)),
	  _database_pool(std::make_unique<DatabasePool>(opts.dbpool_size, opts.max_databases)),
	  _wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  _http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  _http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_servers)),
#ifdef XAPIAND_CLUSTERING
	  _remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  _remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_servers)),
	  _replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  _replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_servers)),
#endif
	  _doc_preparer_pool(std::make_unique<ThreadPool<std::unique_ptr<DocPreparer>, ThreadPolicyType::doc_preparers>>("DP{:02}", opts.num_doc_preparers)),
	  _doc_indexer_pool(std::make_unique<ThreadPool<std::shared_ptr<DocIndexer>, ThreadPolicyType::doc_indexers>>("DI{:02}", opts.num_doc_indexers)),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _state(State::RESET),
	  _node_name(opts.node_name),
	  _new_cluster(0),
	  _process_start(std::chrono::system_clock::now()),
	  atom_sig(0)
{
	std::vector<std::string> values({
		std::to_string(opts.num_http_clients) +( (opts.num_http_clients == 1) ? " http thread" : " http threads"),
#ifdef XAPIAND_CLUSTERING
		std::to_string(opts.num_remote_clients) +( (opts.num_remote_clients == 1) ? " remote protocol thread" : " remote protocol threads"),
		std::to_string(opts.num_replication_clients) +( (opts.num_replication_clients == 1) ? " replication protocol thread" : " replication protocol threads"),
#endif
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_, std::chrono::time_point<std::chrono::system_clock> process_start_)
	: Worker(std::weak_ptr<Worker>{}, ev_loop_, ev_flags_),
	  _total_clients(0),
	  _http_clients(0),
	  _remote_clients(0),
	  _replication_clients(0),
	  _schemas(std::make_unique<SchemasLRU>(opts.dbpool_size * 3)),
	  _database_pool(std::make_unique<DatabasePool>(opts.dbpool_size, opts.max_databases)),
	  _wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  _http_client_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpClient>, ThreadPolicyType::http_clients>>("CH{:02}", opts.num_http_clients)),
	  _http_server_pool(std::make_unique<ThreadPool<std::shared_ptr<HttpServer>, ThreadPolicyType::http_servers>>("SH{:02}", opts.num_servers)),
#ifdef XAPIAND_CLUSTERING
	  _remote_client_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolClient>, ThreadPolicyType::binary_clients>>("CB{:02}", opts.num_remote_clients)),
	  _remote_server_pool(std::make_unique<ThreadPool<std::shared_ptr<RemoteProtocolServer>, ThreadPolicyType::binary_servers>>("SB{:02}", opts.num_servers)),
	  _replication_client_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolClient>, ThreadPolicyType::binary_clients>>("CR{:02}", opts.num_replication_clients)),
	  _replication_server_pool(std::make_unique<ThreadPool<std::shared_ptr<ReplicationProtocolServer>, ThreadPolicyType::binary_servers>>("SR{:02}", opts.num_servers)),
#endif
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
#endif
}


XapiandManager::~XapiandManager() noexcept
{
	try {
		Worker::deinit();

		join();

		if (log) {
			log->clear();
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
XapiandManager::save_node_name(std::string_view node_name)
{
	L_CALL("XapiandManager::save_node_name({})", node_name);

	int fd = io::open("node", O_WRONLY | O_CREAT, 0644);
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
XapiandManager::signal_sig_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("XapiandManager::signal_sig_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

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
			print(STEEL_BLUE + "Workers:\n{}Databases:\n{}Nodes:\n{}", dump_tree(), _database_pool->dump_databases(), Node::dump_nodes());
			break;
	}
}


void
XapiandManager::shutdown_sig(int sig)
{
	L_CALL("XapiandManager::shutdown_sig({})", sig);

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

	shutdown(_shutdown_asap, _shutdown_now, is_running_loop());
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

	// Set the id in local node.
	auto local_node = Node::local_node();
	auto node_copy = std::make_unique<Node>(*local_node);

	// Setup node from node database directory
	std::string node_name(load_node_name());
	if (!node_name.empty()) {
		if (!_node_name.empty() && string::lower(_node_name) != string::lower(node_name)) {
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

	// Set addr in local node
	auto address = host_address();
	L(-LOG_NOTICE, NOTICE_COL, "Node IP address is {} on interface {}, running with pid:{}", inet_ntop(address.first), address.second, getpid());
	node_copy->addr(address.first);

	local_node = std::shared_ptr<const Node>(node_copy.release());
	local_node = Node::local_node(local_node);

	if (opts.solo) {
		Node::leader_node(local_node);
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

		shutdown_sig(0);
		join();

	} catch (const SystemExit& exc) {
		shutdown_sig(0);
		join();
		sig_exit(-exc.code);

	} catch (const BaseException& exc) {
		L_CRIT("Exception: {}", *exc.get_context() ? exc.get_context() : "Unkown BaseException!");
		shutdown_sig(0);
		join();
		sig_exit(-EX_SOFTWARE);

	} catch (const Xapian::Error& exc) {
		L_CRIT("Exception: {}", exc.get_description());
		shutdown_sig(0);
		join();
		sig_exit(-EX_SOFTWARE);

	} catch (const std::exception& exc) {
		L_CRIT("Exception: {}", *exc.what() ? exc.what() : "Unkown std::exception!");
		shutdown_sig(0);
		join();
		sig_exit(-EX_SOFTWARE);

	} catch (...) {
		L_CRIT("Exception: Unknown!");
		shutdown_sig(0);
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
		auto msg = string::format("Discovering cluster {} by listening on ", opts.cluster_name);

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

	auto leader_node = Node::leader_node();
	auto local_node = Node::local_node();
	auto is_leader = Node::is_superset(local_node, leader_node);

	_new_cluster = 0;
	Endpoint cluster_endpoint{".cluster/", leader_node.get()};
	try {
		bool found = false;
		if (is_leader) {
			DatabaseHandler db_handler(Endpoints{cluster_endpoint});
			if (db_handler.get_metadata(std::string_view(RESERVED_SCHEMA)).empty()) {
				THROW(NotFoundError);
			}
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
					_discovery->raft_add_command(serialise_length(did) + serialise_string(obj["name"].as_str()));
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
		db_handler.set_metadata(std::string_view(RESERVED_SCHEMA), Schema::get_initial_schema()->serialise());
		auto did = db_handler.index(local_node->lower_name(), false, {
			{ RESERVED_INDEX, "field_all" },
			{ ID_FIELD_NAME,  { { RESERVED_TYPE,  KEYWORD_STR } } },
			{ "name",         { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_VALUE, local_node->name() } } },
		}, false, msgpack_type).first;
		_new_cluster = 1;
		#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			_discovery->raft_add_command(serialise_length(did) + serialise_string(local_node->name()));
		}
		#else
			ignore_unused(did);
		#endif
	}

	// Set node as ready!
	_node_name = set_node_name(local_node->name());
	if (string::lower(_node_name) != local_node->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node);
		local_node_copy->name(_node_name);
		local_node = Node::local_node(std::shared_ptr<const Node>(local_node_copy.release()));
	}

	Metrics::metrics({{NODE_LABEL, _node_name}, {CLUSTER_LABEL, opts.cluster_name}});

	#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		// Replicate database from the leader
		if (!is_leader) {
			ASSERT(!cluster_endpoint.is_local());
			L_INFO("Synchronizing cluster database from {}{}" + INFO_COL + "...", leader_node->col().ansi(), leader_node->name());
			_new_cluster = 2;
			_replication->trigger_replication({cluster_endpoint, Endpoint{".cluster/"}, true});
			_replication->trigger_replication({cluster_endpoint, Endpoint{".index/"}, true});
		} else {
			load_nodes();
			set_cluster_database_ready_impl();
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

	auto local_node = Node::local_node();
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

	// Create and initialize servers.

	auto local_node_addr = inet_ntop(local_node->addr());

	_http = Worker::make_shared<Http>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? 0 : http_tries);

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		_remote = Worker::make_shared<RemoteProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? 0 : remote_tries);
        _replication = Worker::make_shared<ReplicationProtocol>(shared_from_this(), ev_loop, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? 0 : replication_tries);
	}
#endif

	for (ssize_t i = 0; i < opts.num_servers; ++i) {
		auto _http_server = Worker::make_shared<HttpServer>(_http, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), http_port, reuse_ports ? http_tries : 0);
		if (_http_server->addr.sin_family) {
			_http->addr = _http_server->addr;
		}
		_http_server_pool->enqueue(std::move(_http_server));

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			auto _remote_server = Worker::make_shared<RemoteProtocolServer>(_remote, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), remote_port, reuse_ports ? remote_tries : 0);
			if (_remote_server->addr.sin_family) {
				_remote->addr = _remote_server->addr;
			}
			_remote_server_pool->enqueue(std::move(_remote_server));

			auto _replication_server = Worker::make_shared<ReplicationProtocolServer>(_replication, nullptr, ev_flags, opts.bind_address.empty() ? nullptr : opts.bind_address.c_str(), replication_port, reuse_ports ? replication_tries : 0);
			if (_replication_server->addr.sin_family) {
				_replication->addr = _replication_server->addr;
			}
			_replication_server_pool->enqueue(std::move(_replication_server));
		}
#endif
	}

	// Setup local node ports.
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->http_port = ntohs(_http->addr.sin_port);
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		node_copy->remote_port = ntohs(_remote->addr.sin_port);
		node_copy->replication_port = ntohs(_replication->addr.sin_port);
	}
#endif
	Node::local_node(std::shared_ptr<const Node>(node_copy.release()));

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

	// Now print information about servers and workers.
	std::vector<std::string> values({
		std::to_string(opts.num_servers) + ((opts.num_servers == 1) ? " server" : " servers"),
		std::to_string(opts.num_http_clients) +( (opts.num_http_clients == 1) ? " http thread" : " http threads"),
#ifdef XAPIAND_CLUSTERING
		std::to_string(opts.num_remote_clients) +( (opts.num_remote_clients == 1) ? " remote protocol thread" : " remote protocol threads"),
		std::to_string(opts.num_replication_clients) +( (opts.num_replication_clients == 1) ? " replication protocol thread" : " replication protocol threads"),
#endif
#if XAPIAND_DATABASE_WAL
		std::to_string(opts.num_async_wal_writers) + ((opts.num_async_wal_writers == 1) ? " async wal writer" : " async wal writers"),
#endif
		std::to_string(opts.num_committers) + ((opts.num_committers == 1) ? " autocommitter" : " autocommitters"),
		std::to_string(opts.num_fsynchers) + ((opts.num_fsynchers == 1) ? " fsyncher" : " fsynchers"),
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));
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

	exchange_state(_state.load(), State::READY);

	_http->start();

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		_remote->start();
		_replication->start();
		_discovery->cluster_enter();
	}
#endif

	auto local_node = Node::local_node();
	if (opts.solo) {
		switch (_new_cluster) {
			case 0:
				L_NOTICE("{}{}" + NOTICE_COL + " using solo cluster {}", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 1:
				L_NOTICE("{}{}" + NOTICE_COL + " using new solo cluster {}", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
		}
	} else {
		switch (_new_cluster) {
			case 0:
				L_NOTICE("{}{}" + NOTICE_COL + " joined cluster {} (it is now online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 1:
				L_NOTICE("{}{}" + NOTICE_COL + " joined new cluster {} (it is now online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
				break;
			case 2:
				L_NOTICE("{}{}" + NOTICE_COL + " joined cluster {} (it was already online!)", local_node->col().ansi(), local_node->name(), opts.cluster_name);
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
#endif
	committer_obj.reset();
	db_updater_obj.reset();
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

	L_INFO("Joining cluster {}...", opts.cluster_name);
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
XapiandManager::new_leader_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("XapiandManager::new_leader_async_cb(<watcher>, {:#x} ({}))", revents, readable_revents(revents));

	ignore_unused(revents);

	auto leader_node = Node::leader_node();
	L_INFO("New leader of cluster {} is {}{}", opts.cluster_name, leader_node->col().ansi(), leader_node->name());

	if (_state == State::READY) {
		if (leader_node->is_local()) {
			try {
				// If we get promoted to leader, we immediately try to load the nodes.
				load_nodes();
			} catch (...) {
				L_EXC("ERROR: Cannot load local nodes!");
			}
		}
	}
}


void
XapiandManager::load_nodes()
{
	L_CALL("XapiandManager::load_nodes()");

	// See if our local database has all nodes currently commited.
	// If any is missing, it gets added.

	Endpoint cluster_endpoint{".cluster/"};
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
				L_WARNING("Adding missing node: [{}] {}", node->idx, node->name());
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

#endif


std::vector<std::shared_ptr<const Node>>
XapiandManager::resolve_index_nodes_impl(const std::string& normalized_slashed_path)
{
	L_CALL("XapiandManager::resolve_index_nodes_impl({})", repr(normalized_slashed_path));

	std::vector<std::shared_ptr<const Node>> nodes;

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		DatabaseHandler db_handler;

		MsgPack obj;

		static std::mutex resolve_index_lru_mtx;
		static lru::LRU<std::string, MsgPack> resolve_index_lru(1000);

		std::unique_lock<std::mutex> lk(resolve_index_lru_mtx);
		auto it = resolve_index_lru.find(normalized_slashed_path);
		if (it != resolve_index_lru.end()) {
			obj = it->second;
			lk.unlock();
		} else {
			lk.unlock();
			try {
				db_handler.reset(Endpoints{Endpoint{".index/"}});
				obj = db_handler.get_document(normalized_slashed_path).get_obj();
			} catch (const NotFoundError&) {}
			lk.lock();
			resolve_index_lru.insert(std::make_pair(normalized_slashed_path, obj));
			lk.unlock();
		}

		if (obj.empty()) {
			auto indexed_nodes = Node::indexed_nodes();
			if (indexed_nodes) {
				MsgPack replicas(MsgPack::Type::ARRAY);
				size_t consistent_hash = jump_consistent_hash(normalized_slashed_path, indexed_nodes);
				for (size_t i = std::min(opts.num_replicas, indexed_nodes); i; --i) {
					auto idx = consistent_hash + 1;
					auto node = Node::get_node(idx);
					ASSERT(node);
					nodes.push_back(std::move(node));
					consistent_hash = idx % indexed_nodes;
					replicas.append(idx);
				}
				ASSERT(!replicas.empty());
				lk.lock();
				resolve_index_lru.insert(std::make_pair(normalized_slashed_path, obj));
				lk.unlock();
				auto leader_node = Node::leader_node();
				Endpoint cluster_endpoint{".index/", leader_node.get()};
				db_handler.reset(Endpoints{cluster_endpoint}, DB_WRITABLE | DB_CREATE_OR_OPEN);
				obj = {
					{ RESERVED_INDEX, "none" },
					{ "replicas",         { { RESERVED_TYPE,  "array/positive" }, { RESERVED_VALUE, std::move(replicas) } } },
				};
				db_handler.index(normalized_slashed_path, false, obj, false, msgpack_type);
			}
		} else {
			const auto& replicas = obj["replicas"];
			auto itv = replicas.find(RESERVED_VALUE);
			for (const auto& idx : itv != replicas.end() ? itv.value() : replicas) {
				auto node = Node::get_node(idx.as_u64());
				nodes.push_back(std::move(node));
			}
		}
	}
	else
#else
	ignore_unused(normalized_slashed_path);
#endif
	{
		nodes.push_back(Node::local_node());
	}

	return nodes;
}


Endpoint
XapiandManager::resolve_index_endpoint_impl(const Endpoint& endpoint, bool master)
{
	L_CALL("XapiandManager::resolve_index_endpoint_impl({}, {})", repr(endpoint.to_string()), master ? "true" : "false");

	for (const auto& node : resolve_index_nodes_impl(endpoint.path)) {
		if (Node::is_active(node)) {
			L_MANAGER("Active node used (of {} nodes) {}", Node::indexed_nodes, node ? node->__repr__() : "null");
			return {endpoint, node.get()};
		}
		L_MANAGER("Inactive node ignored (of {} nodes) {}", Node::indexed_nodes, node ? node->__repr__() : "null");
		if (master) {
			break;
		}
	}
	THROW(CheckoutErrorEndpointNotAvailable, "Endpoint not available!");
}


std::string
XapiandManager::server_metrics_impl()
{
	L_CALL("XapiandManager::server_metrics_impl()");

	auto& metrics = Metrics::metrics();

	metrics.xapiand_uptime.Set(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _process_start).count());

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
	metrics.xapiand_tracked_memory_bytes.Set(allocator::total_allocated());
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
	ASSERT(_manager);
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
	return string::format("<XapiandManager ({}) {{cnt:{}}}{}{}{}>",
		StateNames(_state),
		use_count(),
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "");
}


#ifdef XAPIAND_CLUSTERING
void
trigger_replication_trigger(Endpoint src_endpoint, Endpoint dst_endpoint)
{
	XapiandManager::replication()->trigger_replication({src_endpoint, dst_endpoint, false});
}
#endif
