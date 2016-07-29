/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "client_base.h"
#include "database.h"
#include "servers/server_binary.h"


#include <xapian.h>

#include <unordered_map>

class RemoteProtocol;
class Replication;


enum class State {
	INIT,
	REMOTEPROTOCOL_SERVER,
	REPLICATIONPROTOCOL_CLIENT,
	REPLICATIONPROTOCOL_SERVER,
};


// A single instance of a non-blocking Xapiand binary protocol handler
class BinaryClient : public BaseClient {
	std::atomic_int running;

	State state;

	char file_path[PATH_MAX];
	int file_descriptor;

	bool writable;
	int flags;
	std::shared_ptr<Database> database;

	// Buffers that are pending write
	std::string buffer;
	queue::Queue<std::unique_ptr<Buffer>> messages_queue;

	Endpoints repl_endpoints;

	BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double active_timeout_, double idle_timeout_);

	void on_read(const char *buf, size_t received) override;
	void on_read_file(const char *buf, size_t received) override;
	void on_read_file_done() override;

	void checkout_database();
	void checkin_database();

	// Remote protocol:
	std::unique_ptr<RemoteProtocol> remote_protocol;

	// Replication protocol:
	std::unique_ptr<Replication> replication;

	friend Worker;
	friend RemoteProtocol;
	friend Replication;

public:
	std::string __repr__() const override {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "<BinaryClient at %p>", this);
		return buffer;
	}

	~BinaryClient();

	char get_message(std::string &result, char max_type);
	void send_message(char type_as_char, const std::string& message, double end_time=0.0);

	bool init_remote();
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);

	void run() override;
	void _run();
};


#endif  /* XAPIAND_CLUSTERING */
