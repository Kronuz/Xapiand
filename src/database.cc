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
#include "log.h"                  // for L_OBJ, L_CALL, Log
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

#define SIZE_UUID 36

#define WAL_SYNC_MODE     STORAGE_ASYNC_SYNC
#define XAPIAN_SYNC_MODE  0       // This could also be Xapian::DB_FULL_SYNC for xapian to ensure full sync
#define STORAGE_SYNC_MODE STORAGE_FULL_SYNC


////////////////////////////////////////////////////////////////////////////////


std::string
join_data(bool stored, const std::string& stored_locator, const std::string& obj, const std::string& blob)
{
	L_CALL(nullptr, "::join_data(<stored>, <stored_locator>, <obj>, <blob>)");

	auto obj_len = serialise_length(obj.size());
	std::string data;
	if (stored) {
		auto stored_locator_len = serialise_length(stored_locator.size());
		data.reserve(1 + obj_len.size() + obj.size() + stored_locator_len.size() + stored_locator.size() + 1 + blob.size());
		data.push_back(DATABASE_DATA_HEADER_MAGIC_STORED);
		data.append(stored_locator_len);
		data.append(stored_locator);
	} else {
		data.reserve(1 + obj_len.size() + obj.size() + 1 + blob.size());
		data.push_back(DATABASE_DATA_HEADER_MAGIC);
	}
	data.append(obj_len);
	data.append(obj);
	data.push_back(DATABASE_DATA_FOOTER_MAGIC);
	data.append(blob);
	return data;
}


std::pair<bool, std::string>
split_data_store(const std::string& data)
{
	L_CALL(nullptr, "::split_data_store(<data>)");

	std::string stored_locator;
	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		return std::make_pair(false, std::string());
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (Xapian::SerialisationError) {
			return std::make_pair(false, std::string());
		}
		stored_locator = std::string(p, length);
		p += length;
	} else {
		return std::make_pair(false, std::string());
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return std::make_pair(false, std::string());
	}

	if (*(p + length) != DATABASE_DATA_FOOTER_MAGIC) {
		return std::make_pair(false, std::string());
	}

	return std::make_pair(true, stored_locator);
}


std::string
split_data_obj(const std::string& data)
{
	L_CALL(nullptr, "::split_data_obj(<data>)");

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		++p;
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (Xapian::SerialisationError) {
			return std::string();
		}
		p += length;
	} else {
		return std::string();
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return std::string();
	}

	if (*(p + length) != DATABASE_DATA_FOOTER_MAGIC) {
		return std::string();
	}

	return std::string(p, length);
}


std::string
split_data_blob(const std::string& data)
{
	L_CALL(nullptr, "::split_data_blob(<data>)");

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		++p;
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (Xapian::SerialisationError) {
			return data;
		}
		p += length;
	} else {
		return data;
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return data;
	}
	p += length;

	if (*p++ != DATABASE_DATA_FOOTER_MAGIC) {
		return data;
	}

	return std::string(p, p_end - p);
}


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
	L_CALL(this, "DatabaseWAL::open_current()");

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
	L_CALL(this, "DatabaseWAL::execute()");

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
		open(base_path + WAL_STORAGE_PATH + "0", STORAGE_OPEN | STORAGE_COMPRESS);
	} catch (const StorageIOError&) {
		return true;
	}

	auto uuid = Serialise::uuid(std::string(header.head.uuid, 36));

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
	L_CALL(this, "DatabaseWAL::write_line()");

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
	L_CALL(this, "DatabaseWAL::write_add_document()");

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
	L_CALL(this, "DatabaseWAL::write_delete_document_term()");

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
	L_CALL(this, "DatabaseWAL::write_replace_document()");

	write_line(Type::REPLACE_DOCUMENT, serialise_length(did) + doc.serialise());
}


void
DatabaseWAL::write_replace_document_term(const std::string& term, const Xapian::Document& doc)
{
	L_CALL(this, "DatabaseWAL::write_replace_document_term()");

	write_line(Type::REPLACE_DOCUMENT_TERM, serialise_length(term.size()) + term + doc.serialise());
}


