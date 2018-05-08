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

#include "database.h"

#include <algorithm>              // for move
#include <array>                  // for array
#include <dirent.h>               // for closedir, DIR
#include <iterator>               // for distance
#include <limits>                 // for numeric_limits
#include <ratio>                  // for ratio
#include <utility>
#include <strings.h>              // for strncasecmp
#include <sys/errno.h>            // for __error, errno
#include <sys/fcntl.h>            // for O_CREAT, O_WRONLY, O_EXCL
#include <sysexits.h>             // for EX_SOFTWARE

#include "atomic_shared_ptr.h"    // for atomic_shared_ptr
#include "database_autocommit.h"  // for DatabaseAutocommit
#include "database_handler.h"     // for DatabaseHandler
#include "exception.h"            // for Error, MSG_Error, Exception, DocNot...
#include "ignore_unused.h"        // for ignore_unused
#include "io_utils.h"             // for close, strerrno, write, open
#include "length.h"               // for serialise_length, unserialise_length
#include "log.h"                  // for L_OBJ, L_CALL
#include "manager.h"              // for sig_exit
#include "msgpack.h"              // for MsgPack
#include "msgpack/unpack.hpp"     // for unpack_error
#include "schema.h"               // for FieldType, FieldType::KEYWORD
#include "serialise.h"            // for uuid
#include "string.hh"              // for string::from_delta
#include "utils.h"                // for repr, to_string, File_ptr, find_fil...


#define XAPIAN_LOCAL_DB_FALLBACK 1

#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10

#define DATA_STORAGE_PATH "docdata."

#define WAL_STORAGE_PATH "wal."

#define MAGIC 0xC0DE

#define WAL_SYNC_MODE     STORAGE_ASYNC_SYNC
#define XAPIAN_SYNC_MODE  0       // This could also be Xapian::DB_FULL_SYNC for xapian to ensure full sync
#define STORAGE_SYNC_MODE STORAGE_FULL_SYNC

#define DB_TIMEOUT 3


/*  ____        _        _                  __        ___    _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |___
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____|
 *
 */


#if XAPIAND_DATABASE_WAL


void
WalHeader::init(void* param, void* args)
{
	const auto* wal = static_cast<const DatabaseWAL*>(param);
	auto commit_eof = static_cast<bool>(args);

	head.magic = MAGIC;
	head.offset = STORAGE_START_BLOCK_OFFSET;
	strncpy(head.uuid, wal->database->get_uuid().c_str(), sizeof(head.uuid));

	auto revision = wal->database->get_revision();
	if (commit_eof) {
		++revision;
	}

	head.revision = revision;
}


void
WalHeader::validate(void* param, void* /*unused*/)
{
	if (head.magic != MAGIC) {
		THROW(StorageCorruptVolume, "Bad WAL header magic number");
	}

	const auto* wal = static_cast<const DatabaseWAL*>(param);
	if (wal->validate_uuid && (strncasecmp(head.uuid, wal->database->get_uuid().c_str(), sizeof(head.uuid)) != 0)) {
		THROW(StorageCorruptVolume, "WAL UUID mismatch");
	}
}


DatabaseWAL::DatabaseWAL(std::string_view base_path_, Database* database_)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(base_path_, this),
	  modified(false),
	  validate_uuid(true),
	  database(database_)
{
	L_OBJ("CREATED DATABASE WAL!");
}


DatabaseWAL::~DatabaseWAL()
{
	L_OBJ("DELETED DATABASE WAL!");
}


bool
DatabaseWAL::open_current(bool commited)
{
	L_CALL("DatabaseWAL::open_current(%s)", commited ? "true" : "false");

	uint32_t revision = database->reopen_revision;

	DIR *dir = opendir(base_path.c_str(), true);
	if (dir == nullptr) {
		THROW(Error, "Could not open the dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;
	uint32_t lowest_revision = std::numeric_limits<uint32_t>::max();

	File_ptr fptr;
	find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);

	while (fptr.ent != nullptr) {
		try {
			uint32_t file_revision = get_volume(std::string(fptr.ent->d_name));
			if (static_cast<long>(file_revision) >= static_cast<long>(revision - WAL_SLOTS)) {
				if (file_revision < lowest_revision) {
					lowest_revision = file_revision;
				}

				if (file_revision > highest_revision) {
					highest_revision = file_revision;
				}
			}
		} catch (const std::invalid_argument&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name), strerror(errno));
		} catch (const std::out_of_range&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name), strerror(errno));
		}

		find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);
	}

	closedir(dir);
	if (lowest_revision > revision) {
		create(revision);
		return false;
	}

	modified = false;
	bool reach_end = false;
	uint32_t start_off, end_off;
	uint32_t file_rev, begin_rev, rev;
	for (rev = lowest_revision; rev <= highest_revision && not reach_end; ++rev) {
		try {
			open(string::format(WAL_STORAGE_PATH "%u", rev), STORAGE_OPEN);
		} catch (const StorageIOError&) {
			continue;
		}

		file_rev = begin_rev = rev;

		uint32_t high_slot = highest_valid_slot();
		if (high_slot == static_cast<uint32_t>(-1)) {
			continue;
		}

		if (rev == highest_revision) {
			reach_end = true; /* Avoid reenter to the loop with the high valid slot of the highest revision */
			if (!commited) {
				/* last slot contain offset at the end of file */
				/* In case not "commited" not execute the high slot avaible because are operations without commit */
				--high_slot;
			}
		}

		if (rev == lowest_revision) {
			auto slot = revision - header.head.revision - 1;
			if (slot == static_cast<uint32_t>(-1)) {
				/* The offset saved in slot 0 is the beginning of the revision 1 to reach 2
				 * for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
				 */
				start_off = STORAGE_START_BLOCK_OFFSET;
				begin_rev = header.head.revision;
			} else {
				if (slot > high_slot) {
					rev = header.head.revision + high_slot;
					continue;
				}
				start_off = header.slot[slot];
				begin_rev = header.head.revision + slot;
			}
		} else {
			start_off = STORAGE_START_BLOCK_OFFSET;
		}

		seek(start_off);

		end_off =  header.slot[high_slot];

		rev = header.head.revision + high_slot;

		if (start_off < end_off) {
			L_INFO("Read and execute operations WAL file [wal.%u] from (%u..%u) revision", file_rev, begin_rev, rev);
		}

		try {
			while (true) {
				std::string line = read(end_off);
				if (!execute(line)) {
					THROW(StorageCorruptVolume, "WAL revision mismatch!");
				}
			}
		} catch (const StorageEOF& exc) { }
	}

	if (rev < revision) {
		THROW(StorageCorruptVolume, "Revision not reached");
	}

	create(highest_revision);

	return modified;
}


