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

#include "opts.h"

#include <algorithm>                              // for std::max, std:min
#include <cmath>                                  // for std::ceil
#include <cstdio>                                 // for std::fprintf
#include <cstdlib>                                // for std::size_t, std::getenv, std::exit
#include <cstring>                                // for std::strchr, std::strrchr
#include <thread>                                 // for std::thread
#include <sysexits.h>                             // for EX_USAGE

#include <CLI/CLI.hpp>                            // for CLI::App (command-line parser)

#include "hashes.hh"                              // for fnv1ah32
#include "package.h"                              // for Package::VERSION
#include "xapiand_strings.hh"                             // for strings::lower

#define XAPIAND_PID_FILE         "xapiand.pid"
#define XAPIAND_LOG_FILE         "xapiand.log"

#define NUM_SHARDS               5                // Default number of database shards per index

#define SCRIPTS_CACHE_SIZE           100          // Scripts cache
#define RESOLVER_CACHE_SIZE          100          // Endpoint resolver cache
#define WAL_WRITER_CACHE_SIZE        100          // Wal writer cache
#define SCHEMA_POOL_SIZE             100          // Maximum number of schemas in schema pool
#define SCHEMAS_VERSIONS_CACHE_SIZE  100          // Schema versions cache
#define DATABASE_POOL_SIZE           200          // Maximum number of database endpoints in database pool
#define MAX_CLIENTS                 1000          // Maximum number of open client connections


#define NUM_ASYNC_WAL_WRITERS        0.5          // Number of database async WAL writers per CPU
#define MAX_ASYNC_WAL_WRITERS         10
#define MIN_ASYNC_WAL_WRITERS          1

#define NUM_FSYNCHERS                0.5          // Number of threads handling the fsyncs per CPU
#define MAX_FSYNCHERS                 10
#define MIN_FSYNCHERS                  1

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



#define EV_SELECT_NAME  "select"
#define EV_POLL_NAME    "poll"
#define EV_EPOLL_NAME   "epoll"
#define EV_KQUEUE_NAME  "kqueue"
#define EV_DEVPOLL_NAME "devpoll"
#define EV_PORT_NAME    "port"


#define fallback(a, b) (a) ? (a) : (b)


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

		std::string node_name;
		app.add_option("--name", node_name, "Node name.");

		std::size_t num_shards = NUM_SHARDS;
		app.add_option("--shards", num_shards, "Default number of database shards per index.");
#if XAPIAND_DATABASE_WAL
		std::size_t num_async_wal_writers = 0;
		app.add_option("--writers", num_async_wal_writers, "Number of database async wal writers.");
#endif
		std::size_t database_pool_size = DATABASE_POOL_SIZE;
		app.add_option("--database-pool-size", database_pool_size, "Maximum number of databases in database pool.");
		std::size_t max_search_indexes = 100000;
		app.add_option("--max-search-indexes", max_search_indexes, "Most indexes (shards) one search may name, and most directory entries its patterns may read to find them (at least 1); a search over more answers 400. A search reads its indexes in batches, so this bounds only a runaway pattern.")
			->check(CLI::Range(std::size_t{1}, std::size_t{1} << 40));
		std::size_t concurrent_search_indexes = 0;
		app.add_option("--concurrent-search-indexes", concurrent_search_indexes, "Most indexes one search holds open at once; it reads the rest in later batches. 0: a reactor's share of the database pool (the pool size over the reactors).");
		std::size_t search_walk_queue = 8;
		app.add_option("--search-walk-queue", search_walk_queue, "Most searches that may wait to look up the index names of their patterns behind the one that is looking (one walk of the data directory runs at a time); a search over that answers 429 with Retry-After.")
			->check(CLI::Range(std::size_t{0}, std::size_t{1} << 20));
		double search_walk_wait = 10;
		app.add_option("--search-walk-wait", search_walk_wait, "Seconds a search waits for its turn to look up the index names of its patterns; a search that waits longer answers 503 with Retry-After.")
			->check(CLI::Range(0.001, 3600.0));
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

		std::size_t max_files = 0;
		app.add_option("--max-files", max_files, "Maximum number of files to open.");

		std::size_t max_clients = MAX_CLIENTS;
		app.add_option("--max-clients", max_clients, "Max number of open client connections.");

		double processors = hardware_concurrency;
		app.add_option("--processors", processors, "Number of processors to use.");

		unsigned int http_port = 0;
		app.add_option("--port", http_port, "TCP HTTP port number to listen on for REST API.");

		std::string bind_address;
		app.add_option("--bind-address", bind_address, "Bind address to listen to.");

		std::string tls_cert, tls_key;
		std::size_t tls_max_connections = 1024;
