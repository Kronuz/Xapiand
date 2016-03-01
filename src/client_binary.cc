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

#include "client_binary.h"

#ifdef XAPIAND_CLUSTERING

#include "servers/server.h"
#include "servers/tcp_base.h"
#include "utils.h"
#include "length.h"
#include "io_utils.h"

#include <sysexits.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>



inline std::string::size_type
common_prefix_length(const std::string &a, const std::string &b)
{
	std::string::size_type minlen = std::min(a.size(), b.size());
	std::string::size_type common;
	for (common = 0; common < minlen; ++common) {
		if (a[common] != b[common]) break;
	}
	return common;
}

std::string
serialise_error(const Xapian::Error &e)
{
	// The byte before the type name is the type code.
	std::string result(1, (e.get_type())[-1]);
	result += serialise_length(e.get_context().length());
	result += e.get_context();
	result += serialise_length(e.get_msg().length());
	result += e.get_msg();
	// The "error string" goes last so we don't need to store its length.
	const char * err = e.get_error_string();
	if (err) result += err;
	return result;
}


#define SWITCH_TO_REPL '\xfe'


//
// Xapian binary client
//

BinaryClient::BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref *loop_, int sock_, double /*active_timeout_*/, double /*idle_timeout_*/)
	: BaseClient(std::move(server_), loop_, sock_),
	  running(0),
	  state(State::INIT),
	  writable(false),
	  database(nullptr)
{
	int binary_clients = ++XapiandServer::binary_clients;
	int total_clients = XapiandServer::total_clients;
	if (binary_clients > total_clients) {
		L_CRIT(this, "Inconsistency in number of binary clients");
		exit(EX_SOFTWARE);
	}

	L_CONN(this, "New Binary Client (sock=%d), %d client(s) of a total of %d connected.", sock, binary_clients, total_clients);

	L_OBJ(this, "CREATED BINARY CLIENT! (%d clients) [%llx]", binary_clients, this);
}


BinaryClient::~BinaryClient()
{
	checkin_database();

	int binary_clients = --XapiandServer::binary_clients;

	L_OBJ(this, "DELETED BINARY CLIENT! (%d clients left) [%llx]", binary_clients, this);
	if (binary_clients < 0) {
		L_CRIT(this, "Inconsistency in number of binary clients");
		exit(EX_SOFTWARE);
	}
}


