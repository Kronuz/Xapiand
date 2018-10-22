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


/*  ____            _ _           _   _
 * |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
 * | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
 * |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
 * |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
 *           |_|
 */


using dispatch_func = void (Replication::*)(const std::string&);


Replication::Replication(BinaryClient& client_)
	: client(client_),
	  database_locks(0),
	  flags(DB_OPEN),
	  file_descriptor(-1)
{
	L_OBJ("CREATED REPLICATION OBJ!");
}


Replication::~Replication()
{
	L_OBJ("DELETED REPLICATION OBJ!");
}


void
Replication::on_read_file(const char *buf, ssize_t received)
{
	L_CALL("BinaryClient::on_read_file(<buf>, %zu)", received);

	L_BINARY_WIRE("BinaryClient::on_read_file: %zd bytes", received);

	io::write(file_descriptor, buf, received);
}


void
Replication::on_read_file_done()
{
	L_CALL("Replication::on_read_file_done()");

	io::close(file_descriptor);
	file_descriptor = -1;
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
Replication::replication_server(ReplicationMessageType type, const std::string& message)
{
	L_CALL("Replication::replication_server(%s, <message>)", ReplicationMessageTypeNames(type));

	static const dispatch_func dispatch[] = {
		&Replication::msg_get_changesets,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		client.remote_protocol.checkin_database();
		throw;
	}
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

	lock_database<Replication> lk_db(this);
	auto uuid = database->db->get_uuid();
	auto revision = database->db->get_revision();
	lk_db.unlock();

	if (uuid != remote_uuid) {
		from_revision = 0;
	}

	bool need_whole_db = false;
	if (from_revision == 0) {
		need_whole_db = true;
	}

	if (from_revision < revision) {
		if (need_whole_db) {
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
					send_message(ReplicationReplyType::REPLY_DB_FILE, filename);
					client.write_buffer(std::make_shared<Buffer>(fd));
				}
			}

			lk_db.lock();
			revision = database->db->get_revision();
			lk_db.unlock();

			send_message(ReplicationReplyType::REPLY_DB_FOOTER, serialise_length(revision));
		}
	}


	// // Select endpoints and get database
	// Xapian::Database *db_;
	// try {
	// 	endpoints.clear();
	// 	Endpoint endpoint(index_path);
	// 	endpoints.add(endpoint);
	// 	db_ = get_db();
	// 	if (!db_)
	// 		THROW(InvalidOperationError, "Server has no open database");
	// } catch (...) {
	// 	throw;
	// }

	// char path[] = "/tmp/xapian_changesets_sent.XXXXXX";
	// int fd = mkstemp(path);
	// try {
	// 	std::string to_revision = databases[db_]->checkout_revision;
	// 	L_REPLICATION("Replication::msg_get_changesets for %s (%s) from rev:%s to rev:%s [%d]", endpoints.as_string(), uuid, repr(from_revision, false), repr(to_revision, false), need_whole_db);

	// 	if (fd == -1) {
	// 		L_ERR("Cannot write to %s (1)", path);
	// 		return;
	// 	}
	// 	// db_->write_changesets_to_fd(fd, from_revision, uuid != db_->get_uuid().to_string());  // FIXME: Implement Replication
	// } catch (...) {
	// 	release_db(db_);
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// }
	// release_db(db_);

	// send_file(fd);

	// io::close(fd);
	// io::unlink(path);
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
		&Replication::reply_db_file,
		&Replication::reply_db_footer,
		&Replication::reply_changeset,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			THROW(InvalidArgumentError, errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		client.remote_protocol.checkin_database();
		throw;
	}
}


void
Replication::reply_welcome(const std::string&)
{
	// strcpy(file_path, "/tmp/xapian_changesets_received.XXXXXX");
	// file_descriptor = mkstemp(file_path);
	// if (file_descriptor < 0) {
	// 	L_ERR(this, "Cannot write to %s (1)", file_path);
	// 	return;
	// }

	// read_file();

	std::string message;

	lock_database<Replication> lk_db(this);
	message.append(serialise_string(database->db->get_uuid()));
	message.append(serialise_length(database->db->get_revision()));
	message.append(serialise_string(endpoints[0].path));
	lk_db.unlock();

	send_message(static_cast<ReplicationReplyType>(SWITCH_TO_REPL), message);
}

