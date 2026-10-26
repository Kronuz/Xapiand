/*
 * Copyright (c) 2015-2020 Dubalu LLC
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

#include "database/shard.h"

#include <algorithm>              // for std::move
#include <cassert>                // for assert
#include <optional>                // for std::optional
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "database/data.h"        // for Locator
#include "database/flags.h"       // for readable_flags, DB_*
#include "database/pool.h"        // for ShardEndpoint
#include "database/handler.h"     // for committer
#include "debounce_or_run.h"      // for debounce_or_run
#include "database/utils.h"       // for DB_SLOT_VERSION
#include "database/wal.h"         // for DatabaseWAL
#include "engine/xapian/convert.h"  // for engine::xapian::to_xapian, from_xapian
#include "engine/xapian/errors.h"   // for engine::xapian::NotAvailable, Closed, Opening, DatabaseFailure
#include "xapiand_exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "index_resolver_lru.h"   // for IndexSettings
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_CALL
#include "manager.h"              // for XapiandManager, trigger_replication
#include "msgpack.h"              // for MsgPack
#include "random.hh"              // for random_int
#include "repr.hh"                // for repr
#include "reserved/fields.h"      // for ID_FIELD_NAME
#include "server/discovery.h"     // for db_updater
#include "storage.h"              // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "xapiand_strings.hh"             // for strings::from_delta, strings::format

#ifdef XAPIAND_RANDOM_ERRORS
#include "random.hh"                // for random_real
#include "opts.h"                   // for opts.random_errors_db
#define RANDOM_ERRORS_DB_THROW(error, ...) \
	if (opts.random_errors_db) { \
		auto prob = random_real(0, 1); \
		if (prob < opts.random_errors_db) { \
			throw error(__VA_ARGS__); \
		} \
	}
#else
#define RANDOM_ERRORS_DB_THROW(...)
#endif


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE
// #undef L_DATABASE_WRAP_BEGIN
// #define L_DATABASE_WRAP_BEGIN L_DELAYED_100
// #undef L_DATABASE_WRAP_END
// #define L_DATABASE_WRAP_END L_DELAYED_N_UNLOG


#define DATA_STORAGE_PATH "docdata."


// The numbering of a document whose id is new: which shard it lands in, out of how many.
struct NumericId {
	size_t shard_num;
	size_t n_shards;
};


// The version a write requires, as its document asks for it in the version slot.
static inline std::optional<engine::rev>
expected_version(const engine::Document& doc)
{
	auto serialised = doc.get_value(DB_SLOT_VERSION);
	if (serialised.empty()) {
		return std::nullopt;
	}
	return static_cast<engine::rev>(sortable_unserialise(serialised));
}


// Completes a document the engine is about to write: embeds its numeric id (when it is new)
// and its new version in the stored data, and clears the shards slot.
static void
finalise_write(engine::Document& doc, engine::WriteResult& info, const std::optional<NumericId>& numeric)
{
	bool data_modified = false;
	Data data(doc.get_data());
	auto data_obj = data.get_obj();
	if (numeric) {
		auto did = static_cast<double>((info.did - 1) * numeric->n_shards + numeric->shard_num + 1);  // unshard number and shard docid to docid in multi-db
		auto did_serialised = sortable_serialise(did);
		info.term = "QN" + did_serialised;
		doc.add_boolean_term(info.term);
		doc.add_value(DB_SLOT_ID, did_serialised);
		// Set id inside serialized object:
		auto it = data_obj.find(ID_FIELD_NAME);
		if (it != data_obj.end()) {
			auto& value = it.value();
			switch (value.get_type()) {
				case MsgPack::Type::POSITIVE_INTEGER:
					value = static_cast<uint64_t>(did);
					break;
				case MsgPack::Type::NEGATIVE_INTEGER:
					value = static_cast<int64_t>(did);
					break;
				case MsgPack::Type::FLOAT:
					value = static_cast<double>(did);
					break;
				default:
					break;
			}
			data_modified = true;
		}
	}
	auto it = data_obj.find(VERSION_FIELD_NAME);
	if (it != data_obj.end()) {
		auto& value = it.value();
		value = info.version;
		data_modified = true;
	}
	doc.add_value(DB_SLOT_SHARDS, "");  // remove shards slot
	if (data_modified) {
		data.set_obj(data_obj);
		data.flush();
		doc.set_data(data.serialise());
	}
}


// The shard could not be opened (yet), or does not exist, or cannot serve now.
static inline bool
is_opening_failure(engine::errc kind)
{
	return kind == engine::errc::opening || kind == engine::errc::not_found || kind == engine::errc::not_available;
}


// A failure a reopen may recover from by trying again.
static inline bool
is_stale_failure(engine::errc kind)
{
	return is_opening_failure(kind) ||
		kind == engine::errc::reopen ||
		kind == engine::errc::corrupt ||
		kind == engine::errc::timeout ||
		kind == engine::errc::network ||
		kind == engine::errc::closed;
}

#define STORAGE_SYNC_MODE STORAGE_FULL_SYNC

// Codec for newly written data-store records. Zstandard gives ~2x LZ4's ratio at
// LZ4-class write speed (see the storage library's bench). The codec id is stored
// per record, so existing LZ4 volumes keep reading unchanged; only new writes use
// this. Switch back to STORAGE_COMPRESS_LZ4 here to change it.
#define STORAGE_COMPRESS_MODE STORAGE_COMPRESS_ZSTD


/*
 *  ____        _        ____  _
 * |  _ \  __ _| |_ __ _/ ___|| |_ ___  _ __ __ _  __ _  ___
 * | | | |/ _` | __/ _` \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
 * | |_| | (_| | || (_| |___) | || (_) | | | (_| | (_| |  __/
 * |____/ \__,_|\__\__,_|____/ \__\___/|_|  \__,_|\__, |\___|
 *                                                |___/
 */
