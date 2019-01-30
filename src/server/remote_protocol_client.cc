/*
 * Copyright (C) 2015-2019 Dubalu LLC
 * Copyright (C) 2006,2007,2008,2009,2010,2011,2012,2013,2014,2015,2016,2017,2018,2019 Olly Betts
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

#include "remote_protocol_client.h"

#ifdef XAPIAND_CLUSTERING

#include <errno.h>                            // for errno
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "database.h"                         // for Database
#include "error.hh"                           // for error:name, error::description
#include "fs.hh"                              // for delete_files, build_path_index
#include "ignore_unused.h"                    // for ignore_unused
#include "io.hh"                              // for io::*
#include "length.h"
#include "manager.h"                          // for XapiandManager
#include "metrics.h"                          // for Metrics::metrics
#include "repr.hh"                            // for repr
#include "utype.hh"                           // for toUType
#include "server/remote_protocol_client.h"    // for RemoteProtocolClient


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_REPLICATION
// #define L_REPLICATION L_RED
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_BINARY_WIRE
// #define L_BINARY_WIRE L_ORANGE
// #undef L_BINARY
// #define L_BINARY L_TEAL
// #undef L_BINARY_PROTO
// #define L_BINARY_PROTO L_TEAL
// #undef L_OBJ_BEGIN
// #define L_OBJ_BEGIN L_DELAYED_600
// #undef L_OBJ_END
// #define L_OBJ_END L_DELAYED_N_UNLOG


/*  ____                      _       ____            _                  _
 * |  _ \ ___ _ __ ___   ___ | |_ ___|  _ \ _ __ ___ | |_ ___   ___ ___ | |
 * | |_) / _ \ '_ ` _ \ / _ \| __/ _ \ |_) | '__/ _ \| __/ _ \ / __/ _ \| |
 * |  _ <  __/ | | | | | (_) | ||  __/  __/| | | (_) | || (_) | (_| (_) | |
 * |_| \_\___|_| |_| |_|\___/ \__\___|_|   |_|  \___/ \__\___/ \___\___/|_|
 *
 * Based on xapian/xapian-core/net/remoteserver.cc @ 62d608e
 *
 */


constexpr int DB_ACTION_MASK_ = 0x03;  // Xapian::DB_ACTION_MASK_


static inline std::string serialise_error(const Xapian::Error &exc) {
	// The byte before the type name is the type code.
	std::string result(1, (exc.get_type())[-1]);
	result += serialise_length(exc.get_context().length());
	result += exc.get_context();
	result += serialise_length(exc.get_msg().length());
	result += exc.get_msg();
	// The "error string" goes last so we don't need to store its length.
	const char* err = exc.get_error_string();
	if (err) result += err;
	return result;
}


static inline std::string::size_type common_prefix_length(const std::string &a, const std::string &b) {
	std::string::size_type minlen = std::min(a.size(), b.size());
	std::string::size_type common;
	for (common = 0; common < minlen; ++common) {
		if (a[common] != b[common]) break;
	}
	return common;
}


RemoteProtocolClient::RemoteProtocolClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double /*active_timeout_*/, double /*idle_timeout_*/, bool cluster_database_)
	: MetaBaseClient<RemoteProtocolClient>(std::move(parent_), ev_loop_, ev_flags_, sock_),
	  LockableDatabase(),
	  state(State::INIT_REMOTE),
#ifdef SAVE_LAST_MESSAGES
	  last_message_received('\xff'),
	  last_message_sent('\xff'),
#endif
	  file_descriptor(-1),
	  file_message_type('\xff'),
	  temp_file_template("xapiand.XXXXXX"),
	  cluster_database(cluster_database_),
	  _msg_query_database_lock(this)
{
	++XapiandManager::binary_clients();

	Metrics::metrics()
		.xapiand_binary_connections
		.Increment();

	L_CONN("New Binary Client in socket %d, %d client(s) of a total of %d connected.", sock_, XapiandManager::binary_clients().load(), XapiandManager::total_clients().load());
}