void
DatabaseWAL::write_delete_document(Xapian::docid did)
{
	L_CALL(this, "DatabaseWAL::write_delete_document()");

	write_line(Type::DELETE_DOCUMENT, serialise_length(did));
}


void
DatabaseWAL::write_set_metadata(const std::string& key, const std::string& val)
{
	L_CALL(this, "DatabaseWAL::write_set_metadata()");

	write_line(Type::SET_METADATA, serialise_length(key.size()) + key + val);
}


void
DatabaseWAL::write_add_spelling(const std::string& word, Xapian::termcount freqinc)
{
	L_CALL(this, "DatabaseWAL::write_add_spelling()");

	write_line(Type::ADD_SPELLING, serialise_length(freqinc) + word);
}


void
DatabaseWAL::write_remove_spelling(const std::string& word, Xapian::termcount freqdec)
{
	L_CALL(this, "DatabaseWAL::write_remove_spelling()");

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
	  modified(false),
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
			const char* error = exc.get_error_string();
			if (error) {
				L_WARNING(this, "ERROR: %s (%s)", exc.get_msg().c_str(), error);
			} else {
				L_WARNING(this, "ERROR: %s", exc.get_msg().c_str());
			}
			db->close();
			db.reset();
			throw;
		} catch (const Xapian::Error& exc) {
			L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
			db->close();
			db.reset();
		}
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
			{
				DatabaseWAL tmp_wal(e.path, this);
				tmp_wal.init_database();
			}

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
				modified = true;
			}
		}
