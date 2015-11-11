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
	  running(false),
	  repl_database(nullptr),
	  repl_switched_db(false),
	  storing_database(NULL),
	  storing_offset(0),
	  storing_cookie(0)
{
	int binary_clients = ++XapiandServer::binary_clients;
	int total_clients = XapiandServer::total_clients;
	assert(binary_clients <= total_clients);

	LOG_CONN(this, "New Binary Client (sock=%d), %d client(s) of a total of %d connected.\n", sock, binary_clients, total_clients);

	LOG_OBJ(this, "CREATED BINARY CLIENT! (%d clients)\n", binary_clients);
}


BinaryClient::~BinaryClient()
{
	for (auto& database : databases) {
		manager()->database_pool.checkin(database.second);
	}

	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
	}
	if (storing_database) {
		manager()->database_pool.checkin(storing_database);
	}

	int binary_clients = --XapiandServer::binary_clients;

	LOG_OBJ(this, "DELETED BINARY CLIENT! (%d clients left)\n", binary_clients);
	assert(binary_clients >= 0);
}


bool
BinaryClient::init_remote()
{
	state = State::INIT_REMOTEPROTOCOL;
	manager()->thread_pool.enqueue(share_this<BinaryClient>());
	return true;
}


bool
BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	LOG(this, "init_replication: %s  -->  %s\n", src_endpoint.as_string().c_str(), dst_endpoint.as_string().c_str());

	repl_endpoints.insert(src_endpoint);
	endpoints.insert(dst_endpoint);

	if (!manager()->database_pool.checkout(repl_database, endpoints, DB_WRITABLE | DB_SPAWN)) {
		LOG_ERR(this, "Cannot checkout %s\n", endpoints.as_string().c_str());
		return false;
	}

	state = State::REPLICATIONPROTOCOL_SLAVE;
	manager()->thread_pool.enqueue(share_this<BinaryClient>());

	if ((sock = BaseTCP::connect(sock, src_endpoint.host, std::to_string(src_endpoint.port))) < 0) {
		LOG_ERR(this, "Cannot connect to %s\n", src_endpoint.host.c_str(), std::to_string(src_endpoint.port).c_str());
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
		return false;
	}
	LOG_CONN(this, "Connected to %s (sock=%d)!\n", src_endpoint.as_string().c_str(), sock);

	return true;
}


bool BinaryClient::init_storing(const Endpoints &endpoints_, const Xapian::docid &did, const std::string &filename)
{
	LOG(this, "init_storing: %s  -->  %s\n", filename.c_str(), endpoints_.as_string().c_str());

	storing_id = did;
	storing_filename = filename;
	storing_endpoint = *endpoints_.begin();

	state = State::STORINGPROTOCOL_SENDER;
	manager()->thread_pool.enqueue(share_this<BinaryClient>());

	if ((sock = BaseTCP::connect(sock, storing_endpoint.host.c_str(), std::to_string(storing_endpoint.port).c_str())) < 0) {
		LOG_ERR(this, "Cannot connect to %s\n", storing_endpoint.host.c_str(), std::to_string(storing_endpoint.port).c_str());
		return false;
	}
	LOG_CONN(this, "Connected to %s (sock=%d)!\n", storing_endpoint.as_string().c_str(), sock);

	return true;
}


void
BinaryClient::on_read_file_done()
{
	LOG_CONN_WIRE(this, "BinaryClient::on_read_file_done\n");

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
				LOG_ERR(this, "ERROR: Invalid on_read_file_done for state: %d\n", state);
				if (repl_database) {
					manager()->database_pool.checkin(repl_database);
					repl_database.reset();
				}
				shutdown();
		};
	} catch (const Xapian::NetworkError &e) {
		LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		shutdown();
	} catch (const std::exception &err) {
		LOG_ERR(this, "ERROR: %s\n", err.what());
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		shutdown();
	} catch (...) {
		LOG_ERR(this, "ERROR: Unkown exception!\n");
		if (repl_database) {
			manager()->database_pool.checkin(repl_database);
			repl_database.reset();
		}
		shutdown();
	}

	::unlink(file_path);
}