bool
DatabaseWAL::create(uint32_t revision)
{
	try {
		return open(string::format(WAL_STORAGE_PATH "%u", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
	} catch (StorageEmptyFile) {
		initialize_file(reinterpret_cast<void*>(false));
		return true;
	}
}


MsgPack
DatabaseWAL::repr_line(std::string_view line)
{
	L_CALL("DatabaseWAL::repr_line(<line>)");

	const char *p = line.data();
	const char *p_end = p + line.size();

	MsgPack repr;

	repr["revision"] = unserialise_length(&p, p_end);

	auto type = static_cast<Type>(unserialise_length(&p, p_end));

	std::string data(p, p_end);

	size_t size;

	p = data.data();
	p_end = p + data.size();

	switch (type) {
		case Type::ADD_DOCUMENT:
			repr["op"] = "ADD_DOCUMENT";
			repr["document"] = data;
			break;
		case Type::CANCEL:
			repr["op"] = "CANCEL";
			break;
		case Type::DELETE_DOCUMENT_TERM:
			repr["op"] = "DELETE_DOCUMENT_TERM";
			size = unserialise_length(&p, p_end, true);
			repr["term"] = std::string(p, size);
			break;
		case Type::COMMIT:
			repr["op"] = "COMMIT";
			break;
		case Type::REPLACE_DOCUMENT:
			repr["op"] = "REPLACE_DOCUMENT";
			repr["docid"] = unserialise_length(&p, p_end);
			repr["document"] = std::string(p, p_end - p);
			break;
		case Type::REPLACE_DOCUMENT_TERM:
			repr["op"] = "REPLACE_DOCUMENT_TERM";
			size = unserialise_length(&p, p_end, true);
			repr["term"] = std::string(p, size);
			repr["document"] = std::string(p + size, p_end - p - size);
			break;
		case Type::DELETE_DOCUMENT:
			repr["op"] = "DELETE_DOCUMENT";
			repr["docid"] = unserialise_length(&p, p_end);
			break;
		case Type::SET_METADATA:
			repr["op"] = "SET_METADATA";
			size = unserialise_length(&p, p_end, true);
			repr["key"] = std::string(p, size);
			repr["data"] = std::string(p + size, p_end - p - size);
			break;
		case Type::ADD_SPELLING:
			repr["op"] = "ADD_SPELLING";
			repr["term"] = std::string(p, p_end - p);
			repr["freq"] = unserialise_length(&p, p_end);
			break;
		case Type::REMOVE_SPELLING:
			repr["op"] = "REMOVE_SPELLING";
			repr["term"] = std::string(p, p_end - p);
			repr["freq"] = unserialise_length(&p, p_end);
			break;
		default:
			THROW(Error, "Invalid WAL message!");
	}

	return repr;
}


MsgPack
DatabaseWAL::repr(uint32_t start_revision, uint32_t /*end_revision*/)
{
	L_CALL("DatabaseWAL::repr(%u, ...)", start_revision);

	MsgPack repr(MsgPack::Type::ARRAY);

	DIR *dir = opendir(base_path.c_str(), true);
	if (dir == nullptr) {
		THROW(Error, "Could not open the dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;
	uint32_t lowest_revision = std::numeric_limits<uint32_t>::max();

	File_ptr fptr;
	find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);

	while (fptr.ent != nullptr) {
		try {
			uint32_t file_revision = get_volume(std::string(fptr.ent->d_name));
			if (static_cast<long>(file_revision) >= static_cast<long>(start_revision - WAL_SLOTS)) {
				if (file_revision < lowest_revision) {
					lowest_revision = file_revision;
				}

				if (file_revision > highest_revision) {
					highest_revision = file_revision;
				}
			}
		} catch (const std::invalid_argument&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name), strerror(errno));
		} catch (const std::out_of_range&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name), strerror(errno));
		}

		find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);
	}

	closedir(dir);
	if (lowest_revision > start_revision) {
		start_revision = lowest_revision;
	}

	bool reach_end = false;
	uint32_t start_off, end_off;
	uint32_t file_rev, begin_rev, end_rev;
	for (auto rev = lowest_revision; rev <= highest_revision && not reach_end; ++rev) {
		try {
			open(string::format(WAL_STORAGE_PATH "%u", rev), STORAGE_OPEN);
		} catch (const StorageIOError&) {
			continue;
		}

		file_rev = begin_rev = rev;

		uint32_t high_slot = highest_valid_slot();
		if (high_slot == static_cast<uint32_t>(-1)) {
			continue;
		}

		if (rev == highest_revision) {
			reach_end = true; /* Avoid reenter to the loop with the high valid slot of the highest revision */
		}

		if (rev == lowest_revision) {
			auto slot = start_revision - header.head.revision - 1;
			if (slot == static_cast<uint32_t>(-1)) {
				/* The offset saved in slot 0 is the beginning of the revision 1 to reach 2
				 * for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
				 */
				start_off = STORAGE_START_BLOCK_OFFSET;
				begin_rev = header.head.revision;
			} else {
				if (slot > high_slot) {
					rev = header.head.revision + high_slot;
					continue;
				}
				start_off = header.slot[slot];
				begin_rev = header.head.revision + slot;
			}
		} else {
			start_off = STORAGE_START_BLOCK_OFFSET;
		}

		seek(start_off);

		end_off =  header.slot[high_slot];

		if (start_off < end_off) {
			end_rev =  header.head.revision + high_slot;
			L_INFO("Read and repr operations WAL file [wal.%u] from (%u..%u) revision", file_rev, begin_rev, end_rev);
		}

		try {
			while (true) {
				std::string line = read(end_off);
				repr.push_back(repr_line(line));
			}
		} catch (const StorageEOF& exc) { }

		rev = header.head.revision + high_slot;
	}

	return repr;
}


uint32_t
DatabaseWAL::highest_valid_slot()
{
	L_CALL("DatabaseWAL::highest_valid_slot()");

	uint32_t slot = -1;
	for (uint32_t i = 0; i < WAL_SLOTS; ++i) {
		if (header.slot[i] == 0) {
			break;
		}
		slot = i;
	}
	return slot;
}


bool
DatabaseWAL::execute(std::string_view line)
{
	L_CALL("DatabaseWAL::execute(<line>)");

	const char *p = line.data();
	const char *p_end = p + line.size();

	if ((database->flags & DB_WRITABLE) == 0) {
		THROW(Error, "Database is read-only");
	}

	if (!database->endpoints[0].is_local()) {
		THROW(Error, "Can not execute WAL on a remote database!");
	}

	auto revision = unserialise_length(&p, p_end);
	auto db_revision = database->get_revision();

	if (revision != db_revision) {
		return false;
	}

	auto type = static_cast<Type>(unserialise_length(&p, p_end));

	std::string data(p, p_end);

	Xapian::docid did;
	Xapian::Document doc;
	Xapian::termcount freq;
	std::string term;
	size_t size;

	p = data.data();
	p_end = p + data.size();

	modified = true;

	switch (type) {
		case Type::ADD_DOCUMENT:
			doc = Xapian::Document::unserialise(data);
			database->add_document(doc, false, false);
			break;
		case Type::CANCEL:
			database->cancel(false);
			break;
		case Type::DELETE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			database->delete_document_term(term, false, false);
			break;
		case Type::COMMIT:
			database->commit(false);
			modified = false;
			break;
		case Type::REPLACE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			doc = Xapian::Document::unserialise(std::string(p, p_end - p));
			database->replace_document(did, doc, false, false);
			break;
		case Type::REPLACE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			doc = Xapian::Document::unserialise(std::string(p + size, p_end - p - size));
			database->replace_document_term(term, doc, false, false);
			break;
		case Type::DELETE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			database->delete_document(did, false, false);
			break;
		case Type::SET_METADATA:
			size = unserialise_length(&p, p_end, true);
			database->set_metadata(std::string(p, size), std::string(p + size, p_end - p - size), false, false);
			break;
		case Type::ADD_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database->add_spelling(std::string(p, p_end - p), freq, false, false);
			break;
		case Type::REMOVE_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database->remove_spelling(std::string(p, p_end - p), freq, false, false);
			break;
		default:
			THROW(Error, "Invalid WAL message!");
	}

	return true;
}