#ifdef XAPIAND_DATA_STORAGE

// The legacy v1 data-storage header: a single in-place header block. New volumes
// are written in the crash-safe v2 format below; this type exists only so a v2
// DataStorage can still READ pre-existing v1 volumes (read-only).
struct DataHeaderV1 {
	struct DataHeaderHead {
		uint32_t magic;
		uint32_t offset;  // required
		char uuid[UUID_LENGTH];
	} head;

	char padding[(STORAGE_BLOCK_SIZE - sizeof(DataHeaderV1::DataHeaderHead)) / sizeof(char)];

	void init(void* param, void* args);
	void validate(void* param, void* args);
};


// The v2 (dual-meta, crash-safe) data-storage header. It begins with the engine's
// StorageMetaHead (magic, txnid, high-water, whole-block checksum) -- which is what
// opts new volumes into the v2 format -- then the shard uuid, padded to a block.
struct DataHeaderV2 {
	StorageMetaHead meta;
	char uuid[UUID_LENGTH];
	char padding[STORAGE_BLOCK_SIZE - sizeof(StorageMetaHead) - UUID_LENGTH];

	void init(void* param, void* args);
	void validate(void* param, void* args);
};


#pragma pack(push, 1)
struct DataBinHeader {
	uint8_t magic;
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void*, void*, uint32_t size_, uint8_t flags_) {
		magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void*, void*) {
		if (magic != STORAGE_BIN_HEADER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad document header magic number");
		}
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Data Storage document deleted");
		}
	}
};


struct DataBinFooter {
	uint32_t checksum;
	uint8_t magic;

	inline void init(void*, void*, uint32_t checksum_) {
		magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(void*, void*, uint32_t checksum_) {
		if (magic != STORAGE_BIN_FOOTER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad document footer magic number");
		}
		if (checksum != checksum_) {
			THROW(StorageCorruptVolume, "Bad document checksum");
		}
	}
};
#pragma pack(pop)


class DataStorage : public Storage<DataHeaderV2, DataBinHeader, DataBinFooter, STORAGE_DEFAULT_IO, DataHeaderV1> {
public:
	int flags;

	uint32_t volume;

	DataStorage(std::string_view base_path_, void* param_, int flags);

