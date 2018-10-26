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

#include "config.h"             // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include <xapian.h>
#include <memory>
#include <string>
#include <vector>

#include "lock_database.h"


#define XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION 39
#define XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION 0


class BinaryClient;


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


static inline const std::string& RemoteMessageTypeNames(RemoteMessageType type) {
	static const std::string RemoteMessageTypeNames[] = {
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
	auto type_int = static_cast<int>(type);
	if (type_int >= 0 || type_int < static_cast<int>(RemoteMessageType::MSG_MAX)) {
		return RemoteMessageTypeNames[type_int];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


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


static inline const std::string& RemoteReplyTypeNames(RemoteReplyType type) {
	static const std::string RemoteReplyTypeNames[] = {
		"REPLY_UPDATE", "REPLY_EXCEPTION", "REPLY_DONE", "REPLY_ALLTERMS",
		"REPLY_COLLFREQ", "REPLY_DOCDATA", "REPLY_TERMDOESNTEXIST",
		"REPLY_TERMEXISTS", "REPLY_TERMFREQ", "REPLY_VALUESTATS", "REPLY_DOCLENGTH",
		"REPLY_STATS", "REPLY_TERMLIST", "REPLY_POSITIONLIST", "REPLY_POSTLISTSTART",
		"REPLY_POSTLISTITEM", "REPLY_VALUE", "REPLY_ADDDOCUMENT", "REPLY_RESULTS",
		"REPLY_METADATA", "REPLY_METADATAKEYLIST", "REPLY_FREQS", "REPLY_UNIQUETERMS",
	};
	auto type_int = static_cast<int>(type);
	if (type_int >= 0 || type_int < static_cast<int>(RemoteReplyType::REPLY_MAX)) {
		return RemoteReplyTypeNames[type_int];
	}
	static const std::string UNKNOWN = "UNKNOWN";
	return UNKNOWN;
}


class RemoteProtocol : protected LockableDatabase {
	friend class BinaryClient;

	BinaryClient& client;

	// For msg_query and msg_mset:
	lock_database _msg_query_database_lock;
	Xapian::Registry _msg_query_reg;
	std::unique_ptr<Xapian::Enquire> _msg_query_enquire;
	std::vector<Xapian::MatchSpy*> _msg_query_matchspies;
	void init_msg_query();
	void reset();

public:

	explicit RemoteProtocol(BinaryClient& client_);
	~RemoteProtocol();

	void send_message(RemoteReplyType type, const std::string& message);

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
	void select_db(const std::vector<std::string> &dbpaths, bool writable, int xapian_flags);
};


#endif  /* XAPIAND_CLUSTERING */
