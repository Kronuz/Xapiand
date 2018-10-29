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
#include <utility>
#include <strings.h>              // for strncasecmp
#include <errno.h>                // for __error, errno
#include <fcntl.h>                // for O_CREAT, O_WRONLY, O_EXCL
#include <sysexits.h>             // for EX_SOFTWARE

#include "atomic_shared_ptr.h"    // for atomic_shared_ptr
#include "database_autocommit.h"  // for DatabaseAutocommit
#include "database_handler.h"     // for DatabaseHandler
#include "exception.h"            // for Error, MSG_Error, Exception, DocNot...
#include "fs.hh"                  // for move_files, exists, build_path_index
#include "ignore_unused.h"        // for ignore_unused
#include "io.hh"                  // for close, strerrno, write, open
#include "length.h"               // for serialise_length, unserialise_length
#include "log.h"                  // for L_OBJ, L_CALL
#include "lz4_compressor.h"       // for compress_lz4, decompress_lz4
#include "manager.h"              // for sig_exit
#include "msgpack.h"              // for MsgPack
#include "msgpack/unpack.hpp"     // for unpack_error
#include "opts.h"                 // for opts
#include "repr.hh"                // for repr
#include "schema.h"               // for FieldType, FieldType::KEYWORD
#include "serialise.h"            // for uuid
#include "string.hh"              // for string::from_delta


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_DATABASE_BEGIN
// #define L_DATABASE_BEGIN L_DELAYED_600
// #undef L_DATABASE_END
// #define L_DATABASE_END L_DELAYED_N_UNLOG


#define XAPIAN_LOCAL_DB_FALLBACK 1

#define REMOTE_DATABASE_UPDATE_TIME 3
#define LOCAL_DATABASE_UPDATE_TIME 10

#define DATA_STORAGE_PATH "docdata."

