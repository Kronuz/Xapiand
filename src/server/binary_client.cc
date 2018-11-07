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

#include "binary_client.h"

#ifdef XAPIAND_CLUSTERING

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "base_tcp.h"
#include "fs.hh"                              // for delete_files, build_path_index
#include "io.hh"                              // for io::*
#include "length.h"
#include "manager.h"                          // for XapiandManager::manager
#include "metrics.h"                          // for Metrics::metrics
#include "repr.hh"                            // for repr
#include "server.h"
#include "utype.hh"                           // for toUType


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


//
// Xapian binary client
//

BinaryClient::BinaryClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_, double /*active_timeout_*/, double /*idle_timeout_*/, bool cluster_database_)
	: BaseClient(std::move(parent_), ev_loop_, ev_flags_, sock_),
	  state(State::INIT),
	  file_descriptor(-1),
	  file_message_type('\xff'),
	  temp_file_template("xapiand.XXXXXX"),
	  cluster_database(cluster_database_),
	  remote_protocol(*this),
	  replication(*this)
{
	int binary_clients = ++XapiandServer::binary_clients;
	if (binary_clients > XapiandServer::max_binary_clients) {
		XapiandServer::max_binary_clients = binary_clients;
	}

	L_CONN("New Binary Client in socket %d, %d client(s) of a total of %d connected.", sock_, binary_clients, XapiandServer::total_clients);
}


