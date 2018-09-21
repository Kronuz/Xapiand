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

#include "manager.h"

#include <algorithm>                          // for move
#include <atomic>                             // for atomic, atomic_int
#include <chrono>                             // for duration, system_clock
#include <cstdint>                            // for uint64_t, UINT64_MAX
#include <ctime>                              // for time_t, ctime, NULL
#include <cctype>                             // for isspace
#include <exception>                          // for exception
#include <functional>                         // for __base
#include <ifaddrs.h>                          // for ifaddrs, freeifaddrs
#include <memory>                             // for allocator, shared_ptr
#include <mutex>                              // for mutex, lock_guard, uniqu...
#include <net/if.h>                           // for IFF_LOOPBACK
#include <netinet/in.h>                       // for sockaddr_in, INET_ADDRST...
#include <ratio>                              // for milli
#include <regex>                              // for smatch, regex, operator|
#include <cstdlib>                            // for size_t, exit
#include <cstring>                            // for strerror
#include <string>                             // for string, basic_string
#include <errno.h>                            // for __error, errno
#include <fcntl.h>                            // for O_CLOEXEC, O_CREAT, O_RD...
#include <signal.h>                           // for SIGTERM, SIGINT
#include <sys/socket.h>                       // for AF_INET, sockaddr
#include <sys/types.h>                        // for uint64_t
#include <sysexits.h>                         // for EX_IOERR, EX_NOINPUT
#include <unistd.h>                           // for ssize_t, getpid
#include <unordered_map>                      // for __hash_map_const_iterator
#include <utility>                            // for pair
#include <vector>                             // for vector
#include <xapian.h>                           // for Error

#if defined(XAPIAND_V8)
#include <v8-version.h>                       // for V8_MAJOR_VERSION, V8_MINOR_VERSION
#endif
#if defined(XAPIAND_CHAISCRIPT)
#include <chaiscript/chaiscript_defines.hpp>  // for chaiscript::Build_Info
#endif

#include "allocator.h"
#include "async_fsync.h"                      // for AsyncFsync
#include "atomic_shared_ptr.h"                // for atomic_shared_ptr
#include "package.h"                          // for Package::FULLVERSION Package::HASH
#include "database.h"                         // for DatabasePool
#include "database_autocommit.h"              // for DatabaseAutocommit
#include "database_handler.h"                 // for DatabaseHandler
#include "database_utils.h"                   // for RESERVED_TYPE, DB_NOWAL
#include "endpoint.h"                         // for Node, Endpoint, local_node
#include "ev/ev++.h"                          // for async, loop_ref (ptr only)
#include "exception.h"                        // for Exit, ClientError, Excep...
#include "http_parser.h"                      // for http_method
#include "ignore_unused.h"                    // for ignore_unused
#include "io_utils.h"                         // for close, open, read, write
#include "log.h"                              // for L_CALL, L_DEBUG
#include "memory_stats.h"                     // for get_total_ram, get_total_virtual_memor...
#include "msgpack.h"                          // for MsgPack, object::object
#include "serialise.h"                        // for KEYWORD_STR
#include "servers/http.h"                     // for Http
#include "servers/server.h"                   // for XapiandServer, XapiandSe...
#include "servers/server_http.h"              // for HttpServer
#include "threadpool.h"                       // for ThreadPool
#include "utils.h"                            // for Stats::Pos, SLOT_TIME_SE...
#include "worker.h"                           // for Worker, enable_make_shared


#include "prometheus/serializer.h"            // for Serializer
#include "prometheus/text_serializer.h"       // for text_serializer
#include "prometheus/handler.h"               // for SerializeGet


#ifdef XAPIAND_CLUSTERING
#include "replicator.h"                       // for XapiandReplicator
#include "servers/binary.h"                   // for Binary
#include "servers/discovery.h"                // for Discovery
#include "servers/raft.h"                     // for Raft
#include "servers/server_binary.h"            // for RaftBinary
#include "servers/server_discovery.h"         // for DicoveryServer
#include "servers/server_raft.h"              // for RaftServer
#endif


#ifndef L_MANAGER
#define L_MANAGER_DEFINED
#define L_MANAGER L_NOTHING
#endif

#define NODE_LABEL "node"
#define CLUSTER_LABEL "cluster"

static const std::regex time_re("(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)(\\.\\.(?:(?:([0-9]+)h)?(?:([0-9]+)m)?(?:([0-9]+)s)?)?)?", std::regex::icase | std::regex::optimize);


constexpr const char* const XapiandManager::StateNames[];


std::shared_ptr<XapiandManager> XapiandManager::manager;
static ev::loop_ref* loop_ref_nil = nullptr;

void sig_exit(int sig) {
	if (sig < 0) {
		exit(-sig);
	} else if (XapiandManager::manager) {
		XapiandManager::manager->signal_sig(sig);
	}
}


