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
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "cassert.h"              // for ASSERT
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
#include "storage.h"              // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "string.hh"              // for string::from_delta, string::format

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


#define XAPIAN_LOCAL_DB_FALLBACK 1

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
	ASSERT(shard);

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
Shard::Shard(ShardEndpoint& endpoint_, int flags_)
	: reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0),
	  busy(false),
	  _local(false),
	  _closed(false),
	  _modified(false),
	  _incomplete(false),
	  transaction(Transaction::none),
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

	ASSERT(!endpoint.empty());
	Xapian::WritableDatabase wsdb;
	bool local = false;
	int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
		? Xapian::DB_CREATE_OR_OPEN
		: Xapian::DB_OPEN;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote writable shard {}", repr(endpoint.to_string()));
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		auto node = endpoint.node();
		if (!node) {
			throw Xapian::NetworkError("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			throw Xapian::NetworkError("Endpoint node is inactive");
		}
		if (node->remote_port == 0) {
			throw Xapian::NetworkError("Endpoint node without a valid port");
		}
		wsdb = Xapian::Remote::open_writable(node->host(), node->remote_port, 10000, 10000, _flags | XAPIAN_DB_SYNC_MODE, endpoint.path);
		// Writable remote databases do not have a local fallback
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local writable shard {}", repr(endpoint.to_string()));
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			wsdb = Xapian::WritableDatabase(endpoint.path, Xapian::DB_OPEN | XAPIAN_DB_SYNC_MODE);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN) {
				throw;
			}
			build_path_index(endpoint.path);
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			wsdb = Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE | XAPIAN_DB_SYNC_MODE);
			created = true;
		}
		local = true;
	}

	new_database->add_database(wsdb);

	_local.store(local, std::memory_order_relaxed);
	if (local) {
		reopen_revision = new_database->get_revision();
		endpoint.local_revision = reopen_revision;
	}

	if (transaction != Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(new_database.get());
		wdb->begin_transaction(transaction == Transaction::flushed);
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
	reopen_time = std::chrono::system_clock::now();

#ifdef XAPIAND_DATABASE_WAL
	// If reopen_revision is not available WAL work as a log for the operations
	if (is_wal_active()) {
		// WAL wasn't already active for the requested endpoint
		DatabaseWAL wal(this);
		if (wal.execute(true)) {
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

	ASSERT(!endpoint.empty());
	Xapian::Database rsdb;
	bool local = false;
#ifdef XAPIAND_CLUSTERING
	int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
		? Xapian::DB_CREATE_OR_OPEN
		: Xapian::DB_OPEN;
	if (!endpoint.is_local()) {
		L_DATABASE("Opening remote shard {}", repr(endpoint.to_string()));
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		auto node = endpoint.node();
		if (!node) {
			throw Xapian::NetworkError("Endpoint node is invalid");
		}
		if (!node->is_active()) {
			throw Xapian::NetworkError("Endpoint node is inactive");
		}
		if (node->remote_port == 0) {
			throw Xapian::NetworkError("Endpoint node without a valid port");
		}
		rsdb = Xapian::Remote::open(node->host(), node->remote_port, 10000, 10000, _flags, endpoint.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			Xapian::Database tmp = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
			if (tmp.get_uuid() == rsdb.get_uuid()) {
				L_DATABASE("Endpoint {} fallback to local shard!", repr(endpoint.to_string()));
				// Handle remote endpoint and figure out if the endpoint is a local database
				RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
				rsdb = Xapian::Database(endpoint.path, _flags);
				local = true;
			} else {
				try {
					// If remote is master (it should be), try triggering replication
					trigger_replication()->delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, endpoint.path, Endpoint{endpoint}, Endpoint{endpoint.path});
					_incomplete.store(true, std::memory_order_relaxed);
				} catch (...) { }
			}
		} catch (const Xapian::DatabaseNotFoundError& exc) {
		} catch (const Xapian::DatabaseOpeningError& exc) {
			try {
				// If remote is master (it should be), try triggering replication
				trigger_replication()->delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, endpoint.path, Endpoint{endpoint}, Endpoint{endpoint.path});
				_incomplete.store(true, std::memory_order_relaxed);
			} catch (...) { }
		}
#endif  // XAPIAN_LOCAL_DB_FALLBACK
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local shard {}", repr(endpoint.to_string()));
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			rsdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN)  {
				throw;
			}
			build_path_index(endpoint.path);
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE);
			created = true;

			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			rsdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
		}
		local = true;
	}

	new_database->add_database(rsdb);

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
	reopen_time = std::chrono::system_clock::now();
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
				} catch (const Xapian::DatabaseModifiedError& exc) {
					if (t == 0) { throw; }
				} catch (const Xapian::DatabaseOpeningError& exc) {
				} catch (const Xapian::NetworkError& exc) {
				} catch (const Xapian::DatabaseError& exc) {
					if (exc.get_msg() != "Database has been closed") {
						throw;
					}
				}
			}
		}

		do_close(true, is_closed(), transaction, false);
	}

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			if (is_writable()) {
				reopen_writable();
			} else {
				reopen_readable();
			}
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { throw; }
		} catch (...) {
			reset();
			throw;
		}
	}

	ASSERT(database);
	L_DATABASE("Reopen: {}", __repr__());
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
	L_CALL("Shard::do_close({}, {}, {}, {}) {{endpoint:{}, database:{}, modified:{}, closed:{}}}", commit_, closed_, repr(to_string()), database ? "<database>" : "null", is_modified(), is_closed(), transaction == Shard::Transaction::none ? "<none>" : "<transaction>", throw_exceptions);

	if (
		commit_ &&
		database &&
		transaction == Shard::Transaction::none &&
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
	transaction = transaction_;
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
	L_CALL("Shard::autocommit(<shard>)");

	if (
		shard->database &&
		shard->transaction == Shard::Transaction::none &&
		!shard->is_closed() &&
		shard->is_modified() &&
		shard->is_writable() &&
		shard->is_local()
	) {
		// Auto commit only on modified writable databases
		committer()->debounce(shard->endpoint, std::weak_ptr<Shard>(shard));
	}
}


