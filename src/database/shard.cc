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

#include "database/shard.h"

#include <algorithm>              // for std::move
#include <cassert>                // for assert
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "database/data.h"        // for Locator
#include "database/flags.h"       // for readable_flags, DB_*
#include "database/pool.h"        // for ShardEndpoint
#include "database/handler.h"     // for committer
#include "database/utils.h"       // for DB_SLOT_VERSION
#include "database/wal.h"         // for DatabaseWAL
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "fs.hh"                  // for build_path_index
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_CALL
#include "manager.h"              // for XapiandManager, trigger_replication
#include "msgpack.h"              // for MsgPack
#include "random.hh"              // for random_int
#include "repr.hh"                // for repr
#include "reserved/fields.h"      // for ID_FIELD_NAME
#include "server/discovery.h"     // for db_updater
#include "storage.h"              // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "strings.hh"             // for strings::from_delta, strings::format

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

#ifdef XAPIAND_DATABASE_WAL
#define XAPIAN_DB_SYNC_MODE  Xapian::DB_NO_SYNC
#else
#define XAPIAN_DB_SYNC_MODE  0
#endif

#define STORAGE_SYNC_MODE STORAGE_FULL_SYNC


/*
 *  ____        _        ____  _
 * |  _ \  __ _| |_ __ _/ ___|| |_ ___  _ __ __ _  __ _  ___
 * | | | |/ _` | __/ _` \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
 * | |_| | (_| | || (_| |___) | || (_) | | | (_| | (_| |  __/
 * |____/ \__,_|\__\__,_|____/ \__\___/|_|  \__,_|\__, |\___|
 *                                                |___/
 */
#ifdef XAPIAND_DATA_STORAGE

struct DataHeader {
	struct DataHeaderHead {
		uint32_t magic;
		uint32_t offset;  // required
		char uuid[UUID_LENGTH];
	} head;

	char padding[(STORAGE_BLOCK_SIZE - sizeof(DataHeader::DataHeaderHead)) / sizeof(char)];

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


class DataStorage : public Storage<DataHeader, DataBinHeader, DataBinFooter> {
public:
	int flags;

	uint32_t volume;

	DataStorage(std::string_view base_path_, void* param_, int flags);

