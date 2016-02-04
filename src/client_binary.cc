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

#ifdef HAVE_REMOTE_PROTOCOL

#include "servers/server.h"
#include "servers/tcp_base.h"
#include "utils.h"
#include "length.h"
#include "io_utils.h"

#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


#define SWITCH_TO_REPL '\xfe'
#define SWITCH_TO_STORING '\xfd'


//
// Xapian binary client
//

BinaryClient::BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref *loop_, int sock_, double active_timeout_, double idle_timeout_)
	: BaseClient(std::move(server_), loop_, sock_),
	  RemoteProtocol(std::vector<std::string>(), active_timeout_, idle_timeout_, true),
	  running(0),
	  state(State::INIT_REMOTEPROTOCOL),
	  repl_database(nullptr),
	  repl_database_tmp(nullptr),
	  repl_switched_db(false),
	  storing_database(NULL),
	  storing_offset(0),
	  storing_cookie(0)
{
	int binary_clients = ++XapiandServer::binary_clients;
	int total_clients = XapiandServer::total_clients;
	assert(binary_clients <= total_clients);

	L_CONN(this, "New Binary Client (sock=%d), %d client(s) of a total of %d connected.", sock, binary_clients, total_clients);

	L_OBJ(this, "CREATED BINARY CLIENT! (%d clients) [%llx]", binary_clients, this);
}


BinaryClient::~BinaryClient()
{
	for (auto& database : databases) {
		manager()->database_pool.checkin(database.second);
	}

	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
	}

	if (repl_database_tmp) {
		manager()->database_pool.checkin(repl_database_tmp);
	}

	if (storing_database) {
		manager()->database_pool.checkin(storing_database);
	}

	int binary_clients = --XapiandServer::binary_clients;

	L_OBJ(this, "DELETED BINARY CLIENT! (%d clients left) [%llx]", binary_clients, this);
	assert(binary_clients >= 0);
}


