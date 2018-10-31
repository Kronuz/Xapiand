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


/*  ____        _        _                  __        ___    _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |___
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____|
 *
 */


#if XAPIAND_DATABASE_WAL

#include <array>                  // for std::array
#include <errno.h>                // for errno
#include <fcntl.h>                // for O_CREAT, O_WRONLY, O_EXCL
#include <limits>                 // for std::numeric_limits
#include <utility>                // for std::make_pair

#include "cassert.hh"             // for assert

#include "database.h"             // for Database
#include "database_pool.h"        // for DatabasePool, DatabaseUpdate
#include "database_utils.h"       // for read_uuid
#include "exception.h"            // for THROW, Error
#include "fs.hh"                  // for exists
#include "io.hh"                  // for io::*
#include "log.h"                  // for L_OBJ, L_CALL, L_INFO, L_ERR, L_WARNING
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
		if (read_uuid(base_path, uuid_data) != -1) {
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
			if (header.head.revision != volumes.second) {
				if (!unsafe) {
					THROW(StorageCorruptVolume, "Mismatch in WAL revision");
				}
				L_WARNING("Mismatch in WAL revision");
				header.head.revision = volumes.second;
			}
		} else {
			open(string::format(WAL_STORAGE_PATH "%llu", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
			if (header.head.revision != revision) {
				if (!unsafe) {
					THROW(StorageCorruptVolume, "Mismatch in WAL revision");
				}
				L_WARNING("Mismatch in WAL revision");
				header.head.revision = revision;
			}
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

		header.slot[slot] = header.head.offset; /* Beginning of the next revision */

		if (type == Type::COMMIT) {
			if (slot + 1 < WAL_SLOTS) {
				header.slot[slot + 1] = header.slot[slot];
			} else {
				open(string::format(WAL_STORAGE_PATH "%llu", revision + 1), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
				if (header.head.revision != revision + 1) {
					THROW(StorageCorruptVolume, "Mismatch in WAL revision");
				}
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

#endif
