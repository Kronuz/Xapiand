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

#pragma once

#include "config.h"

#include <string>                        // for std::string


extern struct opts_t {
	double processors = 1;
	int verbosity = 0;
	bool detach = false;
	bool solo = false;
	bool strict = false;
	bool force = false;
	bool colors = false;
	bool no_colors = false;
	bool pretty = false;
	bool no_pretty = false;
	bool echo = false;
	bool no_echo = false;
	bool human = false;
	bool no_human = false;
	bool comments = false;
	bool no_comments = false;
	bool admin_commands = false;
	std::string database = "";
	std::string cluster_name = XAPIAND_CLUSTER_NAME;
	std::string node_name = "";
	std::string bind_address = "";
	unsigned int http_port = XAPIAND_HTTP_SERVERPORT;
	unsigned int remote_port = XAPIAND_REMOTE_SERVERPORT;
	unsigned int replication_port = XAPIAND_REPLICATION_SERVERPORT;
	unsigned int discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	std::string discovery_group = XAPIAND_DISCOVERY_GROUP;
	std::string pidfile = "";
	std::string logfile = "";
	std::string uid = "";
	std::string gid = "";
	std::string primary_node = "";
	ssize_t num_http_servers = 1;
	ssize_t num_http_clients = 1;
	ssize_t num_remote_servers = 1;
	ssize_t num_remote_clients = 1;
	ssize_t num_replication_servers = 1;
	ssize_t num_replication_clients = 1;
	ssize_t num_async_wal_writers = 1;
	ssize_t num_doc_matchers = 1;
	ssize_t num_doc_preparers = 1;
	ssize_t num_doc_indexers = 1;
	ssize_t num_committers = 1;
	ssize_t num_fsynchers = 1;
	ssize_t num_replicators = 1;
	ssize_t num_discoverers = 1;
	ssize_t database_pool_size = 10;
	ssize_t schema_pool_size = 30;
	ssize_t scripts_cache_size = 10;
	ssize_t resolver_cache_size = 100;
	ssize_t max_clients = 10;
	ssize_t max_database_readers = 3;
	ssize_t max_files = 0;  // (0 = automatic)
	size_t num_shards = 1;
	size_t num_replicas = 0;
	int flush_threshold = 100000;
	unsigned int ev_flags = 0;
	bool uuid_compact = false;
	uint32_t uuid_repr = 0;
	bool uuid_partition = false;
	std::string dump_documents = "";
	std::string restore_documents = "";
	std::string filename = "";
	bool iterm2 = false;
	bool log_epoch = false;
	bool log_iso8601 = false;
	bool log_timeless = false;
	bool log_plainseconds = false;
	bool log_milliseconds = false;
	bool log_microseconds = false;
	bool log_threads = false;
	bool log_location = false;
	bool log_replicas = false;
	double random_errors_db = 0;
	double random_errors_io = 0;
	double random_errors_net = 0;
	unsigned long long schema_pool_timeout = 3600000;
	unsigned long long resolver_cache_timeout = 60000;
	unsigned long long committer_throttle_time = 0;
	unsigned long long committer_debounce_timeout = 1000;
	unsigned long long committer_debounce_busy_timeout = 1000;
	unsigned long long committer_debounce_min_force_timeout = 1000;
	unsigned long long committer_debounce_max_force_timeout = 1000;
	unsigned long long fsyncher_throttle_time = 0;
	unsigned long long fsyncher_debounce_timeout = 500;
	unsigned long long fsyncher_debounce_busy_timeout = 500;
	unsigned long long fsyncher_debounce_min_force_timeout = 500;
	unsigned long long fsyncher_debounce_max_force_timeout = 500;
	unsigned long long db_updater_throttle_time = 0;
	unsigned long long db_updater_debounce_timeout = 1000;
	unsigned long long db_updater_debounce_busy_timeout = 1000;
	unsigned long long db_updater_debounce_min_force_timeout = 1000;
	unsigned long long db_updater_debounce_max_force_timeout = 1000;
	unsigned long long trigger_replication_throttle_time = 0;
	unsigned long long trigger_replication_debounce_timeout = 1000;
	unsigned long long trigger_replication_debounce_busy_timeout = 1000;
	unsigned long long trigger_replication_debounce_min_force_timeout = 1000;
	unsigned long long trigger_replication_debounce_max_force_timeout = 1000;
	unsigned long long database_stall_time = 0;
} opts;

const char* ev_backend(unsigned int backend);

unsigned int ev_backend(const std::string& name);

opts_t parseOptions(int argc, char** argv);