bool
BinaryClient::init_remote()
{
	L_DEBUG(this, "init_remote");
	state = State::INIT;

	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


bool
BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_REPLICATION(this, "init_replication: %s  -->  %s", src_endpoint.as_string().c_str(), dst_endpoint.as_string().c_str());
	state = State::REPLICATIONPROTOCOL_CLIENT;

	repl_endpoints.add(src_endpoint);
	endpoints.add(dst_endpoint);

	writable = true;

	if (!manager()->database_pool.checkout(database, endpoints, DB_WRITABLE | DB_SPAWN | DB_REPLICATION, [
		manager=manager(),
		src_endpoint,
		dst_endpoint
	] {
		L_DEBUG(manager.get(), "Triggering replication for %s after checkin!", dst_endpoint.as_string().c_str());
		manager->trigger_replication(src_endpoint, dst_endpoint);
	})) {
		L_ERR(this, "Cannot checkout %s", endpoints.as_string().c_str());
		return false;
	}

	int port = (src_endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : src_endpoint.port;

	if ((sock = BaseTCP::connect(sock, src_endpoint.host, std::to_string(port))) < 0) {
		L_ERR(this, "Cannot connect to %s", src_endpoint.host.c_str(), std::to_string(port).c_str());
		checkin_database();
		return false;
	}
	L_CONN(this, "Connected to %s (sock=%d)!", src_endpoint.as_string().c_str(), sock);

	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


void
BinaryClient::on_read_file_done()
{
	L_CONN_WIRE(this, "BinaryClient::on_read_file_done");

	io::lseek(file_descriptor, 0, SEEK_SET);

	try {
		switch (state) {
			case State::REPLICATIONPROTOCOL_CLIENT:
				replication_client_file_done();
				break;
			default:
				L_ERR(this, "ERROR: Invalid on_read_file_done for state: %d", state);
				checkin_database();
				shutdown();
		};
	} catch (const Xapian::NetworkError& exc) {
		L_ERR(this, "ERROR: %s", exc.get_msg().c_str());
		checkin_database();
		shutdown();
	} catch (const std::exception& exc) {
		L_ERR(this, "ERROR: %s", exc.what());
		checkin_database();
		shutdown();
	} catch (...) {
		L_ERR(this, "ERROR: Unkown exception!");
		checkin_database();
		shutdown();
	}

	io::unlink(file_path);
}


void
BinaryClient::replication_client_file_done()
{
	L_REPLICATION(this, "BinaryClient::replication_client_file_done");

	char buf[1024];
	const char *p;
	const char *p_end;
	std::string buffer;

	ssize_t size = io::read(file_descriptor, buf, sizeof(buf));
	buffer.append(buf, size);
	p = buffer.data();
	p_end = p + buffer.size();
	const char *s = p;

	while (p != p_end) {
		ReplicationReplyType type = static_cast<ReplicationReplyType>(*p++);
		ssize_t len = unserialise_length(&p, p_end);
		size_t pos = p - s;
		while (p_end - p < len || static_cast<size_t>(p_end - p) < sizeof(buf) / 2) {
			size = io::read(file_descriptor, buf, sizeof(buf));
			if (!size) break;
			buffer.append(buf, size);
			s = p = buffer.data();
			p_end = p + buffer.size();
			p += pos;
		}
		if (p_end - p < len) {
			throw MSG_Error("Replication failure!");
		}
		std::string msg(p, len);
		p += len;

		replication_client(type, msg);

		buffer.erase(0, p - s);
		s = p = buffer.data();
		p_end = p + buffer.size();
	}
}


void
BinaryClient::on_read_file(const char *buf, size_t received)
{
	L_CONN_WIRE(this, "BinaryClient::on_read_file: %zu bytes", received);
	io::write(file_descriptor, buf, received);
}


void
BinaryClient::on_read(const char *buf, size_t received)
{
	L_CONN_WIRE(this, "BinaryClient::on_read: %zu bytes", received);
	buffer.append(buf, received);
	while (buffer.length() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		char type = *p++;

		ssize_t len;
		try {
			len = unserialise_length(&p, p_end, true);
		} catch (Xapian::SerialisationError) {
			return;
		}
		std::string data = std::string(p, len);
		buffer.erase(0, p - o + len);

		L_BINARY(this, "on_read message: '\\%02x' (state=0x%x)", type, state);
		switch (type) {
			case SWITCH_TO_REPL:
				state = State::REPLICATIONPROTOCOL_SERVER;  // Switch to replication protocol
				type = toUType(ReplicationMessageType::MSG_GET_CHANGESETS);
				L_BINARY(this, "Switched to replication protocol");
				break;
		}

		messages_queue.push(std::make_unique<Buffer>(type, data.c_str(), data.size()));
	}

	if (!messages_queue.empty()) {
		if (!running) {
			manager()->thread_pool.enqueue(share_this<BinaryClient>());
		}
	}
}


char
BinaryClient::get_message(std::string &result, char max_type)
{
	std::unique_ptr<Buffer> msg;
	if (!messages_queue.pop(msg)) {
		throw Xapian::NetworkError("No message available");
	}

	char type = msg->type;

	if (type >= max_type) {
		std::string errmsg("Invalid message type ");
		errmsg += std::to_string(int(type));
		throw Xapian::InvalidArgumentError(errmsg);
	}

	const char *msg_str = msg->dpos();
	size_t msg_size = msg->nbytes();
	result.assign(msg_str, msg_size);

	std::string buf;
	buf += type;

	buf += serialise_length(msg_size);
	buf += result;

	return type;
}


void
BinaryClient::send_message(char type_as_char, const std::string &message, double)
{
	std::string buf;
	buf += type_as_char;

	buf += serialise_length(message.size());
	buf += message;

	write(buf);
}


void
BinaryClient::shutdown()
{
	L_CALL(this, "BinaryClient::shutdown()");

	BaseClient::shutdown();
}



void
BinaryClient::checkout_database()
{
	if (!database) {
		if (!manager()->database_pool.checkout(database, endpoints, (writable ? DB_WRITABLE : 0) | DB_SPAWN)) {
			throw Xapian::InvalidOperationError("Server has no open database");
		}
	}
}


void
BinaryClient::checkin_database()
{
	if (database) {
		manager()->database_pool.checkin(database);
		database.reset();
	}
	enquire.reset();
	matchspies.clear();
}


void
BinaryClient::run()
{
	L_OBJ_BEGIN(this, "BinaryClient::run:BEGIN");
	if (running++) {
		--running;
		L_OBJ_END(this, "BinaryClient::run:END");
		return;
	}

	if (state == State::INIT) {
		state = State::REMOTEPROTOCOL_SERVER;
		msg_update(std::string());
	}

	while (!messages_queue.empty()) {
		try {
			switch (state) {
				case State::INIT:
					L_ERR(this, "Unexpected INIT!");
				case State::REMOTEPROTOCOL_SERVER: {
					std::string message;
					RemoteMessageType type = static_cast<RemoteMessageType>(get_message(message, static_cast<char>(RemoteMessageType::MSG_MAX)));
					L_BINARY(this, ">> get_message(%s)", RemoteMessageTypeNames[static_cast<int>(type)]);
					L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
					remote_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_SERVER: {
					std::string message;
					ReplicationMessageType type = static_cast<ReplicationMessageType>(get_message(message, static_cast<char>(ReplicationMessageType::MSG_MAX)));
					L_BINARY(this, ">> get_message(%s)", ReplicationMessageTypeNames[static_cast<int>(type)]);
					L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
					replication_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_CLIENT: {
					std::string message;
					ReplicationReplyType type = static_cast<ReplicationReplyType>(get_message(message, static_cast<char>(ReplicationReplyType::REPLY_MAX)));
					L_BINARY(this, ">> get_message(%s)", ReplicationReplyTypeNames[static_cast<int>(type)]);
					L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
					replication_client(type, message);
					break;
				}
			}
		} catch (const Xapian::NetworkTimeoutError& exc) {
			try {
				// We've had a timeout, so the client may not be listening, so
				// set the end_time to 1 and if we can't send the message right
				// away, just exit and the client will cope.
				send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc), 1.0);
			} catch (...) {}
			checkin_database();
			shutdown();
		} catch (const Xapian::NetworkError& exc) {
			L_EXC(this, "ERROR: %s", exc.get_msg().empty() ? "Unkown Xapian error!" : exc.get_msg().c_str());
			checkin_database();
			shutdown();
		} catch (const Xapian::Error& exc) {
			// Propagate the exception to the client, then return to the main
			// message handling loop.
			send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
			checkin_database();
		} catch (const Error& exc) {
			L_EXC(this, "ERROR: %s", exc.get_context());
			send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (const std::exception& exc) {
			L_EXC(this, "ERROR: %s", *exc.what() ? exc.what() : "Unkown exception!");
			send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (...) {
			L_ERR(this, "ERROR: %s", "Unkown error!");
			send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		}
	}

	--running;
	L_OBJ_END(this, "BinaryClient::run:END");
}


//  ____                      _       ____            _                  _
// |  _ \ ___ _ __ ___   ___ | |_ ___|  _ \ _ __ ___ | |_ ___   ___ ___ | |
// | |_) / _ \ '_ ` _ \ / _ \| __/ _ \ |_) | '__/ _ \| __/ _ \ / __/ _ \| |
// |  _ <  __/ | | | | | (_) | ||  __/  __/| | | (_) | || (_) | (_| (_) | |
// |_| \_\___|_| |_| |_|\___/ \__\___|_|   |_|  \___/ \__\___/ \___\___/|_|
//
////////////////////////////////////////////////////////////////////////////////
void
BinaryClient::remote_server(RemoteMessageType type, const std::string &message)
{

	static const dispatch_func dispatch[] = {
		&BinaryClient::msg_allterms,
		&BinaryClient::msg_collfreq,
		&BinaryClient::msg_document,
		&BinaryClient::msg_termexists,
		&BinaryClient::msg_termfreq,
		&BinaryClient::msg_valuestats,
		&BinaryClient::msg_keepalive,
		&BinaryClient::msg_doclength,
		&BinaryClient::msg_query,
		&BinaryClient::msg_termlist,
		&BinaryClient::msg_positionlist,
		&BinaryClient::msg_postlist,
		&BinaryClient::msg_reopen,
		&BinaryClient::msg_update,
		&BinaryClient::msg_adddocument,
		&BinaryClient::msg_cancel,
		&BinaryClient::msg_deletedocumentterm,
		&BinaryClient::msg_commit,
		&BinaryClient::msg_replacedocument,
		&BinaryClient::msg_replacedocumentterm,
		&BinaryClient::msg_deletedocument,
		&BinaryClient::msg_writeaccess,
		&BinaryClient::msg_getmetadata,
		&BinaryClient::msg_setmetadata,
		&BinaryClient::msg_addspelling,
		&BinaryClient::msg_removespelling,
		&BinaryClient::msg_getmset,
		&BinaryClient::msg_shutdown,
		&BinaryClient::msg_openmetadatakeylist,
		&BinaryClient::msg_freqs,
		&BinaryClient::msg_uniqueterms,
		&BinaryClient::msg_select,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			throw Xapian::InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		checkin_database();
		throw;
	}
}

void
BinaryClient::msg_allterms(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	std::string prev = message;
	std::string reply;

	const std::string & prefix = message;
	const Xapian::TermIterator end = db->allterms_end(prefix);
	for (Xapian::TermIterator t = db->allterms_begin(prefix); t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string & v = *t;
		size_t reuse = common_prefix_length(prev, v);
		reply = serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply.append(v, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_ALLTERMS, reply);
		prev = v;
	}

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_termlist(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	send_message(RemoteReplyType::REPLY_DOCLENGTH, serialise_length(db->get_doclength(did)));
	std::string prev;
	const Xapian::TermIterator end = db->termlist_end(did);
	for (Xapian::TermIterator t = db->termlist_begin(did); t != end; ++t) {
		if unlikely(prev.size() > 255)
			prev.resize(255);
		const std::string & v = *t;
		size_t reuse = common_prefix_length(prev, v);
		std::string reply = serialise_length(t.get_wdf());
		reply += serialise_length(t.get_termfreq());
		reply.append(1, char(reuse));
		reply.append(v, reuse, std::string::npos);
		send_message(RemoteReplyType::REPLY_TERMLIST, reply);
		prev = v;
	}

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_positionlist(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
	std::string term(p, p_end - p);

	Xapian::termpos lastpos = static_cast<Xapian::termpos>(-1);
	const Xapian::PositionIterator end = db->positionlist_end(did, term);
	for (Xapian::PositionIterator i = db->positionlist_begin(did, term);
		 i != end; ++i) {
		Xapian::termpos pos = *i;
		send_message(RemoteReplyType::REPLY_POSITIONLIST, serialise_length(pos - lastpos - 1));
		lastpos = pos;
	}

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}


void
BinaryClient::msg_select(const std::string &message)
{
	const char *p = message.c_str();
	const char *p_end = p + message.size();

	writable = false;
	endpoints.clear_all();
	while (p != p_end) {
		size_t len = unserialise_length(&p, p_end, true);
		endpoints.add(Endpoint(std::string(p, len)));
		p += len;
	}

	msg_update(message);
}


void
BinaryClient::msg_postlist(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const std::string & term = message;

	Xapian::doccount termfreq = db->get_termfreq(term);
	Xapian::termcount collfreq = db->get_collection_freq(term);
	send_message(RemoteReplyType::REPLY_POSTLISTSTART, serialise_length(termfreq) + serialise_length(collfreq));

	Xapian::docid lastdocid = 0;
	const Xapian::PostingIterator end = db->postlist_end(term);
	for (Xapian::PostingIterator i = db->postlist_begin(term);
		 i != end; ++i) {

		Xapian::docid newdocid = *i;
		std::string reply = serialise_length(newdocid - lastdocid - 1);
		reply += serialise_length(i.get_wdf());

		send_message(RemoteReplyType::REPLY_POSTLISTITEM, reply);
		lastdocid = newdocid;
	}

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_writeaccess(const std::string & msg)
{
	writable = true;

	int flags = Xapian::DB_OPEN;
	const char *p = msg.c_str();
	const char *p_end = p + msg.size();
	if (p != p_end) {
		unsigned flag_bits = static_cast<unsigned>(unserialise_length(&p, p_end));
		flags |= flag_bits;
		if (p != p_end) {
			throw Xapian::NetworkError("Junk at end of MSG_WRITEACCESS");
		}
	}
	// flags &= ~Xapian::DB_ACTION_MASK_;

	msg_update(msg);
}


void
BinaryClient::msg_reopen(const std::string & msg)
{
	checkout_database();

	if (!database->reopen()) {

		checkin_database();

		send_message(RemoteReplyType::REPLY_DONE, std::string());
	} else {

		checkin_database();

		msg_update(msg);
	}
}

void
BinaryClient::msg_update(const std::string &)
{
	static const char protocol[2] = {
		char(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION),
		char(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION)
	};

	std::string message(protocol, 2);

	if (!endpoints.empty()) {
		checkout_database();

		Xapian::Database* db = database->db.get();

		Xapian::doccount num_docs = db->get_doccount();
		message += serialise_length(num_docs);
		message += serialise_length(db->get_lastdocid() - num_docs);
		Xapian::termcount doclen_lb = db->get_doclength_lower_bound();
		message += serialise_length(doclen_lb);
		message += serialise_length(db->get_doclength_upper_bound() - doclen_lb);
		message += (db->has_positions() ? '1' : '0');
		// FIXME: clumsy to reverse calculate total_len like this:
		message += serialise_length(db->get_avlength() * db->get_doccount() + .5);
		//message += serialise_length(db->get_total_length());
		std::string uuid = db->get_uuid();
		message += uuid;

		checkin_database();
	}

	send_message(RemoteReplyType::REPLY_UPDATE, message);
}

void
BinaryClient::msg_query(const std::string &message_in)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message_in.c_str();
	const char *p_end = p + message_in.size();

	enquire = std::make_unique<Xapian::Enquire>(*db);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Query.
	size_t len = unserialise_length(&p, p_end, true);
	Xapian::Query query(Xapian::Query::unserialise(std::string(p, len), reg));
	p += len;

	// Unserialise assorted Enquire settings.
	Xapian::termcount qlen = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	enquire->set_query(query, qlen);

	////////////////////////////////////////////////////////////////////////////
	// Collapse key
	Xapian::valueno collapse_max = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));

	Xapian::valueno collapse_key = Xapian::BAD_VALUENO;
	if (collapse_max) {
		collapse_key = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));
	}

	enquire->set_collapse_key(collapse_key, collapse_max);

	////////////////////////////////////////////////////////////////////////////
	// docid order

	if (p_end - p < 4 || *p < '0' || *p > '2') {
		throw Xapian::NetworkError("bad message (docid_order)");
	}
	Xapian::Enquire::docid_order order;
	order = static_cast<Xapian::Enquire::docid_order>(*p++ - '0');

	enquire->set_docid_order(order);

	////////////////////////////////////////////////////////////////////////////
	// Sort by
	typedef enum { REL, VAL, VAL_REL, REL_VAL } sort_setting;

	Xapian::valueno sort_key = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));

	if (*p < '0' || *p > '3') {
		throw Xapian::NetworkError("bad message (sort_by)");
	}
	sort_setting sort_by;
	sort_by = static_cast<sort_setting>(*p++ - '0');

	if (*p < '0' || *p > '1') {
		throw Xapian::NetworkError("bad message (sort_value_forward)");
	}
	bool sort_value_forward(*p++ != '0');

	switch(sort_by) {
		case REL:
			enquire->set_sort_by_relevance();
			break;
		case VAL:
			enquire->set_sort_by_value(sort_key, sort_value_forward);
			break;
		case VAL_REL:
			enquire->set_sort_by_value_then_relevance(sort_key, sort_value_forward);
			break;
		case REL_VAL:
			enquire->set_sort_by_relevance_then_value(sort_key, sort_value_forward);
			break;
	}

	////////////////////////////////////////////////////////////////////////////
	// Time limit

	double time_limit = unserialise_double(&p, p_end);

	enquire->set_time_limit(time_limit);

	////////////////////////////////////////////////////////////////////////////
	// cutoff

	int percent_cutoff = *p++;
	if (percent_cutoff < 0 || percent_cutoff > 100) {
		throw Xapian::NetworkError("bad message (percent_cutoff)");
	}

	double weight_cutoff = unserialise_double(&p, p_end);
	if (weight_cutoff < 0) {
		throw Xapian::NetworkError("bad message (weight_cutoff)");
	}

	enquire->set_cutoff(percent_cutoff, weight_cutoff);

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the Weight object.
	len = unserialise_length(&p, p_end, true);
	std::string wtname(p, len);
	p += len;

	const Xapian::Weight * wttype = reg.get_weighting_scheme(wtname);
	if (wttype == nullptr) {
		// Note: user weighting schemes should be registered by adding them to
		// a Registry, and setting the context using
		// RemoteServer::set_registry().
		throw Xapian::InvalidArgumentError("Weighting scheme " +
										   wtname + " not registered");
	}

	len = unserialise_length(&p, p_end, true);
	wttype = wttype->unserialise(std::string(p, len));

	enquire->set_weighting_scheme(*wttype);
	p += len;

	////////////////////////////////////////////////////////////////////////////
	// Unserialise the RSet object.
	len = unserialise_length(&p, p_end, true);
	Xapian::RSet rset = Xapian::RSet::unserialise(std::string(p, len));
	p += len;

	////////////////////////////////////////////////////////////////////////////
	// Unserialise any MatchSpy objects.
	matchspies.clear();
	while (p != p_end) {
		len = unserialise_length(&p, p_end, true);
		std::string spytype(p, len);
		const Xapian::MatchSpy * spyclass = reg.get_match_spy(spytype);
		if (spyclass == nullptr) {
			throw Xapian::InvalidArgumentError("Match spy " + spytype +
											   " not registered");
		}
		p += len;

		len = unserialise_length(&p, p_end, true);
		Xapian::MatchSpy *spy = spyclass->unserialise(std::string(p, len), reg);
		matchspies.push_back(std::unique_ptr<Xapian::MatchSpy>(spy));
		enquire->add_matchspy(spy);
		p += len;
	}

	////////////////////////////////////////////////////////////////////////////
	enquire->prepare_mset(&rset, nullptr);

	send_message(RemoteReplyType::REPLY_STATS, enquire->get_stats());

	// No checkout for database (it'll still be needed by msg_getmset)
}

