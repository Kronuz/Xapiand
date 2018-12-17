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

#pragma once

#include "config.h"

#include <cmath>                         // for std::ceil
#include <cstdint>                       // for std::uint32_t
#include <cstdlib>                       // for std::size_t
#include <string>                        // for std::string
#include <sys/types.h>                   // for ssize_t


#define NUM_SERVERS              2       // Number of servers per CPU
#define NUM_HTTP_CLIENTS         16      // Number of http client threads per CPU
#define NUM_BINARY_CLIENTS       16      // Number of binary client threads per CPU
#define NUM_ASYNC_WAL_WRITERS    1       // Number of database async WAL writers per CPU
#define NUM_COMMITTERS           1       // Number of threads handling the commits per CPU
#define NUM_FSYNCHERS            1       // Number of threads handling the fsyncs per CPU

#define DBPOOL_SIZE              300     // Maximum number of database endpoints in database pool
#define MAX_CLIENTS              1000    // Maximum number of open client connections
#define MAX_DATABASES            400     // Maximum number of open databases
#define FLUSH_THRESHOLD          100000  // Database flush threshold (default for xapian is 10000)
#define ENDPOINT_LIST_SIZE       10      // Endpoints List's size
#define NUM_REPLICAS             3       // Default number of database replicas per index


extern struct opts_t {
	int verbosity = 0;
	bool detach = false;
	bool chert = false;
	bool solo = false;
	bool strict = false;
	bool force = false;
	bool optimal = false;
	bool foreign = false;
	bool colors = false;
	bool no_colors = false;
	bool admin_commands = false;
	std::string database = ".";
	std::string cluster_name = XAPIAND_CLUSTER_NAME;
	std::string node_name = "";
	std::string bind_address = "";
	unsigned int http_port = XAPIAND_HTTP_SERVERPORT;
	unsigned int binary_port = XAPIAND_BINARY_SERVERPORT;
	unsigned int discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	unsigned int raft_port = XAPIAND_RAFT_SERVERPORT;
	std::string pidfile = "";
	std::string logfile = "";
	std::string uid = "";
	std::string gid = "";
	std::string discovery_group = XAPIAND_DISCOVERY_GROUP;
	std::string raft_group = XAPIAND_RAFT_GROUP;
	ssize_t num_servers = std::ceil(NUM_SERVERS);
	ssize_t num_http_clients = std::ceil(NUM_BINARY_CLIENTS);
	ssize_t num_binary_clients = std::ceil(NUM_BINARY_CLIENTS);
	ssize_t num_async_wal_writers = std::ceil(NUM_ASYNC_WAL_WRITERS);
	ssize_t num_committers = std::ceil(NUM_COMMITTERS);
	ssize_t num_fsynchers = std::ceil(NUM_FSYNCHERS);
	ssize_t dbpool_size = DBPOOL_SIZE;
	ssize_t endpoints_list_size = ENDPOINT_LIST_SIZE;
	ssize_t max_clients = MAX_CLIENTS;
	ssize_t max_databases = MAX_DATABASES;
	ssize_t max_files = 0;  // (0 = automatic)
	int flush_threshold = FLUSH_THRESHOLD;
	unsigned int ev_flags = 0;
	bool uuid_compact = false;
	std::uint32_t uuid_repr = 0;
	bool uuid_partition = false;
	std::string dump_metadata = "";
	std::string dump_schema = "";
	std::string dump_documents = "";
	std::string restore = "";
	std::string filename = "";
	std::size_t num_replicas = NUM_REPLICAS;
	bool log_epoch = false;
	bool log_iso8601 = false;
	bool log_timeless = false;
	bool log_plainseconds = false;
	bool log_milliseconds = false;
	bool log_microseconds = false;
	bool log_threads = false;
	bool log_location = false;
	double random_errors_db = 0;
	double random_errors_io = 0;
	double random_errors_net = 0;
} opts;