	bool open(std::string_view relative_path);
};


void
DataHeader::init(void* param, void* /*unused*/)
{
	auto shard = static_cast<Shard*>(param);
	assert(shard);

	head.magic = STORAGE_MAGIC;
	strncpy(head.uuid, shard->db()->get_uuid().c_str(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
}


void
DataHeader::validate(void* param, void* /*unused*/)
{
	if (head.magic != STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	auto shard = static_cast<Shard*>(param);
	if (UUID(head.uuid) != UUID(shard->db()->get_uuid())) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


DataStorage::DataStorage(std::string_view base_path_, void* param_, int flags)
	: Storage<DataHeader, DataBinHeader, DataBinFooter>(base_path_, param_),
	  flags(flags)
{
}


bool
DataStorage::open(std::string_view relative_path)
{
	return Storage<DataHeader, DataBinHeader, DataBinFooter>::open(relative_path, flags);
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
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	reset();

	auto new_database = std::make_unique<Xapian::WritableDatabase>();

	assert(!endpoint.empty());
	bool local = false;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote writable shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		auto node = endpoint.node();
		if (!node || node->empty()) {
			L_DEBUG("Writable endpoint {} ({}) is invalid.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			L_DEBUG("Writable endpoint {} ({}) is inactive.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node is inactive");
		}
		auto port = node->remote_port;
		if (port == 0) {
			L_DEBUG("Writable endpoint {} ({}) node without a valid port.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node without a valid port");
		}
		auto& host = node->host();
		if (host.empty()) {
			L_DEBUG("Writable endpoint {} ({}) node without a valid host.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node without a valid host");
		}
		*new_database = Xapian::Remote::open_writable(host, port, 10000, 10000, flags, endpoint.path);
		// Writable remote databases do not have a local database fallback.
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local writable shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			*new_database = Xapian::WritableDatabase(endpoint.path, Xapian::DB_OPEN | Xapian::DB_RETRY_LOCK | XAPIAN_DB_SYNC_MODE);
		} catch (const Xapian::DatabaseNotFoundError&) {
			if (!has_db_create_or_open(flags)) {
				throw;
			}
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			if (!build_path_index(endpoint.path)) {
				L_WARNING("Cannot build path for index {}", endpoint.path);
			}
			*new_database = Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE | Xapian::DB_RETRY_LOCK | XAPIAN_DB_SYNC_MODE);
			created = true;
		}
		local = true;
	}

	_local.store(local, std::memory_order_relaxed);
	if (local) {
		reopen_revision = new_database->get_revision();
		endpoint.set_revision(reopen_revision);
	}

	if (is_transactional()) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(new_database.get());
		wdb->begin_transaction(transactional() == Transaction::flushed);
	}

#ifdef XAPIAND_DATA_STORAGE
	if (local) {
		writable_storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE);
		storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN);
	} else {
		writable_storage = std::unique_ptr<DataStorage>(nullptr);
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	database = std::move(new_database);
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
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	reset();

	auto new_database = std::make_unique<Xapian::Database>();

	assert(!endpoint.empty());
	bool local = false;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote shard {} ({})", repr(endpoint.to_string()), readable_flags(flags));
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		auto node = endpoint.node();
		if (!node || node->empty()) {
			L_DEBUG("Endpoint {} ({}) is invalid.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			L_DEBUG("Endpoint {} ({}) is inactive.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node is inactive");
		}
		auto port = node->remote_port;
		if (port == 0) {
			L_DEBUG("Endpoint {} ({}) node without a valid port.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node without a valid port");
		}
		auto& host = node->host();
		if (host.empty()) {
			L_DEBUG("Endpoint {} ({}) node without a valid host.", repr(endpoint.to_string()), readable_flags(flags));
			throw Xapian::DatabaseNotAvailableError("Endpoint node without a valid host");
		}
		*new_database = Xapian::Remote::open(host, port, 10000, 10000, flags, endpoint.path);
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
					RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
					Xapian::Database tmp = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
					if (tmp.get_uuid() == new_database->get_uuid()) {
						L_DATABASE("Endpoint {} fallback to local shard!", repr(endpoint.to_string()));
						// Handle remote endpoint and figure out if the endpoint is a local database
						RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
						*new_database = tmp;
						local = true;
					} else {
						_incomplete.store(true, std::memory_order_relaxed);
					}
				} catch (const Xapian::DatabaseNotFoundError&) {
					_incomplete.store(true, std::memory_order_relaxed);
				} catch (const Xapian::DatabaseOpeningError&) {
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
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			*new_database = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
		} catch (const Xapian::DatabaseNotFoundError&) {
			if (!has_db_create_or_open(flags))  {
				throw;
			}
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			if (!build_path_index(endpoint.path)) {
				L_WARNING("Cannot build path for index {}", endpoint.path);
			}
			Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE);
			created = true;

			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			*new_database = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
		}
		local = true;
	}

	_local.store(local, std::memory_order_relaxed);
	if (local) {
		reopen_revision = new_database->get_revision();
	}

#ifdef XAPIAND_DATA_STORAGE
	if (local) {
		// WAL required on a local database, open it.
		storage = std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN);
	} else {
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	database = std::move(new_database);
	reopen_time = std::chrono::steady_clock::now();
	// Ends Readable DB
	////////////////////////////////////////////////////////////////

	return created;
}


