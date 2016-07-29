/** @file remoteserver.cc
 *  @brief Xapian remote backend server base class
 */
/* Copyright (C) 2006,2007,2008,2009,2010,2011,2012,2013,2014,2015,2016 Olly Betts
 * Copyright (C) 2006,2007,2009,2010 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#pragma once

#include "client_binary.h"

#ifdef XAPIAND_CLUSTERING

#include <memory>
#include <vector>
#include <string>

#include <xapian.h>

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


class RemoteProtocol {

	BinaryClient* client;

public:

	RemoteProtocol(BinaryClient* client_);
	~RemoteProtocol();

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

	inline void send_message(RemoteReplyType type, const std::string& message, double end_time=0.0) {
		L_BINARY(this, "<< send_message(%s)", RemoteReplyTypeNames[static_cast<int>(type)]);
		L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
		client->send_message(static_cast<char>(type), message, end_time);
	}

};


#endif  /* XAPIAND_CLUSTERING */
