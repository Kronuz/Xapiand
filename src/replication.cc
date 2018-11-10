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

#include "replication.h"

#ifdef XAPIAND_CLUSTERING

#include "cassert.hh"                 // for assert

#include "database.h"                 // for Database
#include "database_wal.h"             // for DatabaseWAL
#include "fs.hh"                      // for move_files, delete_files, build_path_index
#include "io.hh"                      // for io::*
#include "length.h"                   // for serialise_string, unserialise_string
#include "manager.h"                  // for XapiandManager::manager
#include "server/binary_client.h"     // for BinaryClient


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


/*  ____            _ _           _   _
 * |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
 * | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
 * |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
 * |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
 *           |_|
 */


using dispatch_func = void (Replication::*)(const std::string&);


Replication::Replication(BinaryClient& client_)
	: LockableDatabase(),
	  client(client_),
	  lk_db(this)
{
}


Replication::~Replication()
{
	reset();
}


void
Replication::reset()
{
	wal.reset();

	if (switch_database) {
		switch_database->close();
		XapiandManager::manager->database_pool.checkin(switch_database);
	}

	if (!switch_database_path.empty()) {
		delete_files(switch_database_path.c_str());
		switch_database_path.clear();
	}
}


bool
Replication::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_CALL("Replication::init_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	src_endpoints = Endpoints{src_endpoint};

	flags = DB_WRITABLE | DB_CREATE_OR_OPEN;
	endpoints = Endpoints{dst_endpoint};
	lk_db.lock();

	client.temp_directory_template = endpoints[0].path + "/.tmp.XXXXXX";

	L_REPLICATION("init_replication: %s -->  %s", repr(src_endpoints.to_string()), repr(endpoints.to_string()));
	return true;
}


void
Replication::send_message(ReplicationReplyType type, const std::string& message)
{
	L_CALL("Replication::send_message(%s, <message>)", ReplicationReplyTypeNames(type));

	L_BINARY_PROTO("<< send_message (%s): %s", ReplicationReplyTypeNames(type), repr(message));

	client.send_message(toUType(type), message);
}


void
Replication::send_file(ReplicationReplyType type, int fd)
{
	L_CALL("Replication::send_file(%s, <fd>)", ReplicationReplyTypeNames(type));

	L_BINARY_PROTO("<< send_file (%s): %d", ReplicationReplyTypeNames(type), fd);

	client.send_file(toUType(type), fd);
}


void
Replication::replication_server(ReplicationMessageType type, const std::string& message)
{
	L_CALL("Replication::replication_server(%s, <message>)", ReplicationMessageTypeNames(type));

	L_OBJ_BEGIN("Replication::replication_server:BEGIN {type:%s}", ReplicationMessageTypeNames(type));
	L_OBJ_END("Replication::replication_server:END {type:%s}", ReplicationMessageTypeNames(type));

	static const dispatch_func dispatch[] = {
		&Replication::msg_get_changesets,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[static_cast<int>(type)]))(message);
}


void
Replication::msg_get_changesets(const std::string& message)
{
	L_CALL("Replication::msg_get_changesets(<message>)");

	L_REPLICATION("Replication::msg_get_changesets");

	const char *p = message.c_str();
	const char *p_end = p + message.size();

	auto remote_uuid = unserialise_string(&p, p_end);
	auto from_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

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
Replication::replication_client(ReplicationReplyType type, const std::string& message)
{
	L_CALL("Replication::replication_client(%s, <message>)", ReplicationReplyTypeNames(type));

	L_OBJ_BEGIN("Replication::replication_client:BEGIN {type:%s}", ReplicationReplyTypeNames(type));
	L_OBJ_END("Replication::replication_client:END {type:%s}", ReplicationReplyTypeNames(type));

	static const dispatch_func dispatch[] = {
		&Replication::reply_welcome,
		&Replication::reply_end_of_changes,
		&Replication::reply_fail,
		&Replication::reply_db_header,
		&Replication::reply_db_filename,
		&Replication::reply_db_filedata,
		&Replication::reply_db_footer,
		&Replication::reply_changeset,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[static_cast<int>(type)]))(message);
}