Requestinfo::Requestinfo(const std::string& nodename, const std::string& cluster)
	: registry(std::make_shared<prometheus::Registry>()),
	  index_summary(prometheus::BuildSummary()
					.Name("xapiand_index_summary")
					.Help("Index requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_index_summary(index_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  search_summary(prometheus::BuildSummary()
					.Name("xapiand_search_summary")
					.Help("Search requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_search_summary(search_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  delete_summary(prometheus::BuildSummary()
					.Name("xapiand_delete_summary")
					.Help("Delete requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_delete_summary(delete_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  patch_summary(prometheus::BuildSummary()
					.Name("xapiand_patch_summary")
					.Help("Patch requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_patch_summary(patch_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  merge_summary(prometheus::BuildSummary()
					.Name("xapiand_merge_summary")
					.Help("Merge requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_merge_summary(merge_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  aggregation_summary(prometheus::BuildSummary()
					.Name("xapiand_aggretation_summary")
					.Help("Aggregation requests time serie")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Register(*registry)),
	  xapiand_aggregation_summary(aggregation_summary.Add(std::map<std::string, std::string>(), prometheus::Summary::Quantiles{{0, 0.05}, {0.25, 0.05}, {0.5, 0.05}, {0.75, 0.05}, {1, 0.05}})),
	  xapiand_http_clients_run(prometheus::BuildGauge()
					.Name("xapiand_http_clients_run")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Amount of clients running")
					.Register(*registry)),
	  xapiand_http_clients_run_met(xapiand_http_clients_run.Add(std::map<std::string, std::string>())),
	  xapiand_http_clients_queue(prometheus::BuildGauge()
					.Name("xapiand_http_clients_queue")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Clients in the queue")
					.Register(*registry)),
	  xapiand_http_clients_queue_met(xapiand_http_clients_queue.Add(std::map<std::string, std::string>())),
	  xapiand_http_clients_capacity(prometheus::BuildGauge()
					.Name("xapiand_http_clients_capacity")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Client queue capacity")
					.Register(*registry)),
	  xapiand_http_clients_capacity_met(xapiand_http_clients_capacity.Add(std::map<std::string, std::string>())),
	  xapiand_http_clients_pool_size(prometheus::BuildGauge()
					.Name("xapiand_http_clients_pool_size")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Client total pool size")
					.Register(*registry)),
	  xapiand_http_clients_pool_size_met(xapiand_http_clients_pool_size.Add(std::map<std::string, std::string>())),
	  xapiand_servers_run(prometheus::BuildGauge()
					.Name("xapiand_servers_run")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Amount of servers running")
					.Register(*registry)),
	  xapiand_servers_run_met(xapiand_servers_run.Add(std::map<std::string, std::string>())),
	  xapiand_servers_pool_size(prometheus::BuildGauge()
					.Name("xapiand_servers_pool_size")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Server pool size")
					.Register(*registry)),
	  xapiand_servers_pool_size_met(xapiand_servers_pool_size.Add(std::map<std::string, std::string>())),
	  xapiand_servers_queue(prometheus::BuildGauge()
					.Name("xapiand_servers_queue")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Servers in the queue")
					.Register(*registry)),
	  xapiand_servers_queue_met(xapiand_servers_queue.Add(std::map<std::string, std::string>())),
	  xapiand_servers_capacity(prometheus::BuildGauge()
					.Name("xapiand_servers_capacity")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Server queue capacity")
					.Register(*registry)),
	  xapiand_servers_capacity_met(xapiand_servers_capacity.Add(std::map<std::string, std::string>())),
	  xapiand_committers_running(prometheus::BuildGauge()
					.Name("xapiand_committers_running")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Amount of committers running")
					.Register(*registry)),
	  xapiand_committers_running_met(xapiand_committers_running.Add(std::map<std::string, std::string>())),
	  xapiand_committers_queue(prometheus::BuildGauge()
					.Name("xapiand_committers_queue")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Committers in the queue")
					.Register(*registry)),
	  xapiand_committers_queue_met(xapiand_committers_queue.Add(std::map<std::string, std::string>())),
	  xapiand_committers_capacity(prometheus::BuildGauge()
					.Name("xapiand_committers_capacity")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Committers queue capacity")
					.Register(*registry)),
	  xapiand_committers_capacity_met(xapiand_committers_capacity.Add(std::map<std::string, std::string>())),
	  xapiand_committers_pool_size(prometheus::BuildGauge()
					.Name("xapiand_committers_pool_size")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Server pool size")
					.Register(*registry)),
	  xapiand_committers_pool_size_met(xapiand_committers_pool_size.Add(std::map<std::string, std::string>())),
	  xapiand_fsync_running(prometheus::BuildGauge()
					.Name("xapiand_fsync_running")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Amount of fsync running")
					.Register(*registry)),
	  xapiand_fsync_running_met(xapiand_fsync_running.Add(std::map<std::string, std::string>())),
	  xapiand_fsync_queue(prometheus::BuildGauge()
					.Name("xapiand_fsync_queue")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Fsync in the queue")
					.Register(*registry)),
	  xapiand_fsync_queue_met(xapiand_fsync_queue.Add(std::map<std::string, std::string>())),
	  xapiand_fsync_capacity(prometheus::BuildGauge()
					.Name("xapiand_fsync_capacity")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Fsync queue capacity")
					.Register(*registry)),
	  xapiand_fsync_capacity_met(xapiand_fsync_capacity.Add(std::map<std::string, std::string>())),
	  xapiand_fsync_pool_size(prometheus::BuildGauge()
					.Name("xapiand_fsync_pool_size")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Fsync pool size")
					.Register(*registry)),
	  xapiand_fsync_pool_size_met(xapiand_fsync_pool_size.Add(std::map<std::string, std::string>())),
	  xapiand_http_current_connections(prometheus::BuildGauge()
					.Name("xapiand_http_current_connections")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Current http connections")
					.Register(*registry)),
	  xapiand_http_current_connections_met(xapiand_http_current_connections.Add(std::map<std::string, std::string>())),
	  xapiand_http_peak_connections(prometheus::BuildGauge()
					.Name("xapiand_http_peak_connections")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Max http connections")
					.Register(*registry)),
	  xapiand_http_peak_connections_met(xapiand_http_peak_connections.Add(std::map<std::string, std::string>())),
	  xapiand_file_descriptors(prometheus::BuildGauge()
					.Name("xapiand_file_descriptors")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Amount of file descriptors in use")
					.Register(*registry)),
	  xapiand_file_descriptors_met(xapiand_file_descriptors.Add(std::map<std::string, std::string>())),
	  xapiand_max_file_descriptors(prometheus::BuildGauge()
					.Name("xapiand_max_file_descriptors")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum number of file descriptors")
					.Register(*registry)),
	  xapiand_max_file_descriptors_met(xapiand_max_file_descriptors.Add(std::map<std::string, std::string>())),
	  xapiand_resident_memory_bytes(prometheus::BuildGauge()
					.Name("xapiand_resident_memory_bytes")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Memory in use")
					.Register(*registry)),
	  xapiand_resident_memory_bytes_met(xapiand_resident_memory_bytes.Add(std::map<std::string, std::string>())),
	  xapiand_virtual_memory_bytes(prometheus::BuildGauge()
					.Name("xapiand_virtual_memory_bytes")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Virtual memory in use")
					.Register(*registry)),
	  xapiand_virtual_memory_bytes_met(xapiand_virtual_memory_bytes.Add(std::map<std::string, std::string>())),
	  xapiand_used_memory_bytes(prometheus::BuildGauge()
					.Name("xapiand_used_memory_bytes")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total memory currently allocated")
					.Register(*registry)),
	  xapiand_used_memory_bytes_met(xapiand_used_memory_bytes.Add(std::map<std::string, std::string>())),
	  xapiand_total_memory_system_bytes(prometheus::BuildGauge()
					.Name("xapiand_total_memory_system_bytes")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total memory used")
					.Register(*registry)),
	  xapiand_total_memory_system_bytes_met(xapiand_total_memory_system_bytes.Add(std::map<std::string, std::string>())),
	  xapiand_total_virtual_memory_used(prometheus::BuildGauge()
					.Name("xapiand_total_virtual_memory_used")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total virtual memory used")
					.Register(*registry)),
	  xapiand_total_virtual_memory_used_met(xapiand_total_virtual_memory_used.Add(std::map<std::string, std::string>())),
	  xapiand_readable_db(prometheus::BuildGauge()
					.Name("xapiand_readable_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Readable databases in use")
					.Register(*registry)),
	  xapiand_readable_db_met(xapiand_readable_db.Add(std::map<std::string, std::string>())),
	  xapiand_total_readable_db(prometheus::BuildGauge()
					.Name("xapiand_total_readable_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total readable databases used")
					.Register(*registry)),
	  xapiand_total_readable_db_met(xapiand_total_readable_db.Add(std::map<std::string, std::string>())),
	  xapiand_writable_db(prometheus::BuildGauge()
					.Name("xapiand_writable_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Writable databases in use")
					.Register(*registry)),
	  xapiand_writable_db_met(xapiand_writable_db.Add(std::map<std::string, std::string>())),
	  xapiand_total_writable_db(prometheus::BuildGauge()
					.Name("xapiand_total_writable_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total writable databases used")
					.Register(*registry)),
	  xapiand_total_writable_db_met(xapiand_total_writable_db.Add(std::map<std::string, std::string>())),
	  xapiand_total_db(prometheus::BuildGauge()
					.Name("xapiand_total_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total databases in use")
					.Register(*registry)),
	  xapiand_total_db_met(xapiand_total_db.Add(std::map<std::string, std::string>())),
	  xapiand_total_peak_db(prometheus::BuildGauge()
					.Name("xapiand_total_peak_db")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Total databases used")
					.Register(*registry)),
	  xapiand_total_peak_db_met(xapiand_total_peak_db.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_index(prometheus::BuildGauge()
					.Name("xapiand_max_time_index")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by index (ms)")
					.Register(*registry)),
	  xapiand_max_time_index_met(xapiand_max_time_index.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_search(prometheus::BuildGauge()
					.Name("xapiand_max_time_search")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by search (ms)")
					.Register(*registry)),
	  xapiand_max_time_search_met(xapiand_max_time_search.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_delete(prometheus::BuildGauge()
					.Name("xapiand_max_time_delete")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by delete (ms)")
					.Register(*registry)),
	  xapiand_max_time_delete_met(xapiand_max_time_delete.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_patch(prometheus::BuildGauge()
					.Name("xapiand_max_time_patch")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by patch (ms)")
					.Register(*registry)),
	  xapiand_max_time_patch_met(xapiand_max_time_patch.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_merge(prometheus::BuildGauge()
					.Name("xapiand_max_time_merge")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by merge (ms)")
					.Register(*registry)),
	  xapiand_max_time_merge_met(xapiand_max_time_merge.Add(std::map<std::string, std::string>())),
	  xapiand_max_time_aggregation(prometheus::BuildGauge()
					.Name("xapiand_max_time_aggregation")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Maximum time tooked by aggregation (ms)")
					.Register(*registry)),
	  xapiand_max_time_aggregation_met(xapiand_max_time_aggregation.Add(std::map<std::string, std::string>())),
	  xapiand_index_total(prometheus::BuildGauge()
					.Name("xapiand_index_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of indexing")
					.Register(*registry)),
	  xapiand_index_total_met(xapiand_index_total.Add(std::map<std::string, std::string>())),
	  xapiand_search_total(prometheus::BuildGauge()
					.Name("xapiand_search_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of searching")
					.Register(*registry)),
	  xapiand_search_total_met(xapiand_search_total.Add(std::map<std::string, std::string>())),
	  xapiand_delete_total(prometheus::BuildGauge()
					.Name("xapiand_delete_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of deleting")
					.Register(*registry)),
	  xapiand_delete_total_met(xapiand_delete_total.Add(std::map<std::string, std::string>())),
	  xapiand_patch_total(prometheus::BuildGauge()
					.Name("xapiand_patch_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of patching")
					.Register(*registry)),
	  xapiand_patch_total_met(xapiand_patch_total.Add(std::map<std::string, std::string>())),
	  xapiand_merge_total(prometheus::BuildGauge()
					.Name("xapiand_merge_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of merging")
					.Register(*registry)),
	  xapiand_merge_total_met(xapiand_merge_total.Add(std::map<std::string, std::string>())),
	  xapiand_aggregation_total(prometheus::BuildGauge()
					.Name("xapiand_aggregation_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of search with aggregations")
					.Register(*registry)),
	  xapiand_aggregation_total_met(xapiand_aggregation_total.Add(std::map<std::string, std::string>())),
	  xapiand_commit_total(prometheus::BuildGauge()
					.Name("xapiand_commit_total")
					.Labels({{NODE_LABEL, nodename}, {CLUSTER_LABEL, cluster}})
					.Help("Number of commits")
					.Register(*registry)),
	  xapiand_commit_total_met(xapiand_commit_total.Add(std::map<std::string, std::string>())) { }


XapiandManager::XapiandManager()
	: Worker(nullptr, loop_ref_nil, 0),
	  database_pool(opts.dbpool_size, opts.max_databases),
	  schemas(opts.dbpool_size),
	  thread_pool("W%02zu", opts.threadpool_size),
	  client_pool("C%02zu", opts.tasks_size),
	  server_pool("S%02zu", opts.num_servers),
#ifdef XAPIAND_CLUSTERING
	  replicator_pool("R%02zu", opts.num_replicators),
	  endp_r(opts.endpoints_list_size),
#endif
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0)
{
	std::vector<std::string> values({
		std::to_string(opts.tasks_size) +( (opts.tasks_size == 1) ? " async task" : " async tasks"),
		std::to_string(opts.threadpool_size) +( (opts.threadpool_size == 1) ? " worker thread" : " worker threads"),
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));
}


XapiandManager::XapiandManager(ev::loop_ref* ev_loop_, unsigned int ev_flags_)
	: Worker(nullptr, ev_loop_, ev_flags_),
	  database_pool(opts.dbpool_size, opts.max_databases),
	  schemas(opts.dbpool_size),
	  thread_pool("W%02zu", opts.threadpool_size),
	  client_pool("C%02zu", opts.tasks_size),
	  server_pool("S%02zu", opts.num_servers),
#ifdef XAPIAND_CLUSTERING
	  replicator_pool("R%02zu", opts.num_replicators),
	  endp_r(opts.endpoints_list_size),
#endif
	  shutdown_asap(0),
	  shutdown_now(0),
	  state(State::RESET),
	  node_name(opts.node_name),
	  atom_sig(0),
	  signal_sig_async(*ev_loop)
{
	// Set the id in local node.
	auto local_node_ = local_node.load();
	auto node_copy = std::make_unique<Node>(*local_node_);
	node_copy->id = get_node_id();

	// Setup node from node database directory
	std::string node_name_(load_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && string::lower(node_name) != string::lower(node_name_)) {
			node_name = "~";
		} else {
			node_name = node_name_;
		}
	}
	if (opts.solo) {
		if (node_name.empty()) {
			node_name = name_generator();
		}
	}
	node_copy->name(node_name);

	// Set addr in local node
	node_copy->addr(host_address());

	local_node = std::shared_ptr<const Node>(node_copy.release());

	signal_sig_async.set<XapiandManager, &XapiandManager::signal_sig_async_cb>(this);
	signal_sig_async.start();

	L_OBJ("CREATED XAPIAN MANAGER!");
}


XapiandManager::~XapiandManager()
{
	destroyer();
	join();

	L_OBJ("DELETED XAPIAN MANAGER!");
}


std::string
XapiandManager::load_node_name()
{
	L_CALL("XapiandManager::load_node_name()");

	ssize_t length = 0;
	char buf[512];
	int fd = io::open("nodename", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = io::read(fd, buf, sizeof(buf) - 1);
		io::close(fd);
		if (length < 0) { length = 0; }
		buf[length] = '\0';
		for (size_t i = 0, j = 0; (buf[j] = buf[i]) != 0; j += static_cast<unsigned long>(isspace(buf[i++]) == 0)) { }
	}
	return std::string(buf, length);
}


void
XapiandManager::save_node_name(std::string_view _node_name)
{
	L_CALL("XapiandManager::save_node_name(%s)", _node_name);

	int fd = io::open("nodename", O_WRONLY | O_CREAT, 0644);
	if (fd >= 0) {
		if (io::write(fd, _node_name.data(), _node_name.size()) != static_cast<ssize_t>(_node_name.size())) {
			L_CRIT("Cannot write in nodename file");
			sig_exit(-EX_IOERR);
		}
		io::close(fd);
	} else {
		L_CRIT("Cannot open or create the nodename file");
		sig_exit(-EX_NOINPUT);
	}
}


std::string
XapiandManager::set_node_name(std::string_view node_name_)
{
	L_CALL("XapiandManager::set_node_name(%s)", node_name_);

	node_name = load_node_name();

	if (node_name.empty()) {
		if (!node_name_.empty()) {
			// Ignore empty node_name
			node_name = node_name_;
			save_node_name(node_name);
		}
	}

	return node_name;
}


uint64_t
XapiandManager::load_node_id()
{
	L_CALL("XapiandManager::load_node_id()");

	uint64_t node_id = 0;
	ssize_t length = 0;
	char buf[512];
	int fd = io::open("node", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = io::read(fd, buf, sizeof(buf) - 1);
		io::close(fd);
		if (length < 0) { length = 0; }
		buf[length] = '\0';
		for (size_t i = 0, j = 0; (buf[j] = buf[i]) != 0; j += static_cast<unsigned long>(isspace(buf[i++]) == 0)) { }
		try {
			node_id = unserialise_node_id(std::string_view(buf, length));
		} catch (...) {
			L_CRIT("Cannot load node_id!");
			sig_exit(-EX_IOERR);
		}
	}
	return node_id;
}


void
XapiandManager::save_node_id(uint64_t node_id)
{
	L_CALL("XapiandManager::save_node_id(%llu)", node_id);

	int fd = io::open("node", O_WRONLY | O_CREAT, 0644);
	if (fd >= 0) {
		auto node_id_str = serialise_node_id(node_id);
		if (io::write(fd, node_id_str.data(), node_id_str.size()) != static_cast<ssize_t>(node_id_str.size())) {
			L_CRIT("Cannot write in node file");
			sig_exit(-EX_IOERR);
		}
		io::close(fd);
	} else {
		L_CRIT("Cannot open or create the node file");
		sig_exit(-EX_NOINPUT);
	}
}


uint64_t
XapiandManager::get_node_id()
{
	L_CALL("XapiandManager::get_node_id()");

	uint64_t node_id = load_node_id();

	if (node_id == 0u) {
		node_id = random_int(1, UINT64_MAX - 1);
		save_node_id(node_id);
	}

	return node_id;
}


void
XapiandManager::setup_node()
{
	L_CALL("XapiandManager::setup_node()");

	for (const auto& weak_server : servers_weak) {
		if (auto server = weak_server.lock()) {
			server->setup_node_async.send();
			return;
		}
	}
	L_WARNING("Cannot setup node: No servers!");
}


void
XapiandManager::setup_node(std::shared_ptr<XapiandServer>&& /*server*/)
{
	L_CALL("XapiandManager::setup_node(...)");

	L_DISCOVERY("Setup Node!");

	int new_cluster = 0;

	std::lock_guard<std::mutex> lk(qmtx);

	// Open cluster database
	Endpoints cluster_endpoints(Endpoint("."));

	static const std::string reserved_schema(RESERVED_SCHEMA);
	DatabaseHandler db_handler(cluster_endpoints, DB_WRITABLE | DB_PERSISTENT | DB_NOWAL);
	auto local_node_ = local_node.load();
	try {
		if (db_handler.get_metadata(reserved_schema).empty()) {
			THROW(CheckoutError);
		}
		db_handler.get_document(serialise_node_id(local_node_->id));
	} catch (const CheckoutError&) {
		new_cluster = 1;
		L_INFO("Cluster database doesn't exist. Generating database...");
		try {
			db_handler.reset(cluster_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL);
			db_handler.set_metadata(reserved_schema, Schema::get_initial_schema()->serialise());
			db_handler.index(serialise_node_id(local_node_->id), false, {
				{ RESERVED_INDEX, "field_all" },
				{ ID_FIELD_NAME,  { { RESERVED_TYPE,  KEYWORD_STR } } },
				{ "name",         { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_VALUE, local_node_->name() } } },
				{ "tagline",      { { RESERVED_TYPE,  KEYWORD_STR }, { RESERVED_INDEX, "none" }, { RESERVED_VALUE, XAPIAND_TAGLINE } } },
			}, true, msgpack_type);
		} catch (const CheckoutError&) {
			L_CRIT("Cannot generate cluster database");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (const DocNotFoundError&) {
		L_CRIT("Cluster database is corrupt");
		sig_exit(-EX_DATAERR);
	} catch (const Exception& e) {
		L_CRIT("Exception: %s", e.what());
		sig_exit(-EX_SOFTWARE);
	}

	// Set node as ready!
	node_name = set_node_name(local_node_->lower_name());
	if (string::lower(node_name) != local_node_->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->name(node_name);
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
	}
	L_INFO("Node %s accepted to the party!", node_name);

	{
		// Get a node (any node)
		std::lock_guard<std::mutex> lk_n(nodes_mtx);
		for (const auto & it : nodes) {
			auto& node = it.second;
			Endpoint remote_endpoint(".", node.get());
			// Replicate database from the other node
	#ifdef XAPIAND_CLUSTERING
			if (!opts.solo) {
				L_INFO("Syncing cluster data from %s...", node->name());

				auto ret = trigger_replication(remote_endpoint, cluster_endpoints[0]);
				if (ret.get()) {
					L_INFO("Cluster data being synchronized from %s...", node->name());
					new_cluster = 2;
					break;
				}
			}
	#endif
		}
	}

	state = State::READY;

#ifdef XAPIAND_CLUSTERING
	if (!is_single_node()) {
		if (auto raft = weak_raft.lock()) {
			raft->start();
		}
	}

	if (auto discovery = weak_discovery.lock()) {
		discovery->enter();
	}
#endif

	if (opts.solo) {
		switch (new_cluster) {
			case 0:
				L_NOTICE("Using solo cluster: %s.", opts.cluster_name);
				break;
			case 1:
				L_NOTICE("Using new solo cluster: %s.", opts.cluster_name);
				break;
		}
	} else {
		switch (new_cluster) {
			case 0:
				L_NOTICE("Joined cluster: %s. It is now online!", opts.cluster_name);
				break;
			case 1:
				L_NOTICE("Joined new cluster: %s. It is now online!", opts.cluster_name);
				break;
			case 2:
				L_NOTICE("Joined cluster: %s. It was already online!", opts.cluster_name);
				break;
		}
	}
}


void
XapiandManager::set_request_info()
{
	L_CALL("XapiandManager::set_request_info()");
	if (!req_info) {
		req_info = std::make_unique<Requestinfo>(node_name, opts.cluster_name);
	}
}


struct sockaddr_in
XapiandManager::host_address()
{
	L_CALL("XapiandManager::host_address()");

	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		L_ERR("ERROR: getifaddrs: %s", strerror(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != nullptr; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == AF_INET && ((ifa->ifa_flags & IFF_LOOPBACK) == 0u)) { // check it is IP4
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				L_NOTICE("Node IP address is %s on interface %s", fast_inet_ntop4(addr.sin_addr), ifa->ifa_name);
				break;
			}
		}
		freeifaddrs(if_addr_struct);
	}
	return addr;
}


void
XapiandManager::signal_sig(int sig)
{
	atom_sig = sig;
	signal_sig_async.send();
}


void
XapiandManager::signal_sig_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("XapiandManager::signal_sig_async_cb(<watcher>, 0x%x (%s))", revents, readable_revents(revents));

	ignore_unused(revents);

	int sig = atom_sig;
	switch (sig) {
		case SIGTERM:
		case SIGINT:
			shutdown_sig(sig);
			break;
#if defined(__APPLE__) || defined(__FreeBSD__)
		case SIGINFO:
			print(STEEL_BLUE + "Workers: %s", dump_tree());
			break;
#endif
	}
}


void
XapiandManager::shutdown_sig(int sig)
{
	L_CALL("XapiandManager::shutdown_sig(%d)", sig);

	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	auto now = epoch::now<>();

	if (sig < 0) {
		throw Exit(-sig);
	}
	if ((shutdown_now != 0) && sig != SIGTERM) {
		if ((sig != 0) && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			L_WARNING("You insisted... Xapiand exiting now!");
			throw Exit(1);
		}
	} else if ((shutdown_asap != 0) && sig != SIGTERM) {
		if ((sig != 0) && now > shutdown_asap + 1 && now < shutdown_asap + 4) {
			shutdown_now = now;
			L_INFO("Trying immediate shutdown.");
		} else if (sig == 0) {
			shutdown_now = now;
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

	if (now > shutdown_asap + 1) {
		shutdown_asap = now;
	}

	if (XapiandServer::http_clients <= 0) {
		shutdown_now = now;
	}

	shutdown(shutdown_asap, shutdown_now);
}


void
XapiandManager::destroy_impl()
{
	destroyer();
}


void
XapiandManager::destroyer() {
	L_CALL("XapiandManager::destroyer()");

#ifdef XAPIAND_CLUSTERING
	if (auto discovery = weak_discovery.lock()) {
		L_INFO("Waving goodbye to cluster %s!", opts.cluster_name);
		discovery->stop();
	}
#endif

	finish();
}


void
XapiandManager::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("XapiandManager::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
		break_loop();
	}
}


void
XapiandManager::make_servers()
{
	L_CALL("XapiandManager::make_servers()");

	std::string msg("Listening on ");

	auto http = Worker::make_shared<Http>(XapiandManager::manager, ev_loop, ev_flags, opts.http_port);
	msg += http->getDescription() + ", ";

#ifdef XAPIAND_CLUSTERING
	std::shared_ptr<Binary> binary;
	std::shared_ptr<Discovery> discovery;
	std::shared_ptr<Raft> raft;

	if (!opts.solo) {
		binary = Worker::make_shared<Binary>(XapiandManager::manager, ev_loop, ev_flags, opts.binary_port);
		msg += binary->getDescription() + ", ";

		discovery = Worker::make_shared<Discovery>(XapiandManager::manager, ev_loop, ev_flags, opts.discovery_port, opts.discovery_group);
		msg += discovery->getDescription() + ", ";

		raft = Worker::make_shared<Raft>(XapiandManager::manager, ev_loop, ev_flags, opts.raft_port, opts.raft_group);
		msg += raft->getDescription() + ", ";
	}
#endif

	msg += "at pid:" + std::to_string(getpid()) + " ...";
	L_NOTICE(msg);


	for (ssize_t i = 0; i < opts.num_servers; ++i) {
		std::shared_ptr<XapiandServer> server = Worker::make_shared<XapiandServer>(XapiandManager::manager, nullptr, ev_flags);
		servers_weak.push_back(server);

		Worker::make_shared<HttpServer>(server, server->ev_loop, ev_flags, http);

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			auto binary_server = Worker::make_shared<BinaryServer>(server, server->ev_loop, ev_flags, binary);
			binary->add_server(binary_server);

			Worker::make_shared<DiscoveryServer>(server, server->ev_loop, ev_flags, discovery);

			Worker::make_shared<RaftServer>(server, server->ev_loop, ev_flags, raft);
		}
#endif
		server_pool.enqueue([task = std::move(server)]{
			task->run();
		});
	}

	// Make server protocols weak:
	weak_http = std::move(http);
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		L_INFO("Joining cluster %s...", opts.cluster_name);
		discovery->start();

		weak_binary = std::move(binary);
		weak_discovery = std::move(discovery);
		weak_raft = std::move(raft);
	}
#endif
}


void
XapiandManager::make_replicators()
{
#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		for (ssize_t i = 0; i < opts.num_replicators; ++i) {
			replicator_pool.enqueue([task = Worker::make_shared<XapiandReplicator>(XapiandManager::manager, nullptr, ev_flags)]{
				task->run();
			});
		}
	}
#endif
}


void
XapiandManager::run()
{
	L_CALL("XapiandManager::run()");

	if (node_name == "~") {
		L_CRIT("Node name %s doesn't match with the one in the cluster's database!", opts.node_name);
		join();
		throw Exit(EX_CONFIG);
	}

	make_servers();

	make_replicators();

	DatabaseAutocommit::scheduler(opts.num_committers);

	AsyncFsync::scheduler(opts.num_fsynchers);

	std::vector<std::string> values({
		std::to_string(opts.num_servers) + ((opts.num_servers == 1) ? " server" : " servers"),
		std::to_string(opts.tasks_size) +( (opts.tasks_size == 1) ? " async task" : " async tasks"),
		std::to_string(opts.threadpool_size) +( (opts.threadpool_size == 1) ? " worker thread" : " worker threads"),
#ifdef XAPIAND_CLUSTERING
		opts.solo ? "" : std::to_string(opts.num_replicators) + ((opts.num_replicators == 1) ? " replicator" : " replicators"),
#endif
		std::to_string(opts.num_committers) + ((opts.num_committers == 1) ? " autocommitter" : " autocommitters"),
		std::to_string(opts.num_fsynchers) + ((opts.num_fsynchers == 1) ? " fsyncher" : " fsynchers"),
	});
	L_NOTICE("Started " + string::join(values, ", ", " and ", [](const auto& s) { return s.empty(); }));

	if (opts.solo) {
		setup_node();
	}
	set_request_info();

	try {
		L_EV("Entered manager loop...");
		run_loop();
		L_EV("Manager loop ended!");
	} catch (...) {
		join();
		throw;
	}

	join();

	detach();
	detach();  // detach a second time so it cleans lingering protocols
}


void
XapiandManager::finish()
{
	L_CALL("XapiandManager::finish()");

	L_MANAGER("Finishing servers pool!");
	server_pool.finish();

	L_MANAGER("Finishing client threads pool!");
	client_pool.finish();

#ifdef XAPIAND_CLUSTERING
	L_MANAGER("Finishing replicators pool!");
	replicator_pool.finish();
#endif
}


void
XapiandManager::join()
{
	L_CALL("XapiandManager::join()");

	L_MANAGER("Workers:" STEEL_BLUE "%s", dump_tree());

	finish();

	L_MANAGER("Waiting for %zu server%s...", server_pool.running_size(), (server_pool.running_size() == 1) ? "" : "s");
	server_pool.join();

	L_MANAGER("Waiting for %zu client thread%s...", client_pool.running_size(), (client_pool.running_size() == 1) ? "" : "s");
	client_pool.join();

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		L_MANAGER("Waiting for %zu replicator%s...", replicator_pool.running_size(), (replicator_pool.running_size() == 1) ? "" : "s");
		replicator_pool.join();
	}
#endif

	L_MANAGER("Finishing thread pool!");
	thread_pool.finish();

	L_MANAGER("Waiting for %zu worker thread%s...", thread_pool.running_size(), (thread_pool.running_size() == 1) ? "" : "s");
	thread_pool.join();

	L_MANAGER("Finishing database pool!");
	database_pool.finish();

	L_MANAGER("Finishing autocommitter scheduler!");
	DatabaseAutocommit::finish();

	L_MANAGER("Waiting for %zu autocommitter%s...", DatabaseAutocommit::running_size(), (DatabaseAutocommit::running_size() == 1) ? "" : "s");
	DatabaseAutocommit::join();

	L_MANAGER("Finishing async fsync threads pool!");
	AsyncFsync::finish();

	L_MANAGER("Waiting for %zu async fsync%s...", AsyncFsync::running_size(), (AsyncFsync::running_size() == 1) ? "" : "s");
	AsyncFsync::join();

	L_MANAGER("Server ended!");
}


size_t
XapiandManager::nodes_size()
{
	L_CALL("XapiandManager::nodes_size()");

	std::unique_lock<std::mutex> lk_n(nodes_mtx);
	return nodes.size();
}


bool
XapiandManager::is_single_node()
{
	L_CALL("XapiandManager::is_single_node()");

	return opts.solo || (nodes_size() == 0);
}


#ifdef XAPIAND_CLUSTERING

void
XapiandManager::reset_state()
{
	L_CALL("XapiandManager::reset_state()");

	if (state != State::RESET) {
		state = State::RESET;
		if (auto discovery = weak_discovery.lock()) {
			discovery->start();
		}
	}
}


bool
XapiandManager::put_node(std::shared_ptr<const Node> node)
{
	L_CALL("XapiandManager::put_node(%s)", repr(node->to_string()));

	auto local_node_ = local_node.load();
	if (node->lower_name() == local_node_->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->touched = epoch::now<>();
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
	} else {
		std::lock_guard<std::mutex> lk(nodes_mtx);
		auto it = nodes.find(node->lower_name());
		if (it != nodes.end()) {
			auto& node_ref = it->second;
			if (*node == *node_ref) {
				auto node_copy = std::make_unique<Node>(*node_ref);
				node_copy->touched = epoch::now<>();
				node_ref = std::shared_ptr<const Node>(node_copy.release());
			}
		} else {
			auto node_copy = std::make_unique<Node>(*node);
			node_copy->touched = epoch::now<>();
			nodes[node->lower_name()] = std::shared_ptr<const Node>(node_copy.release());
			return true;
		}
	}
	return false;
}


std::shared_ptr<const Node>
XapiandManager::get_node(std::string_view _node_name)
{
	L_CALL("XapiandManager::get_node(%s)", _node_name);

	std::lock_guard<std::mutex> lk(nodes_mtx);
	auto it = nodes.find(string::lower(_node_name));
	if (it != nodes.end()) {
		return it->second;
	}
	return nullptr;
}


std::shared_ptr<const Node>
XapiandManager::touch_node(std::string_view _node_name, int32_t region)
{
	L_CALL("XapiandManager::touch_node(%s, %x)", _node_name, region);

	auto local_node_ = local_node.load();
	auto lower_node_name = string::lower(_node_name);
	if (lower_node_name == local_node_->lower_name()) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->touched = epoch::now<>();
		if (region != UNKNOWN_REGION) {
			local_node_copy->region = region;
		}
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
		return local_node.load();
	} else {
		std::lock_guard<std::mutex> lk(nodes_mtx);
		auto it = nodes.find(lower_node_name);
		if (it != nodes.end()) {
			auto& node_ref = it->second;
			auto node_ref_copy = std::make_unique<Node>(*node_ref);
			node_ref_copy->touched = epoch::now<>();
			if (region != UNKNOWN_REGION) {
				node_ref_copy->region = region;
			}
			node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
			return node_ref;
		}
	}
	return nullptr;
}


void
XapiandManager::drop_node(std::string_view _node_name)
{
	L_CALL("XapiandManager::drop_node(%s)", _node_name);

	std::lock_guard<std::mutex> lk(nodes_mtx);
	nodes.erase(string::lower(_node_name));
}


size_t
XapiandManager::get_nodes_by_region(int32_t region)
{
	L_CALL("XapiandManager::get_nodes_by_region(%x)", region);

	std::lock_guard<std::mutex> lk(nodes_mtx);
	size_t cnt = 0;
	for (const auto& node : nodes) {
		if (node.second->region == region) ++cnt;
	}
	return cnt;
}


int32_t
XapiandManager::get_region(std::string_view db_name)
{
	L_CALL("XapiandManager::get_region(%s)", db_name);

	bool re_load = false;
	auto local_node_ = local_node.load();
	if (local_node_->regions == -1) {
		auto local_node_copy = std::make_unique<Node>(*local_node_);
		local_node_copy->regions = sqrt(nodes_size());
		local_node = std::shared_ptr<const Node>(local_node_copy.release());
		re_load = true;
	}

	if (re_load) {
		local_node_ = local_node.load();
	}

	std::hash<std::string_view> hash_fn;
	return jump_consistent_hash(hash_fn(db_name), local_node_->regions);
}


int32_t
XapiandManager::get_region()
{
	L_CALL("XapiandManager::get_region()");

	if (auto raft = weak_raft.lock()) {
		auto local_node_ = local_node.load();
		if (local_node_->regions == -1) {
			if (is_single_node()) {
				auto local_node_copy = std::make_unique<Node>(*local_node_);
				local_node_copy->regions = 1;
				local_node_copy->region = 0;
				local_node = std::shared_ptr<const Node>(local_node_copy.release());
				raft->stop();
			} else if (state == State::READY) {
				raft->start();
				auto local_node_copy = std::make_unique<Node>(*local_node_);
				local_node_copy->regions = sqrt(nodes_size() + 1);
				int32_t region = jump_consistent_hash(local_node_copy->id, local_node_copy->regions);
				if (local_node_copy->region != region) {
					local_node_copy->region = region;
					raft->reset();
				}
				local_node = std::shared_ptr<const Node>(local_node_copy.release());
			}
			L_RAFT("Regions: %d Region: %d", local_node_->regions, local_node_->region);
		}
	}

	auto local_node_ = local_node.load();
	return local_node_->region;
}


std::shared_future<bool>
XapiandManager::trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint)
{
	L_CALL("XapiandManager::trigger_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	if (auto binary = weak_binary.lock()) {
		return binary->trigger_replication(src_endpoint, dst_endpoint);
	}
	return std::future<bool>();
}

#endif


bool
XapiandManager::resolve_index_endpoint(const std::string &path, std::vector<Endpoint> &endpv, size_t n_endps, std::chrono::duration<double, std::milli> timeout)
{
	L_CALL("XapiandManager::resolve_index_endpoint(%s, ...)", path);

#ifdef XAPIAND_CLUSTERING
	if (!opts.solo) {
		return endp_r.resolve_index_endpoint(path, endpv, n_endps, timeout);
	}
	else
#else
	ignore_unused(n_endps, timeout);
#endif
	{
		endpv.emplace_back(path);
		return true;
	}
}


void
XapiandManager::server_status(MsgPack& stats)
{
	// clients_tasks:
	auto& stats_clients_tasks = stats["clients_tasks"];
	stats_clients_tasks["running"] = client_pool.running_size();
	stats_clients_tasks["enqueued"] = client_pool.size();
	stats_clients_tasks["capacity"] = client_pool.threadpool_capacity();
	stats_clients_tasks["pool_size"] = client_pool.threadpool_size();

	// servers_threads:
	auto& stats_servers_threads = stats["servers_threads"];
	stats_servers_threads["running"] = server_pool.running_size();
	stats_servers_threads["enqueued"] = server_pool.size();
	stats_servers_threads["capacity"] = server_pool.threadpool_capacity();
	stats_servers_threads["pool_size"] = server_pool.threadpool_size();

	// replicator_threads:
#ifdef XAPIAND_CLUSTERING
	if(!opts.solo) {
		auto& stats_replicator_threads = stats["replicator_threads"];
		stats_replicator_threads["running"] = replicator_pool.running_size();
		stats_replicator_threads["enqueued"] = replicator_pool.size();
		stats_replicator_threads["capacity"] = replicator_pool.threadpool_capacity();
		stats_replicator_threads["pool_size"] = replicator_pool.threadpool_size();
	}
#endif

	// committers_threads:
	auto& stats_committers_threads = stats["committers_threads"];
	stats_committers_threads["running"] = DatabaseAutocommit::running_size();
	stats_committers_threads["enqueued"] = DatabaseAutocommit::size();
	stats_committers_threads["capacity"] = DatabaseAutocommit::threadpool_capacity();
	stats_committers_threads["pool_size"] = DatabaseAutocommit::threadpool_size();

	// fsync_threads:
	auto& stats_fsync_threads = stats["fsync_threads"];
	stats_fsync_threads["running"] = AsyncFsync::running_size();
	stats_fsync_threads["enqueued"] = AsyncFsync::size();
	stats_fsync_threads["capacity"] = AsyncFsync::threadpool_capacity();
	stats_fsync_threads["pool_size"] = AsyncFsync::threadpool_size();

	// connections:
	auto& stats_connections = stats["connections"];

	auto& stats_connections_http = stats_connections["http"];
	stats_connections_http["current"] = XapiandServer::http_clients.load();
	stats_connections_http["peak"] = XapiandServer::max_http_clients.load();

#ifdef XAPIAND_CLUSTERING
	if(!opts.solo) {
		auto& stats_connections_binary = stats_connections["binary"];
		stats_connections_binary["current"] = XapiandServer::binary_clients.load();
		stats_connections_binary["peak"] = XapiandServer::max_binary_clients.load();
	}
#endif

	auto& stats_connections_total = stats_connections["total"];
	stats_connections_total["current"] = XapiandServer::total_clients.load();
	stats_connections_total["peak"] = XapiandServer::max_total_clients.load();

	// file_descriptors:
	stats["file_descriptors"] = file_descriptors_cnt();

	// memory:
	auto& stats_memory = stats["memory"];

	auto& stats_memory_used = stats_memory["used"];
	stats_memory_used["resident"] = string::from_bytes(get_current_memory_by_process());
	stats_memory_used["virtual"] = string::from_bytes(get_current_memory_by_process(false));

	// auto& stats_memory_total_used = stats_memory["total_used"];
	// stats_memory_total_used["resident"] =  string::from_bytes(get_current_ram().first);
	// stats_memory_total_used["virtual"] = string::from_bytes(get_total_virtual_used());

	auto& stats_memory_total = stats_memory["total"];
	stats_memory_total["resident"] =  string::from_bytes(get_total_ram());
	stats_memory_total["virtual"] = string::from_bytes(get_total_virtual_memory());

	// databases:
	auto wdb = database_pool.total_writable_databases();
	auto rdb = database_pool.total_readable_databases();

	auto& stats_databases = stats["databases"];

	auto& stats_databases_readable = stats_databases["readable"];
	stats_databases_readable["endpoints"] = rdb.first;
	stats_databases_readable["total"] = rdb.second;

	auto& stats_databases_writable = stats_databases["writable"];
	stats_databases_writable["endpoints"] = wdb.first;
	stats_databases_writable["total"] = wdb.second;

	auto& stats_databases_total = stats_databases["total"];
	stats_databases_total["endpoints"] = rdb.first + wdb.first;
	stats_databases_total["total"] = rdb.second + wdb.second;
}


std::string
XapiandManager::server_metrics()
{
	L_CALL("XapiandManager::server_metrics()");

	std::vector<std::string> values({
			string::format("Xapian v%d.%d.%d", Xapian::major_version(), Xapian::minor_version(), Xapian::revision()),
#if defined(XAPIAND_V8)
			string::format("V8 v%u.%u", V8_MAJOR_VERSION, V8_MINOR_VERSION),
#endif
#if defined(XAPIAND_CHAISCRIPT)
			string::format("ChaiScript v%d.%d", chaiscript::Build_Info::version_major(), chaiscript::Build_Info::version_minor()),
#endif
	});

	auto short_version = "(" + Package::HASH + ") " + Package::FULLVERSION;
	auto full_version = short_version + " on " + check_architecture() + ", compiled by " + check_compiler() + " (" + check_OS() + ")";

	auto& gauge_xapiand_version = prometheus::BuildGauge()
							.Name("xapiand_info")
							.Labels({{NODE_LABEL, node_name}})
							.Help("Version string as reported by Xapiand")
							.Register(*req_info->registry);
	auto& version_gauge = gauge_xapiand_version.Add({{"short_version", short_version}, {"version", full_version}});
	version_gauge.Set(1);

	// clients_tasks:
	req_info->xapiand_http_clients_run_met.Set(client_pool.running_size());
	req_info->xapiand_http_clients_queue_met.Set(client_pool.size());
	req_info->xapiand_http_clients_capacity_met.Set(client_pool.threadpool_capacity());
	req_info->xapiand_http_clients_pool_size_met.Set(client_pool.threadpool_size());

	// servers_threads:
	req_info->xapiand_servers_run_met.Set(server_pool.running_size());
	req_info->xapiand_servers_run_met.Set(server_pool.threadpool_size());
	req_info->xapiand_servers_queue_met.Set(server_pool.size());
	req_info->xapiand_servers_capacity_met.Set(server_pool.size());

	// committers_threads:
	req_info->xapiand_committers_running_met.Set(DatabaseAutocommit::running_size());
	req_info->xapiand_committers_queue_met.Set(DatabaseAutocommit::running_size());
	req_info->xapiand_committers_capacity_met.Set(DatabaseAutocommit::threadpool_capacity());
	req_info->xapiand_committers_pool_size_met.Set(DatabaseAutocommit::threadpool_capacity());

	// fsync_threads:
	req_info->xapiand_fsync_running_met.Set(AsyncFsync::running_size());
	req_info->xapiand_fsync_queue_met.Set(AsyncFsync::size());
	req_info->xapiand_fsync_capacity_met.Set(AsyncFsync::threadpool_capacity());
	req_info->xapiand_fsync_pool_size_met.Set(AsyncFsync::threadpool_size());

	// connections:
	req_info->xapiand_http_current_connections_met.Set(XapiandServer::http_clients.load());
	req_info->xapiand_http_peak_connections_met.Set(XapiandServer::max_http_clients.load());

	// file_descriptors:
	req_info->xapiand_file_descriptors_met.Set(file_descriptors_cnt());
	req_info->xapiand_max_file_descriptors_met.Set(get_max_files_per_proc());

	// memory:
	req_info->xapiand_resident_memory_bytes_met.Set(get_current_memory_by_process());
	req_info->xapiand_virtual_memory_bytes_met.Set(get_current_memory_by_process(false));
	req_info->xapiand_used_memory_bytes_met.Set(allocator::total_allocated());
	req_info->xapiand_total_memory_system_bytes_met.Set(get_total_ram());
	req_info->xapiand_total_virtual_memory_used_met.Set(get_total_virtual_memory());

	// databases:
	auto wdb = database_pool.total_writable_databases();
	auto rdb = database_pool.total_readable_databases();
	req_info->xapiand_readable_db_met.Set(rdb.first);
	req_info->xapiand_total_readable_db_met.Set(rdb.second);
	req_info->xapiand_writable_db_met.Set(wdb.first);
	req_info->xapiand_total_writable_db_met.Set(wdb.second);
	req_info->xapiand_total_db_met.Set(rdb.first + wdb.first);
	req_info->xapiand_total_peak_db_met.Set(rdb.second + wdb.second);

	return prometheus::detail::SerializeGet(*req_info->registry);
}


void
XapiandManager::get_stats_time(MsgPack& stats, const std::string& time_req, const std::string& gran_req)
{
	std::smatch m;
	if ((time_req.length() != 0u) && std::regex_match(time_req, m, time_re) && static_cast<size_t>(m.length()) == time_req.length()) {
		int start = 0, end = 0, increment = 0;
		start += 60 * 60 * (m.length(1) != 0 ? strict_stoul(m.str(1)) : 0);
		start += 60 * (m.length(2) != 0 ? strict_stoul(m.str(2)) : 0);
		start += (m.length(3) != 0 ? strict_stoul(m.str(3)) : 0);
		end += 60 * 60 * (m.length(5) != 0 ? strict_stoul(m.str(5)) : 0);
		end += 60 * (m.length(6) != 0 ? strict_stoul(m.str(6)) : 0);
		end += (m.length(7) != 0 ? strict_stoul(m.str(7)) : 0);

		if (gran_req.length() != 0u) {
			if (std::regex_match(gran_req, m, time_re) && static_cast<size_t>(m.length()) == gran_req.length() && m.length(4) == 0) {
				increment += 60 * 60 * (m.length(1) != 0 ? strict_stoul(m.str(1)) : 0);
				increment += 60 * (m.length(2) != 0 ? strict_stoul(m.str(2)) : 0);
				increment += (m.length(3) != 0 ? strict_stoul(m.str(3)) : 0);
			} else {
				THROW(ClientError, "Incorrect input: %s", gran_req);
			}
		}

		if (start > end) {
			std::swap(end, start);
		}

		return _get_stats_time(stats, start, end, increment);
	}
	THROW(ClientError, "Incorrect input: %s", time_req);
}


void
XapiandManager::_get_stats_time(MsgPack& stats, int start, int end, int increment)
{
	Stats stats_cnt(Stats::cnt());
	auto current_time = std::chrono::system_clock::to_time_t(stats_cnt.current);

	auto total_inc = end - start;
	if (start >= MAX_TIME_SECOND) {
		return;
	}

	if (total_inc > MAX_TIME_SECOND) {
		total_inc = MAX_TIME_SECOND - start;
		end = MAX_TIME_SECOND;
	}

	int minute = start / 60, second = start % SLOT_TIME_SECOND;
	minute = (minute > stats_cnt.current_pos.minute ? SLOT_TIME_MINUTE + stats_cnt.current_pos.minute : stats_cnt.current_pos.minute) - minute;
	second = (second > stats_cnt.current_pos.second ? SLOT_TIME_SECOND + stats_cnt.current_pos.second : stats_cnt.current_pos.second) - second;

	if (increment == 0) {
		increment = total_inc;
	}

	for (int offset = 0; offset < total_inc;) {
		MsgPack stat;
		// stat["system_time"] = Datetime::iso8601(current_time);
		auto& time_period = stat["period"];

		std::unordered_map<std::string, Stats::Counter::Element> added_counters;
		if (start + offset + increment < SLOT_TIME_SECOND) {
			if (offset + increment > total_inc - 1) {
				increment = total_inc - (offset + 1);
			}
			time_period["start"] = Datetime::iso8601(current_time - (start + offset + increment));
			time_period["end"] = Datetime::iso8601(current_time - (start + offset));
			int end_sec = modulus(second - offset, SLOT_TIME_SECOND);
			int start_sec = modulus(end_sec - increment, SLOT_TIME_SECOND);
			// L_DEBUG("sec: %d..%d (pos.second:%u, offset:%d, increment:%d)", start_sec, end_sec, second, offset, increment);
			stats_cnt._add_stats_sec(start_sec, end_sec, added_counters);
			offset += increment + 1;
		} else {
			if (offset + increment > total_inc - 60) {
				increment = total_inc - (offset + 60);
			}
			time_period["start"] = Datetime::iso8601(current_time - (start + offset + increment));
			time_period["end"] = Datetime::iso8601(current_time - (start + offset));
			int end_min = modulus(minute - offset / 60, SLOT_TIME_MINUTE);
			int start_min = modulus(end_min - increment / 60, SLOT_TIME_MINUTE);
			// L_DEBUG("min: %d..%d (pos.minute:%u, offset:%d, increment:%d)", start_min, end_min, minute, offset, increment);
			stats_cnt._add_stats_min(start_min, end_min, added_counters);
			offset += increment + 60;
		}

		for (auto& counter : added_counters) {
			if (counter.second.cnt != 0u) {
				auto& counter_stats = stat[counter.first];
				counter_stats["cnt"] = counter.second.cnt;
				counter_stats["avg"] = string::from_delta(counter.second.total / counter.second.cnt);
				counter_stats["min"] = string::from_delta(counter.second.min);
				counter_stats["max"] = string::from_delta(counter.second.max);
			}
		}
		stats.push_back(std::move(stat));
	}
}


void
XapiandManager::update_req_info(std::uint64_t duration, RequestType typ)
{
	switch (typ) {
		case RequestType::INDEX:
			req_info->xapiand_index_total_met.Increment();
			if (req_info->xapiand_max_time_index_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_index_met.Set(duration / 1e6);
			}
			break;
		case RequestType::SEARCH:
			req_info->xapiand_search_total_met.Increment();
			if (req_info->xapiand_max_time_search_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_search_met.Set(duration / 1e6);
			}
			break;
		case RequestType::DELETE:
			req_info->xapiand_delete_total_met.Increment();
			if (req_info->xapiand_max_time_delete_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_delete_met.Set(duration / 1e6);
			}
			break;
		case RequestType::PATCH:
			req_info->xapiand_patch_total_met.Increment();
			if (req_info->xapiand_max_time_patch_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_patch_met.Set(duration / 1e6);
			}
			break;
		case RequestType::MERGE:
			req_info->xapiand_merge_total_met.Increment();
			if (req_info->xapiand_max_time_merge_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_merge_met.Set(duration / 1e6);
			}
			break;
		case RequestType::AGGREGATIONS:
			req_info->xapiand_aggregation_total_met.Increment();
			if (req_info->xapiand_max_time_aggregation_met.Value() < (duration / 1e6)) {
				req_info->xapiand_max_time_aggregation_met.Set(duration / 1e6);
			}
			break;
		case RequestType::COMMIT:
			req_info->xapiand_commit_total_met.Increment();
	}
}


#ifdef L_MANAGER_DEFINED
#undef L_MANAGER_DEFINED
#undef L_MANAGER
#endif
