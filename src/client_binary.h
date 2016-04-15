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

#define XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION 39
#define XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION 0


enum class RemoteMessageType {
	MSG_ALLTERMS,               // All Terms
	MSG_COLLFREQ,               // Get Collection Frequency
	MSG_DOCUMENT,               // Get Document
	MSG_TERMEXISTS,             // Term Exists?
	MSG_TERMFREQ,               // Get Term Frequency
	MSG_VALUESTATS,             // Get value statistics
	MSG_KEEPALIVE,              // Keep-alive
	MSG_DOCLENGTH,              // Get Doc Length
	MSG_QUERY,                  // Run Query
	MSG_TERMLIST,               // Get TermList
	MSG_POSITIONLIST,           // Get PositionList
	MSG_POSTLIST,               // Get PostList
	MSG_REOPEN,                 // Reopen
	MSG_UPDATE,                 // Get Updated DocCount and AvLength
	MSG_ADDDOCUMENT,            // Add Document
	MSG_CANCEL,                 // Cancel
	MSG_DELETEDOCUMENTTERM,     // Delete Document by term
	MSG_COMMIT,                 // Commit
	MSG_REPLACEDOCUMENT,        // Replace Document
	MSG_REPLACEDOCUMENTTERM,    // Replace Document by term
	MSG_DELETEDOCUMENT,         // Delete Document
	MSG_WRITEACCESS,            // Upgrade to WritableDatabase
	MSG_GETMETADATA,            // Get metadata
	MSG_SETMETADATA,            // Set metadata
	MSG_ADDSPELLING,            // Add a spelling
	MSG_REMOVESPELLING,         // Remove a spelling
	MSG_GETMSET,                // Get MSet
	MSG_SHUTDOWN,               // Shutdown
	MSG_METADATAKEYLIST,        // Iterator for metadata keys
	MSG_FREQS,                  // Get termfreq and collfreq
	MSG_UNIQUETERMS,            // Get number of unique terms in doc
	MSG_READACCESS,             // Select current database
	MSG_MAX
};


static constexpr const char* const RemoteMessageTypeNames[] = {
	"MSG_ALLTERMS", "MSG_COLLFREQ", "MSG_DOCUMENT", "MSG_TERMEXISTS",
	"MSG_TERMFREQ", "MSG_VALUESTATS", "MSG_KEEPALIVE", "MSG_DOCLENGTH",
	"MSG_QUERY", "MSG_TERMLIST", "MSG_POSITIONLIST", "MSG_POSTLIST",
	"MSG_REOPEN", "MSG_UPDATE", "MSG_ADDDOCUMENT", "MSG_CANCEL",
	"MSG_DELETEDOCUMENTTERM", "MSG_COMMIT", "MSG_REPLACEDOCUMENT",
	"MSG_REPLACEDOCUMENTTERM", "MSG_DELETEDOCUMENT", "MSG_WRITEACCESS",
	"MSG_GETMETADATA", "MSG_SETMETADATA", "MSG_ADDSPELLING",
	"MSG_REMOVESPELLING", "MSG_GETMSET", "MSG_SHUTDOWN",
	"MSG_METADATAKEYLIST", "MSG_FREQS", "MSG_UNIQUETERMS", "MSG_READACCESS",
};


enum class RemoteReplyType {
	REPLY_UPDATE,               // Updated database stats
	REPLY_EXCEPTION,            // Exception
	REPLY_DONE,                 // Done sending list
	REPLY_ALLTERMS,             // All Terms
	REPLY_COLLFREQ,             // Get Collection Frequency
	REPLY_DOCDATA,              // Get Document
	REPLY_TERMDOESNTEXIST,      // Term Doesn't Exist
	REPLY_TERMEXISTS,           // Term Exists
	REPLY_TERMFREQ,             // Get Term Frequency
	REPLY_VALUESTATS,           // Value statistics
	REPLY_DOCLENGTH,            // Get Doc Length
	REPLY_STATS,                // Stats
	REPLY_TERMLIST,             // Get Termlist
	REPLY_POSITIONLIST,         // Get PositionList
	REPLY_POSTLISTSTART,        // Start of a postlist
	REPLY_POSTLISTITEM,         // Item in body of a postlist
	REPLY_VALUE,                // Document Value
	REPLY_ADDDOCUMENT,          // Add Document
	REPLY_RESULTS,              // Results (MSet)
	REPLY_METADATA,             // Metadata
	REPLY_METADATAKEYLIST,      // Iterator for metadata keys
	REPLY_FREQS,                // Get termfreq and collfreq
	REPLY_UNIQUETERMS,          // Get number of unique terms in doc
	REPLY_MAX
};