bool
Shard::reopen()
{
	L_CALL("Shard::reopen() {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	L_DATABASE_WRAP_BEGIN("Shard::reopen:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::reopen:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	if (database) {
		if (!is_incomplete()) {
			// Try to reopen
			for (int t = DB_RETRIES; t >= 0; --t) {
				try {
					bool ret = database->reopen();
					return ret;
				} catch (const Xapian::DatabaseModifiedError&) {
				} catch (const Xapian::DatabaseCorruptError&) {
				} catch (const Xapian::DatabaseOpeningError&) {
				} catch (const Xapian::NetworkTimeoutError&) {
				} catch (const Xapian::NetworkError&) {
				} catch (const Xapian::DatabaseClosedError&) {
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
		} catch (const Xapian::DatabaseNotFoundError&) {
			reset();
			throw;
		} catch (const Xapian::DatabaseModifiedError&) {
			if (t == 0) { reset(); throw; }
		} catch (const Xapian::DatabaseCorruptError&) {
			if (t == 0) { reset(); throw; }
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { reset(); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { reset(); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { reset(); throw; }
		} catch (const Xapian::DatabaseError&) {
			reset();
			if (t == 0) { throw; }
		} catch (...) {
			reset();
			throw;
		}
	}

	assert(database);
	L_DATABASE("Reopening shard: {}", __repr__());
	return true;
}


Xapian::Database*
Shard::db()
{
	L_CALL("Shard::db()");

	if (is_closed()) {
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	if (!database) {
		reopen();
	}

	return database.get();
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
		database.reset();
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
	L_CALL("Shard::do_close({}, {}, {}, {}) {{endpoint:{}, database:{}, modified:{}, closed:{}}}", commit_, closed_, transaction_ == Shard::Transaction::none ? "<none>" : "<transaction>", throw_exceptions, repr(to_string()), database ? "<database>" : "null", is_modified(), is_closed());

	if (
		commit_ &&
		database &&
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

	if (database) {
		try {
			database.reset();
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
		shard->database &&              // Autocommit only if there is a database
		!shard->is_transactional() &&   // Autocommit only if there's no transaction active (like during replication)
		!shard->is_closed() &&          // Autocommit only if database is not closed
		shard->is_modified() &&         // Autocommit only modified databases
		shard->is_writable() &&         // Autocommit only writable databases
		shard->is_local() &&            // Autocommit only local databases
		shard->is_autocommit_active()   // Data RESTORE doesn't do autocommit
	) {
		// Auto commit only on modified writable databases
		committer()->debounce(shard->endpoint, std::weak_ptr<Shard>(shard));
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::commit:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::commit:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Committing shard {} {{ try:{} }}", repr(endpoint.to_string()), DB_RETRIES - t);
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif  // XAPIAND_DATA_STORAGE
			auto prior_revision = wdb->get_revision();
			auto transaction = transactional();
			if (transaction == Transaction::flushed) {
				wdb->commit_transaction();
				wdb->begin_transaction(true);
			} else if (transaction == Transaction::unflushed) {
				wdb->commit_transaction();
				wdb->commit();
				wdb->begin_transaction(false);
			} else {
				wdb->commit();
			}
			_modified.store(false, std::memory_order_relaxed);
			if (local) {
				auto current_revision = wdb->get_revision();
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
					auto uuid = wdb->get_uuid();
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
		} catch (const Xapian::DatabaseOpeningError&) {
			if (t == 0) { do_close(false); throw; }
		} catch (const Xapian::NetworkTimeoutError&) {
			if (t == 0) { do_close(false); throw; }
		} catch (const Xapian::NetworkError&) {
			if (t == 0) { do_close(false); throw; }
		} catch (const Xapian::DatabaseClosedError&) {
			do_close(false);
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseError&) {
			do_close(false);
			throw;
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->begin_transaction(flushed);
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
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->commit_transaction();
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
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->cancel_transaction();
		_transaction.store(Transaction::none);
	}
}


void
Shard::delete_document(Xapian::docid shard_did, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::delete_document({}, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::rev version = UNKNOWN_REVISION;  // TODO: Implement version check (version should have required version)

	std::string ver;
	if (version_) {
		ver = version == UNKNOWN_REVISION ? std::string() : sortable_serialise(version);
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Deleting document {} in shard {} {{ try:{} }}", shard_did, repr(endpoint.to_string()), DB_RETRIES - t);

		try {
			auto local = is_local();
			if (local) {
				if (!ver.empty()) {
					auto ver_prefix = "V" + serialise_length(shard_did);
					auto ver_prefix_size = ver_prefix.size();
					auto vit = wdb->allterms_begin(ver_prefix);
					auto vit_e = wdb->allterms_end(ver_prefix);
					if (vit == vit_e) {
						if (ver != "\x80") {  // "\x80" = sortable_serialise(0)
							throw Xapian::DocVersionConflictError("Version mismatch!");
						}
					}
					for (; vit != vit_e; ++vit) {
						std::string current_term = *vit;
						std::string_view current_ver(current_term);
						current_ver.remove_prefix(ver_prefix_size);
						if (!current_ver.empty()) {
							if (ver != current_ver) {
								// Throw error about wrong version!
								throw Xapian::DocVersionConflictError("Version mismatch!");
							}
							break;
						}
					}
				}
			}
			wdb->delete_document(shard_did);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}
}


void
Shard::delete_document_term(const std::string& term, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::docid shard_did = 0;
	Xapian::rev version = UNKNOWN_REVISION;  // TODO: Implement version check (version should have required version)

	std::string ver;
	if (version_) {
		ver = version == UNKNOWN_REVISION ? std::string() : sortable_serialise(version);
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Deleting document {} in shard {{ try:{} }}", repr(term), repr(endpoint.to_string()), DB_RETRIES - t);
		shard_did = 0;

		try {
			auto local = is_local();
			if (local) {
				auto it = wdb->postlist_begin(term);
				if (it == wdb->postlist_end(term)) {
					throw Xapian::DocNotFoundError("Document not found");
				} else {
					shard_did = *it;
					if (!ver.empty()) {
						auto ver_prefix = "V" + serialise_length(shard_did);
						auto ver_prefix_size = ver_prefix.size();
						auto vit = wdb->allterms_begin(ver_prefix);
						auto vit_e = wdb->allterms_end(ver_prefix);
						if (vit == vit_e) {
							if (ver != "\x80") {  // "\x80" = sortable_serialise(0)
								throw Xapian::DocVersionConflictError("Version mismatch!");
							}
						}
						for (; vit != vit_e; ++vit) {
							std::string current_term = *vit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (ver != current_ver) {
									// Throw error about wrong version!
									throw Xapian::DocVersionConflictError("Version mismatch!");
								}
								break;
							}
						}
					}
				}
			}
			if (shard_did) {
				wdb->delete_document(shard_did);
			} else {
				wdb->delete_document(term);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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

	assert(locator.type == Locator::Type::stored || locator.type == Locator::Type::compressed_stored);
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
			if (locator.type == Locator::Type::stored || locator.type == Locator::Type::compressed_stored) {
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


Xapian::DocumentInfo
Shard::add_document(Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::add_document(<doc>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::DocumentInfo info;

	std::string ver;
	if (version_) {
		ver = doc.get_value(DB_SLOT_VERSION);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Adding new document to shard {} {{ try:{} }}", repr(endpoint.to_string()), DB_RETRIES - t);
		info.version = 0;
		info.did = 0;

		try {
			auto local = is_local();
			if (local) {
				if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
					throw Xapian::DocVersionConflictError("Version mismatch!");
				}
				bool data_modified = false;
				Data data(doc.get_data());
				auto data_obj = data.get_obj();
				info.did = wdb->get_lastdocid() + 1;
				auto ver_prefix = "V" + serialise_length(info.did);
				ver = sortable_serialise(++info.version);
				auto it = data_obj.find(VERSION_FIELD_NAME);
				if (it != data_obj.end()) {
					auto& value = it.value();
					value = info.version;
					data_modified = true;
				}
				doc.add_boolean_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
				doc.add_value(DB_SLOT_SHARDS, "");  // remove shards slot
				if (data_modified) {
					data.set_obj(data_obj);
					data.flush();
					doc.set_data(data.serialise());
				}

				assert(info.did);
				wdb->replace_document(info.did, doc);
			} else {
				info = wdb->add_document(doc);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::add_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return info;
}


Xapian::DocumentInfo
Shard::replace_document(Xapian::docid shard_did, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document({}, <doc>, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::DocumentInfo info;
	info.did = shard_did;

	std::string ver;
	if (version_) {
		ver = doc.get_value(DB_SLOT_VERSION);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Replacing document {} in shard {} {{ try:{} }}", info.did, repr(endpoint.to_string()), DB_RETRIES - t);
		info.version = 0;

		try {
			auto local = is_local();
			if (local) {
				bool data_modified = false;
				Data data(doc.get_data());
				auto data_obj = data.get_obj();
				auto ver_prefix = "V" + serialise_length(info.did);
				auto ver_prefix_size = ver_prefix.size();
				auto vit = wdb->allterms_begin(ver_prefix);
				auto vit_e = wdb->allterms_end(ver_prefix);
				if (vit == vit_e) {
					if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
						throw Xapian::DocVersionConflictError("Version mismatch!");
					}
				}
				for (; vit != vit_e; ++vit) {
					std::string current_term = *vit;
					std::string_view current_ver(current_term);
					current_ver.remove_prefix(ver_prefix_size);
					if (!current_ver.empty()) {
						if (!ver.empty() && ver != current_ver) {
							// Throw error about wrong version!
							throw Xapian::DocVersionConflictError("Version mismatch!");
						}
						info.version = sortable_unserialise(current_ver);
						break;
					}
				}
				ver = sortable_serialise(++info.version);
				auto it = data_obj.find(VERSION_FIELD_NAME);
				if (it != data_obj.end()) {
					auto& value = it.value();
					value = info.version;
					data_modified = true;
				}
				doc.add_boolean_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
				doc.add_value(DB_SLOT_SHARDS, "");  // remove shards slot
				if (data_modified) {
					data.set_obj(data_obj);
					data.flush();
					doc.set_data(data.serialise());
				}
				wdb->replace_document(info.did, doc);
			} else {
				info = wdb->replace_document(info.did, doc);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	if (commit_) {
		commit(wal_);
	}

	return info;
}


Xapian::DocumentInfo
Shard::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document_term({}, <doc>, {}, {}, {})", repr(term), commit_, wal_, version_);

	assert(is_writable());
	assert(is_write_active());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::DocumentInfo info;

	std::string ver;
	if (version_) {
		ver = doc.get_value(DB_SLOT_VERSION);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	auto n_shards_ser = doc.get_value(DB_SLOT_SHARDS);

	for (int t = DB_RETRIES; t >= 0; --t) {
		L_DATABASE("Replacing document {} in shard {} {{ try:{} }}", repr(term), repr(endpoint.to_string()), DB_RETRIES - t);
		info.version = 0;
		info.did = 0;
		info.term = term;

		try {
			auto local = is_local();
			if (local) {
				bool data_modified = false;
				Data data(doc.get_data());
				auto data_obj = data.get_obj();
				std::string ver_prefix;
				assert(term.size() > 2);
				if (term[0] == 'Q' && term[1] == 'N') {
					const char *p = n_shards_ser.data();
					const char *p_end = p + n_shards_ser.size();
					size_t shard_num = p == p_end ? 0 : unserialise_length(&p, p_end);
					size_t n_shards = p == p_end ? 1 : unserialise_length(&p, p_end);
					auto did_serialised = term.substr(2);
					auto did = sortable_unserialise(did_serialised);
					if (did == 0u) {
						if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
							throw Xapian::DocVersionConflictError("Version mismatch!");
						}
						info.did = wdb->get_lastdocid() + 1;
						did = (info.did - 1) * n_shards + shard_num + 1;  // unshard number and shard docid to docid in multi-db
						ver_prefix = "V" + serialise_length(info.did);
						did_serialised = sortable_serialise(did);
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
					} else {
						info.did = (did - 1) / n_shards + 1;  // docid in the multi-db to the docid in the shard
						ver_prefix = "V" + serialise_length(info.did);
						auto ver_prefix_size = ver_prefix.size();
						auto vit = wdb->allterms_begin(ver_prefix);
						auto vit_e = wdb->allterms_end(ver_prefix);
						if (vit == vit_e) {
							if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
								throw Xapian::DocVersionConflictError("Version mismatch!");
							}
						}
						for (; vit != vit_e; ++vit) {
							std::string current_term = *vit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (!ver.empty() && ver != current_ver) {
									// Throw error about wrong version!
									throw Xapian::DocVersionConflictError("Version mismatch!");
								}
								info.version = sortable_unserialise(current_ver);
								break;
							}
						}
					}
				} else {
					auto it = wdb->postlist_begin(term);
					auto it_e = wdb->postlist_end(term);
					if (it == it_e) {
						info.did = wdb->get_lastdocid() + 1;
						ver_prefix = "V" + serialise_length(info.did);
						if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
							throw Xapian::DocVersionConflictError("Version mismatch!");
						}
					} else {
						info.did = *it;
						ver_prefix = "V" + serialise_length(info.did);
						auto ver_prefix_size = ver_prefix.size();
						auto vit = wdb->allterms_begin(ver_prefix);
						auto vit_e = wdb->allterms_end(ver_prefix);
						if (vit == vit_e) {
							if (!ver.empty() && ver != "\x80") {  // "\x80" = sortable_serialise(0)
								throw Xapian::DocVersionConflictError("Version mismatch!");
							}
						}
						for (; vit != vit_e; ++vit) {
							std::string current_term = *vit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (!ver.empty() && ver != current_ver) {
									// Throw error about wrong version!
									throw Xapian::DocVersionConflictError("Version mismatch!");
								}
								info.version = sortable_unserialise(current_ver);
								break;
							}
						}
					}
				}
				ver = sortable_serialise(++info.version);
				auto it = data_obj.find(VERSION_FIELD_NAME);
				if (it != data_obj.end()) {
					auto& value = it.value();
					value = info.version;
					data_modified = true;
				}
				doc.add_boolean_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
				doc.add_value(DB_SLOT_SHARDS, "");  // remove shards slot
				if (data_modified) {
					data.set_obj(data_obj);
					data.flush();
					doc.set_data(data.serialise());
				}

				assert(info.did);
				wdb->replace_document(info.did, doc);
			} else {
				info = wdb->replace_document(term, doc);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			wdb->add_spelling(word, freqinc);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::remove_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::remove_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::termcount result = 0;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			result = wdb->remove_spelling(word, freqdec);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

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
		} catch (const Xapian::InvalidArgumentError&) {
			throw Xapian::DocNotFoundError("Document not found");
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

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
		} catch (const Xapian::InvalidArgumentError&) {
			throw Xapian::DocNotFoundError("Document not found");
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::set_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::set_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

#ifdef XAPIAND_CLUSTERING
	if (!Node::quorum()) {
		throw Xapian::DatabaseNotAvailableError("Cluster has no quorum");
	}
#endif

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			wdb->set_metadata(key, value);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			if (local) {
				auto prior_revision = wdb->get_revision();
				if (!is_replica()) {
					endpoint.pending_revision.store(prior_revision + 1, std::memory_order_relaxed);
				}
#if XAPIAND_DATABASE_WAL
				if (wal_ && is_wal_active()) {
					auto uuid = wdb->get_uuid();
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
		wdb = static_cast<Xapian::WritableDatabase *>(db());
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
