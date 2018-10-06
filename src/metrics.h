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

#pragma once

#include "xapiand.h"

#include <string>
#include <memory>

#include "prometheus/registry.h"


class Metrics {
	std::string node_name;
	std::string cluster_name;

	prometheus::Registry registry;

public:
	Metrics(const std::string& node_name_, const std::string& cluster_name_);
	~Metrics() = default;

	static Metrics& metrics(const std::string& new_node_name = "", const std::string& new_cluster_name = "");

	std::string serialise();

	prometheus::Summary& xapiand_index_summary;
	prometheus::Summary& xapiand_search_summary;
	prometheus::Summary& xapiand_delete_summary;
	prometheus::Summary& xapiand_patch_summary;
	prometheus::Summary& xapiand_merge_summary;
	prometheus::Summary& xapiand_aggregation_summary;
	prometheus::Summary& xapiand_commit_summary;

	prometheus::Gauge& xapiand_uptime;
	prometheus::Gauge& xapiand_running;
	prometheus::Gauge& xapiand_info;

	// clients_tasks:
	prometheus::Gauge& xapiand_clients_running;
	prometheus::Gauge& xapiand_clients_queue_size;
	prometheus::Gauge& xapiand_clients_capacity;
	prometheus::Gauge& xapiand_clients_pool_size;

	// server_tasks:
	prometheus::Gauge& xapiand_servers_running;
	prometheus::Gauge& xapiand_servers_queue_size;
	prometheus::Gauge& xapiand_servers_capacity;
	prometheus::Gauge& xapiand_servers_pool_size;

	// committers_threads:
	prometheus::Gauge& xapiand_committers_running;
	prometheus::Gauge& xapiand_committers_queue_size;
	prometheus::Gauge& xapiand_committers_capacity;
	prometheus::Gauge& xapiand_committers_pool_size;

	// fsync_threads:
	prometheus::Gauge& xapiand_fsync_running;
	prometheus::Gauge& xapiand_fsync_queue_size;
	prometheus::Gauge& xapiand_fsync_capacity;
	prometheus::Gauge& xapiand_fsync_pool_size;

	// connections:
	prometheus::Gauge& xapiand_http_current_connections;
	prometheus::Gauge& xapiand_http_peak_connections;

	prometheus::Gauge& xapiand_binary_current_connections;
	prometheus::Gauge& xapiand_binary_peak_connections;

	// file_descriptors:
	prometheus::Gauge& xapiand_file_descriptors;
	prometheus::Gauge& xapiand_max_file_descriptors;

	// inodes:
	prometheus::Gauge& xapiand_free_inodes;
	prometheus::Gauge& xapiand_max_inodes;

	// memory:
	prometheus::Gauge& xapiand_resident_memory_bytes;
	prometheus::Gauge& xapiand_virtual_memory_bytes;
	prometheus::Gauge& xapiand_used_memory_bytes;
	prometheus::Gauge& xapiand_total_memory_system_bytes;
	prometheus::Gauge& xapiand_total_virtual_memory_used;
	prometheus::Gauge& xapiand_total_disk_bytes;
	prometheus::Gauge& xapiand_free_disk_bytes;

	// databases:
	prometheus::Gauge& xapiand_readable_db_queues;
	prometheus::Gauge& xapiand_readable_db;
	prometheus::Gauge& xapiand_writable_db_queues;
	prometheus::Gauge& xapiand_writable_db;
	prometheus::Gauge& xapiand_db_queues;
	prometheus::Gauge& xapiand_db;
};
