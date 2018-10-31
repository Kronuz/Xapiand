/** @file remoteserver.cc
 *  @brief Xapian remote backend server base class
 */
/* Copyright (C) 2006,2007,2008,2009,2010,2011,2012,2013,2014,2015,2016,2017 Olly Betts
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

#include "remote_protocol.h"

#ifdef XAPIAND_CLUSTERING

#include "database.h"                         // for Database
#include "length.h"                           // for serialise_length, unserialise_length
#include "server/binary_client.h"

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


/*  ____                      _       ____            _                  _
 * |  _ \ ___ _ __ ___   ___ | |_ ___|  _ \ _ __ ___ | |_ ___   ___ ___ | |
 * | |_) / _ \ '_ ` _ \ / _ \| __/ _ \ |_) | '__/ _ \| __/ _ \ / __/ _ \| |
 * |  _ <  __/ | | | | | (_) | ||  __/  __/| | | (_) | || (_) | (_| (_) | |
 * |_| \_\___|_| |_| |_|\___/ \__\___|_|   |_|  \___/ \__\___/ \___\___/|_|
 *
 * Based on xapian/xapian-core/net/remoteserver.cc @ 6a76cee
 *
 */


using dispatch_func = void (RemoteProtocol::*)(const std::string&);

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


RemoteProtocol::RemoteProtocol(BinaryClient& client_)
	: LockableDatabase(),
	  client(client_),
	  _msg_query_database_lock(this)
{
	L_OBJ("CREATED REMOTE PROTOCOL!");
}


RemoteProtocol::~RemoteProtocol()
{
	L_OBJ("DELETED REMOTE PROTOCOL!");
}


void
RemoteProtocol::send_message(RemoteReplyType type, const std::string& message)
{
	L_CALL("RemoteProtocol::send_message(%s, <message>)", RemoteReplyTypeNames(type));

	L_BINARY_PROTO("<< send_message (%s): %s", RemoteReplyTypeNames(type), repr(message));

	client.send_message(toUType(type), message);
}