bool
Shard::commit([[maybe_unused]] bool wal_, bool send_update)
{
	L_CALL("Shard::commit({})", wal_);

	ASSERT(is_writable());

	if (!is_modified()) {
		L_DATABASE("Commit on shard {} was discarded, because there are not changes", repr(endpoint.to_string()));
		return false;
	}

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::commit:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::commit:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Commit: t: {}", t);
		try {
			auto local = is_local();
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif  // XAPIAND_DATA_STORAGE
			ASSERT(!local || wdb->get_revision() == endpoint.local_revision.load());
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
				auto prior_revision = endpoint.local_revision.load();
				auto current_revision = wdb->get_revision();
				if (prior_revision == current_revision) {
					L_DATABASE("Commit on shard {} was discarded, because it turned out not to change the revision", repr(endpoint.to_string()));
					return false;
				}
				ASSERT(current_revision == prior_revision + 1);
				L_DATABASE("Commit on shard {}: {} -> {}", repr(endpoint.to_string()), prior_revision, current_revision);
				endpoint.local_revision = current_revision;
			}
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(false, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(false, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(false, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::commit:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_commit(*this, send_update); }
#endif

	return true;
}

void
Shard::begin_transaction(bool flushed)
{
	L_CALL("Shard::begin_transaction({})", flushed);

	ASSERT(is_writable());

	if (transaction == Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->begin_transaction(flushed);
		transaction = flushed ? Transaction::flushed : Transaction::unflushed;
	}
}


void
Shard::commit_transaction()
{
	L_CALL("Shard::commit_transaction()");

	ASSERT(is_writable());

	if (transaction != Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->commit_transaction();
		transaction = Transaction::none;
	}
}


void
Shard::cancel_transaction()
{
	L_CALL("Shard::cancel_transaction()");

	ASSERT(is_writable());

	if (transaction != Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->cancel_transaction();
		transaction = Transaction::none;
	}
}


void
Shard::delete_document(Xapian::docid shard_did, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::delete_document({}, {}, {})", shard_did, commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::rev version = 0;  // TODO: Implement version check (version should have required version)
	auto ver = version ? sortable_serialise(version) : std::string();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Deleting document: {}  t: {}", shard_did, t);

		try {
			auto local = is_local();
			if (local) {
				if (version && version_) {
					auto ver_prefix = "V" + serialise_length(shard_did);
					auto ver_prefix_size = ver_prefix.size();
					auto t_end = wdb->allterms_end(ver_prefix);
					for (auto tit = wdb->allterms_begin(ver_prefix); tit != t_end; ++tit) {
						std::string current_term = *tit;
						std::string_view current_ver(current_term);
						current_ver.remove_prefix(ver_prefix_size);
						if (!current_ver.empty()) {
							if (!ver.empty() && ver != current_ver) {
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
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::delete_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_delete_document(*this, shard_did); }
#endif

	if (commit_) {
		commit(wal_);
	}
}


void
Shard::delete_document_term(const std::string& term, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::delete_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::delete_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::rev version = 0;  // TODO: Implement version check (version should have required version)
	Xapian::docid shard_did = 0;
	auto ver = version ? sortable_serialise(version) : std::string();

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Deleting document: '{}'  t: {}", term, t);
		shard_did = 0;

		try {
			auto local = is_local();
			if (local) {
				auto it = wdb->postlist_begin(term);
				if (it == wdb->postlist_end(term)) {
					throw Xapian::DocNotFoundError("Document not found");
				} else {
					shard_did = *it;
					if (version && version_) {
						auto ver_prefix = "V" + serialise_length(shard_did);
						auto ver_prefix_size = ver_prefix.size();
						auto t_end = wdb->allterms_end(ver_prefix);
						for (auto tit = wdb->allterms_begin(ver_prefix); tit != t_end; ++tit) {
							std::string current_term = *tit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (!ver.empty() && ver != current_ver) {
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
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::delete_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_delete_document(*this, shard_did); }
#endif

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Shard::storage_get_stored(const Locator& locator)
{
	L_CALL("Shard::storage_get_stored()");

	ASSERT(locator.type == Locator::Type::stored || locator.type == Locator::Type::compressed_stored);
	ASSERT(locator.volume != -1);

	if (storage) {
		storage->open(string::format(DATA_STORAGE_PATH "{}", locator.volume));
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

	ASSERT(is_writable());

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
								writable_storage->open(string::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
							}
							offset = writable_storage->write(serialise_strings({ locator.ct_type.to_string(), locator.raw }));
							break;
						} catch (StorageEOF) {
							++writable_storage->volume;
							writable_storage->open(string::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
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


Xapian::docid
Shard::add_document(Xapian::Document&& doc, bool commit_, bool wal_, bool)
{
	L_CALL("Shard::add_document(<doc>, {}, {})", commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::rev version = 0;
	Xapian::docid shard_did = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Adding new document.  t: {}", t);
		version = 0;
		shard_did = 0;

		try {
			auto local = is_local();
			if (local) {
				shard_did = wdb->get_lastdocid() + 1;
				auto ver_prefix = "V" + serialise_length(shard_did);
				ver = sortable_serialise(++version);
				doc.add_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
			}
			if (shard_did) {
				wdb->replace_document(shard_did, doc);
			} else {
				shard_did = wdb->add_document(doc);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::add_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_replace_document(*this, shard_did, std::move(doc));
	}
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return shard_did;
}


Xapian::docid
Shard::replace_document(Xapian::docid shard_did, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document({}, <doc>, {}, {})", shard_did, commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::rev version = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Replacing: {}  t: {}", shard_did, t);
		version = 0;

		try {
			auto local = is_local();
			if (local) {
				auto ver_prefix = "V" + serialise_length(shard_did);
				auto ver_prefix_size = ver_prefix.size();
				auto t_end = wdb->allterms_end(ver_prefix);
				for (auto tit = wdb->allterms_begin(ver_prefix); tit != t_end; ++tit) {
					std::string current_term = *tit;
					std::string_view current_ver(current_term);
					current_ver.remove_prefix(ver_prefix_size);
					if (!current_ver.empty()) {
						if (version_ && !ver.empty() && ver != current_ver) {
							// Throw error about wrong version!
							throw Xapian::DocVersionConflictError("Version mismatch!");
						}
						version = sortable_unserialise(current_ver);
						break;
					}
				}
				ver = sortable_serialise(++version);
				doc.add_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
			}
			wdb->replace_document(shard_did, doc);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::replace_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_replace_document(*this, shard_did, std::move(doc));
	}
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return shard_did;
}


Xapian::docid
Shard::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document_term({}, <doc>, {}, {}, {})", repr(term), commit_, wal_, version_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::replace_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::replace_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	std::string new_term;
	Xapian::rev version = 0;
	Xapian::docid shard_did = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);
	auto n_shards_ser = doc.get_value(DB_SLOT_SHARDS);

	for (int t = DB_RETRIES; t >= 0; --t) {
		// L_DATABASE("Replacing: '{}'  t: {}", term, t);
		version = 0;
		shard_did = 0;

		try {
			auto local = is_local();
			if (local) {
				std::string ver_prefix;
				ASSERT(term.size() > 2);
				if (term[0] == 'Q' && term[1] == 'N') {
					const char *p = n_shards_ser.data();
					const char *p_end = p + n_shards_ser.size();
					size_t shard_num = p == p_end ? 0 : unserialise_length(&p, p_end);
					size_t n_shards = p == p_end ? 1 : unserialise_length(&p, p_end);
					auto did_serialised = term.substr(2);
					auto did = sortable_unserialise(did_serialised);
					if (did == 0) {
						shard_did = wdb->get_lastdocid() + 1;
						did = (shard_did - 1) * n_shards + shard_num + 1;  // shard number and shard docid to docid in multi-db
						ver_prefix = "V" + serialise_length(shard_did);
						did_serialised = sortable_serialise(did);
						new_term = "QN" + did_serialised;
						doc.add_boolean_term(new_term);
						doc.add_value(DB_SLOT_ID, did_serialised);
						// Set id inside serialized object:
						Data data(doc.get_data());
						auto data_obj = data.get_obj();
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
							data.set_obj(data_obj);
							data.flush();
							doc.set_data(data.serialise());
						}
					} else {
						shard_did = (did - 1) / n_shards + 1;  // docid in the multi-db to the docid in the shard
						ver_prefix = "V" + serialise_length(shard_did);
						auto ver_prefix_size = ver_prefix.size();
						auto t_end = wdb->allterms_end(ver_prefix);
						for (auto tit = wdb->allterms_begin(ver_prefix); tit != t_end; ++tit) {
							std::string current_term = *tit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (version_ && !ver.empty() && ver != current_ver) {
									// Throw error about wrong version!
									throw Xapian::DocVersionConflictError("Version mismatch!");
								}
								version = sortable_unserialise(current_ver);
								break;
							}
						}
					}
				} else {
					auto it = wdb->postlist_begin(term);
					if (it == wdb->postlist_end(term)) {
						shard_did = wdb->get_lastdocid() + 1;
						ver_prefix = "V" + serialise_length(shard_did);
					} else {
						shard_did = *it;
						ver_prefix = "V" + serialise_length(shard_did);
						auto ver_prefix_size = ver_prefix.size();
						auto t_end = wdb->allterms_end(ver_prefix);
						for (auto tit = wdb->allterms_begin(ver_prefix); tit != t_end; ++tit) {
							std::string current_term = *tit;
							std::string_view current_ver(current_term);
							current_ver.remove_prefix(ver_prefix_size);
							if (!current_ver.empty()) {
								if (version_ && !ver.empty() && ver != current_ver) {
									// Throw error about wrong version!
									throw Xapian::DocVersionConflictError("Version mismatch!");
								}
								version = sortable_unserialise(current_ver);
								break;
							}
						}
					}
				}
				ver = sortable_serialise(++version);
				doc.add_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
				doc.add_value(DB_SLOT_SHARDS, "");  // remove shards slot
			}
			if (shard_did) {
				wdb->replace_document(shard_did, doc);
			} else {
				shard_did = wdb->replace_document(term, doc);
			}
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::replace_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_replace_document(*this, shard_did, std::move(doc));
	}
#endif

	if (commit_) {
		commit(wal_);
	}

	return shard_did;
}


void
Shard::add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL("Shard::add_spelling(<word, <freqinc>, {}, {})", commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::add_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::add_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			wdb->add_spelling(word, freqinc);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::add_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_add_spelling(*this, word, freqinc); }
#endif

	if (commit_) {
		commit(wal_);
	}
}


Xapian::termcount
Shard::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Shard::remove_spelling(<word>, <freqdec>, {}, {})", commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::remove_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::remove_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::termcount result = 0;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			result = wdb->remove_spelling(word, freqdec);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::remove_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_remove_spelling(*this, word, freqdec); }
#endif

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
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
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
Shard::get_document(Xapian::docid shard_did, bool assume_valid_)
{
	L_CALL("Shard::get_document({})", shard_did);

	Xapian::Document doc;

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::get_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::get_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			if (assume_valid_) {
				doc = rdb->get_document(shard_did, Xapian::DOC_ASSUME_VALID);
			} else {
				doc = rdb->get_document(shard_did);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
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
			storage->open(string::format(DATA_STORAGE_PATH "{}", volume));
			storage->seek(static_cast<uint32_t>(offset));
			return storage->read();
		}
	}

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			value = rdb->get_metadata(key);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
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
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
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

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Shard::set_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Shard::set_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto local = is_local();
			wdb->set_metadata(key, value);
			_modified.store(commit_ || local, std::memory_order_relaxed);
			break;
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, transaction, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, transaction, false); throw; }
				do_close(false, is_closed(), transaction, false);
			} else {
				do_close(false, is_closed(), transaction, false);
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Shard::set_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_set_metadata(*this, key, value); }
#endif

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
	return string::format("<Shard {} ({}){}{}{}{}{}{}{}>",
		repr(to_string()),
		readable_flags(flags),
		is_writable() ? " (writable)" : "",
		is_wal_active() ? " (active WAL)" : "",
		is_local() ? " (local)" : "",
		is_closed() ? " (closed)" : "",
		is_modified() ? " (modified)" : "",
		is_incomplete() ? " (incomplete)" : "",
		is_busy() ? " (busy)" : "");
}
