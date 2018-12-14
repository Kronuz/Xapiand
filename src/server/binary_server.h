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

#include "base_server.h"                      // for BaseServer


class Binary;
class Endpoint;
struct TriggerReplicationArgs;


// Binary Server
class BinaryServer : public MetaBaseServer<BinaryServer> {
	Binary& binary;

	ev::async trigger_replication_async;

	void start_impl() override;

	void trigger_replication_async_cb(ev::async& watcher, int revents);

public:
	BinaryServer(const std::shared_ptr<Binary>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_);
	~BinaryServer() noexcept;

	int accept();

	void io_accept_cb(ev::io& watcher, int revents);

	void trigger_replication(const TriggerReplicationArgs& args);

	void trigger_replication();

	std::string __repr__() const override;
};


#endif /* XAPIAND_CLUSTERING */
