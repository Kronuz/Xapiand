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

#include "database_wal.h"


//  ____        _        _                  __        ___    _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | |
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |___
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____|
//

#if XAPIAND_DATABASE_WAL

#include <array>                  // for std::array
#include <errno.h>                // for errno
#include <fcntl.h>                // for O_CREAT, O_WRONLY, O_EXCL
#include <limits>                 // for std::numeric_limits
#include <thread>                 // for std::thread
#include <utility>                // for std::make_pair

#include "cassert.hh"             // for assert

#include "database.h"             // for Database
#include "database_pool.h"        // for DatabasePool
#include "database_utils.h"       // for read_uuid
#include "server/discovery.h"     // for Discovery::signal_db_update
#include "exception.h"            // for THROW, Error
#include "fs.hh"                  // for exists
#include "ignore_unused.h"        // for ignore_unused
#include "io.hh"                  // for io::*
#include "log.h"                  // for L_OBJ, L_CALL, L_INFO, L_ERR, L_WARNING
#include "lru.h"                  // for lru::LRU
#include "lz4_compressor.h"       // for compress_lz4, decompress_lz4
#include "manager.h"              // for XapiandManager::manager
#include "metrics.h"              // for Metrics::metrics
#include "msgpack.h"              // for MsgPack
#include "opts.h"                 // for opts::*
#include "repr.hh"                // for repr
#include "string.hh"              // for string::format


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


#define WAL_STORAGE_PATH "wal."
#define WAL_SYNC_MODE     STORAGE_ASYNC_SYNC