#define WAL_STORAGE_PATH "wal."

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
WalHeader::init(void* param, void* /*unused*/)
{
	const auto* wal = static_cast<const DatabaseWAL*>(param);

	memcpy(&head.uuid[0], wal->database->get_uuid().get_bytes().data(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
	head.revision = wal->database->get_revision();
}


void
WalHeader::validate(void* param, void* /*unused*/)
{
	const auto* wal = static_cast<const DatabaseWAL*>(param);
	if (wal->validate_uuid) {
		UUID uuid(head.uuid);
		if (wal->database) {
			if (uuid != wal->database->get_uuid()) {
				if (uuid != wal->uuid_le()) {
					THROW(StorageCorruptVolume, "WAL UUID mismatch");
				}
			}
		} else if (wal->uuid().empty()) {
			if (uuid != wal->uuid()) {
				// Xapian under FreeBSD stores UUIDs in native order (could be little endian)
				if (uuid != wal->uuid_le()) {
					THROW(StorageCorruptVolume, "WAL UUID mismatch");
				}
			}
		}
	}
}


DatabaseWAL::DatabaseWAL(std::string_view base_path_, Database* database_)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(base_path_, this),
	  validate_uuid(true),
	  database(database_)
{
	L_OBJ("CREATED DATABASE WAL!");
}


const UUID&
DatabaseWAL::uuid() const
{
	if (_uuid.empty()) {
		std::array<unsigned char, 16> uuid_data;
		if (::read_uuid(base_path, uuid_data) != -1) {
			_uuid = UUID(uuid_data);
			_uuid_le = UUID(uuid_data, true);
		}
	}
	return _uuid;
}


const UUID&
DatabaseWAL::uuid_le() const
{
	if (_uuid_le.empty()) {
		uuid();
	}
	return _uuid_le;
}


DatabaseWAL::~DatabaseWAL()
{
	L_OBJ("DELETED DATABASE WAL!");
}


bool
DatabaseWAL::open_current(bool only_committed, bool unsafe)
{
	L_CALL("DatabaseWAL::open_current(%s)", only_committed ? "true" : "false");

	Xapian::rev revision = database->reopen_revision;

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision);

	bool modified = false;

	try {
		bool end = false;
		Xapian::rev end_rev;
		for (end_rev = volumes.first; end_rev <= volumes.second && !end; ++end_rev) {
			try {
				open(string::format(WAL_STORAGE_PATH "%llu", end_rev), STORAGE_OPEN);
			} catch (const StorageIOError& exc) {
				if (unsafe) {
					L_WARNING("Cannot open WAL volume: %s", exc.get_context());
					continue;
				}
				throw;
			} catch (const StorageCorruptVolume& exc) {
				if (unsafe) {
					L_WARNING("Corrupt WAL volume: %s", exc.get_context());
					continue;
				}
				throw;
			}

			Xapian::rev file_rev, begin_rev;
			file_rev = begin_rev = end_rev;
			if (file_rev != header.head.revision) {
				if (unsafe) {
					L_WARNING("Incorrect WAL revision");
					continue;
				}
				THROW(StorageCorruptVolume, "Incorrect WAL revision");
			}

			auto high_slot = highest_valid_slot();
			if (high_slot == static_cast<uint32_t>(-1)) {
				if (revision != file_rev) {
					if (unsafe) {
						L_WARNING("No WAL slots");
						continue;
					}
					THROW(StorageCorruptVolume, "No WAL slots");
				}
				continue;
			}
			if (high_slot == 0) {
				if (only_committed) {
					continue;
				}
			}

			if (file_rev == volumes.second) {
				end = true;  // Avoid reenter to the loop with the high valid slot of the highest revision
				if (only_committed) {
					// last slot is uncommitted contain offset at the end of file
					// In case not "committed" not execute the high slot avaible because are operations without commit
					--high_slot;
				}
			}

			end_rev = file_rev + high_slot;
			if (end_rev < revision) {
				continue;
			}

			uint32_t start_off;
			if (file_rev == volumes.first) {
				assert(revision >= file_rev);
				if (revision == file_rev) {
					// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
					// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
					begin_rev = file_rev;
					start_off = STORAGE_START_BLOCK_OFFSET;
				} else {
					auto slot = revision - file_rev - 1;
					begin_rev = file_rev + slot;
					start_off = header.slot[slot];
				}
			} else {
				start_off = STORAGE_START_BLOCK_OFFSET;
			}

			auto end_off = header.slot[high_slot];
			if (start_off < end_off) {
				L_INFO("Read and execute operations WAL file (wal.%llu) from [%llu..%llu] revision", file_rev, begin_rev, end_rev);
			}

			seek(start_off);
			try {
				while (true) {
					std::string line = read(end_off);
					modified = execute(line, false, false, unsafe);
				}
			} catch (const StorageEOF& exc) { }
		}

		if (volumes.first <= volumes.second) {
			if (end_rev < revision) {
				if (!unsafe) {
					THROW(StorageCorruptVolume, "WAL revision not reached");
				}
				L_WARNING("WAL revision not reached");
			}
			open(string::format(WAL_STORAGE_PATH "%llu", volumes.second), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
		} else {
			open(string::format(WAL_STORAGE_PATH "%llu", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
		}
	} catch (const StorageException& exc) {
		L_ERR("WAL ERROR in %s: %s", ::repr(database->endpoints.to_string()), exc.get_message());
		database->wal.reset();
		Metrics::metrics()
			.xapiand_wal_errors
			.Increment();
	}

	return modified;
}


MsgPack
DatabaseWAL::repr_document(std::string_view serialised_document, bool unserialised)
{
	L_CALL("DatabaseWAL::repr_document(<serialised_document>)");

	if (!unserialised) {
		return MsgPack(serialised_document);
	}

	auto doc = Xapian::Document::unserialise(std::string(serialised_document));

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
				obj["_data"].push_back(MsgPack({
					{ "_content_type", locator.ct_type.to_string() },
					{ "_type", "stored" },
				}));
#endif
				break;
			}
		}
	}
	return obj;
}


MsgPack
DatabaseWAL::repr_metadata(std::string_view serialised_metadata, bool unserialised)
{
	L_CALL("DatabaseWAL::repr_metadata(<serialised_document>)");

	if (!unserialised) {
		return MsgPack(serialised_metadata);
	}

	return MsgPack::unserialise(serialised_metadata);
}



MsgPack
DatabaseWAL::repr_line(std::string_view line, bool unserialised)
{
	L_CALL("DatabaseWAL::repr_line(<line>)");

	const char *p = line.data();
	const char *p_end = p + line.size();

	MsgPack repr;

	repr["revision"] = unserialise_length(&p, p_end);

	auto type = static_cast<Type>(unserialise_length(&p, p_end));

	auto data = decompress_lz4(std::string_view(p, p_end - p));

	size_t size;

	p = data.data();
	p_end = p + data.size();

	switch (type) {
		case Type::ADD_DOCUMENT:
			repr["op"] = "ADD_DOCUMENT";
			repr["document"] = repr_document(data, unserialised);
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
			repr["document"] = repr_document(std::string_view(p, p_end - p), unserialised);
			break;
		case Type::REPLACE_DOCUMENT_TERM:
			repr["op"] = "REPLACE_DOCUMENT_TERM";
			size = unserialise_length(&p, p_end, true);
			repr["term"] = std::string(p, size);
			repr["document"] = repr_document(std::string_view(p + size, p_end - p - size), unserialised);
			break;
		case Type::DELETE_DOCUMENT:
			repr["op"] = "DELETE_DOCUMENT";
			repr["docid"] = unserialise_length(&p, p_end);
			break;
		case Type::SET_METADATA:
			repr["op"] = "SET_METADATA";
			size = unserialise_length(&p, p_end, true);
			repr["key"] = std::string(p, size);
			repr["data"] = repr_metadata(std::string_view(p + size, p_end - p - size), unserialised);
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
DatabaseWAL::repr(Xapian::rev start_revision, Xapian::rev end_revision, bool unserialised)
{
	L_CALL("DatabaseWAL::repr(%llu, ...)", start_revision);

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, start_revision, end_revision);

	if (volumes.first > start_revision) {
		start_revision = volumes.first;
	}

	MsgPack repr(MsgPack::Type::ARRAY);

	bool end = false;
	Xapian::rev end_rev;
	for (end_rev = volumes.first; end_rev <= volumes.second && !end; ++end_rev) {
		try {
			open(string::format(WAL_STORAGE_PATH "%llu", end_rev), STORAGE_OPEN);
		} catch (const StorageIOError& exc) {
			L_WARNING("wal.%llu cannot be opened: %s", end_rev, exc.get_context());
			continue;
		} catch (const StorageCorruptVolume& exc) {
			L_WARNING("wal.%llu is corrupt: %s", end_rev, exc.get_context());
			continue;
		}

		Xapian::rev file_rev, begin_rev;
		file_rev = begin_rev = end_rev;
		if (file_rev != header.head.revision) {
			L_WARNING("wal.%llu has incorrect revision!", file_rev);
			continue;
		}

		auto high_slot = highest_valid_slot();
		if (high_slot == static_cast<uint32_t>(-1)) {
			if (start_revision != file_rev) {
				L_WARNING("wal.%llu has no valid slots!", file_rev);
			}
			continue;
		}

		if (file_rev == volumes.second) {
			end = true;  // Avoid reenter to the loop with the high valid slot of the highest revision
		}

		end_rev = file_rev + high_slot;
		if (end_rev < start_revision) {
			continue;
		}

		uint32_t start_off;
		if (file_rev == volumes.first) {
			assert(start_revision >= file_rev);
			if (start_revision == file_rev) {
				// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
				// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
				begin_rev = file_rev;
				start_off = STORAGE_START_BLOCK_OFFSET;
			} else {
				auto slot = start_revision - file_rev - 1;
				begin_rev = file_rev + slot;
				start_off = header.slot[slot];
			}
		} else {
			start_off = STORAGE_START_BLOCK_OFFSET;
		}

		auto end_off = header.slot[high_slot];
		if (start_off < end_off) {
			L_INFO("Read and repr operations WAL file (wal.%llu) from [%llu..%llu] revision", file_rev, begin_rev, end_rev);
		}

		seek(start_off);
		try {
			while (true) {
				std::string line = read(end_off);
				repr.push_back(repr_line(line, unserialised));
			}
		} catch (const StorageEOF& exc) { }
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
DatabaseWAL::execute(std::string_view line, bool wal_, bool send_update, bool unsafe)
{
	L_CALL("DatabaseWAL::execute(<line>, %s, %s, %s)", wal_ ? "true" : "false", send_update ? "true" : "false", unsafe ? "true" : "false");

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
		if (!unsafe) {
			THROW(StorageCorruptVolume, "WAL revision mismatch!");
		}
		// L_WARNING("WAL revision mismatch!");
	}

	auto type = static_cast<Type>(unserialise_length(&p, p_end));

	auto data = decompress_lz4(std::string_view(p, p_end - p));

	Xapian::docid did;
	Xapian::Document doc;
	Xapian::termcount freq;
	std::string term;
	size_t size;

	p = data.data();
	p_end = p + data.size();

	bool modified = true;

	switch (type) {
		case Type::ADD_DOCUMENT:
			doc = Xapian::Document::unserialise(data);
			database->add_document(doc, false, wal_);
			break;
		case Type::DELETE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			database->delete_document_term(term, false, wal_);
			break;
		case Type::COMMIT:
			database->commit(wal_, send_update);
			modified = false;
			break;
		case Type::REPLACE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			doc = Xapian::Document::unserialise(std::string(p, p_end - p));
			database->replace_document(did, doc, false, wal_);
			break;
		case Type::REPLACE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			doc = Xapian::Document::unserialise(std::string(p + size, p_end - p - size));
			database->replace_document_term(term, doc, false, wal_);
			break;
		case Type::DELETE_DOCUMENT:
			try {
				did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
				database->delete_document(did, false, wal_);
			} catch (const NotFoundError& exc) {
				if (!unsafe) {
					throw;
				}
				L_WARNING("Error during DELETE_DOCUMENT: %s", exc.get_message());
			}
			break;
		case Type::SET_METADATA:
			size = unserialise_length(&p, p_end, true);
			database->set_metadata(std::string(p, size), std::string(p + size, p_end - p - size), false, wal_);
			break;
		case Type::ADD_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database->add_spelling(std::string(p, p_end - p), freq, false, wal_);
			break;
		case Type::REMOVE_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database->remove_spelling(std::string(p, p_end - p), freq, false, wal_);
			break;
		default:
			THROW(Error, "Invalid WAL message!");
	}

	return modified;
}


bool
DatabaseWAL::init_database()
{
	L_CALL("DatabaseWAL::init_database()");

	static const std::array<std::string, 2> iamglass({{
		std::string("\x0f\x0d\x58\x61\x70\x69\x61\x6e\x20\x47\x6c\x61\x73\x73\x04\x6e", 16),
		std::string("\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00\x03\x00"
					"\x04\x04\x00\x00\x03\x00\x04\x00\x00\x00\x03\x00\x04\x04\x00\x00"
					"\x03\x00\x04\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00", 45)
	}});

	auto filename = base_path + "iamglass";
	if (exists(filename)) {
		return true;
	}

	validate_uuid = false;

	try {
		open(string::format(WAL_STORAGE_PATH "%llu", 0), STORAGE_OPEN);
	} catch (const StorageIOError&) {
		return true;
	}

	UUID header_uuid(header.head.uuid, UUID_LENGTH);
	const auto& bytes = header_uuid.get_bytes();
	std::string uuid_str(bytes.begin(), bytes.end());

	int fd = io::open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL);
	if unlikely(fd == -1) {
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
	if unlikely(fd == -1) {
		L_ERR("ERROR: opening file. %s\n", filename);
		return false;
	}
	io::close(fd);

	return true;
}


void
DatabaseWAL::write_line(Type type, std::string_view data, bool send_update)
{
	L_CALL("DatabaseWAL::write_line(Type::%s, <data>, %s)", names[toUType(type)], send_update ? "true" : "false");
	try {
		assert(database->flags & DB_WRITABLE);
		assert(!(database->flags & DB_NOWAL));

		auto endpoint = database->endpoints[0];
		assert(endpoint.is_local());

		std::string uuid;
		auto revision = database->get_revision();
		if (type == Type::COMMIT) {
			--revision;
			uuid = database->db->get_uuid();
		}

		std::string line;
		line.append(serialise_length(revision));
		line.append(serialise_length(toUType(type)));
		line.append(compress_lz4(data));

		L_DATABASE_WAL("%s on %s: '%s'", names[toUType(type)], endpoint.path, repr(line, quote));

		assert(revision >= header.head.revision);
		uint32_t slot = revision - header.head.revision;

		if (slot >= WAL_SLOTS) {
			open(string::format(WAL_STORAGE_PATH "%llu", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			assert(revision >= header.head.revision);
			slot = revision - header.head.revision;
		}

		assert(slot >= 0 && slot < WAL_SLOTS);

		try {
			write(line.data(), line.size());
		} catch (const StorageClosedError&) {
			auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision, revision);
			open(string::format(WAL_STORAGE_PATH "%llu", (volumes.first <= volumes.second) ? volumes.second : revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			write(line.data(), line.size());
		}

		header.slot[slot] = header.head.offset; /* Beginning of the next revision */

		if (type == Type::COMMIT) {
			if (slot + 1 >= WAL_SLOTS) {
				open(string::format(WAL_STORAGE_PATH "%llu", revision + 1), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			} else {
				header.slot[slot + 1] = header.slot[slot];
			}
		}

		commit();

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			// On COMMIT, add to updated databases queue so replicators do their job
			if (send_update) {
				XapiandManager::manager->database_pool.updated_databases.push(DatabaseUpdate(endpoint, uuid, revision + 1));
			}
		}
#endif

	} catch (const StorageException& exc) {
		L_ERR("WAL ERROR in %s: %s", ::repr(database->endpoints.to_string()), exc.get_message());
		database->wal.reset();
		Metrics::metrics()
			.xapiand_wal_errors
			.Increment();
	}
}


void
DatabaseWAL::write_add_document(const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_add_document(<doc>)");

	auto line = doc.serialise();
	write_line(Type::ADD_DOCUMENT, line, false);
}


void
DatabaseWAL::write_delete_document_term(std::string_view term)
{
	L_CALL("DatabaseWAL::write_delete_document_term(<term>)");

	auto line = serialise_string(term);
	write_line(Type::DELETE_DOCUMENT_TERM, line, false);
}


void
DatabaseWAL::write_remove_spelling(std::string_view word, Xapian::termcount freqdec)
{
       L_CALL("DatabaseWAL::write_remove_spelling(...)");

       auto line = serialise_length(freqdec);
       line.append(word);
       write_line(Type::REMOVE_SPELLING, line, false);
}


void
DatabaseWAL::write_commit(bool send_update)
{
	L_CALL("DatabaseWAL::write_commit(%s)", send_update ? "true" : "false");

	write_line(Type::COMMIT, "", send_update);
}


void
DatabaseWAL::write_replace_document(Xapian::docid did, const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_replace_document(...)");

	auto line = serialise_length(did);
	line.append(doc.serialise());
	write_line(Type::REPLACE_DOCUMENT, line, false);
}


void
DatabaseWAL::write_replace_document_term(std::string_view term, const Xapian::Document& doc)
{
	L_CALL("DatabaseWAL::write_replace_document_term(...)");

	auto line = serialise_string(term);
	line.append(doc.serialise());
	write_line(Type::REPLACE_DOCUMENT_TERM, line, false);
}


void
DatabaseWAL::write_delete_document(Xapian::docid did)
{
	L_CALL("DatabaseWAL::write_delete_document(<did>)");

	auto line = serialise_length(did);
	write_line(Type::DELETE_DOCUMENT, line, false);
}


void
DatabaseWAL::write_set_metadata(std::string_view key, std::string_view val)
{
	L_CALL("DatabaseWAL::write_set_metadata(...)");

	auto line = serialise_string(key);
	line.append(val);
	write_line(Type::SET_METADATA, line, false);
}


void
DatabaseWAL::write_add_spelling(std::string_view word, Xapian::termcount freqinc)
{
	L_CALL("DatabaseWAL::write_add_spelling(...)");

	auto line = serialise_length(freqinc);
	line.append(word);
	write_line(Type::ADD_SPELLING, line, false);
}


std::pair<bool, unsigned long long>
DatabaseWAL::has_revision(Xapian::rev revision)
{
	L_CALL("DatabaseWAL::has_revision(...)");

	unsigned long long volume;
	auto volumes = get_volumes_range(WAL_STORAGE_PATH, 0, revision);
	if (volumes.second == std::numeric_limits<unsigned long long>::max()) {
		volume = 0;
	} else if (volumes.second + WAL_SLOTS <= revision) {
		return std::make_pair(false, std::numeric_limits<unsigned long long>::max());
	} else {
		volume = volumes.second;
	}
	open(string::format(WAL_STORAGE_PATH "%u", volume), STORAGE_OPEN);

	return std::make_pair(header.slot[revision - volume] != 0, volume);
}

DatabaseWAL::iterator
DatabaseWAL::find(Xapian::rev revision)
{
	L_CALL("DatabaseWAL::find(...)");

	auto volume_traits = has_revision(revision).second;
	open(string::format(WAL_STORAGE_PATH "%u", volume_traits), STORAGE_OPEN);

	auto high_slot = highest_valid_slot();

	assert(0 <= high_slot && high_slot < WAL_SLOTS);

	auto end_off = header.slot[high_slot];

	assert(volume_traits <= revision);

	uint32_t start_off;
	if (revision == volume_traits) {
		start_off = STORAGE_START_BLOCK_OFFSET;
	} else {
		start_off = header.slot[(revision - volume_traits) - 1];
	}
	seek(start_off); /* putting us in revision position for get wal lines */
	return iterator(this, get_current_line(end_off), end_off);
}


std::pair<Xapian::rev, std::string>
DatabaseWAL::get_current_line(uint32_t end_off)
{
	L_CALL("DatabaseWAL::get_current_line(...)");

	try {
		std::string line = read(end_off);
		const char* p = line.data();
		const char *p_end = p + line.size();
		auto revision = static_cast<Xapian::rev>(unserialise_length(&p, p_end));
		return std::make_pair(revision, line);
	}  catch (const StorageEOF& exc) { }

	return std::make_pair(std::numeric_limits<Xapian::rev>::max() - 1, "");
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

	head.magic = STORAGE_MAGIC;
	strncpy(head.uuid, database->get_uuid().to_string().c_str(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
}


void
DataHeader::validate(void* param, void* /*unused*/)
{
	if (head.magic != STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	const auto* database = static_cast<const Database*>(param);
	if (UUID(head.uuid) != database->get_uuid()) {
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
	  modified(false),
	  transaction(false),
	  reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0),
	  incomplete(false),
	  closed(false)
{
	reopen();

	queue_->inc_count();

	L_OBJ("CREATED DATABASE!");
}


Database::~Database()
{
	if ((flags & DB_WRITABLE) != 0) {
		if (dbs[0].second) {
			// Commit only local writable databases
			commit();
		}
	}

	if (auto queue = weak_queue.lock()) {
		queue->dec_count();
	}

	L_OBJ("DELETED DATABASE!");
}


void
Database::reopen_writable()
{
	////////////////////////////////////////////////////////////////
	// __        __    _ _        _     _        ____  ____
	// \ \      / / __(_) |_ __ _| |__ | | ___  |  _ \| __ )
	//  \ \ /\ / / '__| | __/ _` | '_ \| |/ _ \ | | | |  _ \.
	//   \ V  V /| |  | | || (_| | |_) | |  __/ | |_| | |_) |
	//    \_/\_/ |_|  |_|\__\__,_|_.__/|_|\___| |____/|____/
	//

	incomplete = false;
	dbs.clear();
#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif /* XAPIAND_DATA_STORAGE */

	auto endpoints_size = endpoints.size();
	ignore_unused(endpoints_size);
	assert(endpoints_size == 1);

	db = std::make_unique<Xapian::WritableDatabase>();

	const auto& endpoint = endpoints[0];
	Xapian::WritableDatabase wdb;
	bool local = false;
	int _flags = (flags & DB_SPAWN) != 0 ? Xapian::DB_CREATE_OR_OPEN | XAPIAN_SYNC_MODE : Xapian::DB_OPEN | XAPIAN_SYNC_MODE;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		int port = (endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : endpoint.port;
		wdb = Xapian::Remote::open_writable(endpoint.host, port, 0, 10000, _flags, endpoint.path);
		// Writable remote databases do not have a local fallback
	}
	else
#endif /* XAPIAND_CLUSTERING */
	{
#ifdef XAPIAND_DATABASE_WAL
		DatabaseWAL tmp_wal(endpoint.path, this);
		tmp_wal.init_database();
#endif
		if (flags & DB_SPAWN) {
			build_path_index(endpoint.path);
		}
		try {
			wdb = Xapian::WritableDatabase(endpoint.path, _flags);
		} catch (const Xapian::DatabaseOpeningError&) {
			if (!exists(endpoint.path + "/iamglass")) {
				if ((flags & DB_SPAWN) == 0) {
					THROW(NotFoundError, "Database not found: %s", repr(endpoint.to_string()));
				}
				wdb = Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE_OR_OVERWRITE | XAPIAN_SYNC_MODE);
			}
			throw;
		}
		local = true;
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
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		} else {
			writable_storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE));
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		}
	} else {
		writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		storages.push_back(std::unique_ptr<DataStorage>(nullptr));
	}
#endif /* XAPIAND_DATA_STORAGE */

#ifdef XAPIAND_DATABASE_WAL
	/* If reopen_revision is not available Wal work as a log for the operations */
	if (local && ((flags & DB_NOWAL) == 0)) {
		// WAL required on a local writable database, open it.
		wal = std::make_unique<DatabaseWAL>(endpoint.path, this);
		if (wal->open_current(true)) {
			if (auto queue = weak_queue.lock()) {
				modified = true;
			}
		}
	}
#endif /* XAPIAND_DATABASE_WAL */
	// Ends Writable DB
	////////////////////////////////////////////////////////////////
}

void
Database::reopen_readable()
{
	////////////////////////////////////////////////////////////////
	//  ____                _       _     _        ____  ____
	// |  _ \ ___  __ _  __| | __ _| |__ | | ___  |  _ \| __ )
	// | |_) / _ \/ _` |/ _` |/ _` | '_ \| |/ _ \ | | | |  _ \.
	// |  _ <  __/ (_| | (_| | (_| | |_) | |  __/ | |_| | |_) |
	// |_| \_\___|\__,_|\__,_|\__,_|_.__/|_|\___| |____/|____/
	//

	incomplete = false;
	dbs.clear();
#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif /* XAPIAND_DATA_STORAGE */

	auto endpoints_size = endpoints.size();
	ignore_unused(endpoints_size);
	assert(endpoints_size >= 1);

	db = std::make_unique<Xapian::Database>();

	size_t failures = 0;

	for (const auto& endpoint : endpoints) {
		Xapian::Database rdb;
		bool local = false;
#ifdef XAPIAND_CLUSTERING
		int _flags = (flags & DB_SPAWN) ? Xapian::DB_CREATE_OR_OPEN : Xapian::DB_OPEN;
		if (!endpoint.is_local()) {
			int port = (endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : endpoint.port;
			rdb = Xapian::Remote::open(endpoint.host, port, 10000, 10000, _flags, endpoint.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
			try {
				Xapian::Database tmp = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
				if (
					tmp.get_uuid() == rdb.get_uuid() &&
					tmp.get_revision() == rdb.get_revision()
				) {
					L_DATABASE("Endpoint %s fallback to local database!", repr(endpoint.to_string()));
					// Handle remote endpoints and figure out if the endpoint is a local database
					rdb = Xapian::Database(endpoint.path, _flags);
					local = true;
				} else {
					try {
						// If remote is master (it should be), try triggering replication
						XapiandManager::manager->trigger_replication(endpoint, Endpoint{endpoint.path});
						incomplete = true;
					} catch (...) { }
				}
			} catch (const Xapian::DatabaseOpeningError& exc) { }
#endif /* XAPIAN_LOCAL_DB_FALLBACK */
		}
		else
#endif /* XAPIAND_CLUSTERING */
		{
#ifdef XAPIAND_DATABASE_WAL
			DatabaseWAL tmp_wal(endpoint.path, this);
			tmp_wal.init_database();
#endif
			try {
				rdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
				local = true;
			} catch (const Xapian::DatabaseOpeningError& exc) {
				if (!exists(endpoint.path + "/iamglass")) {
					++failures;
					if ((flags & DB_SPAWN) == 0)  {
						if (endpoints.size() == failures) {
							db.reset();
							THROW(NotFoundError, "Database not found: %s", repr(endpoint.to_string()));
						}
						incomplete = true;
					} else {
						{
							build_path_index(endpoint.path);
							Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE_OR_OVERWRITE | XAPIAN_SYNC_MODE);
						}
						rdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
						local = true;
					}
				}
				throw;
			}
		}

		db->add_database(rdb);
		dbs.emplace_back(rdb, local);

#ifdef XAPIAND_DATA_STORAGE
		if (local) {
			// WAL required on a local database, open it.
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		} else {
			storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		}
#endif /* XAPIAND_DATA_STORAGE */
	}
	assert(dbs.size() == endpoints_size);
	// Ends Readable DB
	////////////////////////////////////////////////////////////////
}

bool
Database::reopen()
{
	L_CALL("Database::reopen()");

	reopen_time = std::chrono::system_clock::now();

	if (db) {
		if (!incomplete) {
			// Try to reopen
			try {
				bool ret = db->reopen();
				L_DATABASE_WRAP("Reopen done (took %s) [1]", string::from_delta(reopen_time, std::chrono::system_clock::now()));
				return ret;
			} catch (const Xapian::DatabaseOpeningError& exc) {
				L_EXC("ERROR: %s", exc.get_description());
			} catch (const Xapian::Error& exc) {
				L_EXC("ERROR: %s", exc.get_description());
				throw;
			}
		}

		db->close();
		db.reset();
	}

	if ((flags & DB_WRITABLE) != 0) {
		reopen_writable();
	} else {
		reopen_readable();
	}

	L_DATABASE_WRAP("Reopen done (took %s) [1]", string::from_delta(reopen_time, std::chrono::system_clock::now()));

	return true;
}


UUID
Database::get_uuid() const
{
	L_CALL("Database::get_uuid");

	return UUID(db->get_uuid());
}


Xapian::rev
Database::get_revision() const
{
	L_CALL("Database::get_revision()");

#if HAVE_XAPIAN_DATABASE_GET_REVISION
	return db->get_revision();
#else
	return 0;
#endif
}


bool
Database::commit(bool wal_, bool send_update)
{
	L_CALL("Database::commit(%s)", wal_ ? "true" : "false");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	if (!modified) {
		L_DATABASE_WRAP("Do not commit, because there are not changes");
		return false;
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE_WRAP("Commit: t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif /* XAPIAND_DATA_STORAGE */
			if (transaction) {
				wdb->commit_transaction();
				wdb->begin_transaction();
			} else {
				wdb->commit();
			}
			modified = false;
			const auto& db_pair = dbs[0]; // writable database, only one db in dbs
			bool local = db_pair.second;
			if (local) {
				if (auto queue = weak_queue.lock()) {
					queue->local_revision = db_pair.first.get_revision();
				}
			}
			break;
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

#if XAPIAND_DATABASE_WAL
	if (wal_ && wal) { wal->write_commit(send_update); }
#else
	ignore_unused(wal_);
#endif

	return true;
}


void
Database::close()
{
	L_CALL("Database::close()");

	closed = true;
	db->close();
	modified = false;
}


void
Database::begin_transaction(bool flushed)
{
	L_CALL("Database::begin_transaction(%s)", flushed ? "true" : "false");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
	wdb->begin_transaction(flushed);
	transaction = true;
}


void
Database::commit_transaction()
{
	L_CALL("Database::commit_transaction()");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
	wdb->commit_transaction();
	transaction = false;
}


void
Database::cancel_transaction()
{
	L_CALL("Database::cancel_transaction()");

	if ((flags & DB_WRITABLE) == 0) {
		THROW(Error, "database is read-only");
	}

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
	wdb->cancel_transaction();
	transaction = false;
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
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
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
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { THROW(Error, "Problem communicating with the remote database: %s", exc.get_description()); }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { THROW(Error, "Problem opening the database: %s", exc.get_description()); }
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
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
								storage->volume = storage->get_volumes_range(DATA_STORAGE_PATH).second;
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
			modified = true;
			break;
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
			modified = true;
			break;
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
			modified = true;
			break;
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
			modified = true;
			break;
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
			modified = true;
			break;
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
			modified = true;
			break;
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
DatabaseQueue::DatabaseQueue(const Endpoints& endpoints_, Args&&... args)
	: Queue(std::forward<Args>(args)...),
	  locked(false),
	  count(0),
	  endpoints(endpoints_) {
	L_OBJ("CREATED DATABASE QUEUE FOR %s!", repr(endpoints.to_string()));
}


DatabaseQueue::~DatabaseQueue()
{
	if (size() != count) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
	}

	L_OBJ("DELETED DATABASE QUEUE!");
}


size_t
DatabaseQueue::inc_count()
{
	L_CALL("DatabaseQueue::inc_count()");

	std::lock_guard<std::mutex> lk(_state->_mutex);

	return ++count;
}


size_t
DatabaseQueue::dec_count()
{
	L_CALL("DatabaseQueue::dec_count()");

	std::lock_guard<std::mutex> lk(_state->_mutex);

	if (count == 0) {
		L_CRIT("Inconsistency in the number of databases in queue");
		sig_exit(-EX_SOFTWARE);
		return count;
	}

	return --count;
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
	L_CALL("DatabasesLRU::get(%zx)", hash);

	auto it = find(hash);
	if (it != end()) {
		return it->second;
	}
	return nullptr;
}


std::pair<std::shared_ptr<DatabaseQueue>, bool>
DatabasesLRU::get(size_t hash, const Endpoints& endpoints)
{
	L_CALL("DatabasesLRU::get(%zx, %s)", hash, repr(endpoints.to_string()));

	const auto now = std::chrono::system_clock::now();

	const auto on_get = [now](std::shared_ptr<DatabaseQueue>& val) {
		val->renew_time = now;
		return lru::GetAction::renew;
	};

	auto it = find_and(on_get, hash);
	if (it != end()) {
		return std::make_pair(it->second, false);
	}

	const auto on_drop = [now](std::shared_ptr<DatabaseQueue>& val, ssize_t size, ssize_t max_size) {
		if (val->locked) {
			val->renew_time = now;
			L_DATABASE("Renew locked queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::renew;
		}
		if (val->size() < val->count) {
			val->renew_time = now;
			L_DATABASE("Renew occupied queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::renew;
		}
		if (size > max_size) {
			if (val->renew_time < now - 60s) {
				L_DATABASE("Evict queue from full LRU: %s", repr(val->endpoints.to_string()));
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (val->renew_time < now - 3600s) {
			L_DATABASE("Evict queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::evict;
		}
		L_DATABASE("Stop at queue: %s", repr(val->endpoints.to_string()));
		return lru::DropAction::stop;
	};

	auto emplaced = emplace_and(on_drop, hash, DatabaseQueue::make_shared(endpoints, _queue_state));
	return std::make_pair(emplaced.first->second, emplaced.second);
}


void
DatabasesLRU::cleanup(const std::chrono::time_point<std::chrono::system_clock>& now)
{
	L_CALL("DatabasesLRU::cleanup()");

	const auto on_drop = [now](std::shared_ptr<DatabaseQueue>& val, ssize_t size, ssize_t max_size) {
		if (val->locked) {
			L_DATABASE("Leave locked queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (val->size() < val->count) {
			L_DATABASE("Leave occupied queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (size > max_size) {
			if (val->renew_time < now - 60s) {
				L_DATABASE("Evict queue from full LRU: %s", repr(val->endpoints.to_string()));
				return lru::DropAction::evict;
			}
			L_DATABASE("Leave recently used queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::leave;
		}
		if (val->renew_time < now - 3600s) {
			L_DATABASE("Evict queue: %s", repr(val->endpoints.to_string()));
			return lru::DropAction::evict;
		}
		L_DATABASE("Stop at queue: %s", repr(val->endpoints.to_string()));
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
	  locks(0),
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
DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags)
{
	L_CALL("DatabasePool::checkout(%s, 0x%02x (%s))", repr(endpoints.to_string()), flags, [&flags]() {
		std::vector<std::string> values;
		if (flags == DB_OPEN) values.push_back("DB_OPEN");
		if ((flags & DB_WRITABLE) == DB_WRITABLE) values.push_back("DB_WRITABLE");
		if ((flags & DB_SPAWN) == DB_SPAWN) values.push_back("DB_SPAWN");
		if ((flags & DB_EXCLUSIVE) == DB_EXCLUSIVE) values.push_back("DB_EXCLUSIVE");
		if ((flags & DB_NOWAL) == DB_NOWAL) values.push_back("DB_NOWAL");
		if ((flags & DB_NOSTORAGE) == DB_NOSTORAGE) values.push_back("DB_NOSTORAGE");
		return string::join(values, " | ");
	}());

	bool db_writable = (flags & DB_WRITABLE) != 0;
	bool db_exclusive = (flags & DB_EXCLUSIVE) != 0;

	L_DATABASE_BEGIN("++ CHECKING OUT DB [%s]: %s ...", db_writable ? "WR" : "RO", repr(endpoints.to_string()));

	assert(!database);

	if (db_writable && endpoints.size() != 1) {
		L_ERR("ERROR: Expecting exactly one database, %d requested: %s", endpoints.size(), repr(endpoints.to_string()));
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout database: %s (only one)", repr(endpoints.to_string()));
	}

	if (db_exclusive && !db_writable) {
		L_ERR("ERROR: Exclusive access can be granted only for writable databases");
		THROW(CheckoutErrorBadEndpoint, "Cannot checkout database: %s (non-exclusive)", repr(endpoints.to_string()));
	}

	if (!finished) {
		size_t hash = endpoints.hash();

		std::unique_lock<std::mutex> lk(qmtx);

		auto queue_pair = db_writable ? writable_databases.get(hash, endpoints) : databases.get(hash, endpoints);

		auto queue = queue_pair.first;

		if (queue_pair.second) {
			// Queue was just created, add as used queue to the endpoint -> queues map
			for (const auto& endpoint : endpoints) {
				auto endpoint_hash = endpoint.hash();
				auto& queues_set = queues[endpoint_hash];
				queues_set.insert(queue);
			}
		}

		int retries = 10;
		while (true) {
			if (!queue->pop(database, 0)) {
				// Increment so other threads don't delete the queue
				auto count = queue->inc_count();
				try {
					lk.unlock();
					if (
						(db_writable && count > 1) ||
						(!db_writable && count > 1000)
					) {
						// Lock until a database is available if it can't get one.
						if (!queue->pop(database, DB_TIMEOUT)) {
							THROW(TimeOutError, "Database is not available");
						}
					} else {
						database = std::make_shared<Database>(queue, endpoints, flags);
					}
					lk.lock();
				} catch (const Xapian::DatabaseOpeningError& exc) {
					lk.lock();
					L_DATABASE("ERROR: %s", exc.get_description());
				} catch (...) {
					lk.lock();
					database.reset();
					count = queue->dec_count();
					if (count == 0) {
						// There is a error, the queue ended up being empty, remove it
						if (db_writable) {
							writable_databases.erase(hash);
						} else {
							databases.erase(hash);
						}
						// Queue was just erased, remove as used queue to the endpoint -> queues map
						for (const auto& endpoint : endpoints) {
							auto endpoint_hash = endpoint.hash();
							auto& queues_set = queues[endpoint_hash];
							queues_set.erase(queue);
							if (queues_set.empty()) {
								queues.erase(endpoint_hash);
							}
						}
					}
					throw;
				}
				// Decrement, count should have been already incremented if Database was created
				queue->dec_count();
			}

			if (!database || !database->db) {
				database.reset();
				if (queue->count == 0) {
					// There is a error, the queue ended up being empty, remove it
					if (db_writable) {
						writable_databases.erase(hash);
					} else {
						databases.erase(hash);
					}
					// Queue was just erased, remove as used queue to the endpoint -> queues map
					for (const auto& endpoint : endpoints) {
						auto endpoint_hash = endpoint.hash();
						auto& queues_set = queues[endpoint_hash];
						queues_set.erase(queue);
						if (queues_set.empty()) {
							queues.erase(endpoint_hash);
						}
					}
				}
				L_DATABASE_END("!! FAILED CHECKOUT DB [%s]: %s", db_writable ? "WR" : "WR", repr(endpoints.to_string()));
				THROW(NotFoundError, "Database not found: %s", repr(endpoints.to_string()));
			}

			if (locks) {
				if (!db_writable) {
					auto has_locked_endpoints = [&]() -> std::shared_ptr<DatabaseQueue> {
						for (auto& e : database->endpoints) {
							auto wq = writable_databases.get(e.hash());
							if (wq && wq->locked) {
								return wq;
							}
						}
						return nullptr;
					};
					auto timeout_tp = std::chrono::system_clock::now() + std::chrono::seconds(DB_TIMEOUT);
					auto wq = has_locked_endpoints();
					if (wq) {
						database.reset();
						if (--retries == 0) {
							THROW(TimeOutError, "Database is not available");
						}
						do {
							if (wq->unlock_cond.wait_until(lk, timeout_tp) == std::cv_status::timeout) {
								if (has_locked_endpoints()) {
									THROW(TimeOutError, "Database is not available");
								}
								break;
							}
						} while ((wq = has_locked_endpoints()));
						continue;
					}
				} else if (db_exclusive) {
					assert(queue->locked == false);
					++locks;
					queue->locked = true;
					auto is_ready_to_lock = [&] {
						for (auto& q : queues[hash]) {
							if (q != queue && q->size() < q->count) {
								return false;
							}
						}
						return true;
					};
					auto timeout_tp = std::chrono::system_clock::now() + std::chrono::seconds(DB_TIMEOUT);
					if (!queue->exclusive_cond.wait_until(lk, timeout_tp, is_ready_to_lock)) {
						THROW(TimeOutError, "Database is not available");
					}
				}
			}

			break;
		};

	}

	if (!database) {
		L_DATABASE_END("!! FAILED CHECKOUT DB [%s]: %s", db_writable ? "WR" : "WR", repr(endpoints.to_string()));
		THROW(NotFoundError, "Database not found: %s", repr(endpoints.to_string()));
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
				bool local = db_pair.second;
				auto hash = endpoints[i].hash();
				if (local) {
					std::lock_guard<std::mutex> lk(qmtx);
					auto queue = writable_databases.get(hash);
					if (queue) {
						auto revision = queue->local_revision.load();
						if (revision != db_pair.first.get_revision()) {
							// Local writable database has changed revision.
							reopen = true;
							break;
						}
					}
				} else {
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

	L_DATABASE_END("++ CHECKED OUT DB [%s]: %s (rev:%llu)", db_writable ? "WR" : "WR", repr(endpoints.to_string()), database->reopen_revision);
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_CALL("DatabasePool::checkin(%s)", repr(database->to_string()));

	assert(database);
	auto& endpoints = database->endpoints;
	int flags = database->flags;

	L_DATABASE_BEGIN("-- CHECKING IN DB [%s]: %s ...", (flags & DB_WRITABLE) ? "WR" : "RO", repr(endpoints.to_string()));

	if (database->modified && !database->transaction) {
		DatabaseAutocommit::commit(database);
	}

	if (auto queue = database->weak_queue.lock()) {
		std::lock_guard<std::mutex> lk(qmtx);

		bool db_writable = (flags & DB_WRITABLE) != 0;

		if (locks) {
			if (db_writable) {
				if (queue->locked) {
					queue->locked = false;
					assert(locks > 0);
					--locks;
					queue->unlock_cond.notify_all();
				}
			} else {
				bool was_locked = false;
				for (auto& e : endpoints) {
					auto hash = e.hash();
					auto wq = writable_databases.get(hash);
					if (wq && wq->locked) {
						bool unlock = true;
						for (auto& q : queues[hash]) {
							if (q != wq && q->size() < q->count) {
								unlock = false;
								break;
							}
						}
						if (unlock) {
							wq->exclusive_cond.notify_one();
							was_locked = true;
						}
					}
				}
				if (was_locked) {
					database->close();
				}
			}
		}

		if (!database->closed) {
			if (!queue->push(database)) {
				_cleanup(true, true);
				queue->push(database);
			}
		}

		database.reset();

		if (queue->count < queue->size()) {
			L_CRIT("Inconsistency in the number of databases in queue");
			sig_exit(-EX_SOFTWARE);
		}

		if (queue->count == 0) {
			// The queue ended up being empty, remove it
			size_t hash = endpoints.hash();
			if (db_writable) {
				writable_databases.erase(hash);
			} else {
				databases.erase(hash);
			}
			// Queue was just erased, remove as used queue to the endpoint -> queues map
			for (const auto& endpoint : endpoints) {
				auto endpoint_hash = endpoint.hash();
				auto& queues_set = queues[endpoint_hash];
				queues_set.erase(queue);
				if (queues_set.empty()) {
					queues.erase(endpoint_hash);
				}
			}
		}
	} else {
		database.reset();
	}

	L_DATABASE_END("-- CHECKED IN DB [%s]: %s", (flags & DB_WRITABLE) ? "WR" : "RO", repr(endpoints.to_string()));
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


void
DatabasePool::switch_db(const std::string& tmp, const std::string& endpoint_path)
{
	L_CALL("DatabasePool::switch_db(%s, %s)", repr(tmp), repr(endpoint_path));

	try {
		std::shared_ptr<Database> database;
		checkout(database, Endpoints{Endpoint{endpoint_path}}, DB_WRITABLE | DB_EXCLUSIVE);
		database->close();
	} catch (const NotFoundError&) {
		// Database still doesn't exist, just move files
	}

	delete_files(endpoint_path);
	move_files(tmp, endpoint_path);
}


void
DatabasePool::_cleanup(bool writable, bool readable)
{
	L_CALL("DatabasePool::_cleanup(%s, %s)", writable ? "true" : "false", readable ? "true" : "false");

	const auto now = std::chrono::system_clock::now();

	if (writable) {
		if (cleanup_writable_time < now - 60s) {
			L_DATABASE("Cleanup writable databases...");
			writable_databases.cleanup(now);
			cleanup_writable_time = now;
		}
	}

	if (readable) {
		if (cleanup_readable_time < now - 60s) {
			L_DATABASE("Cleanup readable databases...");
			databases.cleanup(now);
			cleanup_readable_time = now;
		}
	}
}


void
DatabasePool::cleanup()
{
	L_CALL("DatabasePool::cleanup()");

	std::unique_lock<std::mutex> lk(qmtx);
	_cleanup(true, true);
}


DatabaseCount
DatabasePool::total_writable_databases()
{
	L_CALL("DatabasePool::total_wdatabases()");

	size_t db_count = 0;
	size_t db_queues = writable_databases.size();
	size_t db_enqueued = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & writable_database : writable_databases) {
		db_count += writable_database.second->count;
		db_enqueued += writable_database.second->size();
	}
	return {
		db_count,
		db_queues,
		db_enqueued,
	};
}


DatabaseCount
DatabasePool::total_readable_databases()
{
	L_CALL("DatabasePool::total_rdatabases()");

	size_t db_count = 0;
	size_t db_queues = databases.size();
	size_t db_enqueued = 0;
	std::lock_guard<std::mutex> lk(qmtx);
	for (auto & database : databases) {
		db_count += database.second->size();
		db_enqueued += database.second->size();
	}
	return {
		db_count,
		db_queues,
		db_enqueued,
	};
}
