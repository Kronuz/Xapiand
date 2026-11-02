/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "allocators.h"                          // for allocators::total_allocated
#include "color_tools.hh"                        // for color
#include "database/cleanup.h"                    // for DatabaseCleanup
#include "database/handler.h"                    // for DatabaseHandler, committer
#include "database/pool.h"                       // for DatabasePool
#include "engine/host_asio.h"                    // for engine::AsioHost
#include "database/schemas_lru.h"                // for SchemasLRU
#include "database/utils.h"                      // for RESERVED_TYPE, unsharded_path
#include "database/wal.h"                        // for DatabaseWALWriter
#include "epoch.hh"                              // for epoch::now
#include "error.hh"                             // for error:name, error::description
#include "xapiand_exception.h"                          // for SystemExit, Excep...
#include "hashes.hh"                             // for jump_consistent_hash
#include "io.hh"                                 // for io::*
#include "index_resolver_lru.h"                  // for IndexResolverLRU
#include "length.h"                              // for serialise_length
#include "log.h"                                 // for L_CALL, L_DEBUG
#include "lru.h"                                 // for lru::lru
#include "memory_stats.h"                        // for get_total_ram, get_total_virtual_memor...
#include "metrics.h"                             // for Metrics::metrics
#include "msgpack.h"                             // for MsgPack, object::object
#include "xapiand_namegen.h"                             // for name_generator
#include "nanosleep.h"                           // for nanosleep
#include "net.hh"                                // for inet_ntop
#include "package.h"                             // for Package
#include "reserved/schema.h"                     // for RESERVED_INDEX, RESERVED_TYPE, ...
#include "serialise.h"                           // for KEYWORD_STR
#include "serialise_list.h"                      // for StringList
#include "http_asio.h"                           // Kronuz/http: HttpAsioService (the Asio transport)
#ifdef XAPIAND_TLS
#include "http_asio_tls.h"                       // Kronuz/http: the TLS connection factory
#include "tls.h"                                 // for tls_context
#endif
#include "reactor.h"                             // Kronuz/reactor: reactor::Reactor (the control-plane loop)
#include "reactor_lag.h"                         // Kronuz/reactor: reactor::LagProbe, LagSnapshot
#include "reactor_events.h"                      // Kronuz/reactor: PeriodicTimer, Signal (ev::timer/async replacements)
#include "server/search_service.h"           // for SearchService (the HttpHandler)
#include "compressor_deflate.h"                  // for DeflateCompressData (the deflate coding)
#include "storage.h"                             // for Storage
#include "xapiand_fsyncher.h"                    // for fsyncher (moved out of storage.h)
#include "strict_stox.hh"                        // for strict_stoll
#include "system.hh"                             // for get_open_files_per_proc, get_max_files_per_proc
#include "thread.hh"                             // for ThreadPool
#include "traceback.h"                           // for traceback::{callstacks_snapshot, dump_callstacks}


#define L_MANAGER L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_MANAGER
// #define L_MANAGER L_DARK_CYAN


#define NODE_LABEL "node"
#define CLUSTER_LABEL "cluster"

// XAPIAND_LAG_PROBE=0 turns the loop probes off (to profile or measure without them).
static bool
lag_probe_enabled()
{
	const char* value = std::getenv("XAPIAND_LAG_PROBE");
	return !(value && std::string_view(value) == "0");
}

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

// The manager's Asio control-plane state, kept out of manager.h (which stays Asio-free).
// One reactor, plus a worker that runs the control plane's database work (node setup, the
// dispatched commands) in arrival order until the control plane is made of tasks (D13); the
// loop keeps the state changes and never blocks on a database. The ev::timer/ev::async watchers become a
// PeriodicTimer + three Signals. The signal wake is deliberately NOT a reactor::Signal
// (its send() does asio::post -> heap alloc + lock, which is NOT async-signal-safe): it is
// a self-pipe (sig_pipe), written from the POSIX signal handler with a bare write() -- the
// same async-signal-safe mechanism ev::async uses internally -- and drained on the loop.
struct XapiandManager::Reactors {
	reactor::Reactor reactor;
	asio::thread_pool pool{1};                   // the control plane's database work (D13 removes it)
	engine::AsioHost host;                       // where the index registry's timer-driven tasks run
	reactor::PeriodicTimer try_shutdown_timer;
	reactor::Signal setup_node_async;
	reactor::Signal shutdown_async;

	int sig_pipe[2];                             // [0] read end (watched), [1] write end (signal handler)
	asio::posix::stream_descriptor sig_reader;
	char sig_drain[64];

