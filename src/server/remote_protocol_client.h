/*
 * Copyright (C) 2015-2019 Dubalu LLC
 * Copyright (C) 2006,2007,2008,2009,2010,2014,2017 Olly Betts
 * Copyright (C) 2007,2009,2010 Lemur Consulting Ltd
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

#include <atomic>                             // for std::atomic_char
#include <memory>                             // for shared_ptr
#include <string>                             // for std::string
#include <utility>                            // for std::pair
#include <vector>                             // for std::vector

#include "endpoint.h"                         // for Endpoint
#include "enum.h"                             // for ENUM_CLASS
#include "xapian.h"                           // for Xapian::Registry, Xapian::rev

// #define SAVE_LAST_MESSAGES
#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)
#ifndef SAVE_LAST_MESSAGES
#define SAVE_LAST_MESSAGES 1
#endif
#endif


// The Xapian remote-backend protocol version (see the Xapian source for the full history).
// 44: 1.5.0 pack_uint() now used; many other changes
#define XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION 44
#define XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION 0

#define FILE_FOLLOWS '\xfd'


class lock_shard;


ENUM_CLASS(RemoteMessageType, int,
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
	MSG_POSITIONLISTCOUNT,      // Get PositionList length
	MSG_READACCESS,             // Select current database
	MSG_MAX
)


ENUM_CLASS(RemoteReplyType, int,
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
	REPLY_POSTLISTHEADER,	    // Header for get postlist
	REPLY_POSTLIST,		        // Get Postlist
	REPLY_VALUE,                // Document Value
	REPLY_ADDDOCUMENT,          // Add Document
	REPLY_RESULTS,              // Results (MSet)
	REPLY_METADATA,             // Metadata
	REPLY_METADATAKEYLIST,      // Iterator for metadata keys
	REPLY_FREQS,                // Get termfreq and collfreq
	REPLY_UNIQUETERMS,          // Get number of unique terms in doc
	REPLY_POSITIONLISTCOUNT,    // Get PositionList length
	REPLY_REMOVESPELLING,       // Remove a spelling
	REPLY_TERMLISTHEADER,	    // Header for get termlist
	REPLY_MAX
)


struct RemoteProtocolPendingQuery {
	Xapian::rev revision;
	std::unique_ptr<Xapian::Enquire> enquire;
	std::vector<Xapian::MatchSpy*> matchspies;
};


// The per-connection Xapian binary remote-backend protocol handler. Transport-agnostic: it
// consumes one request via remote_server() (the blocking Xapian work) and appends its framed
// replies to reply_buffer_, which the Asio connection coroutine (remote_protocol_service.h)
// drains and writes. No libev / Worker / thread pool -- the coroutine owns the socket and the
// reactor pool runs the blocking dispatch.
class RemoteProtocolClient {
	int flags;
	Endpoint endpoint;

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

	Xapian::Registry registry;

	// The reply sink (send_message appends here; the coroutine drains it) + the
	// "close this connection" flag (the handlers' detach()/destroy() set it).
	std::string reply_buffer_;
	bool closing_;

	void send_message(RemoteReplyType type, const std::string& message);
	void send_message(char type_as_char, const std::string& message);

	// The handlers signal "tear this connection down" by calling detach()/destroy() (kept
	// verbatim from the libev version); here they just flag the coroutine to stop + close.
	void destroy() noexcept { closing_ = true; }
	void detach() noexcept { closing_ = true; }

	void msg_allterms(const std::string& message);
	void msg_termlist(const std::string& message);
	void msg_positionlist(const std::string& message);
	void msg_postlist(const std::string& message);
	void msg_positionlistcount(const std::string& message);
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
	void msg_metadatakeylist(const std::string& message);
	void msg_setmetadata(const std::string& message);
	void msg_addspelling(const std::string& message);
	void msg_removespelling(const std::string& message);
	void msg_shutdown(const std::string& message);

	// No copy constructor
	RemoteProtocolClient(const RemoteProtocolClient&) = delete;
	RemoteProtocolClient& operator=(const RemoteProtocolClient&) = delete;

public:
	explicit RemoteProtocolClient(bool cluster_database_ = false);
	~RemoteProtocolClient() noexcept;

	// The initial greeting (REPLY_UPDATE) the server sends as soon as a connection opens.
	void greeting() { msg_update(std::string()); }

	// Dispatch one request; replies land in reply_buffer_. Runs the blocking Xapian work,
	// so the coroutine offloads it to the reactor pool.
	void remote_server(RemoteMessageType type, const std::string& message);

	// --- the coroutine's hooks ---
	bool closing() const noexcept { return closing_; }
	bool has_reply() const noexcept { return !reply_buffer_.empty(); }
	std::string take_reply() { return std::move(reply_buffer_); }

	// FILE_FOLLOWS receive: create + track a temp file, return {fd, path}. The coroutine
	// streams the incoming file into fd, then dispatches remote_server(type, path). Returns
	// {-1, ""} if the temp file could not be created.
	std::pair<int, std::string> new_temp_file();

	std::string __repr__() const;
};


#endif  /* XAPIAND_CLUSTERING */
