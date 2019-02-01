/*
 * Copyright (c) 2015-2018 Dubalu LLC
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
#include <xapian.h>

#include "base_client.h"                    // for MetaBaseClient
#include "lock_database.h"                  // for LockableDatabase
#include "threadpool.hh"                    // for Task


// #define SAVE_LAST_MESSAGES
#if !defined(NDEBUG) || defined(XAPIAND_TRACEBACKS)
#ifndef SAVE_LAST_MESSAGES
#define SAVE_LAST_MESSAGES 1
#endif
#endif


#define FILE_FOLLOWS '\xfd'


enum class ReplicaState {
	INIT_REPLICATION_CLIENT,
	INIT_REPLICATION_SERVER,
	REPLICATION_CLIENT,
	REPLICATION_SERVER,
};


inline const std::string& StateNames(ReplicaState type) {
	static const std::string _[] = {
		"INIT_REPLICATION_CLIENT",
		"INIT_REPLICATION_SERVER",
		"REPLICATION_CLIENT",
		"REPLICATION_SERVER",
	};
	auto idx = static_cast<size_t>(type);
	if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
		return _[idx];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


enum class ReplicationMessageType {
	MSG_GET_CHANGESETS,
	MSG_MAX
};


inline const std::string& ReplicationMessageTypeNames(ReplicationMessageType type) {
	static const std::string _[] = {
		"MSG_GET_CHANGESETS",
	};
	auto idx = static_cast<size_t>(type);
	if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
		return _[idx];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


enum class ReplicationReplyType {
	REPLY_WELCOME,              // Welcome message (same as Remote Protocol's REPLY_UPDATE)
	REPLY_END_OF_CHANGES,       // No more changes to transfer
	REPLY_FAIL,                 // Couldn't generate full set of changes
	REPLY_DB_HEADER,            // The start of a whole DB copy
	REPLY_DB_FILENAME,          // The name of a file in a DB copy
	REPLY_DB_FILEDATA,          // Contents of a file in a DB copy
	REPLY_DB_FOOTER,            // End of a whole DB copy
	REPLY_CHANGESET,            // A changeset file is being sent
	REPLY_MAX
};


inline const std::string& ReplicationReplyTypeNames(ReplicationReplyType type) {
	static const std::string _[] = {
		"REPLY_WELCOME",
		"REPLY_GET_CHANGESETS",
		"REPLY_END_OF_CHANGES", "REPLY_FAIL",
		"REPLY_DB_HEADER", "REPLY_DB_FILENAME", "REPLY_DB_FILEDATA", "REPLY_DB_FOOTER",
		"REPLY_CHANGESET",
	};
	auto idx = static_cast<size_t>(type);
	if (idx >= 0 && idx < sizeof(_) / sizeof(_[0])) {
		return _[idx];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


class DatabaseWAL;


// A single instance of a non-blocking Xapiand replication protocol handler
class ReplicationProtocolClient : public MetaBaseClient<ReplicationProtocolClient>, public LockableDatabase {
	friend MetaBaseClient<ReplicationProtocolClient>;

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

	ReplicationProtocolClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double active_timeout_, double idle_timeout_, bool cluster_database_ = false);

	bool is_idle() const;

	ssize_t on_read(const char *buf, ssize_t received);
	void on_read_file(const char *buf, ssize_t received);
	void on_read_file_done();

	friend Worker;

public:
	Endpoints src_endpoints;

	lock_database lk_db;

	std::string switch_database_path;
	std::shared_ptr<Database> switch_database;

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