#ifdef XAPIAND_TLS
		app.add_option("--tls-cert", tls_cert, "Serve the HTTP API over TLS with this PEM certificate chain (with --tls-key).");
		app.add_option("--tls-key", tls_key, "The PEM private key for --tls-cert; refused when readable by group or others.");
		app.add_option("--tls-max-connections", tls_max_connections, "Most TLS connections held at once (default 1024).");
#endif

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

		bool legacy_ids = false;
		app.add_flag("--legacy-ids", legacy_ids,
			"Mint and decode UUIDs with the legacy v1 codec only (pure v1, no v6). "
			"Use for a deployment that must stay byte-for-byte compatible with pre-1.0 ids.");

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

		o.legacy_ids = legacy_ids;

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
			}
		}

#ifdef DEBUG
		if (!o.log_plainseconds && !o.log_milliseconds && !o.log_microseconds) {
			o.log_microseconds = true;
		}
#endif

		o.database = database;
		o.http_port = http_port;
		o.bind_address = bind_address;
		o.tls_cert = tls_cert;
		o.tls_key = tls_key;
		o.tls_max_connections = tls_max_connections;
		o.node_name = node_name;
		o.pidfile = pidfile;
		o.logfile = logfile;
		o.uid = uid;
		o.gid = gid;
		o.database_pool_size = database_pool_size;
		o.max_search_indexes = max_search_indexes;
		o.concurrent_search_indexes = concurrent_search_indexes;
		o.search_walk_queue = search_walk_queue;
		o.search_walk_wait = search_walk_wait;
		o.schema_pool_size = schema_pool_size;
		o.schema_versions_size =  schema_versions_size;
		o.scripts_cache_size = scripts_cache_size;
		o.resolver_cache_size = resolver_cache_size;
		o.wal_writer_cache_size = wal_writer_cache_size;
		o.num_shards = num_shards;
#if XAPIAND_DATABASE_WAL
		o.num_async_wal_writers = fallback(num_async_wal_writers, std::max(MIN_ASYNC_WAL_WRITERS, std::min(MAX_ASYNC_WAL_WRITERS, static_cast<int>(std::ceil(NUM_ASYNC_WAL_WRITERS * o.processors)))));
#endif
		o.num_fsynchers = std::max(MIN_FSYNCHERS, std::min(MAX_FSYNCHERS, static_cast<int>(std::ceil(NUM_FSYNCHERS * o.processors))));

		o.max_clients = max_clients;
		o.max_files = max_files;
		o.num_http_servers = std::max(1, static_cast<int>(std::ceil(o.processors)));   // one reactor per processor
		if (o.detach) {
			if (o.logfile.empty()) {
				o.logfile = XAPIAND_ROOT "/var/log/" XAPIAND_LOG_FILE;
			}
			if (o.pidfile.empty()) {
				o.pidfile = XAPIAND_ROOT "/var/run/" XAPIAND_PID_FILE;
			}
		}

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
		if (tls_cert.empty() != tls_key.empty()) {
			usage_error("Option invalid: --tls-cert and --tls-key go together");
		}
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



	return o;
}
