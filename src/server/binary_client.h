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

#include "base_client.h"                      // for MetaBaseClient
#include "remote_protocol.h"                  // for RemoteProtocol
#include "replication_protocol.h"             // for ReplicationProtocol
#include "threadpool.hh"                      // for Task

// #define SAVE_LAST_MESSAGES
#ifndef NDEBUG
#ifndef SAVE_LAST_MESSAGES
#define SAVE_LAST_MESSAGES 1
#endif
#endif

enum class State {
	INIT_REMOTE,
	INIT_REPLICATION,
	REMOTE_SERVER,
	REPLICATION_CLIENT,
	REPLICATION_SERVER,
};

#define FILE_FOLLOWS '\xfd'

inline const std::string& StateNames(State type) {
	static const std::string _[] = {
		"INIT_REMOTE",
		"INIT_REPLICATION",
		"REMOTE_SERVER",
		"REPLICATION_CLIENT",
		"REPLICATION_SERVER",
	};
	auto type_int = static_cast<size_t>(type);
	if (type_int >= 0 || type_int < sizeof(_) / sizeof(_[0])) {
		return _[type_int];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


// A single instance of a non-blocking Xapiand binary protocol handler
class BinaryClient : public MetaBaseClient<BinaryClient> {
	friend MetaBaseClient<BinaryClient>;

	mutable std::mutex runner_mutex;

	State state;

#ifdef SAVE_LAST_MESSAGES
	std::atomic_char last_message_received;
	std::atomic_char last_message_sent;
#endif

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

	bool is_idle() const;

	ssize_t on_read(const char *buf, ssize_t received);
	void on_read_file(const char *buf, ssize_t received);
	void on_read_file_done();

	// Remote protocol:
	RemoteProtocol remote_protocol;

	// Replication protocol:
	ReplicationProtocol replication_protocol;

	friend Worker;
	friend RemoteProtocol;
	friend ReplicationProtocol;

public:
	~BinaryClient();

	char get_message(std::string &result, char max_type);
	void send_message(char type_as_char, const std::string& message);
	void send_file(char type_as_char, int fd);

	bool init_remote() noexcept;
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept;

	void operator()();

	std::string __repr__() const override;
};


#endif  /* XAPIAND_CLUSTERING */