void
Replication::reply_end_of_changes(const std::string&)
{
	L_CALL("Replication::reply_end_of_changes(<message>)");

	L_REPLICATION("Replication::reply_end_of_changes");

	// if (repl_switched_db) {
	// 	XapiandManager::manager->database_pool.switch_db(*endpoints.cbegin());
	// }

	// client.remote_protocol.checkin_database();

	// shutdown();
}


void
Replication::reply_fail(const std::string&)
{
	L_CALL("Replication::reply_fail(<message>)");

	L_REPLICATION("Replication::reply_fail");

	// L_ERR("Replication failure!");
	// client.remote_protocol.checkin_database();

	// shutdown();
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

	std::string path_tmp = endpoints[0].path + "/.tmp";

	int dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (dir == 0) {
		L_DEBUG("Directory %s created", path_tmp);
	} else if (errno == EEXIST) {
		delete_files(path_tmp.c_str());
		dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (dir == 0) {
			L_DEBUG("Directory %s created", path_tmp);
		}
	} else {
		L_ERR("Directory %s not created (%s)", path_tmp, strerror(errno));
	}
}


void
Replication::reply_db_file(const std::string& filename)
{
	L_CALL("Replication::reply_db_file(<filename>)");

	L_REPLICATION("Replication::reply_db_file");

	auto path = endpoints[0].path + "/.tmp/" + filename;
	file_descriptor = io::open(path.c_str());

	client.read_file();
}


void
Replication::reply_db_footer(const std::string&)
{
	L_CALL("Replication::reply_db_footer(<message>)");

	L_REPLICATION("Replication::reply_db_footer");

	// // const char *p = message.data();
	// // const char *p_end = p + message.size();
	// // size_t revision = unserialise_length(&p, p_end);
	// // Indicates the end of a DB copy operation, signal switch

	// Endpoints endpoints_tmp;
	// Endpoint& endpoint_tmp = endpoints[0];
	// endpoint_tmp.path.append("/.tmp");
	// endpoints_tmp.insert(endpoint_tmp);

	// if (!repl_database_tmp) {
	// 	try {
	// 		XapiandManager::manager->database_pool.checkout(repl_database_tmp, endpoints_tmp, DB_WRITABLE | DB_VOLATILE);
	// 	} catch (const CheckoutError&)
	// 		L_ERR("Cannot checkout tmp %s", endpoint_tmp.path);
	// 	}
	// }

	// repl_switched_db = true;
	// repl_just_switched_db = true;
}


void
Replication::reply_changeset(const std::string&)
{
	L_CALL("Replication::reply_changeset(<message>)");

	L_REPLICATION("Replication::reply_changeset");

	// Xapian::WritableDatabase *wdb_;
	// if (repl_database_tmp) {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database_tmp->db.get());
	// } else {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(database);
	// }

	// char path[] = "/tmp/xapian_changes.XXXXXX";
	// int fd = mkstemp(path);
	// if (fd == -1) {
	// 	L_ERR("Cannot write to %s (1)", path);
	// 	return;
	// }

	// std::string header;
	// header += toUType(ReplicationMessageType::REPLY_CHANGESET);
	// header += serialise_length(message.size());

	// if (io::write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
	// 	L_ERR("Cannot write to %s (2)", path);
	// 	return;
	// }

	// if (io::write(fd, message.data(), message.size()) != static_cast<ssize_t>(message.size())) {
	// 	L_ERR("Cannot write to %s (3)", path);
	// 	return;
	// }

	// io::lseek(fd, 0, SEEK_SET);

	// try {
	// 	// wdb_->apply_changeset_from_fd(fd, !repl_just_switched_db);  // FIXME: Implement Replication
	// 	repl_just_switched_db = false;
	// } catch (const MSG_NetworkError& exc) {
	// 	L_EXC("ERROR: %s", exc.get_description());
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// } catch (const Xapian::DatabaseError& exc) {
	// 	L_EXC("ERROR: %s", exc.get_description());
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// } catch (...) {
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// }

	// io::close(fd);
	// io::unlink(path);
}


#endif  /* XAPIAND_CLUSTERING */
