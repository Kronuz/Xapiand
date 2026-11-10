/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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


#include "prometheus/text_serializer.h"       // for text_serializer

#include "package.h"                          // for Package::REVISION_STRING Package::HASH
#include "system.hh"                          // for check_compiler, check_OS, check_architecture


namespace {

prometheus::Family<prometheus::Counter>&
AddCounter(prometheus::Registry& registry, const std::string& name, const std::string& help, const prometheus::Labels& labels)
{
	return prometheus::BuildCounter().Name(name).Help(help).Labels(labels).Register(registry);
}


prometheus::Family<prometheus::Gauge>&
AddGauge(prometheus::Registry& registry, const std::string& name, const std::string& help, const prometheus::Labels& labels)
{
	return prometheus::BuildGauge().Name(name).Help(help).Labels(labels).Register(registry);
}


prometheus::Family<prometheus::Summary>&
AddSummary(prometheus::Registry& registry, const std::string& name, const std::string& help, const prometheus::Labels& labels)
{
	return prometheus::BuildSummary().Name(name).Help(help).Labels(labels).Register(registry);
}

}


const prometheus::Summary::Quantiles&
Metrics::summary_quantiles()
{
	static const prometheus::Summary::Quantiles quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}};
	return quantiles;
}


Metrics::Metrics(const std::map<std::string, std::string>& constant_labels_) :
	constant_labels(constant_labels_),
	xapiand_operations_summary(
		AddSummary(registry,
			"xapiand_operations_summary",
			"Operations summary",
			constant_labels)
	),
	xapiand_http_requests_summary(
		AddSummary(registry,
			"xapiand_http_requests_summary",
			"HTTP requests summary",
			constant_labels)
	),
	xapiand_wal_errors(
		AddCounter(registry,
			"xapiand_wal_errors",
			"WAL errors",
			constant_labels)
		.Add({})
	),
	xapiand_uptime(
		AddGauge(registry,
			"xapiand_uptime",
			"Server uptime in seconds",
			constant_labels)
		.Add({})
	),
	xapiand_running(
		AddGauge(registry,
			"xapiand_running",
			"If the node is actually running",
			constant_labels)
		.Add({})
	),
	xapiand_info(
		AddGauge(registry,
			"xapiand_info",
			"Version string as reported by Xapiand",
			constant_labels)
		.Add({
			{ "name", Package::NAME },
			{ "url", Package::URL },
			{ "issues", Package::BUGREPORT },
			{ "version", Package::VERSION },
			{ "revision", Package::REVISION },
			{ "hash", Package::HASH },
			{ "compiler", check_compiler() },
			{ "os", check_OS() },
			{ "arch", check_architecture() },
		})
	),
	xapiand_servers_running(
		AddGauge(registry,
			"xapiand_servers_running",
			"Amount of servers running",
			constant_labels)
		.Add({})
	),
	xapiand_servers_queue_size(
		AddGauge(registry,
			"xapiand_servers_queue_size",
			"Servers in the queue",
			constant_labels)
		.Add({})
	),
	xapiand_servers_capacity(
		AddGauge(registry,
			"xapiand_servers_capacity",
			"Server queue capacity",
			constant_labels)
		.Add({})
	),
	xapiand_servers_pool_size(
		AddGauge(registry,
			"xapiand_servers_pool_size",
			"Server pool size",
			constant_labels)
		.Add({})
	),
	xapiand_committers_running(
		AddGauge(registry,
			"xapiand_committers_running",
			"Amount of committers running",
			constant_labels)
		.Add({})
	),
	xapiand_committers_queue_size(
		AddGauge(registry,
			"xapiand_committers_queue_size",
			"Committers in the queue",
			constant_labels)
		.Add({})
	),
	xapiand_fsync_running(
		AddGauge(registry,
			"xapiand_fsync_running",
			"Amount of fsync running",
			constant_labels)
		.Add({})
	),
	xapiand_fsync_queue_size(
		AddGauge(registry,
			"xapiand_fsync_queue",
			"Fsync in the queue",
			constant_labels)
		.Add({})
	),
	xapiand_fsync_capacity(
		AddGauge(registry,
			"xapiand_fsync_capacity",
			"Fsync queue capacity",
			constant_labels)
		.Add({})
	),
	xapiand_fsync_pool_size(
		AddGauge(registry,
			"xapiand_fsync_pool_size",
			"Fsync pool size",
			constant_labels)
		.Add({})
	),
	xapiand_http_current_connections(
		AddGauge(registry,
			"xapiand_http_current_connections",
			"Current http connections",
			constant_labels)
		.Add({})
	),
	xapiand_http_connections(
		AddCounter(registry,
			"xapiand_http_connections",
			"Bttp connections",
			constant_labels)
		.Add({})
	),
	xapiand_http_requests_cancelled(
		AddCounter(registry,
			"xapiand_http_requests_cancelled",
			"Requests a cancel stopped: the client left, or the node is stopping",
			constant_labels)
		.Add({})
	),
	xapiand_admission_inflight_peak(
		AddGauge(registry,
			"xapiand_admission_inflight_peak",
			"Most requests one reactor held in flight at once",
			constant_labels)
		.Add({})
	),
	xapiand_scheduler_demoted_peak(
		AddGauge(registry,
			"xapiand_scheduler_demoted_peak",
			"Most demoted handles a request found on its reactor at its admission",
			constant_labels)
		.Add({})
	),
	xapiand_http_sent_bytes(
		AddCounter(registry,
			"xapiand_http_sent_bytes",
			"Bytes sent by http connections",
			constant_labels)
		.Add({})
	),
	xapiand_http_received_bytes(
		AddCounter(registry,
			"xapiand_http_received_bytes",
			"Bytes received by http connections",
			constant_labels)
		.Add({})
	),
	xapiand_file_descriptors(
		AddGauge(registry,
			"xapiand_file_descriptors",
			"Amount of file descriptors in use",
			constant_labels)
		.Add({})
	),
	xapiand_max_file_descriptors(
		AddGauge(registry,
			"xapiand_max_file_descriptors",
			"Maximum number of file descriptors",
			constant_labels)
		.Add({})
	),
	xapiand_free_inodes(
		AddGauge(registry,
			"xapiand_free_inodes",
			"Free inodes",
			constant_labels)
		.Add({})
	),
	xapiand_max_inodes(
		AddGauge(registry,
			"xapiand_max_inodes",
			"Maximum inodes",
			constant_labels)
		.Add({})
	),
	xapiand_resident_memory_bytes(
		AddGauge(registry,
			"xapiand_resident_memory_bytes",
			"Memory in use",
			constant_labels)
		.Add({})
	),
	xapiand_virtual_memory_bytes(
		AddGauge(registry,
			"xapiand_virtual_memory_bytes",
			"Virtual memory in use",
			constant_labels)
		.Add({})
	),