void
BinaryClient::msg_getmset(const std::string & msg)
{
	if (!enquire) {
		throw Xapian::NetworkError("Unexpected MSG_GETMSET");
	}

	const char *p = msg.c_str();
	const char *p_end = p + msg.size();

	Xapian::termcount first = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
	Xapian::termcount maxitems = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	Xapian::termcount check_at_least = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));

	enquire->set_stats(std::string(p, p_end));

	Xapian::MSet mset = enquire->get_mset(first, maxitems, check_at_least);

	std::string message;
	for (auto& i : matchspies) {
		std::string spy_results = i->serialise_results();
		message += serialise_length(spy_results.size());
		message += spy_results;
	}
	message += mset.serialise();

	checkin_database();

	send_message(RemoteReplyType::REPLY_RESULTS, message);
}

void
BinaryClient::msg_document(const std::string &message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	Xapian::Document doc;
	if (!database->get_document(did, doc)) {
		throw Xapian::NetworkError("Cannot get document");
	}

	send_message(RemoteReplyType::REPLY_DOCDATA, doc.get_data());

	Xapian::ValueIterator i;
	for (i = doc.values_begin(); i != doc.values_end(); ++i) {
		std::string item = serialise_length(i.get_valueno());
		item += *i;
		send_message(RemoteReplyType::REPLY_VALUE, item);
	}

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_keepalive(const std::string &)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	// Ensure *our* database stays alive, as it may contain remote databases!
	db->keep_alive();
	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_termexists(const std::string &term)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	checkin_database();

	send_message((db->term_exists(term) ? RemoteReplyType::REPLY_TERMEXISTS : RemoteReplyType::REPLY_TERMDOESNTEXIST), std::string());
}