RemoteProtocolClient::~RemoteProtocolClient() noexcept
{
	try {
		if (XapiandManager::binary_clients().fetch_sub(1) == 0) {
			L_CRIT("Inconsistency in number of binary clients");
			sig_exit(-EX_SOFTWARE);
		}

		if (file_descriptor != -1) {
			io::close(file_descriptor);
			file_descriptor = -1;
		}

		for (const auto& filename : temp_files) {
			io::unlink(filename.c_str());
		}

		if (!temp_directory.empty()) {
			delete_files(temp_directory.c_str());
		}

		if (is_shutting_down() && !is_idle()) {
			L_INFO("Binary client killed!");
		}

		if (cluster_database) {
			L_CRIT("Cannot synchronize cluster database!");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
RemoteProtocolClient::send_message(RemoteReplyType type, const std::string& message)
{
	L_CALL("RemoteProtocolClient::send_message(%s, <message>)", RemoteReplyTypeNames(type));

	L_BINARY_PROTO("<< send_message (%s): %s", RemoteReplyTypeNames(type), repr(message));

	send_message(toUType(type), message);
}


void
RemoteProtocolClient::remote_server(RemoteMessageType type, const std::string &message)
{
	L_CALL("RemoteProtocolClient::remote_server(%s, <message>)", RemoteMessageTypeNames(type));

	L_OBJ_BEGIN("RemoteProtocolClient::remote_server:BEGIN {type:%s}", RemoteMessageTypeNames(type));
	L_OBJ_END("RemoteProtocolClient::remote_server:END {type:%s}", RemoteMessageTypeNames(type));

	try {
		switch (type) {
			case RemoteMessageType::MSG_ALLTERMS:
				msg_allterms(message);
				return;
			case RemoteMessageType::MSG_COLLFREQ:
				msg_collfreq(message);
				return;
			case RemoteMessageType::MSG_DOCUMENT:
				msg_document(message);
				return;
			case RemoteMessageType::MSG_TERMEXISTS:
				msg_termexists(message);
				return;
			case RemoteMessageType::MSG_TERMFREQ:
				msg_termfreq(message);
				return;
			case RemoteMessageType::MSG_VALUESTATS:
				msg_valuestats(message);
				return;
			case RemoteMessageType::MSG_KEEPALIVE:
				msg_keepalive(message);
				return;
			case RemoteMessageType::MSG_DOCLENGTH:
				msg_doclength(message);
				return;
			case RemoteMessageType::MSG_QUERY:
				msg_query(message);
				return;
			case RemoteMessageType::MSG_TERMLIST:
				msg_termlist(message);
				return;
			case RemoteMessageType::MSG_POSITIONLIST:
				msg_positionlist(message);
				return;
			case RemoteMessageType::MSG_POSTLIST:
				msg_postlist(message);
				return;
			case RemoteMessageType::MSG_REOPEN:
				msg_reopen(message);
				return;
			case RemoteMessageType::MSG_UPDATE:
				msg_update(message);
				return;
			case RemoteMessageType::MSG_ADDDOCUMENT:
				msg_adddocument(message);
				return;
			case RemoteMessageType::MSG_CANCEL:
				msg_cancel(message);
				return;
			case RemoteMessageType::MSG_DELETEDOCUMENTTERM:
				msg_deletedocumentterm(message);
				return;
			case RemoteMessageType::MSG_COMMIT:
				msg_commit(message);
				return;
			case RemoteMessageType::MSG_REPLACEDOCUMENT:
				msg_replacedocument(message);
				return;
			case RemoteMessageType::MSG_REPLACEDOCUMENTTERM:
				msg_replacedocumentterm(message);
				return;
			case RemoteMessageType::MSG_DELETEDOCUMENT:
				msg_deletedocument(message);
				return;
			case RemoteMessageType::MSG_WRITEACCESS:
				msg_writeaccess(message);
				return;
			case RemoteMessageType::MSG_GETMETADATA:
				msg_getmetadata(message);
				return;
			case RemoteMessageType::MSG_SETMETADATA:
				msg_setmetadata(message);
				return;
			case RemoteMessageType::MSG_ADDSPELLING:
				msg_addspelling(message);
				return;
			case RemoteMessageType::MSG_REMOVESPELLING:
				msg_removespelling(message);
				return;
			case RemoteMessageType::MSG_GETMSET:
				msg_getmset(message);
				return;
			case RemoteMessageType::MSG_SHUTDOWN:
				msg_shutdown(message);
				return;
			case RemoteMessageType::MSG_METADATAKEYLIST:
				msg_metadatakeylist(message);
				return;
			case RemoteMessageType::MSG_FREQS:
				msg_freqs(message);
				return;
			case RemoteMessageType::MSG_UNIQUETERMS:
				msg_uniqueterms(message);
				return;
			case RemoteMessageType::MSG_POSITIONLISTCOUNT:
				msg_positionlistcount(message);
				return;
			case RemoteMessageType::MSG_READACCESS:
				msg_readaccess(message);
				return;
			default: {
				std::string errmsg("Unexpected message type ");
				errmsg += std::to_string(toUType(type));
				THROW(InvalidArgumentError, errmsg);
			}
		}
	} catch (const Xapian::NetworkTimeoutError& exc) {
		try {
			// We've had a timeout, so the client may not be listening, if we can't
			// send the message right away, just exit and the client will cope.
			send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
		} catch (...) {}
		detach();
	} catch (const Xapian::NetworkError&) {
		// All other network errors mean we are fatally confused and are unlikely
		// to be able to communicate further across this connection. So we don't
		// try to propagate the error to the client, but instead just log the
		// exception and close the connection.
		detach();
	} catch (const Xapian::Error& exc) {
		// Propagate the exception to the client, then return to the main
		// message handling loop.
		send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
	} catch (...) {
		L_EXC("ERROR: Dispatching remote protocol message");
		send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
		detach();
	}
}


void
RemoteProtocolClient::msg_allterms(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_allterms(<message>)");

	std::string reply;
	std::string prev = message;
	const std::string& prefix = message;

	reset();
	lock_database lk_db(this);

#if XAPIAN_AT_LEAST(1, 5, 0)
	const Xapian::TermIterator end = db()->allterms_end(prefix);
	for (Xapian::TermIterator t = db()->allterms_begin(prefix); t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply += serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply += serialise_length(term.size() - reuse);
		reply.append(term, reuse, std::string::npos);
		prev = term;
	}
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_ALLTERMS, reply);
#else
	const Xapian::TermIterator end = db()->allterms_end(prefix);
	for (Xapian::TermIterator t = db()->allterms_begin(prefix); t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply = serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply.append(term, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_ALLTERMS, reply);
		prev = term;
	}
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
#endif
}


void
RemoteProtocolClient::msg_termlist(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_termlist(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);

#if XAPIAN_AT_LEAST(1, 5, 0)
	Xapian::TermIterator t = db()->termlist_begin(did);
	Xapian::termcount num_terms = t.get_approx_size();

	send_message(RemoteReplyType::REPLY_TERMLIST0, serialise_length(db()->get_doclength(did)) + serialise_length(num_terms));

    std::string reply;
    std::string prev;

    while (t != db()->termlist_end(did)) {
		if unlikely(prev.size() > 255) {
			prev.resize(255);
		}
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply += serialise_length(t.get_wdf());
		reply += serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply += serialise_length(term.size() - reuse);
		reply.append(term, reuse, std::string::npos);
		prev = term;
		++t;
    }

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_TERMLIST, reply);
#else
	send_message(RemoteReplyType::REPLY_DOCLENGTH, serialise_length(db()->get_doclength(did)));

    std::string reply;
    std::string prev;

	const Xapian::TermIterator end = db()->termlist_end(did);
	for (Xapian::TermIterator t = db()->termlist_begin(did); t != end; ++t) {
		if unlikely(prev.size() > 255) {
			prev.resize(255);
		}
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply = serialise_length(t.get_wdf());
		reply += serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply.append(term, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_TERMLIST, reply);
		prev = term;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
#endif
}


void
RemoteProtocolClient::msg_positionlist(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_positionlist(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
	std::string term(p, p_end - p);

	reset();
	lock_database lk_db(this);

	Xapian::termpos lastpos = static_cast<Xapian::termpos>(-1);
	const Xapian::PositionIterator end = db()->positionlist_end(did, term);
	for (Xapian::PositionIterator i = db()->positionlist_begin(did, term);
		 i != end; ++i) {
		Xapian::termpos pos = *i;
		send_message(RemoteReplyType::REPLY_POSITIONLIST, serialise_length(pos - lastpos - 1));
		lastpos = pos;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_positionlistcount(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_positionlistcount(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	// This is kind of clumsy, but what the public API requires.
	Xapian::termcount result = 0;
	Xapian::TermIterator termit = db()->termlist_begin(did);
	if (termit != db()->termlist_end(did)) {
	   std::string term(p, p_end - p);
	   termit.skip_to(term);
	   if (termit != db()->termlist_end(did)) {
		   result = termit.positionlist_count();
	   }
	}

	send_message(RemoteReplyType::REPLY_POSITIONLISTCOUNT, serialise_length(result));
}


void
RemoteProtocolClient::msg_postlist(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_postlist(<message>)");

	const std::string & term = message;

	reset();
	lock_database lk_db(this);

	Xapian::doccount termfreq = db()->get_termfreq(term);
	Xapian::termcount collfreq = db()->get_collection_freq(term);
	send_message(RemoteReplyType::REPLY_POSTLISTSTART, serialise_length(termfreq) + serialise_length(collfreq));

	Xapian::docid lastdocid = 0;
	const Xapian::PostingIterator end = db()->postlist_end(term);
	for (Xapian::PostingIterator i = db()->postlist_begin(term);
		 i != end; ++i) {

		Xapian::docid newdocid = *i;
		std::string reply(serialise_length(newdocid - lastdocid - 1));
		reply += serialise_length(i.get_wdf());

		send_message(RemoteReplyType::REPLY_POSTLISTITEM, reply);
		lastdocid = newdocid;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_readaccess(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_readaccess(<message>)");

	reset();

	flags = DB_OPEN;
	const char *p = message.c_str();
	const char *p_end = p + message.size();
	if (p != p_end) {
		auto xapian_flags = static_cast<unsigned>(unserialise_length(&p, p_end));
		switch (xapian_flags & DB_ACTION_MASK_) {
			case Xapian::DB_CREATE_OR_OPEN:
				// Create database if it doesn't already exist.
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_CREATE_OR_OVERWRITE:
				// Create database if it doesn't already exist, or overwrite if it does.
				// TODO: Add DB_OVERWRITE
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_CREATE:
				// If the database already exists, an exception will be thrown.
				// TODO: Add DB_CREATE
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_OPEN:
				// Open an existing database.
				flags |= DB_OPEN;
				break;
		}
	}

	if (p != p_end) {
		while (p != p_end) {
			size_t len;
			len = unserialise_length(&p, p_end, true);
			endpoints.add(Endpoint{std::string_view(p, len)});
			p += len;
		}
	}

	msg_update(message);
}


void
RemoteProtocolClient::msg_writeaccess(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_writeaccess(<message>)");

	reset();

	flags = DB_WRITABLE;
	const char *p = message.c_str();
	const char *p_end = p + message.size();
	if (p != p_end) {
		auto xapian_flags = static_cast<unsigned>(unserialise_length(&p, p_end));
		switch (xapian_flags & DB_ACTION_MASK_) {
			case Xapian::DB_CREATE_OR_OPEN:
				// Create database if it doesn't already exist.
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_CREATE_OR_OVERWRITE:
				// Create database if it doesn't already exist, or overwrite if it does.
				// TODO: Add DB_OVERWRITE
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_CREATE:
				// If the database already exists, an exception will be thrown.
				// TODO: Add DB_CREATE
				flags |= DB_CREATE_OR_OPEN;
				break;
			case Xapian::DB_OPEN:
				// Open an existing database.
				flags |= DB_OPEN;
				break;
		}
	}

	if (p != p_end) {
		size_t len;
		len = unserialise_length(&p, p_end, true);
		endpoints.add(Endpoint{std::string_view(p, len)});
		p += len;
		if (p != p_end) {
			THROW(NetworkError, "only one database directory allowed on writable databases");
		}
	}

	msg_update(message);
}


void
RemoteProtocolClient::msg_reopen(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_reopen(<message>)");

	reset();
	lock_database lk_db(this);

	if (!database()->reopen()) {
		lk_db.unlock();

		send_message(RemoteReplyType::REPLY_DONE, std::string());
	} else {
		lk_db.unlock();

		msg_update(message);
	}
}


void
RemoteProtocolClient::msg_update(const std::string &)
{
	L_CALL("RemoteProtocolClient::msg_update(<message>)");

	static const char protocol[2] = {
		char(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION),
		char(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION)
	};

	std::string message(protocol, 2);

	if (!endpoints.empty()) {
		reset();
		lock_database lk_db(this);

		Xapian::doccount num_docs = db()->get_doccount();
		message += serialise_length(num_docs);
		message += serialise_length(db()->get_lastdocid() - num_docs);
		Xapian::termcount doclen_lb = db()->get_doclength_lower_bound();
		message += serialise_length(doclen_lb);
		message += serialise_length(db()->get_doclength_upper_bound() - doclen_lb);
		message += (db()->has_positions() ? '1' : '0');
#if XAPIAN_AT_LEAST(1, 4, 4)
		message += serialise_length(db()->get_total_length());
#else
		message += serialise_length(db()->get_avlength() * db()->get_doccount() + .5);
#endif
		std::string uuid = db()->get_uuid();
		message += uuid;

		lk_db.unlock();
	}

	send_message(RemoteReplyType::REPLY_UPDATE, message);
}


void
RemoteProtocolClient::init_msg_query()
{
	flags = DB_OPEN;
	_msg_query_database_lock.lock();
	_msg_query_matchspies.clear();
	_msg_query_reg = Xapian::Registry{};
	_msg_query_enquire.reset();
}


void
RemoteProtocolClient::reset()
{
	_msg_query_matchspies.clear();
	_msg_query_reg = Xapian::Registry{};
	_msg_query_enquire.reset();
	_msg_query_database_lock.unlock();
}


void
RemoteProtocolClient::msg_query(const std::string &message_in)
{
	L_CALL("RemoteProtocolClient::msg_query(<message>)");

	const char *p = message_in.c_str();
	const char *p_end = p + message_in.size();

	init_msg_query();

	_msg_query_enquire = std::make_unique<Xapian::Enquire>(*db());

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Query.
	size_t len = unserialise_length(&p, p_end, true);
	Xapian::Query query(Xapian::Query::unserialise(std::string(p, len), _msg_query_reg));
	p += len;

	// Unserialise assorted Enquire settings.
	Xapian::termcount qlen = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	_msg_query_enquire->set_query(query, qlen);

	////////////////////////////////////////////////////////////////////////////
	// Collapse key
	Xapian::valueno collapse_max = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));

	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (collapse_max) {
		collapse_key = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));
	}

	_msg_query_enquire->set_collapse_key(collapse_key, collapse_max);

	////////////////////////////////////////////////////////////////////////////
	// docid order

	if (p_end - p < 4 || *p < '0' || *p > '2') {
		THROW(NetworkError, "bad message (docid_order)");
	}
	Xapian::Enquire::docid_order order;
	order = static_cast<Xapian::Enquire::docid_order>(*p++ - '0');

	_msg_query_enquire->set_docid_order(order);

	////////////////////////////////////////////////////////////////////////////
	// Sort by
	using sort_setting = enum { REL, VAL, VAL_REL, REL_VAL, DOCID };

	Xapian::valueno sort_key = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));

	if (*p < '0' || *p > '4') {
		THROW(NetworkError, "bad message (sort_by)");
	}
	sort_setting sort_by;
	sort_by = static_cast<sort_setting>(*p++ - '0');

	if (*p < '0' || *p > '1') {
		THROW(NetworkError, "bad message (sort_value_forward)");
	}
	bool sort_value_forward(*p++ != '0');

	switch (sort_by) {
		case REL:
			_msg_query_enquire->set_sort_by_relevance();
			break;
		case VAL:
			_msg_query_enquire->set_sort_by_value(sort_key, sort_value_forward);
			break;
		case VAL_REL:
			_msg_query_enquire->set_sort_by_value_then_relevance(sort_key, sort_value_forward);
			break;
		case REL_VAL:
			_msg_query_enquire->set_sort_by_relevance_then_value(sort_key, sort_value_forward);
			break;
		case DOCID:
			_msg_query_enquire->set_weighting_scheme(Xapian::BoolWeight());
			break;
	}

	////////////////////////////////////////////////////////////////////////////
	// Time limit

	double time_limit = unserialise_double(&p, p_end);

	_msg_query_enquire->set_time_limit(time_limit);

	////////////////////////////////////////////////////////////////////////////
	// Threshold

	int percent_threshold = *p++;
	if (percent_threshold < 0 || percent_threshold > 100) {
		THROW(NetworkError, "bad message (percent_threshold)");
	}

	double weight_threshold = unserialise_double(&p, p_end);
	if (weight_threshold < 0) {
		THROW(NetworkError, "bad message (weight_threshold)");
	}

	_msg_query_enquire->set_cutoff(percent_threshold, weight_threshold);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Weight object.
	len = unserialise_length(&p, p_end, true);
	std::string wtname(p, len);
	p += len;

	const Xapian::Weight * wttype = _msg_query_reg.get_weighting_scheme(wtname);
	if (wttype == nullptr) {
		// Note: user weighting schemes should be registered by adding them to
		// a Registry, and setting the context using
		// RemoteServer::set_registry().
		THROW(InvalidArgumentError, "Weighting scheme " + wtname + " not registered");
	}

	len = unserialise_length(&p, p_end, true);
	std::unique_ptr<Xapian::Weight> wt(wttype->unserialise(std::string(p, len)));
	_msg_query_enquire->set_weighting_scheme(*wt);
	p += len;

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the RSet object.
	len = unserialise_length(&p, p_end, true);
	Xapian::RSet rset = Xapian::RSet::unserialise(std::string(p, len));
	p += len;

	////////////////////////////////////////////////////////////////////////////
	// Unserialise any MatchSpy objects.
	while (p != p_end) {
		len = unserialise_length(&p, p_end, true);
		std::string spytype(p, len);
		const Xapian::MatchSpy * spyclass = _msg_query_reg.get_match_spy(spytype);
		if (spyclass == nullptr) {
			THROW(InvalidArgumentError, "Match spy " + spytype + " not registered");
		}
		p += len;

		len = unserialise_length(&p, p_end, true);
		Xapian::MatchSpy *spy = spyclass->unserialise(std::string(p, len), _msg_query_reg);
		_msg_query_matchspies.push_back(spy);
		_msg_query_enquire->add_matchspy(spy->release());
		p += len;
	}

	////////////////////////////////////////////////////////////////////////////
	_msg_query_enquire->prepare_mset(&rset, nullptr);

	send_message(RemoteReplyType::REPLY_STATS, _msg_query_enquire->serialise_stats());

	// No checkout for database (it'll still be needed by msg_getmset)
}


void
RemoteProtocolClient::msg_getmset(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_getmset(<message>)");

	if (!_msg_query_enquire) {
		THROW(NetworkError, "Unexpected MSG_GETMSET");
	}

	const char *p = message.c_str();
	const char *p_end = p + message.size();

	Xapian::termcount first = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
	Xapian::termcount maxitems = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	Xapian::termcount check_at_least = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	_msg_query_enquire->unserialise_stats(std::string(p, p_end));

	Xapian::MSet mset = _msg_query_enquire->get_mset(first, maxitems, check_at_least);

	std::string msg;
	for (auto& i : _msg_query_matchspies) {
		std::string spy_results = i->serialise_results();
		msg += serialise_length(spy_results.size());
		msg += spy_results;
	}
	msg += mset.serialise();

	reset();

	send_message(RemoteReplyType::REPLY_RESULTS, msg);
}


void
RemoteProtocolClient::msg_document(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_document(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);

	Xapian::Document doc = database()->get_document(did, false);

	send_message(RemoteReplyType::REPLY_DOCDATA, doc.get_data());

	Xapian::ValueIterator i;
	for (i = doc.values_begin(); i != doc.values_end(); ++i) {
		std::string item(serialise_length(i.get_valueno()));
		item += *i;
		send_message(RemoteReplyType::REPLY_VALUE, item);
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_keepalive(const std::string &)
{
	L_CALL("RemoteProtocolClient::msg_keepalive(<message>)");

	reset();
	lock_database lk_db(this);

	// Ensure *our* database stays alive, as it may contain remote databases!
	db()->keep_alive();

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_termexists(const std::string &term)
{
	L_CALL("RemoteProtocolClient::msg_termexists(<term>)");

	reset();
	lock_database lk_db(this);
	auto reply_type = db()->term_exists(term) ? RemoteReplyType::REPLY_TERMEXISTS : RemoteReplyType::REPLY_TERMDOESNTEXIST;
	lk_db.unlock();

	send_message(reply_type, std::string());
}


void
RemoteProtocolClient::msg_collfreq(const std::string &term)
{
	L_CALL("RemoteProtocolClient::msg_collfreq(<term>)");

	reset();
	lock_database lk_db(this);
	auto collection_freq = db()->get_collection_freq(term);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_COLLFREQ, serialise_length(collection_freq));
}


void
RemoteProtocolClient::msg_termfreq(const std::string &term)
{
	L_CALL("RemoteProtocolClient::msg_termfreq(<term>)");

	reset();
	lock_database lk_db(this);
	auto termfreq = db()->get_termfreq(term);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_TERMFREQ, serialise_length(termfreq));
}


void
RemoteProtocolClient::msg_freqs(const std::string &term)
{
	L_CALL("RemoteProtocolClient::msg_freqs(<term>)");

	reset();
	lock_database lk_db(this);
	auto termfreq = db()->get_termfreq(term);
	auto collection_freq = db()->get_collection_freq(term);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_FREQS,
		serialise_length(termfreq) +
		serialise_length(collection_freq));
}


void
RemoteProtocolClient::msg_valuestats(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_valuestats(<message>)");

	reset();
	lock_database lk_db(this);

	const char *p = message.data();
	const char *p_end = p + message.size();
	while (p != p_end) {
		Xapian::valueno slot = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));
		std::string message_out;
		message_out += serialise_length(db()->get_value_freq(slot));
		std::string bound = db()->get_value_lower_bound(slot);
		message_out += serialise_length(bound.size());
		message_out += bound;
		bound = db()->get_value_upper_bound(slot);
		message_out += serialise_length(bound.size());
		message_out += bound;

		send_message(RemoteReplyType::REPLY_VALUESTATS, message_out);
	}
}


