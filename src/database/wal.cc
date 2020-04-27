/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "database/wal.h"

#if XAPIAND_DATABASE_WAL

#include <array>                    // for std::array
#include <cassert>                  // for assert
#include <errno.h>                  // for errno
#include <fcntl.h>                  // for O_CREAT, O_WRONLY, O_EXCL
#include <limits>                   // for std::numeric_limits
#include <utility>                  // for std::make_pair

#include "compressor_lz4.h"         // for compress_lz4, decompress_lz4
#include "database/pool.h"          // for DatabasePool
#include "database/shard.h"         // for Shard
#include "database/utils.h"         // for read_uuid
#include "exception.h"              // for THROW, Error
#include "error.hh"                 // for error:name, error::description
#include "fs.hh"                    // for exists, delete_files, normalize_path
#include "io.hh"                    // for io::*
#include "log.h"                    // for L_OBJ, L_CALL, L_INFO, L_ERR, L_WARNING
#include "manager.h"                // for XapiandManager
#include "metrics.h"                // for Metrics::metrics
#include "msgpack.h"                // for MsgPack
#include "opts.h"                   // for opts::*
#include "repr.hh"                  // for repr
#include "server/discovery.h"       // for db_updater
#include "strings.hh"               // for strings::format
#include "utype.hh"                 // for toUType

#define L_DATABASE_NOW(name)

#undef L_DEBUG
#define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_REPLICATION
// #define L_REPLICATION L_ROSY_BROWN
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_DATABASE_NOW
// #define L_DATABASE_NOW(name) auto name = std::chrono::steady_clock::now()


#define WAL_STORAGE_PATH "wal."
#define WAL_SYNC_MODE     STORAGE_ASYNC_SYNC


/*
 *  ____        _        _                  __        ___    _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | |
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |___
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____|
 *
 */

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
					THROW(StorageCorruptWAL, "WAL UUID mismatch");
				}
			}
		}
	}
}


DatabaseWAL::DatabaseWAL(std::string_view base_path_)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(base_path_, this),
	  validate_uuid(false),
	  _revision(0),
	  _shard(nullptr)
{
	std::array<unsigned char, 16> uuid_data;
	if (read_uuid(base_path, uuid_data) != -1) {
		_uuid = UUID(uuid_data);
		_uuid_le = UUID(uuid_data, true);
		validate_uuid = true;
	}
}


DatabaseWAL::DatabaseWAL(Shard* shard)
	: Storage<WalHeader, WalBinHeader, WalBinFooter>(shard->endpoint.path, this),
	  validate_uuid(true),
	  _revision(0),
	  _shard(shard)
{
	_uuid = UUID(_shard->db()->get_uuid());
	_uuid_le = UUID(_uuid.get_bytes(), true);
}


const UUID&
DatabaseWAL::get_uuid() const
{
	return _uuid;
}


const UUID&
DatabaseWAL::get_uuid_le() const
{
	return _uuid_le;
}


Xapian::rev
DatabaseWAL::get_revision() const
{
	if (_shard) {
		return _shard->db()->get_revision();
	} else {
		return _revision;
	}
}


