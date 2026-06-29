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

#include "opts.h"

#include <algorithm>                              // for std::max, std:min
#include <cmath>                                  // for std::ceil
#include <cstdio>                                 // for std::fprintf
#include <cstdlib>                                // for std::size_t, std::getenv, std::exit
#include <cstring>                                // for std::strchr, std::strrchr
#include <thread>                                 // for std::thread
#include <sysexits.h>                             // for EX_USAGE

#include <CLI/CLI.hpp>                            // for CLI::App (command-line parser)

#include "ev/ev++.h"                              // for ev_supported
#include "hashes.hh"                              // for fnv1ah32
#include "package.h"                              // for Package::VERSION
#include "strings.hh"                             // for strings::lower

#define XAPIAND_PID_FILE         "xapiand.pid"
#define XAPIAND_LOG_FILE         "xapiand.log"

#define FLUSH_THRESHOLD          100000           // Database flush threshold (default for xapian is 10000)
#define NUM_SHARDS               5                // Default number of database shards per index
#define NUM_REPLICAS             1                // Default number of database replicas per index

#define SCRIPTS_CACHE_SIZE           100          // Scripts cache
#define RESOLVER_CACHE_SIZE          100          // Endpoint resolver cache
#define WAL_WRITER_CACHE_SIZE        100          // Wal writer cache
#define SCHEMA_POOL_SIZE             100          // Maximum number of schemas in schema pool
#define SCHEMAS_VERSIONS_CACHE_SIZE  100          // Schema versions cache
#define DATABASE_POOL_SIZE           200          // Maximum number of database endpoints in database pool
#define MAX_DATABASE_READERS          10          // Maximum number simultaneous readers per database
#define MAX_CLIENTS                 1000          // Maximum number of open client connections

#define NUM_HTTP_SERVERS             1.0          // Number of servers per CPU
#define MAX_HTTP_SERVERS              10
#define MIN_HTTP_SERVERS               1

#define NUM_HTTP_CLIENTS             1.5          // Number of http client threads per CPU
#define MAX_HTTP_CLIENTS              20
#define MIN_HTTP_CLIENTS               2

#define NUM_REMOTE_SERVERS           1.0          // Number of remote protocol client threads per CPU
#define MAX_REMOTE_SERVERS            10
#define MIN_REMOTE_SERVERS             1

#define NUM_REMOTE_CLIENTS           2.0          // Number of remote protocol client threads per CPU
#define MAX_REMOTE_CLIENTS            20
#define MIN_REMOTE_CLIENTS             2

#define NUM_REPLICATION_SERVERS      1.0          // Number of replication protocol client threads per CPU
#define MAX_REPLICATION_SERVERS       10
#define MIN_REPLICATION_SERVERS        1

#define NUM_REPLICATION_CLIENTS      0.5          // Number of replication protocol client threads per CPU
#define MAX_REPLICATION_CLIENTS       10
#define MIN_REPLICATION_CLIENTS        1

#define NUM_ASYNC_WAL_WRITERS        0.5          // Number of database async WAL writers per CPU
#define MAX_ASYNC_WAL_WRITERS         10
#define MIN_ASYNC_WAL_WRITERS          1

#define NUM_DOC_MATCHERS             3.0          // Number of threads handling parallel matching of documents per CPU
#define MAX_DOC_MATCHERS              30
#define MIN_DOC_MATCHERS               3

#define NUM_DOC_PREPARERS            1.0          // Number of threads handling bulk documents preparing per CPU
#define MAX_DOC_PREPARERS             10
#define MIN_DOC_PREPARERS              1

#define NUM_DOC_INDEXERS             3.0          // Number of threads handling bulk documents indexing per CPU
#define MAX_DOC_INDEXERS              30
#define MIN_DOC_INDEXERS               3

#define NUM_COMMITTERS               1.0          // Number of threads handling the commits per CPU
#define MAX_COMMITTERS                10
#define MIN_COMMITTERS                 1

#define NUM_FSYNCHERS                0.5          // Number of threads handling the fsyncs per CPU
#define MAX_FSYNCHERS                 10
#define MIN_FSYNCHERS                  1

