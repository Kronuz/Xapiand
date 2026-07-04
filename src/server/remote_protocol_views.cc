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

#include "remote_protocol_views.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                            // for assert
#include <cstring>                            // for strncpy
#include <errno.h>                            // for errno
#include <fcntl.h>
#include <limits.h>                           // for PATH_MAX
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "aggregations/aggregations.h"        // for AggregationMatchSpy
#include "database/flags.h"                   // for DB_*
#include "database/lock.h"                    // for lock_shard
#include "database/shard.h"                   // for Shard
#include "error.hh"                           // for error:name, error::description
#include "fs.hh"                              // for delete_files, build_path_index
#include "io.hh"                              // for io::*
#include "lru.h"                              // for lru::aging_lru
#include "manager.h"                          // for XapiandManager
#include "metrics.h"                          // for Metrics::metrics
#include "repr.hh"                            // for repr
#include "utype.hh"                           // for toUType
#include "multivalue/geospatialrange.h"       // for GeoSpatialRange
#include "multivalue/range.h"                 // for MultipleValueRange, MultipleValueGE, MultipleValueLE
#include "multivalue/keymaker.h"              // for Multi_MultiValueKeyMaker
#include "server/remote_protocol_views.h"    // for RemoteProtocolViews
#include "xapian/common/pack.h"               // for pack_* unpack_*
#include "xapian/common/serialise-double.h"   // for unserialise_double
#include "xapian/net/serialise-error.h"       // for serialise_error


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


static inline std::string::size_type common_prefix_length(const std::string &a, const std::string &b) {
	std::string::size_type minlen = std::min(a.size(), b.size());
	std::string::size_type common;
	for (common = 0; common < minlen; ++common) {
		if (a[common] != b[common]) break;
	}
	return common;
}

/*  ____                      _       ____            _                  _
 * |  _ \ ___ _ __ ___   ___ | |_ ___|  _ \ _ __ ___ | |_ ___   ___ ___ | |
 * | |_) / _ \ '_ ` _ \ / _ \| __/ _ \ |_) | '__/ _ \| __/ _ \ / __/ _ \| |
 * |  _ <  __/ | | | | | (_) | ||  __/  __/| | | (_) | || (_) | (_| (_) | |
 * |_| \_\___|_| |_| |_|\___/ \__\___|_|   |_|  \___/ \__\___/ \___\___/|_|
 *
 * Based on xapian/xapian-core/net/remoteserver.cc @ db790e9e12bb9b3ebeaf916ac0acdea9a7ab0dd1
 */


static std::mutex pending_queries_mtx;
static lru::aging_lru<std::string, RemoteProtocolPendingQuery> pending_queries(0, 600s);


RemoteProtocolViews::RemoteProtocolViews(bool cluster_database_)
	: flags(0),
#ifdef SAVE_LAST_MESSAGES
	  last_message_received('\xff'),
	  last_message_sent('\xff'),
#endif
	  temp_file_template("xapiand.XXXXXX"),
	  cluster_database(cluster_database_),
	  closing_(false)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		++manager->remote_clients;
	}

	Metrics::metrics()
		.xapiand_remote_connections
		.Increment();

	registry.register_posting_source(GeoSpatialRange{});
	registry.register_posting_source(MultipleValueRange{});
	registry.register_posting_source(MultipleValueGE{});
	registry.register_posting_source(MultipleValueLE{});
	registry.register_match_spy(AggregationMatchSpy{});
	registry.register_key_maker(Multi_MultiValueKeyMaker{});

	L_CONN("New Remote Protocol Client, {} client(s) of a total of {} connected.", manager ? manager->remote_clients.load() : 0, manager ? manager->total_clients.load() : 0);
}


