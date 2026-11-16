/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2018 Dubalu LLC
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

#include "config.h"                // for XAPIAND_TRACKED_MEM

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#include "prometheus/counter.h"
#include "prometheus/family.h"
#include "prometheus/gauge.h"
#include "prometheus/registry.h"
#include "prometheus/summary.h"


class Metrics {
	std::map<std::string, std::string> constant_labels;

	prometheus::Registry registry;

public:
	Metrics(const std::map<std::string, std::string>& constant_labels_);

	static Metrics& metrics(const std::map<std::string, std::string>& constant_labels_ = {});

	static const prometheus::Summary::Quantiles& summary_quantiles();

	// Raises `peak` to `now` when that is more (a compare-exchange loop); serialise() publishes it to its gauge.
	static void raise_peak(std::atomic<std::size_t>& peak, std::size_t now) noexcept;

	std::string serialise();

	prometheus::Family<prometheus::Summary>& xapiand_operations_summary;
	prometheus::Family<prometheus::Summary>& xapiand_http_requests_summary;

	// server info
	prometheus::Counter& xapiand_wal_errors;
	prometheus::Gauge& xapiand_uptime;
	prometheus::Gauge& xapiand_running;
	prometheus::Gauge& xapiand_info;

	// server tasks:
	prometheus::Gauge& xapiand_servers_running;
	prometheus::Gauge& xapiand_servers_queue_size;
	prometheus::Gauge& xapiand_servers_capacity;
	prometheus::Gauge& xapiand_servers_pool_size;

	// autocommit tasks (on the control loop): the watches running and the indexes waiting for a commit
	prometheus::Gauge& xapiand_committers_running;
	prometheus::Gauge& xapiand_committers_queue_size;

	// fsync threads:
	prometheus::Gauge& xapiand_fsync_running;
	prometheus::Gauge& xapiand_fsync_queue_size;
	prometheus::Gauge& xapiand_fsync_capacity;
	prometheus::Gauge& xapiand_fsync_pool_size;

	// connections:
	prometheus::Gauge& xapiand_http_current_connections;
	prometheus::Counter& xapiand_http_connections;
	prometheus::Counter& xapiand_http_requests_cancelled;

	// admission (raised at each admission, docs/mechanisms/cancellation.md):
	std::atomic<std::size_t> admission_inflight_peak{0};
	std::atomic<std::size_t> scheduler_demoted_peak{0};
	prometheus::Gauge& xapiand_admission_inflight_peak;
	prometheus::Gauge& xapiand_scheduler_demoted_peak;

	prometheus::Counter& xapiand_http_sent_bytes;
	prometheus::Counter& xapiand_http_received_bytes;

	// file_descriptors:
	prometheus::Gauge& xapiand_file_descriptors;
	prometheus::Gauge& xapiand_max_file_descriptors;

	// inodes:
	prometheus::Gauge& xapiand_free_inodes;
	prometheus::Gauge& xapiand_max_inodes;

	// memory:
	prometheus::Gauge& xapiand_resident_memory_bytes;
	prometheus::Gauge& xapiand_virtual_memory_bytes;
#ifdef XAPIAND_TRACKED_MEM
	prometheus::Gauge& xapiand_tracked_memory_bytes;
#endif
	prometheus::Gauge& xapiand_total_memory_system_bytes;
	prometheus::Gauge& xapiand_total_virtual_memory_used;
	prometheus::Gauge& xapiand_total_disk_bytes;
	prometheus::Gauge& xapiand_free_disk_bytes;

	// databases:
	prometheus::Gauge& xapiand_endpoints;
	prometheus::Gauge& xapiand_databases;
	prometheus::Gauge& xapiand_search_pinned_indexes;
	prometheus::Gauge& xapiand_search_pinned_indexes_peak;
};
