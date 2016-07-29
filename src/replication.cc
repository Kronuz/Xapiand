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

#include "replication.h"

#ifdef XAPIAND_CLUSTERING

//  ____            _ _           _   _
// |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
// | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
// |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
// |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
//           |_|
////////////////////////////////////////////////////////////////////////////////


using dispatch_func = void (Replication::*)(const std::string&);


Replication::Replication(BinaryClient* client_)
	: client(client_)
{
		L_OBJ(this, "CREATED REPLICATION OBJ!");
}


Replication::~Replication()
{
	L_OBJ(this, "DELETED REPLICATION OBJ!");
}


void
Replication::replication_server(ReplicationMessageType type, const std::string &message)
{
	static const dispatch_func dispatch[] = {
		&Replication::msg_get_changesets,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			throw MSG_InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		client->checkin_database();
		throw;
	}
}


void
Replication::msg_get_changesets(const std::string &)
{
	L_REPLICATION(this, "Replication::msg_get_changesets");

	// Xapian::Database *db_;
	// const char *p = message.c_str();
	// const char *p_end = p + message.size();

	// size_t len = unserialise_length(&p, p_end, true);
	// std::string uuid(p, len);
	// p += len;

	// len = unserialise_length(&p, p_end, true);
	// std::string from_revision(p, len);
	// p += len;

	// len = unserialise_length(&p, p_end, true);
	// std::string index_path(p, len);
	// p += len;

	// // Select endpoints and get database
	// try {
	// 	endpoints.clear();
	// 	Endpoint endpoint(index_path);
	// 	endpoints.add(endpoint);
	// 	db_ = get_db();
	// 	if (!db_)
	// 		throw MSG_InvalidOperationError("Server has no open database");
	// } catch (...) {
	// 	throw;
	// }

	// char path[] = "/tmp/xapian_changesets_sent.XXXXXX";
	// int fd = mkstemp(path);
	// try {
	// 	std::string to_revision = databases[db_]->checkout_revision;
	// 	L_REPLICATION(this, "Replication::msg_get_changesets for %s (%s) from rev:%s to rev:%s [%d]", endpoints.as_string().c_str(), uuid.c_str(), repr(from_revision, false).c_str(), repr(to_revision, false).c_str(), need_whole_db);

	// 	if (fd < 0) {
	// 		L_ERR(this, "Cannot write to %s (1)", path);
	// 		return;
	// 	}
	// 	// db_->write_changesets_to_fd(fd, from_revision, uuid != db_->get_uuid());  // FIXME: Implement Replication
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
Replication::replication_client(ReplicationReplyType type, const std::string &message)
{
	static const dispatch_func dispatch[] = {
		&Replication::reply_end_of_changes,
		&Replication::reply_fail,
		&Replication::reply_db_header,
		&Replication::reply_db_filename,
		&Replication::reply_db_filedata,
		&Replication::reply_db_footer,
		&Replication::reply_changeset,
	};
	try {
		if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
			std::string errmsg("Unexpected message type ");
			errmsg += std::to_string(toUType(type));
			throw MSG_InvalidArgumentError(errmsg);
		}
		(this->*(dispatch[static_cast<int>(type)]))(message);
	} catch (...) {
		client->checkin_database();
		throw;
	}
}


void
Replication::reply_end_of_changes(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_end_of_changes");

	// if (repl_switched_db) {
	// 	XapiandManager::manager->database_pool.switch_db(*endpoints.cbegin());
	// }

	// client->checkin_database();

	// shutdown();
}


void
Replication::reply_fail(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_fail");

	// L_ERR(this, "Replication failure!");
	// client->checkin_database();

	// shutdown();
}


void
Replication::reply_db_header(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_db_header");

	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// size_t length = unserialise_length(&p, p_end, true);
	// repl_db_uuid.assign(p, length);
	// p += length;
	// repl_db_revision = unserialise_length(&p, p_end);
	// repl_db_filename.clear();

	// Endpoint& endpoint = endpoints[0];
	// std::string path_tmp = endpoint.path + "/.tmp";

	// int dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	// if (dir == 0) {
	// 	L_DEBUG(this, "Directory %s created", path_tmp.c_str());
	// } else if (errno == EEXIST) {
	// 	delete_files(path_tmp.c_str());
	// 	dir = ::mkdir(path_tmp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	// 	if (dir == 0) {
	// 		L_DEBUG(this, "Directory %s created", path_tmp.c_str());
	// 	}
	// } else {
	// 	L_ERR(this, "Directory %s not created (%s)", path_tmp.c_str(), strerror(errno));
	// }
}


