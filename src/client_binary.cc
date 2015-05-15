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


//
// Xapian binary client
//

BinaryClient::BinaryClient(XapiandServer *server_, ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_)
	: BaseClient(server_, loop, sock_, database_pool_, thread_pool_, active_timeout_, idle_timeout_),
	  RemoteProtocol(std::vector<std::string>(), active_timeout_, idle_timeout_, true),
	  running(false),
	  state(initializing)
{
	pthread_mutex_lock(&XapiandServer::static_mutex);
	int total_clients = XapiandServer::total_clients;
	int binary_clients = ++XapiandServer::binary_clients;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	LOG_CONN(this, "Got connection (sock=%d), %d binary client(s) of a total of %d connected.\n", sock, binary_clients, XapiandServer::total_clients);

	thread_pool->addTask(this);

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


void BinaryClient::on_read(const char *buf, ssize_t received)
{
	buffer.append(buf, received);
	while (buffer.length() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		unsigned char c = *p++;
		message_type type = static_cast<message_type>(c);
		size_t len = decode_length(&p, p_end, true);
		if (len == -1) {
			return;
		}
		std::string data = std::string(p, len);
		buffer.erase(0, p - o + len);

		if (c == (unsigned char)'\xff') {
			state = replicationprotocol;  // Switch to replication protocol
			LOG_BINARY_PROTO(this, "Switched to replication protocol");
		} else {
			Buffer *msg = new Buffer(type, data.c_str(), data.size());
			messages_queue.push(msg);
		}
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


message_type BinaryClient::get_message(double timeout, std::string & result, message_type required_type)
{
	Buffer* msg;
	if (!messages_queue.pop(msg)) {
		throw Xapian::NetworkError("No message available");
	}

	const char *msg_str = msg->dpos();
	size_t msg_size = msg->nbytes();

	std::string message = std::string(msg_str, msg_size);

	std::string buf(&msg->type, 1);
	buf += encode_length(msg_size);
	buf += message;
	LOG_BINARY_PROTO(this, "get_message: '%s'\n", repr(buf).c_str());

	result = message;

	message_type type = static_cast<message_type>(msg->type);

	delete msg;

	return type;
}


void BinaryClient::send_message(reply_type type, const std::string &message) {
	char type_as_char = static_cast<char>(type);
	std::string buf(&type_as_char, 1);
	buf += encode_length(message.size());
	buf += message;

	LOG_BINARY_PROTO(this, "send_message: '%s'\n", repr(buf).c_str());

	write(buf);
}


void BinaryClient::send_message(reply_type type, const std::string &message, double end_time)
{
	send_message(type, message);
}


void BinaryClient::shutdown()
{
	destroy();
	// server->manager->async_shutdown.send();
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
		if (state != initializing && messages_queue.empty()) {
			running = false;
			pthread_mutex_unlock(&qmtx);
			break;
		}
		pthread_mutex_unlock(&qmtx);

		try {
			switch (state) {
				case initializing:
					state = remoteprotocol;
					msg_update(std::string());
					break;
				case remoteprotocol:
					run_one();
					break;
				case replicationprotocol:
					repl_run_one();
					break;

			}
		} catch (const Xapian::NetworkError &e) {
			LOG_ERR(this, "ERROR: %s\n", e.get_msg().c_str());
		} catch (...) {
			LOG_ERR(this, "ERROR!\n");
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
		};
		std::string message;
		size_t type = get_message(idle_timeout, message);
		if (type >= sizeof(dispatch) / sizeof(dispatch[0]) || !dispatch[type]) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(type);
			throw Xapian::InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[type]))(message);
	} catch (ConnectionClosed &) {
	    return;
	} catch (...) {
		// Propagate an unknown exception to the client.
		send_message(REPLY_EXCEPTION, std::string());
		// And rethrow it so our caller can log it and close the
		// connection.
		throw;
	}

}

void BinaryClient::repl_end_of_changes(const std::string & message)
{
	state = remoteprotocol;
	LOG_BINARY_PROTO(this, "Switched back to remote protocol");
}


void BinaryClient::repl_fail(const std::string & message)
{
	LOG_ERR(this, "Replication failure!\n");
	state = remoteprotocol;
	LOG_BINARY_PROTO(this, "Switched back to remote protocol");
}


void BinaryClient::repl_set_db_header(const std::string & message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t length = decode_length(&p, p_end, true);
	repl_db_uuid.assign(p, length);
	p += length;
	repl_db_revision = decode_length(&p, p_end);
	repl_db_filename.clear();
}


void BinaryClient::repl_set_db_filename(const std::string & message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t length = decode_length(&p, p_end, true);
	repl_db_filename.assign(p, length);
}


void BinaryClient::repl_set_db_filedata(const std::string & message)
{
	LOG(this, "Writing changeset file\n");
}


void BinaryClient::repl_set_db_footer(const std::string & message)
{
	// Indicates the end of a DB copy operation, signal switch
}


void BinaryClient::repl_changeset(const std::string & message)
{
	Xapian::WritableDatabase * wdb_ = static_cast<Xapian::WritableDatabase *>(get_db(true));
	if (!wdb_)
		throw Xapian::InvalidOperationError("Server is read-only");

	// wdb_->apply_changesets_from_fd()

	release_db(wdb_);
}


#endif  /* HAVE_REMOTE_PROTOCOL */
