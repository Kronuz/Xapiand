/*
 * Copyright (C) 2018 Dubalu LLC. All rights reserved.
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

#include "metrics.h"

#include "prometheus/serializer.h"            // for Serializer
#include "prometheus/text_serializer.h"       // for text_serializer
#include "prometheus/handler.h"               // for SerializeGet

#include "package.h"                          // for Package::FULLVERSION Package::HASH
#include "utils.h"                            // for check_compiler, check_OS

#define NODE_LABEL "node"
#define CLUSTER_LABEL "cluster"


Metrics::Metrics(const std::string& node_name_, const std::string& cluster_name_)
	: node_name{node_name_},
	  cluster_name{cluster_name_},
	  xapiand_index_summary{
		prometheus::BuildSummary()
			.Name("xapiand_index_summary")
			.Help("Index requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_search_summary{
		prometheus::BuildSummary()
			.Name("xapiand_search_summary")
			.Help("Search requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_delete_summary{
		prometheus::BuildSummary()
			.Name("xapiand_delete_summary")
			.Help("Delete requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_patch_summary{
		prometheus::BuildSummary()
			.Name("xapiand_patch_summary")
			.Help("Patch requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_merge_summary{
		prometheus::BuildSummary()
			.Name("xapiand_merge_summary")
			.Help("Merge requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_aggregation_summary{
		prometheus::BuildSummary()
			.Name("xapiand_aggregation_summary")
			.Help("Aggregation requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_commit_summary{
		prometheus::BuildSummary()
			.Name("xapiand_commit_summary")
			.Help("Commit requests summary")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_uptime{
		prometheus::BuildGauge()
			.Name("xapiand_uptime")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Server uptime in seconds")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_running{
		prometheus::BuildGauge()
			.Name("xapiand_running")
			.Help("If the node is actually running")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_info{
		prometheus::BuildGauge()
			.Name("xapiand_info")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Version string as reported by Xapiand")
			.Register(registry)
			.Add({
				{"name", Package::NAME},
				{"url", Package::URL},
				{"url", Package::URL},
				{"version", Package::VERSION},
				{"full_version", Package::FULLVERSION},
				{"revision", Package::REVISION},
				{"hash", Package::HASH},
				{"compiler", check_compiler()},
				{"os", check_OS()},
				{"arch", check_architecture()},
			})
	  },
	  xapiand_clients_running{
		prometheus::BuildGauge()
			.Name("xapiand_clients_running")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Amount of clients running")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_clients_queue_size{
		prometheus::BuildGauge()
			.Name("xapiand_clients_queue_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Clients in the queue")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_clients_capacity{
		prometheus::BuildGauge()
			.Name("xapiand_clients_capacity")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Client queue capacity")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_clients_pool_size{
		prometheus::BuildGauge()
			.Name("xapiand_clients_pool_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Client total pool size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_servers_running{
		prometheus::BuildGauge()
			.Name("xapiand_servers_running")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Amount of servers running")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_servers_queue_size{
		prometheus::BuildGauge()
			.Name("xapiand_servers_queue_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Servers in the queue")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_servers_capacity{
		prometheus::BuildGauge()
			.Name("xapiand_servers_capacity")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Server queue capacity")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_servers_pool_size{
		prometheus::BuildGauge()
			.Name("xapiand_servers_pool_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Server pool size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_committers_running{
		prometheus::BuildGauge()
			.Name("xapiand_committers_running")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Amount of committers running")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_committers_queue_size{
		prometheus::BuildGauge()
			.Name("xapiand_committers_queue_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Committers in the queue")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_committers_capacity{
		prometheus::BuildGauge()
			.Name("xapiand_committers_capacity")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Committers queue capacity")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_committers_pool_size{
		prometheus::BuildGauge()
			.Name("xapiand_committers_pool_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Server pool size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_fsync_running{
		prometheus::BuildGauge()
			.Name("xapiand_fsync_running")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Amount of fsync running")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_fsync_queue_size{
		prometheus::BuildGauge()
			.Name("xapiand_fsync_queue")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Fsync in the queue")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_fsync_capacity{
		prometheus::BuildGauge()
			.Name("xapiand_fsync_capacity")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Fsync queue capacity")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_fsync_pool_size{
		prometheus::BuildGauge()
			.Name("xapiand_fsync_pool_size")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Fsync pool size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_http_current_connections{
		prometheus::BuildGauge()
			.Name("xapiand_http_current_connections")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Current http connections")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_http_peak_connections{
		prometheus::BuildGauge()
			.Name("xapiand_http_peak_connections")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Max http connections")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_binary_current_connections{
		prometheus::BuildGauge()
			.Name("xapiand_binary_current_connections")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Current binary connections")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_binary_peak_connections{
		prometheus::BuildGauge()
			.Name("xapiand_binary_peak_connections")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Max binary connections")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_file_descriptors{
		prometheus::BuildGauge()
			.Name("xapiand_file_descriptors")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Amount of file descriptors in use")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_max_file_descriptors{
		prometheus::BuildGauge()
			.Name("xapiand_max_file_descriptors")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Maximum number of file descriptors")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_free_inodes{
		prometheus::BuildGauge()
			.Name("xapiand_free_inodes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Free inodes")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_max_inodes{
		prometheus::BuildGauge()
			.Name("xapiand_max_inodes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Maximum inodes")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_resident_memory_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_resident_memory_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Memory in use")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_virtual_memory_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_virtual_memory_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Virtual memory in use")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_used_memory_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_used_memory_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Total memory currently allocated")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_total_memory_system_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_total_memory_system_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Total memory used")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_total_virtual_memory_used{
		prometheus::BuildGauge()
			.Name("xapiand_total_virtual_memory_used")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Total virtual memory used")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_total_disk_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_total_disk_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Total disk size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_free_disk_bytes{
		prometheus::BuildGauge()
			.Name("xapiand_free_disk_bytes")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Free disk size")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_readable_db_queues{
		prometheus::BuildGauge()
			.Name("xapiand_readable_db_queues")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Readable database queues")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_readable_db{
		prometheus::BuildGauge()
			.Name("xapiand_readable_db")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Open databases")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_writable_db_queues{
		prometheus::BuildGauge()
			.Name("xapiand_writable_db_queues")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Writable database queues")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_writable_db{
		prometheus::BuildGauge()
			.Name("xapiand_writable_db")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Open writable databases")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_db_queues{
		prometheus::BuildGauge()
			.Name("xapiand_db_queues")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Total database queues")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  },
	  xapiand_db{
		prometheus::BuildGauge()
			.Name("xapiand_db")
			.Labels({{NODE_LABEL, node_name}, {CLUSTER_LABEL, cluster_name}})
			.Help("Open databases")
			.Register(registry)
			.Add(std::map<std::string, std::string>())
	  }
{
	xapiand_running.Set(1);
	xapiand_info.Set(1);
}


Metrics&
Metrics::metrics(const std::string& new_node_name, const std::string& new_cluster_name)
{
	static auto metrics = std::make_unique<Metrics>(new_node_name, new_cluster_name);
	if (new_node_name != metrics->node_name || new_cluster_name != metrics->cluster_name) {
		metrics = std::make_unique<Metrics>(new_node_name, new_cluster_name);
	}
	return *metrics;
}


std::string
Metrics::serialise()
{
	return prometheus::detail::SerializeGet(registry);
}