bool
BinaryClient::init_remote()
{
	L_DEBUG(this, "init_remote");
	state = State::INIT_REMOTEPROTOCOL;

	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


bool
BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_REPLICATION(this, "init_replication: %s  -->  %s", src_endpoint.as_string().c_str(), dst_endpoint.as_string().c_str());
	state = State::REPLICATIONPROTOCOL_SLAVE;

	repl_endpoints.insert(src_endpoint);
	endpoints.insert(dst_endpoint);

	if (!manager()->database_pool.checkout(repl_database, endpoints, DB_WRITABLE | DB_SPAWN | DB_REPLICATION, [
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
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
		return false;
	}
	L_CONN(this, "Connected to %s (sock=%d)!", src_endpoint.as_string().c_str(), sock);

	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


bool
BinaryClient::init_storing(const Endpoints &endpoints_, const Xapian::docid &did, const std::string &filename)
{
	L_DEBUG(this, "init_storing: %s  -->  %s", filename.c_str(), endpoints_.as_string().c_str());
	state = State::STORINGPROTOCOL_SENDER;

	storing_id = did;
	storing_filename = filename;
	storing_endpoint = *endpoints_.begin();

	int port = (storing_endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : storing_endpoint.port;

	if ((sock = BaseTCP::connect(sock, storing_endpoint.host.c_str(), std::to_string(port).c_str())) < 0) {
		L_ERR(this, "Cannot connect to %s", storing_endpoint.host.c_str(), std::to_string(port).c_str());
		return false;
	}
	L_CONN(this, "Connected to %s (sock=%d)!", storing_endpoint.as_string().c_str(), sock);

	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


void
BinaryClient::on_read_file_done()
{
	L_CONN_WIRE(this, "BinaryClient::on_read_file_done");

	::lseek(file_descriptor, 0, SEEK_SET);

	try {
		switch (state) {
			case State::REPLICATIONPROTOCOL_SLAVE:
				repl_file_done();
				break;
			case State::STORINGPROTOCOL_RECEIVER:
				storing_file_done();
				break;
			default:
				L_ERR(this, "ERROR: Invalid on_read_file_done for state: %d", state);
				if (repl_database) {
					manager()->database_pool.checkin(repl_database);
					repl_database.reset();
				}
				if (repl_database_tmp) {
					manager()->database_pool.checkin(repl_database_tmp);
					repl_database_tmp.reset();
				}
				shutdown();
		};
	} catch (const Xapian::NetworkError &e) {
		L_ERR(this, "ERROR: %s", e.get_msg().c_str());
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		if (repl_database_tmp) {
			manager()->database_pool.checkin(repl_database_tmp);
			repl_database_tmp.reset();
		}
		shutdown();
	} catch (const std::exception &err) {
		L_ERR(this, "ERROR: %s", err.what());
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		if (repl_database_tmp) {
			manager()->database_pool.checkin(repl_database_tmp);
			repl_database_tmp.reset();
		}
		shutdown();
	} catch (...) {
		L_ERR(this, "ERROR: Unkown exception!");
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		if (repl_database_tmp) {
			manager()->database_pool.checkin(repl_database_tmp);
			repl_database_tmp.reset();
		}
		shutdown();
	}

	::unlink(file_path);
}


void
BinaryClient::repl_file_done()
{
	L_REPLICATION(this, "BinaryClient::repl_file_done");

	char buf[1024];
	const char *p;
	const char *p_end;
	std::string buffer;

	ssize_t size = ::read(file_descriptor, buf, sizeof(buf));
	buffer.append(buf, size);
	p = buffer.data();
	p_end = p + buffer.size();
	const char *s = p;

	while (p != p_end) {
		char type_as_char = *p++;
		ssize_t len = decode_length(&p, p_end);
		size_t pos = p - s;
		while (p_end - p < len || static_cast<size_t>(p_end - p) < sizeof(buf) / 2) {
			size = ::read(file_descriptor, buf, sizeof(buf));
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

		repl_apply((ReplicateType)type_as_char, msg);

		buffer.erase(0, p - s);
		s = p = buffer.data();
		p_end = p + buffer.size();
	}
}


void
BinaryClient::storing_file_done()
{
	L_DEBUG(this, "BinaryClient::storing_file_done");

	cookie_t haystack_cookie = random_int(0, 0xffff);

	// The following should be from a pool of Haystack storage (single writable)
	std::shared_ptr<HaystackVolume> volume = std::make_shared<HaystackVolume>(storing_endpoint.path, true);

	HaystackFile haystack_file(volume, storing_id, haystack_cookie);

	char buf[8 * 1024];
	ssize_t size;
	while ((size = ::read(file_descriptor, buf, sizeof(buf))) > 0) {
		L_DEBUG(this, "Store %zd bytes from buf in the Haystack storage!", size);
		if (haystack_file.write(buf, size) != size) {
			haystack_file.rewind();
			throw MSG_Error("Storage failure (1)!");
		}
	}

	Endpoints endpoints;
	endpoints.insert(storing_endpoint);
	if (!manager()->database_pool.checkout(storing_database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		L_ERR(this, "Cannot checkout %s", endpoints.as_string().c_str());
		haystack_file.rewind();
		throw MSG_Error("Storage failure (2)!");
	}

	offset_t haystack_offset = haystack_file.commit();

	Xapian::Document doc;
	doc.add_value(1, encode_length(haystack_offset));
	storing_database->replace(storing_id, doc, true);

	manager()->database_pool.checkin(storing_database);
	storing_database.reset();

	std::string msg;
	msg.append(encode_length(haystack_offset));
	msg.append(encode_length(haystack_cookie));
	send_message(StoringType::REPLY_DONE, msg);
}


void
BinaryClient::on_read_file(const char *buf, size_t received)
{
	L_CONN_WIRE(this, "BinaryClient::on_read_file: %zu bytes", received);
	::io_write(file_descriptor, buf, received);
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
		ssize_t len = decode_length(&p, p_end, true);
		if (len == -1) {
			return;
		}
		std::string data = std::string(p, len);
		buffer.erase(0, p - o + len);

		L_BINARY(this, "on_read message: '\\%02x' (state=0x%x)", type, state);
		switch (type) {
			case SWITCH_TO_REPL:
				state = State::REPLICATIONPROTOCOL_MASTER;  // Switch to replication protocol
				type = toUType(ReplicateType::MSG_GET_CHANGESETS);
				L_BINARY(this, "Switched to replication protocol");
				break;
			case SWITCH_TO_STORING:
				state = State::STORINGPROTOCOL_RECEIVER;  // Switch to file storing
				type = toUType(StoringType::MSG_CREATE);
				L_BINARY(this, "Switched to file storing");
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
BinaryClient::get_message(double, std::string &result)
{
	std::unique_ptr<Buffer> msg;
	if (!messages_queue.pop(msg)) {
		throw Xapian::NetworkError("No message available");
	}

	const char *msg_str = msg->dpos();
	size_t msg_size = msg->nbytes();
	char type_as_char = msg->type;

	result.assign(msg_str, msg_size);

	std::string buf;
	buf += type_as_char;
	L_BINARY(this, "get_message: '%s'", repr(buf, false).c_str());

	buf += encode_length(msg_size);
	buf += result;
	L_BINARY_PROTO(this, "msg = '%s'", repr(buf).c_str());

	return type_as_char;
}


void
BinaryClient::send_message(char type_as_char, const std::string &message, double)
{
	std::string buf;
	buf += type_as_char;
	L_BINARY(this, "send_message: '%s'", repr(buf, false).c_str());

	buf += encode_length(message.size());
	buf += message;
	L_BINARY_PROTO(this, "msg = '%s'", repr(buf).c_str());

	write(buf);
}


void
BinaryClient::shutdown()
{
	L_CALL(this, "BinaryClient::shutdown()");

	BaseClient::shutdown();
}


Xapian::Database*
BinaryClient::_get_db(bool writable_)
{
	std::unique_lock<std::mutex> lk(qmtx);
	if (endpoints.empty()) {
		return nullptr;
	}
	Endpoints endpoints_ = endpoints;
	lk.unlock();

	std::shared_ptr<Database> database;
	if (!manager()->database_pool.checkout(database, endpoints_, (writable_ ? DB_WRITABLE : 0) | DB_SPAWN)) {
		return nullptr;
	}

	lk.lock();
	databases[database->db.get()] = database;

	return database->db.get();
}

Xapian::Database*
BinaryClient::get_db()
{
	return _get_db(false);
}

Xapian::WritableDatabase*
BinaryClient::get_wdb()
{
	return static_cast<Xapian::WritableDatabase*>(_get_db(true));
}

void
BinaryClient::release_db(Xapian::Database *db_)
{
	if (db_) {
		std::unique_lock<std::mutex> lk(qmtx);
		std::shared_ptr<Database> database = databases[db_];
		databases.erase(db_);
		lk.unlock();

		manager()->database_pool.checkin(database);
	}
}


void
BinaryClient::select_db(const std::vector<std::string> &dbpaths_, bool, int)
{
	std::lock_guard<std::mutex> lk(qmtx);
	endpoints.clear();
	for (auto i = dbpaths_.begin(); i != dbpaths_.end(); ++i) {
		endpoints.insert(Endpoint(*i));
	}
	set_dbpaths(dbpaths_);
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

	std::string message;
	ReplicateType repl_type;
	StoringType storing_type;

	if (state == State::INIT_REMOTEPROTOCOL) {
		state = State::REMOTEPROTOCOL;
		send_greeting();
	}

	while (!messages_queue.empty()) {
		try {
			switch (state) {
				case State::INIT_REMOTEPROTOCOL:
					L_ERR(this, "Unexpected INIT_REMOTEPROTOCOL!");
				case State::REMOTEPROTOCOL:
					run_one();
					break;

				case State::REPLICATIONPROTOCOL_SLAVE:
					get_message(idle_timeout, message, ReplicateType::MAX);
					receive_repl();
					break;

				case State::REPLICATIONPROTOCOL_MASTER:
					repl_type = get_message(idle_timeout, message, ReplicateType::MAX);
					repl_apply(repl_type, message);
					break;

				case State::STORINGPROTOCOL_SENDER:
				case State::STORINGPROTOCOL_RECEIVER:
					storing_type = get_message(idle_timeout, message, StoringType::MAX);
					storing_apply(storing_type, message);
					break;
			}
		} catch (const Xapian::NetworkError &e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (repl_database) {
				manager()->database_pool.checkin(repl_database);
				repl_database.reset();
			}
			shutdown();
		} catch (...) {
			L_ERR(this, "ERROR!");
			if (repl_database) {
				manager()->database_pool.checkin(repl_database);
				repl_database.reset();
			}
			shutdown();
		}
	}

	--running;
	L_OBJ_END(this, "BinaryClient::run:END");
}


void
BinaryClient::repl_apply(ReplicateType type, const std::string &message)
{
	try {
		switch (type) {
			case ReplicateType::REPLY_END_OF_CHANGES: repl_end_of_changes();         return;
			case ReplicateType::REPLY_FAIL:           repl_fail();                   return;
			case ReplicateType::REPLY_DB_HEADER:      repl_set_db_header(message);   return;
			case ReplicateType::REPLY_DB_FILENAME:    repl_set_db_filename(message); return;
			case ReplicateType::REPLY_DB_FILEDATA:    repl_set_db_filedata(message); return;
			case ReplicateType::REPLY_DB_FOOTER:      repl_set_db_footer();          return;
			case ReplicateType::REPLY_CHANGESET:      repl_changeset(message);       return;
			case ReplicateType::MSG_GET_CHANGESETS:   repl_get_changesets(message);  return;
			default:
				std::string errmsg("Unexpected message type ");
				errmsg += std::to_string(toUType(type));
				throw Xapian::InvalidArgumentError(errmsg);
		}
	} catch (ConnectionClosed &) {
		return;
	} catch (...) {
		throw;
	}
}


void
BinaryClient::repl_end_of_changes()
{
	L_REPLICATION(this, "BinaryClient::repl_end_of_changes");

	if (repl_switched_db) {
		manager()->database_pool.switch_db(*endpoints.cbegin());
	}

	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
	}

	if (repl_database_tmp) {
		manager()->database_pool.checkin(repl_database_tmp);
		repl_database_tmp.reset();
	}

	shutdown();
}


void
BinaryClient::repl_fail()
{
	L_REPLICATION(this, "BinaryClient::repl_fail");
	L_ERR(this, "Replication failure!");
	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
	}
	if (repl_database_tmp) {
		manager()->database_pool.checkin(repl_database_tmp);
		repl_database_tmp.reset();
	}

	shutdown();
}


void
BinaryClient::repl_set_db_header(const std::string &message)
{
	L_REPLICATION(this, "BinaryClient::repl_set_db_header");
	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t length = decode_length(&p, p_end, true);
	repl_db_uuid.assign(p, length);
	p += length;
	repl_db_revision = decode_length(&p, p_end);
	repl_db_filename.clear();

	Endpoint endpoint = *endpoints.begin();
	std::string path_tmp = endpoint.path + "/.tmp";

	int dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (dir == 0) {
		L_DEBUG(this, "Directory %s created", path_tmp.c_str());
	} else if (errno == EEXIST) {
		delete_files(path_tmp.c_str());
		dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (dir == 0) {
			L_DEBUG(this, "Directory %s created", path_tmp.c_str());
		}
	} else {
		L_ERR(this, "Directory %s not created (%s)", path_tmp.c_str(), strerror(errno));
	}
}


void
BinaryClient::repl_set_db_filename(const std::string &message)
{
	L_REPLICATION(this, "BinaryClient::repl_set_db_filename");
	const char *p = message.data();
	const char *p_end = p + message.size();
	repl_db_filename.assign(p, p_end - p);
}


void
BinaryClient::repl_set_db_filedata(const std::string &message)
{
	L_REPLICATION(this, "BinaryClient::repl_set_db_filedata");

	const char *p = message.data();
	const char *p_end = p + message.size();

	const Endpoint &endpoint = *endpoints.begin();

	std::string path = endpoint.path + "/.tmp/";
	std::string path_filename = path + repl_db_filename;

	int fd = ::open(path_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (fd >= 0) {
		L_REPLICATION(this, "path_filename %s", path_filename.c_str());
		if (::write(fd, p, p_end - p) != p_end - p) {
			L_ERR(this, "Cannot write to %s", repl_db_filename.c_str());
			return;
		}
		::close(fd);
	}
}


void
BinaryClient::repl_set_db_footer()
{
	L_REPLICATION(this, "BinaryClient::repl_set_db_footer");
	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// size_t revision = decode_length(&p, p_end);
	// Indicates the end of a DB copy operation, signal switch

	Endpoints endpoints_tmp;
	Endpoint endpoint_tmp = *endpoints.begin();
	endpoint_tmp.path.append("/.tmp");
	endpoints_tmp.insert(endpoint_tmp);

	if (!repl_database_tmp) {
		if (!manager()->database_pool.checkout(repl_database_tmp, endpoints_tmp, DB_WRITABLE | DB_VOLATILE)) {
			L_ERR(this, "Cannot checkout tmp %s", endpoint_tmp.path.c_str());
		}
	}

	repl_switched_db = true;
	repl_just_switched_db = true;
}


void
BinaryClient::repl_changeset(const std::string &message)
{
	L_REPLICATION(this, "BinaryClient::repl_changeset");
	Xapian::WritableDatabase *wdb_;
	if (repl_database_tmp) {
		wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database_tmp->db.get());
	} else {
		wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database->db.get());
	}

	char path[] = "/tmp/xapian_changes.XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		L_ERR(this, "Cannot write to %s (1)", path);
		return;
	}

	std::string header;
	header += toUType(ReplicateType::REPLY_CHANGESET);
	header += encode_length(message.size());

	if (::write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
		L_ERR(this, "Cannot write to %s (2)", path);
		return;
	}

	if (::write(fd, message.data(), message.size()) != static_cast<ssize_t>(message.size())) {
		L_ERR(this, "Cannot write to %s (3)", path);
		return;
	}

	::lseek(fd, 0, SEEK_SET);

	try {
		// wdb_->apply_changeset_from_fd(fd, !repl_just_switched_db);  // FIXME: Implement Replication
		repl_just_switched_db = false;
	} catch(const Xapian::NetworkError &e) {
		L_ERR(this, "ERROR: %s", e.get_msg().c_str());
		::close(fd);
		::unlink(path);
		throw;
	} catch (const Xapian::DatabaseError &e) {
		L_ERR(this, "ERROR: %s", e.get_msg().c_str());
		::close(fd);
		::unlink(path);
		throw;
	} catch (...) {
		::close(fd);
		::unlink(path);
		throw;
	}

	::close(fd);
	::unlink(path);
}


void
BinaryClient::repl_get_changesets(const std::string &message)
{
	Xapian::Database *db_;
	const char *p = message.c_str();
	const char *p_end = p + message.size();

	size_t len = decode_length(&p, p_end, true);
	std::string uuid(p, len);
	p += len;

	len = decode_length(&p, p_end, true);
	std::string from_revision(p, len);
	p += len;

	len = decode_length(&p, p_end, true);
	std::string index_path(p, len);
	p += len;

	// Select endpoints and get database
	try {
		endpoints.clear();
		Endpoint endpoint(index_path);
		endpoints.insert(endpoint);
		db_ = get_db();
		if (!db_)
			throw Xapian::InvalidOperationError("Server has no open database");
	} catch (...) {
		throw;
	}

	char path[] = "/tmp/xapian_changesets_sent.XXXXXX";
	int fd = mkstemp(path);
	try {
		std::string to_revision = databases[db_]->checkout_revision;
		L_REPLICATION(this, "BinaryClient::repl_get_changesets for %s (%s) from rev:%s to rev:%s [%d]", endpoints.as_string().c_str(), uuid.c_str(), repr(from_revision, false).c_str(), repr(to_revision, false).c_str(), need_whole_db);

		if (fd < 0) {
			L_ERR(this, "Cannot write to %s (1)", path);
			return;
		}
		// db_->write_changesets_to_fd(fd, from_revision, uuid != db_->get_uuid());  // FIXME: Implement Replication
	} catch (...) {
		release_db(db_);
		::close(fd);
		::unlink(path);
		throw;
	}
	release_db(db_);

	send_file(fd);

	::close(fd);
	::unlink(path);
}


void
BinaryClient::receive_repl()
{
	L_REPLICATION(this, "BinaryClient::receive_repl (init)");

	strcpy(file_path, "/tmp/xapian_changesets_received.XXXXXX");
	file_descriptor = mkstemp(file_path);
	if (file_descriptor < 0) {
		L_ERR(this, "Cannot write to %s (1)", file_path);
		return;
	}

	read_file();

	Xapian::Database *db_ = repl_database->db.get();

	std::string msg;

	std::string uuid = db_->get_uuid();
	msg.append(encode_length(uuid.size()));
	msg.append(uuid);

	std::string revision = db_->get_revision_info();
	msg.append(encode_length(revision.size()));
	msg.append(revision);

	const Endpoint &endpoint = *endpoints.begin();
	msg.append(encode_length(endpoint.path.size()));
	msg.append(endpoint.path);

	send_message(SWITCH_TO_REPL, msg);
}


void
BinaryClient::storing_apply(StoringType type, const std::string & message)
{
	switch (type) {
		case StoringType::REPLY_READY:
			if (state == State::STORINGPROTOCOL_SENDER) {
				storing_send(message);
			}
			break;
		case StoringType::REPLY_DONE:
			storing_done(message);
			break;
		case StoringType::MSG_CREATE:
			storing_create(message);
			break;
		case StoringType::MSG_READ:
			storing_read(message);
		case StoringType::MSG_OPEN:
			storing_open(message);
			break;
		case StoringType::REPLY_FILE:
			break;
		case StoringType::REPLY_DATA:
			break;
		case StoringType::MAX:
			break;
	}
}


void
BinaryClient::storing_send(const std::string &)
{
	L_DEBUG(this, "BinaryClient::storing_send (init)");

	std::string msg;
	msg.append(encode_length(storing_id));
	msg.append(storing_endpoint.as_string());
	send_message(SWITCH_TO_STORING, msg);

	int fd = ::open(storing_filename.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		send_file(fd);
		::close(fd);
	}

	::unlink(storing_filename.c_str());
}


void
BinaryClient::storing_done(const std::string & message)
{
	L_DEBUG(this, "BinaryClient::storing_done");

	const char *p = message.data();
	const char *p_end = p + message.size();
	storing_offset = static_cast<offset_t>(decode_length(&p, p_end, false));
	storing_cookie = decode_length(&p, p_end, false);
}


void
BinaryClient::storing_open(const std::string & message)
{
	L_DEBUG(this, "BinaryClient::storing_open");

	const char *p = message.data();
	const char *p_end = p + message.size();
	storing_id = static_cast<Xapian::docid>(decode_length(&p, p_end, false));
	storing_offset = static_cast<offset_t>(decode_length(&p, p_end, false));
	storing_cookie = decode_length(&p, p_end, false);

	// The following should be from a pool of Haystack storage (single writable)
	storing_volume = std::make_shared<HaystackVolume>(storing_endpoint.path, false);

	storing_file = std::make_unique<HaystackFile>(storing_volume, storing_id, storing_cookie);
	storing_file->seek(storing_offset);

	std::string msg;
	msg.append(encode_length(storing_file->size()));
	send_message(StoringType::REPLY_FILE, msg);

	storing_read(message);
}


void
BinaryClient::storing_read(const std::string &)
{
	L_DEBUG(this, "BinaryClient::storing_read");

	char buffer[8192];
	size_t size = storing_file->read(buffer, sizeof(buffer));

	std::string msg;
	msg.append(encode_length(size));
	msg.append(buffer, size);

	send_message(StoringType::REPLY_DATA, msg);
}


void
BinaryClient::storing_create(const std::string & message)
{
	L_DEBUG(this, "BinaryClient::storing_create");

	const char *p = message.data();
	const char *p_end = p + message.size();
	storing_id = static_cast<Xapian::docid>(decode_length(&p, p_end, false));
	storing_endpoint = Endpoint(std::string(p, p_end - p));

	strcpy(file_path, "/tmp/xapian_storing.XXXXXX");
	file_descriptor = mkstemp(file_path);
	if (file_descriptor < 0) {
		L_ERR(this, "Cannot write to %s (1)", file_path);
		return;
	}

	read_file();
}

#endif  /* HAVE_REMOTE_PROTOCOL */
