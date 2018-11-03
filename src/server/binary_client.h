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

#include "config.h"             // for XAPIAND_CLUSTERING


#ifdef XAPIAND_CLUSTERING

#include <deque>                              // for std::deque
#include <memory>                             // for shared_ptr
#include <mutex>                              // for std::mutex
#include <string>                             // for std::string
#include <vector>                             // for std::vector
#include <xapian.h>

#include "base_client.h"                      // for BaseClient
#include "remote_protocol.h"                  // for RemoteProtocol
#include "replication.h"                      // for Replication
#include "threadpool.h"                       // for Task


#define FILE_FOLLOWS '\xfd'


enum class State {
	INIT,
	REMOTEPROTOCOL_SERVER,
	REPLICATIONPROTOCOL_CLIENT,
	REPLICATIONPROTOCOL_SERVER,
	MAX,
};

static inline const std::string& StateNames(State type) {
	static const std::string _[] = {
		"INIT",
		"REMOTEPROTOCOL_SERVER",
		"REPLICATIONPROTOCOL_CLIENT",
		"REPLICATIONPROTOCOL_SERVER",
		"UNKNOWN",
	};
	return _[static_cast<int>(type >= static_cast<State>(0) || type < State::MAX ? type : State::MAX)];
}


// A single instance of a non-blocking Xapiand binary protocol handler
class BinaryClient : public BaseClient {
	std::mutex runner_mutex;

	State state;

	int file_descriptor;
	char file_message_type;
	std::string temp_directory;
	std::string temp_directory_template;
	std::string temp_file_template;
	std::vector<std::string> temp_files;

	// Buffers that are pending write
	std::string buffer;
	std::deque<Buffer> messages;
	bool cluster_database;

	BinaryClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double active_timeout_, double idle_timeout_, bool cluster_database_ = false);

	bool is_idle() override;

	ssize_t on_read(const char *buf, ssize_t received) override;
	void on_read_file(const char *buf, ssize_t received) override;
	void on_read_file_done() override;

	// Remote protocol:
	RemoteProtocol remote_protocol;

	// Replication protocol:
	Replication replication;

	friend Worker;
	friend RemoteProtocol;
	friend Replication;

public:
	std::string __repr__() const override {
		return Worker::__repr__("BinaryClient");
	}

	~BinaryClient();

	char get_message(std::string &result, char max_type);
	void send_message(char type_as_char, const std::string& message);
	void send_file(char type_as_char, int fd);

	bool init_remote();
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);

	void run();
};


#endif  /* XAPIAND_CLUSTERING */