#ifdef XAPIAND_TRACKED_MEM
	xapiand_tracked_memory_bytes(
		AddGauge(registry,
			"xapiand_tracked_memory_bytes",
			"Total memory currently allocated",
			constant_labels)
		.Add({})
	),
#endif
	xapiand_total_memory_system_bytes(
		AddGauge(registry,
			"xapiand_total_memory_system_bytes",
			"Total memory used",
			constant_labels)
		.Add({})
	),
	xapiand_total_virtual_memory_used(
		AddGauge(registry,
			"xapiand_total_virtual_memory_used",
			"Total virtual memory used",
			constant_labels)
		.Add({})
	),
	xapiand_total_disk_bytes(
		AddGauge(registry,
			"xapiand_total_disk_bytes",
			"Total disk size",
			constant_labels)
		.Add({})
	),
	xapiand_free_disk_bytes(
		AddGauge(registry,
			"xapiand_free_disk_bytes",
			"Free disk size",
			constant_labels)
		.Add({})
	),
	xapiand_endpoints(
		AddGauge(registry,
			"xapiand_endpoints",
			"Total open endpoints",
			constant_labels)
		.Add({})
	),
	xapiand_databases(
		AddGauge(registry,
			"xapiand_databases",
			"Total open databases",
			constant_labels)
		.Add({})
	),
	xapiand_search_pinned_indexes(
		AddGauge(registry,
			"xapiand_search_pinned_indexes",
			"Indexes searches hold open now",
			constant_labels)
		.Add({})
	),
	xapiand_search_pinned_indexes_peak(
		AddGauge(registry,
			"xapiand_search_pinned_indexes_peak",
			"Most indexes searches have held open at once",
			constant_labels)
		.Add({})
	)
{
	xapiand_running.Set(1);
	xapiand_info.Set(1);
}


void
Metrics::raise_peak(std::atomic<std::size_t>& peak, std::size_t now) noexcept
{
	auto most = peak.load(std::memory_order_relaxed);
	while (most < now && !peak.compare_exchange_weak(most, now, std::memory_order_relaxed)) { }
}


Metrics&
Metrics::metrics(const std::map<std::string, std::string>& constant_labels_)
{
	static Metrics metrics(constant_labels_);
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
	xapiand_admission_inflight_peak.Set(static_cast<double>(admission_inflight_peak.load(std::memory_order_relaxed)));
	xapiand_scheduler_demoted_peak.Set(static_cast<double>(scheduler_demoted_peak.load(std::memory_order_relaxed)));
	auto collected = registry.Collect();
	prometheus::TextSerializer serializer;
	return serializer.Serialize(collected);
}