void
Replication::reply_welcome(const std::string&)
{
	std::string message;

	message.append(serialise_string(db()->get_uuid()));
	message.append(serialise_length(db()->get_revision()));
	message.append(serialise_string(endpoints[0].path));

	send_message(static_cast<ReplicationReplyType>(SWITCH_TO_REPL), message);
}


void
Replication::reply_end_of_changes(const std::string&)
{
	L_CALL("Replication::reply_end_of_changes(<message>)");

	L_REPLICATION("Replication::reply_end_of_changes%s", switch_database ? " (switching database)" : "");

	if (switch_database) {
		// Close internal databases
		switch_database->close();
		database()->close();

		// get exclusive lock
		XapiandManager::manager->database_pool.lock(database());

		// Now we are sure no readers are using the switch_database
		delete_files(database()->endpoints[0].path, {"*glass", "wal.*"});
		move_files(switch_database->endpoints[0].path, database()->endpoints[0].path);

		// release exclusive lock
		XapiandManager::manager->database_pool.unlock(database());
	}

	L_REPLICATION("Replication completed!");
	if (client.cluster_database) {
		client.cluster_database = false;
		XapiandManager::manager->cluster_database_ready();
	}

	client.destroy();
	client.detach();
}


void
Replication::reply_fail(const std::string&)
{
	L_CALL("Replication::reply_fail(<message>)");

	L_REPLICATION("Replication::reply_fail");

	reset();

	L_ERR("Replication failure!");
	client.destroy();
	client.detach();
}


void
Replication::reply_db_header(const std::string& message)
{
	L_CALL("Replication::reply_db_header(<message>)");

	L_REPLICATION("Replication::reply_db_header");

	const char *p = message.data();
	const char *p_end = p + message.size();

	current_uuid = unserialise_string(&p, p_end);
	current_revision = unserialise_length(&p, p_end);

	reset();

	char path[PATH_MAX];
	strncpy(path, client.temp_directory_template.c_str(), PATH_MAX);
	build_path_index(client.temp_directory_template);
	if (io::mkdtemp(path) == nullptr) {
		L_ERR("Directory %s not created: %s (%d): %s", path, io::strerrno(errno), errno, strerror(errno));
		client.detach();
		return;
	}
	switch_database_path = path;

	L_REPLICATION("Replication::reply_db_header %s", repr(switch_database_path));
}


void
Replication::reply_db_filename(const std::string& filename)
{
	L_CALL("Replication::reply_db_filename(<filename>)");

	L_REPLICATION("Replication::reply_db_filename");

	assert(!switch_database_path.empty());

	file_path = switch_database_path + "/" + filename;
}


void
Replication::reply_db_filedata(const std::string& tmp_file)
{
	L_CALL("Replication::reply_db_filedata(<tmp_file>)");

	L_REPLICATION("Replication::reply_db_filedata %s -> %s", repr(tmp_file), repr(file_path));

	assert(!switch_database_path.empty());

	if (::rename(tmp_file.c_str(), file_path.c_str()) == -1) {
		L_ERR("Cannot rename temporary file %s to %s: %s (%d): %s", tmp_file, file_path, io::strerrno(errno), errno, strerror(errno));
		client.detach();
		return;
	}
}


void
Replication::reply_db_footer(const std::string& message)
{
	L_CALL("Replication::reply_db_footer(<message>)");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t revision = unserialise_length(&p, p_end);

	assert(!switch_database_path.empty());

	if (revision != current_revision) {
		delete_files(switch_database_path.c_str());
		switch_database_path.clear();
	}

	L_REPLICATION("Replication::reply_db_footer%s", revision != current_revision ? " (ignored files)" : "");
}


void
Replication::reply_changeset(const std::string& line)
{
	L_CALL("Replication::reply_changeset(<line>)");

	L_REPLICATION("Replication::reply_changeset%s", switch_database ? " (to switch database)" : "");

	if (!wal) {
		if (switch_database_path.empty()) {
			database()->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(database().get());
		} else {
			if (!switch_database) {
				XapiandManager::manager->database_pool.checkout(switch_database, Endpoints{Endpoint{switch_database_path}}, DB_WRITABLE | DB_SYNC_WAL);
			}
			switch_database->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(switch_database.get());
		}
	}

	wal->execute_line(line, true, false, false);
}


#endif  /* XAPIAND_CLUSTERING */
