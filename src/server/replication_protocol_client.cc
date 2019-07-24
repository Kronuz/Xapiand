/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "replication_protocol_client.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                            // for assert
#include <errno.h>                            // for errno
#include <fcntl.h>
#include <limits.h>                           // for PATH_MAX
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "database/lock.h"                    // for lock_shard
#include "database/shard.h"                   // for Shard
#include "database/wal.h"                     // for DatabaseWAL
#include "error.hh"                           // for error:name, error::description
#include "exception_xapian.h"                 // for InvalidArgumentError
#include "fs.hh"                              // for delete_files, build_path_index
#include "io.hh"                              // for io::*
#include "length.h"                           // for serialise_length, unserialise_length
#include "manager.h"                          // for XapiandManager
#include "metrics.h"                          // for Metrics::metrics
#include "tcp.h"                              // for TCP::connect
#include "random.hh"                          // for random_int
#include "repr.hh"                            // for repr
#include "utype.hh"                           // for toUType
#include "xapian/net/serialise-error.h"       // for serialise_error, unserialise_error


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_REPLICATION
// #define L_REPLICATION L_ROSY_BROWN
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_REPLICA_WIRE
// #define L_REPLICA_WIRE L_ORANGE
// #undef L_REPLICA
// #define L_REPLICA L_TEAL
// #undef L_REPLICA_PROTO
// #define L_REPLICA_PROTO L_TEAL
// #undef L_TIMED_VAR
// #define L_TIMED_VAR _L_TIMED_VAR


/*  ____            _ _           _   _
 * |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
 * | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
 * |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
 * |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
 *           |_|
 */


ReplicationProtocolClient::ReplicationProtocolClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, double /*active_timeout_*/, double /*idle_timeout_*/, bool cluster_database_)
	: BaseClient<ReplicationProtocolClient>(std::move(parent_), ev_loop_, ev_flags_),
	  state(ReplicationState::INIT_REPLICATION_CLIENT),
#ifdef SAVE_LAST_MESSAGES
	  last_message_received('\xff'),
	  last_message_sent('\xff'),
#endif
	  file_descriptor(-1),
	  file_message_type('\xff'),
	  temp_file_template("xapiand.XXXXXX"),
	  cluster_database(cluster_database_),
	  changesets(0)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		++manager->replication_clients;
	}

	Metrics::metrics()
		.xapiand_replication_connections
		.Increment();

	L_CONN("New Replication Protocol Client, {} client(s) of a total of {} connected.", manager ? manager->replication_clients.load() : 0, manager ? manager->total_clients.load() : 0);
}