bool
DatabaseWAL::init_database()
{
	L_CALL("DatabaseWAL::init_database()");

	static const std::array<std::string, 2> iamglass({{
		std::string("\x0f\x0d\x58\x61\x70\x69\x61\x6e\x20\x47\x6c\x61\x73\x73\x04\x6e"),
		std::string("\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00\x03\x00"
			"\x04\x04\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00"
			"\x03\x00\x04\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00")
	}});

	auto filename = base_path + "iamglass";
	if (exists(filename)) {
		return true;
	}

	validate_uuid = false;

	try {
		open(string::format(WAL_STORAGE_PATH "%u", 0), STORAGE_OPEN | STORAGE_COMPRESS);
	} catch (const StorageIOError&) {
		return true;
	}

	UUID uuid(header.head.uuid, UUID_LENGTH);
	const auto& bytes = uuid.get_bytes();
	std::string uuid_str(bytes.begin(), bytes.end());

	int fd = io::open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL);
	if (unlikely(fd < 0)) {
		L_ERR("ERROR: opening file. %s\n", filename);
		return false;
	}
	if unlikely(io::write(fd, iamglass[0].data(), iamglass[0].size()) < 0) {
		L_ERRNO("io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, uuid_str.data(), uuid_str.size()) < 0) {
		L_ERRNO("io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, iamglass[1].data(), iamglass[1].size()) < 0) {
		L_ERRNO("io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	io::close(fd);

	filename = base_path + "postlist.glass";
	fd = io::open(filename.c_str(), O_WRONLY | O_CREAT);
	if (unlikely(fd < 0)) {
		L_ERR("ERROR: opening file. %s\n", filename);
		return false;
	}
	io::close(fd);

	return true;
}


void
DatabaseWAL::write_line(Type type, std::string_view data, bool commit_)
{
	L_CALL("DatabaseWAL::write_line(...)");

	assert(database->flags & DB_WRITABLE);
	assert(!(database->flags & DB_NOWAL));

	auto endpoint = database->endpoints[0];
	assert(endpoint.is_local());

	std::string revision_encode = database->get_revision_str();
	std::string uuid = database->get_uuid();
	std::string line = revision_encode;
	line.append(serialise_length(toUType(type)));
	line.append(data);

	L_DATABASE_WAL("%s on %s: '%s'", names[toUType(type)], endpoint.path, repr(line, quote));

	uint32_t rev = database->get_revision();

	uint32_t slot = rev - header.head.revision;

	if (slot >= WAL_SLOTS) {
		close();
		open(string::format(WAL_STORAGE_PATH "%u", rev), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
		slot = rev - header.head.revision;
	}

	write(line.data(), line.size());
	header.slot[slot] = header.head.offset; /* Beginning of the next revision */

	if (commit_) {
		if (slot + 1 >= WAL_SLOTS) {
			close();
			open(string::format(WAL_STORAGE_PATH "%u", rev + 1), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE, true);
		} else {
			header.slot[slot + 1] = header.slot[slot];
		}
	}

	commit();
}


void
DatabaseWAL::write_add_document(const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_add_document(<doc>)");

	auto line = doc.serialise();
	write_line(Type::ADD_DOCUMENT, line);
}


void
DatabaseWAL::write_cancel()
{
	L_CALL("DatabaseWAL::write_cancel()");

	write_line(Type::CANCEL, "");
}


void
DatabaseWAL::write_delete_document_term(std::string_view term)
{
	L_CALL("DatabaseWAL::write_delete_document_term(<term>)");

	auto line = serialise_string(term);
	write_line(Type::DELETE_DOCUMENT_TERM, line);
}


void
DatabaseWAL::write_commit()
{
	L_CALL("DatabaseWAL::write_commit()");

	write_line(Type::COMMIT, "", true);
}


void
DatabaseWAL::write_replace_document(Xapian::docid did, const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_replace_document(...)");

	auto line = serialise_length(did);
	line.append(doc.serialise());
	write_line(Type::REPLACE_DOCUMENT, line);
}


void
DatabaseWAL::write_replace_document_term(std::string_view term, const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_replace_document_term(...)");

	auto line = serialise_string(term);
	line.append(doc.serialise());
	write_line(Type::REPLACE_DOCUMENT_TERM, line);
}


void
DatabaseWAL::write_delete_document(Xapian::docid did)
{
	L_CALL("DatabaseWAL::write_delete_document(<did>)");

	auto line = serialise_length(did);
	write_line(Type::DELETE_DOCUMENT, line);
}


void
DatabaseWAL::write_set_metadata(std::string_view key, std::string_view val)
{
	L_CALL("DatabaseWAL::write_set_metadata(...)");

	auto line = serialise_string(key);
	line.append(val);
	write_line(Type::SET_METADATA, line);
}


void
DatabaseWAL::write_add_spelling(std::string_view word, Xapian::termcount freqinc)
{
	L_CALL("DatabaseWAL::write_add_spelling(...)");

	auto line = serialise_length(freqinc);
	line.append(word);
	write_line(Type::ADD_SPELLING, line);
}


void
DatabaseWAL::write_remove_spelling(std::string_view word, Xapian::termcount freqdec)
{
	L_CALL("DatabaseWAL::write_remove_spelling(...)");

	auto line = serialise_length(freqdec);
	line.append(word);
	write_line(Type::REMOVE_SPELLING, line);
}

#endif


/*  ____        _          ____  _
 * |  _ \  __ _| |_ __ _  / ___|| |_ ___  _ __ __ _  __ _  ___
 * | | | |/ _` | __/ _` | \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
 * | |_| | (_| | || (_| |  ___) | || (_) | | | (_| | (_| |  __/
 * |____/ \__,_|\__\__,_| |____/ \__\___/|_|  \__,_|\__, |\___|
 *                                                  |___/
 */


#ifdef XAPIAND_DATA_STORAGE
void
DataHeader::init(void* param, void* /*unused*/)
{
	const auto* database = static_cast<const Database*>(param);

	head.offset = STORAGE_START_BLOCK_OFFSET;
	head.magic = STORAGE_MAGIC;
	strncpy(head.uuid, database->get_uuid().c_str(), sizeof(head.uuid));
}


void
DataHeader::validate(void* param, void* /*unused*/)
{
	if (head.magic != STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	const auto* database = static_cast<const Database*>(param);
	if (strncasecmp(head.uuid, database->get_uuid().c_str(), sizeof(head.uuid)) != 0) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


DataStorage::DataStorage(std::string_view base_path_, void* param_, int flags)
	: Storage<DataHeader, DataBinHeader, DataBinFooter>(base_path_, param_),
	  flags(flags)
{
	L_OBJ("CREATED DATABASE DATA STORAGE!");
}


DataStorage::~DataStorage()
{
	L_OBJ("DELETED DATABASE DATA STORAGE!");
}


bool
DataStorage::open(std::string_view relative_path)
{
	return Storage<DataHeader, DataBinHeader, DataBinFooter>::open(relative_path, flags);
}


uint32_t
DataStorage::highest_volume()
{
	L_CALL("DataStorage::highest_volume()");

	DIR *dir = opendir(base_path.c_str(), true);
	if (dir == nullptr) {
		THROW(Error, "Could not open dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;

	File_ptr fptr;
	find_file_dir(dir, fptr, DATA_STORAGE_PATH, true);

	while (fptr.ent != nullptr) {
		try {
			uint32_t file_revision = get_volume(std::string(fptr.ent->d_name));
			if (file_revision > highest_revision) {
				highest_revision = file_revision;
			}
		} catch (const std::invalid_argument&) {
		} catch (const std::out_of_range&) { }

		find_file_dir(dir, fptr, DATA_STORAGE_PATH, true);
	}
	closedir(dir);
	return highest_revision;
}
#endif /* XAPIAND_DATA_STORAGE */



/*  ____        _        _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|
 *
 */


Database::Database(std::shared_ptr<DatabaseQueue>& queue_, Endpoints  endpoints_, int flags_)
	: weak_queue(queue_),
	  endpoints(std::move(endpoints_)),
	  flags(flags_),
	  hash(endpoints.hash()),
	  mastery_level(-1),
	  reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0)
{
	reopen();

	if (auto queue = weak_queue.lock()) {
		queue->inc_count();
	}

	L_OBJ("CREATED DATABASE!");
}


Database::~Database()
{
	if (auto queue = weak_queue.lock()) {
		queue->dec_count();
	}

	L_OBJ("DELETED DATABASE!");
}


long long
Database::read_mastery(const Endpoint& endpoint)
{
	L_CALL("Database::read_mastery(%s)", repr(endpoint.to_string()));

	if (mastery_level != -1) { return mastery_level; }
	if (!endpoint.is_local()) { return -1; }

	mastery_level = ::read_mastery(endpoint.path, true);

	return mastery_level;
}


bool
Database::reopen()
{
	L_CALL("Database::reopen()");

	reopen_time = std::chrono::system_clock::now();

	if (db) {
		// Try to reopen
		try {
			bool ret = db->reopen();
			L_DATABASE_WRAP("Reopen done (took %s) [1]", string::from_delta(reopen_time, std::chrono::system_clock::now()));
			return ret;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			L_EXC("ERROR: %s", exc.get_description());
		} catch (const Xapian::Error& exc) {
			L_EXC("ERROR: %s", exc.get_description());
		}

		db->close();
		db.reset();
	}

	dbs.clear();

#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif /* XAPIAND_DATA_STORAGE */

	auto endpoints_size = endpoints.size();
	auto i = endpoints.cbegin();
	if ((flags & DB_WRITABLE) != 0) {
		////////////////////////////////////////////////////////////////
		// __        __    _ _        _     _        ____  ____
		// \ \      / / __(_) |_ __ _| |__ | | ___  |  _ \| __ )
		//  \ \ /\ / / '__| | __/ _` | '_ \| |/ _ \ | | | |  _ \
		//   \ V  V /| |  | | || (_| | |_) | |  __/ | |_| | |_) |
		//    \_/\_/ |_|  |_|\__\__,_|_.__/|_|\___| |____/|____/
		//
		assert(endpoints_size == 1);
		db = std::make_unique<Xapian::WritableDatabase>();
		const auto& e = *i;
		Xapian::WritableDatabase wdb;
		bool local = false;
		int _flags = (flags & DB_SPAWN) != 0 ? Xapian::DB_CREATE_OR_OPEN | XAPIAN_SYNC_MODE : Xapian::DB_OPEN | XAPIAN_SYNC_MODE;
#ifdef XAPIAND_CLUSTERING
		if (!e.is_local()) {
			// Writable remote databases do not have a local fallback
			int port = (e.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : e.port;
			wdb = Xapian::Remote::open_writable(e.host, port, 0, 10000, _flags, e.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
			try {
				Xapian::Database tmp = Xapian::Database(e.path, Xapian::DB_OPEN);
				if (tmp.get_uuid() == wdb.get_uuid()) {
					L_DATABASE("Endpoint %s fallback to local database!", repr(e.to_string()));
					// Handle remote endpoints and figure out if the endpoint is a local database
					build_path_index(e.path);
					try {
						wdb = Xapian::WritableDatabase(e.path, _flags);
					} catch (const Xapian::DatabaseOpeningError&) {
						if ((flags & DB_SPAWN) && !exists(e.path + "/iamglass")) {
							_flags = Xapian::DB_CREATE_OR_OVERWRITE | XAPIAN_SYNC_MODE;
							wdb = Xapian::WritableDatabase(e.path, _flags);
						} else throw;
					}
					local = true;
					if (endpoints_size == 1) read_mastery(e);
				}
			} catch (const Xapian::DatabaseOpeningError&) { }
#endif /* XAPIAN_LOCAL_DB_FALLBACK */

		}
		else
#endif /* XAPIAND_CLUSTERING */
		{
#ifdef XAPIAND_DATABASE_WAL
			try {
				DatabaseWAL tmp_wal(e.path, this);
				tmp_wal.init_database();
			} catch (const Exception& exc) {
				L_EXC("WAL ERROR: %s", exc.get_message());
				throw;
			}
#endif
			build_path_index(e.path);
			try {
				wdb = Xapian::WritableDatabase(e.path, _flags);
			} catch (const Xapian::DatabaseOpeningError&) {
				if (((flags & DB_SPAWN) != 0) && !exists(e.path + "/iamglass")) {
					_flags = Xapian::DB_CREATE_OR_OVERWRITE | XAPIAN_SYNC_MODE;
					wdb = Xapian::WritableDatabase(e.path, _flags);
				} else { throw; }
			}
			local = true;
			if (endpoints_size == 1) { read_mastery(e); }
		}

		db->add_database(wdb);
		dbs.emplace_back(wdb, local);

		if (local) {
			reopen_revision = get_revision();
		}

#ifdef XAPIAND_DATA_STORAGE
		if (local) {
			if ((flags & DB_NOSTORAGE) != 0) {
				writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
				storages.push_back(std::make_unique<DataStorage>(e.path, this, STORAGE_OPEN));
			} else {
				writable_storages.push_back(std::make_unique<DataStorage>(e.path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE));
				storages.push_back(std::make_unique<DataStorage>(e.path, this, STORAGE_OPEN));
			}
		} else {
			writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
			storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		}
#endif /* XAPIAND_DATA_STORAGE */

#ifdef XAPIAND_DATABASE_WAL
		try {
			/* If reopen_revision is not available Wal work as a log for the operations */
			if (local && ((flags & DB_NOWAL) == 0)) {
				// WAL required on a local writable database, open it.
				wal = std::make_unique<DatabaseWAL>(e.path, this);
				try {
					if (wal->open_current(true)) {
						if (auto queue = weak_queue.lock()) {
							queue->modified = true;
						}
					}
				} catch (const StorageCorruptVolume& exc) {
					if (wal->create(reopen_revision)) {
						L_WARNING("Revision not found in wal for endpoint %s! (%u)", repr(e.to_string()), reopen_revision);
						if (auto queue = weak_queue.lock()) {
							queue->modified = true;
						}
					} else {
						L_ERR("Revision not found in wal for endpoint %s! (%u)", repr(e.to_string()), reopen_revision);
					}
				}
			}
		} catch (const Exception& exc) {
			L_EXC("WAL ERROR: %s", exc.get_message());
			throw;
		}
#endif /* XAPIAND_DATABASE_WAL */
		// Ends Writable DB
		////////////////////////////////////////////////////////////////
	} else {
		////////////////////////////////////////////////////////////////
		//  ____                _       _     _        ____  ____
		// |  _ \ ___  __ _  __| | __ _| |__ | | ___  |  _ \| __ )
		// | |_) / _ \/ _` |/ _` |/ _` | '_ \| |/ _ \ | | | |  _ \
		// |  _ <  __/ (_| | (_| | (_| | |_) | |  __/ | |_| | |_) |
		// |_| \_\___|\__,_|\__,_|\__,_|_.__/|_|\___| |____/|____/
		//
		auto size_endp = endpoints.size();
		for (db = std::make_unique<Xapian::Database>(); i != endpoints.cend(); ++i) {
			const auto& e = *i;
			Xapian::Database rdb;
			bool local = false;
#ifdef XAPIAND_CLUSTERING
			int _flags = (flags & DB_SPAWN) ? Xapian::DB_CREATE_OR_OPEN : Xapian::DB_OPEN;
			if (!e.is_local()) {
				int port = (e.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : e.port;
				rdb = Xapian::Remote::open(e.host, port, 10000, 10000, _flags, e.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
				try {
					Xapian::Database tmp = Xapian::Database(e.path, Xapian::DB_OPEN);
					if (tmp.get_uuid() == rdb.get_uuid()) {
						L_DATABASE("Endpoint %s fallback to local database!", repr(e.to_string()));
						// Handle remote endpoints and figure out if the endpoint is a local database
						rdb = Xapian::Database(e.path, _flags);
						local = true;
						if (endpoints_size == 1) read_mastery(e);
					}
				} catch (const Xapian::DatabaseOpeningError& exc) { }
#endif /* XAPIAN_LOCAL_DB_FALLBACK */
			}
			else
#endif /* XAPIAND_CLUSTERING */
			{
#ifdef XAPIAND_DATABASE_WAL
				try {
					DatabaseWAL tmp_wal(e.path, this);
					tmp_wal.init_database();
				} catch (const Exception& exc) {
					L_EXC("WAL ERROR: %s", exc.get_message());
					throw;
				}
#endif
				try {
					rdb = Xapian::Database(e.path, Xapian::DB_OPEN);
					local = true;
					if (endpoints_size == 1) { read_mastery(e); }
				} catch (const Xapian::DatabaseOpeningError& exc) {
					if ((flags & DB_SPAWN) == 0)  {
						if (endpoints.size() == 1) {
							db.reset();
							throw;
						}

						--size_endp;
						continue;
					}
					{
						build_path_index(e.path);
						try {
							Xapian::WritableDatabase tmp(e.path, Xapian::DB_CREATE_OR_OPEN);
						} catch (const Xapian::DatabaseOpeningError&) {
							if (!exists(e.path + "/iamglass")) {
								Xapian::WritableDatabase(e.path, Xapian::DB_CREATE_OR_OVERWRITE);
							} else { throw; }
						}
					}

					rdb = Xapian::Database(e.path, Xapian::DB_OPEN);
					local = true;
					if (endpoints_size == 1) { read_mastery(e); }
				}
			}

			db->add_database(rdb);
			dbs.emplace_back(rdb, local);

	#ifdef XAPIAND_DATA_STORAGE
			if (local && endpoints_size == 1) {
				// WAL required on a local database, open it.
				storages.push_back(std::make_unique<DataStorage>(e.path, this, STORAGE_OPEN));
			} else {
				storages.push_back(std::unique_ptr<DataStorage>(nullptr));
			}
	#endif /* XAPIAND_DATA_STORAGE */
		}

		if (size_endp == 0u) {
			throw Xapian::DatabaseOpeningError("Empty set of databases");
		}
		// Ends Readable DB
		////////////////////////////////////////////////////////////////
	}

	L_DATABASE_WRAP("Reopen done (took %s) [1]", string::from_delta(reopen_time, std::chrono::system_clock::now()));

	return true;
}


std::string
Database::get_uuid() const
{
	L_CALL("Database::get_uuid");

	return db->get_uuid();
}


uint32_t
Database::get_revision() const
{
	L_CALL("Database::get_revision()");

#if HAVE_XAPIAN_DATABASE_GET_REVISION
	return db->get_revision();
#else
	return 0;
#endif
}


std::string
Database::get_revision_str() const
{
	L_CALL("Database::get_revision_str()");

	return serialise_length(get_revision());
}


bool
Database::commit(bool wal_)
{
	L_CALL("Database::commit(%s)", wal_ ? "true" : "false");

	auto queue = weak_queue.lock();
	if (queue && !queue->modified) {
		L_DATABASE_WRAP("Do not commit, because there are not changes");
		return false;
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_commit(); }
#else
	ignore_unused(wal_);
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Commit: t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif /* XAPIAND_DATA_STORAGE */
			wdb->commit();
			if (queue) {
				queue->modified = false;
				queue->revision = wdb->get_revision();
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseError& exc) {
			L_WARNING("ERROR: %s", exc.get_description());
			throw;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Commit made (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

	return true;
}


void
Database::cancel(bool wal_)
{
	L_CALL("Database::cancel(%s)", wal_ ? "true" : "false");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Cancel: t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->begin_transaction(false);
			wdb->cancel_transaction();
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Cancel made (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_cancel(); }
#else
	ignore_unused(wal_);
#endif
}


void
Database::delete_document(Xapian::docid did, bool commit_, bool wal_)
{
	L_CALL("Database::delete_document(%d, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Deleting document: %d  t: %d", did, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(did);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document deleted (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_delete_document(did); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


void
Database::delete_document_term(const std::string& term, bool commit_, bool wal_)
{
	L_CALL("Database::delete_document_term(%s, %s, %s)", repr(term), commit_ ? "true" : "false", wal_ ? "true" : "false");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Deleting document: '%s'  t: %d", term, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(term);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document deleted (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_delete_document_term(term); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Database::storage_get_stored(const Xapian::docid& did, const Data::Locator& locator) const
{
	L_CALL("Database::storage_get_stored()");

	assert(locator.type == Data::Type::stored);
	assert(locator.volume != -1);

	int subdatabase = (did - 1) % endpoints.size();
	const auto& storage = storages[subdatabase];
	if (storage) {
		storage->open(string::format(DATA_STORAGE_PATH "%u", locator.volume));
		storage->seek(static_cast<uint32_t>(locator.offset));
		return storage->read();
	}

	return "";
}


void
Database::storage_pull_blobs(Xapian::Document& doc, const Xapian::docid& did) const
{
	L_CALL("Database::storage_pull_blobs()");

	int subdatabase = (did - 1) % endpoints.size();
	const auto& storage = storages[subdatabase];
	if (storage) {
		auto data = Data(doc.get_data());
		for (auto& locator : data) {
			if (locator.type == Data::Type::stored) {
				assert(locator.volume != -1);
				storage->open(string::format(DATA_STORAGE_PATH "%u", locator.volume));
				storage->seek(static_cast<uint32_t>(locator.offset));
				auto stored = storage->read();
				data.update(locator.ct_type, unserialise_string_at(STORED_BLOB, stored));
			}
		}
		data.flush();
		doc.set_data(data.serialise());
	}
}


void
Database::storage_push_blobs(Xapian::Document& doc, const Xapian::docid& did) const
{
	L_CALL("Database::storage_push_blobs()");

	assert((flags & DB_WRITABLE) != 0);

	int subdatabase = (did - 1) % endpoints.size();
	const auto& storage = writable_storages[subdatabase];
	if (storage) {
		auto data = Data(doc.get_data());
		for (auto& locator : data) {
			if (locator.size == 0) {
				data.erase(locator.ct_type);
			}
			if (locator.type == Data::Type::stored) {
				if (!locator.data().empty()) {
					uint32_t offset;
					while (true) {
						try {
							if (storage->closed()) {
								storage->volume = storage->highest_volume();
								storage->open(string::format(DATA_STORAGE_PATH "%u", storage->volume));
							}
							offset = storage->write(serialise_strings({ locator.ct_type.to_string(), locator.data() }));
							break;
						} catch (StorageEOF) {
							++storage->volume;
							storage->open(string::format(DATA_STORAGE_PATH "%u", storage->volume));
						}
					}
					data.update(locator.ct_type, storage->volume, offset, locator.data().size());
				}
			}
		}
		data.flush();
		doc.set_data(data.serialise());
	}
}


void
Database::storage_commit()
{
	L_CALL("Database::storage_commit()");

	for (auto& storage : writable_storages) {
		if (storage) {
			storage->commit();
		}
	}
}
#endif /* XAPIAND_DATA_STORAGE */


Xapian::docid
Database::add_document(const Xapian::Document& doc, bool commit_, bool wal_)
{
	L_CALL("Database::add_document(<doc>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

	Xapian::docid did = 0;

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc_, doc_.get_docid()); // Only writable database get_docid is enough
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Adding new document.  t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->add_document(doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document added (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_add_document(doc); }
#else
	ignore_unused(wal_);
#endif /* XAPIAND_DATABASE_WAL */

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_, bool wal_)
{
	L_CALL("Database::replace_document(%d, <doc>, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc_, did);
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Replacing: %d  t: %d", did, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->replace_document(did, doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document replaced (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_replace_document(did, doc); }
#else
	ignore_unused(wal_);
#endif /* XAPIAND_DATABASE_WAL */

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_, bool wal_)
{
	L_CALL("Database::replace_document_term(%s, <doc>, %s, %s)", repr(term), commit_ ? "true" : "false", wal_ ? "true" : "false");

	Xapian::docid did = 0;

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc_, doc_.get_docid()); // Only writable database get_docid is enough
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Replacing: '%s'  t: %d", term, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->replace_document(term, doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database %s was modified, try again: %s", repr(endpoints.to_string()), exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, "Database %s error: %s", repr(endpoints.to_string()), exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document replaced (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_replace_document_term(term, doc); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}

	return did;
}


void
Database::add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL("Database::add_spelling(<word, <freqinc>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->add_spelling(word, freqinc);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Spelling added (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_add_spelling(word, freqinc); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


void
Database::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Database::remove_spelling(<word>, <freqdec>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->remove_spelling(word, freqdec);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Spelling removed (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_remove_spelling(word, freqdec); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


Xapian::docid
Database::find_document(const std::string& term_id)
{
	L_CALL("Database::find_document(%s)", repr(term_id));

	Xapian::docid did = 0;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			Xapian::PostingIterator it = db->postlist_begin(term_id);
			if (it == db->postlist_end(term_id)) {
				THROW(DocNotFoundError, "Document not found");
			}
			did = *it;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Document found (took %s) [1]", string::from_delta(start, std::chrono::system_clock::now()));

	return did;
}


Xapian::Document
Database::get_document(const Xapian::docid& did, bool assume_valid_, bool pull_)
{
	L_CALL("Database::get_document(%d)", did);

	Xapian::Document doc;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
#ifdef HAVE_XAPIAN_DATABASE_GET_DOCUMENT_WITH_FLAGS
			if (assume_valid_) {
				doc = db->get_document(did, Xapian::DOC_ASSUME_VALID);
			} else
#else
			ignore_unused(assume_valid_);
#endif
			{
				doc = db->get_document(did);
			}
#ifdef XAPIAND_DATA_STORAGE
			if (pull_) {
				storage_pull_blobs(doc, did);
			}
#else
	ignore_unused(pull_);
#endif /* XAPIAND_DATA_STORAGE */
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Got document (took %s) [1]", string::from_delta(start, std::chrono::system_clock::now()));

	return doc;
}


std::string
Database::get_metadata(const std::string& key)
{
	L_CALL("Database::get_metadata(%s)", repr(key));

	std::string value;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			value = db->get_metadata(key);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Got metadata (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

	return value;
}


std::vector<std::string>
Database::get_metadata_keys()
{
	L_CALL("Database::get_metadata_keys()");

	std::vector<std::string> values;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto it = db->metadata_keys_begin();
			auto it_e = db->metadata_keys_end();
			for (; it != it_e; ++it) {
				values.push_back(*it);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
		values.clear();
	}

	L_DATABASE_WRAP("Got metadata keys (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

	return values;
}


void
Database::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL("Database::set_metadata(%s, %s, %s, %s)", repr(key), repr(value), commit_ ? "true" : "false", wal_ ? "true" : "false");

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->set_metadata(key, value);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
	}

	L_DATABASE_WRAP("Set metadata (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_set_metadata(key, value); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


void
Database::dump_metadata(int fd, XXH32_state_t* xxh_state)
{
	L_CALL("Database::dump_metadata()");

	L_DATABASE_WRAP_INIT();

	std::string initial;
	for (int t = DB_RETRIES; t >= 0; --t) {
		std::string key;
		try {
			auto it = db->metadata_keys_begin();
			auto it_e = db->metadata_keys_end();
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				key = *it;
				auto value = db->get_metadata(key);
				serialise_string(fd, key);
				XXH32_update(xxh_state, key.data(), key.size());
				serialise_string(fd, value);
				XXH32_update(xxh_state, value.data(), value.size());
			}
			// mark end:
			serialise_string(fd, "");
			XXH32_update(xxh_state, "", 0);
			serialise_string(fd, "");
			XXH32_update(xxh_state, "", 0);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
		initial = key;
	}

	L_DATABASE_WRAP("Dump metadata (took %s)", string::from_delta(start, std::chrono::system_clock::now()));
}


void
Database::dump_documents(int fd, XXH32_state_t* xxh_state)
{
	L_CALL("Database::dump_documents()");

	L_DATABASE_WRAP_INIT();

	Xapian::docid initial = 1;
	for (int t = DB_RETRIES; t >= 0; --t) {
		Xapian::docid did = initial;
		try {
			auto it = db->postlist_begin("");
			auto it_e = db->postlist_end("");
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				did = *it;
				auto doc = db->get_document(did);
				auto data = Data(doc.get_data());
				for (auto& locator : data) {
					switch (locator.type) {
						case Data::Type::inplace: {
							auto content_type = locator.ct_type.to_string();
							auto blob = locator.data();
							char type = toUType(locator.type);
							serialise_string(fd, blob);
							XXH32_update(xxh_state, blob.data(), blob.size());
							serialise_string(fd, content_type);
							XXH32_update(xxh_state, content_type.data(), content_type.size());
							serialise_char(fd, type);
							XXH32_update(xxh_state, &type, 1);
							break;
						}
						case Data::Type::stored: {
#ifdef XAPIAND_DATA_STORAGE
							auto stored = storage_get_stored(did, locator);
							auto content_type = unserialise_string_at(STORED_CONTENT_TYPE, stored);
							auto blob = unserialise_string_at(STORED_BLOB, stored);
							char type = toUType(locator.type);
							serialise_string(fd, blob);
							XXH32_update(xxh_state, blob.data(), blob.size());
							serialise_string(fd, content_type);
							XXH32_update(xxh_state, content_type.data(), content_type.size());
							serialise_char(fd, type);
							XXH32_update(xxh_state, &type, 1);
#endif
							break;
						}
					}
				}
				serialise_string(fd, "");
				XXH32_update(xxh_state, "", 0);
			}
			// mark end:
			serialise_string(fd, "");
			XXH32_update(xxh_state, "", 0);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
		initial = did;
	}

	L_DATABASE_WRAP("Dump documents (took %s)", string::from_delta(start, std::chrono::system_clock::now()));
}


MsgPack
Database::dump_documents()
{
	L_CALL("Database::dump_documents()");

	L_DATABASE_WRAP_INIT();

	MsgPack docs(MsgPack::Type::ARRAY);
	Xapian::docid initial = 1;
	for (int t = DB_RETRIES; t >= 0; --t) {
		Xapian::docid did = initial;
		try {
			auto it = db->postlist_begin("");
			auto it_e = db->postlist_end("");
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				did = *it;
				auto doc = db->get_document(did);
				auto data = Data(doc.get_data());
				auto main_locator = data.get("");
				auto obj = main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack();
				for (auto& locator : data) {
					switch (locator.type) {
						case Data::Type::inplace: {
							if (!locator.ct_type.empty()) {
								obj["_data"].push_back(MsgPack({
									{ "_content_type", locator.ct_type.to_string() },
									{ "_type", "inplace" },
									{ "_blob", locator.data() },
								}));
							}
							break;
						}
						case Data::Type::stored: {
#ifdef XAPIAND_DATA_STORAGE
							auto stored = storage_get_stored(did, locator);
							obj["_data"].push_back(MsgPack({
								{ "_content_type", unserialise_string_at(STORED_CONTENT_TYPE, stored) },
								{ "_type", "stored" },
								{ "_blob", unserialise_string_at(STORED_BLOB, stored) },
							}));
#endif
							break;
						}
					}
				}
				docs.push_back(obj);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { THROW(TimeOutError, "Database was modified, try again: %s", exc.get_description()); }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_description());
		}
		reopen();
		initial = did;
	}

	L_DATABASE_WRAP("Dump documents (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

	return docs;
}


/*  ____        _        _                     ___
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___ / _ \ _   _  ___ _   _  ___
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ | | | | | |/ _ \ | | |/ _ \
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |_| | |_| |  __/ |_| |  __/
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|\__\_\\__,_|\___|\__,_|\___|
 *
 */

template <typename... Args>
DatabaseQueue::DatabaseQueue(Args&&... args)
	: Queue(std::forward<Args>(args)...),
	  state(replica_state::REPLICA_FREE),
	  modified(false),
	  persistent(false),
	  count(0) {
	L_OBJ("CREATED DATABASE QUEUE!");
}


DatabaseQueue::~DatabaseQueue()
{
	if (size() != count) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	L_OBJ("DELETED DATABASE QUEUE!");
}


bool
DatabaseQueue::inc_count(int max)
{
	L_CALL("DatabaseQueue::inc_count(%d)", max);

	std::lock_guard<std::mutex> lk(_state->_mutex);

	if (count == 0) {
		if (auto database_pool = weak_database_pool.lock()) {
			for (const auto& endpoint : endpoints) {
				database_pool->add_endpoint_queue(endpoint, shared_from_this());
			}
		}
	}

	if (max == -1 || count < static_cast<size_t>(max)) {
		++count;
		return true;
	}

	return false;
}


bool
DatabaseQueue::dec_count()
{
	L_CALL("DatabaseQueue::dec_count()");

	std::lock_guard<std::mutex> lk(_state->_mutex);

	if (count <= 0) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	if (count > 0) {
		--count;
		return true;
	}

	if (auto database_pool = weak_database_pool.lock()) {
		for (const auto& endpoint : endpoints) {
			database_pool->drop_endpoint_queue(endpoint, shared_from_this());
		}
	}

	return false;
}


/*  ____        _        _                    _     ____  _   _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| |   |  _ \| | | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |   | |_) | | | |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___|  _ <| |_| |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| \_\\___/
 *
 */


DatabasesLRU::DatabasesLRU(size_t dbpool_size, std::shared_ptr<queue::QueueState> queue_state)
	: LRU(dbpool_size),
	  _queue_state(std::move(queue_state)) { }


std::shared_ptr<DatabaseQueue>
DatabasesLRU::get(size_t hash)
{
	auto it = find(hash);
	if (it != end()) {
		return it->second;
	}
	return nullptr;
}


std::shared_ptr<DatabaseQueue>
DatabasesLRU::get(size_t hash, bool db_volatile)
{
	const auto now = std::chrono::system_clock::now();

	const auto on_get = [now](std::shared_ptr<DatabaseQueue>& val) {
		val->renew_time = now;
		return lru::GetAction::renew;
	};

	auto it = find_and(on_get, hash);
	if (it != end()) {
		return it->second;
	}

	const auto on_drop = [now](std::shared_ptr<DatabaseQueue>& val, ssize_t size, ssize_t max_size) {
		if (val->persistent ||
			val->size() < val->count ||
			val->state != DatabaseQueue::replica_state::REPLICA_FREE) {
			val->renew_time = now;
			return lru::DropAction::renew;
		}
		if (size > max_size) {
			if (val->renew_time < now - 500ms) {
				return lru::DropAction::evict;
			}
			return lru::DropAction::leave;
		}
		if (val->renew_time < now - 60s) {
			return lru::DropAction::evict;
		}
		return lru::DropAction::stop;
	};

	if (db_volatile) {
		// Volatile, insert default on the back
		return emplace_back_and(on_drop, hash, DatabaseQueue::make_shared(_queue_state)).first->second;
	}

	// Non-volatile, insert default on the front
	return emplace_and(on_drop, hash, DatabaseQueue::make_shared(_queue_state)).first->second;
}


void
DatabasesLRU::cleanup()
{
	const auto now = std::chrono::system_clock::now();
	const auto on_drop = [now](std::shared_ptr<DatabaseQueue>& val, ssize_t size, ssize_t max_size) {
		if (val->persistent ||
			val->size() < val->count ||
			val->state != DatabaseQueue::replica_state::REPLICA_FREE) {
			return lru::DropAction::leave;
		}
		if (size > max_size) {
			if (val->renew_time < now - 500ms) {
				return lru::DropAction::evict;
			}
			return lru::DropAction::leave;
		}
		if (val->renew_time < now - 60s) {
			return lru::DropAction::evict;
		}
		return lru::DropAction::stop;
	};
	trim(on_drop);
}


void
DatabasesLRU::finish()
{
	L_CALL("DatabasesLRU::finish()");

	for (auto& queue : *this) {
		queue.second->finish();
	}
}


/*  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */


DatabasePool::DatabasePool(size_t dbpool_size, size_t max_databases)
	: finished(false),
	  queue_state(std::make_shared<queue::QueueState>(-1, max_databases, -1)),
	  databases(dbpool_size, queue_state),
	  writable_databases(dbpool_size, queue_state)
{
	L_OBJ("CREATED DATABASE POLL!");
}


DatabasePool::~DatabasePool()
{
	finish();

	L_OBJ("DELETED DATABASE POOL!");
}


void
DatabasePool::add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	L_CALL("DatabasePool::add_endpoint_queue(%s, <queue>)", repr(endpoint.to_string()));

	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.insert(queue);
}


void
DatabasePool::drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	L_CALL("DatabasePool::drop_endpoint_queue(%s, <queue>)", repr(endpoint.to_string()));

	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.erase(queue);

	if (queues_set.empty()) {
		queues.erase(hash);
	}
}


void
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags)
{
	L_CALL("DatabasePool::checkout(%s, 0x%02x (%s))", repr(endpoints.to_string()), flags, [&flags]() {
		std::vector<std::string> values;
		if (flags == DB_OPEN) values.push_back("DB_OPEN");
		if ((flags & DB_WRITABLE) == DB_WRITABLE) values.push_back("DB_WRITABLE");
		if ((flags & DB_SPAWN) == DB_SPAWN) values.push_back("DB_SPAWN");
		if ((flags & DB_PERSISTENT) == DB_PERSISTENT) values.push_back("DB_PERSISTENT");
		if ((flags & DB_INIT_REF) == DB_INIT_REF) values.push_back("DB_INIT_REF");
		if ((flags & DB_VOLATILE) == DB_VOLATILE) values.push_back("DB_VOLATILE");
		if ((flags & DB_REPLICATION) == DB_REPLICATION) values.push_back("DB_REPLICATION");
		if ((flags & DB_NOWAL) == DB_NOWAL) values.push_back("DB_NOWAL");
		if ((flags & DB_NOSTORAGE) == DB_NOSTORAGE) values.push_back("DB_NOSTORAGE");
		return string::join(values, " | ");
	}());

	bool db_writable = (flags & DB_WRITABLE) != 0;
	bool db_commit = (flags & DB_COMMIT) == DB_COMMIT;
	bool db_persistent = (flags & DB_PERSISTENT) != 0;
	bool db_init_ref = (flags & DB_INIT_REF) != 0;
	bool db_replication = (flags & DB_REPLICATION) != 0;
	bool db_volatile = (flags & DB_VOLATILE) != 0;

	L_DATABASE_BEGIN("++ CHECKING OUT DB [%s]: %s ...", db_writable ? "WR" : "RO", repr(endpoints.to_string()));

	assert(!database);

	if (db_writable && endpoints.size() != 1) {
		L_ERR("ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout database: %s (only one)", repr(endpoints.to_string()));
	}

	if (!finished) {
		size_t hash = endpoints.hash();

		std::unique_lock<std::mutex> lk(qmtx);

		std::shared_ptr<DatabaseQueue> queue;
		if (db_writable) {
			queue = writable_databases.get(hash, db_volatile);
			databases.cleanup();
			if (db_commit && !queue->modified) {
				L_DATABASE_END("!! ABORTED CHECKOUT DB COMMIT NOT NEEDED [%s]: %s", db_writable ? "WR" : "RO", repr(endpoints.to_string()));
				THROW(CheckoutErrorCommited, "Cannot checkout database: %s (commit)", repr(endpoints.to_string()));
			}
		} else {
			queue = databases.get(hash, db_volatile);
			writable_databases.cleanup();
		}

		auto old_state = queue->state;

		if (db_replication) {
			switch (queue->state) {
				case DatabaseQueue::replica_state::REPLICA_FREE:
					queue->state = DatabaseQueue::replica_state::REPLICA_LOCK;
					break;
				case DatabaseQueue::replica_state::REPLICA_LOCK:
				case DatabaseQueue::replica_state::REPLICA_SWITCH:
					L_REPLICATION("A replication task is already waiting");
					L_DATABASE_END("!! ABORTED CHECKOUT DB [%s]: %s", db_writable ? "WR" : "RO", repr(endpoints.to_string()));
					THROW(CheckoutErrorReplicating, "Cannot checkout database: %s (aborted)", repr(endpoints.to_string()));
			}
		} else {
			if (queue->state == DatabaseQueue::replica_state::REPLICA_SWITCH) {
				queue->switch_cond.wait(lk);
			}
		}

		bool old_persistent = queue->persistent;
		queue->persistent = db_persistent;

		if (!queue->pop(database, 0)) {
			// Increment so other threads don't delete the queue
			if (queue->inc_count(db_writable ? 1 : -1)) {
#ifdef XAPIAND_DATABASE_WAL
				size_t count = queue->count;
#endif
				lk.unlock();
				try {
					database = std::make_shared<Database>(queue, endpoints, flags);

					if (db_writable && db_init_ref) {
						DatabaseHandler::init_ref(endpoints[0]);
					}

#ifdef XAPIAND_DATABASE_WAL
					if (!db_writable && count == 1 && ((flags & DB_NOWAL) == 0)) {
						bool reopen = false;
						for (const auto& endpoint : database->endpoints) {
							if (endpoint.is_local()) {
								std::shared_ptr<Database> d;
								try {
									// Checkout executes any commands from the WAL
									checkout(d, Endpoints(endpoint), DB_WRITABLE);
									reopen = true;
									checkin(d);
								} catch (const CheckoutError&) {
								} catch (...) {
									database.reset();
									reopen = false;
									break;
								}
							}
						}
						if (reopen) {
							database->reopen();
						}
					}
#endif
				} catch (const Xapian::DatabaseOpeningError& exc) {
					L_DATABASE("ERROR: %s", exc.get_description());
				} catch (const Xapian::Error& exc) {
					L_EXC("ERROR: %s", exc.get_description());
				}
				lk.lock();
				queue->dec_count();  // Decrement, count should have been already incremented if Database was created
			} else {
				// Lock until a database is available if it can't get one.
				lk.unlock();
				auto s = static_cast<int>(queue->pop(database, DB_TIMEOUT));
				if (s == 0) {
					THROW(TimeOutError, "Database is not available");
				}
				lk.lock();
			}
		}
		if (!database || !database->db) {
			queue->state = old_state;
			queue->persistent = old_persistent;
			if (queue->count == 0) {
				//L_DEBUG("There is a error, the queue ended up being empty, remove it");
				if (db_writable) {
					writable_databases.erase(hash);
				} else {
					databases.erase(hash);
				}
			}
			database.reset();
		}
	}

	if (!database) {
		L_DATABASE_END("!! FAILED CHECKOUT DB [%s]: %s", db_writable ? "WR" : "WR", repr(endpoints.to_string()));
		THROW(CheckoutError, "Cannot checkout database: %s", repr(endpoints.to_string()));
	}

	// Reopening of old/outdated databases:
	if (!db_writable) {
		bool reopen = false;
		auto reopen_age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - database->reopen_time).count();
		if (reopen_age >= LOCAL_DATABASE_UPDATE_TIME) {
			// Database is just too old, reopen
			reopen = true;
		} else {
			for (size_t i = 0; i < database->dbs.size(); ++i) {
				const auto& db_pair = database->dbs[i];
				auto hash = endpoints[i].hash();
				std::lock_guard<std::mutex> lk(qmtx);
				if (db_pair.second) {
					// Local database:
					auto queue = writable_databases.get(hash);
					if (queue) {
						auto revision = queue->revision.load();
						if (revision != db_pair.first.get_revision()) {
							// Local writable database has changed revision.
							reopen = true;
							break;
						}
					}
				} else {
					// Remote database:
					if (reopen_age >= REMOTE_DATABASE_UPDATE_TIME) {
						// Remote database is too old, reopen.
						reopen = true;
						break;
					}
				}
			}
		}
		if (reopen) {
			database->reopen();
			L_DATABASE("== REOPEN DB [%s]: %s", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(database->endpoints.to_string()));
		}
	}

	L_DATABASE_END("++ CHECKED OUT DB [%s]: %s (rev:%u)", db_writable ? "WR" : "WR", repr(endpoints.to_string()), database->reopen_revision);
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::checkin(%s)", repr(database->to_string()));

	L_DATABASE_BEGIN("-- CHECKING IN DB [%s]: %s ...", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(database->endpoints.to_string()));

	assert(database);

	std::shared_ptr<DatabaseQueue> queue;

	std::unique_lock<std::mutex> lk(qmtx);

	if ((database->flags & DB_WRITABLE) != 0) {
		auto& endpoint = database->endpoints[0];
		if (endpoint.is_local()) {
			auto new_revision = database->get_revision();
			if (database->reopen_revision != new_revision) {
				database->reopen_revision = new_revision;
				if (database->mastery_level != -1) {
					endpoint.mastery_level = database->mastery_level;
					updated_databases.push(endpoint);
				}
			}
		}
		queue = writable_databases.get(database->hash, false);
	} else {
		queue = databases.get(database->hash, false);
	}

	assert(database->weak_queue.lock() == queue);

	if (queue->modified) {
		DatabaseAutocommit::commit(database);
	}

	if (!queue->push(database)) {
		writable_databases.cleanup();
		databases.cleanup();
		queue->push(database);
	}

	auto& endpoints = database->endpoints;
	bool signal_checkins = false;
	switch (queue->state) {
		case DatabaseQueue::replica_state::REPLICA_SWITCH:
			for (const auto& endpoint : endpoints) {
				_switch_db(endpoint);
			}
			if (queue->state == DatabaseQueue::replica_state::REPLICA_FREE) {
				signal_checkins = true;
			}
			break;
		case DatabaseQueue::replica_state::REPLICA_LOCK:
			queue->state = DatabaseQueue::replica_state::REPLICA_FREE;
			signal_checkins = true;
			break;
		case DatabaseQueue::replica_state::REPLICA_FREE:
			break;
	}

	if (queue->count < queue->size()) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	L_DATABASE_END("-- CHECKED IN DB [%s]: %s", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(endpoints.to_string()));

	database.reset();

	lk.unlock();
}


void
DatabasePool::finish()
{
	L_CALL("DatabasePool::finish()");

	finished = true;

	writable_databases.finish();
	databases.finish();

	L_OBJ("FINISH DATABASE!");
}


bool
DatabasePool::_switch_db(const Endpoint& endpoint)
{
	L_CALL("DatabasePool::_switch_db(%s)", repr(endpoint.to_string()));

	auto& queues_set = queues[endpoint.hash()];

	bool switched = true;
	for (auto& queue : queues_set) {
		queue->state = DatabaseQueue::replica_state::REPLICA_SWITCH;
		if (queue->count != queue->size()) {
			switched = false;
			break;
		}
	}

	if (switched) {
		move_files(endpoint.path + "/.tmp", endpoint.path);

		for (auto& queue : queues_set) {
			queue->state = DatabaseQueue::replica_state::REPLICA_FREE;
			queue->switch_cond.notify_all();
		}
	} else {
		L_CRIT("Inside switch_db, but not all queues have (queue->count == queue->size())");
	}

	return switched;
}


bool
DatabasePool::switch_db(const Endpoint& endpoint)
{
	L_CALL("DatabasePool::switch_db(%s)", repr(endpoint.to_string()));

	std::lock_guard<std::mutex> lk(qmtx);
	return _switch_db(endpoint);
}


std::pair<size_t, size_t>
DatabasePool::total_writable_databases()
{
	L_CALL("DatabasePool::total_wdatabases()");

	size_t db_count = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & writable_database : writable_databases) {
		db_count += writable_database.second->size();
	}
	return std::make_pair(writable_databases.size(), db_count);
}


std::pair<size_t, size_t>
DatabasePool::total_readable_databases()
{
	L_CALL("DatabasePool::total_rdatabases()");

	size_t db_count = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & database : databases) {
		db_count += database.second->size();
	}
	return std::make_pair(databases.size(), db_count);
}