RemoteProtocolViews::~RemoteProtocolViews() noexcept
{
	try {
		if (auto manager = XapiandManager::manager()) {
			if (manager->remote_clients.fetch_sub(1) == 0) {
				L_CRIT("Inconsistency in number of binary clients");
				sig_exit(-EX_SOFTWARE);
			}
		}

		for (const auto& filename : temp_files) {
			io::unlink(filename.c_str());
		}

		if (!temp_directory.empty()) {
			delete_files(temp_directory.c_str());
		}

		if (cluster_database) {
			L_CRIT("Cannot synchronize cluster database!");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


std::pair<int, std::string>
RemoteProtocolViews::new_temp_file()
{
	// Create (and track, for the destructor's cleanup) a temp file to stream an incoming
	// FILE_FOLLOWS file into. Ported verbatim from the classic on_read FILE_FOLLOWS path.
	char path[PATH_MAX + 1];
	if (temp_directory.empty()) {
		if (temp_directory_template.empty()) {
			temp_directory = "/tmp";
		} else {
			strncpy(path, temp_directory_template.c_str(), PATH_MAX);
			build_path_index(temp_directory_template);
			if (io::mkdtemp(path) == nullptr) {
				L_ERR("Directory {} not created: {} ({}): {}", temp_directory_template, error::name(errno), errno, error::description(errno));
				return {-1, std::string()};
			}
			temp_directory = path;
		}
	}
	strncpy(path, (temp_directory + "/" + temp_file_template).c_str(), PATH_MAX);
	int fd = io::mkstemp(path);
	if (fd == -1) {
		L_ERR("Cannot create temporary file: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return {-1, std::string()};
	}
	temp_files.push_back(path);
	return {fd, std::string(path)};
}


void
RemoteProtocolViews::send_message(RemoteReplyType type, const std::string& message)
{
	L_CALL("RemoteProtocolViews::send_message({}, <message>)", enum_name(type));

	L_BINARY_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));

	send_message(toUType(type), message);
}


void
RemoteProtocolViews::remote_server(RemoteMessageType type, const std::string &message)
{
	L_CALL("RemoteProtocolViews::remote_server({}, <message>)", enum_name(type));

	L_OBJ_BEGIN("RemoteProtocolViews::remote_server:BEGIN {{type:{}}}", enum_name(type));
	L_OBJ_END("RemoteProtocolViews::remote_server:END {{type:{}}}", enum_name(type));

	L_DEBUG("{} ({}) -> {}", enum_name(type), strings::from_bytes(message.size()), repr(endpoint.to_string()));

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
		L_EXC("ERROR: Dispatching replication protocol message");
		try {
			// We've had a timeout, so the client may not be listening, if we can't
			// send the message right away, just exit and the client will cope.
			send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
		} catch (...) { }
		destroy();
		detach();
	} catch (const Xapian::NetworkError&) {
		// All other network errors mean we are fatally confused and are unlikely
		// to be able to communicate further across this connection. So we don't
		// try to propagate the error to the client, but instead just log the
		// exception and close the connection.
		L_EXC("ERROR: Dispatching remote protocol message");
		destroy();
		detach();
	} catch (const Xapian::Error& exc) {
		// Propagate the exception to the client, then return to the main
		// message handling loop.
		send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
	} catch (...) {
		L_EXC("ERROR: Dispatching remote protocol message");
		send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
		destroy();
		detach();
	}
}


void
RemoteProtocolViews::msg_allterms(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_allterms(<message>)");

	std::string prev = message;

	std::string reply;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		const std::string& prefix = message;
		const Xapian::TermIterator end = db->allterms_end(prefix);
		for (Xapian::TermIterator t = db->allterms_begin(prefix); t != end; ++t) {
			if unlikely(prev.size() > 255)
				prev.resize(255);
			const std::string& term = *t;
			size_t reuse = common_prefix_length(prev, term);
			reply.append(1, char(reuse));
			pack_uint(reply, term.size() - reuse);
			reply.append(term, reuse, std::string::npos);
			pack_uint(reply, t.get_termfreq());
			prev = term;
		}
	}

	send_message(RemoteReplyType::REPLY_ALLTERMS, reply);
}


