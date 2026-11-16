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
#include "database/handler.h"                    // for DatabaseHandler, committer_commit
#include "database/pool.h"                       // for DatabasePool
#include "engine/host_asio.h"                    // for engine::AsioHost
#include "engine/sync.h"                         // for engine::sync_wait
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
#include "task_asio.hh"                           // for task::await_asio
#include "reactor_events.h"                      // Kronuz/reactor: PeriodicTimer, Signal (ev::timer/async replacements)
#include "server/search_service.h"           // for SearchService (the HttpHandler)
#include "control_plane.h"                       // for control::Tasks, Periodic, Debounce
#include "compressor_deflate.h"                  // for DeflateCompressData (the deflate coding)
#include "server/mime_types.h"                   // for preload_mime_types
#include "shutdown_wait.h"                       // for shutdown_wait::wait
#include "stopper.h"                             // for preloadStopWords
#include "storage.h"                             // for Storage
#include "xapiand_fsyncher.h"                    // for fsyncher (moved out of storage.h)
#include "strict_stox.hh"                        // for strict_stoll
#include "system.hh"                             // for get_open_files_per_proc, get_max_files_per_proc
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

// How long the reactors let requests finish after they are asked to stop, before they close the sockets that
// stalled peers still hold.
static constexpr auto SHUTDOWN_GRACE = 5s;

// What the HTTP service's stop leaves to the rest of the shutdown: the registry's close and the responses.
static constexpr auto SHUTDOWN_RESERVE = 5s;
// The shortest grace, when the budget is nearly spent: stalled peers are cut quickly.
static constexpr auto SHUTDOWN_GRACE_MIN = 250ms;

// The longest the shutdown waits for the writes that were admitted (C14). A read ends at its next checkpoint and a
// stalled body is closed after the grace; a write queued on an index waits for its turn for DB_TIMEOUT, and one that
// holds the turn is finishing. So the longer of the two, and a few seconds to write the responses.
std::chrono::milliseconds
XapiandManager::default_shutdown_write_bound()
{
	return std::max(std::chrono::milliseconds(static_cast<long long>(DB_TIMEOUT * 1000)), std::chrono::duration_cast<std::chrono::milliseconds>(SHUTDOWN_GRACE)) + SHUTDOWN_RESERVE;
}

// The reactors' stop waits one grace, forces the connections of what is left, and waits one more (2 x grace). The
// grace is sized so that the two end before the shutdown's one budget does, leaving the reserve for the rest: an
// admitted write that waits for its turn is let finish while D16's time remains (docs/design/phase7c.md D16).
std::chrono::milliseconds
XapiandManager::http_shutdown_grace(std::chrono::milliseconds left)
{
	return std::max(std::chrono::duration_cast<std::chrono::milliseconds>(SHUTDOWN_GRACE_MIN), (left - SHUTDOWN_RESERVE) / 2);
}

// The manager's Asio control-plane state, kept out of manager.h (which stays Asio-free).
// One reactor, the control loop, and everything that runs on it as a task on its host (D13): the setup, the
// shutdown checks, the signal reader, the housekeeping and the debounced autocommit. Nothing there waits;
// a task that has to wait awaits a timer, an event or a chore. The ev::timer/ev::async watchers become a
// PeriodicTimer + two Signals. The signal wake is deliberately NOT a reactor::Signal
// (its send() does asio::post -> heap alloc + lock, which is NOT async-signal-safe): it is
// a self-pipe (sig_pipe), written from the POSIX signal handler with a bare write() -- the
// same async-signal-safe mechanism ev::async uses internally -- and read by a task on the loop.
struct XapiandManager::Reactors {
	reactor::Reactor reactor;
	engine::AsioHost host;                       // where the control plane's tasks run: the reactor's scheduler
	control::Tasks tasks;                        // the control tasks that have not ended
	std::unique_ptr<control::Periodic> housekeeping;
	std::unique_ptr<control::Debounce<Endpoint>> autocommits;
	reactor::PeriodicTimer try_shutdown_timer;
	reactor::Signal setup_node_async;
	reactor::Signal shutdown_async;

	int sig_pipe[2];                             // [0] read end (watched), [1] write end (signal handler)
	asio::posix::stream_descriptor sig_reader;
	char sig_drain[64];