	Reactors()
		: host(reactor.io().get_executor()),
		  try_shutdown_timer(reactor.io().get_executor()),
		  setup_node_async(reactor.io().get_executor()),
		  shutdown_async(reactor.io().get_executor()),
		  sig_pipe{-1, -1},
		  sig_reader(reactor.io())
	{
		if (::pipe(sig_pipe) == 0) {
			for (int fd : sig_pipe) {
				::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
				::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
			}
			sig_reader.assign(sig_pipe[0]);
		}
	}

	~Reactors() {
		asio::error_code ec;
		sig_reader.close(ec);
		if (sig_pipe[1] != -1) { ::close(sig_pipe[1]); }
	}
};

// The Xapiand search API served over the Asio transport. SearchService is a
// stateless http::HttpHandler (its handle() builds all per-request state locally),
// so one shared instance is safe across every reactor thread. The transport owns the
// offload pool (blocking work runs off the reactor) and the response
// transforms, so no bespoke dispatcher/response wiring survives here.
static SearchService search_service;

// The request/response logging middleware (Kronuz/http-log) wrapping the search
// handler: it renders each request and response to the log with per-method / per-
// status colors and prettified bodies, recovering what Request::to_text /
// Response::to_text did before the decomposition removed them. It forwards the whole
// HttpHandler seam to search_service, so the transport drives it transparently. One
// shared instance, like search_service.
static http_log::AccessLog logged_search_service(search_service, make_http_log_options());

// The response transforms the transport applies to the buffered response: compress to
// the codec the client advertised. Xapiand no longer encodes responses itself; it just
// registers "deflate" (the library ships zstd + gzip) and lets the transport negotiate
// + apply the coding from Accept-Encoding. min_size 0 matches the old "compress
// whenever it helps" (the library still skips a coding that doesn't shrink the body).
static http::CompressionOptions
search_compression()
{
	http::CompressionOptions c;
	c.min_size = 0;
	c.add_coding("deflate", [](std::string_view body) {
		std::string out;
		DeflateCompressData d(body.data(), body.size(), /*gzip=*/false);
		for (auto it = d.begin(); it; ++it) { out.append(*it); }
		return out;
	});
	return c;
}

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


// The most views of one index that are held at once, and the hazard records that back them: a view past it is
// refused (503) before it takes a record. A view is held while a search runs and while it waits demoted for its next
// slice: 16 searches per reactor on one index is the estimate here. The pool workers and the control loop hold one each,
// doubled because a read that runs under a view (a scan's callback, a spy) may take a second view of the same index on
// its thread. Each record is a cache line; strata's chunked registry (X3-1) will let the bytes follow the load.
static std::size_t
readers_per_index()
{
	const auto reactors = static_cast<std::size_t>(std::max<ssize_t>(1, opts.num_http_servers));
	const auto pool = static_cast<std::size_t>(std::max(1.0, opts.processors));
	return reactors * 16 + 2 * (pool + 1);
}


XapiandManager::XapiandManager(std::chrono::steady_clock::time_point process_start_)
	: total_clients(0),
	  http_clients(0),
	  schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size, std::chrono::milliseconds(opts.schema_pool_timeout))),
	  database_pool(std::make_unique<DatabasePool>(opts.database_pool_size, 0, readers_per_index())),
	  wal_writer(std::make_unique<DatabaseWALWriter>("WL{:02}", opts.num_async_wal_writers)),
	  index_settings_resolver(std::make_unique<IndexResolverLRU>(opts.resolver_cache_size, std::chrono::milliseconds(opts.resolver_cache_timeout))),
	  state(State::RESET),
	  node_name(opts.node_name),
	  _shutdown_asap(0),
	  _shutdown_now(0),
	  _process_start(process_start_),
	  _try_shutdown(0),
	  _running_loop(false),
	  _deinited(false),
	  _asap(0),
	  _now(0),
	  _rx(std::make_unique<Reactors>()),
	  atom_sig(0)
{
	database_pool->set_control(&_rx->host);
	_rx->try_shutdown_timer.set_callback([this] { try_shutdown_timer_cb(); });
	_rx->setup_node_async.set_callback([this] { setup_node_async_cb(); });
	_rx->shutdown_async.set_callback([this] { shutdown_async_cb(); });
}