BinaryClient::~BinaryClient()
{
	if (XapiandServer::binary_clients.fetch_sub(1) == 0) {
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

	if (shutting_down && !is_idle()) {
		L_WARNING("Binary client killed!");
	}

	if (cluster_database) {
		L_CRIT("Cannot synchronize cluster database!");
		sig_exit(-EX_CANTCREAT);
	}
}


bool
BinaryClient::is_idle()
{
	if (!waiting && !running && write_queue.empty()) {
		std::lock_guard<std::mutex> lk(runner_mutex);
		return messages.empty();
	}
	return false;
}


bool
BinaryClient::init_remote()
{
	L_CALL("BinaryClient::init_remote()");

	std::lock_guard<std::mutex> lk(runner_mutex);

	if (!running) {
		// Setup state...
		state = State::INIT;
		// And start a runner.
		running = true;
		XapiandManager::manager->client_pool.enqueue([task = share_this<BinaryClient>()]{
			task->run();
		});

		return true;
	}

	return false;
}


bool
BinaryClient::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_CALL("BinaryClient::init_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	std::lock_guard<std::mutex> lk(runner_mutex);

	state = State::REPLICATIONPROTOCOL_CLIENT;

	int port = (src_endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : src_endpoint.port;

	if ((sock = BaseTCP::connect(sock, src_endpoint.host, std::to_string(port))) == -1) {
		L_ERR("Cannot connect to %s", src_endpoint.host, std::to_string(port));
		return false;
	}
	L_CONN("Connected to %s! (in socket %d)", repr(src_endpoint.to_string()), sock.load());

	return replication.init_replication(src_endpoint, dst_endpoint);
}


ssize_t
BinaryClient::on_read(const char *buf, ssize_t received)
{
	L_CALL("BinaryClient::on_read(<buf>, %zu)", received);

	if (received <= 0) {
		return received;
	}

	L_BINARY_WIRE("BinaryClient::on_read: %zd bytes", received);
	ssize_t processed = -buffer.size();
	buffer.append(buf, received);
	while (buffer.size() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		char type = *p++;
		L_BINARY_WIRE("on_read message: %s {state:%s}", repr(std::string(1, type)), StateNames(state));
		switch (type) {
			case SWITCH_TO_REPL: {
				std::lock_guard<std::mutex> lk(runner_mutex);
				state = State::REPLICATIONPROTOCOL_SERVER;  // Switch to replication protocol
				type = toUType(ReplicationMessageType::MSG_GET_CHANGESETS);
				L_BINARY("Switched to replication protocol");
				break;
			}
			case FILE_FOLLOWS: {
				char path[PATH_MAX];
				if (temp_directory.empty()) {
					if (temp_directory_template.empty()) {
						temp_directory = "/tmp";
					} else {
						strncpy(path, temp_directory_template.c_str(), PATH_MAX);
						build_path_index(temp_directory_template);
						if (io::mkdtemp(path) == nullptr) {
							L_ERR("Directory %s not created: %s (%d): %s", temp_directory_template, io::strerrno(errno), errno, strerror(errno));
							destroy();
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
					L_ERR("Cannot create temporary file: %s (%d): %s", io::strerrno(errno), errno, strerror(errno));
					destroy();
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
				XapiandManager::manager->client_pool.enqueue([task = share_this<BinaryClient>()]{
					task->run();
				});
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
BinaryClient::on_read_file(const char *buf, ssize_t received)
{
	L_CALL("BinaryClient::on_read_file(<buf>, %zu)", received);

	L_BINARY_WIRE("BinaryClient::on_read_file: %zd bytes", received);

	io::write(file_descriptor, buf, received);
}


void
BinaryClient::on_read_file_done()
{
	L_CALL("BinaryClient::on_read_file_done()");

	L_BINARY_WIRE("BinaryClient::on_read_file_done");

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
			XapiandManager::manager->client_pool.enqueue([task = share_this<BinaryClient>()]{
				task->run();
			});
		} else {
			// There should be a runner, just enqueue message.
			messages.push_back(Buffer(file_message_type, temp_file.data(), temp_file.size()));
		}
	}
}


char
BinaryClient::get_message(std::string &result, char max_type)
{
	L_CALL("BinaryClient::get_message(<result>, <max_type>)");

	auto& msg = messages.front();

	char type = msg.type;

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
BinaryClient::send_message(char type_as_char, const std::string &message)
{
	L_CALL("BinaryClient::send_message(<type_as_char>, <message>)");

	std::string buf;
	buf += type_as_char;
	buf += serialise_length(message.size());
	buf += message;
	write(buf);
}


void
BinaryClient::send_file(char type_as_char, int fd)
{
	L_CALL("BinaryClient::send_file(<type_as_char>, <fd>)");

	std::string buf;
	buf += FILE_FOLLOWS;
	buf += type_as_char;
	write(buf);

	BaseClient::send_file(fd);
}


void
BinaryClient::run()
{
	L_CALL("BinaryClient::run()");

	L_CONN("Start running in binary worker...");

	std::unique_lock<std::mutex> lk(runner_mutex);

	if (state == State::INIT) {
		state = State::REMOTEPROTOCOL_SERVER;
		lk.unlock();
		try {
			remote_protocol.msg_update(std::string());
		} catch (...) {
			lk.lock();
			running = false;
			L_CONN("Running in worker ended with an exception.");
			detach();  // try re-detaching if already flagged as detaching
			throw;
		}
		lk.lock();
	}

	while (!messages.empty() && !closed) {
		switch (state) {
			case State::REMOTEPROTOCOL_SERVER: {
				std::string message;
				RemoteMessageType type = static_cast<RemoteMessageType>(get_message(message, static_cast<char>(RemoteMessageType::MSG_MAX)));
				lk.unlock();
				try {

					L_BINARY_PROTO(">> get_message[REMOTEPROTOCOL_SERVER] (%s): %s", RemoteMessageTypeNames(type), repr(message));
					remote_protocol.remote_server(type, message);

					Metrics::metrics()
						.xapiand_remote_protocol_sent_bytes
						.Increment(total_sent_bytes);
					total_sent_bytes = 0;

					Metrics::metrics()
						.xapiand_remote_protocol_received_bytes
						.Increment(total_received_bytes);
					total_received_bytes = 0;

				} catch (...) {
					lk.lock();
					running = false;
					L_CONN("Running in worker ended with an exception.");
					detach();  // try re-detaching if already flagged as detaching
					throw;
				}
				lk.lock();
				break;
			}

			case State::REPLICATIONPROTOCOL_SERVER: {
				std::string message;
				ReplicationMessageType type = static_cast<ReplicationMessageType>(get_message(message, static_cast<char>(ReplicationMessageType::MSG_MAX)));
				lk.unlock();
				try {

					L_BINARY_PROTO(">> get_message[REPLICATIONPROTOCOL_SERVER] (%s): %s", ReplicationMessageTypeNames(type), repr(message));
					replication.replication_server(type, message);

					Metrics::metrics()
						.xapiand_replication_sent_bytes
						.Increment(total_sent_bytes);
					total_sent_bytes = 0;

					Metrics::metrics()
						.xapiand_replication_received_bytes
						.Increment(total_received_bytes);
					total_received_bytes = 0;

				} catch (...) {
					lk.lock();
					running = false;
					L_CONN("Running in worker ended with an exception.");
					detach();  // try re-detaching if already flagged as detaching
					throw;
				}
				lk.lock();
				break;
			}

			case State::REPLICATIONPROTOCOL_CLIENT: {
				std::string message;
				ReplicationReplyType type = static_cast<ReplicationReplyType>(get_message(message, static_cast<char>(ReplicationReplyType::REPLY_MAX)));
				lk.unlock();
				try {

					L_BINARY_PROTO(">> get_message[REPLICATIONPROTOCOL_CLIENT] (%s): %s", ReplicationReplyTypeNames(type), repr(message));
					replication.replication_client(type, message);

					Metrics::metrics()
						.xapiand_replication_sent_bytes
						.Increment(total_sent_bytes);
					total_sent_bytes = 0;

					Metrics::metrics()
						.xapiand_replication_received_bytes
						.Increment(total_received_bytes);
					total_received_bytes = 0;

				} catch (...) {
					lk.lock();
					running = false;
					L_CONN("Running in worker ended with an exception.");
					detach();  // try re-detaching if already flagged as detaching
					throw;
				}
				lk.lock();
				break;
			}

			case State::INIT:
				L_ERR("Unexpected BinaryClient State::INIT!");
				break;

			default:
				L_ERR("Unexpected BinaryClient State!");
				break;
		}
	}

	running = false;

	if (shutting_down && is_idle()) {
		L_WARNING("Programmed shut down!");
		destroy();
		detach();
	}

	L_CONN("Running in binary worker ended.");
	redetach();  // try re-detaching if already flagged as detaching
}

#endif  /* XAPIAND_CLUSTERING */