#endif /* XAPIAND_DATABASE_WAL */
	} else {
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
				{
					DatabaseWAL tmp_wal(e.path, this);
					tmp_wal.init_database();
				}

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

	if (!modified) {
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
			modified = false;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
std::tuple<ssize_t, size_t, size_t>
Database::storage_unserialise_locator(const std::string& store)
{
	ssize_t volume;
	size_t offset, size;

	const char *p = store.data();
	const char *p_end = p + store.size();
	if (*p++ != STORAGE_BIN_HEADER_MAGIC) {
		return std::make_tuple(-1, 0, 0);
	}
	try {
		volume = unserialise_length(&p, p_end);
	} catch (Xapian::SerialisationError) {
		return std::make_tuple(-1, 0, 0);
	}
	try {
		offset = unserialise_length(&p, p_end);
	} catch (Xapian::SerialisationError) {
		return std::make_tuple(-1, 0, 0);
	}
	try {
		size = unserialise_length(&p, p_end);
	} catch (Xapian::SerialisationError) {
		return std::make_tuple(-1, 0, 0);
	}
	if (*p++ != STORAGE_BIN_FOOTER_MAGIC) {
		return std::make_tuple(-1, 0, 0);
	}

	return std::make_tuple(volume, offset, size);
}


std::string
Database::storage_serialise_locator(ssize_t volume, size_t offset, size_t size)
{
	std::string ret;
	ret.append(std::string(1, STORAGE_BIN_HEADER_MAGIC));
	ret.append(serialise_length(volume));
	ret.append(serialise_length(offset));
	ret.append(serialise_length(size));
	ret.append(std::string(1, STORAGE_BIN_FOOTER_MAGIC));
	return ret;
}


std::string
Database::storage_get(const std::unique_ptr<DataStorage>& storage, const std::string& store)
{
	L_CALL(this, "Database::storage_get()");

	auto locator = storage_unserialise_locator(store);

	storage->open(DATA_STORAGE_PATH + std::to_string(std::get<0>(locator)), STORAGE_OPEN);
	storage->seek(static_cast<uint32_t>(std::get<1>(locator)));

	return storage->read();
}


std::string
Database::storage_get_blob(const Xapian::Document& doc)
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
Database::storage_pull_blob(Xapian::Document& doc)
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
Database::storage_push_blob(Xapian::Document& doc)
{
	L_CALL(this, "Database::storage_push_blob()");

	int subdatabase = (doc.get_docid() - 1) % endpoints.size();
	auto& storage = writable_storages[subdatabase];
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database %s was modified, try again (%s)", repr(endpoints.to_string()).c_str(), exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			if (!t) {
				THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
			}
		} catch (const Xapian::NetworkError& exc) {
			if (!t) {
				THROW(Error, "Problem communicating with the remote database (%s)", exc.get_msg().c_str());
			}
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
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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
			modified = true;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) THROW(Error, "Database was modified, try again (%s)", exc.get_msg().c_str());
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


#ifdef XAPIAND_V8
short
Database::get_revision_document(const std::string& term_id)
{
	L_CALL(this, "Database::get_revision_document(%s)", repr(term_id).c_str());

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
Database::set_revision_document(const std::string& term_id, short old_revision)
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
Database::dec_count_document(const std::string& term_id)
{
	L_CALL(this, "Database::dec_count_document(%s)", repr(term_id).c_str());

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
	  persistent(false),
	  count(0)
{
	L_OBJ(this, "CREATED DATABASE QUEUE!");
}


DatabaseQueue::DatabaseQueue(DatabaseQueue&& q)
{
	std::lock_guard<std::mutex> lk(q._mutex);
	_items_queue = std::move(q._items_queue);
	_limit = std::move(q._limit);
	state = std::move(q.state);
	persistent = std::move(q.persistent);
	count = std::move(q.count);
	weak_database_pool = std::move(q.weak_database_pool);

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
DatabasesLRU::operator[] (size_t key)
{
	try {
		return at(key);
	} catch (std::range_error) {
		return insert_and([](std::shared_ptr<DatabaseQueue>& val) {
			if (val->persistent || val->size() < val->count || val->state != DatabaseQueue::replica_state::REPLICA_FREE) {
				return lru::DropAction::renew;
			} else {
				return lru::DropAction::drop;
			}
		}, std::make_pair(key, std::make_shared<DatabaseQueue>()));
	}
}


void
DatabasesLRU::finish()
{
	L_CALL(this, "DatabasesLRU::finish()");

	for (iterator it = begin(); it != end(); ++it) {
		it->second->finish();
	}
}


/*  __  __           ____            _    _     ____  _   _
 * |  \/  |___  __ _|  _ \ __ _  ___| | _| |   |  _ \| | | |
 * | |\/| / __|/ _` | |_) / _` |/ __| |/ / |   | |_) | | | |
 * | |  | \__ \ (_| |  __/ (_| | (__|   <| |___|  _ <| |_| |
 * |_|  |_|___/\__, |_|   \__,_|\___|_|\_\_____|_| \_\\___/
 *             |___/
 */


MsgPackLRU::MsgPackLRU(ssize_t max_size)
	: LRU(max_size) { }


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
	  writable_databases(max_size),
	  schemas(max_size)
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


long long
DatabasePool::get_mastery_level(const std::string& dir)
{
	L_CALL(this, "DatabasePool::get_mastery_level(%s)", repr(dir).c_str());

	Endpoints endpoints;
	endpoints.add(Endpoint(dir));

	std::shared_ptr<Database> database;
	if (checkout(database, endpoints, 0)) {
		long long mastery_level = database->mastery_level;
		checkin(database);
		return mastery_level;
	}

	return read_mastery(dir, false);
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


template<typename F, typename... Args>
inline bool
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags, F&& f, Args&&... args)
{
	bool ret = checkout(database, endpoints, flags);
	if (!ret) {
		std::lock_guard<std::mutex> lk(qmtx);

		size_t hash = endpoints.hash();

		std::shared_ptr<DatabaseQueue> queue;
		if (flags & DB_WRITABLE) {
			queue = writable_databases[hash];
		} else {
			queue = databases[hash];
		}

		queue->checkin_callbacks.clear();
		queue->checkin_callbacks.enqueue(std::forward<F>(f), std::forward<Args>(args)...);
	}
	return ret;
}


bool
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
	bool persistent = flags & DB_PERSISTENT;
	bool initref = flags & DB_INIT_REF;
	bool replication = flags & DB_REPLICATION;

	L_DATABASE_BEGIN(this, "++ CHECKING OUT DB [%s]: %s ...", writable ? "WR" : "RO", repr(endpoints.to_string()).c_str());

	ASSERT (!database);

	if (writable && endpoints.size() != 1) {
		L_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()).c_str());
		return false;
	}

	if (!finished) {
		size_t hash = endpoints.hash();
		std::shared_ptr<DatabaseQueue> queue;

		std::unique_lock<std::mutex> lk(qmtx);

		if (writable) {
			queue = writable_databases[hash];
		} else {
			queue = databases[hash];
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
					return false;
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
				bool count = queue->count;
				lk.unlock();
				try {
					database = std::make_shared<Database>(queue, endpoints, flags);

					if (writable && initref) {
						init_ref(endpoints[0]);
					}

#ifdef XAPIAND_DATABASE_WAL
					if (!writable && count == 1) {
						bool reopen = false;
						for (const auto& endpoint : database->endpoints) {
							if (endpoint.is_local()) {
								Endpoints e;
								e.add(endpoint);
								std::shared_ptr<Database> d;
								if (checkout(d, e, DB_WRITABLE)) {
									// Checkout executes any commands from the WAL
									reopen = true;
									checkin(d);
								}
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
				int s = queue->pop(database);
				if (!s) {
					L_ERR(this, "ERROR: Database is not available. Writable: %d", writable);
				}
				lk.lock();
			}
		}
		if (!database) {
			queue->state = old_state;
			queue->persistent = old_persistent;
			if (queue->count == 0) {
				// There was an error and the queue ended up being empty, remove it!
				if (writable) {
					writable_databases.erase(hash);
				} else {
					databases.erase(hash);
				}
			}
		}
	}

	if (!database) {
		L_DATABASE_END(this, "!! FAILED CHECKOUT DB [%s]: %s", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str());
		return false;
	}

	if (!writable && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() -  database->access_time).count() >= DATABASE_UPDATE_TIME) {
		try {
			database->reopen();
		} catch (const Xapian::DatabaseOpeningError& exc) {
			// Try to recover from DatabaseOpeningError (i.e when the index is manually deleted)
			recover_database(database->endpoints, RECOVER_REMOVE_ALL | RECOVER_DECREMENT_COUT);
			L_DATABASE_END(this, "!! FAILED CHECKOUT DB [%s]: %s", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str());
			return false;
		}
		L_DATABASE(this, "== REOPEN DB [%s]: %s", (database->flags & DB_WRITABLE) ? "WR" : "RO", repr(database->endpoints.to_string()).c_str());
	}

	L_DATABASE_END(this, "++ CHECKED OUT DB [%s]: %s (rev:%u)", writable ? "WR" : "WR", repr(endpoints.to_string()).c_str(), database->checkout_revision);
	return true;
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
		queue = writable_databases[database->hash];
	} else {
		queue = databases[database->hash];
	}

	ASSERT(database->weak_queue.lock() == queue);

	int flags = database->flags;

	if (database->modified) {
		DatabaseAutocommit::commit(database);
	}

	if (!(flags & DB_VOLATILE)) {
		queue->push(database);
	}

	auto& endpoints = database->endpoints;
	bool signal_checkins = false;
	switch (queue->state) {
		case DatabaseQueue::replica_state::REPLICA_SWITCH:
			for (auto& endpoint : endpoints) {
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

	L_DATABASE_END(this, "-- CHECKED IN DB [%s]: %s", (flags & DB_WRITABLE) ? "WR" : "RO", repr(endpoints.to_string()).c_str());

	database.reset();

	lk.unlock();

	if (signal_checkins) {
		while (queue->checkin_callbacks.call());
	}
}


bool
DatabasePool::_switch_db(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::_switch_db(%s)", repr(endpoint.to_string()).c_str());

	auto queues_set = queues[endpoint.hash()];

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
DatabasePool::init_ref(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::init_ref(%s)", repr(endpoint.to_string()).c_str());

	Endpoints ref_endpoints;
	ref_endpoints.add(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL)) {
		L_CRIT(this, "Cannot open %s database.", repr(ref_endpoints.to_string()).c_str());
		return;
	}

	std::string unique_id(prefixed(get_hashed(endpoint.path), DOCUMENT_ID_TERM_PREFIX, toUType(FieldType::TERM)));
	Xapian::PostingIterator p(ref_database->db->postlist_begin(unique_id));
	if (p == ref_database->db->postlist_end(unique_id)) {
		Xapian::Document doc;
		// Boolean term for the node.
		doc.add_boolean_term(unique_id);
		// Start values for the DB.
		doc.add_boolean_term(prefixed(DOCUMENT_DB_MASTER, get_prefix("master"), toUType(FieldType::TERM)));
		try {
			ref_database->replace_document_term(unique_id, doc, true);
		} catch (const BaseException& exc) {
			L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown exception!");
		}
	}

	checkin(ref_database);
}


void
DatabasePool::inc_ref(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::inc_ref(%s)", repr(endpoint.to_string()).c_str());

	Endpoints ref_endpoints;
	ref_endpoints.add(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL)) {
		L_CRIT(this, "Cannot open %s database.", repr(ref_endpoints.to_string()).c_str());
		return;
	}

	Xapian::Document doc;

	std::string unique_id(prefixed(get_hashed(endpoint.path), DOCUMENT_ID_TERM_PREFIX, toUType(FieldType::TERM)));
	Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
	if (p == ref_database->db->postlist_end(unique_id)) {
		// QUESTION: Document not found - should add?
		// QUESTION: This case could happen?
		doc.add_boolean_term(unique_id);
		doc.add_value(0, "0");
		try {
			ref_database->replace_document_term(unique_id, doc, true);
		} catch (const BaseException& exc) {
			L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown exception!");
		}
	} else {
		// Document found - reference increased
		doc = ref_database->db->get_document(*p);
		doc.add_boolean_term(unique_id);
		int nref = std::stoi(doc.get_value(0));
		doc.add_value(0, std::to_string(nref + 1));
		try {
			ref_database->replace_document_term(unique_id, doc, true);
		} catch (const BaseException& exc) {
			L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown exception!");
		}
	}

	checkin(ref_database);
}


void
DatabasePool::dec_ref(const Endpoint& endpoint)
{
	L_CALL(this, "DatabasePool::dec_ref(%s)", repr(endpoint.to_string()).c_str());

	Endpoints ref_endpoints;
	ref_endpoints.add(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL)) {
		L_CRIT(this, "Cannot open %s database.", repr(ref_endpoints.to_string()).c_str());
		return;
	}

	Xapian::Document doc;

	std::string unique_id(prefixed(get_hashed(endpoint.path), DOCUMENT_ID_TERM_PREFIX, toUType(FieldType::TERM)));
	Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
	if (p != ref_database->db->postlist_end(unique_id)) {
		doc = ref_database->db->get_document(*p);
		doc.add_boolean_term(unique_id);
		int nref = std::stoi(doc.get_value(0)) - 1;
		doc.add_value(0, std::to_string(nref));
		try {
			ref_database->replace_document_term(unique_id, doc, true);
		} catch (const BaseException& exc) {
			L_EXC(this, "ERROR: %s", *exc.get_context() ? exc.get_context() : "Unkown exception!");
		}
		if (nref == 0) {
			// qmtx need a lock
			delete_files(endpoint.path);
		}
	}

	checkin(ref_database);
}


void
DatabasePool::recover_database(const Endpoints& endpoints, int flags)
{
	L_CALL(this, "DatabasePool::reestablish_database()");

	size_t hash = endpoints.hash();

	if ((flags & RECOVER_REMOVE_WRITABLE) || (flags & RECOVER_REMOVE_ALL)) {
		/* erase from writable queue if exist */
		writable_databases.erase(hash);
	}

	if (flags & RECOVER_DECREMENT_COUT) {
		/* Delete the count of the database creation */
		/* Avoid mismatch between queue size and counter */
		databases[hash]->dec_count();
	}

	if ((flags & RECOVER_REMOVE_DATABASE) || (flags & RECOVER_REMOVE_ALL)) {
		/* erase from queue if exist */
		databases.erase(hash);
	}
}


int
DatabasePool::get_master_count()
{
	L_CALL(this, "DatabasePool::get_master_count()");

	Endpoints ref_endpoints;
	ref_endpoints.add(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT | DB_NOWAL)) {
		L_CRIT(this, "Cannot open %s database.", repr(ref_endpoints.to_string()).c_str());
		return -1;
	}

	int count = 0;

	if (ref_database) {
		Xapian::PostingIterator p(ref_database->db->postlist_begin(DOCUMENT_DB_MASTER));
		count = std::distance(ref_database->db->postlist_begin(DOCUMENT_DB_MASTER), ref_database->db->postlist_end(DOCUMENT_DB_MASTER));
	}

	checkin(ref_database);

	return count;
}


std::pair<bool, atomic_shared_ptr<const MsgPack>*>
DatabasePool::get_local_schema(const Endpoint& endpoint, int flags, const MsgPack* obj)
{
	bool created = false;
	auto local_schema_hash = endpoint.hash();
	atomic_shared_ptr<const MsgPack>* atom_local_schema;
	{
		std::lock_guard<std::mutex> lk(smtx);
		atom_local_schema = &schemas[local_schema_hash];
	}
	auto local_schema_ptr = atom_local_schema->load();
	if (!local_schema_ptr) {
		std::string str_schema;
		std::shared_ptr<Database> database;
		if (checkout(database, Endpoints(endpoint), flags != -1 ? flags : DB_WRITABLE)) {
			str_schema.assign(database->get_metadata(DB_META_SCHEMA));
			std::shared_ptr<const MsgPack> aux_schema_ptr;
			if (str_schema.empty()) {
				created = true;
				if (obj && obj->find(RESERVED_SCHEMA) != obj->end()) {
					auto path = obj->at(RESERVED_SCHEMA);
					if (path.is_string()) {
						aux_schema_ptr = std::make_shared<const MsgPack>(path.as_string());
					} else {
						std::lock_guard<std::mutex> lk(smtx);
						schemas.erase(local_schema_hash);
						THROW(ClientError, "%s must be string", RESERVED_SCHEMA);
					}
				} else {
					aux_schema_ptr = Schema::get_initial_schema();
				}
			} else {
				try {
					aux_schema_ptr = std::make_shared<const MsgPack>(MsgPack::unserialise(str_schema));
				} catch (const msgpack::unpack_error& e) {
					std::lock_guard<std::mutex> lk(smtx);
					schemas.erase(local_schema_hash);
					THROW(SerialisationError, "Unpack error: %s", e.what());
				}
			}
			aux_schema_ptr->lock();
			if (atom_local_schema->compare_exchange_strong(local_schema_ptr, aux_schema_ptr)) {
				local_schema_ptr = aux_schema_ptr;
				if (str_schema.empty()) {
					database->set_metadata(DB_META_SCHEMA, local_schema_ptr->serialise());
				}
			}
			checkin(database);
		} else {
			std::lock_guard<std::mutex> lk(smtx);
			schemas.erase(local_schema_hash);
			THROW(CheckoutError, "Cannot checkout database: %s", repr(endpoint.to_string()).c_str());
		}
	}

	if (!local_schema_ptr->is_map() && !local_schema_ptr->is_string()) {
		std::lock_guard<std::mutex> lk(smtx);
		schemas.erase(local_schema_hash);
		THROW(Error, "Invalid type for schema: %s", repr(endpoint.to_string()).c_str());
	}

	return std::make_pair(created, atom_local_schema);
}


MsgPack
DatabasePool::get_shared_schema(const Endpoint& endpoint, const std::string& id, int flags)
{
	L_CALL(this, "DatabasePool::get_shared_schema(%s)(%s)", repr(endpoint.to_string()).c_str(), id.c_str());

	try {
		DatabaseHandler db_handler;
		db_handler.reset(Endpoints(endpoint), flags != -1 ? flags : DB_OPEN, HTTP_POST);
		auto doc = db_handler.get_document(id);
		return doc.get_obj();
	} catch (const DocNotFoundError&) {
		THROW(DocNotFoundError, "In shared schema %s document not found: %s", repr(endpoint.to_string()).c_str());
	}
}


std::shared_ptr<const MsgPack>
DatabasePool::get_schema(const Endpoint& endpoint, int flags, const MsgPack* obj)
{
	L_CALL(this, "DatabasePool::get_schema(%s, 0x%02x, <obj>)", repr(endpoint.to_string()).c_str(), flags);

	if (finished) return nullptr;

	auto atom_local_schema = get_local_schema(endpoint, flags, obj);

	auto schema_ptr = atom_local_schema.second->load();
	if (!schema_ptr->is_map()) {
		auto schema_path = split_index(schema_ptr->as_string());
		auto shared_schema_hash = std::hash<std::string>{}(schema_path.first);
		atomic_shared_ptr<const MsgPack>* atom_shared_schema;
		{
			std::lock_guard<std::mutex> lk(smtx);
			atom_shared_schema = &schemas[shared_schema_hash];
		}
		auto shared_schema_ptr = atom_shared_schema->load();
		if (shared_schema_ptr) {
			schema_ptr = shared_schema_ptr;
		} else {
			if (atom_local_schema.first) {
				schema_ptr = Schema::get_initial_schema();
			} else {
				schema_ptr = std::make_shared<const MsgPack>(get_shared_schema(schema_path.first, schema_path.second));
			}
			schema_ptr->lock();
			if (!atom_shared_schema->compare_exchange_strong(shared_schema_ptr, schema_ptr)) {
				schema_ptr = shared_schema_ptr;
			}
		}
		if (!schema_ptr->is_map()) {
			std::lock_guard<std::mutex> lk(smtx);
			schemas.erase(shared_schema_hash);
			THROW(Error, "Schema is not a map: %s", repr(endpoint.to_string()).c_str());
		}
	}

	return schema_ptr;
}


bool
DatabasePool::set_schema(const Endpoint& endpoint, int flags, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema)
{
	L_CALL(this, "DatabasePool::set_schema(%s, %d, <old_schema>, %s)", repr(endpoint.to_string()).c_str(), flags, new_schema ? repr(new_schema->to_string()).c_str() : "nullptr");

	auto atom_local_schema = get_local_schema(endpoint, flags, nullptr);
	auto local_schema_ptr = atom_local_schema.second->load();

	if (!local_schema_ptr->is_map()) {
		auto schema_path = split_index(local_schema_ptr->as_string());
		auto shared_schema_hash = std::hash<std::string>{}(schema_path.first);
		atomic_shared_ptr<const MsgPack>* atom_shared_schema;
		{
			std::lock_guard<std::mutex> lk(smtx);
			atom_shared_schema = &schemas[shared_schema_hash];
		}
		std::shared_ptr<const MsgPack> aux_schema;
		if (atom_shared_schema->load()) {
			aux_schema = old_schema;
		}
		if (atom_shared_schema->compare_exchange_strong(aux_schema, new_schema)) {
			DatabaseHandler db_handler;
			Endpoint shared_endpoint(schema_path.first);
			try {
				db_handler.reset(shared_endpoint, DB_WRITABLE | DB_SPAWN | DB_NOWAL, HTTP_GET);
				MsgPack shared_schema;
				shared_schema = *new_schema;
				shared_schema[RESERVED_RECURSE] = false;
				db_handler.index(schema_path.second, true, shared_schema, false, MSGPACK_CONTENT_TYPE);
				if (XapiandManager::manager->strict) {
					shared_schema[ID_FIELD_NAME][RESERVED_TYPE] = TERM_STR;
				}
				return true;
			} catch (const CheckoutError&) {
				THROW(CheckoutError, "Cannot checkout document: %s", repr(schema_path.first).c_str());
			}
		} else {
			old_schema = aux_schema;
		}
	} else {
		if (atom_local_schema.second->compare_exchange_strong(old_schema, new_schema)) {
			std::shared_ptr<Database> database;
			if (checkout(database, Endpoints(endpoint), flags != -1 ? flags : DB_WRITABLE)) {
				database->set_metadata(DB_META_SCHEMA, new_schema->serialise());
				checkin(database);
				return true;
			} else {
				THROW(CheckoutError, "Cannot checkout database: %s", repr(endpoint.to_string()).c_str());
			}
		}
	}

	return false;
}