void
BinaryClient::msg_collfreq(const std::string &term)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	checkin_database();

	send_message(RemoteReplyType::REPLY_COLLFREQ, serialise_length(db->get_collection_freq(term)));
}

void
BinaryClient::msg_termfreq(const std::string &term)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	checkin_database();

	send_message(RemoteReplyType::REPLY_TERMFREQ, serialise_length(db->get_termfreq(term)));
}

void
BinaryClient::msg_freqs(const std::string &term)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	std::string msg = serialise_length(db->get_termfreq(term));
	msg += serialise_length(db->get_collection_freq(term));

	checkin_database();

	send_message(RemoteReplyType::REPLY_FREQS, msg);
}

void
BinaryClient::msg_valuestats(const std::string & message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message.data();
	const char *p_end = p + message.size();
	while (p != p_end) {
		Xapian::valueno slot = static_cast<Xapian::valueno>(unserialise_length(&p, p_end));
		std::string message_out;
		message_out += serialise_length(db->get_value_freq(slot));
		std::string bound = db->get_value_lower_bound(slot);
		message_out += serialise_length(bound.size());
		message_out += bound;
		bound = db->get_value_upper_bound(slot);
		message_out += serialise_length(bound.size());
		message_out += bound;

		send_message(RemoteReplyType::REPLY_VALUESTATS, message_out);
	}

	checkin_database();
}

