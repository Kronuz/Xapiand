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

#include "server.h"
#include "utils.h"
#include "length.h"

#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


//
// Xapian binary client
//

BinaryClient::BinaryClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(server_, loop, sock_, database_pool_, thread_pool_, active_timeout_, idle_timeout_),
	  RemoteProtocol(std::vector<std::string>(), active_timeout_, idle_timeout_, true),
	  running(false),
	  repl_database(NULL),
	  repl_switched_db(false)
{
	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = XapiandServer::total_clients;
	int binary_clients = ++XapiandServer::binary_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	LOG_CONN(this, "Got connection (sock=%d), %d binary client(s) of a total of %d connected.\n", sock, binary_clients, XapiandServer::total_clients);

	LOG_OBJ(this, "CREATED BINARY CLIENT! (%d clients)\n", binary_clients);
	assert(binary_clients <= total_clients);
}


BinaryClient::~BinaryClient()
{
	databases_map_t::const_iterator it(databases.begin());
	for (; it != databases.end(); it++) {
		Database *database = it->second;
		database_pool->checkin(&database);
	}

	pthread_mutex_lock(&XapiandServer::static_mutex);
	int binary_clients = --XapiandServer::binary_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	LOG_OBJ(this, "DELETED BINARY CLIENT! (%d clients left)\n", binary_clients);
	assert(binary_clients >= 0);
}


bool BinaryClient::init_remote()
{
	state = init_remoteprotocol;
	thread_pool->addTask(this);

	return true;
}


bool BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	LOG(this, "src_endpoint: %s\n", src_endpoint.as_string().c_str());
	LOG(this, "dst_endpoint: %s\n", dst_endpoint.as_string().c_str());

	repl_endpoints.insert(src_endpoint);
	endpoints.insert(dst_endpoint);

	if (!database_pool->checkout(&repl_database, endpoints, DB_WRITABLE|DB_SPAWN)) {
		LOG_ERR(this, "Cannot checkout %s\n", endpoints.as_string().c_str());
		return false;
	}

	state = init_replicationprotocol;
	thread_pool->addTask(this);

	return true;
}


void BinaryClient::on_read(const char *buf, ssize_t received)
{
	buffer.append(buf, received);
	while (buffer.length() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		char type = *p++;
		size_t len = decode_length(&p, p_end, true);
		if (len == -1) {
			return;
		}
		std::string data = std::string(p, len);
		buffer.erase(0, p - o + len);

		if (type == '\xfe') {
			state = replicationprotocol;  // Switch to replication protocol
			type = static_cast<char>(REPL_MSG_GET_CHANGESETS);
			LOG_BINARY(this, "Switched to replication protocol");
		}

		Buffer *msg = new Buffer(type, data.c_str(), data.size());
		messages_queue.push(msg);
	}
	pthread_mutex_lock(&qmtx);
	if (!messages_queue.empty()) {
		if (!running) {
			running = true;
			thread_pool->addTask(this);
		}
	}
	pthread_mutex_unlock(&qmtx);
}


char BinaryClient::get_message(double timeout, std::string & result, char required_type)
{
	Buffer* msg;
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

	delete msg;

	return type_as_char;
}

void BinaryClient::send_message(char type_as_char, const std::string &message, double end_time) {
	std::string buf;
	buf += type_as_char;
	LOG_BINARY(this, "send_message: '%s'\n", repr(buf, false).c_str());

	buf += encode_length(message.size());
	buf += message;
	LOG_BINARY_PROTO(this, "msg = '%s'\n", repr(buf).c_str());

	write(buf);
}


void BinaryClient::shutdown()
{
	BaseClient::shutdown();
}


Xapian::Database * BinaryClient::get_db(bool writable_)
{
	pthread_mutex_lock(&qmtx);
	if (endpoints.empty()) {
		pthread_mutex_unlock(&qmtx);
		return NULL;
	}
	Endpoints endpoints_ = endpoints;
	pthread_mutex_unlock(&qmtx);

	Database *database = NULL;
	if (!database_pool->checkout(&database, endpoints_, (writable_ ? DB_WRITABLE : 0)|DB_SPAWN)) {
		return NULL;
	}

	pthread_mutex_lock(&qmtx);
	databases[database->db] = database;
	pthread_mutex_unlock(&qmtx);

	return database->db;
}


