/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "io_utils.h"
#include "length.h"
#include "servers/server.h"
#include "servers/tcp_base.h"
#include "remote_protocol.h"
#include "replication.h"
#include "utils.h"

#include <fcntl.h>
#include <sysexits.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>


std::string serialise_error(const Xapian::Error &exc) {
	// The byte before the type name is the type code.
	std::string result(1, (exc.get_type())[-1]);
	result += serialise_length(exc.get_context().length());
	result += exc.get_context();
	result += serialise_length(exc.get_msg().length());
	result += exc.get_msg();
	// The "error string" goes last so we don't need to store its length.
	const char * err = exc.get_error_string();
	if (err) result += err;
	return result;
}


#define SWITCH_TO_REPL '\xfe'


//
// Xapian binary client
//

BinaryClient::BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double /*active_timeout_*/, double /*idle_timeout_*/)
	: BaseClient(std::move(server_), ev_loop_, ev_flags_, sock_),
	  running(0),
	  state(State::INIT),
	  writable(false),
	  flags(0)
{
	remote_protocol = std::make_unique<RemoteProtocol>(this);
	replication = std::make_unique<Replication>(this);

	int binary_clients = ++XapiandServer::binary_clients;
	if (binary_clients > XapiandServer::max_binary_clients) {
		XapiandServer::max_binary_clients = binary_clients;
	}
	int total_clients = XapiandServer::total_clients;
	if (binary_clients > total_clients) {
		L_CRIT(this, "Inconsistency in number of binary clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_CONN(this, "New Binary Client (sock=%d), %d client(s) of a total of %d connected.", sock, binary_clients, total_clients);

	L_OBJ(this, "CREATED BINARY CLIENT! (%d clients)", binary_clients);
}


BinaryClient::~BinaryClient()
{
	checkin_database();

	int binary_clients = --XapiandServer::binary_clients;
	int total_clients = XapiandServer::total_clients;
	if (binary_clients < 0 || binary_clients > total_clients) {
		L_CRIT(this, "Inconsistency in number of binary clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_OBJ(this, "DELETED BINARY CLIENT! (%d clients left)", binary_clients);
}


bool
BinaryClient::init_remote()
{
	L_DEBUG(this, "init_remote");
	state = State::INIT;

	XapiandManager::manager->thread_pool.enqueue(share_this<BinaryClient>());
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

	if (!XapiandManager::manager->database_pool.checkout(database, endpoints, DB_WRITABLE | DB_SPAWN | DB_REPLICATION, [
		src_endpoint,
		dst_endpoint
	] () {
		L_DEBUG(XapiandManager::manager.get(), "Triggering replication for %s after checkin!", dst_endpoint.as_string().c_str());
		XapiandManager::manager->trigger_replication(src_endpoint, dst_endpoint);
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

	XapiandManager::manager->thread_pool.enqueue(share_this<BinaryClient>());
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
				replication->replication_client_file_done();
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
		L_ERR(this, "ERROR: %s", *exc.what() ? exc.what() : "Unkown exception!");
		checkin_database();
		shutdown();
	} catch (...) {
		L_ERR(this, "ERROR: Unkown error!");
		checkin_database();
		shutdown();
	}

	io::unlink(file_path);
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
			XapiandManager::manager->thread_pool.enqueue(share_this<BinaryClient>());
		}
	}
}


char
BinaryClient::get_message(std::string &result, char max_type)
{
	std::unique_ptr<Buffer> msg;
	if (!messages_queue.pop(msg)) {
		throw MSG_NetworkError("No message available");
	}

	char type = msg->type;

	if (type >= max_type) {
		std::string errmsg("Invalid message type ");
		errmsg += std::to_string(int(type));
		throw MSG_InvalidArgumentError(errmsg);
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
BinaryClient::checkout_database()
{
	if (!database) {
		int _flags = writable ? DB_WRITABLE : DB_OPEN;
		if ((flags & Xapian::DB_CREATE_OR_OPEN) == Xapian::DB_CREATE_OR_OPEN) {
			_flags |= DB_SPAWN;
		} else if ((flags & Xapian::DB_CREATE_OR_OVERWRITE) == Xapian::DB_CREATE_OR_OVERWRITE) {
			_flags |= DB_SPAWN;
		} else if ((flags & Xapian::DB_CREATE) == Xapian::DB_CREATE) {
			_flags |= DB_SPAWN;
		}
		if (!XapiandManager::manager->database_pool.checkout(database, endpoints, _flags)) {
			throw MSG_InvalidOperationError("Server has no open database");
		}
	}
}


void
BinaryClient::checkin_database()
{
	if (database) {
		XapiandManager::manager->database_pool.checkin(database);
		database.reset();
	}
	remote_protocol->matchspies.clear();
	remote_protocol->enquire.reset();
}


void
BinaryClient::run()
{
	try {
		_run();
	} catch (...) {
		cleanup();
		throw;
	}
	cleanup();
}


void
BinaryClient::_run()
{
	L_OBJ_BEGIN(this, "BinaryClient::run:BEGIN");
	if (running++) {
		--running;
		L_OBJ_END(this, "BinaryClient::run:END");
		return;
	}

	if (state == State::INIT) {
		state = State::REMOTEPROTOCOL_SERVER;
		remote_protocol->msg_update(std::string());
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
					remote_protocol->remote_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_SERVER: {
					std::string message;
					ReplicationMessageType type = static_cast<ReplicationMessageType>(get_message(message, static_cast<char>(ReplicationMessageType::MSG_MAX)));
					L_BINARY(this, ">> get_message(%s)", ReplicationMessageTypeNames[static_cast<int>(type)]);
					L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
					replication->replication_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_CLIENT: {
					std::string message;
					ReplicationReplyType type = static_cast<ReplicationReplyType>(get_message(message, static_cast<char>(ReplicationReplyType::REPLY_MAX)));
					L_BINARY(this, ">> get_message(%s)", ReplicationReplyTypeNames[static_cast<int>(type)]);
					L_BINARY_PROTO(this, "message: '%s'", repr(message).c_str());
					replication->replication_client(type, message);
					break;
				}
			}
		} catch (const Xapian::NetworkTimeoutError& exc) {
			try {
				// We've had a timeout, so the client may not be listening, so
				// set the end_time to 1 and if we can't send the message right
				// away, just exit and the client will cope.
				remote_protocol->send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc), 1.0);
			} catch (...) {}
			checkin_database();
			shutdown();
		} catch (const Xapian::NetworkError& exc) {
			auto exc_msg = exc.get_msg().c_str();
			L_EXC(this, "ERROR: %s", *exc_msg ? exc_msg : "Unkown Xapian::NetworkError!");
			checkin_database();
			shutdown();
		} catch (const Xapian::Error& exc) {
			// Propagate the exception to the client, then return to the main
			// message handling loop.
			remote_protocol->send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
			checkin_database();
		} catch (const Exception& exc) {
			L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
			remote_protocol->send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (const std::exception& exc) {
			L_EXC(this, "ERROR: %s", *exc.what() ? exc.what() : "Unkown std::exception!");
			remote_protocol->send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (...) {
			std::exception exc;
			L_EXC(this, "ERROR: %s", "Unkown exception!");
			remote_protocol->send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		}
	}

	--running;
	L_OBJ_END(this, "BinaryClient::run:END");
}

#endif  /* XAPIAND_CLUSTERING */