void
RemoteProtocolViews::msg_termlist(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_termlist(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint_last(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_TERMLIST");
	}

	std::string reply;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		Xapian::TermIterator t = db->termlist_begin(did);
		Xapian::termcount num_terms = t.get_approx_size();

		pack_uint(reply, db->get_doclength(did));
		pack_uint_last(reply, num_terms);
		send_message(RemoteReplyType::REPLY_TERMLISTHEADER, reply);

		reply.resize(0);
		std::string prev;

		while (t != db->termlist_end(did)) {
			if unlikely(prev.size() > 255) {
				prev.resize(255);
			}
			const std::string& term = *t;
			size_t reuse = common_prefix_length(prev, term);
			reply.append(1, char(reuse));
			pack_uint(reply, term.size() - reuse);
			reply.append(term, reuse, std::string::npos);
			pack_uint(reply, t.get_wdf());
			pack_uint(reply, t.get_termfreq());
			prev = term;
			++t;
		}
	}

	send_message(RemoteReplyType::REPLY_TERMLIST, reply);
}


void
RemoteProtocolViews::msg_positionlist(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_positionlist(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_POSITIONLIST");
	}
	std::string term(p, p_end - p);

	std::string reply;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		Xapian::termpos lastpos = static_cast<Xapian::termpos>(-1);
		const Xapian::PositionIterator end = db->positionlist_end(did, term);
		for (Xapian::PositionIterator i = db->positionlist_begin(did, term);
			i != end; ++i) {
			Xapian::termpos pos = *i;
			pack_uint(reply, pos - lastpos - 1);
			lastpos = pos;
		}
	}

	send_message(RemoteReplyType::REPLY_POSITIONLIST, reply);
}


void
RemoteProtocolViews::msg_positionlistcount(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_positionlistcount(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_POSITIONLISTCOUNT");
	}

	Xapian::termcount result = 0;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		// This is kind of clumsy, but what the public API requires.
		Xapian::TermIterator termit = db->termlist_begin(did);
		if (termit != db->termlist_end(did)) {
		std::string term(p, p_end - p);
		termit.skip_to(term);
		if (termit != db->termlist_end(did)) {
			result = termit.positionlist_count();
		}
		}
	}

	std::string reply;
	pack_uint_last(reply, result);
	send_message(RemoteReplyType::REPLY_POSITIONLISTCOUNT, reply);
}


void
RemoteProtocolViews::msg_postlist(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_postlist(<message>)");

	std::string reply;
	const std::string& term = message;

	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		Xapian::doccount termfreq = db->get_termfreq(term);
		pack_uint_last(reply, termfreq);
		send_message(RemoteReplyType::REPLY_POSTLISTHEADER, reply);

		reply.resize(0);
		Xapian::docid lastdocid = 0;
		const Xapian::PostingIterator end = db->postlist_end(term);
		for (Xapian::PostingIterator i = db->postlist_begin(term);
			i != end; ++i) {

			Xapian::docid newdocid = *i;
			pack_uint(reply, newdocid - lastdocid - 1);
			pack_uint(reply, i.get_wdf());

			lastdocid = newdocid;
		}
	}

	send_message(RemoteReplyType::REPLY_POSTLIST, reply);
}


void
RemoteProtocolViews::msg_readaccess(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_readaccess(<message>)");

	flags = 0;
	const char *p = message.c_str();
	const char *p_end = p + message.size();

	if (p != p_end) {
		unsigned flag_bits;
		if (!unpack_uint(&p, p_end, &flag_bits)) {
			throw Xapian::NetworkError("Bad flags in MSG_READACCESS");
		}
		flags = static_cast<int>(flag_bits);
		if (has_db_writable(flags)) {
			throw Xapian::NetworkError("Bad flags in MSG_READACCESS");
		}
	}

	if (p != p_end) {
		std::string path;
		if (!unpack_string(&p, p_end, path)) {
			throw Xapian::NetworkError("Bad path in MSG_READACCESS");
		}
		endpoint = Endpoint(path);
		if (p != p_end) {
			THROW(NetworkError, "only one database allowed on remote databases");
		}
	}

	msg_update(message);
}


void
RemoteProtocolViews::msg_writeaccess(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_writeaccess(<message>)");

	flags = 0;
	const char *p = message.c_str();
	const char *p_end = p + message.size();

	if (p != p_end) {
		unsigned flag_bits;
		if (!unpack_uint(&p, p_end, &flag_bits)) {
			throw Xapian::NetworkError("Bad flags in MSG_WRITEACCESS");
		}
		flags = static_cast<int>(flag_bits);
		if (!has_db_writable(flags)) {
			throw Xapian::NetworkError("Bad flags in MSG_WRITEACCESS");
		}
	}

	if (p != p_end) {
		std::string path;
		if (!unpack_string(&p, p_end, path)) {
			throw Xapian::NetworkError("Bad path in MSG_WRITEACCESS");
		}
		endpoint = Endpoint(path);
		if (p != p_end) {
			THROW(NetworkError, "only one database allowed on remote databases");
		}
	}

	msg_update(message);
}