void BinaryClient::release_db(Xapian::Database *db_)
{
	if (db_) {
		pthread_mutex_lock(&qmtx);
		Database *database = databases[db_];
		databases.erase(db_);
		pthread_mutex_unlock(&qmtx);

		database_pool->checkin(&database);
	}
}


void BinaryClient::select_db(const std::vector<std::string> &dbpaths_, bool writable_, int flags)
{
	pthread_mutex_lock(&qmtx);
	endpoints.clear();
	std::vector<std::string>::const_iterator i(dbpaths_.begin());
	for (; i != dbpaths_.end(); i++) {
		endpoints.insert(Endpoint(*i));
	}
	dbpaths = dbpaths_;
	pthread_mutex_unlock(&qmtx);
}


void BinaryClient::run()
{
	while (true) {
		pthread_mutex_lock(&qmtx);
		if (state != init_remoteprotocol && messages_queue.empty()) {
			running = false;
			pthread_mutex_unlock(&qmtx);
			break;
		}
		pthread_mutex_unlock(&qmtx);

		try {
			switch (state) {
				case init_remoteprotocol:
					state = remoteprotocol;
					msg_update(std::string());
					break;
				case remoteprotocol:
					run_one();
					break;
				case init_replicationprotocol:
				case replicationprotocol:
					repl_run_one();
					break;

			}
		} catch (const Xapian::NetworkError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
			shutdown();
		} catch (...) {
			LOG_ERR(this, "ERROR!\n");
			shutdown();
		}
	}
}


typedef void (BinaryClient::* dispatch_repl_func)(const std::string &);

void BinaryClient::repl_run_one()
{
	try {
		static const dispatch_repl_func dispatch[] = {
			&BinaryClient::repl_end_of_changes,
			&BinaryClient::repl_fail,
			&BinaryClient::repl_set_db_header,
			&BinaryClient::repl_set_db_filename,
			&BinaryClient::repl_set_db_filedata,
			&BinaryClient::repl_set_db_footer,
			&BinaryClient::repl_changeset,
			&BinaryClient::repl_get_changesets,
		};
		std::string message;
		replicate_reply_type type = get_message(idle_timeout, message, REPL_MAX);
		if (static_cast<char>(type) >= sizeof(dispatch) / sizeof(dispatch[0]) || !dispatch[type]) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(type);
			throw Xapian::InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[type]))(message);
	} catch (ConnectionClosed &) {
		return;
	} catch (...) {
		throw;
	}

}

void BinaryClient::repl_end_of_changes(const std::string & message)
{
	if (state != init_replicationprotocol) {
		LOG(this, "BinaryClient::repl_end_of_changes\n");

		if (repl_database) {
			database_pool->checkin(&repl_database);
			repl_database = NULL;
		}

		if (repl_switched_db) {
			database_pool->switch_db(*endpoints.cbegin());
		}

		shutdown();
		return;
	}

	LOG(this, "BinaryClient::repl_end_of_changes (init)\n");
	state = replicationprotocol;

	Xapian::Database *db_ = repl_database->db;

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

	send_message('\xfe', msg);
}


void BinaryClient::repl_fail(const std::string & message)
{
	LOG(this, "BinaryClient::repl_fail\n");
	LOG_ERR(this, "Replication failure!\n");
	shutdown();
}


