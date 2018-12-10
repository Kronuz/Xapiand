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

#include "replication_protocol.h"

#ifdef XAPIAND_CLUSTERING

#include <errno.h>                    // for errno

#include "cassert.h"                  // for ASSERT
#include "database.h"                 // for Database
#include "database_wal.h"             // for DatabaseWAL
#include "error.hh"                   // for error:name, error::description
#include "fs.hh"                      // for move_files, delete_files, build_path_index
#include "io.hh"                      // for io::*
#include "length.h"                   // for serialise_string, unserialise_string
#include "manager.h"                  // for XapiandManager::manager
#include "random.hh"                  // for random_int
#include "server/binary_client.h"     // for BinaryClient
#include "tcp.h"                      // for TCP::connect


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_REPLICATION
// #define L_REPLICATION L_MEDIUM_TURQUOISE
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_BINARY_WIRE
// #define L_BINARY_WIRE L_MOCCASIN
// #undef L_BINARY
// #define L_BINARY L_TEAL
// #undef L_BINARY_PROTO
// #define L_BINARY_PROTO L_TEAL
// #undef L_TIMED_VAR
// #define L_TIMED_VAR _L_TIMED_VAR


/*  ____            _ _           _   _
 * |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
 * | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
 * |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
 * |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
 *           |_|
 */


ReplicationProtocol::ReplicationProtocol(BinaryClient& client_)
	: LockableDatabase(),
	  client(client_),
	  lk_db(this),
	  changesets(0)
{
}


