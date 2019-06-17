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

#include "cmdoutput.h"                            // for CmdOutput
#include "ev/ev++.h"                              // for ev_supported
#include "hashes.hh"                              // for fnv1ah32
#include "strings.hh"                             // for strings::lower

#define XAPIAND_PID_FILE         "xapiand.pid"
#define XAPIAND_LOG_FILE         "xapiand.log"

#define FLUSH_THRESHOLD          100000           // Database flush threshold (default for xapian is 10000)
#define NUM_SHARDS               5                // Default number of database shards per index
#define NUM_REPLICAS             1                // Default number of database replicas per index

#define SCRIPTS_CACHE_SIZE           100          // Scripts cache
#define RESOLVER_CACHE_SIZE          100          // Endpoint resolver cache
#define SCHEMA_POOL_SIZE             100          // Maximum number of schemas in schema pool
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

	using namespace TCLAP;

	try {
		CmdLine cmd("", ' ', Package::VERSION);

		CmdOutput output;
		ZshCompletionOutput zshoutput;

		if (std::getenv("ZSH_COMPLETE") != nullptr) {
			cmd.setOutput(&zshoutput);
		} else {
			cmd.setOutput(&output);
		}

#ifdef XAPIAND_RANDOM_ERRORS
		ValueArg<double> random_errors_net("", "random-errors-net", "Inject random network errors with this probability (0-1)", false, 0, "probability", cmd);
		ValueArg<double> random_errors_io("", "random-errors-io", "Inject random IO errors with this probability (0-1)", false, 0, "probability", cmd);
		ValueArg<double> random_errors_db("", "random-errors-db", "Inject random database errors with this probability (0-1)", false, 0, "probability", cmd);
#endif

		ValueArg<std::string> out("o", "out", "Output filename for dump.", false, "", "file", cmd);
		ValueArg<std::string> dump_documents("", "dump", "Dump endpoint to stdout.", false, "", "endpoint", cmd);
		ValueArg<std::string> in("i", "in", "Input filename for restore.", false, "", "file", cmd);
		ValueArg<std::string> restore_documents("", "restore", "Restore endpoint from stdin.", false, "", "endpoint", cmd);

		MultiSwitchArg verbose("v", "verbose", "Increase verbosity.", cmd);
		ValueArg<unsigned int> verbosity("", "verbosity", "Set verbosity.", false, 0, "verbosity", cmd);

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
		ValuesConstraint<std::string> uuid_constraint(uuid_allowed);
		MultiArg<std::string> uuid("", "uuid", "Toggle modes for compact and/or encoded UUIDs and UUID index path partitioning.", false, &uuid_constraint, cmd);

#ifdef XAPIAND_CLUSTERING
		ValueArg<unsigned int> discovery_port("", "discovery-port", "Discovery UDP port number to listen on.", false, 0, "port", cmd);
		ValueArg<std::string> discovery_group("", "discovery-group", "Discovery UDP group name.", false, XAPIAND_DISCOVERY_GROUP, "group", cmd);
		ValueArg<std::string> cluster_name("", "cluster", "Cluster name to join.", false, XAPIAND_CLUSTER_NAME, "cluster", cmd);
		ValueArg<std::string> node_name("", "name", "Node name.", false, "", "node", cmd);
#endif

#if XAPIAND_DATABASE_WAL
		ValueArg<std::size_t> num_async_wal_writers("", "writers", "Number of database async wal writers.", false, 0, "writers", cmd);
#endif
#ifdef XAPIAND_CLUSTERING
		ValueArg<std::size_t> num_replicas("", "replicas", "Default number of database replicas per index.", false, NUM_REPLICAS, "replicas", cmd);
		ValueArg<std::size_t> num_shards("", "shards", "Default number of database shards per index.", false, NUM_SHARDS, "shards", cmd);
#endif
		ValueArg<std::size_t> num_doc_matchers("", "matchers", "Number of threads handling parallel document matching.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_doc_preparers("", "bulk-preparers", "Number of threads handling bulk documents preparing.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_doc_indexers("", "bulk-indexers", "Number of threads handling bulk documents indexing.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_committers("", "committers", "Number of threads handling the commits.", false, 0, "threads", cmd);
		ValueArg<std::size_t> max_database_readers("", "max-database-readers", "Max number of open databases.", false, MAX_DATABASE_READERS, "databases", cmd);
		ValueArg<std::size_t> database_pool_size("", "database-pool-size", "Maximum number of databases in database pool.", false, DATABASE_POOL_SIZE, "size", cmd);
		ValueArg<std::size_t> schema_pool_size("", "schema-pool-size", "Maximum number of schemas in schema pool.", false, SCHEMA_POOL_SIZE, "size", cmd);
		ValueArg<std::size_t> scripts_cache_size("", "scripts-cache-size", "Cache size for scripts.", false, SCRIPTS_CACHE_SIZE, "size", cmd);
		ValueArg<std::size_t> resolver_cache_size("", "resolver-cache-size", "Cache size for index resolver.", false, RESOLVER_CACHE_SIZE, "size", cmd);

		ValueArg<std::size_t> num_fsynchers("", "fsynchers", "Number of threads handling the fsyncs.", false, 0, "fsynchers", cmd);
#ifdef XAPIAND_CLUSTERING
		ValueArg<std::size_t> num_replicators("", "replicators", "Number of replicators triggering database replication.", false, 0, "replicators", cmd);
		ValueArg<std::size_t> num_discoverers("", "discoverers", "Number of discoverers doing cluster discovery.", false, 0, "discoverers", cmd);
#endif

		ValueArg<std::size_t> max_files("", "max-files", "Maximum number of files to open.", false, 0, "files", cmd);
		ValueArg<std::size_t> flush_threshold("", "flush-threshold", "Xapian flush threshold.", false, FLUSH_THRESHOLD, "threshold", cmd);

#ifdef XAPIAND_CLUSTERING
		ValueArg<std::size_t> num_remote_clients("", "remote-clients", "Number of remote protocol client threads.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_remote_servers("", "remote-servers", "Number of remote protocol servers.", false, 0, "servers", cmd);
		ValueArg<std::size_t> num_replication_clients("", "replication-clients", "Number of replication protocol client threads.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_replication_servers("", "replication-servers", "Number of replication protocol servers.", false, 0, "servers", cmd);
#endif
		ValueArg<std::size_t> num_http_clients("", "http-clients", "Number of http client threads.", false, 0, "threads", cmd);
		ValueArg<std::size_t> num_http_servers("", "http-servers", "Number of http servers.", false, 0, "servers", cmd);
		ValueArg<std::size_t> max_clients("", "max-clients", "Max number of open client connections.", false, MAX_CLIENTS, "clients", cmd);

		ValueArg<double> processors("", "processors", "Number of processors to use.", false, hardware_concurrency, "processors", cmd);

		auto use_allowed = ev_supported();
		ValuesConstraint<std::string> use_constraint(use_allowed);
		ValueArg<std::string> use("", "use", "Connection processing backend.", false, "auto", &use_constraint, cmd);

#ifdef XAPIAND_CLUSTERING
		ValueArg<unsigned int> remote_port("", "xapian-port", "Xapian binary protocol TCP port number to listen on.", false, 0, "port", cmd);
		ValueArg<unsigned int> replication_port("", "replica-port", "Xapiand replication protocol TCP port number to listen on.", false, 0, "port", cmd);
#endif
		ValueArg<unsigned int> http_port("", "port", "TCP HTTP port number to listen on for REST API.", false, 0, "port", cmd);

		ValueArg<std::string> bind_address("", "bind-address", "Bind address to listen to.", false, "", "bind", cmd);

		SwitchArg iterm2("", "iterm2", "Set marks, tabs, title, badges and growl.", cmd, false);

		std::vector<std::string> log_allowed({
			"epoch",
			"iso8601",
			"timeless",
			"seconds",
			"milliseconds",
			"microseconds",
			"thread-names",
			"locations",
		});
		ValuesConstraint<std::string> log_constraint(log_allowed);
		MultiArg<std::string> log("", "log", "Enable logging settings.", false, &log_constraint, cmd);

		ValueArg<std::string> gid("", "gid", "Group ID.", false, "", "gid", cmd);
		ValueArg<std::string> uid("", "uid", "User ID.", false, "", "uid", cmd);

		ValueArg<std::string> pidfile("P", "pidfile", "Save PID in <file>.", false, "", "file", cmd);
		ValueArg<std::string> logfile("L", "logfile", "Save logs in <file>.", false, "", "file", cmd);

		SwitchArg admin_commands("", "admin-commands", "Enables administrative HTTP commands.", cmd, false);

		SwitchArg no_colors("", "no-colors", "Disables colors on the console.", cmd, false);
		SwitchArg colors("", "colors", "Enables colors on the console.", cmd, false);
		SwitchArg no_pretty("", "no-pretty", "Disables pretty results.", cmd, false);
		SwitchArg pretty("", "pretty", "Enables pretty results.", cmd, false);
		SwitchArg no_comments("", "no-comments", "Disables result comments.", cmd, false);
		SwitchArg comments("", "comments", "Enables result comments.", cmd, false);
		SwitchArg no_echo("", "no-echo", "Disables objects echo in results.", cmd, false);
		SwitchArg echo("", "echo", "Enables objects echo in results.", cmd, false);
		SwitchArg no_human("", "no-human", "Disables objects humanizer in results.", cmd, false);
		SwitchArg human("", "human", "Enables objects humanizer in results.", cmd, false);

		SwitchArg detach("d", "detach", "detach process. (run in background)", cmd);
#ifdef XAPIAND_CLUSTERING
		SwitchArg solo("", "solo", "Run solo indexer. (no replication or discovery)", cmd, false);
#endif
		SwitchArg strict("", "strict", "Force the user to define the type for each field.", cmd, false);
		SwitchArg force("", "force", "Force using path as the root of the node.", cmd, false);
		ValueArg<std::string> database("D", "database", "Path to the root of the node.", false, XAPIAND_ROOT "/var/db/xapiand", "path", cmd);

		std::vector<std::string> args;
		for (int i = 0; i < argc; ++i) {
			if (i == 0) {
				const char* a = std::strrchr(argv[i], '/');
				if (a != nullptr) {
					++a;
				} else {
					a = argv[i];
				}
				args.emplace_back(a);
			} else {
				// Split arguments when possible (e.g. -Dnode, --verbosity=3)
				const char* arg = argv[i];
				if (arg[0] == '-') {
					if (arg[1] != '-' && arg[1] != 'v') {  // skip long arguments (e.g. --verbosity) or multiswitch (e.g. -vvv)
						std::string tmp(arg, 2);
						args.push_back(tmp);
						arg += 2;
					}
				}
				const char* a = std::strchr(arg, '=');
				if (a != nullptr) {
					if ((a - arg) != 0) {
						std::string tmp(arg, a - arg);
						args.push_back(tmp);
					}
					arg = a + 1;
				}
				if (*arg != 0) {
					args.emplace_back(arg);
				}
			}
		}

		cmd.parse(args);

#ifdef XAPIAND_RANDOM_ERRORS
		o.random_errors_db = random_errors_db.getValue();
		o.random_errors_io = random_errors_io.getValue();
		o.random_errors_net = random_errors_net.getValue();
#endif

		o.processors = std::max(1.0, std::min(processors.getValue(), hardware_concurrency));
		o.verbosity = verbosity.getValue() + verbose.getValue();
		o.detach = detach.getValue();

#ifdef XAPIAND_CLUSTERING
		o.solo = solo.getValue();
#else
		o.solo = true;
#endif
		o.strict = strict.getValue();
		o.force = force.getValue();

		o.echo = echo.getValue();
		o.no_echo = no_echo.getValue();

		o.human = human.getValue();
		o.no_human = no_human.getValue();

		o.comments = comments.getValue();
		o.no_comments = no_comments.getValue();

		o.pretty = pretty.getValue();
		o.no_pretty = no_pretty.getValue();

		o.colors = colors.getValue();
		o.no_colors = no_colors.getValue();

		o.admin_commands = admin_commands.getValue();

		o.iterm2 = iterm2.getValue();

		for (const auto& u : log.getValue()) {
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
			}
		}

#ifdef DEBUG
		if (!o.log_plainseconds && !o.log_milliseconds && !o.log_microseconds) {
			o.log_microseconds = true;
		}
#endif

		o.database = database.getValue();
		if (o.database.empty()) {
			o.database = ".";
		}
		o.http_port = http_port.getValue();
		o.bind_address = bind_address.getValue();
#ifdef XAPIAND_CLUSTERING
		o.cluster_name = cluster_name.getValue();
		o.node_name = node_name.getValue();
		o.remote_port = remote_port.getValue();
		o.replication_port = replication_port.getValue();
		o.discovery_port = discovery_port.getValue();
		o.discovery_group = discovery_group.getValue();
#endif
		o.pidfile = pidfile.getValue();
		o.logfile = logfile.getValue();
		o.uid = uid.getValue();
		o.gid = gid.getValue();
		o.database_pool_size = database_pool_size.getValue();
		o.schema_pool_size = schema_pool_size.getValue();
		o.scripts_cache_size = scripts_cache_size.getValue();
		o.resolver_cache_size = resolver_cache_size.getValue();
#if XAPIAND_DATABASE_WAL
		o.num_async_wal_writers = fallback(num_async_wal_writers.getValue(), std::max(MIN_ASYNC_WAL_WRITERS, std::min(MAX_ASYNC_WAL_WRITERS, static_cast<int>(std::ceil(NUM_ASYNC_WAL_WRITERS * o.processors)))));
#endif
#ifdef XAPIAND_CLUSTERING
		o.num_shards = num_shards.getValue();
		o.num_replicas = num_replicas.getValue();
		o.num_replicators = fallback(num_replicators.getValue(), std::max(MIN_REPLICATORS, std::min(MAX_REPLICATORS, static_cast<int>(std::ceil(NUM_REPLICATORS * o.processors)))));
		o.num_discoverers = fallback(num_discoverers.getValue(), std::max(MIN_DISCOVERERS, std::min(MAX_DISCOVERERS, static_cast<int>(std::ceil(NUM_DISCOVERERS * o.processors)))));
#endif
		o.num_doc_matchers = fallback(num_doc_matchers.getValue(), std::max(MIN_DOC_MATCHERS, std::min(MAX_DOC_MATCHERS, static_cast<int>(std::ceil(NUM_DOC_MATCHERS * o.processors)))));
		o.num_doc_preparers = fallback(num_doc_preparers.getValue(), std::max(MIN_DOC_PREPARERS, std::min(MAX_DOC_PREPARERS, static_cast<int>(std::ceil(NUM_DOC_PREPARERS * o.processors)))));
		o.num_doc_indexers = fallback(num_doc_indexers.getValue(), std::max(MIN_DOC_INDEXERS, std::min(MAX_DOC_INDEXERS, static_cast<int>(std::ceil(NUM_DOC_INDEXERS * o.processors)))));
		o.num_committers = fallback(num_committers.getValue(), std::max(MIN_COMMITTERS, std::min(MAX_COMMITTERS, static_cast<int>(std::ceil(NUM_COMMITTERS * o.processors)))));
		o.num_fsynchers = fallback(num_fsynchers.getValue(), std::max(MIN_FSYNCHERS, std::min(MAX_FSYNCHERS, static_cast<int>(std::ceil(NUM_FSYNCHERS * o.processors)))));

		o.max_clients = max_clients.getValue();
		o.max_database_readers = max_database_readers.getValue();
		o.max_files = max_files.getValue();
		o.flush_threshold = flush_threshold.getValue();
		o.num_http_clients = fallback(num_http_clients.getValue(), std::max(MIN_HTTP_CLIENTS, std::min(MAX_HTTP_CLIENTS, static_cast<int>(std::ceil(NUM_HTTP_CLIENTS * o.processors)))));
		o.num_http_servers = fallback(num_http_servers.getValue(), std::max(MIN_HTTP_SERVERS, std::min(MAX_HTTP_SERVERS, static_cast<int>(std::ceil(NUM_HTTP_SERVERS * o.processors)))));
#ifdef XAPIAND_CLUSTERING
		o.num_remote_clients = fallback(num_remote_clients.getValue(), std::max(MIN_REMOTE_CLIENTS, std::min(MAX_REMOTE_CLIENTS, static_cast<int>(std::ceil(NUM_REMOTE_CLIENTS * o.processors)))));
		o.num_remote_servers = fallback(num_remote_servers.getValue(), std::max(MIN_REMOTE_SERVERS, std::min(MAX_REMOTE_SERVERS, static_cast<int>(std::ceil(NUM_REMOTE_SERVERS * o.processors)))));
		o.num_replication_clients = fallback(num_replication_clients.getValue(), std::max(MIN_REPLICATION_CLIENTS, std::min(MAX_REPLICATION_CLIENTS, static_cast<int>(std::ceil(NUM_REPLICATION_CLIENTS * o.processors)))));
		o.num_replication_servers = fallback(num_replication_servers.getValue(), std::max(MIN_REPLICATION_SERVERS, std::min(MAX_REPLICATION_SERVERS, static_cast<int>(std::ceil(NUM_REPLICATION_SERVERS * o.processors)))));
#endif
		if (o.detach) {
			if (o.logfile.empty()) {
				o.logfile = XAPIAND_ROOT "/var/log/" XAPIAND_LOG_FILE;
			}
			if (o.pidfile.empty()) {
				o.pidfile = XAPIAND_ROOT "/var/run/" XAPIAND_PID_FILE;
			}
		}
		o.ev_flags = ev_backend(use.getValue());

		bool uuid_configured = false;
		for (const auto& u : uuid.getValue()) {
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

		o.dump_documents = dump_documents.getValue();
		auto out_filename = out.getValue();
		o.restore_documents = restore_documents.getValue();
		auto in_filename = in.getValue();
		if ((!o.dump_documents.empty()) && !o.restore_documents.empty()) {
			throw CmdLineParseException("Cannot dump and restore at the same time");
		} else if (!o.dump_documents.empty() || !o.restore_documents.empty()) {
			if (!o.restore_documents.empty()) {
				if (!out_filename.empty()) {
					throw CmdLineParseException("Option invalid: --out <file> can be used only with --dump");
				}
				o.filename = in_filename;
			} else {
				if (!in_filename.empty()) {
					throw CmdLineParseException("Option invalid: --in <file> can be used only with --restore");
				}
				o.filename = out_filename;
			}
			o.detach = false;
		} else {
			if (!in_filename.empty()) {
				throw CmdLineParseException("Option invalid: --in <file> can be used only with --restore");
			}
			if (!out_filename.empty()) {
				throw CmdLineParseException("Option invalid: --out <file> can be used only with --dump");
			}
		}

	} catch (const ArgException& exc) { // catch any exceptions
		std::fprintf(stderr, "Error: %s for arg %s\n", exc.error().c_str(), exc.argId().c_str());
		std::exit(EX_USAGE);
	}

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
