/*
 * Copyright (c) 2018,2019 Dubalu LLC
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

#include "package.h"                          // for Package::REVISION_STRING Package::HASH
#include "system.hh"                          // for check_compiler, check_OS


Metrics::Metrics(const std::map<std::string, std::string>& constant_labels_) :
	constant_labels{constant_labels_},
	xapiand_operations_summary{
		registry.AddSummary(
			"xapiand_operations_summary",
			"Operations summary",
			constant_labels)
	},
	xapiand_http_requests_summary{
		registry.AddSummary(
			"xapiand_http_requests_summary",
			"HTTP requests summary",
			constant_labels)
	},
	xapiand_wal_errors{
		registry.AddCounter(
			"xapiand_wal_errors",
			"WAL errors",
			constant_labels)
		.Add({})
	},
	xapiand_uptime{
		registry.AddGauge(
			"xapiand_uptime",
			"Server uptime in seconds",
			constant_labels)
		.Add({})
	},
	xapiand_running{
		registry.AddGauge(
			"xapiand_running",
			"If the node is actually running",
			constant_labels)
		.Add({})
	},
	xapiand_info{
		registry.AddGauge(
			"xapiand_info",
			"Version string as reported by Xapiand",
			constant_labels)
		.Add({
			{"name", Package::NAME},
			{"url", Package::URL},
			{"url", Package::URL},
			{"version", Package::VERSION},
			{"revision", Package::REVISION},
			{"hash", Package::HASH},
			{"compiler", check_compiler()},
			{"os", check_OS()},
			{"arch", check_architecture()},
		})
	},
	xapiand_http_clients_running{
		registry.AddGauge(
			"xapiand_http_clients_running",
			"Number of Http clients running",
			constant_labels)
		.Add({})
	},
	xapiand_http_clients_queue_size{
		registry.AddGauge(
			"xapiand_http_clients_queue_size",
			"Http clients in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_http_clients_capacity{
		registry.AddGauge(
			"xapiand_http_clients_capacity",
			"Http client queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_http_clients_pool_size{
		registry.AddGauge(
			"xapiand_http_clients_pool_size",
			"Http client total pool size",
			constant_labels)
		.Add({})
	},
#ifdef XAPIAND_CLUSTERING
	xapiand_remote_clients_running{
		registry.AddGauge(
			"xapiand_remote_clients_running",
			"Number of remote protocol clients running",
			constant_labels)
		.Add({})
	},
	xapiand_remote_clients_queue_size{
		registry.AddGauge(
			"xapiand_remote_clients_queue_size",
			"Remote protocol clients in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_remote_clients_capacity{
		registry.AddGauge(
			"xapiand_remote_clients_capacity",
			"Remote protocol client queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_remote_clients_pool_size{
		registry.AddGauge(
			"xapiand_clients_pool_size",
			"Remote protocol client total pool size",
			constant_labels)
		.Add({})
	},
	xapiand_replication_clients_running{
		registry.AddGauge(
			"xapiand_replication_clients_running",
			"Number of replication protocol clients running",
			constant_labels)
		.Add({})
	},
	xapiand_replication_clients_queue_size{
		registry.AddGauge(
			"xapiand_replication_clients_queue_size",
			"Replication protocol clients in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_replication_clients_capacity{
		registry.AddGauge(
			"xapiand_replication_clients_capacity",
			"Replication protocol client queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_replication_clients_pool_size{
		registry.AddGauge(
			"xapiand_clients_pool_size",
			"Replication protocol client total pool size",
			constant_labels)
		.Add({})
	},
#endif
	xapiand_servers_running{
		registry.AddGauge(
			"xapiand_servers_running",
			"Amount of servers running",
			constant_labels)
		.Add({})
	},
	xapiand_servers_queue_size{
		registry.AddGauge(
			"xapiand_servers_queue_size",
			"Servers in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_servers_capacity{
		registry.AddGauge(
			"xapiand_servers_capacity",
			"Server queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_servers_pool_size{
		registry.AddGauge(
			"xapiand_servers_pool_size",
			"Server pool size",
			constant_labels)
		.Add({})
	},
	xapiand_committers_running{
		registry.AddGauge(
			"xapiand_committers_running",
			"Amount of committers running",
			constant_labels)
		.Add({})
	},
	xapiand_committers_queue_size{
		registry.AddGauge(
			"xapiand_committers_queue_size",
			"Committers in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_committers_capacity{
		registry.AddGauge(
			"xapiand_committers_capacity",
			"Committers queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_committers_pool_size{
		registry.AddGauge(
			"xapiand_committers_pool_size",
			"Server pool size",
			constant_labels)
		.Add({})
	},
	xapiand_fsync_running{
		registry.AddGauge(
			"xapiand_fsync_running",
			"Amount of fsync running",
			constant_labels)
		.Add({})
	},
	xapiand_fsync_queue_size{
		registry.AddGauge(
			"xapiand_fsync_queue",
			"Fsync in the queue",
			constant_labels)
		.Add({})
	},
	xapiand_fsync_capacity{
		registry.AddGauge(
			"xapiand_fsync_capacity",
			"Fsync queue capacity",
			constant_labels)
		.Add({})
	},
	xapiand_fsync_pool_size{
		registry.AddGauge(
			"xapiand_fsync_pool_size",
			"Fsync pool size",
			constant_labels)
		.Add({})
	},
	xapiand_http_current_connections{
		registry.AddGauge(
			"xapiand_http_current_connections",
			"Current http connections",
			constant_labels)
		.Add({})
	},
	xapiand_http_connections{
		registry.AddCounter(
			"xapiand_http_connections",
			"Bttp connections",
			constant_labels)
		.Add({})
	},
#ifdef XAPIAND_CLUSTERING
	xapiand_remote_current_connections{
		registry.AddGauge(
			"xapiand_remote_current_connections",
			"Current remote protocol connections",
			constant_labels)
		.Add({})
	},
	xapiand_remote_connections{
		registry.AddCounter(
			"xapiand_remote_connections",
			"Remote protocol connections",
			constant_labels)
		.Add({})
	},
	xapiand_replication_current_connections{
		registry.AddGauge(
			"xapiand_replication_current_connections",
			"Current replication connections",
			constant_labels)
		.Add({})
	},
	xapiand_replication_connections{
		registry.AddCounter(
			"xapiand_replication_connections",
			"Replication connections",
			constant_labels)
		.Add({})
	},
#endif
	xapiand_http_sent_bytes{
		registry.AddCounter(
			"xapiand_http_sent_bytes",
			"Bytes sent by http connections",
			constant_labels)
		.Add({})
	},
	xapiand_http_received_bytes{
		registry.AddCounter(
			"xapiand_http_received_bytes",
			"Bytes received by http connections",
			constant_labels)
		.Add({})
	},
	xapiand_replication_sent_bytes{
		registry.AddCounter(
			"xapiand_replication_sent_bytes",
			"Bytes sent by replication connections",
			constant_labels)
		.Add({})
	},
	xapiand_replication_received_bytes{
		registry.AddCounter(
			"xapiand_replication_received_bytes",
			"Bytes received by replication connections",
			constant_labels)
		.Add({})
	},
	xapiand_remote_protocol_sent_bytes{
		registry.AddCounter(
			"xapiand_remote_protocol_sent_bytes",
			"Bytes sent by remote protocol connections",
			constant_labels)
		.Add({})
	},
	xapiand_remote_protocol_received_bytes{
		registry.AddCounter(
			"xapiand_remote_protocol_received_bytes",
			"Bytes received by remote protocol connections",
			constant_labels)
		.Add({})
	},
	xapiand_file_descriptors{
		registry.AddGauge(
			"xapiand_file_descriptors",
			"Amount of file descriptors in use",
			constant_labels)
		.Add({})
	},
	xapiand_max_file_descriptors{
		registry.AddGauge(
			"xapiand_max_file_descriptors",
			"Maximum number of file descriptors",
			constant_labels)
		.Add({})
	},
	xapiand_free_inodes{
		registry.AddGauge(
			"xapiand_free_inodes",
			"Free inodes",
			constant_labels)
		.Add({})
	},
	xapiand_max_inodes{
		registry.AddGauge(
			"xapiand_max_inodes",
			"Maximum inodes",
			constant_labels)
		.Add({})
	},
	xapiand_resident_memory_bytes{
		registry.AddGauge(
			"xapiand_resident_memory_bytes",
			"Memory in use",
			constant_labels)
		.Add({})
	},
	xapiand_virtual_memory_bytes{
		registry.AddGauge(
			"xapiand_virtual_memory_bytes",
			"Virtual memory in use",
			constant_labels)
		.Add({})
	},
#ifdef XAPIAND_TRACKED_MEM
	xapiand_tracked_memory_bytes{
		registry.AddGauge(
			"xapiand_tracked_memory_bytes",
			"Total memory currently allocated",
			constant_labels)
		.Add({})
	},
#endif
	xapiand_total_memory_system_bytes{
		registry.AddGauge(
			"xapiand_total_memory_system_bytes",
			"Total memory used",
			constant_labels)
		.Add({})
	},
	xapiand_total_virtual_memory_used{
		registry.AddGauge(
			"xapiand_total_virtual_memory_used",
			"Total virtual memory used",
			constant_labels)
		.Add({})
	},
	xapiand_total_disk_bytes{
		registry.AddGauge(
			"xapiand_total_disk_bytes",
			"Total disk size",
			constant_labels)
		.Add({})
	},
	xapiand_free_disk_bytes{
		registry.AddGauge(
			"xapiand_free_disk_bytes",
			"Free disk size",
			constant_labels)
		.Add({})
	},
	xapiand_endpoints{
		registry.AddGauge(
			"xapiand_endpoints",
			"Total open endpoints",
			constant_labels)
		.Add({})
	},
	xapiand_databases{
		registry.AddGauge(
			"xapiand_databases",
			"Total open databases",
			constant_labels)
		.Add({})
	}
{
	xapiand_running.Set(1);
	xapiand_info.Set(1);
}


Metrics&
Metrics::metrics(const std::map<std::string, std::string>& constant_labels_)
{
	static Metrics metrics{constant_labels_};
	for (auto& label : constant_labels_) {
		if (metrics.constant_labels[label.first] != label.second) {
			metrics.constant_labels[label.first] = label.second;
		}
	}
	return metrics;
}


std::string
Metrics::serialise()
{
	return prometheus::detail::SerializeGet(registry);
}