void
Replication::reply_db_filename(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_db_filename");

	// const char *p = message.data();
	// const char *p_end = p + message.size();
	// repl_db_filename.assign(p, p_end - p);
}


void
Replication::reply_db_filedata(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_db_filedata");

	// const char *p = message.data();
	// const char *p_end = p + message.size();

	// const Endpoint& endpoint = endpoints[0];

	// std::string path = endpoint.path + "/.tmp/";
	// std::string path_filename = path + repl_db_filename;

	// int fd = io::open(path_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	// if (fd >= 0) {
	// 	L_REPLICATION(this, "path_filename %s", path_filename.c_str());
	// 	if (io::write(fd, p, p_end - p) != p_end - p) {
	// 		L_ERR(this, "Cannot write to %s", repl_db_filename.c_str());
	// 		return;
	// 	}
	// 	io::close(fd);
	// }
}


void
Replication::reply_db_footer(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_db_footer");

	// // const char *p = message.data();
	// // const char *p_end = p + message.size();
	// // size_t revision = unserialise_length(&p, p_end);
	// // Indicates the end of a DB copy operation, signal switch

	// Endpoints endpoints_tmp;
	// Endpoint& endpoint_tmp = endpoints[0];
	// endpoint_tmp.path.append("/.tmp");
	// endpoints_tmp.insert(endpoint_tmp);

	// if (!repl_database_tmp) {
	// 	if (!XapiandManager::manager->database_pool.checkout(repl_database_tmp, endpoints_tmp, DB_WRITABLE | DB_VOLATILE)) {
	// 		L_ERR(this, "Cannot checkout tmp %s", endpoint_tmp.path.c_str());
	// 	}
	// }

	// repl_switched_db = true;
	// repl_just_switched_db = true;
}


void
Replication::reply_changeset(const std::string &)
{
	L_REPLICATION(this, "Replication::reply_changeset");

	// Xapian::WritableDatabase *wdb_;
	// if (repl_database_tmp) {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(repl_database_tmp->db.get());
	// } else {
	// 	wdb_ = static_cast<Xapian::WritableDatabase *>(database);
	// }

	// char path[] = "/tmp/xapian_changes.XXXXXX";
	// int fd = mkstemp(path);
	// if (fd < 0) {
	// 	L_ERR(this, "Cannot write to %s (1)", path);
	// 	return;
	// }

	// std::string header;
	// header += toUType(ReplicationMessageType::REPLY_CHANGESET);
	// header += serialise_length(message.size());

	// if (io::write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
	// 	L_ERR(this, "Cannot write to %s (2)", path);
	// 	return;
	// }

	// if (io::write(fd, message.data(), message.size()) != static_cast<ssize_t>(message.size())) {
	// 	L_ERR(this, "Cannot write to %s (3)", path);
	// 	return;
	// }

	// io::lseek(fd, 0, SEEK_SET);

	// try {
	// 	// wdb_->apply_changeset_from_fd(fd, !repl_just_switched_db);  // FIXME: Implement Replication
	// 	repl_just_switched_db = false;
	// } catch (const MSG_NetworkError& exc) {
	// 	L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
	// 	io::close(fd);
	// 	io::unlink(path);
	// 	throw;
	// } catch (const Xapian::DatabaseError& exc) {
	// 	L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
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


void
Replication::replication_client_file_done()
{
	L_REPLICATION(this, "Replication::replication_client_file_done");

	char buf[1024];
	const char *p;
	const char *p_end;
	std::string buffer;

	ssize_t size = io::read(client->file_descriptor, buf, sizeof(buf));
	buffer.append(buf, size);
	p = buffer.data();
	p_end = p + buffer.size();
	const char *s = p;

	while (p != p_end) {
		ReplicationReplyType type = static_cast<ReplicationReplyType>(*p++);
		ssize_t len = unserialise_length(&p, p_end);
		size_t pos = p - s;
		while (p_end - p < len || static_cast<size_t>(p_end - p) < sizeof(buf) / 2) {
			size = io::read(client->file_descriptor, buf, sizeof(buf));
			if (!size) break;
			buffer.append(buf, size);
			s = p = buffer.data();
			p_end = p + buffer.size();
			p += pos;
		}
		if (p_end - p < len) {
			throw MSG_Error("Replication failure!");
		}
		std::string msg(p, len);
		p += len;

		replication_client(type, msg);

		buffer.erase(0, p - s);
		s = p = buffer.data();
		p_end = p + buffer.size();
	}
}


#endif  /* XAPIAND_CLUSTERING */