	bool open(std::string_view relative_path);
};


void
DataHeaderV1::init(void* param, void* /*unused*/)
{
	auto shard = static_cast<Shard*>(param);
	assert(shard);

	head.magic = _LEGACY_STORAGE_MAGIC;
	strncpy(head.uuid, shard->db()->get_uuid().c_str(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
}


void
DataHeaderV1::validate(void* param, void* /*unused*/)
{
	if (head.magic != _LEGACY_STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	auto shard = static_cast<Shard*>(param);
	if (UUID(head.uuid) != UUID(shard->db()->get_uuid())) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


void
DataHeaderV2::init(void* param, void* /*unused*/)
{
	auto shard = static_cast<Shard*>(param);
	assert(shard);

	// The engine stamps meta (magic/txnid/high-water/checksum); we only carry the
	// shard uuid so a foreign volume is rejected on open.
	memset(uuid, 0, sizeof(uuid));
	strncpy(uuid, shard->db()->get_uuid().c_str(), sizeof(uuid));
}


void
DataHeaderV2::validate(void* param, void* /*unused*/)
{
	// The engine has already checked the meta magic + whole-block checksum; this
	// is the shard-domain check.
	auto shard = static_cast<Shard*>(param);
	if (UUID(uuid) != UUID(shard->db()->get_uuid())) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


DataStorage::DataStorage(std::string_view base_path_, void* param_, int flags)
	: Storage<DataHeaderV2, DataBinHeader, DataBinFooter, STORAGE_DEFAULT_IO, DataHeaderV1>(base_path_, param_),
	  flags(flags)
{
}


bool
DataStorage::open(std::string_view relative_path)
{
	return Storage<DataHeaderV2, DataBinHeader, DataBinFooter, STORAGE_DEFAULT_IO, DataHeaderV1>::open(relative_path, flags);
}
#endif  // XAPIAND_DATA_STORAGE


/*
 *   ____  _                   _
 *  / ___|| |__   __ _ _ __ __| |
 *  \___ \| '_ \ / _` | '__/ _` |
 *   ___) | | | | (_| | | | (_| |
 *  |____/|_| |_|\__,_|_|  \__,_|
 *
 */
Shard::Shard(ShardEndpoint& endpoint_, int flags_, bool busy_)
	: reopen_time(std::chrono::steady_clock::now()),
	  reopen_revision(0),
	  _busy(busy_),
	  _local(false),
	  _closed(false),
	  _modified(false),
	  _incomplete(false),
	  _transaction(Transaction::none),
	  endpoint(endpoint_),
	  flags(flags_)
{
}


Shard::~Shard() noexcept
{
	try {
		do_close(true, true, Shard::Transaction::none, false);
		if (log) {
			log->clear();
			log.reset();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}

bool
Shard::reopen_writable()
{
	/*
	 * __        __    _ _        _     _        ____  ____
	 * \ \      / / __(_) |_ __ _| |__ | | ___  |  _ \| __ )
	 *  \ \ /\ / / '__| | __/ _` | '_ \| |/ _ \ | | | |  _ \.
	 *   \ V  V /| |  | | || (_| | |_) | |  __/ | |_| | |_) |
	 *    \_/\_/ |_|  |_|\__\__,_|_.__/|_|\___| |____/|____/
	 *
	 */
	L_CALL("Shard::reopen_writable()");

	bool created = false;

	if (is_closed()) {
		throw engine::xapian::Closed("Database has been closed");
	}

	reset();

	std::unique_ptr<engine::xapian::Shard> new_database;

	assert(!endpoint.empty());
	bool local = false;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote writable shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
		if (is_replica()) {
			throw engine::xapian::NotAvailable("Endpoint must be local for replicas");
		}
		auto node = endpoint.node();
		if (!node || node->empty()) {
			L_DEBUG("Writable endpoint {} ({}) is invalid.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			L_DEBUG("Writable endpoint {} ({}) is inactive.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node is inactive");
		}
		auto port = node->remote_port;
		if (port == 0) {
			L_DEBUG("Writable endpoint {} ({}) node without a valid port.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node without a valid port");
		}
		auto& host = node->host();
		if (host.empty()) {
			L_DEBUG("Writable endpoint {} ({}) node without a valid host.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node without a valid host");
		}
		new_database = task::sync_wait(engine::xapian::open_remote(host, port, flags, endpoint.path, true));
		// Writable remote databases do not have a local database fallback.
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local writable shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
		auto opened = task::sync_wait(engine::xapian::open_local(endpoint.path, true, has_db_create_or_open(flags)));
		new_database = std::move(opened.shard);
		created = opened.created;
		local = true;
	}

	_local.store(local, std::memory_order_relaxed);
	if (local) {
		reopen_revision = new_database->revision();
		endpoint.set_revision(reopen_revision);
	}

	if (is_transactional()) {
		task::sync_wait(new_database->begin_transaction(transactional() == Transaction::flushed));
	}

#ifdef XAPIAND_DATA_STORAGE
	if (local) {
		writable_storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS_MODE | STORAGE_SYNC_MODE);
		storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN);
	} else {
		writable_storage = std::unique_ptr<DataStorage>(nullptr);
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	engine_shard = std::move(new_database);
	reopen_time = std::chrono::steady_clock::now();

#ifdef XAPIAND_DATABASE_WAL
	// If reopen_revision is not available WAL work as a log for the operations
	if (is_wal_active()) {
		// WAL wasn't already active for the requested endpoint
		DatabaseWAL wal(this);
		if (wal.execute()) {
			_modified.store(true, std::memory_order_relaxed);
		}
	}
#endif  // XAPIAND_DATABASE_WAL

	// Ends Writable DB
	////////////////////////////////////////////////////////////////

	return created;
}

bool
Shard::reopen_readable()
{
	/*
	 *  ____                _       _     _        ____  ____
	 * |  _ \ ___  __ _  __| | __ _| |__ | | ___  |  _ \| __ )
	 * | |_) / _ \/ _` |/ _` |/ _` | '_ \| |/ _ \ | | | |  _ \.
	 * |  _ <  __/ (_| | (_| | (_| | |_) | |  __/ | |_| | |_) |
	 * |_| \_\___|\__,_|\__,_|\__,_|_.__/|_|\___| |____/|____/
	 *
	 */
	L_CALL("Shard::reopen_readable()");

	bool created = false;

	if (is_closed()) {
		throw engine::xapian::Closed("Database has been closed");
	}

	reset();

	std::unique_ptr<engine::xapian::Shard> new_database;

	assert(!endpoint.empty());
	bool local = false;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
		if (is_replica()) {
			throw engine::xapian::NotAvailable("Endpoint must be local for replicas");
		}
		auto node = endpoint.node();
		if (!node || node->empty()) {
			L_DEBUG("Endpoint {} ({}) is invalid.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			L_DEBUG("Endpoint {} ({}) is inactive.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node is inactive");
		}
		auto port = node->remote_port;
		if (port == 0) {
			L_DEBUG("Endpoint {} ({}) node without a valid port.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node without a valid port");
		}
		auto& host = node->host();
		if (host.empty()) {
			L_DEBUG("Endpoint {} ({}) node without a valid host.", repr(endpoint.to_string()), readable_flags(flags));
			throw engine::xapian::NotAvailable("Endpoint node without a valid host");
		}
		new_database = task::sync_wait(engine::xapian::open_remote(host, port, flags, endpoint.path, false));
		// Check for a local database fallback:
		auto index_settings = XapiandManager::resolve_index_settings(endpoint.path);
		if (index_settings.shards.size() == 1) {
			auto local_node = Node::get_local_node();
			auto fallback = false;
			auto& nodes = index_settings.shards[0].nodes;
			for (const auto& node_name : nodes) {
				if (strings::lower(node_name) == local_node->lower_name()) {
					fallback = true;
					break;
				}
			}
			if (fallback) {
				try {
					RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
					auto same = task::sync_wait(engine::xapian::open_local_if_same(*new_database, endpoint.path));
					if (same) {
						L_DATABASE("Endpoint {} fallback to local shard!", repr(endpoint.to_string()));
						// Handle remote endpoint and figure out if the endpoint is a local database
						RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
						new_database = std::move(same);
						local = true;
					} else {
						_incomplete.store(true, std::memory_order_relaxed);
					}
				} catch (const engine::Error& error) {
					if (!is_opening_failure(error.kind)) {
						throw;
					}
					_incomplete.store(true, std::memory_order_relaxed);
				}
				if (has_db_trigger_replication(flags)) {
					if (XapiandManager::get_state() == XapiandManager::State::READY) {
						// Try triggering replication from primary shard:
						try {
							trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), endpoint.path, Endpoint{endpoint.path, Node::get_node(nodes[0])}, Endpoint{endpoint.path});
						} catch (...) { }
					}
				}
			}
		}
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(engine::xapian::Opening, "Random Error");
		auto opened = task::sync_wait(engine::xapian::open_local(endpoint.path, false, has_db_create_or_open(flags)));
		new_database = std::move(opened.shard);
		created = opened.created;
		local = true;
	}

	_local.store(local, std::memory_order_relaxed);
	if (local) {
		reopen_revision = new_database->revision();
	}

#ifdef XAPIAND_DATA_STORAGE
	if (local) {
		// WAL required on a local database, open it.
		storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN);
	} else {
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	engine_shard = std::move(new_database);
	reopen_time = std::chrono::steady_clock::now();
	// Ends Readable DB
	////////////////////////////////////////////////////////////////

	return created;
}


void
Shard::retry_or_throw(const engine::Error& error, int t, bool commit_)
{
	// Called from a catch block: closes the shard as the failure demands, and rethrows it
	// when it is final or not a failure of the shard.
	switch (error.kind) {
		case engine::errc::opening:
		case engine::errc::not_found:
		case engine::errc::not_available:
		case engine::errc::timeout:
		case engine::errc::network:
			if (t == 0) { do_close(commit_); throw; }
			return;
		case engine::errc::closed:
			do_close(commit_);
			if (t == 0) { throw; }
			return;
		case engine::errc::reopen:
		case engine::errc::corrupt:
		case engine::errc::database:
			do_close(commit_);
			throw;
		default:
			throw;
	}
}


bool
Shard::reopen()
{
	L_CALL("Shard::reopen() {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	L_DATABASE_WRAP_BEGIN("Shard::reopen:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::reopen:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	if (engine_shard) {
		if (!is_incomplete()) {
			// Try to reopen
			for (int t = DB_RETRIES; t >= 0; --t) {
				try {
					bool ret = task::sync_wait(engine_shard->reopen());
					return ret;
				} catch (const engine::Error& error) {
					if (!is_stale_failure(error.kind)) {
						throw;
					}
				}
			}
		}

		do_close(true);
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			if (is_writable()) {
				reopen_writable();
			} else {
				reopen_readable();
			}
			break;
		} catch (const engine::Error& error) {
			switch (error.kind) {
				case engine::errc::not_found:
					reset();
					throw;
				case engine::errc::reopen:
				case engine::errc::corrupt:
				case engine::errc::opening:
				case engine::errc::not_available:
				case engine::errc::timeout:
				case engine::errc::network:
					if (t == 0) { reset(); throw; }
					break;
				case engine::errc::closed:
				case engine::errc::database:
					reset();
					if (t == 0) { throw; }
					break;
				default:
					reset();
					throw;
			}
		} catch (...) {
			reset();
			throw;
		}
	}

	assert(engine_shard);
	L_DATABASE("Reopening shard: {}", __repr__());
	return true;
}


Xapian::Database*
Shard::db()
{
	L_CALL("Shard::db()");

	if (is_closed()) {
		throw engine::xapian::Closed("Database has been closed");
	}

	if (!engine_shard) {
		reopen();
	}

	return engine_shard->database();
}


unsigned
Shard::refs() const
{
    L_CALL("Shard::refs()");

    if (engine_shard) {
        return engine_shard->refs();
    }
    return 0;
}


std::shared_ptr<const Node>
Shard::node() const
{
	return endpoint.node();
}


void
Shard::reset() noexcept
{
	L_CALL("Shard::reset()");

	try {
		engine_shard.reset();
	} catch(...) {}
	reopen_revision = 0;
	_local.store(false, std::memory_order_relaxed);
	_closed.store(false, std::memory_order_relaxed);
	_modified.store(false, std::memory_order_relaxed);
	_incomplete.store(false, std::memory_order_relaxed);
#ifdef XAPIAND_DATA_STORAGE
	try {
		storage.reset();
	} catch(...) {}
	try {
		writable_storage.reset();
	} catch(...) {}
#endif  // XAPIAND_DATA_STORAGE
}


void
Shard::do_close(bool commit_, bool closed_, Transaction transaction_, bool throw_exceptions)
{
	L_CALL("Shard::do_close({}, {}, {}, {}) {{endpoint:{}, database:{}, modified:{}, closed:{}}}", commit_, closed_, transaction_ == Shard::Transaction::none ? "<none>" : "<transaction>", throw_exceptions, repr(to_string()), engine_shard ? "<database>" : "null", is_modified(), is_closed());

	if (
		commit_ &&
		engine_shard &&
		!is_transactional() &&
		!is_closed() &&
		is_modified() &&
		is_writable() &&
		is_local()

	) {
		// Commit only on modified writable databases
		try {
			commit();
		} catch (...) {
			if (throw_exceptions) {
				throw;
			}
			L_WARNING("WARNING: Commit during close failed!");
		}
	}

	if (engine_shard) {
		try {
			engine_shard.reset();
		} catch (...) {
			if (throw_exceptions) {
				throw;
			}
			L_WARNING("WARNING: Internal database close failed!");
		}
	}

	bool local_ = is_local();

	reset();

	_local.store(local_, std::memory_order_relaxed);
	_closed.store(closed_, std::memory_order_relaxed);
	_transaction.store(transaction_, std::memory_order_relaxed);
}


void
Shard::do_close(bool commit_)
{
	L_CALL("Shard::do_close()");

	do_close(commit_, is_closed(), transactional(), false);
}


void
Shard::close()
{
	L_CALL("Shard::close()");

	if (is_closed()) {
		return;
	}

	do_close(true, true, Transaction::none);
}


void
Shard::autocommit(const std::shared_ptr<Shard>& shard)
{
	L_CALL("Shard::autocommit({})", shard ? shard->__repr__() : "null");

	if (
		shard->engine_shard &&          // Autocommit only if there is a database
		!shard->is_transactional() &&   // Autocommit only if there's no transaction active (like during replication)
		!shard->is_closed() &&          // Autocommit only if database is not closed
		shard->is_modified() &&         // Autocommit only modified databases
		shard->is_writable() &&         // Autocommit only writable databases
		shard->is_local() &&            // Autocommit only local databases
		shard->is_autocommit_active()   // Data RESTORE doesn't do autocommit
	) {
		// Auto commit only on modified writable databases. If the committer
		// refuses (shutting down, or out of memory), commit now, in place: the
		// caller (checkin) still holds the shard, so committer_commit would wait
		// on itself checking it out again.
		debounce_or_run(*committer(), [&](const std::weak_ptr<Shard>&) {
			try {
				shard->commit();
			} catch (const Exception& exc) {
				L_WARNING("Autocommit of {} falied: {}", repr(shard->to_string()), exc.get_message());
			} catch (const Xapian::Error& exc) {
				L_WARNING("Autocommit of {} falied: {}", repr(shard->to_string()), exc.get_description());
			} catch (const std::exception& exc) {
				// Runs inside checkin, which is noexcept: nothing may escape.
				L_ERR("Autocommit of {} falied: {}", repr(shard->to_string()), exc.what());
			} catch (...) {
				L_ERR("Autocommit of {} falied: unknown exception", repr(shard->to_string()));
			}
		}, shard->endpoint, std::weak_ptr<Shard>(shard));
	}
}


bool
Shard::commit([[maybe_unused]] bool wal_, bool send_update)
{
	L_CALL("Shard::commit({})", wal_);

	assert(is_writable());
	assert(is_write_active());

	auto local = is_local();

	if (local && !is_modified()) {
		L_DATABASE("Commit on shard {} was discarded, because there are not changes", repr(endpoint.to_string()));
		return false;
	}

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::commit:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::commit:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Committing shard {} {{ try:{} }}", repr(endpoint.to_string()), DB_RETRIES - t);
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif  // XAPIAND_DATA_STORAGE
			auto prior_revision = wdb->revision();
			auto transaction = transactional();
			task::sync_wait(wdb->commit(
				transaction == Transaction::flushed ? engine::Transaction::flushed :
				transaction == Transaction::unflushed ? engine::Transaction::unflushed :
				engine::Transaction::none));
			_modified.store(false, std::memory_order_relaxed);
			if (local) {
				auto current_revision = wdb->revision();
				if (prior_revision == current_revision) {
					L_DATABASE("Commit on shard {} was discarded, because it turned out not to change the revision", repr(endpoint.to_string()));
					return false;
				}
				assert(current_revision == prior_revision + 1);
				L_DATABASE("Commit on shard {}: {} -> {}", repr(endpoint.to_string()), prior_revision, current_revision);
				endpoint.set_revision(current_revision);
				if (!is_replica()) {
					endpoint.pending_revision.store(current_revision, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_commit(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						send_update);
				}
#endif
#ifdef XAPIAND_CLUSTERING
				if (!opts.solo) {
					if (send_update) {
						db_updater()->debounce(endpoint.path, current_revision, endpoint.path);
					}
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t, false);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::commit:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return true;
}

void
Shard::begin_transaction(bool flushed)
{
	L_CALL("Shard::begin_transaction({})", flushed);

	assert(is_writable());
	assert(is_write_active());

	if (!is_transactional()) {
		RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

		db();
		task::sync_wait(engine_shard->begin_transaction(flushed));
		_transaction.store(flushed ? Transaction::flushed : Transaction::unflushed, std::memory_order_relaxed);
	}
}


void
Shard::commit_transaction()
{
	L_CALL("Shard::commit_transaction()");

	assert(is_writable());
	assert(is_write_active());

	if (is_transactional()) {
		RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

		db();
		task::sync_wait(engine_shard->commit_transaction());
		_transaction.store(Transaction::none, std::memory_order_relaxed);
	}
}


void
Shard::cancel_transaction()
{
	L_CALL("Shard::cancel_transaction()");

	assert(is_writable());
	assert(is_write_active());

	if (is_transactional()) {
		RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

		db();
		task::sync_wait(engine_shard->cancel_transaction());
		_transaction.store(Transaction::none);
	}
}


void
Shard::delete_document(Xapian::docid shard_did, bool commit_, bool wal_, [[maybe_unused]] bool version_)
{
	L_CALL("Shard::delete_document({}, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	std::optional<engine::rev> expected;  // TODO: Implement version check (version should have required version)

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Deleting document {} in shard {} {{ try:{} }}", shard_did, repr(endpoint.to_string()), DB_RETRIES - t);

		try {
			auto local = is_local();
			task::sync_wait(wdb->erase(shard_did, local, expected));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_delete_document(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						shard_did);
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}
}


void
Shard::delete_document_term(const std::string& term, bool commit_, bool wal_, [[maybe_unused]] bool version_)
{
	L_CALL("Shard::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	Xapian::docid shard_did = 0;
	std::optional<engine::rev> expected;  // TODO: Implement version check (version should have required version)

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Deleting document {} in shard {{ try:{} }}", repr(term), repr(endpoint.to_string()), DB_RETRIES - t);
		shard_did = 0;

		try {
			auto local = is_local();
			shard_did = task::sync_wait(wdb->erase_by_key(term, local, expected));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_delete_document(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						shard_did);
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::delete_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Shard::storage_get_stored(const Locator& locator)
{
	L_CALL("Shard::storage_get_stored()");

	assert(locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored);
	assert(locator.volume != -1);

	if (storage) {
		storage->open(strings::format(DATA_STORAGE_PATH "{}", locator.volume));
		storage->seek(static_cast<uint32_t>(locator.offset));
		return storage->read();
	}

	std::string locator_key;
	locator_key.push_back('\x00');
	locator_key.append(serialise_length(locator.volume));
	locator_key.append(serialise_length(locator.offset));
	return get_metadata(locator_key);
}


std::pair<std::string, std::string>
Shard::storage_push_blobs(std::string&& doc_data)
{
	L_CALL("Shard::storage_push_blobs()");

	assert(is_writable());
	assert(is_write_active());

	std::pair<std::string, std::string> pushed;
	if (doc_data.empty()) {
		return pushed;
	}

	if (writable_storage) {
		auto data = Data(std::move(doc_data));
		for (auto& locator : data) {
			if (locator.size == 0) {
				data.erase(locator.ct_type);
			}
			if (locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored) {
				if (!locator.raw.empty()) {
					uint32_t offset;
					while (true) {
						try {
							if (writable_storage->closed()) {
								writable_storage->volume = writable_storage->get_volumes_range(DATA_STORAGE_PATH).second;
								writable_storage->open(strings::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
							}
							offset = writable_storage->write(serialise_strings({ locator.ct_type.to_string(), locator.raw }));
							break;
						} catch (StorageEOF) {
							++writable_storage->volume;
							writable_storage->open(strings::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
						} catch (const StorageLegacyReadOnly&) {
							// The latest volume is a legacy v1 volume (read-only).
							// Start a fresh v2 volume above it for new writes; the
							// old v1 volumes stay readable and age out over time.
							++writable_storage->volume;
							writable_storage->open(strings::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
						}
					}
					data.update(locator.ct_type, writable_storage->volume, offset, locator.size);
				}
			}
		}
		pushed.second = std::move(data.serialise());
		data.flush();
		pushed.first = std::move(data.serialise());
	}
	return pushed;
}


void
Shard::storage_commit()
{
	L_CALL("Shard::storage_commit()");

	if (writable_storage) {
		writable_storage->commit();
	}
}
#endif  // XAPIAND_DATA_STORAGE


engine::WriteResult
Shard::add_document(engine::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::add_document(<doc>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteResult info;

	engine::WriteArgs args;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, {}); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Adding new document to shard {} {{ try:{} }}", repr(endpoint.to_string()), DB_RETRIES - t);

		try {
			args.local = is_local();
			const auto local = args.local;
			info = task::sync_wait(wdb->add(doc, args));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
#ifdef XAPIAND_DATA_STORAGE
					if (!pushed.second.empty()) {
						doc.set_data(pushed.second);  // restore data with blobs
					}
#endif  // XAPIAND_DATA_STORAGE
					XapiandManager::manager(true)->wal_writer->write_replace_document(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						info.did,
						std::move(doc));
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::add_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return info;
}


Xapian::DocumentInfo
Shard::add_document(Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	return engine::xapian::to_xapian(add_document(engine::xapian::from_xapian(doc), commit_, wal_, version_));
}


Xapian::DocumentInfo
Shard::replace_document(Xapian::docid shard_did, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	return engine::xapian::to_xapian(replace_document(shard_did, engine::xapian::from_xapian(doc), commit_, wal_, version_));
}


Xapian::DocumentInfo
Shard::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	return engine::xapian::to_xapian(replace_document_term(term, engine::xapian::from_xapian(doc), commit_, wal_, version_));
}


engine::WriteResult
Shard::replace_document(Xapian::docid shard_did, engine::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document({}, <doc>, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteResult info;
	info.did = shard_did;

	engine::WriteArgs args;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, std::nullopt); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Replacing document {} in shard {} {{ try:{} }}", info.did, repr(endpoint.to_string()), DB_RETRIES - t);

		try {
			args.local = is_local();
			const auto local = args.local;
			info = task::sync_wait(wdb->replace(shard_did, doc, args));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
#ifdef XAPIAND_DATA_STORAGE
					if (!pushed.second.empty()) {
						doc.set_data(pushed.second);  // restore data with blobs
					}
#endif  // XAPIAND_DATA_STORAGE
					XapiandManager::manager(true)->wal_writer->write_replace_document(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						info.did,
						std::move(doc));
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return info;
}


engine::WriteResult
Shard::replace_document_term(const std::string& term, engine::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document_term({}, <doc>, {}, {}, {})", repr(term), commit_, wal_, version_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteResult info;

	engine::WriteArgs args;
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	auto n_shards_ser = doc.get_value(DB_SLOT_SHARDS);

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Replacing document {} in shard {} {{ try:{} }}", repr(term), repr(endpoint.to_string()), DB_RETRIES - t);
		info = engine::WriteResult{};
		info.term = term;

		try {
			args.local = is_local();
			const auto local = args.local;
			args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, std::nullopt); };
			if (!local) {
				info = task::sync_wait(wdb->replace_by_key(term, doc, args));
			} else if (term[0] == 'Q' && term[1] == 'N') {
				assert(term.size() > 2);
				const char *p = n_shards_ser.data();
				const char *p_end = p + n_shards_ser.size();
				size_t shard_num = p == p_end ? 0 : unserialise_length(&p, p_end);
				size_t n_shards = p == p_end ? 1 : unserialise_length(&p, p_end);
				auto did = sortable_unserialise(term.substr(2));
				if (did == 0u) {
					// A new numeric id: the shard assigns the document id, and the multi-db id derives from it.
					args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, NumericId{shard_num, n_shards}); };
					info = task::sync_wait(wdb->add(doc, args));
				} else {
					info = task::sync_wait(wdb->replace((did - 1) / n_shards + 1, doc, args));  // docid in the multi-db to the docid in the shard
					info.term = term;
				}
			} else {
				info = task::sync_wait(wdb->replace_by_key(term, doc, args));
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
#ifdef XAPIAND_DATA_STORAGE
					if (!pushed.second.empty()) {
						doc.set_data(pushed.second);  // restore data with blobs
					}
#endif  // XAPIAND_DATA_STORAGE
					XapiandManager::manager(true)->wal_writer->write_replace_document(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						info.did,
						std::move(doc));
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::replace_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return info;
}


void
Shard::add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL("Shard::add_spelling(<word, <freqinc>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			task::sync_wait(wdb->add_spelling(word, freqinc));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_add_spelling(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						word,
						freqinc);
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::add_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}
}


Xapian::termcount
Shard::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Shard::remove_spelling(<word>, <freqdec>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::remove_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::remove_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	Xapian::termcount result = 0;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			result = task::sync_wait(wdb->remove_spelling(word, freqdec));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_remove_spelling(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						word,
						freqdec);
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::remove_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return result;
}


Xapian::docid
Shard::get_docid_term(const std::string& term)
{
	L_CALL("Shard::get_docid_term({})", repr(term));

	Xapian::docid did = 0;

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::get_docid_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::get_docid_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto it = rdb->postlist_begin(term);
			if (it == rdb->postlist_end(term)) {
				throw Xapian::DocNotFoundError("Document not found");
			}
			did = *it;
			break;
		} catch (const Xapian::DatabaseModifiedError&) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::DatabaseClosedError&) {
			do_close();
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseError&) {
			do_close();
			throw;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Shard::get_docid_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return did;
}


Xapian::Document
Shard::get_document(Xapian::docid shard_did, unsigned doc_flags)
{
	L_CALL("Shard::get_document({})", shard_did);

	Xapian::Document doc;

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::get_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::get_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			doc = rdb->get_document(shard_did, doc_flags);
			break;
		} catch (const Xapian::DatabaseModifiedError&) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::DatabaseClosedError&) {
			do_close();
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseError&) {
			do_close();
			throw;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::get_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return doc;
}


std::string
Shard::get_metadata(const std::string& key)
{
	L_CALL("Shard::get_metadata({})", repr(key));

	std::string value;

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::get_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::get_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	const char *p = key.data();
	const char *p_end = p + key.size();
	if (*p == '\x00') {
		if (storage) {
			++p;
			ssize_t volume = unserialise_length(&p, p_end);
			size_t offset = unserialise_length(&p, p_end);
			storage->open(strings::format(DATA_STORAGE_PATH "{}", volume));
			storage->seek(static_cast<uint32_t>(offset));
			return storage->read();
		}
	}

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			value = rdb->get_metadata(key);
			break;
		} catch (const Xapian::DatabaseModifiedError&) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::DatabaseClosedError&) {
			do_close();
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseError&) {
			do_close();
			throw;
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Shard::get_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return value;
}


std::vector<std::string>
Shard::get_metadata_keys()
{
	L_CALL("Shard::get_metadata_keys()");

	std::vector<std::string> values;

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::get_metadata_keys:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::get_metadata_keys:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto it = rdb->metadata_keys_begin();
			auto it_e = rdb->metadata_keys_end();
			for (; it != it_e; ++it) {
				values.push_back(*it);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError&) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { do_close(); throw; }
		} catch (const Xapian::DatabaseClosedError&) {
			do_close();
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseError&) {
			do_close();
			throw;
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Shard::get_metadata_keys:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);

		values.clear();
	}

	return values;
}


void
Shard::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL("Shard::set_metadata({}, {}, {}, {})", repr(key), repr(value), commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(engine::xapian::DatabaseFailure, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::set_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::set_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw engine::xapian::NotAvailable("Cluster has no quorum");
	}
#endif

	db();
	auto* wdb = engine_shard.get();

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			task::sync_wait(wdb->set_metadata(key, value));
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->uuid();
					XapiandManager::manager(true)->wal_writer->write_set_metadata(
						is_synchronous_wal(),
						endpoint.path,
						std::move(uuid),
						prior_revision,
						key,
						value);
				}
#endif
			}
			break;
		} catch (const engine::Error& error) {
			retry_or_throw(error, t);
		}
		reopen();
		db();
		wdb = engine_shard.get();
		L_DATABASE_WRAP_END("Shard::set_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}
}


std::string
Shard::to_string() const
{
	return endpoint.to_string();
}


std::string
Shard::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Shard {} ({}){}{}{}{}{}{}{}{}>",
		repr(to_string()),
		readable_flags(flags),
		is_writable() ? " " + DARK_STEEL_BLUE + "(writable)" + STEEL_BLUE : "",
		is_wal_active() ? " " + DARK_STEEL_BLUE + "(active WAL)" + STEEL_BLUE : "",
		is_local() ? " " + DARK_STEEL_BLUE + "(local)" + STEEL_BLUE : "",
		is_closed() ? " " + ORANGE + "(closed)" + STEEL_BLUE : "",
		is_modified() ? " " + LIGHT_STEEL_BLUE + "(modified)" + STEEL_BLUE : "",
		is_incomplete() ? " " + DARK_STEEL_BLUE + "(incomplete)" + STEEL_BLUE : "",
		is_busy() ? " " + DARK_ORANGE + "(busy)" + STEEL_BLUE : "",
		is_transactional() ? " " + DARK_STEEL_BLUE + "(transactional)" + STEEL_BLUE : "");
}