	Reactors()
		: host(reactor.scheduler()),
		  tasks(host),
		  try_shutdown_timer(reactor.io().get_executor()),
		  setup_node_async(reactor.io().get_executor()),
		  shutdown_async(reactor.io().get_executor()),
		  sig_pipe{-1, -1},
		  sig_reader(reactor.io())
	{
		tasks.failed = [](const std::exception& error) { L_WARNING("A control task failed: {}", error.what()); };
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
// so one shared instance is safe across every reactor thread. A request is a task on the
// loop that accepted it; the transport owns the response transforms, so no bespoke
// dispatcher/response wiring survives here.
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
	: _rx(std::make_unique<Reactors>()),
	  total_clients(0),
	  http_clients(0),
	  schemas(std::make_unique<SchemasLRU>(opts.schema_pool_size, std::chrono::milliseconds(opts.schema_pool_timeout))),
	  database_pool(std::make_unique<DatabasePool>(_rx->host, opts.database_pool_size, 0, readers_per_index())),
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
	  atom_sig(0)
{
	database_pool->set_control(&_rx->host);
	_rx->autocommits = std::make_unique<control::Debounce<Endpoint>>(_rx->host,
		control::Debounce<Endpoint>::Timing{
			std::chrono::milliseconds(opts.committer_debounce_timeout),
			std::chrono::milliseconds(opts.committer_debounce_busy_timeout),
			std::chrono::milliseconds(opts.committer_debounce_min_force_timeout),
			std::chrono::milliseconds(opts.committer_debounce_max_force_timeout)},
		[](const Endpoint& endpoint) { committer_commit(endpoint); });
	_rx->autocommits->tasks().failed = _rx->tasks.failed;
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
	if (sig == SIGINT || sig == SIGTERM) {
		shutdown_signals.fetch_add(1);
	}
	if (_rx && _rx->sig_pipe[1] != -1) {
		char byte = 1;
		ssize_t written = ::write(_rx->sig_pipe[1], &byte, 1);
		(void)written;  // EAGAIN (pipe full) is fine: a wake is already pending -- coalesced
	}
}


// Reads the self-pipe the signal handler writes and runs the signal on the loop. Ends when the read is cancelled.
task::task<void>
XapiandManager::signal_loop()
{
	for (;;) {
		auto [ec, n] = co_await task::await_asio<void(asio::error_code, std::size_t)>([this](auto handler) {
			_rx->sig_reader.async_read_some(asio::buffer(_rx->sig_drain), std::move(handler));
		});
		(void)n;
		if (ec) {
			co_return;   // the pipe closed, or the read was cancelled at shutdown
		}
		// A signal that fails to run is logged and the next one is still read: the loop is how a node is told to stop.
		try {
			signal_sig_impl();
		} catch (const std::exception& error) {
			L_WARNING("Handling signal {} failed: {}", atom_sig.load(), error.what());
		}
	}
}


// The idle work of the schema cache, once a minute; the index catalog's sweep closes idle indexes on its own
// (docs/design/x-pool.md D8). Nothing here waits.
void
XapiandManager::housekeeping_tick()
{
	if (schemas) {
		schemas->cleanup();
	}
}


std::uint64_t
XapiandManager::housekeeping_ticks() const
{
	return _rx->housekeeping ? _rx->housekeeping->ticks() : 0;
}


void
XapiandManager::spawn_control(task::task<void> t)
{
	_rx->tasks.spawn(std::move(t));
}


reactor::LagProbe&
XapiandManager::control_lag()
{
	return _rx->reactor.lag();
}


bool
XapiandManager::schedule_autocommit(const Endpoint& endpoint)
{
	return _manager && _manager->_rx->autocommits->touch(endpoint);
}


void
XapiandManager::run_loop()
{
	L_CALL("XapiandManager::run_loop()");

	assert(!is_running_loop());

	_running_loop.store(true, std::memory_order_release);
	_rx->tasks.spawn(signal_loop());
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

		if (_running_loop.load(std::memory_order_acquire)) {
			_rx->tasks.spawn(shutdown_checks(now));   // on the control loop
		} else {
			engine::sync_wait(shutdown_checks(now));   // the loop has ended: this is the main thread
		}
	}
}


// A control task that cannot go on leaves the node half-set-up or half-shut-down: the process ends with `code`,
// through the path a negative signal takes (the loop breaks and run() exits with it). A task that is only
// maintenance (the housekeeping, an autocommit, a signal that fails to run) is logged by Tasks::failed instead.
void
XapiandManager::control_task_failed(const char* what, int code)
{
	L_CRIT("{} failed: ending the process with status {}", what, code);
	shutdown_sig(-code, true);
}


task::task<void>
XapiandManager::shutdown_checks(long long now)
{
	try {
		if (!ready_to_end_http()) {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down: HTTP is busy...", "Continuing shutdown process!");
		} else {
			L_MANAGER_TIMED(3s, "Is taking too long to start shutting down...", "Starting shutdown process!");
		}

		if (now == 0 && ready_to_end(true)) {
			break_loop();
		}
	} catch (const SystemExit& exit) {
		control_task_failed("The shutdown checks", exit.code);
	} catch (...) {
		L_EXC("The shutdown checks");
		control_task_failed("The shutdown checks", EX_SOFTWARE);
	}
	co_return;
}


void
XapiandManager::init()
{
	L_CALL("XapiandManager::init()");

	bool snooping = (
		!opts.dump_documents.empty() ||
		!opts.restore_documents.empty()
	);

	// The files requests read are read now, before any listener opens (D1).
	preloadStopWords();
	preload_mime_types();

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

		// The node's name is read and saved here, on the main thread, before the loop and its listeners exist.
		node_name = set_node_name(Node::get_local_node()->name());
		if (auto local_node = Node::get_local_node(); strings::lower(node_name) != local_node->lower_name()) {
			auto local_node_copy = std::make_unique<Node>(*local_node);
			local_node_copy->name(node_name);
			Node::set_local_node(std::shared_ptr<const Node>(local_node_copy.release()));
		}

		_rx->housekeeping = std::make_unique<control::Periodic>(_rx->tasks, housekeeping_period, [this] { housekeeping_tick(); });

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

	_rx->tasks.spawn(setup_node_task());
}


// Node setup, on the control loop: the servers, then the ready state, which opens the listeners.
// A failure (a port that is taken, a TLS file that cannot be read) ends the process.
task::task<void>
XapiandManager::setup_node_task()
{
	try {
		make_servers();

		// Once all threads have started, get a callstacks snapshot:
		traceback::callstacks_snapshot();

		Metrics::metrics({{NODE_LABEL, node_name}, {CLUSTER_LABEL, opts.cluster_name}});

		set_ready_impl();
	} catch (const SystemExit& exit) {
		control_task_failed("The node setup", exit.code);
	} catch (...) {
		L_EXC("The node setup");
		control_task_failed("The node setup", EX_SOFTWARE);
	}
	co_return;
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
				http::tls_connection_factory(tls_context, std::chrono::seconds(5), 64, bound, failed), bound);
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
		http_service->set_shutdown_grace(std::chrono::duration_cast<std::chrono::milliseconds>(SHUTDOWN_GRACE));
		if (lag_probe_enabled()) { http_service->enable_lag_probe(); }
		http_service->enable_compression(search_compression());
		http_service->set_max_body(BUFFERED_BODY_CAP);