static constexpr const char* const RemoteReplyTypeNames[] = {
	"REPLY_UPDATE", "REPLY_EXCEPTION", "REPLY_DONE", "REPLY_ALLTERMS",
	"REPLY_COLLFREQ", "REPLY_DOCDATA", "REPLY_TERMDOESNTEXIST",
	"REPLY_TERMEXISTS", "REPLY_TERMFREQ", "REPLY_VALUESTATS", "REPLY_DOCLENGTH",
	"REPLY_STATS", "REPLY_TERMLIST", "REPLY_POSITIONLIST", "REPLY_POSTLISTSTART",
	"REPLY_POSTLISTITEM", "REPLY_VALUE", "REPLY_ADDDOCUMENT", "REPLY_RESULTS",
	"REPLY_METADATA", "REPLY_METADATAKEYLIST", "REPLY_FREQS", "REPLY_UNIQUETERMS",
};


enum class ReplicationMessageType {
	MSG_GET_CHANGESETS,
	MSG_MAX,
};


static constexpr const char* const ReplicationMessageTypeNames[] = {
	"MSG_GET_CHANGESETS",
};


enum class ReplicationReplyType {
	REPLY_END_OF_CHANGES,       // No more changes to transfer.
	REPLY_FAIL,                 // Couldn't generate full set of changes.
	REPLY_DB_HEADER,            // The start of a whole DB copy.
	REPLY_DB_FILENAME,          // The name of a file in a DB copy.
	REPLY_DB_FILEDATA,          // Contents of a file in a DB copy.
	REPLY_DB_FOOTER,            // End of a whole DB copy.
	REPLY_CHANGESET,            // A changeset file is being sent.
	REPLY_MAX,
};


static constexpr const char* const ReplicationReplyTypeNames[] = {
	"REPLY_END_OF_CHANGES", "REPLY_FAIL", "REPLY_DB_HEADER", "REPLY_DB_FILENAME",
	"REPLY_DB_FILEDATA", "REPLY_DB_FOOTER", "REPLY_CHANGESET",
};


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

	// For msg_query and msg_mset:
	Xapian::Registry reg;
	std::unique_ptr<Xapian::Enquire> enquire;
	std::vector<Xapian::MatchSpy*> matchspies;

	void remote_server(RemoteMessageType type, const std::string& message);
	void msg_allterms(const std::string& message);
	void msg_termlist(const std::string& message);
	void msg_positionlist(const std::string& message);
	void msg_postlist(const std::string& message);
	void msg_readaccess(const std::string& message);
	void msg_writeaccess(const std::string& message);
	void msg_reopen(const std::string& message);
	void msg_update(const std::string& message);
	void msg_query(const std::string& message);
	void msg_getmset(const std::string& message);
	void msg_document(const std::string& message);
	void msg_keepalive(const std::string& message);
	void msg_termexists(const std::string& message);
	void msg_collfreq(const std::string& message);
	void msg_termfreq(const std::string& message);
	void msg_freqs(const std::string& message);
	void msg_valuestats(const std::string& message);
	void msg_doclength(const std::string& message);
	void msg_uniqueterms(const std::string& message);
	void msg_commit(const std::string& message);
	void msg_cancel(const std::string& message);
	void msg_adddocument(const std::string& message);
	void msg_deletedocument(const std::string& message);
	void msg_deletedocumentterm(const std::string& message);
	void msg_replacedocument(const std::string& message);
	void msg_replacedocumentterm(const std::string& message);
	void msg_getmetadata(const std::string& message);
	void msg_openmetadatakeylist(const std::string& message);
	void msg_setmetadata(const std::string& message);
	void msg_addspelling(const std::string& message);
	void msg_removespelling(const std::string& message);
	void msg_shutdown(const std::string& message);
	void select_db(const std::vector<std::string> &dbpaths_, bool writable_, int flags_);

	// Replication protocol:
	void replication_server(ReplicationMessageType type, const std::string& message);
	void replication_client(ReplicationReplyType type, const std::string& message);
	void replication_client_file_done();

	void msg_get_changesets(const std::string& message);
	void reply_end_of_changes(const std::string& message);
	void reply_fail(const std::string& message);
	void reply_db_header(const std::string& message);
	void reply_db_filename(const std::string& message);
	void reply_db_filedata(const std::string& message);
	void reply_db_footer(const std::string& message);
	void reply_changeset(const std::string& message);

	friend Worker;

public:
	std::string __repr__() const override {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "<BinaryClient at %p>", this);
		return buffer;
	}

	~BinaryClient();

	char get_message(std::string &result, char max_type);

	inline void send_message(RemoteReplyType type, const std::string& message, double end_time=0.0) {
		L_BINARY(this, "<< send_message(%s)", RemoteReplyTypeNames[static_cast<int>(type)]);
		L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
		send_message(static_cast<char>(type), message, end_time);
	}
	inline void send_message(ReplicationReplyType type, const std::string& message, double end_time=0.0) {
		L_BINARY(this, "<< send_message(%s)", ReplicationReplyTypeNames[static_cast<int>(type)]);
		L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
		send_message(static_cast<char>(type), message, end_time);
	}
	void send_message(char type_as_char, const std::string& message, double end_time=0.0);

	bool init_remote();
	bool init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);

	void run() override;
	void _run();
};


using dispatch_func = void (BinaryClient::*)(const std::string&);


#endif  /* XAPIAND_CLUSTERING */
