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

#ifdef HAVE_REMOTE_PROTOCOL

#include "client_base.h"

#include <xapian.h>

#include <unordered_map>


enum replicate_reply_type {
	REPL_REPLY_END_OF_CHANGES,  // 0 - No more changes to transfer.
	REPL_REPLY_FAIL,            // 1 - Couldn't generate full set of changes.
	REPL_REPLY_DB_HEADER,       // 2 - The start of a whole DB copy.
	REPL_REPLY_DB_FILENAME,     // 3 - The name of a file in a DB copy.
	REPL_REPLY_DB_FILEDATA,     // 4 - Contents of a file in a DB copy.
	REPL_REPLY_DB_FOOTER,       // 5 - End of a whole DB copy.
	REPL_REPLY_CHANGESET,       // 6 - A changeset file is being sent.
	REPL_MSG_GET_CHANGESETS,
	REPL_MAX,
};

enum binary_state {
	init_remoteprotocol,
	remoteprotocol,
	init_replicationprotocol,
	replicationprotocol,
};

//
//   A single instance of a non-blocking Xapiand binary protocol handler
//
class BinaryClient : public BaseClient, public RemoteProtocol {
	typedef std::unordered_map<Xapian::Database *, Database *> databases_map_t;

private:
	bool running;
	enum binary_state state;
	int file_descriptor;
	char file_path[PATH_MAX];

	databases_map_t databases;

	// Buffers that are pending write
	std::string buffer;
	queue::Queue<std::unique_ptr<Buffer>> messages_queue;

	Database *repl_database;
	std::string repl_uuid;
	Endpoints repl_endpoints;
	std::string repl_db_filename;
	std::string repl_db_uuid;
	size_t repl_db_revision;
	bool repl_switched_db;
	bool repl_just_switched_db;

	void on_read(const char *buf, size_t received);
	void on_read_file(const char *buf, size_t received);
	void on_read_file_done();

	void repl_apply(replicate_reply_type type, const std::string & message);
	void repl_end_of_changes(const std::string & message);
	void repl_fail(const std::string & message);
	void repl_set_db_header(const std::string & message);
	void repl_set_db_filename(const std::string & message);
	void repl_set_db_filedata(const std::string & message);
	void repl_set_db_footer(const std::string & message);
	void repl_changeset(const std::string & message);
	void repl_get_changesets(const std::string & message);
	void receive_repl();

public:
	inline replicate_reply_type get_message(double timeout, std::string & result, replicate_reply_type required_type) {
		char required_type_as_char = static_cast<char>(required_type);
		return static_cast<replicate_reply_type>(get_message(timeout, result, required_type_as_char));
	}

	inline message_type get_message(double timeout, std::string & result, message_type required_type) {
		char required_type_as_char = static_cast<char>(required_type);
		return static_cast<message_type>(get_message(timeout, result, required_type_as_char));
	}

	char get_message(double timeout, std::string & result, char required_type);

	inline void send_message(reply_type type, const std::string &message) {
		char type_as_char = static_cast<char>(type);
		send_message(type_as_char, message, 0.0);
	}

	inline void send_message(reply_type type, const std::string &message, double end_time) {
		char type_as_char = static_cast<char>(type);
		send_message(type_as_char, message, end_time);
	}

	void send_message(char type_as_char, const std::string &message, double end_time=0.0);


	Xapian::Database * get_db(bool);
	void release_db(Xapian::Database *);
	void select_db(const std::vector<std::string> &, bool, int);
	void shutdown();

	BinaryClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	~BinaryClient();

	bool init_remote();
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);

	void run();
};

#endif  /* HAVE_REMOTE_PROTOCOL */