XapiandManager::~XapiandManager() noexcept
{
	try {
		_deinited.store(true, std::memory_order_release);

		join();

		if (log) {
			log->clear();
			log.reset();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


task::task<IndexSettings>
XapiandManager::resolve_index_settings(std::string_view normalized_path, bool writable, const MsgPack* settings, bool reload, bool rebuild, bool clear)
{
	return manager(true)->resolve_index_settings_impl(normalized_path, writable, settings, reload, rebuild, clear);
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
	// Runs in a POSIX signal handler (see main.cc sig_handler) -- everything here MUST be
	// async-signal-safe. atom_sig is an atomic store; the wake is a bare write() of one
	// byte to the self-pipe (both safe). Do NOT use reactor::Signal here: its send() calls
	// asio::post (heap alloc + lock), which is unsafe in a signal handler. The reactor
	// drains the pipe on the loop thread (arm_signal_reader) and runs signal_sig_impl.
	atom_sig = sig;
	if (_rx && _rx->sig_pipe[1] != -1) {
		char byte = 1;
		ssize_t written = ::write(_rx->sig_pipe[1], &byte, 1);
		(void)written;  // EAGAIN (pipe full) is fine: a wake is already pending -- coalesced
	}
}


void
XapiandManager::arm_signal_reader()
{
	if (!_rx) { return; }
	_rx->sig_reader.async_read_some(asio::buffer(_rx->sig_drain),
		[this](const asio::error_code& ec, std::size_t /*n*/) {
			if (ec) { return; }  // pipe closed / read cancelled at shutdown
			signal_sig_impl();
			arm_signal_reader();
		});
}


void
XapiandManager::run_loop()
{
	L_CALL("XapiandManager::run_loop()");

	assert(!is_running_loop());

	_running_loop.store(true, std::memory_order_release);
	arm_signal_reader();
	// Keep the loop alive while idle (no pending I/O) until break_loop() stops it.
	auto guard = asio::make_work_guard(_rx->reactor.io());
	{
		engine::HostBinding binding(&_rx->host);   // the tasks resumed on this thread run on the control host
		if (lag_probe_enabled()) { _rx->reactor.lag().start(); }
		_rx->reactor.run();  // marks this thread reactor::inside_run() for the blocking guard
	}
	_running_loop.store(false, std::memory_order_release);
}


void
XapiandManager::break_loop()
{
	L_CALL("XapiandManager::break_loop()");

	// io_context::stop() is thread-safe and makes a running run() return ASAP.
	_rx->reactor.io().stop();
}


void
XapiandManager::control_offload(std::function<void()> work, std::function<void()> then)
{
	// One worker: the database parts run in arrival order. An exception from work is
	// rethrown on the loop, out of run_loop(), as when it ran there.
	auto& rx = _rx->reactor;
	asio::post(_rx->pool, [&rx, work = std::move(work), then = std::move(then)]() mutable {
		std::exception_ptr error;
		try {
			work();
		} catch (...) {
			error = std::current_exception();
		}
		asio::post(rx.io(), [error, then = std::move(then)] {
			if (error) {
				std::rethrow_exception(error);
			}
			if (then) {
				then();
			}
		});
	});
}


void
XapiandManager::try_shutdown_timer_cb()
{
	L_CALL("XapiandManager::try_shutdown_timer_cb()");

	try_shutdown();
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
			print(DARK_STEEL_BLUE + "Threads:\n{}" + DARK_STEEL_BLUE + "Manager:\n{}" + DARK_STEEL_BLUE + "Databases:\n{}" + DARK_STEEL_BLUE + "Schemas:\n{}", traceback::dump_callstacks(), __repr__(), database_pool->dump_databases(), schemas->dump_schemas());
			break;
	}
}


void
XapiandManager::shutdown(long long asap, long long now, bool async)
{
	L_CALL("XapiandManager::shutdown({}, {})", asap, now);

	if (async) {
		_asap.store(asap, std::memory_order_release);
		_now.store(now, std::memory_order_release);
		_rx->shutdown_async.send();
	} else {
		shutdown_impl(asap, now);
	}
}


void
XapiandManager::shutdown_async_cb()
{
	L_CALL("XapiandManager::shutdown_async_cb()");

	shutdown_impl(_asap.load(std::memory_order_acquire), _now.load(std::memory_order_acquire));
}


void
XapiandManager::shutdown_sig(int sig, bool async)
{
	L_CALL("XapiandManager::shutdown_sig({}, {})", sig, async);

	if (sig) {
		if (sig < 0) {
			// System Exit with error code (-sig)
			atom_sig = sig;
			if (is_running_loop()) {
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
					if (is_running_loop()) {
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

	if (asap) {
		_rx->try_shutdown_timer.set_repeat(5.0);
		_rx->try_shutdown_timer.again();
		L_EV("Configured try shutdown timer (5.0)");

		if (now != 0) {
			// No Worker tree to stop/destroy/detach anymore -- the manager is the sole
			// remaining runner, so shutting down is exactly "break the control-plane loop".
			break_loop();
		}

		// The readiness checks resolve index settings for pending shards, a database read, so
		// they run on the control plane's worker; break_loop() is thread-safe.
		control_offload([this, now] {
			if (!ready_to_end_http()) {
				L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: HTTP is busy...", "Continuing shutdown process!");
			} else {
				L_MANAGER_TIMED(3s, "Is taking too long to start shutting down...", "Starting shutdown process!");
			}

			if (now == 0 && ready_to_end(true)) {
				break_loop();
			}
		});
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
	if (node_name.empty()) {
		node_name = name_generator();
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

	Node::touch_node(*local_node, true);
}


void
XapiandManager::run()
{
	L_CALL("XapiandManager::run()");

	try {
		if (node_name == "~") {
			L_CRIT("Node name {} doesn't match the one saved in the node's directory!", opts.node_name);
			throw SystemExit(EX_CONFIG);
		}

		setup_node();

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
XapiandManager::setup_node_impl()
{
	_rx->setup_node_async.send();
}


void
XapiandManager::setup_node_async_cb()
{
	L_CALL("XapiandManager::setup_node_async_cb(...)");

	L_MANAGER("Setup Node!");

	make_servers();

	// Once all threads have started, get a callstacks snapshot:
	traceback::callstacks_snapshot();

	// The node name is saved on the control plane's worker (a file write); the loop then
	// sets the ready state.
	control_offload([this] {
		node_name = set_node_name(Node::get_local_node()->name());
		auto local_node = Node::get_local_node();
		if (strings::lower(node_name) != local_node->lower_name()) {
			auto local_node_copy = std::make_unique<Node>(*local_node);
			local_node_copy->name(node_name);
			Node::set_local_node(std::shared_ptr<const Node>(local_node_copy.release()));
		}
	}, [this] {
		Metrics::metrics({{NODE_LABEL, node_name}, {CLUSTER_LABEL, opts.cluster_name}});

		set_ready_impl();
	});
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


	// Create and initialize servers.

	auto local_node_addr = inet_ntop(local_node->addr());

	// The search HTTP API, served over the Asio transport: N reactors
	// (num_http_servers) share the listen port via SO_REUSEPORT. A request is a task on the
	// scheduler of the reactor that accepted it. The service binds + starts later, when the
	// node is ready (like http->start).
	{
		std::size_t reactors = opts.num_http_servers > 0 ? static_cast<std::size_t>(opts.num_http_servers) : 1;
		#ifdef XAPIAND_TLS
		if (tls_context) {
			// HTTPS (transport-security D1): http's handshake deadline and caps, the connection
			// bound, and the idle, header, and inactivity timeouts, which plain HTTP leaves off.
			auto failed = [](http::TlsFailure why) {
				switch (why) {  // one call site per reason, so each is rate-limited on its own
					case http::TlsFailure::deadline: L_WARNING_ONCE_PER_MINUTE("TLS handshake past its deadline"); break;
					case http::TlsFailure::connections: L_WARNING_ONCE_PER_MINUTE("TLS connection refused: --tls-max-connections reached"); break;
					case http::TlsFailure::handshakes: L_WARNING_ONCE_PER_MINUTE("TLS connection refused: too many handshakes in progress"); break;
					case http::TlsFailure::version: L_WARNING_ONCE_PER_MINUTE("TLS handshake refused: protocol version"); break;
					case http::TlsFailure::cipher: L_WARNING_ONCE_PER_MINUTE("TLS handshake refused: no shared cipher"); break;
					case http::TlsFailure::certificate: L_WARNING_ONCE_PER_MINUTE("TLS handshake failed: the client refused the certificate"); break;
					case http::TlsFailure::protocol: L_WARNING_ONCE_PER_MINUTE("TLS port received something other than TLS (plain HTTP?)"); break;
					case http::TlsFailure::other: L_WARNING_ONCE_PER_MINUTE("TLS handshake failed"); break;
				}
			};
			const std::size_t bound = opts.tls_max_connections;
			http_service = std::make_unique<http::HttpAsioService>(logged_search_service, reactors, 1000,
				http::tls_connection_factory_timed(tls_context, std::chrono::seconds(5), 64, bound, failed), bound);
			http_service->set_idle_timeout(std::chrono::seconds(75));
			http_service->set_header_timeout(std::chrono::seconds(30));
			http_service->set_inactivity_timeout(std::chrono::seconds(60));
		} else
#endif
		http_service = std::make_unique<http::HttpAsioService>(logged_search_service, reactors, 1000);
		http::AsioBindOptions bind_options;
		bind_options.address = opts.bind_address;
		bind_options.reuse_port = reuse_ports;
		http_service->set_bind_options(bind_options);
		if (lag_probe_enabled()) { http_service->enable_lag_probe(); }
		http_service->enable_compression(search_compression());
		http_service->set_max_body(BUFFERED_BODY_CAP);

		// Each reactor's host: the scheduler its requests' tasks resume through. The loop's thread
		// is bound to it before the loop serves anything.
		for (std::size_t i = 0; i < http_service->reactors(); ++i) {
			auto& io = http_service->reactor(i).io();
			auto& host = http_hosts.emplace_back(std::make_unique<engine::AsioHost>(io.get_executor()));
			asio::post(io, [host = host.get()] { host->bind_loop_thread(); });
		}
	}
	this->http_port = http_port;


	// Setup local node ports.
	auto node_copy = std::make_unique<Node>(*local_node);
	node_copy->http_port = http_port;
	Node::set_local_node(std::shared_ptr<const Node>(node_copy.release()));

	std::string msg("Servers listening on ");
	msg += strings::format("TCP {}:{} (HTTP v1.1)", opts.bind_address, http_port);
	L(-LOG_NOTICE, NOTICE_COL, msg);

	// Setup database cleanup thread.
	database_cleanup = std::make_shared<DatabaseCleanup>();
	database_cleanup->run();

	// Start committers, fsynchers, database updaters.
	committer();
	fsyncher();
}


void
XapiandManager::set_ready_impl()
{
	L_CALL("XapiandManager::set_ready_impl()");

	auto local_node = Node::get_local_node();
	assert(local_node->is_active());

	exchange_state(state.load(), State::READY);

	if (http_service) {
		http_service->start(static_cast<unsigned short>(http_port));
	}

	L(-LOG_NOTICE, SEA_GREEN, "Node {}{}" + SEA_GREEN + " is Ready to Rock!", local_node->col().ansi(), local_node->to_string());
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	if (!database_pool) {
		return;  // already joined!
	}

	// The control plane's worker: the loop has stopped (break_loop), so drop what is queued
	// and wait for the running database part before the database pool finishes.
	_rx->pool.stop();
	_rx->pool.join();

	// The index registry's tasks that timers started (the debounced commit, an idle close) run on the control
	// loop, which has stopped: from now on they run on the thread that starts them, and the ones already
	// posted run here, on this thread, until they end.
	database_pool->set_control(nullptr);
	{
		engine::HostBinding binding(&_rx->host);   // the tasks the drain resumes find their host, as they did on the loop
		auto& io = _rx->reactor.io();
		const auto limit = std::chrono::steady_clock::now() + 60s;
		while (database_pool->detached() != 0 && std::chrono::steady_clock::now() < limit) {
			io.restart();   // a loop that ran out of work is stopped: a chore that finishes later posts to a stopped one
			io.run_for(10ms);
		}
		if (database_pool->detached() != 0) {
			L_WARNING("{} tasks of the index registry did not end", database_pool->detached());
		}
	}

	// This method should finish and wait for all objects and threads to finish
	// their work. Order of waiting for objects here matters!
	L_MANAGER(STEEL_BLUE + "Manager:\n{}Databases:\n{}Nodes:\n{}", __repr__(), database_pool->dump_databases(), Node::dump_nodes());

	////////////////////////////////////////////////////////////////////
	if (http_service) {
		L_MANAGER("Stopping the HTTP service!");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP service...", "HTTP service finished!");
		http_service->stop();  // aborts the requests, then stops and joins the reactors
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

	http_service.reset();
	http_hosts.clear();

	database_pool.reset();

	wal_writer.reset();


	database_cleanup.reset();

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
XapiandManager::ready_to_end(bool notify)
{
	return ready_to_end_http(notify);
}


task::task<IndexSettings>
XapiandManager::resolve_index_settings_impl(std::string_view normalized_path, bool writable, const MsgPack* settings, bool reload, bool rebuild, bool clear)
{
	L_CALL("XapiandManager::resolve_index_settings_impl()");

	return index_settings_resolver->resolve_index_settings(normalized_path, writable, settings, reload, rebuild, clear);
}


task::task<Endpoints>
XapiandManager::resolve_index_endpoints_impl(const Endpoint& endpoint, bool writable, const MsgPack* settings)
{
	L_CALL("XapiandManager::resolve_index_endpoints_impl()");

	return index_settings_resolver->resolve_index_endpoints(endpoint, writable, settings);
}


void
XapiandManager::invalidate_settings_impl(const std::string& uri)
{
	L_CALL("XapiandManager::invalidate_settings_impl()");

	index_settings_resolver->invalidate_settings(uri);
}


std::string
XapiandManager::server_metrics_impl()
{
	L_CALL("XapiandManager::server_metrics_impl()");

	auto& metrics = Metrics::metrics();

	metrics.xapiand_uptime.Set(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _process_start).count());

	// http client tasks: the bespoke http_client_pool retired with HttpClient (Leg 2
	// stage 3c); the http::Dispatcher that replaced it is not yet observable from here.
	metrics.xapiand_http_clients_running.Set(0);
	metrics.xapiand_http_clients_queue_size.Set(0);
	metrics.xapiand_http_clients_pool_size.Set(0);
	metrics.xapiand_http_clients_capacity.Set(0);


	// servers_threads: the Asio transport owns its reactors internally; expose the
	// reactor count as the running/pool size (queue/capacity are not modelled here).
	{
		auto reactors = http_service ? static_cast<double>(http_service->reactors()) : 0.0;
		metrics.xapiand_servers_running.Set(reactors);
		metrics.xapiand_servers_queue_size.Set(0);
		metrics.xapiand_servers_pool_size.Set(reactors);
		metrics.xapiand_servers_capacity.Set(reactors);
	}

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

	return metrics.serialise() + reactor_lag_metrics();
}


// xapiand_reactor_lag_seconds: one histogram per loop ("http0", ..., "control"), fed by
// reactor::LagProbe. Bucket edges are the probe's (log2 microseconds, 4 sub-buckets each), so a
// percentile reads as the upper edge of its bucket; `le` is that edge in seconds.
std::string
XapiandManager::reactor_lag_metrics() const
{
	std::vector<std::pair<std::string, reactor::LagSnapshot>> loops;
	if (http_service) {
		for (std::size_t i = 0; i < http_service->reactors(); ++i) {
			loops.emplace_back("http" + std::to_string(i), http_service->reactor(i).lag().snapshot());
		}
	}
	loops.emplace_back("control", _rx->reactor.lag().snapshot());

	const auto node = Node::get_local_node();
	const std::string labels = strings::format("cluster=\"{}\",node=\"{}\"", opts.cluster_name, node ? node->name() : std::string());
	std::string out = "# HELP xapiand_reactor_lag_seconds How late a reactor loop ran its probe timer\n"
		"# TYPE xapiand_reactor_lag_seconds histogram\n";
	for (const auto& [loop, snap] : loops) {
		std::uint64_t running = 0;
		for (std::size_t i = 0; i < reactor::LagSnapshot::buckets; ++i) {
			running += snap.count[i];
			if (i + 1 == reactor::LagSnapshot::buckets || snap.count[i] != 0 || (i < 120 && (i % 4) == 3)) {
				const auto edge = i + 1 == reactor::LagSnapshot::buckets ? std::string("+Inf") : strings::format("{}", static_cast<double>(reactor::LagSnapshot::upper_us(i)) / 1e6);
				out += strings::format("xapiand_reactor_lag_seconds_bucket{{{},loop=\"{}\",le=\"{}\"}} {}\n", labels, loop, edge, running);
			}
		}
		out += strings::format("xapiand_reactor_lag_seconds_sum{{{},loop=\"{}\"}} {}\n", labels, loop, static_cast<double>(snap.sum_us) / 1e6);
		out += strings::format("xapiand_reactor_lag_seconds_count{{{},loop=\"{}\"}} {}\n", labels, loop, snap.wakes);
		out += strings::format("xapiand_reactor_lag_max_seconds{{{},loop=\"{}\"}} {}\n", labels, loop, static_cast<double>(snap.max_us) / 1e6);
	}
	return out;
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
	return strings::format(STEEL_BLUE + "<XapiandManager ({}) {{cnt:{}}}{}>",
		enum_name(state.load()),
		static_cast<long>(_manager.use_count()),
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE);
}