void
RemoteProtocolClient::msg_doclength(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_doclength(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	auto doclength = db()->get_doclength(did);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DOCLENGTH, serialise_length(doclength));
}


void
RemoteProtocolClient::msg_uniqueterms(const std::string &message)
{
	L_CALL("RemoteProtocolClient::msg_uniqueterms(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	auto unique_terms = db()->get_unique_terms(did);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_UNIQUETERMS, serialise_length(unique_terms));
}


void
RemoteProtocolClient::msg_commit(const std::string &)
{
	L_CALL("RemoteProtocolClient::msg_commit(<message>)");

	reset();
	lock_database lk_db(this);
	database()->commit();
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_cancel(const std::string &)
{
	L_CALL("RemoteProtocolClient::msg_cancel(<message>)");

	reset();
	lock_database lk_db(this);
	// We can't call cancel since that's an internal method, but this
	// has the same effect with minimal additional overhead.
	database()->begin_transaction(false);
	database()->cancel_transaction();
}


void
RemoteProtocolClient::msg_adddocument(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_adddocument(<message>)");

	auto document = Xapian::Document::unserialise(message);

	reset();
	lock_database lk_db(this);
	auto did = database()->add_document(std::move(document));
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}


void
RemoteProtocolClient::msg_deletedocument(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_deletedocument(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	auto did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	database()->delete_document(did);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolClient::msg_deletedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_deletedocumentterm(<message>)");

	reset();
	lock_database lk_db(this);
	database()->delete_document_term(message);
}


void
RemoteProtocolClient::msg_replacedocument(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_replacedocument(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	reset();
	lock_database lk_db(this);
	database()->replace_document(did, std::move(document));
}


void
RemoteProtocolClient::msg_replacedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_replacedocumentterm(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t len = unserialise_length(&p, p_end, true);
	std::string unique_term(p, len);
	p += len;

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	reset();
	lock_database lk_db(this);
	auto did = database()->replace_document_term(unique_term, std::move(document));
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}


void
RemoteProtocolClient::msg_getmetadata(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_getmetadata(<message>)");

	reset();
	lock_database lk_db(this);
	auto value = database()->get_metadata(message);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_METADATA, value);
}


void
RemoteProtocolClient::msg_metadatakeylist(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_metadatakeylist(<message>)");

	reset();
	lock_database lk_db(this);

#if XAPIAN_AT_LEAST(1, 5, 0)
	std::string reply;
	std::string prev = message;
	const std::string& prefix = message;
	for (Xapian::TermIterator t = db()->metadata_keys_begin(prefix);
		 t != db()->metadata_keys_end(prefix);
		 ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply.append(1, char(reuse));
		reply += serialise_length(term.size() - reuse);
		reply.append(term, reuse, std::string::npos);
		prev = term;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_METADATAKEYLIST, reply);

#else
	std::string reply;
	std::string prev = message;
	const std::string& prefix = message;
	const Xapian::TermIterator end = db()->metadata_keys_end(prefix);
	Xapian::TermIterator t = db()->metadata_keys_begin(prefix);
	for (; t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string& term = *t;
		size_t reuse = common_prefix_length(prev, term);
		reply.assign(1, char(reuse));
		reply.append(term, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_METADATAKEYLIST, reply);
		prev = term;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
#endif
}


void
RemoteProtocolClient::msg_setmetadata(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_setmetadata(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t keylen = unserialise_length(&p, p_end, true);
	std::string key(p, keylen);
	p += keylen;
	std::string val(p, p_end - p);

	reset();
	lock_database lk_db(this);
	database()->set_metadata(key, val);
}


void
RemoteProtocolClient::msg_addspelling(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_addspelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqinc = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	database()->add_spelling(std::string(p, p_end - p), freqinc);
}


void
RemoteProtocolClient::msg_removespelling(const std::string & message)
{
	L_CALL("RemoteProtocolClient::msg_removespelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqdec = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	auto result = database()->remove_spelling(std::string(p, p_end - p), freqdec);
#if XAPIAN_AT_LEAST(1, 5, 0)
	send_message(RemoteReplyType::REPLY_REMOVESPELLING, serialise_length(result));
#else
	ignore_unused(result);
#endif
}


void
RemoteProtocolClient::msg_shutdown(const std::string &)
{
	L_CALL("RemoteProtocolClient::msg_shutdown(<message>)");

	close();
	stop();
	destroy();
	detach();
}


bool
RemoteProtocolClient::is_idle() const
{
	if (!is_waiting() && !is_running() && write_queue.empty()) {
		std::lock_guard<std::mutex> lk(runner_mutex);
		return messages.empty();
	}
	return false;
}


bool
RemoteProtocolClient::init_remote() noexcept
{
	L_CALL("RemoteProtocolClient::init_remote()");

	std::lock_guard<std::mutex> lk(runner_mutex);

	ASSERT(!running);

	// Setup state...
	state = State::INIT_REMOTE;

	// And start a runner.
	running = true;
	XapiandManager::remote_client_pool()->enqueue(share_this<RemoteProtocolClient>());
	return true;
}


ssize_t
RemoteProtocolClient::on_read(const char *buf, ssize_t received)
{
	L_CALL("RemoteProtocolClient::on_read(<buf>, %zu)", received);

	if (received <= 0) {
		return received;
	}

	L_BINARY_WIRE("RemoteProtocolClient::on_read: %zd bytes", received);
	ssize_t processed = -buffer.size();
	buffer.append(buf, received);
	while (buffer.size() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		char type = *p++;
		L_BINARY_WIRE("on_read message: %s {state:%s}", repr(std::string(1, type)), StateNames(state));
		switch (type) {
			case FILE_FOLLOWS: {
				char path[PATH_MAX];
				if (temp_directory.empty()) {
					if (temp_directory_template.empty()) {
						temp_directory = "/tmp";
					} else {
						strncpy(path, temp_directory_template.c_str(), PATH_MAX);
						build_path_index(temp_directory_template);
						if (io::mkdtemp(path) == nullptr) {
							L_ERR("Directory %s not created: %s (%d): %s", temp_directory_template, error::name(errno), errno, error::description(errno));
							detach();
							return processed;
						}
						temp_directory = path;
					}
				}
				strncpy(path, (temp_directory + "/" + temp_file_template).c_str(), PATH_MAX);
				file_descriptor = io::mkstemp(path);
				temp_files.push_back(path);
				file_message_type = *p++;
				if (file_descriptor == -1) {
					L_ERR("Cannot create temporary file: %s (%d): %s", error::name(errno), errno, error::description(errno));
					detach();
					return processed;
				} else {
					L_BINARY("Start reading file: %s (%d)", path, file_descriptor);
				}
				read_file();
				processed += p - o;
				buffer.clear();
				return processed;
			}
		}

		ssize_t len;
		try {
			len = unserialise_length(&p, p_end, true);
		} catch (const Xapian::SerialisationError) {
			return received;
		}

		if (!closed) {
			std::lock_guard<std::mutex> lk(runner_mutex);
			if (!running) {
				// Enqueue message...
				messages.push_back(Buffer(type, p, len));
				// And start a runner.
				running = true;
				XapiandManager::remote_client_pool()->enqueue(share_this<RemoteProtocolClient>());
			} else {
				// There should be a runner, just enqueue message.
				messages.push_back(Buffer(type, p, len));
			}
		}

		buffer.erase(0, p - o + len);
		processed += p - o + len;
	}

	return received;
}


void
RemoteProtocolClient::on_read_file(const char *buf, ssize_t received)
{
	L_CALL("RemoteProtocolClient::on_read_file(<buf>, %zu)", received);

	L_BINARY_WIRE("RemoteProtocolClient::on_read_file: %zd bytes", received);

	io::write(file_descriptor, buf, received);
}


void
RemoteProtocolClient::on_read_file_done()
{
	L_CALL("RemoteProtocolClient::on_read_file_done()");

	L_BINARY_WIRE("RemoteProtocolClient::on_read_file_done");

	io::close(file_descriptor);
	file_descriptor = -1;

	const auto& temp_file = temp_files.back();

	if (!closed) {
		std::lock_guard<std::mutex> lk(runner_mutex);
		if (!running) {
			// Enqueue message...
			messages.push_back(Buffer(file_message_type, temp_file.data(), temp_file.size()));
			// And start a runner.
			running = true;
			XapiandManager::remote_client_pool()->enqueue(share_this<RemoteProtocolClient>());
		} else {
			// There should be a runner, just enqueue message.
			messages.push_back(Buffer(file_message_type, temp_file.data(), temp_file.size()));
		}
	}
}


char
RemoteProtocolClient::get_message(std::string &result, char max_type)
{
	L_CALL("RemoteProtocolClient::get_message(<result>, <max_type>)");

	auto& msg = messages.front();

	char type = msg.type;

#ifdef SAVE_LAST_MESSAGES
	last_message_received.store(type, std::memory_order_relaxed);
#endif

	if (type >= max_type) {
		std::string errmsg("Invalid message type ");
		errmsg += std::to_string(int(type));
		THROW(InvalidArgumentError, errmsg);
	}

	const char *msg_str = msg.dpos();
	size_t msg_size = msg.nbytes();
	result.assign(msg_str, msg_size);

	messages.pop_front();

	return type;
}


void
RemoteProtocolClient::send_message(char type_as_char, const std::string &message)
{
	L_CALL("RemoteProtocolClient::send_message(<type_as_char>, <message>)");

#ifdef SAVE_LAST_MESSAGES
	last_message_sent.store(type_as_char, std::memory_order_relaxed);
#endif

	std::string buf;
	buf += type_as_char;
	buf += serialise_length(message.size());
	buf += message;
	write(buf);
}


void
RemoteProtocolClient::send_file(char type_as_char, int fd)
{
	L_CALL("RemoteProtocolClient::send_file(<type_as_char>, <fd>)");

	std::string buf;
	buf += FILE_FOLLOWS;
	buf += type_as_char;
	write(buf);

	MetaBaseClient<RemoteProtocolClient>::send_file(fd);
}


void
RemoteProtocolClient::operator()()
{
	L_CALL("RemoteProtocolClient::operator()()");

	L_CONN("Start running in binary worker...");

	std::unique_lock<std::mutex> lk(runner_mutex);

	switch (state) {
		case State::INIT_REMOTE:
			state = State::REMOTE_SERVER;
			lk.unlock();
			try {
				msg_update(std::string());
			} catch (...) {
				lk.lock();
				running = false;
				lk.unlock();
				L_CONN("Running in worker ended with an exception.");
				detach();
				throw;
			}
			lk.lock();
			break;
		default:
			break;
	}

	while (!messages.empty() && !closed) {
		switch (state) {
			case State::REMOTE_SERVER: {
				std::string message;
				RemoteMessageType type = static_cast<RemoteMessageType>(get_message(message, static_cast<char>(RemoteMessageType::MSG_MAX)));
				lk.unlock();
				try {

					L_BINARY_PROTO(">> get_message[REMOTE_SERVER] (%s): %s", RemoteMessageTypeNames(type), repr(message));
					remote_server(type, message);

					auto sent = total_sent_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_remote_protocol_sent_bytes
						.Increment(sent);

					auto received = total_received_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_remote_protocol_received_bytes
						.Increment(received);

				} catch (...) {
					lk.lock();
					running = false;
					lk.unlock();
					L_CONN("Running in worker ended with an exception.");
					detach();
					throw;
				}
				lk.lock();
				break;
			}

			default:
				running = false;
				lk.unlock();
				L_ERR("Unexpected RemoteProtocolClient State!");
				stop();
				destroy();
				detach();
				return;
		}
	}

	running = false;
	lk.unlock();

	if (is_shutting_down() && is_idle()) {
		L_CONN("Running in worker ended due shutdown.");
		detach();
		return;
	}

	L_CONN("Running in binary worker ended.");
	redetach();  // try re-detaching if already flagged as detaching
}


std::string
RemoteProtocolClient::__repr__() const
{
#ifdef SAVE_LAST_MESSAGES
	auto state_repr = ([this]() -> std::string {
		auto received = last_message_received.load(std::memory_order_relaxed);
		auto sent = last_message_sent.load(std::memory_order_relaxed);
		auto st = state.load(std::memory_order_relaxed);
		switch (st) {
			case State::INIT_REMOTE:
			case State::REMOTE_SERVER:
				return string::format("%s) (%s<->%s",
					StateNames(st),
					RemoteMessageTypeNames(static_cast<RemoteMessageType>(received)),
					RemoteReplyTypeNames(static_cast<RemoteReplyType>(sent)));
			default:
				return "";
		}
	})();
#else
	auto& state_repr = StateNames(state.load(std::memory_order_relaxed));
#endif
	return string::format("<RemoteProtocolClient (%s) {cnt:%ld, sock:%d}%s%s%s%s%s%s%s%s>",
		state_repr,
		use_count(),
		sock,
		is_runner() ? " (runner)" : " (worker)",
		is_running_loop() ? " (running loop)" : " (stopped loop)",
		is_detaching() ? " (deteaching)" : "",
		is_idle() ? " (idle)" : "",
		is_waiting() ? " (waiting)" : "",
		is_running() ? " (running)" : "",
		is_shutting_down() ? " (shutting down)" : "",
		is_closed() ? " (closed)" : "");
}

#endif  /* XAPIAND_CLUSTERING */