void
BinaryClient::msg_doclength(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	checkin_database();

	send_message(RemoteReplyType::REPLY_DOCLENGTH, serialise_length(db->get_doclength(did)));
}

void
BinaryClient::msg_uniqueterms(const std::string &message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	checkin_database();

	send_message(RemoteReplyType::REPLY_UNIQUETERMS, serialise_length(db->get_unique_terms(did)));
}

void
BinaryClient::msg_commit(const std::string &)
{
	checkout_database();

	database->commit();

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_cancel(const std::string &)
{
	checkout_database();

	database->cancel();

	checkin_database();
}

void
BinaryClient::msg_adddocument(const std::string & message)
{
	checkout_database();

	Xapian::docid did = database->add_document(Xapian::Document::unserialise(message));

	checkin_database();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}

void
BinaryClient::msg_deletedocument(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	database->delete_document(did);

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_deletedocumentterm(const std::string & message)
{
	checkout_database();

	database->delete_document_term(message);

	checkin_database();
}

void
BinaryClient::msg_replacedocument(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::docid did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));

	database->replace_document(did, Xapian::Document::unserialise(std::string(p, p_end)));

	checkin_database();
}

void
BinaryClient::msg_replacedocumentterm(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t len = unserialise_length(&p, p_end, true);
	std::string unique_term(p, len);
	p += len;

	Xapian::docid did = database->replace_document_term(unique_term, Xapian::Document::unserialise(std::string(p, p_end)));

	checkin_database();

	send_message(RemoteReplyType::REPLY_ADDDOCUMENT, serialise_length(did));
}

