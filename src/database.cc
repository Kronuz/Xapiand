/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "database.h"

#include <algorithm>              // for move
#include <array>                  // for array
#include <dirent.h>               // for closedir, DIR
#include <iterator>               // for distance
#include <limits>                 // for numeric_limits
#include <ratio>                  // for ratio
#include <strings.h>              // for strncasecmp
#include <sys/errno.h>            // for __error, errno
#include <sys/fcntl.h>            // for O_CREAT, O_WRONLY, O_EXCL
#include <sysexits.h>             // for EX_SOFTWARE

#include "atomic_shared_ptr.h"    // for atomic_shared_ptr
#include "database_autocommit.h"  // for DatabaseAutocommit
#include "database_handler.h"     // for DatabaseHandler
#include "exception.h"            // for Error, MSG_Error, Exception, DocNot...
#include "io_utils.h"             // for close, strerrno, write, open
#include "length.h"               // for serialise_length, unserialise_length
#include "log.h"                  // for L_OBJ, L_CALL
#include "manager.h"              // for sig_exit
#include "msgpack.h"              // for MsgPack
#include "msgpack/unpack.hpp"     // for unpack_error
#include "schema.h"               // for FieldType, FieldType::TERM
#include "serialise.h"            // for uuid
#include "utils.h"                // for repr, to_string, File_ptr, find_fil...


#define XAPIAN_LOCAL_DB_FALLBACK 1

#define DATABASE_UPDATE_TIME 10

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
	const DatabaseWAL* wal = static_cast<const DatabaseWAL*>(param);
	bool commit_eof = static_cast<bool>(args);

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
WalHeader::validate(void* param, void*)
{
	if (head.magic != MAGIC) {
		THROW(StorageCorruptVolume, "Bad WAL header magic number");
	}

	const DatabaseWAL* wal = static_cast<const DatabaseWAL*>(param);
	if (wal->validate_uuid && strncasecmp(head.uuid, wal->database->get_uuid().c_str(), sizeof(head.uuid))) {
		THROW(StorageCorruptVolume, "WAL UUID mismatch");
	}
}


DatabaseWAL::DatabaseWAL(const std::string& base_path_, Database* database_)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(base_path_, this),
	  modified(false),
	  validate_uuid(true),
	  database(database_)
{
	L_OBJ(this, "CREATED DATABASE WAL!");
}


DatabaseWAL::~DatabaseWAL()
{
	L_OBJ(this, "DELETED DATABASE WAL!");
}