ReplicationProtocolClient::~ReplicationProtocolClient() noexcept
{
	try {
		reset();
		lk_shard_ptr.reset();

		if (auto manager = XapiandManager::manager()) {
			if (manager->replication_clients.fetch_sub(1) == 0) {
				L_CRIT("Inconsistency in number of replication clients");
				sig_exit(-EX_SOFTWARE);
			}
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

		if (is_shutting_down() && !is_idle()) {
			L_INFO("Replication Protocol client killed!");
		}

		if (cluster_database) {
			L_CRIT("Cannot synchronize cluster database!");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
ReplicationProtocolClient::reset()
{
	wal.reset();

	if (switch_shard) {
		switch_shard->close();
		auto manager = XapiandManager::manager();
		if (manager) {
			manager->database_pool->checkin(switch_shard);
		}
	}

	if (!switch_shard_path.empty()) {
		delete_files(switch_shard_path.c_str());
		switch_shard_path.clear();
	}

	if (log) {
		log->clear();
		log.reset();
	}

	changesets = 0;
}


bool
ReplicationProtocolClient::init_replication_protocol(const std::string& host, int port, const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept
{
	L_CALL("ReplicationProtocolClient::init_replication_protocol({}, {})", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	// Get fast write lock for replication or retry later
	try {
		lk_shard_ptr = std::make_unique<lock_shard>(dst_endpoint, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT | DB_REPLICA, false);
		lk_shard_ptr->lock(0, [=] {
			// If it cannot checkout because database is busy, retry when ready...
			trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), dst_endpoint.path, src_endpoint, dst_endpoint);
		});

		temp_directory_template = dst_endpoint.path + "/.tmp.XXXXXX";
	} catch (const Xapian::DatabaseNotAvailableError&) {
		L_REPLICATION("Replication deferred (not available): {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
		return false;
	} catch (...) {
		L_EXC("ERROR: Replication initialization ended with an unhandled exception (1)");
		return false;
	}

	int client_sock = TCP::connect(host.c_str(), std::to_string(port).c_str());
	if (client_sock == -1) {
		lk_shard_ptr.reset();
		try {
			// If it cannot replicate because the other end is down, retry in a bit...
			trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), dst_endpoint.path, src_endpoint, dst_endpoint);
			L_REPLICATION("Replication deferred (cannot connect): {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
			return false;
		} catch (...) {
			L_EXC("ERROR: Replication initialization ended with an unhandled exception (2)");
			return false;
		}
	}
	L_CONN("Connected to {}! (in socket {})", repr(src_endpoint.to_string()), client_sock);

	if (!init(client_sock)) {
		io::close(client_sock);
		lk_shard_ptr.reset();
		return false;
	}

	L_REPLICATION("Replication initialized: {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
	return true;
}


void
ReplicationProtocolClient::send_message(ReplicationMessageType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::send_message({}, <message>)", enum_name(type));

	L_REPLICA_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));

	send_message(toUType(type), message);
}


void
ReplicationProtocolClient::send_message(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::send_message({}, <message>)", enum_name(type));

	L_REPLICA_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));

	send_message(toUType(type), message);
}


void
ReplicationProtocolClient::send_file(ReplicationReplyType type, int fd)
{
	L_CALL("ReplicationProtocolClient::send_file({}, <fd>)", enum_name(type));

	L_REPLICA_PROTO("<< send_file ({}): {}", enum_name(type), fd);

	send_file(toUType(type), fd);
}


void
ReplicationProtocolClient::replication_server(ReplicationMessageType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::replication_server({}, <message>)", enum_name(type));

	L_OBJ_BEGIN("ReplicationProtocolClient::replication_server:BEGIN {{type:{}}}", enum_name(type));
	L_OBJ_END("ReplicationProtocolClient::replication_server:END {{type:{}}}", enum_name(type));

	L_DEBUG("{} ({}) -> {}", enum_name(type), strings::from_bytes(message.size()), repr(endpoint.to_string()));

	try {
		switch (type) {
			case ReplicationMessageType::MSG_GET_CHANGESETS:
				msg_get_changesets(message);
				return;
			case ReplicationMessageType::MSG_SET_REVISION:
				msg_set_revision(message);
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
			send_message(ReplicationReplyType::REPLY_EXCEPTION, serialise_error(exc));
		} catch (...) { }
		reset();
		lk_shard_ptr.reset();
		destroy();
		detach();
	} catch (const Xapian::NetworkError&) {
		// All other network errors mean we are fatally confused and are unlikely
		// to be able to communicate further across this connection. So we don't
		// try to propagate the error to the client, but instead just log the
		// exception and close the connection.
		L_EXC("ERROR: Dispatching replication protocol message");
		reset();
		lk_shard_ptr.reset();
		destroy();
		detach();
	} catch (const Xapian::Error& exc) {
		// Propagate the exception to the client, then return to the main
		// message handling loop.
		send_message(ReplicationReplyType::REPLY_EXCEPTION, serialise_error(exc));
		reset();
		lk_shard_ptr.reset();
	} catch (...) {
		L_EXC("ERROR: Dispatching replication protocol message");
		send_message(ReplicationReplyType::REPLY_EXCEPTION, std::string());
		reset();
		lk_shard_ptr.reset();
		destroy();
		detach();
	}
}


void
ReplicationProtocolClient::msg_get_changesets(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::msg_get_changesets(<message>)");

	L_REPLICATION("GET_CHANGESETS");

	size_t _total_sent_bytes = total_sent_bytes;
	auto begins = std::chrono::steady_clock::now();

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_uuid = unserialise_string(&p, p_end);
	auto remote_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

	if (endpoint_path.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Database must have a valid path");
		reset();
		lk_shard_ptr.reset();
		destroy();
		detach();

		auto ends = std::chrono::steady_clock::now();
		_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
		L(LOG_NOTICE, rgba(190, 30, 10, 0.6), "MSG_GET_CHANGESETS {} {{db:{}, rev:{}}} -> FAILURE {} {}", repr(endpoint_path), remote_uuid, remote_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
		return;
	}

	lock_shard lk_shard(Endpoint{endpoint_path}, DB_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT, false);

	auto db = lk_shard.lock()->db();
	auto uuid = db->get_uuid();
	auto db_revision = db->get_revision();
	lk_shard.unlock();

	auto from_revision = remote_revision;
	if (from_revision && uuid != remote_uuid) {
		L_REPLICATION("GET_CHANGESETS: UUID mismatch for {} ({} vs. {})", repr(endpoint_path), uuid, remote_uuid);
		from_revision = 0;
	}

	wal = std::make_unique<DatabaseWAL>(endpoint_path);
	if (from_revision && db_revision != from_revision && wal->locate_revision(from_revision).first == DatabaseWAL::max_rev) {
		L_REPLICATION("GET_CHANGESETS: Cannot locate revision {} for {}", from_revision, repr(endpoint_path));
		from_revision = 0;
	}

	auto to_revision = from_revision;

	if (to_revision < db_revision) {
		if (to_revision == 0) {
			int whole_db_copies_left = 5;

			while (true) {
				// Send the current revision number in the header.
				send_message(ReplicationReplyType::REPLY_DB_HEADER,
					serialise_string(uuid) +
					serialise_length(db_revision));

				static std::array<const std::string, 7> filenames = {
					"termlist.glass",
					"synonym.glass",
					"spelling.glass",
					"docdata.glass",
					"position.glass",
					"postlist.glass",
					"iamglass"
				};

				for (const auto& filename : filenames) {
					auto path = strings::format("{}/{}", endpoint_path, filename);
					int fd = io::open(path.c_str());
					if (fd != -1) {
						send_message(ReplicationReplyType::REPLY_DB_FILENAME, filename);
						send_file(ReplicationReplyType::REPLY_DB_FILEDATA, fd);
					}
				}

				for (size_t volume = 0; true; ++volume) {
					auto filename = strings::format("docdata.{}", volume);
					auto path = strings::format("{}/{}", endpoint_path, filename);
					int fd = io::open(path.c_str());
					if (fd != -1) {
						send_message(ReplicationReplyType::REPLY_DB_FILENAME, filename);
						send_file(ReplicationReplyType::REPLY_DB_FILEDATA, fd);
						continue;
					}
					break;
				}

				db = lk_shard.lock()->db();
				auto final_revision = db->get_revision();
				lk_shard.unlock();

				send_message(ReplicationReplyType::REPLY_DB_FOOTER, serialise_length(final_revision));

				if (db_revision == final_revision) {
					to_revision = db_revision;
					break;
				}

				if (whole_db_copies_left == 0) {
					send_message(ReplicationReplyType::REPLY_FAIL, "Database changing too fast");
					reset();
					lk_shard_ptr.reset();
					destroy();
					detach();

					auto ends = std::chrono::steady_clock::now();
					_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
					L(LOG_NOTICE, rgba(190, 30, 10, 0.6), "MSG_GET_CHANGESETS {} {{db:{}, rev:{}}} -> FAILURE {} {}", repr(endpoint_path), remote_uuid, remote_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
					return;
				} else if (--whole_db_copies_left == 0) {
					db = lk_shard.lock()->db();
					uuid = db->get_uuid();
					db_revision = db->get_revision();
				} else {
					db = lk_shard.lock()->db();
					uuid = db->get_uuid();
					db_revision = db->get_revision();
					lk_shard.unlock();
				}
			}
			lk_shard.unlock();
		}

		int wal_iterations = 5;
		do {
			// Send WAL operations.
			std::vector<std::string> reply_changesets;
			for (auto wal_it = wal->find(to_revision); wal_it != wal->end(); ++wal_it) {
				auto& line = *wal_it;
				const char *lp = line.data();
				const char *lp_end = lp + line.size();
				auto revision = unserialise_length(&lp, lp_end);
				if (revision >= db_revision) {
					break;
				}
				auto type = static_cast<DatabaseWAL::Type>(unserialise_length(&lp, lp_end));
				if (type == DatabaseWAL::Type::COMMIT) {
					for (auto& reply_changeset : reply_changesets) {
						send_message(ReplicationReplyType::REPLY_CHANGESET, reply_changeset);
					}
					send_message(ReplicationReplyType::REPLY_CHANGESET, line);
					reply_changesets.clear();
					++to_revision;
				} else {
					reply_changesets.push_back(std::move(line));
				}
			}
			db = lk_shard.lock()->db();
			db_revision = db->get_revision();
			lk_shard.unlock();
		} while (to_revision < db_revision && --wal_iterations != 0);
	}

	send_message(ReplicationReplyType::REPLY_END_OF_CHANGES);

	auto ends = std::chrono::steady_clock::now();
	_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
	if (from_revision == to_revision) {
		L(LOG_DEBUG, rgba(116, 100, 77, 0.6), "MSG_GET_CHANGESETS {} {{db:{}, rev:{}}} -> SENT EMPTY {} {}", repr(endpoint_path), remote_uuid, remote_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
	} else {
		L(LOG_DEBUG, rgba(55, 100, 79, 0.6), "MSG_GET_CHANGESETS {} {{db:{}, rev:{}}} -> SENT [{}..{}] {} {}", repr(endpoint_path), remote_uuid, remote_revision, from_revision, to_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
	}
}


void
ReplicationProtocolClient::msg_set_revision(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::msg_set_revision(<message>)");

	L_REPLICATION("SET_REVISION");

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node_lower_name = std::string(unserialise_string(&p, p_end));
	auto remote_uuid = unserialise_string(&p, p_end);
	auto remote_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

	if (endpoint_path.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Database must have a valid path");
		reset();
		lk_shard_ptr.reset();
		destroy();
		detach();
		return;
	}

	lock_shard lk_shard(Endpoint{endpoint_path}, DB_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT, false);

	auto db = lk_shard.lock()->db();
	auto uuid = db->get_uuid();
	if (uuid == remote_uuid) {
		lk_shard->endpoint.set_revision(remote_node_lower_name, remote_revision);
	}
	lk_shard.unlock();

	send_message(ReplicationReplyType::REPLY_DONE);
}


void
ReplicationProtocolClient::replication_client(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::replication_client({}, <message>)", enum_name(type));

	L_OBJ_BEGIN("ReplicationProtocolClient::replication_client:BEGIN {{type:{}}}", enum_name(type));
	L_OBJ_END("ReplicationProtocolClient::replication_client:END {{type:{}}}", enum_name(type));

	L_DEBUG("{} ({}) -> {}", enum_name(type), strings::from_bytes(message.size()), repr(endpoint.to_string()));

	try {
		switch (type) {
			case ReplicationReplyType::REPLY_WELCOME:
				reply_welcome(message);
				return;
			case ReplicationReplyType::REPLY_EXCEPTION:
				reply_exception(message);
				return;
			case ReplicationReplyType::REPLY_END_OF_CHANGES:
				reply_end_of_changes(message);
				return;
			case ReplicationReplyType::REPLY_FAIL:
				reply_fail(message);
				return;
			case ReplicationReplyType::REPLY_DB_HEADER:
				reply_db_header(message);
				return;
			case ReplicationReplyType::REPLY_DB_FILENAME:
				reply_db_filename(message);
				return;
			case ReplicationReplyType::REPLY_DB_FILEDATA:
				reply_db_filedata(message);
				return;
			case ReplicationReplyType::REPLY_DB_FOOTER:
				reply_db_footer(message);
				return;
			case ReplicationReplyType::REPLY_CHANGESET:
				reply_changeset(message);
				return;
			case ReplicationReplyType::REPLY_DONE:
				reply_done(message);
				return;
			default: {
				std::string errmsg("Unexpected message type ");
				errmsg += std::to_string(toUType(type));
				THROW(InvalidArgumentError, errmsg);
			}
		}
	} catch (const BaseException& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (const Xapian::DatabaseNotFoundError& exc) {
	} catch (const Xapian::Error& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (const std::exception& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (...) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	}

	close();  // client closes on error, take no more messages!
	reset();
	lk_shard_ptr.reset();
	destroy();
	detach();
}


void
ReplicationProtocolClient::reply_welcome(const std::string&)
{
	std::string message;

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();
	auto db = shard->db();

	message.append(serialise_string(db->get_uuid()));
	message.append(serialise_length(db->get_revision()));
	message.append(serialise_string(shard->endpoint.path));

	send_message(ReplicationMessageType::MSG_GET_CHANGESETS, message);
}


void
ReplicationProtocolClient::reply_exception(const std::string& message)
{
	unserialise_error(message, "REPLICATION:", "");
}


void
ReplicationProtocolClient::reply_end_of_changes(const std::string&)
{
	L_CALL("ReplicationProtocolClient::reply_end_of_changes(<message>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	L_REPLICATION("END_OF_CHANGES");

	bool switching = !switch_shard_path.empty();

	if (switching) {
		// Close internal databases
		shard->do_close(false, false, Shard::Transaction::none);

		auto manager = XapiandManager::manager();
		if (switch_shard) {
			switch_shard->close();
			if (manager) {
				manager->database_pool->checkin(switch_shard);
			}
		}

		// get exclusive lock
		if (manager) {
			manager->database_pool->lock(shard);
		}

		// Now we are sure no readers are using the database before moving the files
		delete_files(shard->endpoint.path, {"*glass", "wal.*", "flintlock"});
		move_files(switch_shard_path, shard->endpoint.path);

		// release exclusive lock
		if (manager) {
			manager->database_pool->unlock(shard);
		}
	}

	auto db = shard->db();
	if (switching && changesets) {
		L(LOG_DEBUG, rgb(55, 100, 79), "REPLY_END_OF_CHANGES {} {{db:{}, rev:{}}}: From a full copy and a set of {} {}{}", repr(shard->endpoint.path), db->get_uuid(), db->get_revision(), changesets, changesets == 1 ? "changeset" : "changesets", switch_shard ? " (to switch database)" : "");
	} else if (changesets) {
		L(LOG_DEBUG, rgb(55, 100, 79), "REPLY_END_OF_CHANGES {} {{db:{}, rev:{}}}: From a set of {} {}{}", repr(shard->endpoint.path), db->get_uuid(), db->get_revision(), changesets, changesets == 1 ? "changeset" : "changesets", switch_shard ? " (to switch database)" : "");
	} else if (switching) {
		L(LOG_DEBUG, rgb(55, 100, 79), "REPLY_END_OF_CHANGES {} {{db:{}, rev:{}}}: From a full copy{}", repr(shard->endpoint.path), db->get_uuid(), db->get_revision(), switch_shard ? " (to switch database)" : "");
	} else {
		L(LOG_DEBUG, rgb(116, 100, 77), "REPLY_END_OF_CHANGES {} {{db:{}, rev:{}}}: No changes", repr(shard->endpoint.path), db->get_uuid(), db->get_revision(), switch_shard ? " (to switch database)" : "");
	}

	if (cluster_database) {
		cluster_database = false;
		XapiandManager::set_cluster_database_ready();
	}

	auto local_node = Node::get_local_node();
	assert(local_node && !local_node->lower_name().empty());

	std::string reply;
	reply.append(serialise_string(local_node->lower_name()));
	reply.append(serialise_string(db->get_uuid()));
	reply.append(serialise_length(db->get_revision()));
	reply.append(serialise_string(shard->endpoint.path));

	send_message(ReplicationMessageType::MSG_SET_REVISION, reply);
}


void
ReplicationProtocolClient::reply_fail(const std::string& msg)
{
	L_CALL("ReplicationProtocolClient::reply_fail(<message>)");

	assert(lk_shard_ptr);
	L_REPLICATION("FAIL: {}", repr((*lk_shard_ptr)->endpoint.path));

	L(LOG_DEBUG, rgb(190, 30, 10), "REPLY_FAIL {}: {}", repr((*lk_shard_ptr)->endpoint.path), msg);

	reset();
	lk_shard_ptr.reset();
	destroy();
	detach();
}


void
ReplicationProtocolClient::reply_db_header(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::reply_db_header(<message>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	const char *p = message.data();
	const char *p_end = p + message.size();

	current_uuid = unserialise_string(&p, p_end);
	current_revision = unserialise_length(&p, p_end);

	reset();

	char path[PATH_MAX + 1];
	strncpy(path, temp_directory_template.c_str(), PATH_MAX);
	build_path_index(temp_directory_template);
	if (io::mkdtemp(path) == nullptr) {
		L_ERR("Directory {} not created: {} ({}): {}", path, error::name(errno), errno, error::description(errno));
		detach();
		return;
	}
	switch_shard_path = path;

	L_REPLICATION("DB_HEADER: {} in {} ({} rev:{})", repr(shard->endpoint.path), repr(switch_shard_path), current_uuid, current_revision);
	L_TIMED_VAR(log, 1s,
		"Replication of whole database taking too long: {}",
		"Replication of whole database took too long: {}",
		repr(shard->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_filename(const std::string& filename)
{
	L_CALL("ReplicationProtocolClient::reply_db_filename(<filename>)");

	assert(lk_shard_ptr);

	assert(!switch_shard_path.empty());

	file_path = switch_shard_path + "/" + filename;

	L_REPLICATION("DB_FILENAME({}): {}", repr(filename), repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_filedata(const std::string& tmp_file)
{
	L_CALL("ReplicationProtocolClient::reply_db_filedata(<tmp_file>)");

	assert(lk_shard_ptr);

	assert(!switch_shard_path.empty());

	if (::rename(tmp_file.c_str(), file_path.c_str()) == -1) {
		L_ERR("Cannot rename temporary file {} to {}: {} ({}): {}", tmp_file, file_path, error::name(errno), errno, error::description(errno));
		detach();
		return;
	}

	L_REPLICATION("DB_FILEDATA({} -> {}): {}", repr(tmp_file), repr(file_path), repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_footer(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::reply_db_footer(<message>)");

	assert(lk_shard_ptr);

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t revision = unserialise_length(&p, p_end);

	assert(!switch_shard_path.empty());

	if (revision != current_revision) {
		delete_files(switch_shard_path.c_str());
		switch_shard_path.clear();
	}

	L_REPLICATION("DB_FOOTER{}: {}", revision != current_revision ? " (ignored files)" : "", repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_changeset(const std::string& line)
{
	L_CALL("ReplicationProtocolClient::reply_changeset(<line>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	bool switching = !switch_shard_path.empty();

	if (!wal) {
		if (switching) {
			if (!switch_shard) {
				switch_shard = XapiandManager::manager(true)->database_pool->checkout(Endpoint{switch_shard_path}, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT | DB_REPLICA | DB_SYNCHRONOUS_WAL);
			}
			switch_shard->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(switch_shard.get());
		} else {
			shard->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(shard.get());
		}
		L_TIMED_VAR(log, 1s,
			"Replication of {}changesets taking too long: {}",
			"Replication of {}changesets took too long: {}",
			switching ? "whole database with " : "",
			repr(shard->endpoint.path));
	}

	wal->execute_line(line, false, false);

	++changesets;
	L_REPLICATION("CHANGESET ({} changesets{}): {}", changesets, switch_shard ? " to a new database" : "", repr(shard->endpoint.path));
}


void
ReplicationProtocolClient::reply_done(const std::string&)
{
	L_CALL("ReplicationProtocolClient::reply_done(<message>)");

	reset();
	lk_shard_ptr.reset();
	destroy();
	detach();
}


size_t
ReplicationProtocolClient::pending_messages() const
{
	std::lock_guard<std::mutex> lk(runner_mutex);
	return messages.size();
}


bool
ReplicationProtocolClient::is_idle() const
{
	L_CALL("ReplicationProtocolClient::is_idle() {{is_waiting:{}, is_running:{}, write_queue_empty:{}, pending_messages:{}}}", is_waiting(), is_running(), write_queue.empty(), pending_messages());

	return !is_waiting() && !is_running() && write_queue.empty() && !pending_messages();
}


void
ReplicationProtocolClient::shutdown_impl(long long asap, long long now)
{
	L_CALL("ReplicationProtocolClient::shutdown_impl({}, {})", asap, now);

	Worker::shutdown_impl(asap, now);

	if (asap) {
		shutting_down = true;
		auto manager = XapiandManager::manager();
		if (now != 0 || !manager || manager->ready_to_end_replication() || is_idle()) {
			stop(false);
			destroy(false);
			detach();
		}
	} else {
		if (is_idle()) {
			stop(false);
			destroy(false);
			detach();
		}
	}
}


bool
ReplicationProtocolClient::init_replication(int sock_) noexcept
{
	L_CALL("ReplicationProtocolClient::init_replication({})", sock_);

	if (!init(sock_)) {
		return false;
	}

	std::lock_guard<std::mutex> lk(runner_mutex);

	assert(!running);

	// Setup state...
	state = ReplicationState::INIT_REPLICATION_SERVER;

	// And start a runner.
	running = true;
	auto manager = XapiandManager::manager();
	if (manager) {
		manager->replication_client_pool->enqueue(share_this<ReplicationProtocolClient>());
	}
	return true;
}


bool
ReplicationProtocolClient::init_replication(const std::string& host, int port, const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept
{
	L_CALL("ReplicationProtocolClient::init_replication({}, {})", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	std::lock_guard<std::mutex> lk(runner_mutex);

	assert(!running);

	// Setup state...
	state = ReplicationState::INIT_REPLICATION_CLIENT;

	if (init_replication_protocol(host, port, src_endpoint, dst_endpoint)) {
		// And start a runner.
		running = true;
		auto manager = XapiandManager::manager();
		if (manager) {
			manager->replication_client_pool->enqueue(share_this<ReplicationProtocolClient>());
		}
		return true;
	}
	return false;
}


ssize_t
ReplicationProtocolClient::on_read(const char *buf, ssize_t received)
{
	L_CALL("ReplicationProtocolClient::on_read(<buf>, {})", received);

	if (received <= 0) {
		std::string reason;

		if (received < 0) {
			reason = strings::format("{} ({}): {}", error::name(errno), errno, error::description(errno));
			if (errno != ENOTCONN && errno != ECONNRESET && errno != ESPIPE) {
				L_NOTICE("Replication Protocol {} connection closed unexpectedly: {}", enum_name(state.load(std::memory_order_relaxed)), reason);
				close();
				return received;
			}
		} else {
			reason = "EOF";
		}

		if (is_waiting()) {
			L_NOTICE("Replication Protocol {} closed unexpectedly: There was still a request in progress: {}", enum_name(state.load(std::memory_order_relaxed)), reason);
			close();
			return received;
		}

		if (!write_queue.empty()) {
			L_NOTICE("Replication Protocol {} closed unexpectedly: There is still pending data: {}", enum_name(state.load(std::memory_order_relaxed)), reason);
			close();
			return received;
		}

		if (pending_messages()) {
			L_NOTICE("Replication Protocol {} closed unexpectedly: There are still pending messages: {}", enum_name(state.load(std::memory_order_relaxed)), reason);
			close();
			return received;
		}

		// Replication Protocol normally closed connection.
		close();
		return received;
	}

	L_REPLICA_WIRE("ReplicationProtocolClient::on_read: {} bytes", received);
	ssize_t processed = -buffer.size();
	buffer.append(buf, received);
	while (buffer.size() >= 2) {
		const char *o = buffer.data();
		const char *p = o;
		const char *p_end = p + buffer.size();

		char type = *p++;
		L_REPLICA_WIRE("on_read message: {} {{state:{}}}", repr(std::string(1, type)), enum_name(state));
		switch (type) {
			case FILE_FOLLOWS: {
				char path[PATH_MAX + 1];
				if (temp_directory.empty()) {
					if (temp_directory_template.empty()) {
						temp_directory = "/tmp";
					} else {
						strncpy(path, temp_directory_template.c_str(), PATH_MAX);
						build_path_index(temp_directory_template);
						if (io::mkdtemp(path) == nullptr) {
							L_ERR("Directory {} not created: {} ({}): {}", temp_directory_template, error::name(errno), errno, error::description(errno));
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
					L_ERR("Cannot create temporary file: {} ({}): {}", error::name(errno), errno, error::description(errno));
					detach();
					return processed;
				} else {
					L_REPLICA("Start reading file: {} ({})", path, file_descriptor);
				}
				read_file();
				processed += p - o;
				buffer.clear();
				return processed;
			}
		}

		ssize_t len;
		try {
			len = unserialise_length_and_check(&p, p_end);
		} catch (const Xapian::SerialisationError&) {
			return received;
		}

		if (!closed) {
			std::lock_guard<std::mutex> lk(runner_mutex);
			if (!running) {
				// Enqueue message...
				messages.push_back(Buffer(type, p, len));
				// And start a runner.
				running = true;
				auto manager = XapiandManager::manager();
				if (manager) {
					manager->replication_client_pool->enqueue(share_this<ReplicationProtocolClient>());
				}
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
ReplicationProtocolClient::on_read_file(const char *buf, ssize_t received)
{
	L_CALL("ReplicationProtocolClient::on_read_file(<buf>, {})", received);

	L_REPLICA_WIRE("ReplicationProtocolClient::on_read_file: {} bytes", received);

	io::write(file_descriptor, buf, received);
}


void
ReplicationProtocolClient::on_read_file_done()
{
	L_CALL("ReplicationProtocolClient::on_read_file_done()");

	L_REPLICA_WIRE("ReplicationProtocolClient::on_read_file_done");

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
			auto manager = XapiandManager::manager();
			if (manager) {
				manager->replication_client_pool->enqueue(share_this<ReplicationProtocolClient>());
			}
		} else {
			// There should be a runner, just enqueue message.
			messages.push_back(Buffer(file_message_type, temp_file.data(), temp_file.size()));
		}
	}
}


char
ReplicationProtocolClient::get_message(std::string &result, char max_type)
{
	L_CALL("ReplicationProtocolClient::get_message(<result>, <max_type>)");

	auto& msg = messages.front();

	char type = msg.type;

#ifdef SAVE_LAST_MESSAGES
	last_message_received.store(type, std::memory_order_relaxed);
#endif

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
ReplicationProtocolClient::send_message(char type_as_char, const std::string &message)
{
	L_CALL("ReplicationProtocolClient::send_message(<type_as_char>, <message>)");

#ifdef SAVE_LAST_MESSAGES
	last_message_sent.store(type_as_char, std::memory_order_relaxed);
#endif

	std::string buf;
	buf += type_as_char;
	buf += serialise_length(message.size());
	buf += message;
	write(buf);
}


void
ReplicationProtocolClient::send_file(char type_as_char, int fd)
{
	L_CALL("ReplicationProtocolClient::send_file(<type_as_char>, <fd>)");

	std::string buf;
	buf += FILE_FOLLOWS;
	buf += type_as_char;
	write(buf);

	BaseClient<ReplicationProtocolClient>::send_file(fd);
}


void
ReplicationProtocolClient::operator()()
{
	L_CALL("ReplicationProtocolClient::operator()()");

	L_CONN("Start running in replication worker...");

	std::unique_lock<std::mutex> lk(runner_mutex);

	switch (state) {
		case ReplicationState::INIT_REPLICATION_SERVER:
			state = ReplicationState::REPLICATION_SERVER;
			lk.unlock();
			try {
				send_message(ReplicationReplyType::REPLY_WELCOME);
			} catch (...) {
				lk.lock();
				running = false;
				L_CONN("Running in worker ended with an exception.");
				lk.unlock();
				L_EXC("ERROR: Replication server ended with an unhandled exception");
				detach();
				throw;
			}
			lk.lock();
			break;
		case ReplicationState::INIT_REPLICATION_CLIENT:
			state = ReplicationState::REPLICATION_CLIENT;
			[[fallthrough]];
		default:
			break;
	}

	while (!messages.empty() && !closed) {
		switch (state) {
			case ReplicationState::REPLICATION_SERVER: {
				std::string message;
				ReplicationMessageType type = static_cast<ReplicationMessageType>(get_message(message, static_cast<char>(ReplicationMessageType::MSG_MAX)));
				lk.unlock();
				try {

					L_REPLICA_PROTO(">> get_message[REPLICATION_SERVER] ({}): {}", enum_name(type), repr(message));
					replication_server(type, message);

					auto sent = total_sent_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_replication_sent_bytes
						.Increment(sent);

					auto received = total_received_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_replication_received_bytes
						.Increment(received);

				} catch (...) {
					lk.lock();
					running = false;
					L_CONN("Running in worker ended with an exception.");
					lk.unlock();
					L_EXC("ERROR: Replication server ended with an unhandled exception");
					detach();
					throw;
				}
				lk.lock();
				break;
			}

			case ReplicationState::REPLICATION_CLIENT: {
				std::string message;
				ReplicationReplyType type = static_cast<ReplicationReplyType>(get_message(message, static_cast<char>(ReplicationReplyType::REPLY_MAX)));
				lk.unlock();
				try {

					L_REPLICA_PROTO(">> get_message[REPLICATION_CLIENT] ({}): {}", enum_name(type), repr(message));
					replication_client(type, message);

					auto sent = total_sent_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_replication_sent_bytes
						.Increment(sent);

					auto received = total_received_bytes.exchange(0);
					Metrics::metrics()
						.xapiand_replication_received_bytes
						.Increment(received);

				} catch (...) {
					lk.lock();
					running = false;
					L_CONN("Running in worker ended with an exception.");
					lk.unlock();
					L_EXC("ERROR: Replication client ended with an unhandled exception");
					detach();
					throw;
				}
				lk.lock();
				break;
			}

			default:
				running = false;
				L_CONN("Running in worker ended with unexpected state.");
				lk.unlock();
				L_ERR("ERROR: Unexpected ReplicationProtocolClient state");
				stop();
				reset();
				lk_shard_ptr.reset();
				destroy();
				detach();
				return;
		}
	}

	running = false;
	L_CONN("Running in replication worker ended. {{messages_empty:{}, closed:{}, is_shutting_down:{}}}", messages.empty(), closed.load(), is_shutting_down());
	lk.unlock();

	if (is_shutting_down() && is_idle()) {
		detach();
		return;
	}

	redetach();  // try re-detaching if already flagged as detaching
}


std::string
ReplicationProtocolClient::__repr__() const
{
#ifdef SAVE_LAST_MESSAGES
	auto state_repr = ([this]() -> std::string {
		auto received = last_message_received.load(std::memory_order_relaxed);
		auto sent = last_message_sent.load(std::memory_order_relaxed);
		auto st = state.load(std::memory_order_relaxed);
		switch (st) {
			case ReplicationState::INIT_REPLICATION_CLIENT:
			case ReplicationState::REPLICATION_CLIENT:
				return strings::format("{}) ({}<->{}",
					enum_name(st),
					enum_name(static_cast<ReplicationReplyType>(received)),
					enum_name(static_cast<ReplicationMessageType>(sent)));
			case ReplicationState::INIT_REPLICATION_SERVER:
			case ReplicationState::REPLICATION_SERVER:
				return strings::format("{}) ({}<->{}",
					enum_name(st),
					enum_name(static_cast<ReplicationMessageType>(received)),
					enum_name(static_cast<ReplicationReplyType>(sent)));
			default:
				return "";
		}
	})();
#else
	const auto& state_repr = enum_name(state.load(std::memory_order_relaxed));
#endif
	return strings::format(STEEL_BLUE + "<ReplicationProtocolClient ({}) {{cnt:{}, sock:{}}}{}{}{}{}{}{}{}{}>",
		state_repr,
		use_count(),
		sock,
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "",
		is_idle() ? " " + DARK_STEEL_BLUE + "(idle)" + STEEL_BLUE : "",
		is_waiting() ? " " + LIGHT_STEEL_BLUE + "(waiting)" + STEEL_BLUE : "",
		is_running() ? " " + DARK_ORANGE + "(running)" + STEEL_BLUE : "",
		is_shutting_down() ? " " + ORANGE + "(shutting down)" + STEEL_BLUE : "",
		is_closed() ? " " + ORANGE + "(closed)" + STEEL_BLUE : "");
}

#endif  /* XAPIAND_CLUSTERING */