void
BinaryClient::msg_getmetadata(const std::string & message)
{
	checkout_database();

	std::string value;
	database->get_metadata(message, value);

	checkin_database();

	send_message(RemoteReplyType::REPLY_METADATA, value);
}

void
BinaryClient::msg_openmetadatakeylist(const std::string & message)
{
	checkout_database();
	Xapian::Database* db = database->db.get();

	std::string prev = message;
	std::string reply;

	const std::string & prefix = message;
	const Xapian::TermIterator end = db->metadata_keys_end(prefix);
	Xapian::TermIterator t = db->metadata_keys_begin(prefix);
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

	checkin_database();

	send_message(RemoteReplyType::REPLY_DONE, std::string());
}

void
BinaryClient::msg_setmetadata(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t keylen = unserialise_length(&p, p_end, true);
	std::string key(p, keylen);
	p += keylen;
	std::string val(p, p_end - p);
	database->set_metadata(key, val);

	checkin_database();
}

void
BinaryClient::msg_addspelling(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqinc = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
	database->add_spelling(std::string(p, p_end - p), freqinc);

	checkin_database();
}

void
BinaryClient::msg_removespelling(const std::string & message)
{
	checkout_database();

	const char *p = message.data();
	const char *p_end = p + message.size();
	Xapian::termcount freqdec = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
	database->remove_spelling(std::string(p, p_end - p), freqdec);

	checkin_database();
}