void
RemoteProtocol::remote_server(RemoteMessageType type, const std::string &message)
{
	L_CALL("RemoteProtocol::remote_server(%s, <message>)", RemoteMessageTypeNames(type));

	static const dispatch_func dispatch[] = {
		&RemoteProtocol::msg_allterms,
		&RemoteProtocol::msg_collfreq,
		&RemoteProtocol::msg_document,
		&RemoteProtocol::msg_termexists,
		&RemoteProtocol::msg_termfreq,
		&RemoteProtocol::msg_valuestats,
		&RemoteProtocol::msg_keepalive,
		&RemoteProtocol::msg_doclength,
		&RemoteProtocol::msg_query,
		&RemoteProtocol::msg_termlist,
		&RemoteProtocol::msg_positionlist,
		&RemoteProtocol::msg_postlist,
		&RemoteProtocol::msg_reopen,
		&RemoteProtocol::msg_update,
		&RemoteProtocol::msg_adddocument,
		&RemoteProtocol::msg_cancel,
		&RemoteProtocol::msg_deletedocumentterm,
		&RemoteProtocol::msg_commit,
		&RemoteProtocol::msg_replacedocument,
		&RemoteProtocol::msg_replacedocumentterm,
		&RemoteProtocol::msg_deletedocument,
		&RemoteProtocol::msg_writeaccess,
		&RemoteProtocol::msg_getmetadata,
		&RemoteProtocol::msg_setmetadata,
		&RemoteProtocol::msg_addspelling,
		&RemoteProtocol::msg_removespelling,
		&RemoteProtocol::msg_getmset,
		&RemoteProtocol::msg_shutdown,
		&RemoteProtocol::msg_openmetadatakeylist,
		&RemoteProtocol::msg_freqs,
		&RemoteProtocol::msg_uniqueterms,
		&RemoteProtocol::msg_readaccess,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (const Xapian::NetworkTimeoutError& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		try {
			// We've had a timeout, so the client may not be listening, if we can't
			// send the message right away, just exit and the client will cope.
			send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
		} catch (...) {}
		client.destroy();
		client.detach();
	} catch (const Xapian::NetworkError& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		client.destroy();
		client.detach();
	} catch (const BaseException& exc) {
		L_EXC("ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
		send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
		client.destroy();
		client.detach();
	} catch (const Xapian::Error& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		// Propagate the exception to the client, then return to the main
		// message handling loop.
		send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", *exc.what() ? exc.what() : "Unkown std::exception!");
		send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
		client.destroy();
		client.detach();
	} catch (...) {
		std::exception exc;
		L_EXC("ERROR: %s", "Unkown exception!");
		send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
		client.destroy();
		client.detach();
	}
}


void
RemoteProtocol::msg_allterms(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_allterms(<message>)");

	std::string prev = message;
	const std::string& prefix = message;

	reset();
	lock_database lk_db(this);

	const Xapian::TermIterator end = db()->allterms_end(prefix);
	for (Xapian::TermIterator t = db()->allterms_begin(prefix); t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string & v = *t;
		size_t reuse = common_prefix_length(prev, v);
		std::string reply(serialise_length(t.get_termfreq()));
		reply.append(1, char(reuse));
		reply.append(v, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_ALLTERMS, reply);
		prev = v;
	}
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocol::msg_termlist(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_termlist(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);

	send_message(RemoteReplyType::REPLY_DOCLENGTH, serialise_length(db()->get_doclength(did)));
	std::string prev;
	const Xapian::TermIterator end = db()->termlist_end(did);
	for (Xapian::TermIterator t = db()->termlist_begin(did); t != end; ++t) {
		if unlikely(prev.size() > 255) {
			prev.resize(255);
		}
		const std::string & v = *t;
		size_t reuse = common_prefix_length(prev, v);
		std::string reply(serialise_length(t.get_wdf()));
		reply += serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply.append(v, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_TERMLIST, reply);
		prev = v;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocol::msg_positionlist(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_positionlist(<message>)");

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
RemoteProtocol::msg_postlist(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_postlist(<message>)");

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
RemoteProtocol::msg_readaccess(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_readaccess(<message>)");

	int xapian_flags = Xapian::DB_OPEN;
	const char *p = message.c_str();
	const char *p_end = p + message.size();
	if (p != p_end) {
		unsigned flag_bits;
		flag_bits = static_cast<unsigned>(unserialise_length(&p, p_end));
		 xapian_flags |= flag_bits &~ DB_ACTION_MASK_;
	}

	std::vector<std::string> dbpaths;
	if (p != p_end) {
		while (p != p_end) {
			size_t len;
			len = unserialise_length(&p, p_end, true);
			std::string dbpath(p, len);
			dbpaths.push_back(dbpath);
			p += len;
		}
	}
	select_db(dbpaths, false, xapian_flags);

	msg_update(message);
}


void
RemoteProtocol::msg_writeaccess(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_writeaccess(<message>)");

	int xapian_flags = Xapian::DB_OPEN;
	const char *p = message.c_str();
	const char *p_end = p + message.size();
	if (p != p_end) {
		unsigned flag_bits;
		flag_bits = static_cast<unsigned>(unserialise_length(&p, p_end));
		 xapian_flags |= flag_bits &~ DB_ACTION_MASK_;
	}

	std::vector<std::string> dbpaths;
	if (p != p_end) {
		size_t len;
		len = unserialise_length(&p, p_end, true);
		std::string dbpath(p, len);
		dbpaths.push_back(dbpath);
		p += len;
		if (p != p_end) {
			THROW(NetworkError, "only one database directory allowed on writable databases");
		}
	}
	select_db(dbpaths, true, xapian_flags);

	msg_update(message);
}


void
RemoteProtocol::msg_reopen(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_reopen(<message>)");

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
RemoteProtocol::msg_update(const std::string &)
{
	L_CALL("RemoteProtocol::msg_update(<message>)");

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
#if XAPIAN_MAJOR_VERSION <= 1 && XAPIAN_MINOR_VERSION <= 4 && XAPIAN_REVISION <= 4
		message += serialise_length(db()->get_avlength() * db()->get_doccount() + .5);
#else
		message += serialise_length(db()->get_total_length());
#endif
		std::string uuid = db()->get_uuid();
		message += uuid;

		lk_db.unlock();
	}

	send_message(RemoteReplyType::REPLY_UPDATE, message);
}


void
RemoteProtocol::init_msg_query()
{
	flags = DB_OPEN;
	_msg_query_database_lock.lock();
	_msg_query_matchspies.clear();
	_msg_query_reg = Xapian::Registry{};
	_msg_query_enquire.reset();
}


void
RemoteProtocol::reset()
{
	_msg_query_matchspies.clear();
	_msg_query_reg = Xapian::Registry{};
	_msg_query_enquire.reset();
	_msg_query_database_lock.unlock();
}


void
RemoteProtocol::msg_query(const std::string &message_in)
{
	L_CALL("RemoteProtocol::msg_query(<message>)");

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
	using sort_setting = enum { REL, VAL, VAL_REL, REL_VAL };

	Xapian::valueno sort_key = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));

	if (*p < '0' || *p > '3') {
		THROW(NetworkError, "bad message (sort_by)");
	}
	sort_setting sort_by;
	sort_by = static_cast<sort_setting>(*p++ - '0');

	if (*p < '0' || *p > '1') {
		THROW(NetworkError, "bad message (sort_value_forward)");
	}
	bool sort_value_forward(*p++ != '0');

	switch(sort_by) {
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
	}

	////////////////////////////////////////////////////////////////////////////
	// Time limit

	double time_limit = unserialise_double(&p, p_end);

	_msg_query_enquire->set_time_limit(time_limit);

	////////////////////////////////////////////////////////////////////////////
	// cutoff

	int percent_cutoff = *p++;
	if (percent_cutoff < 0 || percent_cutoff > 100) {
		THROW(NetworkError, "bad message (percent_cutoff)");
	}

	double weight_cutoff = unserialise_double(&p, p_end);
	if (weight_cutoff < 0) {
		THROW(NetworkError, "bad message (weight_cutoff)");
	}

	_msg_query_enquire->set_cutoff(percent_cutoff, weight_cutoff);

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
RemoteProtocol::msg_getmset(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_getmset(<message>)");

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
RemoteProtocol::msg_document(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_document(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);

	Xapian::Document doc = database()->get_document(did, false, true);

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
RemoteProtocol::msg_keepalive(const std::string &)
{
	L_CALL("RemoteProtocol::msg_keepalive(<message>)");

	reset();
	lock_database lk_db(this);

	// Ensure *our* database stays alive, as it may contain remote databases!
	db()->keep_alive();

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocol::msg_termexists(const std::string &term)
{
	L_CALL("RemoteProtocol::msg_termexists(<term>)");

	reset();
	lock_database lk_db(this);
	auto reply_type = db()->term_exists(term) ? RemoteReplyType::REPLY_TERMEXISTS : RemoteReplyType::REPLY_TERMDOESNTEXIST;
	lk_db.unlock();

	send_message(reply_type, std::string());
}


void
RemoteProtocol::msg_collfreq(const std::string &term)
{
	L_CALL("RemoteProtocol::msg_collfreq(<term>)");

	reset();
	lock_database lk_db(this);
	auto collection_freq = db()->get_collection_freq(term);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_COLLFREQ, serialise_length(collection_freq));
}


void
RemoteProtocol::msg_termfreq(const std::string &term)
{
	L_CALL("RemoteProtocol::msg_termfreq(<term>)");

	reset();
	lock_database lk_db(this);
	auto termfreq = db()->get_termfreq(term);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_TERMFREQ, serialise_length(termfreq));
}


void
RemoteProtocol::msg_freqs(const std::string &term)
{
	L_CALL("RemoteProtocol::msg_freqs(<term>)");

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
RemoteProtocol::msg_valuestats(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_valuestats(<message>)");

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
RemoteProtocol::msg_doclength(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_doclength(<message>)");

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
RemoteProtocol::msg_uniqueterms(const std::string &message)
{
	L_CALL("RemoteProtocol::msg_uniqueterms(<message>)");

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
RemoteProtocol::msg_commit(const std::string &)
{
	L_CALL("RemoteProtocol::msg_commit(<message>)");

	reset();
	lock_database lk_db(this);
	database()->commit();
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocol::msg_cancel(const std::string &)
{
	L_CALL("RemoteProtocol::msg_cancel(<message>)");

	reset();
	lock_database lk_db(this);
    // We can't call cancel since that's an internal method, but this
    // has the same effect with minimal additional overhead.
	database()->begin_transaction(false);
	database()->cancel_transaction();
}


void
RemoteProtocol::msg_adddocument(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_adddocument(<message>)");

	auto document = Xapian::Document::unserialise(message);

	reset();
	lock_database lk_db(this);
	auto did = database()->add_document(document);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}


void
RemoteProtocol::msg_deletedocument(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_deletedocument(<message>)");

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
RemoteProtocol::msg_deletedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_deletedocumentterm(<message>)");

	reset();
	lock_database lk_db(this);
	database()->delete_document_term(message);
}


void
RemoteProtocol::msg_replacedocument(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_replacedocument(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	reset();
	lock_database lk_db(this);
	database()->replace_document(did, document);
}


void
RemoteProtocol::msg_replacedocumentterm(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_replacedocumentterm(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t len = unserialise_length(&p, p_end, true);
	std::string unique_term(p, len);
	p += len;

	auto document = Xapian::Document::unserialise(std::string(p, p_end));

	reset();
	lock_database lk_db(this);
	auto did = database()->replace_document_term(unique_term, document);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}


void
RemoteProtocol::msg_getmetadata(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_getmetadata(<message>)");

	reset();
	lock_database lk_db(this);
	auto value = database()->get_metadata(message);
	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_METADATA, value);
}


void
RemoteProtocol::msg_openmetadatakeylist(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_openmetadatakeylist(<message>)");

	reset();
	lock_database lk_db(this);

	std::string prev = message;
	std::string reply;

	const std::string & prefix = message;
	const Xapian::TermIterator end = db()->metadata_keys_end(prefix);
	Xapian::TermIterator t = db()->metadata_keys_begin(prefix);
	for (; t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string & v = *t;
		size_t reuse = common_prefix_length(prev, v);
		reply.assign(1, char(reuse));
		reply.append(v, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_METADATAKEYLIST, reply);
		prev = v;
	}

	lk_db.unlock();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
RemoteProtocol::msg_setmetadata(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_setmetadata(<message>)");

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
RemoteProtocol::msg_addspelling(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_addspelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqinc = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	database()->add_spelling(std::string(p, p_end - p), freqinc);
}


void
RemoteProtocol::msg_removespelling(const std::string & message)
{
	L_CALL("RemoteProtocol::msg_removespelling(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqdec = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	reset();
	lock_database lk_db(this);
	database()->remove_spelling(std::string(p, p_end - p), freqdec);
}


void
RemoteProtocol::msg_shutdown(const std::string &)
{
	L_CALL("RemoteProtocol::msg_shutdown(<message>)");

	client.destroy();
	client.detach();
}


void
RemoteProtocol::select_db(const std::vector<std::string> &dbpaths, bool writable, int xapian_flags)
{
	L_CALL("RemoteProtocol::select_db(<dbpaths>, %s, %d)", writable ? "true" : "false", xapian_flags);

	reset();

	flags = writable ? DB_WRITABLE : DB_OPEN;
	if ((xapian_flags & Xapian::DB_CREATE_OR_OPEN) == Xapian::DB_CREATE_OR_OPEN) {
		flags |= DB_SPAWN;
	} else if ((xapian_flags & Xapian::DB_CREATE_OR_OVERWRITE) == Xapian::DB_CREATE_OR_OVERWRITE) {
		flags |= DB_SPAWN;
	} else if ((xapian_flags & Xapian::DB_CREATE) == Xapian::DB_CREATE) {
		flags |= DB_SPAWN;
	}

	if (!dbpaths.empty()) {
		if (writable) {
			assert(dbpaths.size() == 1); // Expecting exactly one database.
			endpoints.add(Endpoint(dbpaths[0]));
		} else {
			for (auto& path : dbpaths) {
				endpoints.add(Endpoint(path));
			}
		}
	}
}


#endif  /* XAPIAND_CLUSTERING */
