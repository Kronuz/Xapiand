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

#include "client_binary.h"

#ifdef XAPIAND_CLUSTERING


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


class Replication {

	BinaryClient* client;

public:
	Replication(BinaryClient* client_);
	~Replication();

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

	inline void send_message(ReplicationReplyType type, const std::string& message, double end_time=0.0) {
		L_BINARY(this, "<< send_message(%s)", ReplicationReplyTypeNames[static_cast<int>(type)]);
		L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
		client->send_message(static_cast<char>(type), message, end_time);
	}

};


#endif  /* XAPIAND_CLUSTERING */