bool
DatabaseWAL::execute()
{
	L_CALL("DatabaseWAL::execute()");

	if (!_shard) {
		THROW(Error, "Database is not defined");
	}

	Xapian::rev revision = _shard->reopen_revision;

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision);

	bool modified = false;

	try {
		bool end = false;
		Xapian::rev end_rev;
		for (end_rev = volumes.first; end_rev <= volumes.second && !end; ++end_rev) {
			try {
				open(strings::format(WAL_STORAGE_PATH "{}", end_rev), STORAGE_OPEN);
			} catch (const StorageIOError& exc) {
				L_DEBUG("Cannot open WAL {} volume {}: {}", repr(base_path), end_rev, exc.get_context());
				throw;
			} catch (const StorageCorruptVolume& exc) {
				L_DEBUG("Corrupt WAL {} volume {}: {}", repr(base_path), end_rev, exc.get_context());
				throw;
			}
			if (header.head.revision != end_rev) {
				L_DEBUG("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), end_rev);
				THROW(StorageCorruptWAL, "Mismatch in WAL revision");
			}

			Xapian::rev file_rev, begin_rev;
			file_rev = begin_rev = end_rev;

			auto high_slot = highest_valid_slot();
			if (high_slot == DatabaseWAL::max_slot) {
				if (revision != file_rev) {
					L_DEBUG("No WAL slots in the volume {} while trying to reach revision {}: {} volume {}", file_rev, revision, repr(base_path), file_rev);
					THROW(StorageCorruptWAL, "No WAL slots in the volume");
				}
				continue;
			}

			if (file_rev == volumes.second) {
				end = true;  // Avoid reenter to the loop with the high valid slot of the highest revision
			}

			end_rev = file_rev + high_slot;
			if (end_rev < revision) {
				continue;
			}

			uint32_t start_off;
			if (file_rev == volumes.first) {
				if (revision == file_rev) {
					// First volume found is the same as the current revision.
					// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
					// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
					begin_rev = file_rev;
					start_off = STORAGE_START_BLOCK_OFFSET;
				} else if (revision > file_rev) {
					// First volume found is older than current revision,
					// we advance the cursor to the proper slot.
					auto slot = revision - file_rev - 1;
					begin_rev = file_rev + slot;
					start_off = header.slot[slot];
				} else {
					// First volume found is beyond the current revision,
					// this could mean there are missing volumes between the
					// current revision and the revisions in existing volumes.
					L_DEBUG("Missing WAL volumes; the first one found is beyond current revision {}: {} volume {}", revision, repr(base_path), file_rev);
					THROW(StorageCorruptWAL, "Missing WAL volumes");
				}
			} else {
				// Always start at STORAGE_START_BLOCK_OFFSET for other volumes.
				start_off = STORAGE_START_BLOCK_OFFSET;
			}

			auto end_off = header.slot[high_slot];
			if (start_off < end_off) {
				L_INFO("Read and execute operations WAL file ({} volume {}) from [{}..{}] revision", repr(base_path), file_rev, begin_rev, end_rev);
			}

			seek(start_off);
			try {
				while (true) {
					std::string line = read(end_off);
					modified = execute_line(line, false, false);
				}
			} catch (const StorageEOF& exc) { }
		}

		if (volumes.first <= volumes.second) {
			if (end_rev < revision) {
				L_DEBUG("WAL did not reach the current revision {}, WAL ends at {}: {} volume {}", revision, end_rev, repr(base_path), volumes.second);
				THROW(StorageCorruptWAL, "WAL did not reach the current revision");
			}
		}
	} catch (const StorageException& exc) {
		L_EXC("WAL ERROR in {}", repr(base_path));
		Metrics::metrics()
			.xapiand_wal_errors
			.Increment();

		close();
		quarantine_files(base_path, {"wal.*"});  // Quarantine WAL instead of deleting
	}

	return modified;
}


MsgPack
DatabaseWAL::to_string_document(std::string_view serialised_document, bool unserialised)
{
	L_CALL("DatabaseWAL::to_string_document(<serialised_document>)");

	if (!unserialised) {
		return MsgPack(serialised_document);
	}

	auto doc = Xapian::Document::unserialise(std::string(serialised_document));

	auto data = Data(doc.get_data());
	auto main_locator = data.get("");
	auto obj = main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack();
	for (auto& locator : data) {
		switch (locator.type) {
			case Locator::Type::inplace:
			case Locator::Type::compressed_inplace:
				if (!locator.ct_type.empty()) {
					obj["_data"].push_back(MsgPack({
						{ "_content_type", locator.ct_type.to_string() },
						{ "_type", "inplace" },
						{ "_blob", locator.data() },
					}));
				}
				break;
			case Locator::Type::stored:
			case Locator::Type::compressed_stored:
#ifdef XAPIAND_DATA_STORAGE
				obj["_data"].push_back(MsgPack({
					{ "_content_type", locator.ct_type.to_string() },
					{ "_type", "stored" },
				}));
#endif
				break;
		}
	}
	return obj;
}