		// Each reactor's host, over the reactor's scheduler: the one its requests' tasks resume through. The
		// loop's thread is bound to it before the loop serves anything.
		for (std::size_t i = 0; i < http_service->reactors(); ++i) {
			auto& reactor = http_service->reactor(i);
			auto& host = http_hosts.emplace_back(std::make_unique<engine::AsioHost>(reactor.scheduler()));
			asio::post(reactor.io(), [host = host.get()] { host->bind_loop_thread(); });
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

	// The idle housekeeping runs as a task on this loop; the autocommit is already waiting for its first write.
	_rx->housekeeping->start();

	// Start the fsynchers.
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
XapiandManager::drain_control(control::Tasks& tasks, const char* what, std::chrono::steady_clock::time_point limit)
{
	while ((database_pool->detached() != 0 || tasks.running() != 0) && std::chrono::steady_clock::now() < limit) {
		run_control(10ms);
	}
	if (database_pool->detached() != 0 || tasks.running() != 0) {
		L_WARNING("{} {} tasks and {} tasks of the index registry did not end", tasks.running(), what, database_pool->detached());
	}
}


void
XapiandManager::run_control(std::chrono::milliseconds slice)
{
	engine::HostBinding binding(&_rx->host);   // the tasks it resumes find their host, as they did on the loop
	auto& io = _rx->reactor.io();
	io.restart();   // a loop that ran out of work is stopped: a chore that finishes later posts to a stopped one
	if (slice.count() == 0) {
		io.poll();
	} else {
		io.run_for(slice);
	}
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	if (!database_pool) {
		return;  // already joined!
	}

	const auto signals_at_join = shutdown_signals.load();
	const auto insisted = [this, signals_at_join] { return shutdown_signals.load() > signals_at_join; };

	// The stop has one bound (D16): the drains, the HTTP service and the registry's close all end by this time.
	const auto deadline = std::chrono::steady_clock::now() + shutdown_write_bound;

	// The control loop has stopped (break_loop). The tasks that wait end now: the housekeeping, and the signal
	// reader, whose read is cancelled.
	if (_rx->housekeeping) {
		_rx->housekeeping->stop();
	}
	{
		asio::error_code ec;
		_rx->sig_reader.cancel(ec);
	}

	// The index registry's tasks that timers started (an idle close, a commit) run on the control loop, which
	// has stopped: from now on they run on the thread that starts them, and the ones already posted run here,
	// on this thread, until they end.
	database_pool->set_control(nullptr);
	drain_control(_rx->tasks, "control", deadline);

	// This method should finish and wait for all objects and threads to finish
	// their work. Order of waiting for objects here matters!
	L_MANAGER(STEEL_BLUE + "Manager:\n{}Databases:\n{}Nodes:\n{}", __repr__(), database_pool->dump_databases(), Node::dump_nodes());

	////////////////////////////////////////////////////////////////////
	if (http_service) {
		L(-LOG_NOTICE, NOTICE_COL, "Stop 1: the HTTP service");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the HTTP service...", "HTTP service finished!");
		// Phase 1 of task's teardown: cancels the requests, lets them finish for the grace, forces the sockets a
		// stalled peer still holds, lets them finish for another grace, then stops and joins the reactors. The
		// writes admitted are let finish within that; what is still running at the deadline is reported and abandoned.
		// A SIGINT or SIGTERM that arrives from here on is an insistence: it ends the wait and the registry's.
		const auto left = std::max(std::chrono::milliseconds(1), std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()));
		http_service->set_shutdown_grace(http_shutdown_grace(left));   // before stop() starts, which reads it
		// The poll every 50 ms runs the control loop: the catalog's timers (a wake a busy index retries) fire there.
		auto report = shutdown_wait::wait(left, [service = http_service.get()] { service->stop(); }, [this] { return unfinished_work(); }, [this, &insisted] {
			run_control(0ms);
			return insisted();
		});
		if (!report.finished) {
			L_WARNING("Shutdown abandoned {} unfinished item{} after waiting {} ms for the writes in flight{}:", report.abandoned.size(), report.abandoned.size() == 1 ? "" : "s", report.waited.count(), report.interrupted ? " (stopped by a second signal)" : "");
			for (const auto& line : report.abandoned) {
				L_WARNING("    {}", line);
			}
			abandoned_http_ = std::move(http_service);   // the thread that waits in stop() still uses it
			abandoned_ = true;
		}
	}

	// The requests left suspended on the stopped HTTP loops (on a turn, a chore, a body read) are destroyed now,
	// before the indexes close, which releases their shard uses and turns (task's cancellation D8): the chores
	// they may wait on are cancelled and drained, then the service closes every loop's scheduler (phase 2) and
	// destroys the tasks still suspended on them (phase 3).
	if (http_service && !abandoned_) {
		if (!IndexChores::cancel_all(deadline)) {
			L_WARNING("Index chores were still running at the shutdown deadline");
		}
		http_service->close();
		http_service->destroy_roots();
	}

	////////////////////////////////////////////////////////////////////
	// The autocommits still waiting do not run: closing an index commits what it has pending. After an abandoned
	// stop the reactors still run, and a write that ends late would commit on its own thread: leave the scheduler.
	if (!abandoned_) {
		L(-LOG_NOTICE, NOTICE_COL, "Stop 2: the autocommits");
		_rx->autocommits->stop();
		drain_control(_rx->autocommits->tasks(), "autocommit", deadline);
	}


	// The control loop's tasks still suspended (an autocommit that did not run, a wait the drains gave up on) go
	// the same way, with the loop stopped: its scheduler closes and destroys them. Every root that may hold a pin
	// is gone before the catalog's shutdown is awaited, which waits for every pin (x-pool C24).
	if (!abandoned_) {
		_rx->reactor.io().stop();
		_rx->reactor.scheduler().close();
		_rx->reactor.scheduler().destroy_roots();
	}

	////////////////////////////////////////////////////////////////////
	if (database_pool) {
		L(-LOG_NOTICE, NOTICE_COL, "Stop 3: the indexes");
		database_pool->finish();

		L_MANAGER("Clearing and waiting for database pool!");
		L_MANAGER_TIMED(1s, "Is taking too long to finish the database pool...", "Database pool finished!");
		for (;;) {
			run_control(0ms);   // the catalog's timers, which its shutdown may arm, fire on the control loop
			const auto remaining = deadline - std::chrono::steady_clock::now();
			if (database_pool->join(std::clamp(std::chrono::duration_cast<std::chrono::milliseconds>(remaining), std::chrono::milliseconds(1), std::chrono::milliseconds(10)))) {
				break;
			}
			int sig = atom_sig;
			if (sig < 0) {
				throw SystemExit(-sig);
			}
			if (insisted() || std::chrono::steady_clock::now() >= deadline) {
				L_WARNING("Shutdown abandoned the indexes that are still in use{}:", insisted() ? " (stopped by a second signal)" : "");
				for (const auto& line : database_pool->busy()) {
					L_WARNING("    {}", line);
				}
				abandoned_ = true;
				break;
			}
		}
	}

#if XAPIAND_DATABASE_WAL

	////////////////////////////////////////////////////////////////////
	if (wal_writer) {
		L(-LOG_NOTICE, NOTICE_COL, "Stop 4: the write-ahead logs");
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
	auto& fsyncher_obj = fsyncher(false);
	if (fsyncher_obj) {
		L(-LOG_NOTICE, NOTICE_COL, "Stop 5: the fsync threads");
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
	if (abandoned_) {
		// Threads are still inside requests or index work that did not end, and they use the registry, the log
		// writers and the service: leave without destroying any of them. What was acknowledged is in the log
		// writers' files.
		L(-LOG_WARNING, WARNING_COL, "Exiting with unfinished writes abandoned (see above).");
		Logging::finish();
		Logging::join();
		_Exit(EX_TEMPFAIL);
	}

	L_MANAGER_TIMED(1s, "Is taking too long to reset manager...", "Manager reset finished!");

	// The hosts before the reactors whose schedulers they wrap; the service destroys its reactors (phase 4).
	http_hosts.clear();
	http_service.reset();

	database_pool.reset();

	wal_writer.reset();


	fsyncher_obj.reset();

	schemas.reset();

	////////////////////////////////////////////////////////////////////
	L_MANAGER("Server ended!");

}


std::vector<std::string>
XapiandManager::unfinished_work()
{
	std::vector<std::string> lines;
	if (http_service) {
		for (std::size_t i = 0; i < http_service->reactors(); ++i) {
			const auto inflight = http_service->reactor(i).gate().inflight.load();
			if (inflight > 0) {
				lines.push_back(strings::format("{} request{} in flight on http{}", inflight, inflight == 1 ? "" : "s", i));
			}
		}
	}
	if (database_pool) {
		for (auto& line : database_pool->busy()) {
			lines.push_back(std::move(line));
		}
	}
	return lines;
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

	// servers_threads: the Asio transport owns its reactors internally; expose the
	// reactor count as the running/pool size (queue/capacity are not modelled here).
	{
		auto reactors = http_service ? static_cast<double>(http_service->reactors()) : 0.0;
		metrics.xapiand_servers_running.Set(reactors);
		metrics.xapiand_servers_queue_size.Set(0);
		metrics.xapiand_servers_pool_size.Set(reactors);
		metrics.xapiand_servers_capacity.Set(reactors);
	}

	// committers: autocommits are tasks on the control loop, not threads; the queue is the keys waiting.
	metrics.xapiand_committers_running.Set(static_cast<double>(_rx->autocommits->tasks().running()));
	metrics.xapiand_committers_queue_size.Set(static_cast<double>(_rx->autocommits->pending()));

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
	metrics.xapiand_search_pinned_indexes.Set(static_cast<double>(search_pinned_indexes(false)));
	metrics.xapiand_search_pinned_indexes_peak.Set(static_cast<double>(search_pinned_indexes(true)));

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


