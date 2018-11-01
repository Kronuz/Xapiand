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

#include <string>
#include <cstdint>


extern struct opts_t {
	int verbosity;
	bool detach;
	bool chert;
	bool solo;
	bool strict;
	bool force;
	bool optimal;
	bool foreign;
	bool colors;
	bool no_colors;
	std::string database;
	std::string cluster_name;
	std::string node_name;
	unsigned int http_port;
	unsigned int binary_port;
	unsigned int discovery_port;
	unsigned int raft_port;
	std::string pidfile;
	std::string logfile;
	std::string uid;
	std::string gid;
	std::string discovery_group;
	std::string raft_group;
	ssize_t num_servers;
	ssize_t dbpool_size;
	ssize_t num_updaters;
	ssize_t threadpool_size;
	ssize_t tasks_size;
	ssize_t endpoints_list_size;
	ssize_t num_committers;
	ssize_t num_fsynchers;
	ssize_t max_clients;
	ssize_t max_databases;
	ssize_t max_files;
	int flush_threshold;
	unsigned int ev_flags;
	bool uuid_compact;
	uint32_t uuid_repr;
	bool uuid_partition;
	std::string dump_metadata;
	std::string dump_schema;
	std::string dump_documents;
	std::string restore;
	std::string filename;
} opts;