void
BinaryClient::msg_shutdown(const std::string &)
{
	shutdown();
}


//  ____            _ _           _   _
// |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
// | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
// |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
// |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
//           |_|
////////////////////////////////////////////////////////////////////////////////

void
BinaryClient::replication_server(ReplicationMessageType type, const std::string &message)
{
	static const dispatch_func dispatch[] = {
		&BinaryClient::msg_get_changesets,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			throw Xapian::InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		checkin_database();
		throw;
	}
}

void
BinaryClient::msg_get_changesets(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::msg_get_changesets");

	// Xapian::Database *db_;
	// const char *p = message.c_str();
	// const char *p_end = p + message.size();

	// size_t len = unserialise_length(&p, p_end, true);
	// std::string uuid(p, len);
	// p += len;

	// len = unserialise_length(&p, p_end, true);
	// std::string from_revision(p, len);
	// p += len;

	// len = unserialise_length(&p, p_end, true);
	// std::string index_path(p, len);
	// p += len;

	// // Select endpoints and get database
	// try {
	// 	endpoints.clear_all();
	// 	Endpoint endpoint(index_path);
	// 	endpoints.add(endpoint);
	// 	db_ = get_db();
	// 	if (!db_)
	// 		throw Xapian::InvalidOperationError("Server has no open database");
	// } catch (...) {
	// 	throw;
	// }

	// char path[] = "/tmp/xapian_changesets_sent.XXXXXX";
	// int fd = mkstemp(path);
	// try {
	// 	std::string to_revision = databases[db_]->checkout_revision;
	// 	L_REPLICATION(this, "BinaryClient::msg_get_changesets for %s (%s) from rev:%s to rev:%s [%d]", endpoints.as_string().c_str(), uuid.c_str(), repr(from_revision, false).c_str(), repr(to_revision, false).c_str(), need_whole_db);

	// 	if (fd < 0) {
	// 		L_ERR(this, "Cannot write to %s (1)", path);
	// 		return;
	// 	}
	// 	// db_->write_changesets_to_fd(fd, from_revision, uuid != db_->get_uuid());  // FIXME: Implement Replication
	// } catch (...) {
	// 	release_db(db_);
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// }
	// release_db(db_);

	// send_file(fd);

	// io::close(fd);
	// io::unlink(path);
}


void
BinaryClient::replication_client(ReplicationReplyType type, const std::string &message)
{
	static const dispatch_func dispatch[] = {
		&BinaryClient::reply_end_of_changes,
		&BinaryClient::reply_fail,
		&BinaryClient::reply_db_header,
		&BinaryClient::reply_db_filename,
		&BinaryClient::reply_db_filedata,
		&BinaryClient::reply_db_footer,
		&BinaryClient::reply_changeset,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			throw Xapian::InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		checkin_database();
		throw;
	}
}