void
RemoteProtocolViews::msg_reopen(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_reopen(<message>)");

	lock_shard lk_shard(endpoint, flags);

	if (!lk_shard->reopen()) {
		lk_shard.unlock();

		send_message(RemoteReplyType::REPLY_DONE, std::string());
	} else {
		lk_shard.unlock();

		msg_update(message);
	}
}


void
RemoteProtocolViews::msg_update(const std::string &)
{
	L_CALL("RemoteProtocolViews::msg_update(<message>)");

	static const char protocol[2] = {
		char(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION),
		char(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION)
	};

	std::string message(protocol, 2);

	if (!endpoint.empty()) {
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		Xapian::doccount num_docs = db->get_doccount();
		pack_uint(message, num_docs);
		pack_uint(message, db->get_lastdocid() - num_docs);
		Xapian::termcount doclen_lb = db->get_doclength_lower_bound();
		pack_uint(message, doclen_lb);
		pack_uint(message, db->get_doclength_upper_bound() - doclen_lb);
		pack_bool(message, db->has_positions());
		pack_uint(message, db->get_total_length());
		pack_uint(message, db->get_revision());
		message += db->get_uuid();
	}

	send_message(RemoteReplyType::REPLY_UPDATE, message);
}


void
RemoteProtocolViews::msg_query(const std::string &message_in)
{
	L_CALL("RemoteProtocolViews::msg_query(<message>)");

	const char *p = message_in.c_str();
	const char *p_end = p + message_in.size();

	lock_shard lk_shard(endpoint, flags);
	auto db = lk_shard->db();

	RemoteProtocolPendingQuery pending_query;

	pending_query.revision = db->get_revision();

	pending_query.enquire = std::make_unique<Xapian::Enquire>(*db);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise Query ID

	std::string query_id;
	if (!unpack_string(&p, p_end, query_id)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Query.

	std::string serialisation;
	if (!unpack_string(&p, p_end, serialisation)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	Xapian::Query query(Xapian::Query::unserialise(serialisation, registry));

	// Unserialise assorted Enquire settings.
	Xapian::termcount qlen;
	if (!unpack_uint(&p, p_end, &qlen)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	pending_query.enquire->set_query(query, qlen);

	////////////////////////////////////////////////////////////////////////////
	// Collapse key
	Xapian::valueno collapse_max;
	if (!unpack_uint(&p, p_end, &collapse_max)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (collapse_max) {
		if (!unpack_uint(&p, p_end, &collapse_key)) {
			throw Xapian::NetworkError("Bad MSG_QUERY");
		}
	}

	pending_query.enquire->set_collapse_key(collapse_key, collapse_max);

	////////////////////////////////////////////////////////////////////////////
	// docid order

	if (p_end - p < 4 || static_cast<unsigned char>(*p) > 2) {
		THROW(NetworkError, "bad message (docid_order)");
	}
	Xapian::Enquire::docid_order order;
	order = static_cast<Xapian::Enquire::docid_order>(*p++);

	pending_query.enquire->set_docid_order(order);

	////////////////////////////////////////////////////////////////////////////
	// Sort by
	using sort_setting = enum { REL, VAL, VAL_REL, REL_VAL, DOCID };

	if (static_cast<unsigned char>(*p) > 4) {
		throw Xapian::NetworkError("bad message (sort_by)");
	}

	sort_setting sort_by;
	sort_by = static_cast<sort_setting>(*p++);

	Xapian::valueno sort_key = Xapian::BAD_VALUENO;
	if (sort_by != REL) {
		if (!unpack_uint(&p, p_end, &sort_key)) {
			throw Xapian::NetworkError("Bad MSG_QUERY");
		}
	}

	bool sort_value_forward;
	if (!unpack_bool(&p, p_end, &sort_value_forward)) {
		throw Xapian::NetworkError("bad message (sort_value_forward)");
	}

	switch (sort_by) {
		case REL:
			pending_query.enquire->set_sort_by_relevance();
			break;
		case VAL:
			pending_query.enquire->set_sort_by_value(sort_key, sort_value_forward);
			break;
		case VAL_REL:
			pending_query.enquire->set_sort_by_value_then_relevance(sort_key, sort_value_forward);
			break;
		case REL_VAL:
			pending_query.enquire->set_sort_by_relevance_then_value(sort_key, sort_value_forward);
			break;
		case DOCID:
			pending_query.enquire->set_weighting_scheme(Xapian::BoolWeight());
			break;
	}

	////////////////////////////////////////////////////////////////////////////
	// Has positions

	bool full_db_has_positions;
	if (!unpack_bool(&p, p_end, &full_db_has_positions)) {
		throw Xapian::NetworkError("bad message (full_db_has_positions)");
	}

	////////////////////////////////////////////////////////////////////////////
	// Time limit

	double time_limit = unserialise_double(&p, p_end);

	pending_query.enquire->set_time_limit(time_limit);

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

	pending_query.enquire->set_cutoff(percent_threshold, weight_threshold);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Weight object.
	std::string wtname;
	if (!unpack_string(&p, p_end, wtname)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	const Xapian::Weight * wttype = registry.get_weighting_scheme(wtname);
	if (wttype == nullptr) {
		// Note: user weighting schemes should be registered by adding them to
		// a Registry, and setting the context using
		// RemoteServer::set_registry().
		THROW(InvalidArgumentError, "Weighting scheme " + wtname + " not registered");
	}

	if (!unpack_string(&p, p_end, serialisation)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	std::unique_ptr<Xapian::Weight> wt(wttype->unserialise(serialisation));
	pending_query.enquire->set_weighting_scheme(*wt);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the RSet object.
	if (!unpack_string(&p, p_end, serialisation)) {
		throw Xapian::NetworkError("Bad MSG_QUERY");
	}

	Xapian::RSet rset = Xapian::RSet::unserialise(serialisation);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise any MatchSpy or KeyMaker objects.
	while (p != p_end) {
		std::string classtype;
		if (!unpack_string(&p, p_end, classtype)) {
			throw Xapian::NetworkError("Bad MSG_QUERY");
		}
		if (classtype.size() < 8) {
			THROW(InvalidArgumentError, "Class type {} is invalid", classtype);
		}
		std::string_view type(classtype);
		type.remove_prefix(classtype.size() - 8);

		if (!unpack_string(&p, p_end, serialisation)) {
			throw Xapian::NetworkError("Bad MSG_QUERY");
		}

		if (type == "KeyMaker") {
			const Xapian::KeyMaker * sorterclass = registry.get_key_maker(classtype);
			if (sorterclass == nullptr) {
				THROW(InvalidArgumentError, "Key maker {} not registered", classtype);
			}
			Xapian::KeyMaker * sorter = sorterclass->unserialise(serialisation, registry);
			switch (sort_by) {
				case REL:
					break;
				case VAL:
					pending_query.enquire->set_sort_by_key(sorter->release(), sort_value_forward);
					break;
				case VAL_REL:
					pending_query.enquire->set_sort_by_key_then_relevance(sorter->release(), sort_value_forward);
					break;
				case REL_VAL:
					pending_query.enquire->set_sort_by_relevance_then_key(sorter->release(), sort_value_forward);
					break;
				case DOCID:
					break;
			}
		} else if (type == "MatchSpy") {
			const Xapian::MatchSpy * spyclass = registry.get_match_spy(classtype);
			if (spyclass == nullptr) {
				THROW(InvalidArgumentError, "Match spy {} not registered", classtype);
			}
			Xapian::MatchSpy * spy = spyclass->unserialise(serialisation, registry);
			pending_query.matchspies.push_back(spy);
			pending_query.enquire->add_matchspy(spy->release());
		} else {
			THROW(InvalidArgumentError, "Class type {} is invalid", classtype);
		}
	}

	////////////////////////////////////////////////////////////////////////////
	auto prepared_mset = pending_query.enquire->prepare_mset(query_id, full_db_has_positions, &rset, nullptr);
	// Clear internal database, as it's going to be checked in.
	pending_query.enquire->set_database(Xapian::Database{});

	{
		std::lock_guard<std::mutex> lk(pending_queries_mtx);
		pending_queries[query_id] = std::move(pending_query);
	}

	send_message(RemoteReplyType::REPLY_STATS, prepared_mset.serialise_stats());
}


void
RemoteProtocolViews::msg_getmset(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_getmset(<message>)");

	lock_shard lk_shard(endpoint, flags);
	auto db = lk_shard->db();

	const char *p = message.c_str();
	const char *p_end = p + message.size();

	std::string query_id;
	Xapian::termcount first;
	Xapian::termcount maxitems;
	Xapian::termcount check_at_least;
	if (!unpack_string(&p, p_end, query_id) ||
		!unpack_uint(&p, p_end, &first) ||
		!unpack_uint(&p, p_end, &maxitems) ||
		!unpack_uint(&p, p_end, &check_at_least)) {
		throw Xapian::NetworkError("Bad MSG_GETMSET");
	}

	RemoteProtocolPendingQuery pending_query;
	{
		std::lock_guard<std::mutex> lk(pending_queries_mtx);
		auto it = pending_queries.find(query_id);
		if (it == pending_queries.end()) {
			throw Xapian::DatabaseModifiedError("The query ID is not available - you should call Xapian::Database::reopen() and retry the operation");
		}
		pending_query = std::move(it->second);
		pending_queries.erase(it);
	}

	if (pending_query.revision != db->get_revision()) {
		throw Xapian::DatabaseModifiedError("The revision being read has been discarded - you should call Xapian::Database::reopen() and retry the operation");
	}

	// Set internal database from checked out database.
	pending_query.enquire->set_database(*db);


	pending_query.enquire->set_prepared_mset(Xapian::MSet::unserialise_stats(std::string(p, p_end)));

	std::string msg;
	{
		Xapian::MSet mset = pending_query.enquire->get_mset(first, maxitems, check_at_least);
		for (auto& i : pending_query.matchspies) {
			pack_string(msg, i->serialise_results());
		}
		msg += mset.serialise();
		// Make sure mset is destroyed before the database is
		// checked in by the enquire reset() below, hence the scope.
	}

	send_message(RemoteReplyType::REPLY_RESULTS, msg);
}


void
RemoteProtocolViews::msg_document(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_document(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint_last(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_DOCUMENT");
	}

	{
		lock_shard lk_shard(endpoint, flags);

		Xapian::Document doc = lk_shard->get_document(did, false);

		send_message(RemoteReplyType::REPLY_DOCDATA, doc.get_data());

		Xapian::ValueIterator i;
		for (i = doc.values_begin(); i != doc.values_end(); ++i) {
			std::string item;
			pack_uint(item, i.get_valueno());
			item += *i;
			send_message(RemoteReplyType::REPLY_VALUE, item);
		}
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_keepalive(const std::string &)
{
	L_CALL("RemoteProtocolViews::msg_keepalive(<message>)");

	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		// Ensure *our* database stays alive, as it may contain remote databases!
		db->keep_alive();
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_termexists(const std::string &term)
{
	L_CALL("RemoteProtocolViews::msg_termexists(<term>)");

	bool term_exists;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		term_exists = db->term_exists(term);
	}

	auto reply_type = term_exists ? RemoteReplyType::REPLY_TERMEXISTS : RemoteReplyType::REPLY_TERMDOESNTEXIST;
	send_message(reply_type, std::string());
}


void
RemoteProtocolViews::msg_collfreq(const std::string &term)
{
	L_CALL("RemoteProtocolViews::msg_collfreq(<term>)");

	Xapian::termcount collection_freq;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		collection_freq = db->get_collection_freq(term);
	}

	std::string reply;
	pack_uint_last(reply, collection_freq);
	send_message(RemoteReplyType::REPLY_COLLFREQ, reply);
}


void
RemoteProtocolViews::msg_termfreq(const std::string &term)
{
	L_CALL("RemoteProtocolViews::msg_termfreq(<term>)");

	Xapian::doccount termfreq;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		termfreq = db->get_termfreq(term);
	}

	std::string reply;
	pack_uint_last(reply, termfreq);
	send_message(RemoteReplyType::REPLY_TERMFREQ, reply);
}


void
RemoteProtocolViews::msg_freqs(const std::string &term)
{
	L_CALL("RemoteProtocolViews::msg_freqs(<term>)");

	Xapian::doccount termfreq;
	Xapian::termcount collection_freq;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		termfreq = db->get_termfreq(term);
		collection_freq = db->get_collection_freq(term);
	}

	std::string reply;
	pack_uint(reply, termfreq);
	pack_uint_last(reply, collection_freq);
	send_message(RemoteReplyType::REPLY_FREQS, reply);
}


void
RemoteProtocolViews::msg_valuestats(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_valuestats(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::valueno slot;
	if (!unpack_uint_last(&p, p_end, &slot)) {
		throw Xapian::NetworkError("Bad MSG_VALUESTATS");
	}

	Xapian::doccount value_freq;
	std::string value_lower_bound;
	std::string value_upper_bound;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		value_freq = db->get_value_freq(slot);
		value_lower_bound = db->get_value_lower_bound(slot);
		value_upper_bound = db->get_value_upper_bound(slot);
	}

	std::string reply;
	pack_uint(reply, value_freq);
	pack_string(reply, value_lower_bound);
	reply += value_upper_bound;
	send_message(RemoteReplyType::REPLY_VALUESTATS, reply);
}


void
RemoteProtocolViews::msg_doclength(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_doclength(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint_last(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_DOCLENGTH");
	}

	Xapian::termcount doclength;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		doclength = db->get_doclength(did);
	}

	std::string reply;
	pack_uint_last(reply, doclength);
	send_message(RemoteReplyType::REPLY_DOCLENGTH, reply);
}


void
RemoteProtocolViews::msg_uniqueterms(const std::string &message)
{
	L_CALL("RemoteProtocolViews::msg_uniqueterms(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint_last(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_UNIQUETERMS");
	}

	Xapian::termcount unique_terms;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		unique_terms = db->get_unique_terms(did);
	}

	std::string reply;
	pack_uint_last(reply, unique_terms);
	send_message(RemoteReplyType::REPLY_UNIQUETERMS, reply);
}


void
RemoteProtocolViews::msg_commit(const std::string &)
{
	L_CALL("RemoteProtocolViews::msg_commit(<message>)");

	{
		lock_shard lk_shard(endpoint, flags);

		lk_shard->commit();
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_cancel(const std::string &)
{
	L_CALL("RemoteProtocolViews::msg_cancel(<message>)");

	{
		lock_shard lk_shard(endpoint, flags);

		// We can't call cancel since that's an internal method, but this
		// has the same effect with minimal additional overhead.
		lk_shard->begin_transaction(false);
		lk_shard->cancel_transaction();
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_adddocument(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_adddocument(<message>)");

	auto document = Xapian::Document::unserialise(message);

	Xapian::DocumentInfo info;
	{
		lock_shard lk_shard(endpoint, flags);

		info = lk_shard->add_document(std::move(document));
	}

	std::string reply;
	pack_uint(reply, info.did);
	pack_uint(reply, info.version);
	reply += info.term;
	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, reply);
}


void
RemoteProtocolViews::msg_deletedocument(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_deletedocument(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint_last(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_DELETEDOCUMENT");
	}

	{
		lock_shard lk_shard(endpoint, flags);

		lk_shard->delete_document(did);
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_deletedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_deletedocumentterm(<message>)");

	{
		lock_shard lk_shard(endpoint, flags);

		lk_shard->delete_document_term(message);
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_replacedocument(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_replacedocument(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::docid did;
	if (!unpack_uint(&p, p_end, &did)) {
		throw Xapian::NetworkError("Bad MSG_REPLACEDOCUMENT");
	}

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	Xapian::DocumentInfo info;
	{
		lock_shard lk_shard(endpoint, flags);

		info = lk_shard->replace_document(did, std::move(document));
	}

	std::string reply;
	pack_uint(reply, info.did);
	pack_uint(reply, info.version);
	reply += info.term;
	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, reply);
}


void
RemoteProtocolViews::msg_replacedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_replacedocumentterm(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	std::string unique_term;
	if (!unpack_string(&p, p_end, unique_term)) {
		throw Xapian::NetworkError("Bad MSG_REPLACEDOCUMENTTERM");
	}

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	Xapian::DocumentInfo info;
	{
		lock_shard lk_shard(endpoint, flags);

		info = lk_shard->replace_document_term(unique_term, std::move(document));
	}

	std::string reply;
	pack_uint(reply, info.did);
	pack_uint(reply, info.version);
	reply += info.term;
	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, reply);
}


void
RemoteProtocolViews::msg_getmetadata(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_getmetadata(<message>)");

	std::string value;
	{
		lock_shard lk_shard(endpoint, flags);

		value = lk_shard->get_metadata(message);
	}

	send_message(RemoteReplyType::REPLY_METADATA, value);
}


void
RemoteProtocolViews::msg_metadatakeylist(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_metadatakeylist(<message>)");

	std::string reply;
	{
		lock_shard lk_shard(endpoint, flags);
		auto db = lk_shard->db();

		std::string prev = message;
		const std::string& prefix = message;
		for (Xapian::TermIterator t = db->metadata_keys_begin(prefix);
			t != db->metadata_keys_end(prefix);
			++t) {
			if unlikely(prev.size() > 255)
				prev.resize(255);
			const std::string& term = *t;
			size_t reuse = common_prefix_length(prev, term);
			reply.append(1, char(reuse));
			pack_uint(reply, term.size() - reuse);
			reply.append(term, reuse, std::string::npos);
			prev = term;
		}
	}

	send_message(RemoteReplyType::REPLY_METADATAKEYLIST, reply);
}


void
RemoteProtocolViews::msg_setmetadata(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_setmetadata(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	std::string key;
	if (!unpack_string(&p, p_end, key)) {
		throw Xapian::NetworkError("Bad MSG_SETMETADATA");
	}
	std::string val(p, p_end - p);

	{
		lock_shard lk_shard(endpoint, flags);

		lk_shard->set_metadata(key, val);
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_addspelling(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_addspelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::termcount freqinc;
	if (!unpack_uint(&p, p_end, &freqinc)) {
		throw Xapian::NetworkError("Bad MSG_ADDSPELLING");
	}

	{
		lock_shard lk_shard(endpoint, flags);

		lk_shard->add_spelling(std::string(p, p_end - p), freqinc);
	}

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocolViews::msg_removespelling(const std::string & message)
{
	L_CALL("RemoteProtocolViews::msg_removespelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	Xapian::termcount freqdec;
	if (!unpack_uint(&p, p_end, &freqdec)) {
		throw Xapian::NetworkError("Bad MSG_REMOVESPELLING");
	}

	Xapian::termcount result;
	{
		lock_shard lk_shard(endpoint, flags);

		result = lk_shard->remove_spelling(std::string(p, p_end - p), freqdec);
	}

	std::string reply;
	pack_uint_last(reply, result);
	send_message(RemoteReplyType::REPLY_REMOVESPELLING, reply);
}


void
RemoteProtocolViews::msg_shutdown(const std::string &)
{
	L_CALL("RemoteProtocolViews::msg_shutdown(<message>)");

	destroy();
	detach();
}


void
RemoteProtocolViews::send_message(char type_as_char, const std::string &message)
{
	L_CALL("RemoteProtocolViews::send_message(<type_as_char>, <message>)");

#ifdef SAVE_LAST_MESSAGES
	last_message_sent.store(type_as_char, std::memory_order_relaxed);
#endif

	std::string buf;
	buf.push_back(type_as_char);
	pack_uint(buf, message.size());
	buf.append(message);
	reply_buffer_.append(buf);
}


std::string
RemoteProtocolViews::__repr__() const
{
	return strings::format(STEEL_BLUE + "<RemoteProtocolViews {{ endpoint:{} }}{}>",
		repr(endpoint.to_string()),
		closing_ ? " " + ORANGE + "(closing)" + STEEL_BLUE : "");
}

#endif  /* XAPIAND_CLUSTERING */
