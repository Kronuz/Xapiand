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
#include "servers/server_binary.h"

#include <xapian.h>

#include <unordered_map>


enum class ReplicateType {
	REPLY_END_OF_CHANGES,  // 0 - No more changes to transfer.
	REPLY_FAIL,            // 1 - Couldn't generate full set of changes.
	REPLY_DB_HEADER,       // 2 - The start of a whole DB copy.
	REPLY_DB_FILENAME,     // 3 - The name of a file in a DB copy.
	REPLY_DB_FILEDATA,     // 4 - Contents of a file in a DB copy.
	REPLY_DB_FOOTER,       // 5 - End of a whole DB copy.
	REPLY_CHANGESET,       // 6 - A changeset file is being sent.
	MSG_GET_CHANGESETS,
	MAX,
};

enum class State {
	INIT_REMOTEPROTOCOL,
	REMOTEPROTOCOL,

	REPLICATIONPROTOCOL_SLAVE,
	REPLICATIONPROTOCOL_MASTER,
};


// A single instance of a non-blocking Xapiand binary protocol handler
class BinaryClient : public BaseClient, public RemoteProtocol {
	std::atomic_int running;

	State state;
	int file_descriptor;
	char file_path[PATH_MAX];

	std::unordered_map<Xapian::Database *, std::shared_ptr<Database>> databases;

	// Buffers that are pending write
	std::string buffer;
	queue::Queue<std::unique_ptr<Buffer>> messages_queue;

	std::shared_ptr<Database> repl_database;
	std::shared_ptr<Database> repl_database_tmp;
	std::string repl_uuid;
	Endpoints repl_endpoints;
	std::string repl_db_filename;
	std::string repl_db_uuid;
	size_t repl_db_revision;
	bool repl_switched_db;
	bool repl_just_switched_db;

	BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref *loop_, int sock_, double active_timeout_, double idle_timeout_);

	void on_read(const char *buf, size_t received) override;
	void on_read_file(const char *buf, size_t received) override;
	void on_read_file_done() override;

	void repl_file_done();
	void repl_apply(ReplicateType type, const std::string & message);
	void repl_end_of_changes();
	void repl_fail();
	void repl_set_db_header(const std::string & message);
	void repl_set_db_filename(const std::string & message);
	void repl_set_db_filedata(const std::string & message);
	void repl_set_db_footer();
	void repl_changeset(const std::string & message);
	void repl_get_changesets(const std::string & message);
	void receive_repl();

	friend Worker;

	Xapian::Database* _get_db(bool writable);

public:
	~BinaryClient();

	inline ReplicateType get_message(double timeout, std::string & result, ReplicateType required_type) {
		return (ReplicateType)get_message(timeout, result, static_cast<int>(required_type));
	}

	int get_message(double timeout, std::string &result, int) override;

	char get_message(double timeout, std::string &result);

	inline void send_message(int type, const std::string &message, double end_time=0.0) override {
		send_message(static_cast<char>(type), message, end_time);
	}

	inline void send_message(ReplicateType type, const std::string &message) {
		send_message(static_cast<char>(type), message);
	}

	inline void send_message(ReplicateType type, const std::string &message, double end_time) {
		send_message(static_cast<char>(type), message, end_time);
	}

	void send_message(char type_as_char, const std::string &message, double end_time=0.0);

	Xapian::Database* get_db() override;
	Xapian::WritableDatabase* get_wdb() override;
	void release_db(Xapian::Database *) override;
	void select_db(const std::vector<std::string> &dbpaths_, bool, int) override;
	void shutdown() override;

	bool init_remote();
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);

	void run() override;
};

#endif  /* XAPIAND_CLUSTERING */
