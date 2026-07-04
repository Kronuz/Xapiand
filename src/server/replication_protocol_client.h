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

#include <atomic>                           // for std::atomic_char
#include <memory>                           // for shared_ptr
#include <string>                           // for std::string
#include <utility>                          // for std::pair
#include <vector>                           // for std::vector

#include "endpoint.h"                       // for Endpoint
#include "enum.h"                           // for ENUM_CLASS
#include "xapian.h"                         // for Xapian::rev


// #define SAVE_LAST_MESSAGES
#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)
#ifndef SAVE_LAST_MESSAGES
#define SAVE_LAST_MESSAGES 1
#endif
#endif


#define FILE_FOLLOWS '\xfd'


ENUM_CLASS(ReplicationMessageType, int,
	MSG_GET_CHANGESETS,
	MSG_SET_REVISION,
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
	REPLY_DONE,                 // Finish
	REPLY_MAX
)


class Logging;
class Shard;
class DatabaseWAL;
class lock_shard;


// The per-connection Xapiand replication protocol handler. Transport-agnostic: it consumes
// requests via replication_server()/replication_client() (the blocking work) and its handlers
// write framed replies + stream whole DB files DIRECTLY to the socket fd (send_file streams in
// bounded memory via flume, blocking the reactor pool thread it runs on -- the coroutine is
// suspended awaiting the offload, so nothing races the socket). No libev / Worker / thread
// pool. Two roles, driven by the coroutine (replication_protocol_service.h): an accepted inbound
// connection (server: greet, answer MSG_GET_CHANGESETS/MSG_SET_REVISION) and an outbound
// connection (client: fetch changesets from a primary).
class ReplicationProtocolClient {
#ifdef SAVE_LAST_MESSAGES
	std::atomic_char last_message_received;
	std::atomic_char last_message_sent;
#endif

	// FILE_FOLLOWS receive bookkeeping: temp files the coroutine streams an incoming file
	// into, then dispatches with the temp path as the message body.
	std::string temp_directory;
	std::string temp_directory_template;
	std::string temp_file_template;
	std::vector<std::string> temp_files;

	bool cluster_database;

	// The socket fd the handlers write replies/files to (set by the coroutine) + the
	// "close this connection" flag (the handlers' detach()/destroy()/close() set it).
	int sock_fd_;
	bool closing_;

	// Running count of bytes written on this connection (the classic BaseClient counter),
	// read by the handlers for the "SENDING ...: N bytes" progress logs.
	std::size_t total_sent_bytes;

	void send_message(char type_as_char, const std::string& message);
	void send_file(char type_as_char, int fd);

	void destroy() noexcept { closing_ = true; }
	void detach() noexcept { closing_ = true; }
	void close() noexcept { closing_ = true; }

	// No copy constructor
	ReplicationProtocolClient(const ReplicationProtocolClient&) = delete;
	ReplicationProtocolClient& operator=(const ReplicationProtocolClient&) = delete;

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

	explicit ReplicationProtocolClient(bool cluster_database_ = false);
	~ReplicationProtocolClient() noexcept;

	void reset();

	// Outbound setup: lock the destination shard + BLOCKING connect to the primary. Offloaded
	// to the reactor pool by the outbound coroutine; on success sock_fd_ holds the connection.
	bool init_replication_protocol(const std::string& host, int port, const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept;

	void send_message(ReplicationMessageType type, const std::string& message = "");
	void send_message(ReplicationReplyType type, const std::string& message = "");
	void send_file(ReplicationReplyType type, int fd);

	void replication_server(ReplicationMessageType type, const std::string& message);
	void replication_client(ReplicationReplyType type, const std::string& message);

	void msg_get_changesets(const std::string& message);
	void msg_set_revision(const std::string& message);
	void reply_welcome(const std::string& message);
	void reply_exception(const std::string& message);
	void reply_end_of_changes(const std::string& message);
	void reply_fail(const std::string& message);
	void reply_db_header(const std::string& message);
	void reply_db_filename(const std::string& message);
	void reply_db_filedata(const std::string& message);
	void reply_db_footer(const std::string& message);
	void reply_changeset(const std::string& message);
	void reply_done(const std::string& message);

	// The server greeting (REPLY_WELCOME) sent as soon as an inbound connection opens.
	void greeting() { send_message(ReplicationReplyType::REPLY_WELCOME); }

	// --- the coroutine's hooks ---
	void set_socket_fd(int fd) noexcept { sock_fd_ = fd; }
	int socket_fd() const noexcept { return sock_fd_; }
	bool closing() const noexcept { return closing_; }

	// FILE_FOLLOWS receive: create + track a temp file, return {fd, path}. The coroutine
	// streams the incoming file into fd, then dispatches with the temp path. {-1,""} on error.
	std::pair<int, std::string> new_temp_file();

	std::string __repr__() const;
};

#endif  /* XAPIAND_CLUSTERING */
