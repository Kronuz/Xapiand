/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "client_binary.h"

#ifdef XAPIAND_CLUSTERING

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "database.h"
#include "io_utils.h"
#include "length.h"
#include "manager.h"
#include "servers/server.h"
#include "servers/server_binary.h"
#include "servers/tcp_base.h"
#include "utils.h"


std::string serialise_error(const Xapian::Error &exc) {
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


//
// Xapian binary client
//

BinaryClient::BinaryClient(std::shared_ptr<BinaryServer> server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double /*active_timeout_*/, double /*idle_timeout_*/)
	: BaseClient(std::move(server_), ev_loop_, ev_flags_, sock_),
	  state(State::INIT),
	  writable(false),
	  flags(0),
	  remote_protocol(*this),
	  replication(*this)
{
	int binary_clients = ++XapiandServer::binary_clients;
	if (binary_clients > XapiandServer::max_binary_clients) {
		XapiandServer::max_binary_clients = binary_clients;
	}
	int total_clients = XapiandServer::total_clients;
	if (binary_clients > total_clients) {
		L_CRIT("Inconsistency in number of binary clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_CONN("New Binary Client in socket %d, %d client(s) of a total of %d connected.", sock_, binary_clients, total_clients);

	idle = true;

	L_OBJ("CREATED BINARY CLIENT! (%d clients)", binary_clients);
}


BinaryClient::~BinaryClient()
{
	checkin_database();

	int binary_clients = --XapiandServer::binary_clients;
	int total_clients = XapiandServer::total_clients;
	if (binary_clients < 0 || binary_clients > total_clients) {
		L_CRIT("Inconsistency in number of binary clients");
		sig_exit(-EX_SOFTWARE);
	}

	L_OBJ("DELETED BINARY CLIENT! (%d clients left)", binary_clients);
}


bool
BinaryClient::init_remote()
{
	L_CALL("BinaryClient::init_remote()");

	state = State::INIT;

	XapiandManager::manager->client_pool.enqueue([task = share_this<BinaryClient>()]{
		task->run();
	});

	return true;
}


bool
BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_CALL("BinaryClient::init_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	L_REPLICATION("init_replication: %s  -->  %s", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
	state = State::REPLICATIONPROTOCOL_CLIENT;

	repl_endpoints.add(src_endpoint);
	endpoints.add(dst_endpoint);

	writable = true;

	try {
		XapiandManager::manager->database_pool.checkout(database, endpoints, DB_WRITABLE | DB_SPAWN | DB_REPLICATION, [
			src_endpoint,
			dst_endpoint
		] () {
			L_DEBUG("Triggering replication for %s after checkin!", repr(dst_endpoint.to_string()));
			XapiandManager::manager->trigger_replication(src_endpoint, dst_endpoint);
		});
	} catch (const CheckoutError&) {
		L_ERR("Cannot checkout %s", repr(endpoints.to_string()));
		return false;
	}

	int port = (src_endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : src_endpoint.port;

	if ((sock = BaseTCP::connect(sock, src_endpoint.host, std::to_string(port))) == -1) {
		L_ERR("Cannot connect to %s", src_endpoint.host, std::to_string(port));
		checkin_database();
		return false;
	}
	L_CONN("Connected to %s! (in socket %d)", repr(src_endpoint.to_string()), sock);

	return true;
}


void
BinaryClient::on_read_file_done()
{
	L_CALL("BinaryClient::on_read_file_done()");

	L_BINARY_WIRE("BinaryClient::on_read_file_done");

	io::lseek(file_descriptor, 0, SEEK_SET);

	try {
		switch (state) {
			case State::REPLICATIONPROTOCOL_CLIENT:
				replication.replication_client_file_done();
				break;
			default:
				L_ERR("ERROR: Invalid on_read_file_done for state: %d", toUType(state));
				checkin_database();
				shutdown();
		};
	} catch (const Xapian::NetworkError& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		checkin_database();
		shutdown();
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", *exc.what() ? exc.what() : "Unkown exception!");
		checkin_database();
		shutdown();
	} catch (...) {
		std::exception exc;
		L_EXC("ERROR: Unkown error!");
		checkin_database();
		shutdown();
	}

	io::unlink(file_path);
}


void
BinaryClient::on_read_file(const char *buf, ssize_t received)
{
	L_CALL("BinaryClient::on_read_file(<buf>, %zu)", received);

	L_BINARY_WIRE("BinaryClient::on_read_file: %zd bytes", received);
	io::write(file_descriptor, buf, received);
}


void
BinaryClient::on_read(const char *buf, ssize_t received)
{
	L_CALL("BinaryClient::on_read(<buf>, %zu)", received);

	if (received <= 0) {
		return;
	}

	L_BINARY_WIRE("BinaryClient::on_read: %zd bytes", received);
	buffer.append(buf, received);
	while (buffer.size() >= 2) {
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

		L_BINARY("on_read message: '\\%02x' (state=0x%x)", static_cast<int>(type), toUType(state));
		switch (type) {
			case SWITCH_TO_REPL:
				state = State::REPLICATIONPROTOCOL_SERVER;  // Switch to replication protocol
				type = toUType(ReplicationMessageType::MSG_GET_CHANGESETS);
				L_BINARY("Switched to replication protocol");
				break;
		}

		if (messages_queue.empty()) {
			// Enqueue message...
			messages_queue.push(Buffer(type, p, len));
			// And start a runner.
			XapiandManager::manager->client_pool.enqueue([task = share_this<BinaryClient>()]{
				task->run();
			});
		} else {
			// There should be a runner, just enqueue message.
			messages_queue.push(Buffer(type, p, len));
		}
		buffer.erase(0, p - o + len);
	}
}


char
BinaryClient::get_message(std::string &result, char max_type)
{
	L_CALL("BinaryClient::get_message(<result>, <max_type>)");

	Buffer msg;
	if (!messages_queue.pop(msg)) {
		THROW(NetworkError, "No message available");
	}

	char type = msg.type;

	if (type >= max_type) {
		std::string errmsg("Invalid message type ");
		errmsg += std::to_string(int(type));
		THROW(InvalidArgumentError, errmsg);
	}

	const char *msg_str = msg.dpos();
	size_t msg_size = msg.nbytes();
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
	L_CALL("BinaryClient::send_message(<type_as_char>, <message>, <>)");

	std::string buf;
	buf += type_as_char;

	buf += serialise_length(message.size());
	buf += message;

	write(buf);
}


void
BinaryClient::checkout_database()
{
	L_CALL("BinaryClient::checkout_database()");

	if (!database) {
		int _flags = writable ? DB_WRITABLE : DB_OPEN;
		if ((flags & Xapian::DB_CREATE_OR_OPEN) == Xapian::DB_CREATE_OR_OPEN) {
			_flags |= DB_SPAWN;
		} else if ((flags & Xapian::DB_CREATE_OR_OVERWRITE) == Xapian::DB_CREATE_OR_OVERWRITE) {
			_flags |= DB_SPAWN;
		} else if ((flags & Xapian::DB_CREATE) == Xapian::DB_CREATE) {
			_flags |= DB_SPAWN;
		}
		try {
			XapiandManager::manager->database_pool.checkout(database, endpoints, _flags);
		} catch (const CheckoutError&) {
			THROW(InvalidOperationError, "Server has no open database");
		}
	}
}


void
BinaryClient::checkin_database()
{
	L_CALL("BinaryClient::checkin_database()");

	if (database) {
		XapiandManager::manager->database_pool.checkin(database);
		database.reset();
	}
	remote_protocol.matchspies.clear();
	remote_protocol.enquire.reset();
}


void
BinaryClient::run()
{
	L_CALL("BinaryClient::run()");

	std::lock_guard<std::mutex> lk(running_mutex);
	L_CONN("Start running in binary worker...");

	idle = false;
	try {
		_run();
	} catch (...) {
		idle = true;
		L_CONN("Running in binary worker ended with an exception.");
		detach();
		throw;
	}
	idle = true;
	L_CONN("Running in binary worker ended.");
	redetach();  // try re-detaching if already detaching
}


void
BinaryClient::_run()
{
	L_CALL("BinaryClient::_run()");

	L_OBJ_BEGIN("BinaryClient::run:BEGIN");

	if (state == State::INIT) {
		state = State::REMOTEPROTOCOL_SERVER;
		remote_protocol.msg_update(std::string());
	}

	while (!messages_queue.empty() && !closed) {
		try {
			switch (state) {
				case State::REMOTEPROTOCOL_SERVER: {
					std::string message;
					RemoteMessageType type = static_cast<RemoteMessageType>(get_message(message, static_cast<char>(RemoteMessageType::MSG_MAX)));
					L_BINARY(">> get_message(%s) -> REMOTEPROTOCOL_SERVER", RemoteMessageTypeNames(type));
					L_BINARY_PROTO("message: '%s'", repr(message));
					remote_protocol.remote_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_SERVER: {
					std::string message;
					ReplicationMessageType type = static_cast<ReplicationMessageType>(get_message(message, static_cast<char>(ReplicationMessageType::MSG_MAX)));
					L_BINARY(">> get_message(%s) -> REPLICATIONPROTOCOL_SERVER", ReplicationMessageTypeNames(type));
					L_BINARY_PROTO("message: '%s'", repr(message));
					replication.replication_server(type, message);
					break;
				}

				case State::REPLICATIONPROTOCOL_CLIENT: {
					std::string message;
					ReplicationReplyType type = static_cast<ReplicationReplyType>(get_message(message, static_cast<char>(ReplicationReplyType::REPLY_MAX)));
					L_BINARY(">> get_message(%s) -> REPLICATIONPROTOCOL_CLIENT", ReplicationReplyTypeNames(type));
					L_BINARY_PROTO("message: '%s'", repr(message));
					replication.replication_client(type, message);
					break;
				}

				case State::INIT:
					L_ERR("Unexpected BinaryClient State::INIT!");
					break;

				default:
					L_ERR("Unexpected BinaryClient State!");
					break;
			}
		} catch (const Xapian::NetworkTimeoutError& exc) {
			L_EXC("ERROR: %s", exc.get_description());
			try {
				// We've had a timeout, so the client may not be listening, so
				// set the end_time to 1 and if we can't send the message right
				// away, just exit and the client will cope.
				remote_protocol.send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc), 1.0);
			} catch (...) {}
			checkin_database();
			shutdown();
		} catch (const Xapian::NetworkError& exc) {
			L_EXC("ERROR: %s", exc.get_description());
			checkin_database();
			shutdown();
		} catch (const BaseException& exc) {
			L_EXC("ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
			remote_protocol.send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (const Xapian::Error& exc) {
			L_EXC("ERROR: %s", exc.get_description());
			// Propagate the exception to the client, then return to the main
			// message handling loop.
			remote_protocol.send_message(RemoteReplyType::REPLY_EXCEPTION, serialise_error(exc));
			checkin_database();
		} catch (const std::exception& exc) {
			L_EXC("ERROR: %s", *exc.what() ? exc.what() : "Unkown std::exception!");
			remote_protocol.send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		} catch (...) {
			std::exception exc;
			L_EXC("ERROR: %s", "Unkown exception!");
			remote_protocol.send_message(RemoteReplyType::REPLY_EXCEPTION, std::string());
			checkin_database();
			shutdown();
		}
	}

	L_OBJ_END("BinaryClient::run:END");
}

#endif  /* XAPIAND_CLUSTERING */