bool
DatabaseWAL::open_current(bool commited)
{
	L_CALL(this, "DatabaseWAL::open_current(%s)", commited ? "true" : "false");

	uint32_t revision = database->checkout_revision;

	DIR *dir = opendir(base_path.c_str(), true);
	if (!dir) {
		THROW(Error, "Could not open the dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;
	uint32_t lowest_revision = std::numeric_limits<uint32_t>::max();

	File_ptr fptr;
	find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);

	while (fptr.ent) {
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
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name).c_str(), strerror(errno));
		} catch (const std::out_of_range&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name).c_str(), strerror(errno));
		}

		find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);
	}

	closedir(dir);
	if (lowest_revision > revision) {
		open(WAL_STORAGE_PATH + std::to_string(revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
	} else {
		modified = false;

		bool reach_end = false;
		uint32_t start_off, end_off;
		uint32_t file_rev, begin_rev, end_rev;
		for (auto slot = lowest_revision; slot <= highest_revision && not reach_end; ++slot) {
			file_rev = begin_rev = slot;
			open(WAL_STORAGE_PATH + std::to_string(slot), STORAGE_OPEN);

			uint32_t high_slot = highest_valid_slot();
			if (high_slot == static_cast<uint32_t>(-1)) {
				continue;
			}

			if (slot == highest_revision) {
				reach_end = true; /* Avoid reenter to the loop with the high valid slot of the highest revision */
				if (!commited) {
					/* last slot contain offset at the end of file */
					/* In case not "commited" not execute the high slot avaible because are operations without commit */
					--high_slot;
				}
			}

			if (slot == lowest_revision) {
				slot = revision - header.head.revision - 1;
				if (slot == static_cast<uint32_t>(-1)) {
					/* The offset saved in slot 0 is the beginning of the revision 1 to reach 2
					 * for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
					 */
					start_off = STORAGE_START_BLOCK_OFFSET;
					begin_rev = 0;
				} else {
					start_off = header.slot[slot];
					if (start_off == 0) {
						THROW(StorageCorruptVolume, "Bad offset");
					}
					begin_rev = slot;
				}
			} else {
				start_off = STORAGE_START_BLOCK_OFFSET;
			}

			seek(start_off);

			end_off =  header.slot[high_slot];

			if (start_off < end_off) {
				end_rev =  header.head.revision + high_slot;
				L_INFO(nullptr, "Read and execute operations WAL file [wal.%u] from (%u..%u) revision", file_rev, begin_rev, end_rev);
			}

			try {
				while (true) {
					std::string line = read(end_off);
					if (!execute(line)) {
						THROW(Error, "WAL revision mismatch!");
					}
				}
			} catch (const StorageEOF& exc) { }

			slot = high_slot;
		}

		open(WAL_STORAGE_PATH + std::to_string(highest_revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
	}
	return modified;
}

MsgPack
DatabaseWAL::repr_line(const std::string& line)
{
	L_CALL(this, "DatabaseWAL::repr_line(<line>)");

	const char *p = line.data();
	const char *p_end = p + line.size();

	MsgPack repr;

	repr["revision"] = unserialise_length(&p, p_end);

	Type type = static_cast<Type>(unserialise_length(&p, p_end));

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
DatabaseWAL::repr(uint32_t start_revision, uint32_t end_revision)
{
	L_CALL(this, "DatabaseWAL::repr(%u, %u)", start_revision, end_revision);

	fprintf(stderr, "%u\n", database->checkout_revision);

	MsgPack repr(MsgPack::Type::ARRAY);

	DIR *dir = opendir(base_path.c_str(), true);
	if (!dir) {
		THROW(Error, "Could not open the dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;
	uint32_t lowest_revision = std::numeric_limits<uint32_t>::max();

	File_ptr fptr;
	find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);

	while (fptr.ent) {
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
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name).c_str(), strerror(errno));
		} catch (const std::out_of_range&) {
			THROW(Error, "In wal file %s (%s)", std::string(fptr.ent->d_name).c_str(), strerror(errno));
		}

		find_file_dir(dir, fptr, WAL_STORAGE_PATH, true);
	}

	closedir(dir);
	if (lowest_revision > start_revision) {
		open(WAL_STORAGE_PATH + std::to_string(start_revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
	} else {
		bool reach_end = false;
		uint32_t start_off, end_off;
		uint32_t file_rev, begin_rev, end_rev;
		for (auto slot = lowest_revision; slot <= highest_revision && not reach_end; ++slot) {
			file_rev = begin_rev = slot;
			open(WAL_STORAGE_PATH + std::to_string(slot), STORAGE_OPEN);

			uint32_t high_slot = highest_valid_slot();
			if (high_slot == static_cast<uint32_t>(-1)) {
				continue;
			}

			if (slot == highest_revision) {
				reach_end = true; /* Avoid reenter to the loop with the high valid slot of the highest revision */
			}

			if (slot == lowest_revision) {
				slot = start_revision - header.head.revision - 1;
				if (slot == static_cast<uint32_t>(-1)) {
					/* The offset saved in slot 0 is the beginning of the revision 1 to reach 2
					 * for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
					 */
					start_off = STORAGE_START_BLOCK_OFFSET;
					begin_rev = 0;
				} else {
					start_off = header.slot[slot];
					if (start_off == 0) {
						THROW(StorageCorruptVolume, "Bad offset");
					}
					begin_rev = slot;
				}
			} else {
				start_off = STORAGE_START_BLOCK_OFFSET;
			}

			seek(start_off);

			end_off =  header.slot[high_slot];

			if (start_off < end_off) {
				end_rev =  header.head.revision + high_slot;
				L_INFO(nullptr, "Read and repr operations WAL file [wal.%u] from (%u..%u) revision", file_rev, begin_rev, end_rev);
			}

			try {
				while (true) {
					std::string line = read(end_off);
					repr.push_back(repr_line(line));
				}
			} catch (const StorageEOF& exc) { }

			slot = high_slot;
		}

		open(WAL_STORAGE_PATH + std::to_string(highest_revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
	}

	return repr;
}


uint32_t
DatabaseWAL::highest_valid_slot()
{
	L_CALL(this, "DatabaseWAL::highest_valid_slot()");

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
DatabaseWAL::execute(const std::string& line)
{
	L_CALL(this, "DatabaseWAL::execute(<line>)");

	const char *p = line.data();
	const char *p_end = p + line.size();

	if (!(database->flags & DB_WRITABLE)) {
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

	Type type = static_cast<Type>(unserialise_length(&p, p_end));

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
	L_CALL(this, "DatabaseWAL::init_database()");

	static const std::array<std::string, 2> iamglass({{
		std::to_string("\x0f\x0d\x58\x61\x70\x69\x61\x6e\x20\x47\x6c\x61\x73\x73\x04\x6e"),
		std::to_string("\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00\x03\x00"
			"\x04\x04\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00"
			"\x03\x00\x04\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00")
	}});

	auto filename = base_path + "iamglass";
	if (exists(filename)) {
		return true;
	}

	validate_uuid = false;

	try {
		open(std::string(WAL_STORAGE_PATH) + "0", STORAGE_OPEN | STORAGE_COMPRESS);
	} catch (const StorageIOError&) {
		return true;
	}

	std::string uuid_str(header.head.uuid, UUID_LENGTH);
	if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' || uuid_str[23] != '-') {
		THROW(SerialisationError, "Invalid UUID format in: %s", uuid_str.c_str());
	}
	for (size_t i = 0; i < UUID_LENGTH; ++i) {
		if (!std::isxdigit(uuid_str.at(i)) && i != 8 && i != 13 && i != 18 && i != 23) {
			THROW(SerialisationError, "Invalid UUID format in: %s", uuid_str.c_str());
		}
	}

	Guid guid(uuid_str);
	const auto& bytes = guid.get_bytes();
	std::string uuid;
	uuid.reserve(bytes.size());
	for (const char& c : bytes) {
		uuid.push_back(c);
	}

	int fd = io::open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL);
	if (unlikely(fd < 0)) {
		L_ERR(nullptr, "ERROR: opening file. %s\n", filename.c_str());
		return false;
	}
	if unlikely(io::write(fd, iamglass[0].data(), iamglass[0].size()) < 0) {
		L_ERRNO(nullptr, "io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, uuid.data(), uuid.size()) < 0) {
		L_ERRNO(nullptr, "io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, iamglass[1].data(), iamglass[1].size()) < 0) {
		L_ERRNO(nullptr, "io::write() -> %s", io::strerrno(errno));
		io::close(fd);
		return false;
	}
	io::close(fd);

	filename = base_path + "postlist.glass";
	fd = io::open(filename.c_str(), O_WRONLY | O_CREAT);
	if (unlikely(fd < 0)) {
		L_ERR(nullptr, "ERROR: opening file. %s\n", filename.c_str());
		return false;
	}
	io::close(fd);

	return true;
}


void
DatabaseWAL::write_line(Type type, const std::string& data, bool commit_)
{
	L_CALL(this, "DatabaseWAL::write_line(...)");

	ASSERT(database->flags & DB_WRITABLE);
	ASSERT(!(database->flags & DB_NOWAL));

	auto endpoint = database->endpoints[0];
	ASSERT(endpoint.is_local());

	std::string revision_encode = database->get_revision_str();
	std::string uuid = database->get_uuid();
	std::string line(revision_encode + serialise_length(toUType(type)) + data);

	L_DATABASE_WAL(this, "%s on %s: '%s'", names[toUType(type)], endpoint.path.c_str(), repr(line, quote).c_str());

	uint32_t rev = database->get_revision();

	uint32_t slot = rev - header.head.revision;

	if (slot >= WAL_SLOTS) {
		close();
		open(WAL_STORAGE_PATH + std::to_string(rev), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE);
		slot = rev - header.head.revision;
	}

	write(line.data(), line.size());
	header.slot[slot] = header.head.offset; /* Beginning of the next revision */

	if (commit_) {
		if (slot + 1 >= WAL_SLOTS) {
			close();
			open(WAL_STORAGE_PATH + std::to_string(rev + 1), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | WAL_SYNC_MODE, true);
		} else {
			header.slot[slot + 1] = header.slot[slot];
		}
	}

	commit();
}


void
DatabaseWAL::write_add_document(const Xapian::Document& doc)
{
	L_CALL(this, "DatabaseWAL::write_add_document(<doc>)");

	write_line(Type::ADD_DOCUMENT, doc.serialise());
}


void
DatabaseWAL::write_cancel()
{
	L_CALL(this, "DatabaseWAL::write_cancel()");

	write_line(Type::CANCEL, "");
}


void
DatabaseWAL::write_delete_document_term(const std::string& term)
{
	L_CALL(this, "DatabaseWAL::write_delete_document_term(<term>)");

	write_line(Type::DELETE_DOCUMENT_TERM, serialise_length(term.size()) + term);
}


void
DatabaseWAL::write_commit()
{
	L_CALL(this, "DatabaseWAL::write_commit()");

	write_line(Type::COMMIT, "", true);
}


void
DatabaseWAL::write_replace_document(Xapian::docid did, const Xapian::Document& doc)
{
	L_CALL(this, "DatabaseWAL::write_replace_document(...)");

	write_line(Type::REPLACE_DOCUMENT, serialise_length(did) + doc.serialise());
}


void
DatabaseWAL::write_replace_document_term(const std::string& term, const Xapian::Document& doc)
{
	L_CALL(this, "DatabaseWAL::write_replace_document_term(...)");

	write_line(Type::REPLACE_DOCUMENT_TERM, serialise_length(term.size()) + term + doc.serialise());
}


void
DatabaseWAL::write_delete_document(Xapian::docid did)
{
	L_CALL(this, "DatabaseWAL::write_delete_document(<did>)");

	write_line(Type::DELETE_DOCUMENT, serialise_length(did));
}


void
DatabaseWAL::write_set_metadata(const std::string& key, const std::string& val)
{
	L_CALL(this, "DatabaseWAL::write_set_metadata(...)");

	write_line(Type::SET_METADATA, serialise_length(key.size()) + key + val);
}


void
DatabaseWAL::write_add_spelling(const std::string& word, Xapian::termcount freqinc)
{
	L_CALL(this, "DatabaseWAL::write_add_spelling(...)");

	write_line(Type::ADD_SPELLING, serialise_length(freqinc) + word);
}


void
DatabaseWAL::write_remove_spelling(const std::string& word, Xapian::termcount freqdec)
{
	L_CALL(this, "DatabaseWAL::write_remove_spelling(...)");

	write_line(Type::REMOVE_SPELLING, serialise_length(freqdec) + word);
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
DataHeader::init(void* param, void*)
{
	const Database* database = static_cast<const Database*>(param);

	head.offset = STORAGE_START_BLOCK_OFFSET;
	head.magic = STORAGE_MAGIC;
	strncpy(head.uuid, database->get_uuid().c_str(), sizeof(head.uuid));
}


void
DataHeader::validate(void* param, void*)
{
	if (head.magic != STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	const Database* database = static_cast<const Database*>(param);
	if (strncasecmp(head.uuid, database->get_uuid().c_str(), sizeof(head.uuid))) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


DataStorage::DataStorage(const std::string& base_path_, void* param_)
	: Storage<DataHeader, DataBinHeader, DataBinFooter>(base_path_, param_)
{
	L_OBJ(this, "CREATED DATABASE DATA STORAGE!");
}


DataStorage::~DataStorage()
{
	L_OBJ(this, "DELETED DATABASE DATA STORAGE!");
}


uint32_t
DataStorage::highest_volume()
{
	L_CALL(this, "DataStorage::highest_volume()");

	DIR *dir = opendir(base_path.c_str(), true);
	if (!dir) {
		THROW(Error, "Could not open dir (%s)", strerror(errno));
	}

	uint32_t highest_revision = 0;

	File_ptr fptr;
	find_file_dir(dir, fptr, DATA_STORAGE_PATH, true);

	while (fptr.ent) {
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


Database::Database(std::shared_ptr<DatabaseQueue>& queue_, const Endpoints& endpoints_, int flags_)
	: weak_queue(queue_),
	  endpoints(endpoints_),
	  flags(flags_),
	  hash(endpoints.hash()),
	  access_time(std::chrono::system_clock::now()),
	  mastery_level(-1),
	  checkout_revision(0)
{
	reopen();

	if (auto queue = weak_queue.lock()) {
		queue->inc_count();
	}

	L_OBJ(this, "CREATED DATABASE!");
}


Database::~Database()
{
	if (auto queue = weak_queue.lock()) {
		queue->dec_count();
	}

	L_OBJ(this, "DELETED DATABASE!");
}


long long
Database::read_mastery(const Endpoint& endpoint)
{
	L_CALL(this, "Database::read_mastery(%s)", repr(endpoint.to_string()).c_str());

	if (mastery_level != -1) return mastery_level;
	if (!endpoint.is_local()) return -1;

	mastery_level = ::read_mastery(endpoint.path, true);

	return mastery_level;
}


bool
Database::reopen()
{
	L_CALL(this, "Database::reopen()");

	access_time = std::chrono::system_clock::now();

	if (db) {
		// Try to reopen
		try {
			bool ret = db->reopen();
			L_DATABASE_WRAP(this, "Reopen done (took %s) [1]", delta_string(access_time, std::chrono::system_clock::now()).c_str());
			return ret;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
		}

		db->close();
		db.reset();
	}

#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif /* XAPIAND_DATA_STORAGE */

	auto endpoints_size = endpoints.size();
	auto i = endpoints.cbegin();
	if (flags & DB_WRITABLE) {
		ASSERT(endpoints_size == 1);
		db = std::make_unique<Xapian::WritableDatabase>();
		const auto& e = *i;
		Xapian::WritableDatabase wdb;
		bool local = false;
		int _flags = (flags & DB_SPAWN) ? Xapian::DB_CREATE_OR_OPEN | XAPIAN_SYNC_MODE : Xapian::DB_OPEN | XAPIAN_SYNC_MODE;
#ifdef XAPIAND_CLUSTERING
		if (!e.is_local()) {
			// Writable remote databases do not have a local fallback
			int port = (e.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : e.port;
			wdb = Xapian::Remote::open_writable(e.host, port, 0, 10000, _flags, e.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
			try {
				Xapian::Database tmp = Xapian::Database(e.path, Xapian::DB_OPEN);
				if (tmp.get_uuid() == wdb.get_uuid()) {
					L_DATABASE(this, "Endpoint %s fallback to local database!", repr(e.to_string()).c_str());
					// Handle remote endpoints and figure out if the endpoint is a local database
					build_path_index(e.path);
					wdb = Xapian::WritableDatabase(e.path, _flags);
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
			{
				DatabaseWAL tmp_wal(e.path, this);
				tmp_wal.init_database();
			}
#endif
			build_path_index(e.path);
			wdb = Xapian::WritableDatabase(e.path, _flags);
			local = true;
			if (endpoints_size == 1) read_mastery(e);
		}

		db->add_database(wdb);

		if (local) {
			checkout_revision = get_revision();
		}

#ifdef XAPIAND_DATA_STORAGE
		if (local) {
			if (flags & DB_NOSTORAGE) {
				writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
				storages.push_back(std::make_unique<DataStorage>(e.path, this));
			} else {
				auto storage = std::make_unique<DataStorage>(e.path, this);
				storage->volume = storage->highest_volume();

				storage->open(DATA_STORAGE_PATH + std::to_string(storage->volume), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE);
				writable_storages.push_back(std::unique_ptr<DataStorage>(storage.release()));
				storages.push_back(std::make_unique<DataStorage>(e.path, this));
			}
		} else {
			writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
			storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		}
#endif /* XAPIAND_DATA_STORAGE */

#ifdef XAPIAND_DATABASE_WAL
		/* If checkout_revision is not available Wal work as a log for the operations */
		if (local && !(flags & DB_NOWAL)) {
			// WAL required on a local writable database, open it.
			wal = std::make_unique<DatabaseWAL>(e.path, this);
			if (wal->open_current(true)) {
				if (auto queue = weak_queue.lock()) {
					queue->modified = true;
				}
			}
		}
#endif /* XAPIAND_DATABASE_WAL */
	} else {
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
						L_DATABASE(this, "Endpoint %s fallback to local database!", repr(e.to_string()).c_str());
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
				{
					DatabaseWAL tmp_wal(e.path, this);
					tmp_wal.init_database();
				}
#endif
				try {
					rdb = Xapian::Database(e.path, Xapian::DB_OPEN);
					local = true;
					if (endpoints_size == 1) read_mastery(e);
				} catch (const Xapian::DatabaseOpeningError& exc) {
					if (!(flags & DB_SPAWN))  {
						if (endpoints.size() == 1) {
							db.reset();
							throw;
						} else {
							--size_endp;
							continue;
						}
					}
					{
						build_path_index(e.path);
						Xapian::WritableDatabase tmp(e.path, Xapian::DB_CREATE_OR_OPEN);
					}

					rdb = Xapian::Database(e.path, Xapian::DB_OPEN);
					local = true;
					if (endpoints_size == 1) read_mastery(e);
				}
			}

			db->add_database(rdb);

	#ifdef XAPIAND_DATA_STORAGE
			if (local && endpoints_size == 1) {
				// WAL required on a local database, open it.
				storages.push_back(std::make_unique<DataStorage>(e.path, this));
			} else {
				storages.push_back(std::unique_ptr<DataStorage>(nullptr));
			}
	#endif /* XAPIAND_DATA_STORAGE */
		}

		if (!size_endp) {
			throw Xapian::DatabaseOpeningError("Empty set of databases");
		}
	}

	L_DATABASE_WRAP(this, "Reopen done (took %s) [1]", delta_string(access_time, std::chrono::system_clock::now()).c_str());

	return true;
}


std::string
Database::get_uuid() const
{
	L_CALL(this, "Database::get_uuid");

	return db->get_uuid();
}


uint32_t
Database::get_revision() const
{
	L_CALL(this, "Database::get_revision()");

#if HAVE_XAPIAN_DATABASE_GET_REVISION
	return db->get_revision();
#else
	return 0;
#endif
}


std::string
Database::get_revision_str() const
{
	L_CALL(this, "Database::get_revision_str()");

	return serialise_length(get_revision());
}


bool
Database::commit(bool wal_)
{
	L_CALL(this, "Database::commit(%s)", wal_ ? "true" : "false");

	auto queue = weak_queue.lock();
	if (queue && !queue->modified) {
		L_DATABASE_WRAP(this, "Do not commit, because there are not changes");
		return false;
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_commit();
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Commit: t: %d", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif /* XAPIAND_DATA_STORAGE */
			wdb->commit();
			if (queue) {
				queue->modified = false;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::DatabaseError& exc) {
			const char* error = exc.get_error_string();
			if (error) {
				L_WARNING(this, "ERROR: %s (%s)", exc.get_msg().c_str(), error);
			} else {
				L_WARNING(this, "ERROR: %s", exc.get_msg().c_str());
			}
			throw;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Commit made (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	return true;
}


void
Database::cancel(bool wal_)
{
	L_CALL(this, "Database::cancel(%s)", wal_ ? "true" : "false");

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_cancel();
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Cancel: t: %d", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->begin_transaction(false);
			wdb->cancel_transaction();
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Cancel made (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());
}


void
Database::delete_document(Xapian::docid did, bool commit_, bool wal_)
{
	L_CALL(this, "Database::delete_document(%d, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) {
		wal->write_delete_document(did);
	}
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Deleting document: %d  t: %d", did, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(did);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document deleted (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}
}


void
Database::delete_document_term(const std::string& term, bool commit_, bool wal_)
{
	L_CALL(this, "Database::delete_document_term(%s, %s, %s)", repr(term).c_str(), commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!(flags & DB_WRITABLE)) {
		THROW(Error, "database is read-only");
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_delete_document_term(term);
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Deleting document: '%s'  t: %d", term.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(term);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document deleted (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Database::storage_get(const std::unique_ptr<DataStorage>& storage, const std::string& store) const
{
	L_CALL(this, "Database::storage_get()");

	auto locator = storage_unserialise_locator(store);

	storage->open(DATA_STORAGE_PATH + std::to_string(std::get<0>(locator)), STORAGE_OPEN);
	storage->seek(static_cast<uint32_t>(std::get<1>(locator)));

	return storage->read();
}


std::string
Database::storage_get_blob(const Xapian::Document& doc) const
{
	L_CALL(this, "Database::storage_get_blob()");

	int subdatabase = (doc.get_docid() - 1) % endpoints.size();
	const auto& storage = storages[subdatabase];
	if (!storage) {
		return "";
	}

	auto data = doc.get_data();
	auto blob = split_data_blob(data);

	if (blob.empty()) {
		auto store = split_data_store(data);
		if (store.first) {
			blob = storage_get(storage, store.second);
		}
	}

	return blob;
}


void
Database::storage_pull_blob(Xapian::Document& doc) const
{
	L_CALL(this, "Database::storage_pull_blob()");

	int subdatabase = (doc.get_docid() - 1) % endpoints.size();
	const auto& storage = storages[subdatabase];
	if (!storage) {
		return;
	}

	auto data = doc.get_data();
	auto blob = split_data_blob(data);

	if (blob.empty()) {
		auto store = split_data_store(data);
		if (store.first) {
			blob = storage_get(storage, store.second);
			doc.set_data(join_data(store.first, "", split_data_obj(data), blob));
		}
	}
}


void
Database::storage_push_blob(Xapian::Document& doc) const
{
	L_CALL(this, "Database::storage_push_blob()");

	int subdatabase = (doc.get_docid() - 1) % endpoints.size();
	const auto& storage = writable_storages[subdatabase];
	if (!storage) {
		return;
	}

	uint32_t offset;
	auto data = doc.get_data();
	auto blob = split_data_blob(data);
	if (!blob.size()) {
		return;
	}

	auto store = split_data_store(data);
	if (store.first) {
		if (store.second.empty()) {
			while (true) {
				try {
					offset = storage->write(blob);
					break;
				} catch (StorageEOF) {
					++storage->volume;
					storage->open(DATA_STORAGE_PATH + std::to_string(storage->volume), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE);
				}
			}
			auto stored_locator = storage_serialise_locator(storage->volume, offset, blob.size());
			doc.set_data(join_data(true, stored_locator, split_data_obj(data), ""));
		} else {
			doc.set_data(join_data(store.first, store.second, split_data_obj(data), ""));
		}
	}
}


void
Database::storage_commit()
{
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
	L_CALL(this, "Database::add_document(<doc>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

	Xapian::docid did = 0;

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_add_document(doc);
#else
	(void)wal_;
#endif /* XAPIAND_DATABASE_WAL */

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blob(doc_);
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Adding new document.  t: %d", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->add_document(doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document added (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document(Xapian::docid did, const Xapian::Document& doc, bool commit_, bool wal_)
{
	L_CALL(this, "Database::replace_document(%d, <doc>, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_replace_document(did, doc);
#else
	(void)wal_;
#endif /* XAPIAND_DATABASE_WAL */

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blob(doc_);
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Replacing: %d  t: %d", did, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->replace_document(did, doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document replaced (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document_term(const std::string& term, const Xapian::Document& doc, bool commit_, bool wal_)
{
	L_CALL(this, "Database::replace_document_term(%s, <doc>, %s, %s)", repr(term).c_str(), commit_ ? "true" : "false", wal_ ? "true" : "false");

	Xapian::docid did = 0;

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_replace_document_term(term, doc);
#else
	(void)wal_;
#endif

	Xapian::Document doc_ = doc;
#ifdef XAPIAND_DATA_STORAGE
	storage_push_blob(doc_);
#endif /* XAPIAND_DATA_STORAGE */

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP(this, "Replacing: '%s'  t: %d", term.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->replace_document(term, doc_);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database %s was modified, try again (%s)", repr(endpoints.to_string()).c_str(), exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, "Database %s error %s", repr(endpoints.to_string()).c_str(), exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document replaced (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}

	return did;
}


void
Database::add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL(this, "Database::add_spelling(<word, <freqinc>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_add_spelling(word, freqinc);
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->add_spelling(word, freqinc);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Spelling added (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}
}


void
Database::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL(this, "Database::remove_spelling(<word>, <freqdec>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_remove_spelling(word, freqdec);
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->remove_spelling(word, freqdec);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Spelling removed (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}
}


Xapian::docid
Database::find_document(const std::string& term_id)
{
	L_CALL(this, "Database::find_document(%s)", repr(term_id).c_str());

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
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Document found (took %s) [1]", delta_string(start, std::chrono::system_clock::now()).c_str());

	return did;
}


Xapian::Document
Database::get_document(const Xapian::docid& did, bool assume_valid_, bool pull_)
{
	L_CALL(this, "Database::get_document(%d)", did);

	Xapian::Document doc;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
#ifdef HAVE_XAPIAN_DATABASE_GET_DOCUMENT_WITH_FLAGS
			if (assume_valid_) {
				doc = db->get_document(did, Xapian::DOC_ASSUME_VALID);
			} else
#endif
			{
				doc = db->get_document(did);
			}
#ifdef XAPIAND_DATA_STORAGE
			if (pull_) {
				storage_pull_blob(doc);
			}
#else
	(void)pull_;
#endif /* XAPIAND_DATA_STORAGE */
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Got document (took %s) [1]", delta_string(start, std::chrono::system_clock::now()).c_str());

	return doc;
}


std::string
Database::get_metadata(const std::string& key)
{
	L_CALL(this, "Database::get_metadata(%s)", repr(key).c_str());

	std::string value;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			value = db->get_metadata(key);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Got metadata (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	return value;
}


std::vector<std::string>
Database::get_metadata_keys()
{
	L_CALL(this, "Database::get_metadata_keys()");

	std::vector<std::string> values;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			const Xapian::TermIterator end = db->metadata_keys_end();
			Xapian::TermIterator key = db->metadata_keys_begin();
			for (; key != end; ++key) {
				values.push_back(*key);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
		values.clear();
	}

	L_DATABASE_WRAP(this, "Got metadata keys (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	return values;
}


void
Database::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL(this, "Database::set_metadata(%s, %s, %s, %s)", repr(key).c_str(), repr(value).c_str(), commit_ ? "true" : "false", wal_ ? "true" : "false");

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) wal->write_set_metadata(key, value);
#else
	(void)wal_;
#endif

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->set_metadata(key, value);
			if (auto queue = weak_queue.lock()) {
				queue->modified = true;
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(TimeOutError, "Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			THROW(Error, exc.get_msg().c_str());
		}
		reopen();
	}

	L_DATABASE_WRAP(this, "Set metadata (took %s)", delta_string(start, std::chrono::system_clock::now()).c_str());

	if (commit_) {
		commit(wal_);
	}
}


#if defined(XAPIAND_V8) || defined(XAPIAND_CHAISCRIPT)
short
Database::get_document_change_seq(const std::string& term_id)
{
	L_CALL(this, "Database::get_document_change_seq(%s)", repr(term_id).c_str());

	auto it = documents.find(term_id);
	if (it == documents.end()) {
		documents.emplace(std::piecewise_construct, std::forward_as_tuple(term_id), std::forward_as_tuple(1, 0));
		return 0;
	} else {
		++it->second.first;
		return it->second.second;
	}
}


bool
Database::set_document_change_seq(const std::string& term_id, short old_revision)
{
	L_CALL(this, "Database::set_revision_document(%s, %d)", repr(term_id).c_str(), old_revision);

	auto it = documents.find(term_id);
	if (it == documents.end()) {
		return false;
	} else {
		if (old_revision == it->second.second) {
			++it->second.second; // Increment revision.
			if (--it->second.first == 0) { // Decrement count.
				documents.erase(term_id);
			}
			return true;
		} else {
			if (--it->second.first == 0) {
				documents.erase(term_id);
			}
			return false;
		}
	}
}


void
Database::dec_document_count(const std::string& term_id)
{
	L_CALL(this, "Database::dec_document_count(%s)", repr(term_id).c_str());

	auto it = documents.find(term_id);
	if (it != documents.end()) {
		if (--it->second.first == 0) {
			documents.erase(term_id);
		}
	}
}
#endif


/*  ____        _        _                     ___
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___ / _ \ _   _  ___ _   _  ___
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ | | | | | |/ _ \ | | |/ _ \
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |_| | |_| |  __/ |_| |  __/
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|\__\_\\__,_|\___|\__,_|\___|
 *
 */


DatabaseQueue::DatabaseQueue()
	: state(replica_state::REPLICA_FREE),
	  modified(false),
	  persistent(false),
	  count(0)
{
	L_OBJ(this, "CREATED DATABASE QUEUE!");
}


DatabaseQueue::~DatabaseQueue()
{
	if (size() != count) {
		L_CRIT(this, "Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	L_OBJ(this, "DELETED DATABASE QUEUE!");
}


bool
DatabaseQueue::inc_count(int max)
{
	L_CALL(this, "DatabaseQueue::inc_count(%d)", max);

	std::lock_guard<std::mutex> lk(_mutex);

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
	L_CALL(this, "DatabaseQueue::dec_count()");

	std::lock_guard<std::mutex> lk(_mutex);

	if (count <= 0) {
		L_CRIT(this, "Inconsistency in the number of databases in queue");
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


DatabasesLRU::DatabasesLRU(ssize_t max_size)
	: LRU(max_size) { }


std::shared_ptr<DatabaseQueue>&
DatabasesLRU::operator[](const std::pair<size_t, bool>& key)
{
	const auto now = std::chrono::system_clock::now();
	try {
		const auto on_get = [now](std::shared_ptr<DatabaseQueue>& val) {
			val->renew_time = now;
			return lru::GetAction::renew;
		};
		return at_and(on_get, key.first);
	} catch (const std::out_of_range&) {
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
		if (key.second) {
			// Volatile, insert to the back
			return insert_back_and(on_drop, std::make_pair(key.first, DatabaseQueue::make_shared()));
		} else {
			// Non-volatile, insert to the front
			return insert_and(on_drop, std::make_pair(key.first, DatabaseQueue::make_shared()));
		}
	}
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
	L_CALL(this, "DatabasesLRU::finish()");

	for (auto it = begin(); it != end(); ++it) {
		it->second->finish();
	}
}


/*  ____        _        _                    ____             _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___|  _ \ ___   ___ | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ |_) / _ \ / _ \| |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/  __/ (_) | (_) | |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_|   \___/ \___/|_|
 *
 */


DatabasePool::DatabasePool(size_t max_size)
	: finished(false),
	  databases(max_size),
	  writable_databases(max_size)
{
	L_OBJ(this, "CREATED DATABASE POLL!");
}


DatabasePool::~DatabasePool()
{
	finish();

	L_OBJ(this, "DELETED DATABASE POOL!");
}


void
DatabasePool::add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	L_CALL(this, "DatabasePool::add_endpoint_queue(%s, <queue>)", repr(endpoint.to_string()).c_str());

	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.insert(queue);
}


void
DatabasePool::drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	L_CALL(this, "DatabasePool::drop_endpoint_queue(%s, <queue>)", repr(endpoint.to_string()).c_str());

	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.erase(queue);

	if (queues_set.empty()) {
		queues.erase(hash);
	}
}


template<typename F, typename... Args>
inline void
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, F&& f, Args&&... args)
{
	try {
		checkout(database, endpoints, flags);
	} catch (const CheckoutError& e) {
		std::lock_guard<std::mutex> lk(qmtx);

		const auto index = std::make_pair(endpoints.hash(), flags & DB_VOLATILE);

		std::shared_ptr<DatabaseQueue> queue;
		if (flags & DB_WRITABLE) {
			queue = writable_databases[index];
		} else {
			queue = databases[index];
		}

		queue->checkin_callbacks.clear();
		queue->checkin_callbacks.enqueue(std::forward<F>(f), std::forward<Args>(args)...);

		throw e;
	}
}


void
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags)
{
	L_CALL(this, "DatabasePool::checkout(%s, 0x%02x (%s))", repr(endpoints.to_string()).c_str(), flags, [&flags]() {
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
		return join_string(values, " | ");
	}().c_str());

	bool writable = flags & DB_WRITABLE;
	bool writable_for_commit = (flags & DB_COMMIT) == DB_COMMIT;
	bool persistent = flags & DB_PERSISTENT;
	bool initref = flags & DB_INIT_REF;
	bool replication = flags & DB_REPLICATION;
	bool _volatile = flags & DB_VOLATILE;

	L_DATABASE_BEGIN(this, "++ CHECKING OUT DB [%s]: %s ...", writable ? "WR" : "RO", repr(endpoints.to_string()).c_str());

	ASSERT (!database);

	if (writable && endpoints.size() != 1) {
		L_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()).c_str());
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout database: %s (only one)", repr(endpoints.to_string()).c_str());
	}

	if (!finished) {
		size_t hash = endpoints.hash();
		const auto index = std::make_pair(hash, _volatile);
		std::shared_ptr<DatabaseQueue> queue;

		std::unique_lock<std::mutex> lk(qmtx);

		if (writable) {
			queue = writable_databases[index];
			databases.cleanup();
			if (writable_for_commit && !queue->modified) {
				L_DATABASE_END(this, "!! ABORTED CHECKOUT DB COMMIT NOT NEEDED [%s]: %s", writable ? "WR" : "RO", repr(endpoints.to_string()).c_str());
				THROW(CheckoutErrorCommited, "Cannot checkout database: %s (commit)", repr(endpoints.to_string()).c_str());
			}
		} else {
			queue = databases[index];
			writable_databases.cleanup();
		}

		auto old_state = queue->state;

		if (replication) {
			switch (queue->state) {
				case DatabaseQueue::replica_state::REPLICA_FREE:
					queue->state = DatabaseQueue::replica_state::REPLICA_LOCK;
					break;
				case DatabaseQueue::replica_state::REPLICA_LOCK:
				case DatabaseQueue::replica_state::REPLICA_SWITCH:
					L_REPLICATION(this, "A replication task is already waiting");
					L_DATABASE_END(this, "!! ABORTED CHECKOUT DB [%s]: %s", writable ? "WR" : "RO", repr(endpoints.to_string()).c_str());
					THROW(CheckoutErrorReplicating, "Cannot checkout database: %s (aborted)", repr(endpoints.to_string()).c_str());
			}
		} else {
			if (queue->state == DatabaseQueue::replica_state::REPLICA_SWITCH) {
				queue->switch_cond.wait(lk);
			}
		}

		bool old_persistent = queue->persistent;
		queue->persistent = persistent;

		if (!queue->pop(database, 0)) {
			// Increment so other threads don't delete the queue
			if (queue->inc_count(writable ? 1 : -1)) {
#ifdef XAPIAND_DATABASE_WAL
				size_t count = queue->count;
#endif
				lk.unlock();
				try {
					database = std::make_shared<Database>(queue, endpoints, flags);

					if (writable && initref) {
						DatabaseHandler::init_ref(endpoints[0]);
					}

#ifdef XAPIAND_DATABASE_WAL
					if (!writable && count == 1) {
						bool reopen = false;
						for (const auto& endpoint : database->endpoints) {
							if (endpoint.is_local()) {
								std::shared_ptr<Database> d;
								try {
									// Checkout executes any commands from the WAL
									checkout(d, endpoint, DB_WRITABLE);
									reopen = true;
									checkin(d);
								} catch (const CheckoutError&) { }
							}
						}
						if (reopen) {
							database->reopen();
						}
					}
#endif
				} catch (const Xapian::DatabaseOpeningError& exc) {
					const char* error = exc.get_error_string();
					if (error) {
						L_DATABASE(this, "ERROR: %s (%s)", exc.get_msg().c_str(), error);
					} else {
						L_DATABASE(this, "ERROR: %s", exc.get_msg().c_str());
					}
				} catch (const Xapian::Error& exc) {
					const char* error = exc.get_error_string();
					if (error) {
						L_EXC(this, "ERROR: %s (%s)", exc.get_msg().c_str(), error);
					} else {
						L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
					}
				}
				lk.lock();
				queue->dec_count();  // Decrement, count should have been already incremented if Database was created
			} else {
				// Lock until a database is available if it can't get one.
				lk.unlock();
				int s = queue->pop(database, DB_TIMEOUT);
				if (!s) {
					THROW(TimeOutError, "Database is not available");
				}
				lk.lock();
			}
		}
		if (!database || !database->db) {
			queue->state = old_state;
			queue->persistent = old_persistent;
			if (queue->count == 0) {
				//L_DEBUG(this, "There is a error, the queue ended up being empty, remove it");
				if (writable) {
					writable_databases.erase(hash);
				} else {
					databases.erase(hash);
				}
			}
			database.reset();
		}
	}

	if (!database) {
		L_DATABASE_END(this, "!! FAILED CHECKOUT DB [%s]: %s", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str());
		THROW(CheckoutError, "Cannot checkout database: %s", repr(endpoints.to_string()).c_str());
	}

	if (!writable && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() -  database->access_time).count() >= DATABASE_UPDATE_TIME) {
		try {
			database->reopen();
		} catch (const Xapian::DatabaseOpeningError& exc) {
			// Try to recover from DatabaseOpeningError (i.e when the index is manually deleted)
			recover_database(database->endpoints, RECOVER_REMOVE_ALL | RECOVER_DECREMENT_COUNT);
			database.reset();
			L_DATABASE_END(this, "!! FAILED CHECKOUT DB [%s]: %s (reopen)", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str());
			THROW(CheckoutError, "Cannot checkout database: %s (reopen)", repr(endpoints.to_string()).c_str());
		}
		L_DATABASE(this, "== REOPEN DB [%s]: %s", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(database->endpoints.to_string()).c_str());
	}

	L_DATABASE_END(this, "++ CHECKED OUT DB [%s]: %s (rev:%u)", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str(), database->checkout_revision);
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL(this, "DatabasePool::checkin(%s)", repr(database->to_string()).c_str());

	L_DATABASE_BEGIN(this, "-- CHECKING IN DB [%s]: %s ...", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(database->endpoints.to_string()).c_str());

	ASSERT(database);

	std::shared_ptr<DatabaseQueue> queue;

	std::unique_lock<std::mutex> lk(qmtx);

	if (database->flags & DB_WRITABLE) {
		auto& endpoint = database->endpoints[0];
		if (endpoint.is_local()) {
			auto new_revision = database->get_revision();
			if (database->checkout_revision != new_revision) {
				database->checkout_revision = new_revision;
				if (database->mastery_level != -1) {
					endpoint.mastery_level = database->mastery_level;
					updated_databases.push(endpoint);
				}
			}
		}
		queue = writable_databases[std::make_pair(database->hash, false)];
	} else {
		queue = databases[std::make_pair(database->hash, false)];
	}

	ASSERT(database->weak_queue.lock() == queue);

	if (queue->modified) {
		DatabaseAutocommit::commit(database);
	}

	queue->push(database);

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
		L_CRIT(this, "Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	L_DATABASE_END(this, "-- CHECKED IN DB [%s]: %s", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(endpoints.to_string()).c_str());

	database.reset();

	lk.unlock();

	if (signal_checkins) {
		while (queue->checkin_callbacks.call());
	}
}


void
DatabasePool::finish()
{
	L_CALL(this, "DatabasePool::finish()");

	finished = true;

	writable_databases.finish();
	databases.finish();

	L_OBJ(this, "FINISH DATABASE!");
}


bool
DatabasePool::_switch_db(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::_switch_db(%s)", repr(endpoint.to_string()).c_str());

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
		L_CRIT(this, "Inside switch_db, but not all queues have (queue->count == queue->size())");
	}

	return switched;
}


bool
DatabasePool::switch_db(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::switch_db(%s)", repr(endpoint.to_string()).c_str());

	std::lock_guard<std::mutex> lk(qmtx);
	return _switch_db(endpoint);
}


void
DatabasePool::recover_database(const Endpoints& endpoints, int flags)
{
	L_CALL(this, "DatabasePool::recover_database(%s, %d)", repr(endpoints.to_string()).c_str(), flags);

	size_t hash = endpoints.hash();

	if ((flags & RECOVER_REMOVE_WRITABLE) || (flags & RECOVER_REMOVE_ALL)) {
		/* erase from writable queue if exist */
		std::lock_guard<std::mutex> lk(qmtx);
		writable_databases.erase(hash);
	}

	if (flags & RECOVER_DECREMENT_COUNT) {
		/* Delete the count of the database creation */
		/* Avoid mismatch between queue size and counter */
		std::lock_guard<std::mutex> lk(qmtx);
		databases[std::make_pair(hash, false)]->dec_count();
	}

	if ((flags & RECOVER_REMOVE_DATABASE) || (flags & RECOVER_REMOVE_ALL)) {
		/* erase from queue if exist */
		std::lock_guard<std::mutex> lk(qmtx);
		databases.erase(hash);
	}
}


std::pair<size_t, size_t>
DatabasePool::total_writable_databases()
{
	L_CALL(this, "DatabasePool::total_wdatabases()");

	size_t db_count = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto d = writable_databases.begin(); d != writable_databases.end(); d++) {
		db_count += d->second->size();
	}
	return std::make_pair(writable_databases.size(), db_count);
}


std::pair<size_t, size_t>
DatabasePool::total_readable_databases()
{
	L_CALL(this, "DatabasePool::total_rdatabases()");

	size_t db_count = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto d = databases.begin(); d != databases.end(); d++) {
		db_count += d->second->size();
	}
	return std::make_pair(databases.size(), db_count);
}