void
BinaryClient::repl_file_done()
{
	LOG(this, "BinaryClient::repl_file_done\n");

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
	LOG(this, "BinaryClient::storing_file_done\n");

	cookie_t haystack_cookie = random_int(0, 0xffff);

	// The following should be from a pool of Haystack storage (single writable)
	std::shared_ptr<HaystackVolume> volume = std::make_shared<HaystackVolume>(storing_endpoint.path, true);

	HaystackFile haystack_file(volume, storing_id, haystack_cookie);

	char buf[8 * 1024];
	ssize_t size;
	while ((size = ::read(file_descriptor, buf, sizeof(buf))) > 0) {
		LOG(this, "Store %zd bytes from buf in the Haystack storage!\n", size);
		if (haystack_file.write(buf, size) != size) {
			haystack_file.rewind();
			throw MSG_Error("Storage failure (1)!");
		}
	}

	Endpoints endpoints;
	endpoints.insert(storing_endpoint);
	if (!manager()->database_pool.checkout(storing_database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		LOG_ERR(this, "Cannot checkout %s\n", endpoints.as_string().c_str());
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
	LOG_CONN_WIRE(this, "BinaryClient::on_read_file: %zu bytes\n", received);
	::io_write(file_descriptor, buf, received);
}


void
BinaryClient::on_read(const char *buf, size_t received)
{
	LOG_CONN_WIRE(this, "BinaryClient::on_read: %zu bytes\n", received);
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

		switch (type) {
			case SWITCH_TO_REPL:
				state = State::REPLICATIONPROTOCOL_MASTER;  // Switch to replication protocol
				type = toUType(ReplicateType::MSG_GET_CHANGESETS);
				LOG_BINARY(this, "Switched to replication protocol");
				break;
			case SWITCH_TO_STORING:
				state = State::STORINGPROTOCOL_RECEIVER;  // Switch to file storing
				type = toUType(StoringType::CREATE);
				LOG_BINARY(this, "Switched to file storing");
				break;
		}

		messages_queue.push(std::make_unique<Buffer>(type, data.c_str(), data.size()));
	}

	std::lock_guard<std::mutex> lk(qmtx);
	if (!messages_queue.empty()) {
		if (!running) {
			running = true;
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
	LOG_BINARY(this, "get_message: '%s'\n", repr(buf, false).c_str());

	buf += encode_length(msg_size);
	buf += result;
	LOG_BINARY_PROTO(this, "msg = '%s'\n", repr(buf).c_str());

	return type_as_char;
}


void
BinaryClient::send_message(char type_as_char, const std::string &message, double)
{
	std::string buf;
	buf += type_as_char;
	LOG_BINARY(this, "send_message: '%s'\n", repr(buf, false).c_str());

	buf += encode_length(message.size());
	buf += message;
	LOG_BINARY_PROTO(this, "msg = '%s'\n", repr(buf).c_str());

	write(buf);
}


void
BinaryClient::shutdown()
{
	LOG_OBJ(this, "BinaryClient::shutdown()\n");

	BaseClient::shutdown();
}


Xapian::Database*
BinaryClient::get_db(bool writable_)
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
	std::vector<std::string>::const_iterator i(dbpaths_.begin());
	for ( ; i != dbpaths_.end(); i++) {
		endpoints.insert(Endpoint(*i));
	}
	dbpaths = dbpaths_;
}


void
BinaryClient::run()
{
	LOG_OBJ(this, "BinaryClient::run() BEGINS!\n");

	std::string message;
	ReplicateType repl_type;
	StoringType storing_type;

	while (true) {
		std::unique_lock<std::mutex> lk(qmtx);
		if (state != State::INIT_REMOTEPROTOCOL && messages_queue.empty()) {
			running = false;
			break;
		}
		lk.unlock();

		try {
			switch (state) {
				case State::INIT_REMOTEPROTOCOL:
					state = State::REMOTEPROTOCOL;
					msg_update(std::string());
					break;
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
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			if (repl_database) {
				manager()->database_pool.checkin(repl_database);
				repl_database.reset();
			}
			shutdown();
		} catch (...) {
			LOG_ERR(this, "ERROR!\n");
			if (repl_database) {
				manager()->database_pool.checkin(repl_database);
				repl_database.reset();
			}
			shutdown();
		}
	}

	LOG_OBJ(this, "BinaryClient::run() ENDS!\n");
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
	LOG(this, "BinaryClient::repl_end_of_changes\n");

	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
	}

	if (repl_switched_db) {
		manager()->database_pool.switch_db(*endpoints.cbegin());
	}

	shutdown();
}


void
BinaryClient::repl_fail()
{
	LOG(this, "BinaryClient::repl_fail\n");
	LOG_ERR(this, "Replication failure!\n");
	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
	}
	shutdown();
}


void
BinaryClient::repl_set_db_header(const std::string &message)
{
	LOG(this, "BinaryClient::repl_set_db_header\n");
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
		LOG(this, "Directory %s created\n", path_tmp.c_str());
	} else if (dir == EEXIST) {
		delete_files(path_tmp.c_str());
		dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (dir == 0) {
			LOG(this, "Directory %s created\n", path_tmp.c_str());
		}
	} else {
		LOG_ERR(this, "file %s not created\n", path_tmp.c_str());
	}
}