#define NUM_REPLICATORS              0.5          // Number of threads handling the replication per CPU
#define MAX_REPLICATORS               10
#define MIN_REPLICATORS                1

#define NUM_DISCOVERERS             0.25          // Number of threads handling the discoverers per CPU
#define MAX_DISCOVERERS                5
#define MIN_DISCOVERERS                1

#define SCHEMA_POOL_TIMEOUT                          3600000
#define RESOLVER_CACHE_TIMEOUT                         60000

#define COMMITTER_THROTTLE_TIME                            0
#define COMMITTER_DEBOUNCE_TIMEOUT                      1000
#define COMMITTER_DEBOUNCE_BUSY_TIMEOUT                 3000
#define COMMITTER_DEBOUNCE_MIN_FORCE_TIMEOUT            8000
#define COMMITTER_DEBOUNCE_MAX_FORCE_TIMEOUT           10000

#define FSYNCHER_THROTTLE_TIME                          1000
#define FSYNCHER_DEBOUNCE_TIMEOUT                        500
#define FSYNCHER_DEBOUNCE_BUSY_TIMEOUT                   800
#define FSYNCHER_DEBOUNCE_MIN_FORCE_TIMEOUT             2500
#define FSYNCHER_DEBOUNCE_MAX_FORCE_TIMEOUT             3500

#define DB_UPDATER_THROTTLE_TIME                        1000
#define DB_UPDATER_DEBOUNCE_TIMEOUT                      100
#define DB_UPDATER_DEBOUNCE_BUSY_TIMEOUT                 500
#define DB_UPDATER_DEBOUNCE_MIN_FORCE_TIMEOUT           4900
#define DB_UPDATER_DEBOUNCE_MAX_FORCE_TIMEOUT           5100

#define TRIGGER_REPLICATION_THROTTLE_TIME               1000
#define TRIGGER_REPLICATION_DEBOUNCE_TIMEOUT             100
#define TRIGGER_REPLICATION_DEBOUNCE_BUSY_TIMEOUT        500
#define TRIGGER_REPLICATION_DEBOUNCE_MIN_FORCE_TIMEOUT  4900
#define TRIGGER_REPLICATION_DEBOUNCE_MAX_FORCE_TIMEOUT  5100

#define DATABASE_STALL_TIME                              300


#define EV_SELECT_NAME  "select"
#define EV_POLL_NAME    "poll"
#define EV_EPOLL_NAME   "epoll"
#define EV_KQUEUE_NAME  "kqueue"
#define EV_DEVPOLL_NAME "devpoll"
#define EV_PORT_NAME    "port"


#define fallback(a, b) (a) ? (a) : (b)


unsigned int
ev_backend(const std::string& name)
{
	auto ev_use = strings::lower(name);
	if (ev_use.empty() || ev_use.compare("auto") == 0) {
		return ev::AUTO;
	}
	if (ev_use.compare(EV_SELECT_NAME) == 0) {
		return ev::SELECT;
	}
	if (ev_use.compare(EV_POLL_NAME) == 0) {
		return ev::POLL;
	}
	if (ev_use.compare(EV_EPOLL_NAME) == 0) {
		return ev::EPOLL;
	}
	if (ev_use.compare(EV_KQUEUE_NAME) == 0) {
		return ev::KQUEUE;
	}
	if (ev_use.compare(EV_DEVPOLL_NAME) == 0) {
		return ev::DEVPOLL;
	}
	if (ev_use.compare(EV_PORT_NAME) == 0) {
		return ev::PORT;
	}
	return -1;
}


const char*
ev_backend(unsigned int backend)
{
	switch(backend) {
		case ev::SELECT:
			return EV_SELECT_NAME;
		case ev::POLL:
			return EV_POLL_NAME;
		case ev::EPOLL:
			return EV_EPOLL_NAME;
		case ev::KQUEUE:
			return EV_KQUEUE_NAME;
		case ev::DEVPOLL:
			return EV_DEVPOLL_NAME;
		case ev::PORT:
			return EV_PORT_NAME;
	}
	return "unknown";
}