void BinaryClient::repl_set_db_header(const std::string & message)
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
	std::string path_tmp = endpoint.path+"/.tmp";

	int dir = ::mkdir(path_tmp.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (dir == 0) {
		LOG(this,"Directory %s created\n",path_tmp.c_str());
	} else if(dir == EEXIST) {
		delete_files(path_tmp.c_str());
		dir = ::mkdir(path_tmp.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (dir == 0) {
			LOG(this,"Directory %s created\n",path_tmp.c_str());
		}
	} else {
		LOG_ERR(this,"file %s not created\n",(endpoint.path+"/.tmp/").c_str());
	}
}


void BinaryClient::repl_set_db_filename(const std::string & message)
{
	LOG(this, "BinaryClient::repl_set_db_filename\n");
	const char *p = message.data();
	const char *p_end = p + message.size();
	repl_db_filename.assign(p, p_end - p);
}


void BinaryClient::repl_set_db_filedata(const std::string & message)
{
	LOG(this, "BinaryClient::repl_set_db_filedata\n");
	LOG(this, "Writing changeset file\n");

	const char *p = message.data();
	const char *p_end = p + message.size();

	const Endpoint &endpoint = *endpoints.begin();

	std::string path = endpoint.path + "/.tmp/";
	std::string path_filename = path + repl_db_filename;

	int fd = ::open(path_filename.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644);
	if (fd >= 0) {
		LOG(this, "path_filename %s\n",path_filename.c_str());
		if (::write(fd, p, p_end - p) != p_end - p) {
			LOG_ERR(this, "Cannot write to %s\n", repl_db_filename.c_str());
			return;
		}
		::close(fd);
	}
}


void BinaryClient::repl_set_db_footer(const std::string & message)
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

	if (repl_database != NULL) {
		database_pool->checkin(&repl_database);
		repl_database = NULL;
	}

	if (!database_pool->checkout(&repl_database, endpoints_tmp, DB_WRITABLE|DB_SPAWN|DB_VOLATILE)) {
		LOG_ERR(this, "Cannot checkout tmp %s\n", endpoint_tmp.path.c_str());
	}

	repl_switched_db = true;
}


void BinaryClient::repl_changeset(const std::string & message)
{
	LOG(this, "BinaryClient::repl_changeset\n");
	Xapian::WritableDatabase * wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database->db);

	char path[] = "/tmp/xapian_changes.XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		LOG_ERR(this, "Cannot write to %s (1)\n", path);
		return;
	}

	std::string header;
	header += REPL_REPLY_CHANGESET;
	header += encode_length(message.size());

	if (::write(fd, header.data(), header.size()) != header.size()) {
		LOG_ERR(this, "Cannot write to %s (2)\n", path);
		return;
	}

	if (::write(fd, message.data(), message.size()) != message.size()) {
		LOG_ERR(this, "Cannot write to %s (3)\n", path);
		return;
	}

	::lseek(fd, 0, SEEK_SET);

	try {
		wdb_->apply_changeset_from_fd(fd);
	} catch (...) {
		::close(fd);
		::unlink(path);
		throw;
	}

	::close(fd);
	::unlink(path);
}

void BinaryClient::repl_get_changesets(const std::string & message)
{
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
	pthread_mutex_lock(&qmtx);
	endpoints.clear();
	Endpoint endpoint(index_path);
	endpoints.insert(endpoint);
	Xapian::Database * db_ = get_db(false);
	pthread_mutex_unlock(&qmtx);

	if (!db_)
		throw Xapian::InvalidOperationError("Server has no open database");

	std::string to_revision = databases[db_]->checkout_revision;
	bool need_whole_db = (uuid != db_->get_uuid());
	LOG(this, "BinaryClient::repl_get_changesets for %s (%s) from rev:%s to rev:%s [%d]\n", endpoints.as_string().c_str(), uuid.c_str(), repr(from_revision, false).c_str(), repr(to_revision, false).c_str(), need_whole_db);

	// write changesets to a temporary file
	char path[] = "/tmp/xapian_changesets.XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		LOG_ERR(this, "Cannot write to %s (1)\n", path);
		return;
	}
	db_->write_changesets_to_fd(fd, from_revision, need_whole_db);

	// decode and send messages:
	::lseek(fd, 0, SEEK_SET);

	char buf[1024];
	std::string buffer;

	size_t size = ::read(fd, buf, sizeof(buf));
	buffer.append(buf, size);
	p = buffer.data();
	p_end = p + buffer.size();
	const char *s = p;

	while (p != p_end) {
		char type_as_char = *p++;
		size_t len = decode_length(&p, p_end);
		size_t pos = p - s;
		while (p_end - p < len || p_end - p < sizeof(buf) / 2) {
			size = ::read(fd, buf, sizeof(buf));
			if (!size) break;
			buffer.append(buf, size);
			s = p = buffer.data();
			p_end = p + buffer.size();
			p += pos;
		}
		if (p_end - p < len) {
			send_message(REPL_REPLY_FAIL, "");
			return;
		}
		std::string msg(p, len);
		p += len;

		send_message(type_as_char, msg);

		buffer.erase(0, p - s);
		s = p = buffer.data();
		p_end = p + buffer.size();
	}

	::close(fd);
	::unlink(path);

	release_db(db_);
}

#endif  /* HAVE_REMOTE_PROTOCOL */