void
WalHeader::init(void* param, void* /*unused*/)
{
	const auto* wal = static_cast<const DatabaseWAL*>(param);
	assert(wal);

	memcpy(&head.uuid[0], wal->get_uuid().get_bytes().data(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
	head.revision = wal->get_revision();
}


void
WalHeader::validate(void* param, void* /*unused*/)
{
	const auto* wal = static_cast<const DatabaseWAL*>(param);
	assert(wal);

	if (wal->validate_uuid) {
		UUID uuid(head.uuid);
		if (!wal->get_uuid().empty()) {
			if (uuid != wal->get_uuid()) {
				// Xapian under FreeBSD stores UUIDs in native order (could be little endian)
				if (uuid != wal->get_uuid_le()) {
					THROW(StorageCorruptVolume, "WAL UUID mismatch");
				}
			}
		}
	}
}


DatabaseWAL::DatabaseWAL(std::string_view base_path_)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(base_path_, this),
	  validate_uuid(true),
	  _revision(0),
	  _database(nullptr)
{
	L_OBJ("CREATED DATABASE WAL!");
}


const UUID&
DatabaseWAL::get_uuid() const
{
	if (_uuid.empty()) {
		if (_database) {
			_uuid = UUID(_database->get_uuid());
			_uuid_le = UUID(_uuid.get_bytes(), true);
		} else {
			std::array<unsigned char, 16> uuid_data;
			if (read_uuid(base_path, uuid_data) != -1) {
				_uuid = UUID(uuid_data);
				_uuid_le = UUID(uuid_data, true);
			}
		}
	}
	return _uuid;
}


const UUID&
DatabaseWAL::get_uuid_le() const
{
	if (_uuid_le.empty()) {
		get_uuid();
	}
	return _uuid_le;
}


Xapian::rev
DatabaseWAL::get_revision() const
{
	if (_database) {
		_revision = _database->get_revision();
	}
	return _revision;
}


DatabaseWAL::~DatabaseWAL()
{
	L_OBJ("DELETED DATABASE WAL!");
}


bool
DatabaseWAL::execute(Database& database, bool only_committed, bool unsafe)
{
	L_CALL("DatabaseWAL::execute(<database>, %s, %s)", only_committed ? "true" : "false", unsafe ? "true" : "false");

	_database = &database;

	Xapian::rev revision = database.reopen_revision;

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision);

	bool modified = false;

	try {
		bool end = false;
		Xapian::rev end_rev;
		for (end_rev = volumes.first; end_rev <= volumes.second && !end; ++end_rev) {
			try {
				open(string::format(WAL_STORAGE_PATH "%llu", end_rev), STORAGE_OPEN);
				if (header.head.revision != end_rev) {
					if (!unsafe) {
						THROW(StorageCorruptVolume, "Mismatch in WAL revision");
					}
					L_WARNING("Mismatch in WAL revision");
					header.head.revision = end_rev;
				}
			} catch (const StorageIOError& exc) {
				if (!unsafe) {
					throw;
				}
				L_WARNING("Cannot open WAL volume: %s", exc.get_context());
				continue;
			} catch (const StorageCorruptVolume& exc) {
				if (!unsafe) {
					throw;
				}
				L_WARNING("Corrupt WAL volume: %s", exc.get_context());
				continue;
			}

			Xapian::rev file_rev, begin_rev;
			file_rev = begin_rev = end_rev;

			auto high_slot = highest_valid_slot();
			if (high_slot == DatabaseWAL::max_slot) {
				if (revision != file_rev) {
					if (!unsafe) {
						THROW(StorageCorruptVolume, "No WAL slots");
					}
					L_WARNING("No WAL slots");
					continue;
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
				if (revision == file_rev) {
					// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
					// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
					begin_rev = file_rev;
					start_off = STORAGE_START_BLOCK_OFFSET;
				} else if (revision > file_rev) {
					auto slot = revision - file_rev - 1;
					begin_rev = file_rev + slot;
					start_off = header.slot[slot];
				} else {
					if (!unsafe) {
						THROW(StorageCorruptVolume, "Incorrect WAL revision");
					}
					L_WARNING("Incorrect WAL revision");
					continue;
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
					modified = execute_line(database, line, false, false, unsafe);
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
		}
	} catch (const StorageException& exc) {
		L_ERR("WAL ERROR in %s: %s", ::repr(database.endpoints.to_string()), exc.get_message());
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

	std::size_t size;

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
			if (header.head.revision != end_rev) {
				L_WARNING("wal.%llu has mismatch in WAL revision!", end_rev);
				header.head.revision = end_rev;
			}
		} catch (const StorageIOError& exc) {
			L_WARNING("wal.%llu cannot be opened: %s", end_rev, exc.get_context());
			continue;
		} catch (const StorageCorruptVolume& exc) {
			L_WARNING("wal.%llu is corrupt: %s", end_rev, exc.get_context());
			continue;
		}

		Xapian::rev file_rev, begin_rev;
		file_rev = begin_rev = end_rev;

		auto high_slot = highest_valid_slot();
		if (high_slot == DatabaseWAL::max_slot) {
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
			if (start_revision == file_rev) {
				// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
				// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
				begin_rev = file_rev;
				start_off = STORAGE_START_BLOCK_OFFSET;
			} else if (start_revision > file_rev) {
				auto slot = start_revision - file_rev - 1;
				begin_rev = file_rev + slot;
				start_off = header.slot[slot];
			} else {
				L_WARNING("wal.%llu has incorrect WAL revision!", file_rev);
				continue;
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

	uint32_t slot = DatabaseWAL::max_slot;
	for (uint32_t i = 0; i < WAL_SLOTS; ++i) {
		if (header.slot[i] == 0) {
			break;
		}
		slot = i;
	}
	return slot;
}


bool
DatabaseWAL::execute_line(Database& database, std::string_view line, bool wal_, bool send_update, bool unsafe)
{
	L_CALL("DatabaseWAL::execute_line(<database>, <line>, %s, %s, %s)", wal_ ? "true" : "false", send_update ? "true" : "false", unsafe ? "true" : "false");

	_database = &database;

	const char *p = line.data();
	const char *p_end = p + line.size();

	if ((database.flags & DB_WRITABLE) != DB_WRITABLE) {
		THROW(Error, "Database is read-only");
	}

	if (!database.endpoints[0].is_local()) {
		THROW(Error, "Can not execute WAL on a remote database!");
	}

	auto revision = unserialise_length(&p, p_end);
	auto db_revision = database.get_revision();

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
	std::size_t size;

	p = data.data();
	p_end = p + data.size();

	bool modified = true;

	switch (type) {
		case Type::ADD_DOCUMENT:
			doc = Xapian::Document::unserialise(data);
			database.add_document(doc, false, wal_);
			break;
		case Type::DELETE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			database.delete_document_term(term, false, wal_);
			break;
		case Type::COMMIT:
			database.commit(wal_, send_update);
			modified = false;
			break;
		case Type::REPLACE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			doc = Xapian::Document::unserialise(std::string(p, p_end - p));
			database.replace_document(did, doc, false, wal_);
			break;
		case Type::REPLACE_DOCUMENT_TERM:
			size = unserialise_length(&p, p_end, true);
			term = std::string(p, size);
			doc = Xapian::Document::unserialise(std::string(p + size, p_end - p - size));
			database.replace_document_term(term, doc, false, wal_);
			break;
		case Type::DELETE_DOCUMENT:
			try {
				did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
				database.delete_document(did, false, wal_);
			} catch (const NotFoundError& exc) {
				if (!unsafe) {
					throw;
				}
				L_WARNING("Error during DELETE_DOCUMENT: %s", exc.get_message());
			}
			break;
		case Type::SET_METADATA:
			size = unserialise_length(&p, p_end, true);
			database.set_metadata(std::string(p, size), std::string(p + size, p_end - p - size), false, wal_);
			break;
		case Type::ADD_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database.add_spelling(std::string(p, p_end - p), freq, false, wal_);
			break;
		case Type::REMOVE_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			database.remove_spelling(std::string(p, p_end - p), freq, false, wal_);
			break;
		default:
			THROW(Error, "Invalid WAL message!");
	}

	return modified;
}


bool
DatabaseWAL::init_database(Database& database)
{
	L_CALL("DatabaseWAL::init_database(<database>)");

	_database = &database;

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
		if (header.head.revision != 0) {
			THROW(StorageCorruptVolume, "Mismatch in WAL revision");
		}
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
DatabaseWAL::write_line(const std::string& path, const UUID& uuid, Xapian::rev revision, Type type, std::string_view data, bool send_update)
{
	L_CALL("DatabaseWAL::write_line(<path>, <uuid>, <revision>, Type::%s, <data>, %s)", names[toUType(type)], send_update ? "true" : "false");

	_uuid = uuid;
	_uuid_le = UUID(uuid.get_bytes(), true);
	_revision = revision;

	// COMMIT is one prior the current revision
	if (type == Type::COMMIT) {
		--revision;
	}

	try {
		std::string line;
		line.append(serialise_length(revision));
		line.append(serialise_length(toUType(type)));
		line.append(compress_lz4(data));

		L_DATABASE_WAL("%s on %s: '%s'", names[toUType(type)], path, repr(line, quote));

		if (header.head.revision > revision) {
			THROW(Error, "Invalid WAL revision (too old for volume)");
		}
		uint32_t slot = revision - header.head.revision;
		if (slot >= WAL_SLOTS) {
			// We need a new volume, the old one is full
			open(string::format(WAL_STORAGE_PATH "%llu", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			if (header.head.revision != revision) {
				THROW(StorageCorruptVolume, "Mismatch in WAL revision");
			}
			slot = revision - header.head.revision;
		}
		assert(slot >= 0 && slot < WAL_SLOTS);
		if (slot + 1 < WAL_SLOTS) {
			if (header.slot[slot + 1] != 0) {
				THROW(Error, "Invalid WAL revision (not latest)");
			}
		}

		try {
			write(line.data(), line.size());
		} catch (const StorageClosedError&) {
			auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision, revision);
			auto volume = (volumes.first <= volumes.second) ? volumes.second : revision;
			open(string::format(WAL_STORAGE_PATH "%llu", volume), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			if (header.head.revision != volume) {
				THROW(StorageCorruptVolume, "Mismatch in WAL revision");
			}
			write(line.data(), line.size());
		}

		header.slot[slot] = header.head.offset; // Beginning of the next revision

		if (type == Type::COMMIT) {
			if (slot + 1 < WAL_SLOTS) {
				header.slot[slot + 1] = header.slot[slot];
			} else {
				open(string::format(WAL_STORAGE_PATH "%llu", _revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
				if (header.head.revision != _revision) {
					THROW(StorageCorruptVolume, "Mismatch in WAL revision");
				}
			}
		}

		commit();

#ifdef XAPIAND_CLUSTERING
		if (!opts.solo) {
			// On COMMIT, let the updaters do their job
			if (send_update) {
				if (auto discovery = XapiandManager::manager->weak_discovery.lock()) {
					discovery->signal_db_update(path, uuid, _revision);
				}
			}
		}
#endif

	} catch (const StorageException& exc) {
		L_ERR("WAL ERROR in %s: %s", ::repr(path), exc.get_message());
		Metrics::metrics()
			.xapiand_wal_errors
			.Increment();
	}
}


std::pair<Xapian::rev, uint32_t>
DatabaseWAL::locate_revision(Xapian::rev revision)
{
	L_CALL("DatabaseWAL::locate_revision(...)");

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, 0, revision);
	if (volumes.first <= volumes.second && revision - volumes.second < WAL_SLOTS) {
		open(string::format(WAL_STORAGE_PATH "%u", volumes.second), STORAGE_OPEN);
		if (header.head.revision != volumes.second) {
			THROW(StorageCorruptVolume, "Mismatch in WAL revision");
		}
		if (header.head.revision <= revision) {
			auto high_slot = highest_valid_slot();
			if (high_slot != DatabaseWAL::max_slot) {
				uint32_t slot = revision - header.head.revision;
				if (slot <= high_slot) {
					return std::make_pair(header.head.revision, high_slot);
				}
			}
		}
	}
	return std::make_pair(DatabaseWAL::max_rev, DatabaseWAL::max_slot);
}


DatabaseWAL::iterator
DatabaseWAL::find(Xapian::rev revision)
{
	L_CALL("DatabaseWAL::find(...)");

	auto pair = locate_revision(revision);
	auto init_revision = pair.first;
	if (init_revision != DatabaseWAL::max_rev) {
		auto high_slot = pair.second;
		if (high_slot != DatabaseWAL::max_slot) {
			uint32_t start_off = init_revision < revision
				? header.slot[revision - init_revision - 1]
				: STORAGE_START_BLOCK_OFFSET;
			seek(start_off);  // move to revision offset, for get WAL lines
			uint32_t end_off = header.slot[high_slot];
			return iterator(this, get_current_line(end_off), end_off);
		}
	}
	return end();
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

	return std::make_pair(DatabaseWAL::max_rev, "");
}


//  ____        _        _                  __        ___    _ __        __    _ _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |\ \      / / __(_) |_ ___ _ __
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | | \ \ /\ / / '__| | __/ _ \ '__|
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |__\ V  V /| |  | | ||  __/ |
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____\_/\_/ |_|  |_|\__\___|_|
//

std::unique_ptr<DatabaseWALWriter> DatabaseWALWriter::_instance;

struct DatabaseWALWriterThread {
	DatabaseWALWriter* _wal_writer;
	size_t _idx;
	std::thread _thread;
	BlockingConcurrentQueue<std::function<void(DatabaseWALWriterThread&)>> _queue;

	lru::LRU<std::string, std::unique_ptr<DatabaseWAL>> lru;

	DatabaseWALWriterThread() noexcept :
		_wal_writer(nullptr),
		_idx(0)
	{}

	DatabaseWALWriterThread(size_t idx, DatabaseWALWriter* async_wal) noexcept :
		_wal_writer(async_wal),
		_idx(idx)
	{}

	DatabaseWALWriterThread(const DatabaseWALWriterThread&) = delete;

	DatabaseWALWriterThread& operator=(DatabaseWALWriterThread&& other) {
		_wal_writer = std::move(other._wal_writer);
		_idx = std::move(other._idx);
		_thread = std::move(other._thread);
		return *this;
	}

	void start() {
		_thread = std::thread(&DatabaseWALWriterThread::operator(), this);
	}

	bool joinable() const noexcept {
		return _thread.joinable();
	}

	void join() {
		try {
			if (_thread.joinable()) {
				_thread.join();
			}
		} catch (const std::system_error&) { }
	}

	void operator()() {
		set_thread_name(string::format(_wal_writer->_format, _idx));

		while (!_wal_writer->_finished.load(std::memory_order_acquire)) {
			std::function<void(DatabaseWALWriterThread&)> task;
			_queue.wait_dequeue(task);
			if likely(task != nullptr) {
				try {
					task(*this);
				} catch (const BaseException& exc) {
					L_EXC("Task died with an unhandled exception: %s", *exc.get_context() ? exc.get_context() : "Unkown BaseException!");
				} catch (const std::exception& exc) {
					L_EXC("Task died with an unhandled exception: %s", *exc.what() != 0 ? exc.what() : "Unkown std::exception!");
				} catch (...) {
					std::exception exc;
					L_EXC("WAL writer task died with an unhandled exception: Unkown exception!");
				}
			} else if (_wal_writer->_ending.load(std::memory_order_acquire)) {
				break;
			}
		}
	}

	void clear() {
		std::function<void(DatabaseWALWriterThread&)> task;
		while (_queue.try_dequeue(task)) {}
	}

	DatabaseWAL& wal(const std::string& path) {
		auto it = lru.find(path);
		if (it == lru.end()) {
			it = lru.emplace(path, std::make_unique<DatabaseWAL>(path)).first;
		}
		return *it->second;
	}
};


DatabaseWALWriter::DatabaseWALWriter(const char* format, std::size_t num_threads) :
	_threads(num_threads),
	_format(format),
	_ending(false),
	_finished(false)
{

	for (std::size_t idx = 0; idx < num_threads; ++idx) {
		_threads[idx] = DatabaseWALWriterThread(idx, this);
		_threads[idx].start();
	}
}


bool
DatabaseWALWriter::enqueue(const std::string& path, std::function<void(DatabaseWALWriterThread&)>&& func)
{
	static const std::hash<std::string> hasher;
	auto hash = hasher(path);
	auto& thread = _threads[hash % _threads.size()];
	return thread._queue.enqueue(std::move(func));
}


void
DatabaseWALWriter::start(std::size_t num_threads = 0)
{
	_instance = std::make_unique<DatabaseWALWriter>("X%02zu", num_threads);
}


void
DatabaseWALWriter::clear()
{
	assert(_instance);
	for (auto& _thread : _instance->_threads) {
		_thread.clear();
	}
}


void
DatabaseWALWriter::end()
{
	if (!_instance->_ending.exchange(true, std::memory_order_release)) {
		for (auto& _thread : _instance->_threads) {
			_thread._queue.enqueue(nullptr);
		}
	}
}


void
DatabaseWALWriter::finish(bool wait)
{
	if (!_instance->_finished.exchange(true, std::memory_order_release)) {
		for (auto& _thread : _instance->_threads) {
			_thread._queue.enqueue(nullptr);
		}
		if (wait) {
			join();
		}
	}
}


void
DatabaseWALWriter::join()
{
	assert(_instance);
	for (auto& _thread : _instance->_threads) {
		if (_thread.joinable()) {
			_thread.join();
		}
	}
}


std::size_t
DatabaseWALWriter::running_size()
{
	assert(_instance);
	return _instance->_threads.size();
}


void DatabaseWALWriter::write_add_document(const Database& database, const Xapian::Document& doc)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = doc.serialise();
		L_DATABASE("write_add_document {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::ADD_DOCUMENT, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_delete_document_term(const Database& database, const std::string& term)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_string(term);
		L_DATABASE("write_delete_document_term {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::DELETE_DOCUMENT_TERM, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_remove_spelling(const Database& database, const std::string& word, Xapian::termcount freqdec)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_length(freqdec);
		line.append(word);
		L_DATABASE("write_remove_spelling {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::REMOVE_SPELLING, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_commit(const Database& database, bool send_update)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		L_DATABASE("write_commit {path:%s, rev:%llu}", repr(path), revision);

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::COMMIT, "", send_update);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_replace_document(const Database& database, Xapian::docid did, const Xapian::Document& doc)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_length(did);
		line.append(doc.serialise());
		L_DATABASE("write_replace_document {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::REPLACE_DOCUMENT, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_replace_document_term(const Database& database, const std::string& term, const Xapian::Document& doc)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_string(term);
		line.append(doc.serialise());
		L_DATABASE("write_replace_document_term {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::REPLACE_DOCUMENT_TERM, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_delete_document(const Database& database, Xapian::docid did)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_length(did);
		L_DATABASE("write_delete_document {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::DELETE_DOCUMENT, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_set_metadata(const Database& database, const std::string& key, const std::string& val)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_string(key);
		line.append(val);
		L_DATABASE("write_set_metadata {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::SET_METADATA, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}


void DatabaseWALWriter::write_add_spelling(const Database& database, const std::string& word, Xapian::termcount freqinc)
{
	assert((database.flags & DB_WRITABLE) == DB_WRITABLE);
	auto endpoint = database.endpoints[0];
	assert(endpoint.is_local());
	auto path = endpoint.path;
	auto uuid = database.get_uuid();
	auto revision = database.get_revision();
	assert(_instance);
	_instance->enqueue(path, [=] (DatabaseWALWriterThread& thread) {
		L_TIMED(start)

		auto line = serialise_length(freqinc);
		line.append(word);
		L_DATABASE("write_add_spelling {path:%s, rev:%llu}: %s", repr(path), revision, repr(line));

		auto& wal = thread.wal(path);
		wal.write_line(path, uuid, revision, DatabaseWAL::Type::ADD_SPELLING, line, false);

		L_TIMED(end);
		L_DEBUG("Database WAL writer of %s succeeded after %s", repr(path), string::from_delta(start, end));
	});
}

#endif