MsgPack
DatabaseWAL::to_string_metadata(std::string_view serialised_metadata, bool unserialised)
{
	L_CALL("DatabaseWAL::to_string_metadata(<serialised_document>)");

	if (!unserialised) {
		return MsgPack(serialised_metadata);
	}

	return MsgPack::unserialise(serialised_metadata);
}



MsgPack
DatabaseWAL::to_string_line(std::string_view line, bool unserialised)
{
	L_CALL("DatabaseWAL::to_string_line(<line>)");

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
		case Type::COMMIT:
			repr["op"] = "COMMIT";
			break;
		case Type::REPLACE_DOCUMENT:
			repr["op"] = "REPLACE_DOCUMENT";
			repr["docid"] = unserialise_length(&p, p_end);
			repr["document"] = to_string_document(std::string_view(p, p_end - p), unserialised);
			break;
		case Type::DELETE_DOCUMENT:
			repr["op"] = "DELETE_DOCUMENT";
			repr["docid"] = unserialise_length(&p, p_end);
			break;
		case Type::SET_METADATA:
			repr["op"] = "SET_METADATA";
			size = unserialise_length_and_check(&p, p_end);
			repr["key"] = std::string(p, size);
			repr["data"] = to_string_metadata(std::string_view(p + size, p_end - p - size), unserialised);
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
DatabaseWAL::to_string(Xapian::rev start_revision, Xapian::rev end_revision, bool unserialised)
{
	L_CALL("DatabaseWAL::to_string({}, {}, {})", start_revision, end_revision, unserialised);

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, start_revision, end_revision);

	if (volumes.first > start_revision) {
		start_revision = volumes.first;
	}

	auto result = MsgPack::ARRAY();

	bool end = false;
	Xapian::rev end_rev;
	for (end_rev = volumes.first; end_rev <= volumes.second && !end; ++end_rev) {
		try {
			open(strings::format(WAL_STORAGE_PATH "{}", end_rev), STORAGE_OPEN);
		} catch (const StorageIOError& exc) {
			L_WARNING("Cannot open WAL {} volume {}: {}", repr(base_path), end_rev, exc.get_context());
			continue;
		} catch (const StorageCorruptVolume& exc) {
			L_WARNING("Corrupt WAL {} volume {}: {}", repr(base_path), end_rev, exc.get_context());
			continue;
		}
		if (header.head.revision != end_rev) {
			L_WARNING("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), end_rev);
			header.head.revision = end_rev;
		}

		Xapian::rev file_rev, begin_rev;
		file_rev = begin_rev = end_rev;

		auto high_slot = highest_valid_slot();
		if (high_slot == DatabaseWAL::max_slot) {
			if (start_revision != file_rev) {
				L_WARNING("No WAL slots in the volume {} while trying to reach revision {}: {} volume {}", file_rev, start_revision, repr(base_path), file_rev);
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
				// First volume found is the same as the current revision.
				// The offset saved in slot 0 is the beginning of the revision 1 to reach 2
				// for that reason the revision 0 to reach 1 start in STORAGE_START_BLOCK_OFFSET
				begin_rev = file_rev;
				start_off = STORAGE_START_BLOCK_OFFSET;
			} else if (start_revision > file_rev) {
				// First volume found is older than current revision,
				// we advance the cursor to the proper slot.
				auto slot = start_revision - file_rev - 1;
				begin_rev = file_rev + slot;
				start_off = header.slot[slot];
			} else {
				// First volume found is beyond the current revision,
				// this could mean there are missing volumes between the
				// current revision and the revisions in existing volumes.
				L_WARNING("Missing WAL volumes; the first one found is beyond start revision {}: {} volume {}", start_revision, repr(base_path), file_rev);
				continue;
			}
		} else {
			// Always start at STORAGE_START_BLOCK_OFFSET for other volumes.
			start_off = STORAGE_START_BLOCK_OFFSET;
		}

		auto end_off = header.slot[high_slot];
		if (start_off < end_off) {
			L_INFO("Read and repr operations WAL file ({} volume {}) from [{}..{}] revision", repr(base_path), file_rev, begin_rev, end_rev);
		}

		seek(start_off);
		try {
			while (true) {
				std::string line = read(end_off);
				result.push_back(to_string_line(line, unserialised));
			}
		} catch (const StorageEOF& exc) { }
	}

	if (volumes.first <= volumes.second) {
		if (end_rev < end_revision && end_revision != std::numeric_limits<Xapian::rev>::max()) {
			L_WARNING("WAL did not reach the end revision {}, WAL ends at {}: {} volume {}", end_revision, end_rev, repr(base_path), volumes.second);
		}
	}

	return result;
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
DatabaseWAL::execute_line(std::string_view line, bool wal_, bool send_update)
{
	L_CALL("DatabaseWAL::execute_line(<line>, {}, {})", wal_, send_update);

	if (!_shard) {
		THROW(Error, "Database is not defined");
	}

	auto db_revision = _shard->db()->get_revision();

	const char *p = line.data();
	const char *p_end = p + line.size();

	auto revision = unserialise_length(&p, p_end);
	auto type = static_cast<Type>(unserialise_length(&p, p_end));

	L_REPLICATION("EXECUTE LINE: {} ({})", revision, enum_name(type));

	if (revision != db_revision) {
		L_DEBUG("WAL revision mismatch for {}: Expected {}, got {} ({})", repr(base_path), db_revision, revision, enum_name(type));
		THROW(StorageCorruptWAL, "WAL revision mismatch!");
	}

	auto data = decompress_lz4(std::string_view(p, p_end - p));

	Xapian::docid did;
	std::string document;
	Xapian::termcount freq;
	std::string term;
	std::size_t size;

	p = data.data();
	p_end = p + data.size();

	bool modified = true;

	switch (type) {
		case Type::COMMIT:
			if (!_shard->commit(wal_, send_update)) {
				L_DEBUG("WAL commit did nothing for {} at revision {}: line={{revision:{}, type:{}}}", repr(base_path), db_revision, revision, enum_name(type));
				L_WARNING("WAL commit did nothing");
			}
			modified = false;
			break;
		case Type::REPLACE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			document = std::string(p, p_end - p);
			_shard->replace_document(did, Xapian::Document::unserialise(document), false, wal_, false);
			break;
		case Type::DELETE_DOCUMENT:
			did = static_cast<Xapian::docid>(unserialise_length(&p, p_end));
			_shard->delete_document(did, false, wal_, false);
			break;
		case Type::SET_METADATA:
			size = unserialise_length_and_check(&p, p_end);
			_shard->set_metadata(std::string(p, size), std::string(p + size, p_end - p - size), false, wal_);
			break;
		case Type::ADD_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			_shard->add_spelling(std::string(p, p_end - p), freq, false, wal_);
			break;
		case Type::REMOVE_SPELLING:
			freq = static_cast<Xapian::termcount>(unserialise_length(&p, p_end));
			_shard->remove_spelling(std::string(p, p_end - p), freq, false, wal_);
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

	if (!_shard) {
		THROW(Error, "Database is not defined");
	}

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
		open(strings::format(WAL_STORAGE_PATH "{}", 0), STORAGE_OPEN);
		if (header.head.revision != 0) {
			L_DEBUG("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), 0);
			THROW(StorageCorruptWAL, "Mismatch in WAL revision");
		}
	} catch (const StorageIOError&) {
		return true;
	}

	UUID header_uuid(header.head.uuid, UUID_LENGTH);
	const auto& bytes = header_uuid.get_bytes();
	std::string uuid_str(bytes.begin(), bytes.end());

	int fd = io::open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL);
	if unlikely(fd == -1) {
		L_ERR("ERROR: opening file. {}\n", filename);
		return false;
	}
	if unlikely(io::write(fd, iamglass[0].data(), iamglass[0].size()) < 0) {
		L_ERRNO("io::write() -> {} ({}): {}", error::name(errno), errno, error::description(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, uuid_str.data(), uuid_str.size()) < 0) {
		L_ERRNO("io::write() -> {} ({}): {}", error::name(errno), errno, error::description(errno));
		io::close(fd);
		return false;
	}
	if unlikely(io::write(fd, iamglass[1].data(), iamglass[1].size()) < 0) {
		L_ERRNO("io::write() -> {} ({}): {}", error::name(errno), errno, error::description(errno));
		io::close(fd);
		return false;
	}
	io::close(fd);

	filename = base_path + "postlist.glass";
	fd = io::open(filename.c_str(), O_WRONLY | O_CREAT);
	if unlikely(fd == -1) {
		L_ERR("ERROR: opening file. {}\n", filename);
		return false;
	}
	io::close(fd);

	return true;
}


void
DatabaseWAL::write_line(const UUID& uuid, Xapian::rev revision, Type type, std::string_view data, [[maybe_unused]] bool send_update)
{
	L_CALL("DatabaseWAL::write_line({}, {}, Type::{}, <data>, {})", repr(uuid.to_string()), revision, enum_name(type), send_update);

	_uuid = uuid;
	_uuid_le = UUID(uuid.get_bytes(), true);

	_revision = revision;

	std::string line;
	line.append(serialise_length(revision));
	line.append(serialise_length(toUType(type)));
	line.append(compress_lz4(data));

	L_DATABASE_WAL("{} on {}: '{}'", enum_name(type), base_path, repr(line, quote));

	for (int t = 1; t >= 0; --t) {
		try {
			if (closed()) {
				auto volumes = get_volumes_range(WAL_STORAGE_PATH, revision, revision);
				auto volume = (volumes.first <= volumes.second) ? volumes.second : revision;
				open(strings::format(WAL_STORAGE_PATH "{}", volume), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
				if (header.head.revision != volume) {
					L_DEBUG("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), volume);
					THROW(StorageCorruptWAL, "Mismatch in WAL revision");
				}
			}

			if (header.head.revision > revision) {
				L_DEBUG("Invalid WAL revision {}: too old for {} volume {}", revision, repr(base_path), header.head.revision);
				THROW(StorageCorruptWAL, "Invalid WAL revision", revision, header.head.revision);
			}

			uint32_t slot = revision - header.head.revision;

			if (slot > WAL_SLOTS) {
				L_DEBUG("Volume {} skips unexistent revision {}: {} volume {}", slot, revision - 1, repr(base_path), header.head.revision);
				THROW(StorageCorruptWAL, "Volume skips unexistent revision");
			} else if (slot == WAL_SLOTS) {
				// We need a new volume, the old one is full
				open(strings::format(WAL_STORAGE_PATH "{}", revision), STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | WAL_SYNC_MODE);
				if (header.head.revision != revision) {
					L_DEBUG("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), revision);
					THROW(StorageCorruptWAL, "Mismatch in WAL revision");
				}
				slot = revision - header.head.revision;
			}

			assert(slot >= 0);
			assert(slot < WAL_SLOTS);
			if (slot > 0) {
				if (header.slot[slot - 1] == 0) {
					L_DEBUG("Slot {} skips unexistent revision {}: {} volume {}", slot, revision - 1, repr(base_path), header.head.revision);
					THROW(StorageCorruptWAL, "Slot skips unexistent revision");
				}
			}
			if (slot < WAL_SLOTS - 1) {
				if (header.slot[slot + 1] != 0) {
					L_DEBUG("Slot {} already occupied for revision {}: {} volume {}", slot, revision, repr(base_path), header.head.revision);
					THROW(StorageCorruptWAL, "Slot already occupied for revision");
				}
			}

			write(line.data(), line.size());

			header.slot[slot] = header.head.offset; // Beginning of the next revision

			commit();

#ifdef XAPIAND_CLUSTERING
			if (!opts.solo) {
				// On COMMIT, let the updaters do their job
				if (send_update) {
					auto path = normalize_path(base_path);
					db_updater()->debounce(path, revision + 1, path);
				}
			}
#endif
			break;
		} catch (const StorageException&) {
			L_EXC("WAL ERROR in {}", repr(base_path));
			Metrics::metrics()
				.xapiand_wal_errors
				.Increment();

			close();
			quarantine_files(base_path, {"wal.*"});  // Quarantine WAL instead of deleting
		}
	}
}


std::pair<Xapian::rev, uint32_t>
DatabaseWAL::locate_revision(Xapian::rev revision)
{
	L_CALL("DatabaseWAL::locate_revision(...)");

	auto volumes = get_volumes_range(WAL_STORAGE_PATH, 0, revision);
	if (volumes.first <= volumes.second && revision - volumes.second < WAL_SLOTS) {
		open(strings::format(WAL_STORAGE_PATH "{}", volumes.second), STORAGE_OPEN);
		if (header.head.revision != volumes.second) {
			L_DEBUG("Mismatch in WAL revision {}: {} volume {}", header.head.revision, repr(base_path), volumes.second);
			THROW(StorageCorruptWAL, "Mismatch in WAL revision");
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


std::string
DatabaseWAL::get_current_line(uint32_t end_off)
{
	L_CALL("DatabaseWAL::get_current_line(...)");

	try {
		return read(end_off);
	}  catch (const StorageEOF& exc) { }

	return "";
}


/*
 *  ____        _        _                  __        ___    _ __        __    _ _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |\ \      / / __(_) |_ ___ _ __
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | | \ \ /\ / / '__| | __/ _ \ '__|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |__\ V  V /| |  | | ||  __/ |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____\_/\_/ |_|  |_|\__\___|_|
 *
 */

DatabaseWALWriterThread::DatabaseWALWriterThread() noexcept :
	_wal_writer(nullptr),
	lru(opts.wal_writer_cache_size)
{
}


DatabaseWALWriterThread::DatabaseWALWriterThread(size_t idx, DatabaseWALWriter* wal_writer) noexcept :
	_wal_writer(wal_writer),
	_name(strings::format(wal_writer->_format, idx))
{
}


DatabaseWALWriterThread&
DatabaseWALWriterThread::operator=(DatabaseWALWriterThread&& other)
{
	L_CALL("DatabaseWALWriterThread::operator=()");

	_wal_writer = std::move(other._wal_writer);
	_name = std::move(other._name);
	Thread::operator=(static_cast<Thread&&>(other));
	return *this;
}


const std::string&
DatabaseWALWriterThread::name() const noexcept
{
	L_CALL("DatabaseWALWriterThread::name()");

	return _name;
}


void
DatabaseWALWriterThread::operator()()
{
	L_CALL("DatabaseWALWriterThread::operator()()");

	_wal_writer->_workers.fetch_add(1, std::memory_order_relaxed);
	while (!_wal_writer->_finished.load(std::memory_order_acquire)) {
		DatabaseWALWriterTask task;
		auto valid = _queue.wait_dequeue_timed(task, 100000);  // wait 100ms
		if likely(valid && task) {
			try {
				task(*this);
			} catch (...) {
				L_EXC("ERROR: Task died with an unhandled exception");
			}
		} else if (_wal_writer->_ending.load(std::memory_order_acquire)) {
			break;
		}
	}
	_wal_writer->_workers.fetch_sub(1, std::memory_order_relaxed);
}


void
DatabaseWALWriterThread::clear()
{
	L_CALL("DatabaseWALWriterThread::clear()");

	DatabaseWALWriterTask task;
	while (_queue.try_dequeue(task)) {}
}


DatabaseWAL&
DatabaseWALWriterThread::get(const std::string& path)
{
	L_CALL("DatabaseWALWriterThread::get()");

	auto it = lru.find(path);
	if (it == lru.end()) {
		it = lru.emplace(path, std::make_unique<DatabaseWAL>(path)).first;
	}
	return *it->second;
}


void
DatabaseWALWriterThread::erase(const std::string& path)
{
	L_CALL("DatabaseWALWriterThread::erase()");

	lru.erase(path);
}


DatabaseWALWriter::DatabaseWALWriter(const char* format, std::size_t num_threads) :
	_threads(num_threads),
	_format(format),
	_ending(false),
	_finished(false),
	_workers(0)
{
	for (std::size_t idx = 0; idx < num_threads; ++idx) {
		_threads[idx] = DatabaseWALWriterThread(idx, this);
		_threads[idx].run();
	}
}


void
DatabaseWALWriter::execute(DatabaseWALWriterTask&& task)
{
	L_CALL("DatabaseWALWriter::execute()");

	static thread_local DatabaseWALWriterThread thread(0, this);
	task(thread);
}


bool
DatabaseWALWriter::enqueue(DatabaseWALWriterTask&& task)
{
	L_CALL("DatabaseWALWriter::enqueue()");

	static const std::hash<std::string> hasher;
	auto hash = hasher(task.path);
	auto& thread = _threads[hash % _threads.size()];
	return thread._queue.enqueue(std::move(task));
}


void
DatabaseWALWriter::clear()
{
	L_CALL("DatabaseWALWriter::clear()");

	for (auto& _thread : _threads) {
		_thread.clear();
	}
}


bool
DatabaseWALWriter::join(std::chrono::milliseconds timeout)
{
	L_CALL("DatabaseWALWriter::join()");

	bool ret = true;
	// Divide timeout among number of running worker threads
	// to give each thread the chance to "join".
	auto threadpool_workers = _workers.load(std::memory_order_relaxed);
	if (!threadpool_workers) {
		threadpool_workers = 1;
	}
	auto single_timeout = timeout / threadpool_workers;
	for (auto& _thread : _threads) {
		auto wakeup = std::chrono::steady_clock::now() + single_timeout;
		if (!_thread.join(wakeup)) {
			ret = false;
		}
	}
	return ret;
}


void
DatabaseWALWriter::end()
{
	L_CALL("DatabaseWALWriter::end()");

	if (!_ending.exchange(true, std::memory_order_release)) {
		for (auto& _thread : _threads) {
			_thread._queue.enqueue(DatabaseWALWriterTask{});
		}
	}
}


void
DatabaseWALWriter::finish()
{
	L_CALL("DatabaseWALWriter::finish()");

	if (!_finished.exchange(true, std::memory_order_release)) {
		for (auto& _thread : _threads) {
			_thread._queue.enqueue(DatabaseWALWriterTask{});
		}
	}
}


std::size_t
DatabaseWALWriter::running_size()
{
	L_CALL("DatabaseWALWriter::running_size()");

	return _threads.size();
}


void
DatabaseWALWriterTask::write_remove_spelling(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_remove_spelling()");

	L_DATABASE_NOW(start);

	auto line = serialise_length(freq);  // freqdec
	line.append(term_word_val);  // word
	L_DATABASE("write_remove_spelling {{path:{}, rev:{}}}: {}", repr(path), revision, repr(line));

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::REMOVE_SPELLING, line, false);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::write_commit(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_commit()");

	L_DATABASE_NOW(start);

	L_DATABASE("write_commit {{path:{}, rev:{}}}", repr(path), revision);

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::COMMIT, "", send_update);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::write_replace_document(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_replace_document()");

	L_DATABASE_NOW(start);

	auto line = serialise_length(did);
	line.append(doc.serialise());
	L_DATABASE("write_replace_document {{path:{}, rev:{}}}: {}", repr(path), revision, repr(line));

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::REPLACE_DOCUMENT, line, false);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::write_delete_document(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_delete_document()");

	L_DATABASE_NOW(start);

	auto line = serialise_length(did);
	L_DATABASE("write_delete_document {{path:{}, rev:{}}}: {}", repr(path), revision, repr(line));

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::DELETE_DOCUMENT, line, false);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::write_set_metadata(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_set_metadata()");

	L_DATABASE_NOW(start);

	auto line = serialise_string(key);
	line.append(term_word_val);  // val
	L_DATABASE("write_set_metadata {{path:{}, rev:{}}}: {}", repr(path), revision, repr(line));

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::SET_METADATA, line, false);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::write_add_spelling(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::write_add_spelling()");

	L_DATABASE_NOW(start);

	auto line = serialise_length(freq);  // freqinc
	line.append(term_word_val);  // word
	L_DATABASE("write_add_spelling {{path:{}, rev:{}}}: {}", repr(path), revision, repr(line));

	auto& wal = thread.get(path);
	wal.write_line(uuid, revision, DatabaseWAL::Type::ADD_SPELLING, line, false);

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL writer of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriterTask::delete_wal(DatabaseWALWriterThread& thread)
{
	L_CALL("DatabaseWALWriterTask::delete_wal()");

	L_DATABASE_NOW(start);

	L_DATABASE("delete_wal {{path:{}}}: {}", repr(path));

	thread.erase(path);
	delete_files(path, {"wal.*"});

	L_DATABASE_NOW(end);
	L_DATABASE("Database WAL delete of {} succeeded after {}", repr(path), strings::from_delta(start, end));
}


void
DatabaseWALWriter::write_remove_spelling(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, const std::string& word, Xapian::termcount freqdec)
{
	L_CALL("DatabaseWALWriter::write_remove_spelling({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.term_word_val = word;
	task.freq = freqdec;
	task.dispatcher = &DatabaseWALWriterTask::write_remove_spelling;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::write_commit(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, bool send_update)
{
	L_CALL("DatabaseWALWriter::write_commit({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.send_update = send_update;
	task.dispatcher = &DatabaseWALWriterTask::write_commit;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::write_replace_document(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, Xapian::docid did, Xapian::Document&& doc)
{
	L_CALL("DatabaseWALWriter::write_replace_document({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	assert(did != 0);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.did = did;
	task.doc = std::move(doc);
	task.dispatcher = &DatabaseWALWriterTask::write_replace_document;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::write_delete_document(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, Xapian::docid did)
{
	L_CALL("DatabaseWALWriter::write_delete_document({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	assert(did != 0);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.did = did;
	task.dispatcher = &DatabaseWALWriterTask::write_delete_document;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::write_set_metadata(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, const std::string& key, const std::string& val)
{
	L_CALL("DatabaseWALWriter::write_set_metadata({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.key = key;
	task.term_word_val = val;
	task.dispatcher = &DatabaseWALWriterTask::write_set_metadata;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::write_add_spelling(bool synchronous, const std::string& path, const std::string& uuid, Xapian::rev revision, const std::string& word, Xapian::termcount freqinc)
{
	L_CALL("DatabaseWALWriter::write_add_spelling({}, {}, {}, {}, ...)", synchronous, repr(path), repr(uuid), revision);

	DatabaseWALWriterTask task;
	task.path = path;
	task.uuid = UUID(uuid);
	task.revision = revision;
	task.term_word_val = word;
	task.freq = freqinc;
	task.dispatcher = &DatabaseWALWriterTask::write_add_spelling;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


void
DatabaseWALWriter::delete_wal(bool synchronous, const std::string& path)
{
	L_CALL("DatabaseWALWriter::delete_wal({}, {})", synchronous, repr(path));

	DatabaseWALWriterTask task;
	task.path = path;
	task.dispatcher = &DatabaseWALWriterTask::delete_wal;

	if (synchronous) {
		execute(std::move(task));
	} else {
		enqueue(std::move(task));
	}
}


#endif