std::vector<std::string> ev_supported() {
	std::vector<std::string> backends;
	unsigned int supported = ev::supported_backends();
	if ((supported & ev::SELECT) != 0u) { backends.emplace_back(EV_SELECT_NAME); }
	if ((supported & ev::POLL) != 0u) { backends.emplace_back(EV_POLL_NAME); }
	if ((supported & ev::EPOLL) != 0u) { backends.emplace_back(EV_EPOLL_NAME); }
	if ((supported & ev::KQUEUE) != 0u) { backends.emplace_back(EV_KQUEUE_NAME); }
	if ((supported & ev::DEVPOLL) != 0u) { backends.emplace_back(EV_DEVPOLL_NAME); }
	if ((supported & ev::PORT) != 0u) { backends.emplace_back(EV_PORT_NAME); }
	if (backends.empty()) {
		backends.emplace_back("auto");
	}
	return backends;
}


opts_t
parseOptions(int argc, char** argv)
{
	opts_t o;

	const double hardware_concurrency = std::thread::hardware_concurrency();

	CLI::App app{"", "xapiand"};
	app.set_version_flag("--version", std::string(Package::VERSION));
	app.set_help_flag("-h,--help", "Display usage information and exit.");
	// Unknown options are an error, as TCLAP enforced. The old ZSH_COMPLETE
	// zsh-completion path and the manual -Dx / --x=y arg pre-splitting are dropped:
	// CLI11 has no drop-in zsh equivalent and parses both attached forms natively.

	try {
		// Every option binds to a variable named after the old TCLAP arg, so the
		// value-extraction block below stays valid with the  calls gone.

#ifdef XAPIAND_RANDOM_ERRORS
		double random_errors_net = 0;
		app.add_option("--random-errors-net", random_errors_net, "Inject random network errors with this probability (0-1)");
		double random_errors_io = 0;
		app.add_option("--random-errors-io", random_errors_io, "Inject random IO errors with this probability (0-1)");
		double random_errors_db = 0;
		app.add_option("--random-errors-db", random_errors_db, "Inject random database errors with this probability (0-1)");
#endif

		std::string out;
		app.add_option("-o,--out", out, "Output filename for dump.");
		std::string dump_documents;
		app.add_option("--dump", dump_documents, "Dump endpoint to stdout.");
		std::string in;
		app.add_option("-i,--in", in, "Input filename for restore.");
		std::string restore_documents;
		app.add_option("--restore", restore_documents, "Restore endpoint from stdin.");

		int verbose = 0;
		app.add_flag("-v,--verbose", verbose, "Increase verbosity.");
		unsigned int verbosity = 0;
		app.add_option("--verbosity", verbosity, "Set verbosity.");

		std::vector<std::string> uuid_allowed({
			"vanilla",
#ifdef XAPIAND_UUID_GUID
			"guid",
#endif
#ifdef XAPIAND_UUID_URN
			"urn",
#endif
#ifdef XAPIAND_UUID_ENCODED
			"compact",
			"encoded",
			"partition",
#endif
		});
		std::vector<std::string> uuid;
		app.add_option("--uuid", uuid, "Toggle modes for compact and/or encoded UUIDs and UUID index path partitioning.")
			->check(CLI::IsMember(uuid_allowed));

#ifdef XAPIAND_CLUSTERING
		unsigned int discovery_port = 0;
		app.add_option("--discovery-port", discovery_port, "Discovery UDP port number to listen on.");
		std::string discovery_group = XAPIAND_DISCOVERY_GROUP;
		app.add_option("--discovery-group", discovery_group, "Discovery UDP group name.");
		std::string cluster_name = XAPIAND_CLUSTER_NAME;
		app.add_option("--cluster", cluster_name, "Cluster name to join.");
#endif
		std::string node_name;
		app.add_option("--name", node_name, "Node name.");

#if XAPIAND_DATABASE_WAL
		std::size_t num_async_wal_writers = 0;
		app.add_option("--writers", num_async_wal_writers, "Number of database async wal writers.");
#endif
#ifdef XAPIAND_CLUSTERING
		std::size_t num_replicas = NUM_REPLICAS;
		app.add_option("--replicas", num_replicas, "Default number of database replicas per index.");
		std::size_t num_shards = NUM_SHARDS;
		app.add_option("--shards", num_shards, "Default number of database shards per index.");
#endif
		std::size_t num_doc_matchers = 0;
		app.add_option("--matchers", num_doc_matchers, "Number of threads handling parallel document matching.");
		std::size_t num_doc_preparers = 0;
		app.add_option("--bulk-preparers", num_doc_preparers, "Number of threads handling bulk documents preparing.");
		std::size_t num_doc_indexers = 0;
		app.add_option("--bulk-indexers", num_doc_indexers, "Number of threads handling bulk documents indexing.");
		std::size_t num_committers = 0;
		app.add_option("--committers", num_committers, "Number of threads handling the commits.");
		std::size_t max_database_readers = MAX_DATABASE_READERS;
		app.add_option("--max-database-readers", max_database_readers, "Max number of open databases.");
		std::size_t database_pool_size = DATABASE_POOL_SIZE;
		app.add_option("--database-pool-size", database_pool_size, "Maximum number of databases in database pool.");
		std::size_t schema_pool_size = SCHEMA_POOL_SIZE;
		app.add_option("--schema-pool-size", schema_pool_size, "Maximum number of schemas in schema pool.");
		std::size_t schema_versions_size = SCHEMAS_VERSIONS_CACHE_SIZE;
		app.add_option("--schema-versions-size", schema_versions_size, "Maximum number of versions of schema in cache.");
		std::size_t scripts_cache_size = SCRIPTS_CACHE_SIZE;
		app.add_option("--scripts-cache-size", scripts_cache_size, "Cache size for scripts.");
		std::size_t resolver_cache_size = RESOLVER_CACHE_SIZE;
		app.add_option("--resolver-cache-size", resolver_cache_size, "Cache size for index resolver.");
		std::size_t wal_writer_cache_size = WAL_WRITER_CACHE_SIZE;
		app.add_option("--wal-writer-cache-size", wal_writer_cache_size, "Cache size wal writer.");

		std::size_t num_fsynchers = 0;
		app.add_option("--fsynchers", num_fsynchers, "Number of threads handling the fsyncs.");
#ifdef XAPIAND_CLUSTERING
		std::size_t num_replicators = 0;
		app.add_option("--replicators", num_replicators, "Number of replicators triggering database replication.");
		std::size_t num_discoverers = 0;
		app.add_option("--discoverers", num_discoverers, "Number of discoverers doing cluster discovery.");
#endif

		std::size_t max_files = 0;
		app.add_option("--max-files", max_files, "Maximum number of files to open.");
		std::size_t flush_threshold = FLUSH_THRESHOLD;
		app.add_option("--flush-threshold", flush_threshold, "Xapian flush threshold.");

#ifdef XAPIAND_CLUSTERING
		std::size_t num_remote_clients = 0;
		app.add_option("--remote-clients", num_remote_clients, "Number of remote protocol client threads.");
		std::size_t num_remote_servers = 0;
		app.add_option("--remote-servers", num_remote_servers, "Number of remote protocol servers.");
		std::size_t num_replication_clients = 0;
		app.add_option("--replication-clients", num_replication_clients, "Number of replication protocol client threads.");
		std::size_t num_replication_servers = 0;
		app.add_option("--replication-servers", num_replication_servers, "Number of replication protocol servers.");

		double database_stall_time = DATABASE_STALL_TIME;
		app.add_option("--database-stall-time", database_stall_time, "Seconds before allowing a shard to be promoted to primary.");
#endif
		std::size_t num_http_clients = 0;
		app.add_option("--http-clients", num_http_clients, "Number of http client threads.");
		std::size_t num_http_servers = 0;
		app.add_option("--http-servers", num_http_servers, "Number of http servers.");
		std::size_t max_clients = MAX_CLIENTS;
		app.add_option("--max-clients", max_clients, "Max number of open client connections.");

		double processors = hardware_concurrency;
		app.add_option("--processors", processors, "Number of processors to use.");

		auto use_allowed = ev_supported();
		std::string use = "auto";
		app.add_option("--use", use, "Connection processing backend.")
			->check(CLI::IsMember(use_allowed));

#ifdef XAPIAND_CLUSTERING
		std::string primary_node;
		app.add_option("--primary-node", primary_node, "Primary node (the one with the primary cluster database).");
		unsigned int remote_port = 0;
		app.add_option("--xapian-port", remote_port, "Xapian binary protocol TCP port number to listen on.");
		unsigned int replication_port = 0;
		app.add_option("--replica-port", replication_port, "Xapiand replication protocol TCP port number to listen on.");
#endif
		unsigned int http_port = 0;
		app.add_option("--port", http_port, "TCP HTTP port number to listen on for REST API.");

		std::string bind_address;
		app.add_option("--bind-address", bind_address, "Bind address to listen to.");

		bool iterm2 = false;
		app.add_flag("--iterm2", iterm2, "Set marks, tabs, title, badges and growl.");

		std::vector<std::string> log_allowed({
			"epoch",
			"iso8601",
			"timeless",
			"seconds",
			"milliseconds",
			"microseconds",
			"thread-names",
			"locations",
			"replicas",
		});
		std::vector<std::string> log;
		app.add_option("--log", log, "Enable logging settings.")
			->check(CLI::IsMember(log_allowed));

		std::string gid;
		app.add_option("--gid", gid, "Group ID.");
		std::string uid;
		app.add_option("--uid", uid, "User ID.");

		std::string pidfile;
		app.add_option("-P,--pidfile", pidfile, "Save PID in <file>.");
		std::string logfile;
		app.add_option("-L,--logfile", logfile, "Save logs in <file>.");

		bool admin_commands = false;
		app.add_flag("--admin-commands", admin_commands, "Enables administrative HTTP commands.");

		std::string color = "auto";
		auto color_opt = app.add_option("--color", color,
			"When to colorize console output: auto, always, never, truecolor, 256, 16.")
			->type_name("MODE")
			->check(CLI::IsMember({"auto", "always", "on", "never", "off",
				"truecolor", "24bit", "256", "256color", "16", "ansi", "stacked"}));
		bool no_color = false;
		app.add_flag("--no-color", no_color, "Disables colors on the console (alias for --color=never).");
		bool no_pretty = false;
		app.add_flag("--no-pretty", no_pretty, "Disables pretty results.");
		bool pretty = false;
		app.add_flag("--pretty", pretty, "Enables pretty results.");
		bool no_comments = false;
		app.add_flag("--no-comments", no_comments, "Disables result comments.");
		bool comments = false;
		app.add_flag("--comments", comments, "Enables result comments.");
		bool no_echo = false;
		app.add_flag("--no-echo", no_echo, "Disables objects echo in results.");
		bool echo = false;
		app.add_flag("--echo", echo, "Enables objects echo in results.");
		bool no_human = false;
		app.add_flag("--no-human", no_human, "Disables objects humanizer in results.");
		bool human = false;
		app.add_flag("--human", human, "Enables objects humanizer in results.");

		bool detach = false;
		app.add_flag("-d,--detach", detach, "detach process. (run in background)");
#ifdef XAPIAND_CLUSTERING
		bool solo = false;
		app.add_flag("--solo", solo, "Run solo indexer. (no replication or discovery)");
#endif
		bool strict = false;
		app.add_flag("--strict", strict, "Force the user to define the type for each field.");
		bool force = false;
		app.add_flag("--force", force, "Force using path as the root of the node.");
		std::string database = XAPIAND_ROOT "/var/db/xapiand";
		app.add_option("-D,--database", database, "Path to the root of the node.");

		app.parse(argc, argv);

#ifdef XAPIAND_RANDOM_ERRORS
		o.random_errors_db = random_errors_db;
		o.random_errors_io = random_errors_io;
		o.random_errors_net = random_errors_net;
#endif

		o.processors = std::max(1.0, std::min(processors, hardware_concurrency));
		o.verbosity = verbosity + verbose;
		o.detach = detach;

#ifdef XAPIAND_CLUSTERING
		o.solo = solo;
#else
		o.solo = true;
#endif
		o.strict = strict;
		o.force = force;

		o.echo = echo;
		o.no_echo = no_echo;

		o.human = human;
		o.no_human = no_human;

		o.comments = comments;
		o.no_comments = no_comments;

		o.pretty = pretty;
		o.no_pretty = no_pretty;

		// An explicit --color wins; otherwise --no-color maps to never, else auto.
		if (color_opt->count() > 0) {
			o.color = color;
		} else if (no_color) {
			o.color = "never";
		} else {
			o.color = "auto";
		}

		o.admin_commands = admin_commands;

		o.iterm2 = iterm2;

		for (const auto& u : log) {
			switch (fnv1ah32::hash(u)) {
				case fnv1ah32::hash("epoch"):
					o.log_epoch = true;
					break;
				case fnv1ah32::hash("iso8601"):
					o.log_iso8601 = true;
					break;
				case fnv1ah32::hash("timeless"):
					o.log_timeless = true;
					break;
				case fnv1ah32::hash("seconds"):
					o.log_plainseconds = true;
					break;
				case fnv1ah32::hash("milliseconds"):
					o.log_milliseconds = true;
					break;
				case fnv1ah32::hash("microseconds"):
					o.log_microseconds = true;
					break;
				case fnv1ah32::hash("thread-names"):
					o.log_threads = true;
					break;
				case fnv1ah32::hash("locations"):
					o.log_location = true;
					break;
				case fnv1ah32::hash("replicas"):
					o.log_replicas = true;
					break;
			}
		}

#ifdef DEBUG
		if (!o.log_plainseconds && !o.log_milliseconds && !o.log_microseconds) {
			o.log_microseconds = true;
		}
#endif

		o.database = database;
		if (o.database.empty()) {
			o.database = ".";
		}
		o.http_port = http_port;
		o.bind_address = bind_address;
		o.node_name = node_name;
#ifdef XAPIAND_CLUSTERING
		o.primary_node = primary_node;
		o.cluster_name = cluster_name;
		o.remote_port = remote_port;
		o.replication_port = replication_port;
		o.discovery_port = discovery_port;
		o.discovery_group = discovery_group;
#endif
		o.pidfile = pidfile;
		o.logfile = logfile;
		o.uid = uid;
		o.gid = gid;
		o.database_pool_size = database_pool_size;
		o.schema_pool_size = schema_pool_size;
		o.schema_versions_size =  schema_versions_size;
		o.scripts_cache_size = scripts_cache_size;
		o.resolver_cache_size = resolver_cache_size;
		o.wal_writer_cache_size = wal_writer_cache_size;
#if XAPIAND_DATABASE_WAL
		o.num_async_wal_writers = fallback(num_async_wal_writers, std::max(MIN_ASYNC_WAL_WRITERS, std::min(MAX_ASYNC_WAL_WRITERS, static_cast<int>(std::ceil(NUM_ASYNC_WAL_WRITERS * o.processors)))));
#endif
#ifdef XAPIAND_CLUSTERING
		o.num_shards = num_shards;
		o.num_replicas = num_replicas;
		o.num_replicators = fallback(num_replicators, std::max(MIN_REPLICATORS, std::min(MAX_REPLICATORS, static_cast<int>(std::ceil(NUM_REPLICATORS * o.processors)))));
		o.num_discoverers = fallback(num_discoverers, std::max(MIN_DISCOVERERS, std::min(MAX_DISCOVERERS, static_cast<int>(std::ceil(NUM_DISCOVERERS * o.processors)))));
#endif
		o.num_doc_matchers = fallback(num_doc_matchers, std::max(MIN_DOC_MATCHERS, std::min(MAX_DOC_MATCHERS, static_cast<int>(std::ceil(NUM_DOC_MATCHERS * o.processors)))));
		o.num_doc_preparers = fallback(num_doc_preparers, std::max(MIN_DOC_PREPARERS, std::min(MAX_DOC_PREPARERS, static_cast<int>(std::ceil(NUM_DOC_PREPARERS * o.processors)))));
		o.num_doc_indexers = fallback(num_doc_indexers, std::max(MIN_DOC_INDEXERS, std::min(MAX_DOC_INDEXERS, static_cast<int>(std::ceil(NUM_DOC_INDEXERS * o.processors)))));
		o.num_committers = fallback(num_committers, std::max(MIN_COMMITTERS, std::min(MAX_COMMITTERS, static_cast<int>(std::ceil(NUM_COMMITTERS * o.processors)))));
		o.num_fsynchers = fallback(num_fsynchers, std::max(MIN_FSYNCHERS, std::min(MAX_FSYNCHERS, static_cast<int>(std::ceil(NUM_FSYNCHERS * o.processors)))));

		o.max_clients = max_clients;
		o.max_database_readers = max_database_readers;
		o.max_files = max_files;
		o.flush_threshold = flush_threshold;
		o.num_http_clients = fallback(num_http_clients, std::max(MIN_HTTP_CLIENTS, std::min(MAX_HTTP_CLIENTS, static_cast<int>(std::ceil(NUM_HTTP_CLIENTS * o.processors)))));
		o.num_http_servers = fallback(num_http_servers, std::max(MIN_HTTP_SERVERS, std::min(MAX_HTTP_SERVERS, static_cast<int>(std::ceil(NUM_HTTP_SERVERS * o.processors)))));
#ifdef XAPIAND_CLUSTERING
		o.num_remote_clients = fallback(num_remote_clients, std::max(MIN_REMOTE_CLIENTS, std::min(MAX_REMOTE_CLIENTS, static_cast<int>(std::ceil(NUM_REMOTE_CLIENTS * o.processors)))));
		o.num_remote_servers = fallback(num_remote_servers, std::max(MIN_REMOTE_SERVERS, std::min(MAX_REMOTE_SERVERS, static_cast<int>(std::ceil(NUM_REMOTE_SERVERS * o.processors)))));
		o.num_replication_clients = fallback(num_replication_clients, std::max(MIN_REPLICATION_CLIENTS, std::min(MAX_REPLICATION_CLIENTS, static_cast<int>(std::ceil(NUM_REPLICATION_CLIENTS * o.processors)))));
		o.num_replication_servers = fallback(num_replication_servers, std::max(MIN_REPLICATION_SERVERS, std::min(MAX_REPLICATION_SERVERS, static_cast<int>(std::ceil(NUM_REPLICATION_SERVERS * o.processors)))));

		o.database_stall_time = database_stall_time * 1000.0;
#endif
		if (o.detach) {
			if (o.logfile.empty()) {
				o.logfile = XAPIAND_ROOT "/var/log/" XAPIAND_LOG_FILE;
			}
			if (o.pidfile.empty()) {
				o.pidfile = XAPIAND_ROOT "/var/run/" XAPIAND_PID_FILE;
			}
		}
		o.ev_flags = ev_backend(use);

		bool uuid_configured = false;
		for (const auto& u : uuid) {
			switch (fnv1ah32::hash(u)) {
				case fnv1ah32::hash("vanilla"):
					o.uuid_repr = fnv1ah32::hash("vanilla");
					uuid_configured = true;
					break;
#ifdef XAPIAND_UUID_GUID
				case fnv1ah32::hash("guid"):
					o.uuid_repr = fnv1ah32::hash("guid");
					uuid_configured = true;
					break;
#endif
#ifdef XAPIAND_UUID_URN
				case fnv1ah32::hash("urn"):
					o.uuid_repr = fnv1ah32::hash("urn");
					uuid_configured = true;
					break;
#endif
#ifdef XAPIAND_UUID_ENCODED
				case fnv1ah32::hash("encoded"):
					o.uuid_repr = fnv1ah32::hash("encoded");
					uuid_configured = true;
					break;
#endif
				case fnv1ah32::hash("compact"):
					o.uuid_compact = true;
					break;
				case fnv1ah32::hash("partition"):
					o.uuid_partition = true;
					break;
			}
		}
		if (!uuid_configured) {
#ifdef XAPIAND_UUID_ENCODED
			o.uuid_repr = fnv1ah32::hash("encoded");
#else
			o.uuid_repr = fnv1ah32::hash("vanilla");
#endif
			o.uuid_compact = true;
		}

		o.dump_documents = dump_documents;
		auto out_filename = out;
		o.restore_documents = restore_documents;
		auto in_filename = in;
		auto usage_error = [](const char* msg) {
			std::fprintf(stderr, "Error: %s\n", msg);
			std::exit(EX_USAGE);
		};
		if ((!o.dump_documents.empty()) && !o.restore_documents.empty()) {
			usage_error("Cannot dump and restore at the same time");
		} else if (!o.dump_documents.empty() || !o.restore_documents.empty()) {
			if (!o.restore_documents.empty()) {
				if (!out_filename.empty()) {
					usage_error("Option invalid: --out <file> can be used only with --dump");
				}
				o.filename = in_filename;
			} else {
				if (!in_filename.empty()) {
					usage_error("Option invalid: --in <file> can be used only with --restore");
				}
				o.filename = out_filename;
			}
			o.detach = false;
		} else {
			if (!in_filename.empty()) {
				usage_error("Option invalid: --in <file> can be used only with --restore");
			}
			if (!out_filename.empty()) {
				usage_error("Option invalid: --out <file> can be used only with --dump");
			}
		}

	} catch (const CLI::ParseError& exc) {
		// Handles --help, --version, and any parse/validation error: CLI11 prints
		// (via our formatter) and returns the appropriate exit code.
		std::exit(app.exit(exc));
	}

	o.schema_pool_timeout = SCHEMA_POOL_TIMEOUT;
	o.resolver_cache_timeout = RESOLVER_CACHE_TIMEOUT;

	o.committer_throttle_time = COMMITTER_THROTTLE_TIME;
	o.committer_debounce_timeout = COMMITTER_DEBOUNCE_TIMEOUT;
	o.committer_debounce_busy_timeout = COMMITTER_DEBOUNCE_BUSY_TIMEOUT;
	o.committer_debounce_min_force_timeout = COMMITTER_DEBOUNCE_MIN_FORCE_TIMEOUT;
	o.committer_debounce_max_force_timeout = COMMITTER_DEBOUNCE_MAX_FORCE_TIMEOUT;

	o.fsyncher_throttle_time = FSYNCHER_THROTTLE_TIME;
	o.fsyncher_debounce_timeout = FSYNCHER_DEBOUNCE_TIMEOUT;
	o.fsyncher_debounce_busy_timeout = FSYNCHER_DEBOUNCE_BUSY_TIMEOUT;
	o.fsyncher_debounce_min_force_timeout = FSYNCHER_DEBOUNCE_MIN_FORCE_TIMEOUT;
	o.fsyncher_debounce_max_force_timeout = FSYNCHER_DEBOUNCE_MAX_FORCE_TIMEOUT;

	o.db_updater_throttle_time = DB_UPDATER_THROTTLE_TIME;
	o.db_updater_debounce_timeout = DB_UPDATER_DEBOUNCE_TIMEOUT;
	o.db_updater_debounce_busy_timeout = DB_UPDATER_DEBOUNCE_BUSY_TIMEOUT;
	o.db_updater_debounce_min_force_timeout = DB_UPDATER_DEBOUNCE_MIN_FORCE_TIMEOUT;
	o.db_updater_debounce_max_force_timeout = DB_UPDATER_DEBOUNCE_MAX_FORCE_TIMEOUT;

	o.trigger_replication_throttle_time = TRIGGER_REPLICATION_THROTTLE_TIME;
	o.trigger_replication_debounce_timeout = TRIGGER_REPLICATION_DEBOUNCE_TIMEOUT;
	o.trigger_replication_debounce_busy_timeout = TRIGGER_REPLICATION_DEBOUNCE_BUSY_TIMEOUT;
	o.trigger_replication_debounce_min_force_timeout = TRIGGER_REPLICATION_DEBOUNCE_MIN_FORCE_TIMEOUT;
	o.trigger_replication_debounce_max_force_timeout = TRIGGER_REPLICATION_DEBOUNCE_MAX_FORCE_TIMEOUT;

	return o;
}