ReplicationProtocol::~ReplicationProtocol() noexcept
{
	try {
		reset();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
ReplicationProtocol::reset()
{
	wal.reset();

	if (switch_database) {
		switch_database->close();
		XapiandManager::manager->database_pool->checkin(switch_database);
	}

	if (!switch_database_path.empty()) {
		delete_files(switch_database_path.c_str());
		switch_database_path.clear();
	}

	if (log) {
		log->clear();
	}
	changesets = 0;
}


bool
ReplicationProtocol::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept
{
	L_CALL("ReplicationProtocol::init_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	try {
		src_endpoints = Endpoints{src_endpoint};

		flags = DB_WRITABLE | DB_CREATE_OR_OPEN;
		endpoints = Endpoints{dst_endpoint};
		lk_db.lock(0, [=] {
			// If it cannot checkout because database is busy, retry when ready...
			trigger_replication().delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, dst_endpoint.path, src_endpoint, dst_endpoint);
		});

		client.temp_directory_template = endpoints[0].path + "/.tmp.XXXXXX";

		auto& node = src_endpoint.node;
		int port = (node.binary_port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : node.binary_port;
		auto& host = node.host();
		if (TCP::connect(client.sock, host, std::to_string(port)) == -1) {
			L_ERR("Cannot connect to %s:%d", host, port);
			return false;
		}
		L_CONN("Connected to %s! (in socket %d)", repr(src_endpoints.to_string()), client.sock);

		L_REPLICATION("init_replication initialized: %s -->  %s", repr(src_endpoints.to_string()), repr(endpoints.to_string()));
	} catch (const TimeOutError&) {
		L_REPLICATION("init_replication deferred: %s -->  %s", repr(src_endpoints.to_string()), repr(endpoints.to_string()));
		return false;
	} catch (...) {
		L_EXC("ERROR: Replication initialization ended with an unhandled exception");
		return false;
	}
	return true;
}


void
ReplicationProtocol::send_message(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocol::send_message(%s, <message>)", ReplicationReplyTypeNames(type));

	L_BINARY_PROTO("<< send_message (%s): %s", ReplicationReplyTypeNames(type), repr(message));

	client.send_message(toUType(type), message);
}


void
ReplicationProtocol::send_file(ReplicationReplyType type, int fd)
{
	L_CALL("ReplicationProtocol::send_file(%s, <fd>)", ReplicationReplyTypeNames(type));

	L_BINARY_PROTO("<< send_file (%s): %d", ReplicationReplyTypeNames(type), fd);

	client.send_file(toUType(type), fd);
}


void
ReplicationProtocol::replication_server(ReplicationMessageType type, const std::string& message)
{
	L_CALL("ReplicationProtocol::replication_server(%s, <message>)", ReplicationMessageTypeNames(type));

	L_OBJ_BEGIN("ReplicationProtocol::replication_server:BEGIN {type:%s}", ReplicationMessageTypeNames(type));
	L_OBJ_END("ReplicationProtocol::replication_server:END {type:%s}", ReplicationMessageTypeNames(type));

	switch (type) {
		case ReplicationMessageType::MSG_GET_CHANGESETS:
			msg_get_changesets(message);
			return;
		default: {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
	}
}


void
ReplicationProtocol::msg_get_changesets(const std::string& message)
{
	L_CALL("ReplicationProtocol::msg_get_changesets(<message>)");

	L_REPLICATION("ReplicationProtocol::msg_get_changesets");

	const char *p = message.c_str();
	const char *p_end = p + message.size();

	auto remote_uuid = unserialise_string(&p, p_end);
	auto from_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

	flags = DB_WRITABLE;
	endpoints = Endpoints{Endpoint{endpoint_path}};
	if (endpoints.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Database must have a valid path");
	}

	lk_db.lock();
	auto uuid = db()->get_uuid();
	auto revision = db()->get_revision();
	lk_db.unlock();

	if (from_revision && uuid != remote_uuid) {
		from_revision = 0;
	}

	wal = std::make_unique<DatabaseWAL>(endpoints[0].path);
	if (from_revision && wal->locate_revision(from_revision).first == DatabaseWAL::max_rev) {
		from_revision = 0;
	}

	if (from_revision < revision) {
		if (from_revision == 0) {
			int whole_db_copies_left = 5;

			while (true) {
				// Send the current revision number in the header.
				send_message(ReplicationReplyType::REPLY_DB_HEADER,
					serialise_string(uuid) +
					serialise_length(revision));

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
					auto path = endpoints[0].path + "/" + filename;
					int fd = io::open(path.c_str());
					if (fd != -1) {
						send_message(ReplicationReplyType::REPLY_DB_FILENAME, filename);
						send_file(ReplicationReplyType::REPLY_DB_FILEDATA, fd);
					}
				}

				lk_db.lock();
				auto final_revision = db()->get_revision();
				lk_db.unlock();

				send_message(ReplicationReplyType::REPLY_DB_FOOTER, serialise_length(final_revision));

				if (revision == final_revision) {
					from_revision = revision;
					break;
				}

				if (whole_db_copies_left == 0) {
					send_message(ReplicationReplyType::REPLY_FAIL, "Database changing too fast");
					return;
				} else if (--whole_db_copies_left == 0) {
					lk_db.lock();
					uuid = db()->get_uuid();
					revision = db()->get_revision();
				} else {
					lk_db.lock();
					uuid = db()->get_uuid();
					revision = db()->get_revision();
					lk_db.unlock();
				}
			}
			lk_db.unlock();
		}

		int wal_iterations = 5;
		do {
			// Send WAL operations.
			auto wal_it = wal->find(from_revision);
			for (; wal_it != wal->end(); ++wal_it) {
				send_message(ReplicationReplyType::REPLY_CHANGESET, wal_it->second);
			}
			from_revision = wal_it->first + 1;
			lk_db.lock();
			revision = db()->get_revision();
			lk_db.unlock();
		} while (from_revision < revision && --wal_iterations != 0);
	}

	send_message(ReplicationReplyType::REPLY_END_OF_CHANGES, "");
}


void
ReplicationProtocol::replication_client(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocol::replication_client(%s, <message>)", ReplicationReplyTypeNames(type));

	L_OBJ_BEGIN("ReplicationProtocol::replication_client:BEGIN {type:%s}", ReplicationReplyTypeNames(type));
	L_OBJ_END("ReplicationProtocol::replication_client:END {type:%s}", ReplicationReplyTypeNames(type));

	switch (type) {
		case ReplicationReplyType::REPLY_WELCOME:
			reply_welcome(message);
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
		default: {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
	}
}


void
ReplicationProtocol::reply_welcome(const std::string&)
{
	std::string message;

	message.append(serialise_string(db()->get_uuid()));
	message.append(serialise_length(db()->get_revision()));
	message.append(serialise_string(endpoints[0].path));

	send_message(static_cast<ReplicationReplyType>(SWITCH_TO_REPL), message);
}


void
ReplicationProtocol::reply_end_of_changes(const std::string&)
{
	L_CALL("ReplicationProtocol::reply_end_of_changes(<message>)");

	bool switching = !switch_database_path.empty();

	if (switching) {
		// Close internal databases
		database()->do_close(false, false, database()->transaction);

		if (switch_database) {
			switch_database->close();
			XapiandManager::manager->database_pool->checkin(switch_database);
		}

		// get exclusive lock
		XapiandManager::manager->database_pool->lock(database());

		// Now we are sure no readers are using the database before moving the files
		delete_files(endpoints[0].path, {"*glass", "wal.*"});
		move_files(switch_database_path, endpoints[0].path);

		// release exclusive lock
		XapiandManager::manager->database_pool->unlock(database());
	}

	L_REPLICATION("ReplicationProtocol::reply_end_of_changes: %s (%s a set of %zu changesets)%s", repr(endpoints[0].path), switching ? "from a full copy and" : "from", changesets, switch_database ? " (to switch database)" : "");
	L_DEBUG("Replication of %s {%s} was completed at revision %llu (%s a set of %zu changesets)", repr(endpoints[0].path), database()->get_uuid(), database()->get_revision(), switching ? "from a full copy and" : "from", changesets);

	if (client.cluster_database) {
		client.cluster_database = false;
		XapiandManager::manager->cluster_database_ready();
	}

	client.destroy();
	client.detach();
}


void
ReplicationProtocol::reply_fail(const std::string&)
{
	L_CALL("ReplicationProtocol::reply_fail(<message>)");

	L_REPLICATION("ReplicationProtocol::reply_fail: %s", repr(endpoints[0].path));

	reset();

	L_ERR("ReplicationProtocol failure!");
	client.destroy();
	client.detach();
}


void
ReplicationProtocol::reply_db_header(const std::string& message)
{
	L_CALL("ReplicationProtocol::reply_db_header(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();

	current_uuid = unserialise_string(&p, p_end);
	current_revision = unserialise_length(&p, p_end);

	reset();

	char path[PATH_MAX];
	strncpy(path, client.temp_directory_template.c_str(), PATH_MAX);
	build_path_index(client.temp_directory_template);
	if (io::mkdtemp(path) == nullptr) {
		L_ERR("Directory %s not created: %s (%d): %s", path, error::name(errno), errno, error::description(errno));
		client.detach();
		return;
	}
	switch_database_path = path;

	L_REPLICATION("ReplicationProtocol::reply_db_header: %s in %s", repr(endpoints[0].path), repr(switch_database_path));
	L_TIMED_VAR(log, 1s,
		"Replication of whole database taking too long: %s",
		"Replication of whole database took too long: %s",
		repr(endpoints[0].path));
}


void
ReplicationProtocol::reply_db_filename(const std::string& filename)
{
	L_CALL("ReplicationProtocol::reply_db_filename(<filename>)");

	ASSERT(!switch_database_path.empty());

	file_path = switch_database_path + "/" + filename;

	L_REPLICATION("ReplicationProtocol::reply_db_filename(%s): %s", repr(filename), repr(endpoints[0].path));
}


void
ReplicationProtocol::reply_db_filedata(const std::string& tmp_file)
{
	L_CALL("ReplicationProtocol::reply_db_filedata(<tmp_file>)");

	ASSERT(!switch_database_path.empty());

	if (::rename(tmp_file.c_str(), file_path.c_str()) == -1) {
		L_ERR("Cannot rename temporary file %s to %s: %s (%d): %s", tmp_file, file_path, error::name(errno), errno, error::description(errno));
		client.detach();
		return;
	}

	L_REPLICATION("ReplicationProtocol::reply_db_filedata(%s -> %s): %s", repr(tmp_file), repr(file_path), repr(endpoints[0].path));
}


void
ReplicationProtocol::reply_db_footer(const std::string& message)
{
	L_CALL("ReplicationProtocol::reply_db_footer(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t revision = unserialise_length(&p, p_end);

	ASSERT(!switch_database_path.empty());

	if (revision != current_revision) {
		delete_files(switch_database_path.c_str());
		switch_database_path.clear();
	}

	L_REPLICATION("ReplicationProtocol::reply_db_footer%s: %s", revision != current_revision ? " (ignored files)" : "", repr(endpoints[0].path));
}


void
ReplicationProtocol::reply_changeset(const std::string& line)
{
	L_CALL("ReplicationProtocol::reply_changeset(<line>)");

	bool switching = !switch_database_path.empty();

	if (!wal) {
		if (switching) {
			if (!switch_database) {
				switch_database = XapiandManager::manager->database_pool->checkout(Endpoints{Endpoint{switch_database_path}}, DB_WRITABLE | DB_SYNC_WAL);
			}
			switch_database->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(switch_database.get());
		} else {
			database()->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(database().get());
		}
		L_TIMED_VAR(log, 1s,
			"Replication of %schangesets taking too long: %s",
			"Replication of %schangesets took too long: %s",
			switching ? "whole database with " : "",
			repr(endpoints[0].path));
	}

	wal->execute_line(line, true, false, false);

	++changesets;
	L_REPLICATION("ReplicationProtocol::reply_changeset (%zu changesets%s): %s", changesets, switch_database ? " to a new database" : "", repr(endpoints[0].path));
}


#endif  /* XAPIAND_CLUSTERING */
