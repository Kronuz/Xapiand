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

#include "config.h"                           // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include <memory>                             // for std::shared_ptr, std::weak_ptr
#include <string>                             // for std::string
#include <vector>                             // for std::vector

#include "tcp.h"                              // for BaseTCP
#include "threadpool.hh"                      // for TaskQueue


class Endpoint;
class BinaryServer;
class DiscoveryServer;

// Configuration data for Binary
class Binary : public BaseTCP {
	friend BinaryServer;

	std::mutex bsmtx;
	std::vector<std::weak_ptr<BinaryServer>> servers_weak;
	TaskQueue<void(const std::shared_ptr<BinaryServer>&)> tasks;

	void process_tasks();

public:
	std::string __repr__() const override {
		return Worker::__repr__("Binary");
	}

	Binary(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port);

	std::string getDescription() const noexcept override;

	int connection_socket();

	void add_server(const std::shared_ptr<BinaryServer>& server);
	void start();

	void trigger_replication(const Endpoint& src_endpoint, const Endpoint& dst_endpoint, bool cluster_database);
};


#endif  /* XAPIAND_CLUSTERING */
