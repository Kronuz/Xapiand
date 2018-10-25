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

#include "database_handler.h"
#include "io_utils.h"
#include "length.h"
#include "server/binary_client.h"


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
	  client(client_)
{
	L_OBJ("CREATED REPLICATION OBJ!");
}


Replication::~Replication()
{
	if (!switch_db.empty()) {
		delete_files(switch_db.c_str());
	}

	L_OBJ("DELETED REPLICATION OBJ!");
}


bool
Replication::init_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint)
{
	L_CALL("Replication::init_replication(%s, %s)", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	src_endpoints = Endpoints{src_endpoint};
	endpoints = Endpoints{dst_endpoint};
	L_REPLICATION("init_replication: %s  -->  %s", repr(src_endpoints.to_string()), repr(endpoints.to_string()));

	flags = DB_WRITABLE | DB_SPAWN | DB_NOWAL;

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
	endpoints = Endpoints{Endpoint{unserialise_string(&p, p_end)}};

	flags = DB_WRITABLE | DB_NOWAL;
	lock_database lk_db(this);
	auto uuid = db()->get_uuid();
	auto revision = db()->get_revision();
	lk_db.unlock();

	if (from_revision && uuid != remote_uuid) {
		from_revision = 0;
	}

	// TODO: Implement WAL's has_revision() and iterator
	// WAL required on a local writable database, open it.
	lk_db.lock();
	DatabaseWAL wal(endpoints[0].path, database().get());
	if (from_revision && !wal.has_revision(from_revision).first) {
		from_revision = 0;
	}
	lk_db.unlock();

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

		// TODO: Implement WAL's has_revision() and iterator
		int wal_iterations = 5;
		do {
			// Send WAL operations.
			auto wal_it = wal.find(from_revision);
			for (; wal_it != wal.end(); ++wal_it) {
				send_message(ReplicationReplyType::REPLY_CHANGESET,
					serialise_string(wal_it->second)
				);
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

	lock_database lk_db(this);
	message.append(serialise_string(db()->get_uuid()));
	message.append(serialise_length(db()->get_revision()));
	message.append(serialise_string(endpoints[0].path));
	lk_db.unlock();

	send_message(static_cast<ReplicationReplyType>(SWITCH_TO_REPL), message);
}

void
Replication::reply_end_of_changes(const std::string&)
{
	L_CALL("Replication::reply_end_of_changes(<message>)");

	L_REPLICATION("Replication::reply_end_of_changes");

	if (switch_db.empty()) {
		flags = DB_WRITABLE;
		lock_database lk_db(this);
	} else {
		XapiandManager::manager->database_pool.switch_db(endpoints[0]/*, switch_db*/);  // TODO: pass switch_db
	}

	client.destroy();
	client.detach();
}


void
Replication::reply_fail(const std::string&)
{
	L_CALL("Replication::reply_fail(<message>)");

	L_REPLICATION("Replication::reply_fail");

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

	char switch_db_path[PATH_MAX];
	snprintf(switch_db_path, PATH_MAX, "%s/.tmp.XXXXXX", endpoints[0].path.c_str());
	if (io::mkdtemp(switch_db_path) == nullptr) {
		L_ERR("Directory %s not created: %s (%d): %s", switch_db, io::strerrno(errno), errno, strerror(errno));
		client.destroy();
		client.detach();
		return;
	}

	switch_db = switch_db_path;

	L_REPLICATION("Replication::reply_db_header %s", repr(switch_db));
}


void
Replication::reply_db_filename(const std::string& filename)
{
	L_CALL("Replication::reply_db_filename(<filename>)");

	L_REPLICATION("Replication::reply_db_filename");

	file_path = switch_db + "/" + filename;
}


void
Replication::reply_db_filedata(const std::string& tmp_file)
{
	L_CALL("Replication::reply_db_filedata(<tmp_file>)");

	L_REPLICATION("Replication::reply_db_filedata %s -> %s", repr(tmp_file), repr(file_path));

	if (::rename(tmp_file.c_str(), file_path.c_str()) == -1) {
		L_ERR("Cannot rename temporary file %s to %s: %s (%d): %s", tmp_file, file_path, io::strerrno(errno), errno, strerror(errno));
		client.destroy();
		client.detach();
		return;
	}
}


void
Replication::reply_db_footer(const std::string& message)
{
	L_CALL("Replication::reply_db_footer(<message>)");

	L_REPLICATION("Replication::reply_db_footer");

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t revision = unserialise_length(&p, p_end);

	if (revision != current_revision) {
		delete_files(switch_db.c_str());
		switch_db.clear();
	}
}


void
Replication::reply_changeset(const std::string& message)
{
	L_CALL("Replication::reply_changeset(<message>)");

	L_REPLICATION("Replication::reply_changeset");

	ignore_unused(message);

	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// auto line = unserialise_string(&p, p_end);

	if (switch_db.empty()) {
		// write changeset to WAL in endpoints[0] directory
		// DatabaseWAL wal(switch_db, nullptr);
		// wal.write_line(line);  // TODO: Implement
	} else {
		// write changeset to WAL in '.tmp' directory (in switch_db)
		// flags = DB_WRITABLE;
		// lock_database lk_db(this);
		// DatabaseWAL wal(endpoints[0].path, database.get());
		// wal.write_line(line);  // TODO: Implement
	}
}


#endif  /* XAPIAND_CLUSTERING */
