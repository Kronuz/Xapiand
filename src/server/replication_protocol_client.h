/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"                         // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include <deque>                            // for std::deque
#include <memory>                           // for shared_ptr
#include <mutex>                            // for std::mutex
#include <string>                           // for std::string
#include <vector>                           // for std::vector

#include "base_client.h"                    // for BaseClient
#include "endpoint.h"                       // for Endpoint
#include "enum.h"                           // for ENUM_CLASS
#include "threadpool.hh"                    // for Task
#include "xapian.h"


// #define SAVE_LAST_MESSAGES
#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)
#ifndef SAVE_LAST_MESSAGES
#define SAVE_LAST_MESSAGES 1
#endif
#endif


#define FILE_FOLLOWS '\xfd'


ENUM_CLASS(ReplicaState, int,
	INIT_REPLICATION_CLIENT,
	INIT_REPLICATION_SERVER,
	REPLICATION_CLIENT,
	REPLICATION_SERVER
)


ENUM_CLASS(ReplicationMessageType, int,
	MSG_GET_CHANGESETS,
	MSG_MAX
)


ENUM_CLASS(ReplicationReplyType, int,
	REPLY_WELCOME,              // Welcome message (same as Remote Protocol's REPLY_UPDATE)
	REPLY_EXCEPTION,            // Exception
	REPLY_END_OF_CHANGES,       // No more changes to transfer
	REPLY_FAIL,                 // Couldn't generate full set of changes
	REPLY_DB_HEADER,            // The start of a whole DB copy
	REPLY_DB_FILENAME,          // The name of a file in a DB copy
	REPLY_DB_FILEDATA,          // Contents of a file in a DB copy
	REPLY_DB_FOOTER,            // End of a whole DB copy
	REPLY_CHANGESET,            // A changeset file is being sent
	REPLY_MAX
)


class Shard;
class DatabaseWAL;
class lock_shard;


// A single instance of a non-blocking Xapiand replication protocol handler
class ReplicationProtocolClient : public BaseClient<ReplicationProtocolClient> {
	friend BaseClient<ReplicationProtocolClient>;

	mutable std::mutex runner_mutex;

	std::atomic<ReplicaState> state;

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

	ReplicationProtocolClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, double active_timeout_, double idle_timeout_, bool cluster_database_ = false);

	size_t pending_messages() const;
	bool is_idle() const;

	void destroy_impl() override;

	ssize_t on_read(const char *buf, ssize_t received);
	void on_read_file(const char *buf, ssize_t received);
	void on_read_file_done();

	friend Worker;

public:
	std::unique_ptr<lock_shard> lk_shard_ptr;

	std::string switch_shard_path;
	std::shared_ptr<Shard> switch_shard;

	std::unique_ptr<DatabaseWAL> wal;

	std::string file_path;

	std::string current_uuid;
	Xapian::rev current_revision;

	size_t changesets;
	std::shared_ptr<Logging> log;

	~ReplicationProtocolClient() noexcept;

	void reset();

	bool init_replication_protocol(const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept;

	void send_message(ReplicationReplyType type, const std::string& message);
	void send_file(ReplicationReplyType type, int fd);

	void replication_server(ReplicationMessageType type, const std::string& message);
	void replication_client(ReplicationReplyType type, const std::string& message);

	void msg_get_changesets(const std::string& message);
	void reply_welcome(const std::string& message);
	void reply_exception(const std::string& message);
	void reply_end_of_changes(const std::string& message);
	void reply_fail(const std::string& message);
	void reply_db_header(const std::string& message);
	void reply_db_filename(const std::string& message);
	void reply_db_filedata(const std::string& message);
	void reply_db_footer(const std::string& message);
	void reply_changeset(const std::string& message);

	char get_message(std::string &result, char max_type);
	void send_message(char type_as_char, const std::string& message);
	void send_file(char type_as_char, int fd);

	bool init_replication() noexcept;
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept;

	void operator()();

	std::string __repr__() const override;
};

#endif  /* XAPIAND_CLUSTERING */