void
BinaryClient::reply_end_of_changes(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_end_of_changes");

	// if (repl_switched_db) {
	// 	manager()->database_pool.switch_db(*endpoints.cbegin());
	// }

	// checkin_database();

	// shutdown();
}


void
BinaryClient::reply_fail(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_fail");

	// L_ERR(this, "Replication failure!");
	// checkin_database();

	// shutdown();
}


void
BinaryClient::reply_db_header(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_db_header");

	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// size_t length = unserialise_length(&p, p_end, true);
	// repl_db_uuid.assign(p, length);
	// p += length;
	// repl_db_revision = unserialise_length(&p, p_end);
	// repl_db_filename.clear();

	// Endpoint& endpoint = endpoints[0];
	// std::string path_tmp = endpoint.path + "/.tmp";

	// int dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	// if (dir == 0) {
	// 	L_DEBUG(this, "Directory %s created", path_tmp.c_str());
	// } else if (errno == EEXIST) {
	// 	delete_files(path_tmp.c_str());
	// 	dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	// 	if (dir == 0) {
	// 		L_DEBUG(this, "Directory %s created", path_tmp.c_str());
	// 	}
	// } else {
	// 	L_ERR(this, "Directory %s not created (%s)", path_tmp.c_str(), strerror(errno));
	// }
}


void
BinaryClient::reply_db_filename(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_db_filename");

	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// repl_db_filename.assign(p, p_end - p);
}


void
BinaryClient::reply_db_filedata(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_db_filedata");

	// const char *p = message.data();
	// const char *p_end = p + message.size();

	// const Endpoint& endpoint = endpoints[0];

	// std::string path = endpoint.path + "/.tmp/";
	// std::string path_filename = path + repl_db_filename;

	// int fd = io::open(path_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	// if (fd >= 0) {
	// 	L_REPLICATION(this, "path_filename %s", path_filename.c_str());
	// 	if (io::write(fd, p, p_end - p) != p_end - p) {
	// 		L_ERR(this, "Cannot write to %s", repl_db_filename.c_str());
	// 		return;
	// 	}
	// 	io::close(fd);
	// }
}


void
BinaryClient::reply_db_footer(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_db_footer");

	// // const char *p = message.data();
	// // const char *p_end = p + message.size();
	// // size_t revision = unserialise_length(&p, p_end);
	// // Indicates the end of a DB copy operation, signal switch

	// Endpoints endpoints_tmp;
	// Endpoint& endpoint_tmp = endpoints[0];
	// endpoint_tmp.path.append("/.tmp");
	// endpoints_tmp.insert(endpoint_tmp);

	// if (!repl_database_tmp) {
	// 	if (!manager()->database_pool.checkout(repl_database_tmp, endpoints_tmp, DB_WRITABLE | DB_VOLATILE)) {
	// 		L_ERR(this, "Cannot checkout tmp %s", endpoint_tmp.path.c_str());
	// 	}
	// }

	// repl_switched_db = true;
	// repl_just_switched_db = true;
}


void
BinaryClient::reply_changeset(const std::string &)
{
	L_REPLICATION(this, "BinaryClient::reply_changeset");

	// Xapian::WritableDatabase *wdb_;
	// if (repl_database_tmp) {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database_tmp->db.get());
	// } else {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(database->db.get());
	// }

	// char path[] = "/tmp/xapian_changes.XXXXXX";
	// int fd = mkstemp(path);
	// if (fd < 0) {
	// 	L_ERR(this, "Cannot write to %s (1)", path);
	// 	return;
	// }

	// std::string header;
	// header += toUType(ReplicationMessageType::REPLY_CHANGESET);
	// header += serialise_length(message.size());

	// if (io::write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
	// 	L_ERR(this, "Cannot write to %s (2)", path);
	// 	return;
	// }

	// if (io::write(fd, message.data(), message.size()) != static_cast<ssize_t>(message.size())) {
	// 	L_ERR(this, "Cannot write to %s (3)", path);
	// 	return;
	// }

	// io::lseek(fd, 0, SEEK_SET);

	// try {
	// 	// wdb_->apply_changeset_from_fd(fd, !repl_just_switched_db);  // FIXME: Implement Replication
	// 	repl_just_switched_db = false;
	// } catch(const Xapian::NetworkError& exc) {
	// 	L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// } catch (const Xapian::DatabaseError& exc) {
	// 	L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// } catch (...) {
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// }

	// io::close(fd);
	// io::unlink(path);
}

#endif  /* XAPIAND_CLUSTERING */