void
BinaryClient::repl_set_db_filename(const std::string &message)
{
	LOG(this, "BinaryClient::repl_set_db_filename\n");
	const char *p = message.data();
	const char *p_end = p + message.size();
	repl_db_filename.assign(p, p_end - p);
}


void
BinaryClient::repl_set_db_filedata(const std::string &message)
{
	LOG(this, "BinaryClient::repl_set_db_filedata\n");
	LOG(this, "Writing changeset file\n");

	const char *p = message.data();
	const char *p_end = p + message.size();

	const Endpoint &endpoint = *endpoints.begin();

	std::string path = endpoint.path + "/.tmp/";
	std::string path_filename = path + repl_db_filename;

	int fd = ::open(path_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (fd >= 0) {
		LOG(this, "path_filename %s\n", path_filename.c_str());
		if (::write(fd, p, p_end - p) != p_end - p) {
			LOG_ERR(this, "Cannot write to %s\n", repl_db_filename.c_str());
			return;
		}
		::close(fd);
	}
}


void
BinaryClient::repl_set_db_footer()
{
	LOG(this, "BinaryClient::repl_set_db_footer\n");
	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// size_t revision = decode_length(&p, p_end);
	// Indicates the end of a DB copy operation, signal switch

	Endpoints endpoints_tmp;
	Endpoint endpoint_tmp = *endpoints.begin();
	endpoint_tmp.path.append("/.tmp");
	endpoints_tmp.insert(endpoint_tmp);

	if (repl_database) {
		manager()->database_pool.checkin(repl_database);
		repl_database.reset();
	}

	if (!manager()->database_pool.checkout(repl_database, endpoints_tmp, DB_WRITABLE | DB_VOLATILE)) {
		LOG_ERR(this, "Cannot checkout tmp %s\n", endpoint_tmp.path.c_str());
	}

	repl_switched_db = true;
	repl_just_switched_db = true;
}


void
BinaryClient::repl_changeset(const std::string &message)
{
	LOG(this, "BinaryClient::repl_changeset\n");
	Xapian::WritableDatabase *wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database->db.get());

	char path[] = "/tmp/xapian_changes.XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		LOG_ERR(this, "Cannot write to %s (1)\n", path);
		return;
	}

	std::string header;
	header += toUType(ReplicateType::REPLY_CHANGESET);
	header += encode_length(message.size());

	if (::write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
		LOG_ERR(this, "Cannot write to %s (2)\n", path);
		return;
	}

	if (::write(fd, message.data(), message.size()) != static_cast<ssize_t>(message.size())) {
		LOG_ERR(this, "Cannot write to %s (3)\n", path);
		return;
	}

	::lseek(fd, 0, SEEK_SET);

	try {
		wdb_->apply_changeset_from_fd(fd, !repl_just_switched_db);
		repl_just_switched_db = false;
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
	std::unique_lock<std::mutex> lk(qmtx);
	try {
		endpoints.clear();
		Endpoint endpoint(index_path);
		endpoints.insert(endpoint);
		db_ = get_db(false);
		if (!db_)
			throw Xapian::InvalidOperationError("Server has no open database");
	} catch (...) {
		throw;
	}
	lk.unlock();

	char path[] = "/tmp/xapian_changesets_sent.XXXXXX";
	int fd = mkstemp(path);
	try {
		std::string to_revision = databases[db_]->checkout_revision;
		bool need_whole_db = (uuid != db_->get_uuid());
		LOG(this, "BinaryClient::repl_get_changesets for %s (%s) from rev:%s to rev:%s [%d]\n", endpoints.as_string().c_str(), uuid.c_str(), repr(from_revision, false).c_str(), repr(to_revision, false).c_str(), need_whole_db);

		if (fd < 0) {
			LOG_ERR(this, "Cannot write to %s (1)\n", path);
			return;
		}

		db_->write_changesets_to_fd(fd, from_revision, need_whole_db);
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
	LOG(this, "BinaryClient::receive_repl (init)\n");

	strcpy(file_path, "/tmp/xapian_changesets_received.XXXXXX");
	file_descriptor = mkstemp(file_path);
	if (file_descriptor < 0) {
		LOG_ERR(this, "Cannot write to %s (1)\n", file_path);
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


void BinaryClient::storing_apply(StoringType type, const std::string & message)
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
		case StoringType::CREATE:
			storing_create(message);
			break;
		case StoringType::READ:
			storing_read(message);
		case StoringType::OPEN:
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

void BinaryClient::storing_send(const std::string &)
{
	LOG(this, "BinaryClient::storing_send (init)\n");

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


void BinaryClient::storing_done(const std::string & message)
{
	LOG(this, "BinaryClient::storing_done\n");

	const char *p = message.data();
	const char *p_end = p + message.size();
	storing_offset = static_cast<offset_t>(decode_length(&p, p_end, false));
	storing_cookie = decode_length(&p, p_end, false);
}


void BinaryClient::storing_open(const std::string & message)
{
	LOG(this, "BinaryClient::storing_open\n");

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


void BinaryClient::storing_read(const std::string &)
{
	LOG(this, "BinaryClient::storing_read\n");

	char buffer[8192];
	size_t size = storing_file->read(buffer, sizeof(buffer));

	std::string msg;
	msg.append(encode_length(size));
	msg.append(buffer, size);

	send_message(StoringType::REPLY_DATA, msg);
}


void BinaryClient::storing_create(const std::string & message)
{
	LOG(this, "BinaryClient::storing_create\n");

	const char *p = message.data();
	const char *p_end = p + message.size();
	storing_id = static_cast<Xapian::docid>(decode_length(&p, p_end, false));
	storing_endpoint = Endpoint(std::string(p, p_end - p));

	strcpy(file_path, "/tmp/xapian_storing.XXXXXX");
	file_descriptor = mkstemp(file_path);
	if (file_descriptor < 0) {
		LOG_ERR(this, "Cannot write to %s (1)\n", file_path);
		return;
	}

	read_file();
}

#endif  /* HAVE_REMOTE_PROTOCOL */
