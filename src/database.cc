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

#include "database.h"

#include <algorithm>              // for std::move
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "cassert.h"              // for ASSERT
#include "database_data.h"        // for Locator
#include "database_flags.h"       // DB_*
#include "database_pool.h"        // for DatabaseEndpoint
#include "database_handler.h"     // for committer
#include "database_utils.h"       // for DB_SLOT_VERSION
#include "database_wal.h"         // for DatabaseWAL, DatabaseWALWriter
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "fs.hh"                  // for exists, build_path_index
#include "ignore_unused.h"        // for ignore_unused
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_OBJ, L_CALL
#include "lz4/xxhash.h"           // for XXH32_update, XXH32_state_t
#include "manager.h"              // for XapiandManager, sig_exit, trigger_replication
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
// #undef L_DATABASE_BEGIN
// #define L_DATABASE_BEGIN L_DELAYED_600
// #undef L_DATABASE_END
// #define L_DATABASE_END L_DELAYED_N_UNLOG
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


//  ____        _        ____  _
// |  _ \  __ _| |_ __ _/ ___|| |_ ___  _ __ __ _  __ _  ___
// | | | |/ _` | __/ _` \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
// | |_| | (_| | || (_| |___) | || (_) | | | (_| | (_| |  __/
// |____/ \__,_|\__\__,_|____/ \__\___/|_|  \__,_|\__, |\___|
//                                                |___/
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
	auto database = static_cast<Database*>(param);
	ASSERT(database);

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

	auto database = static_cast<Database*>(param);
	if (UUID(head.uuid) != database->get_uuid()) {
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


//  ____        _        _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|
//

Database::Database(std::vector<std::shared_ptr<Database>>&& shards_, const Endpoints& endpoints_, int flags_)
	: endpoint(nullptr),
	  flags(flags_),
	  busy(false),
	  reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0),
	  local(false),
	  closed(false),
	  modified(false),
	  incomplete(false),
#ifdef XAPIAND_DATABASE_WAL
	  producer_token(nullptr),
#endif
	  transaction(Transaction::none),
	  _shards(std::move(shards_)),
	  endpoints(endpoints_)
{
	reopen();
}


Database::Database(DatabaseEndpoint* endpoint_, int flags_)
	: endpoint(endpoint_),
	  flags(flags_),
	  busy(false),
	  reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0),
	  local(false),
	  closed(false),
	  modified(false),
	  incomplete(false),
#ifdef XAPIAND_DATABASE_WAL
	  producer_token(nullptr),
#endif
	  transaction(Transaction::none)
{
	reopen();
}


Database::~Database() noexcept
{
	try {
		do_close(true, true, Database::Transaction::none, false);
		if (log) {
			log->clear();
		}
#ifdef XAPIAND_DATABASE_WAL
		if (producer_token) {
			ASSERT(endpoint);
			XapiandManager::wal_writer()->dec_producer_token(endpoint->path);
		}
#endif
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


bool
Database::reopen_writable()
{
	////////////////////////////////////////////////////////////////
	// __        __    _ _        _     _        ____  ____
	// \ \      / / __(_) |_ __ _| |__ | | ___  |  _ \| __ )
	//  \ \ /\ / / '__| | __/ _` | '_ \| |/ _ \ | | | |  _ \.
	//   \ V  V /| |  | | || (_| | |_) | |  __/ | |_| | |_) |
	//    \_/\_/ |_|  |_|\__\__,_|_.__/|_|\___| |____/|____/
	//
	L_CALL("Database::reopen_writable()");

	bool created = false;

	if (is_closed()) {
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	reset();

	auto database = std::make_unique<Xapian::WritableDatabase>();

	local.store(true, std::memory_order_relaxed);

	ASSERT(endpoint);
	ASSERT(!endpoint->empty());
#ifdef XAPIAND_CLUSTERING
	auto node = endpoint->node();
	if (!node) {
		throw Xapian::NetworkError("Endpoint node is invalid");
	}
	if (node->remote_port == 0) {
		throw Xapian::NetworkError("Endpoint node without a valid port");
	}
	if (!node->is_active()) {
		throw Xapian::NetworkError("Endpoint node is inactive");
	}
#endif  // XAPIAND_CLUSTERING

	Xapian::WritableDatabase wsdb;
	bool localdb = false;
	int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
		? Xapian::DB_CREATE_OR_OPEN
		: Xapian::DB_OPEN;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint->is_local()) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		wsdb = Xapian::Remote::open_writable(node->host(), node->remote_port, 10000, 10000, _flags | XAPIAN_DB_SYNC_MODE, endpoint->path);
		// Writable remote databases do not have a local fallback
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			wsdb = Xapian::WritableDatabase(endpoint->path, Xapian::DB_OPEN | XAPIAN_DB_SYNC_MODE);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN) {
				throw;
			}
			build_path_index(endpoint->path);
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			wsdb = Xapian::WritableDatabase(endpoint->path, Xapian::DB_CREATE | XAPIAN_DB_SYNC_MODE);
			created = true;
		}
		localdb = true;
	}

	database->add_database(wsdb);

	if (localdb) {
		reopen_revision = database->get_revision();
	} else {
		local.store(false, std::memory_order_relaxed);
	}

	if (transaction != Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(database.get());
		wdb->begin_transaction(transaction == Transaction::flushed);
	}

#ifdef XAPIAND_DATA_STORAGE
	if (localdb) {
		if ((flags & DB_NOSTORAGE) != DB_NOSTORAGE) {
			writable_storage = std::make_unique<DataStorage>(endpoint->path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE);
			storage = std::make_unique<DataStorage>(endpoint->path, this, STORAGE_OPEN);
		} else {
			writable_storage = std::unique_ptr<DataStorage>(nullptr);
			storage = std::make_unique<DataStorage>(endpoint->path, this, STORAGE_OPEN);
		}
	} else {
		writable_storage = std::unique_ptr<DataStorage>(nullptr);
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	_database = std::move(database);
	reopen_time = std::chrono::system_clock::now();

#ifdef XAPIAND_DATABASE_WAL
	// If reopen_revision is not available WAL work as a log for the operations
	if (is_wal_active()) {
		// Create or get a producer token for this database
		if (XapiandManager::wal_writer()->inc_producer_token(endpoint->path, &producer_token) == 1) {
			// WAL wasn't already active for the requested endpoint
			DatabaseWAL wal(this);
			if (wal.execute(true)) {
				modified.store(true, std::memory_order_relaxed);
			}
		}
	}
#endif  // XAPIAND_DATABASE_WAL

	// Ends Writable DB
	////////////////////////////////////////////////////////////////

	return created;
}

bool
Database::reopen_readable()
{
	////////////////////////////////////////////////////////////////
	//  ____                _       _     _        ____  ____
	// |  _ \ ___  __ _  __| | __ _| |__ | | ___  |  _ \| __ )
	// | |_) / _ \/ _` |/ _` |/ _` | '_ \| |/ _ \ | | | |  _ \.
	// |  _ <  __/ (_| | (_| | (_| | |_) | |  __/ | |_| | |_) |
	// |_| \_\___|\__,_|\__,_|\__,_|_.__/|_|\___| |____/|____/
	//
	L_CALL("Database::reopen_readable()");

	bool created = false;

	if (is_closed()) {
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	reset();

	auto database = std::make_unique<Xapian::Database>();

	local.store(true, std::memory_order_relaxed);

	ASSERT(endpoint);
	ASSERT(!endpoint->empty());
#ifdef XAPIAND_CLUSTERING
	auto node = endpoint->node();
	if (!node) {
		throw Xapian::NetworkError("Endpoint node is invalid");
	}
	if (node->remote_port == 0) {
		throw Xapian::NetworkError("Endpoint node without a valid port");
	}
	if (!node->is_active()) {
		throw Xapian::NetworkError("Endpoint node is inactive");
	}
#endif  // XAPIAND_CLUSTERING

	Xapian::Database rsdb;
	bool localdb = false;
#ifdef XAPIAND_CLUSTERING
	int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
		? Xapian::DB_CREATE_OR_OPEN
		: Xapian::DB_OPEN;
	if (!endpoint->is_local()) {
		L_DATABASE("Opening remote endpoint {}", repr(endpoint->to_string()));
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
		rsdb = Xapian::Remote::open(node->host(), node->remote_port, 10000, 10000, _flags, endpoint->path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			Xapian::Database tmp = Xapian::Database(endpoint->path, Xapian::DB_OPEN);
			if (tmp.get_uuid() == rsdb.get_uuid()) {
				L_DATABASE("Endpoint {} fallback to local database!", repr(endpoint->to_string()));
				// Handle remote endpoint and figure out if the endpoint is a local database
				RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
				rsdb = Xapian::Database(endpoint->path, _flags);
				localdb = true;
			} else {
				try {
					// If remote is master (it should be), try triggering replication
					trigger_replication()->delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, endpoint->path, Endpoint{*endpoint}, Endpoint{endpoint->path});
					incomplete.store(true, std::memory_order_relaxed);
				} catch (...) { }
			}
		} catch (const Xapian::DatabaseNotFoundError& exc) {
		} catch (const Xapian::DatabaseOpeningError& exc) {
			try {
				// If remote is master (it should be), try triggering replication
				trigger_replication()->delayed_debounce(std::chrono::milliseconds{random_int(0, 3000)}, endpoint->path, Endpoint{*endpoint}, Endpoint{endpoint->path});
				incomplete.store(true, std::memory_order_relaxed);
			} catch (...) { }
		}
#endif  // XAPIAN_LOCAL_DB_FALLBACK
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		L_DATABASE("Opening local endpoint {}", repr(endpoint->to_string()));
		try {
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			rsdb = Xapian::Database(endpoint->path, Xapian::DB_OPEN);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN)  {
				throw;
			}
			build_path_index(endpoint->path);
			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			Xapian::WritableDatabase(endpoint->path, Xapian::DB_CREATE);
			created = true;

			RANDOM_ERRORS_DB_THROW(Xapian::DatabaseOpeningError, "Random Error");
			rsdb = Xapian::Database(endpoint->path, Xapian::DB_OPEN);
		}
		localdb = true;
	}

	database->add_database(rsdb);

	if (localdb) {
		reopen_revision = database->get_revision();
	} else {
		local.store(false, std::memory_order_relaxed);
	}

#ifdef XAPIAND_DATA_STORAGE
	if (localdb) {
		// WAL required on a local database, open it.
		storage = std::make_unique<DataStorage>(endpoint->path, this, STORAGE_OPEN);
	} else {
		storage = std::unique_ptr<DataStorage>(nullptr);
	}
#endif  // XAPIAND_DATA_STORAGE

	_database = std::move(database);
	reopen_time = std::chrono::system_clock::now();
	// Ends Readable DB
	////////////////////////////////////////////////////////////////

	return created;
}


bool
Database::reopen()
{
	L_CALL("Database::reopen() {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	L_DATABASE_WRAP_BEGIN("Database::reopen:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::reopen:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	if (endpoint) {
		if (_database) {
			if (!is_incomplete()) {
				// Try to reopen
				for (int t = DB_RETRIES; t; --t) {
					try {
						bool ret = _database->reopen();
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

		try {
			if (is_writable()) {
				reopen_writable();
			} else {
				reopen_readable();
			}
		} catch (...) {
			reset();
			throw;
		}
	} else {
		_database = std::make_unique<Xapian::Database>();
		for (auto& shard : _shards) {
			_database->add_database(*shard->db());
		}
	}

	ASSERT(_database);
	L_DATABASE("Reopen: {}", __repr__());
	return true;
}


Xapian::Database*
Database::db()
{
	L_CALL("Database::db()");

	if (is_closed()) {
		throw Xapian::DatabaseClosedError("Database has been closed");
	}

	if (!_database) {
		reopen();
	}

	return _database.get();
}


UUID
Database::get_uuid()
{
	L_CALL("Database::get_uuid()");

	return UUID(get_uuid_string());
}


std::string
Database::get_uuid_string()
{
	L_CALL("Database::get_uuid_string()");

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	return db()->get_uuid();
}


Xapian::rev
Database::get_revision()
{
	L_CALL("Database::get_revision()");

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	return db()->get_revision();
}


void
Database::reset() noexcept
{
	L_CALL("Database::reset()");

	try {
		_database.reset();
	} catch(...) {}
	reopen_revision = 0;
	local.store(false, std::memory_order_relaxed);
	closed.store(false, std::memory_order_relaxed);
	modified.store(false, std::memory_order_relaxed);
	incomplete.store(false, std::memory_order_relaxed);
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
Database::do_close(bool commit_, bool closed_, Transaction transaction_, bool throw_exceptions)
{
	L_CALL("Database::do_close({}, {}, {}, {}) {{endpoint:{}, database:{}, modified:{}, closed:{}}}", commit_, closed_, repr(to_string()), _database ? "<database>" : "null", is_modified(), is_closed(), transaction == Database::Transaction::none ? "<none>" : "<transaction>", throw_exceptions);

	if (
		commit_ &&
		_database &&
		transaction == Database::Transaction::none &&
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

	if (_database) {
		try {
			_database->close();
		} catch (...) {
			if (throw_exceptions) {
				throw;
			}
			L_WARNING("WARNING: Internal database close failed!");
		}
	}

	bool local_ = is_local();

	reset();

	local.store(local_, std::memory_order_relaxed);
	closed.store(closed_, std::memory_order_relaxed);
	transaction = transaction_;
}


void
Database::close()
{
	L_CALL("Database::close()");

	if (is_closed()) {
		return;
	}

	do_close(true, true, Transaction::none);
}


void
Database::autocommit(const std::shared_ptr<Database>& database)
{
	L_CALL("Database::autocommit(<database>)");

	if (
		database->endpoint &&
		database->_database &&
		database->transaction == Database::Transaction::none &&
		!database->is_closed() &&
		database->is_modified() &&
		database->is_writable() &&
		database->is_local()
	) {
		// Auto commit only on modified writable databases
		committer()->debounce(*database->endpoint, std::weak_ptr<Database>(database));
	}
}


bool
Database::commit(bool wal_, bool send_update)
{
	L_CALL("Database::commit({})", wal_);

	ASSERT(is_writable());

	if (!is_modified()) {
		L_DATABASE("Do not commit, because there are not changes");
		return false;
	}

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::commit:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::commit:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Commit: t: {}", t);
		try {
#ifdef XAPIAND_DATA_STORAGE
			storage_commit();
#endif  // XAPIAND_DATA_STORAGE
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
			modified.store(false, std::memory_order_relaxed);
			if (is_local()) {
				endpoint->local_revision = wdb->get_revision();
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::commit:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_commit(*this, send_update); }
#else
	ignore_unused(wal_);
#endif

	return true;
}


void
Database::begin_transaction(bool flushed)
{
	L_CALL("Database::begin_transaction({})", flushed);

	ASSERT(is_writable());

	if (transaction == Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->begin_transaction(flushed);
		transaction = flushed ? Transaction::flushed : Transaction::unflushed;
	}
}


void
Database::commit_transaction()
{
	L_CALL("Database::commit_transaction()");

	ASSERT(is_writable());

	if (transaction != Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->commit_transaction();
		transaction = Transaction::none;
	}
}


void
Database::cancel_transaction()
{
	L_CALL("Database::cancel_transaction()");

	ASSERT(is_writable());

	if (transaction != Transaction::none) {
		RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

		auto *wdb = static_cast<Xapian::WritableDatabase *>(db());
		wdb->cancel_transaction();
		transaction = Transaction::none;
	}
}


void
Database::delete_document(Xapian::docid did, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::delete_document({}, {}, {})", did, commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::delete_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::delete_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::rev version = 0;  // TODO: Implement version check (version should have required version)
	auto ver = sortable_serialise(version);

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Deleting document: {}  t: {}", did, t);

		try {
			if (is_local() && version && version_) {
				std::string ver_prefix;
				ver_prefix = "V" + serialise_length(did);
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
			wdb->delete_document(did);
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::delete_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_delete_document(*this, did); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


void
Database::delete_document_term(const std::string& term, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::delete_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::delete_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	Xapian::rev version = 0;  // TODO: Implement version check (version should have required version)
	Xapian::docid did = 0;
	auto ver = sortable_serialise(version);

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Deleting document: '{}'  t: {}", term, t);
		did = 0;

		try {
			if (is_local() && version && version_) {
				std::string ver_prefix;
				auto it = wdb->postlist_begin(term);
				if (it == wdb->postlist_end(term)) {
					return;
				} else {
					did = *it;
					ver_prefix = "V" + serialise_length(did);
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
			if (did) {
				wdb->delete_document(did);
			} else {
				wdb->delete_document(term);
			}
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::delete_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_delete_document_term(*this, term); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Database::storage_get_stored(const Locator& locator)
{
	L_CALL("Database::storage_get_stored()");

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
Database::storage_push_blobs(std::string&& doc_data)
{
	L_CALL("Database::storage_push_blobs()");

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
Database::storage_commit()
{
	L_CALL("Database::storage_commit()");

	if (writable_storage) {
		writable_storage->commit();
	}
}
#endif  // XAPIAND_DATA_STORAGE


Xapian::docid
Database::add_document(Xapian::Document&& doc, bool commit_, bool wal_, bool)
{
	L_CALL("Database::add_document(<doc>, {}, {})", commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::add_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::add_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::rev version = 0;
	Xapian::docid did = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Adding new document.  t: {}", t);
		version = 0;
		did = 0;

		try {
			if (is_local()) {
				std::string ver_prefix;
				did = wdb->get_lastdocid() + 1;
				ver_prefix = "V" + serialise_length(did);
				ver = sortable_serialise(++version);
				doc.add_term(ver_prefix + ver);
				doc.add_value(DB_SLOT_VERSION, ver);  // Update version
			}
			if (did) {
				wdb->replace_document(did, doc);
			} else {
				did = wdb->add_document(doc);
			}
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::add_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_add_document(*this, std::move(doc));
	}
#else
	ignore_unused(wal_);
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::replace_document({}, <doc>, {}, {})", did, commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::replace_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::replace_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	Xapian::rev version = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Replacing: {}  t: {}", did, t);
		version = 0;

		try {
			if (is_local()) {
				std::string ver_prefix;
				ver_prefix = "V" + serialise_length(did);
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
			wdb->replace_document(did, doc);
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::replace_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_replace_document(*this, did, std::move(doc));
	}
#else
	ignore_unused(wal_);
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::replace_document_term({}, <doc>, {}, {})", repr(term), commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::replace_document_term:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::replace_document_term:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = storage_push_blobs(doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#endif  // XAPIAND_DATA_STORAGE

	std::string new_term;
	Xapian::rev version = 0;
	Xapian::docid did = 0;
	auto ver = doc.get_value(DB_SLOT_VERSION);

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE("Replacing: '{}'  t: {}", term, t);
		version = 0;
		did = 0;

		try {
			if (is_local()) {
				std::string ver_prefix;
				if (term == "QN\x80") {
					// Special term for autoincrement
					did = wdb->get_lastdocid() + 1;
					ver_prefix = "V" + serialise_length(did);
					auto did_serialised = serialise_length(did);
					new_term = "QN" + did_serialised;
					doc.add_boolean_term(new_term);
					doc.add_value(DB_SLOT_ID, did_serialised);
					// Set id inside serialized object:
					Data data(doc.get_data());
					auto data_obj = data.get_obj();
					auto it = data_obj.find(ID_FIELD_NAME);
					if (it != data_obj.end()) {
						auto& value = it.value();
						switch (value.getType()) {
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
					auto it = wdb->postlist_begin(term);
					if (it == wdb->postlist_end(term)) {
						did = wdb->get_lastdocid() + 1;
						ver_prefix = "V" + serialise_length(did);
					} else {
						did = *it;
						ver_prefix = "V" + serialise_length(did);
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
			}
			if (did) {
				wdb->replace_document(did, doc);
			} else {
				did = wdb->replace_document(term, doc);
			}
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::replace_document_term:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		XapiandManager::wal_writer()->write_replace_document_term(*this, new_term.empty() ? term : new_term, std::move(doc));
	}
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
	L_CALL("Database::add_spelling(<word, <freqinc>, {}, {})", commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::add_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::add_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t; --t) {
		try {
			wdb->add_spelling(word, freqinc);
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::add_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_add_spelling(*this, word, freqinc); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


Xapian::termcount
Database::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Database::remove_spelling(<word>, <freqdec>, {}, {})", commit_, wal_);

	Xapian::termcount result = 0;

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::remove_spelling:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::remove_spelling:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t; --t) {
		try {
			result = wdb->remove_spelling(word, freqdec);
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::remove_spelling:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_remove_spelling(*this, word, freqdec); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}

	return result;
}


Xapian::docid
Database::find_document(const std::string& term_id)
{
	L_CALL("Database::find_document({})", repr(term_id));

	Xapian::docid did = 0;

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::find_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::find_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t; --t) {
		try {
			auto it = rdb->postlist_begin(term_id);
			if (it == rdb->postlist_end(term_id)) {
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
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			throw Xapian::DocNotFoundError("Document not found");
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::find_document:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return did;
}


Xapian::Document
Database::get_document(Xapian::docid did, bool assume_valid_)
{
	L_CALL("Database::get_document({})", did);

	Xapian::Document doc;

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::get_document:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::get_document:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t; --t) {
		try {
			if (assume_valid_) {
				doc = rdb->get_document(did, Xapian::DOC_ASSUME_VALID);
			} else {
				doc = rdb->get_document(did);
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
Database::get_metadata(const std::string& key)
{
	L_CALL("Database::get_metadata({})", repr(key));

	std::string value;

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::get_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::get_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

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

	db();
	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t; --t) {
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
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::get_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

	return value;
}


std::vector<std::string>
Database::get_metadata_keys()
{
	L_CALL("Database::get_metadata_keys()");

	std::vector<std::string> values;

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::get_metadata_keys:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::get_metadata_keys:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	for (int t = DB_RETRIES; t; --t) {
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
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::get_metadata_keys:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);

		values.clear();
	}

	return values;
}


void
Database::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL("Database::set_metadata({}, {}, {}, {})", repr(key), repr(value), commit_, wal_);

	ASSERT(is_writable());

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::set_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::set_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *wdb = static_cast<Xapian::WritableDatabase *>(db());

	for (int t = DB_RETRIES; t; --t) {
		try {
			wdb->set_metadata(key, value);
			modified.store(commit_ || is_local(), std::memory_order_relaxed);
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
				throw;
			}
		}
		reopen();
		wdb = static_cast<Xapian::WritableDatabase *>(db());
		L_DATABASE_WRAP_END("Database::set_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);
	}

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_wal_active()) { XapiandManager::wal_writer()->write_set_metadata(*this, key, value); }
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

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::dump_metadata:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::dump_metadata:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	std::string initial;
	for (int t = DB_RETRIES; t; --t) {
		std::string key;
		try {
			auto it = rdb->metadata_keys_begin();
			auto it_e = rdb->metadata_keys_end();
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				key = *it;
				auto value = rdb->get_metadata(key);
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
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::dump_metadata:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);

		initial = key;
	}
}


void
Database::dump_documents(int fd, XXH32_state_t* xxh_state)
{
	L_CALL("Database::dump_documents()");

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::dump_documents:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::dump_documents:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	Xapian::docid initial = 1;
	for (int t = DB_RETRIES; t; --t) {
		Xapian::docid did = initial;
		try {
			auto it = rdb->postlist_begin("");
			auto it_e = rdb->postlist_end("");
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				did = *it;
				auto doc = rdb->get_document(did);
				auto data = Data(doc.get_data());
				for (auto& locator : data) {
					switch (locator.type) {
						case Locator::Type::inplace:
						case Locator::Type::compressed_inplace: {
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
						case Locator::Type::stored:
						case Locator::Type::compressed_stored: {
#ifdef XAPIAND_DATA_STORAGE
							auto stored = storage_get_stored(locator);
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
				throw;
			}
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::dump_documents:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);

		initial = did;
	}
}


MsgPack
Database::dump_documents()
{
	L_CALL("Database::dump_documents()");

	RANDOM_ERRORS_DB_THROW(Xapian::DatabaseError, "Random Error");

	L_DATABASE_WRAP_BEGIN("Database::dump_documents:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::dump_documents:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	auto *rdb = static_cast<Xapian::Database *>(db());

	auto docs = MsgPack::ARRAY();
	Xapian::docid initial = 1;
	for (int t = DB_RETRIES; t; --t) {
		Xapian::docid did = initial;
		try {
			auto it = rdb->postlist_begin("");
			auto it_e = rdb->postlist_end("");
			it.skip_to(initial);
			for (; it != it_e; ++it) {
				did = *it;
				auto doc = rdb->get_document(did);
				auto data = Data(doc.get_data());
				auto main_locator = data.get("");
				auto obj = main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack();
				for (auto& locator : data) {
					switch (locator.type) {
						case Locator::Type::inplace:
						case Locator::Type::compressed_inplace: {
							if (!locator.ct_type.empty()) {
								obj["_data"].push_back(MsgPack({
									{ "_content_type", locator.ct_type.to_string() },
									{ "_type", "inplace" },
									{ "_blob", locator.data() },
								}));
							}
							break;
						}
						case Locator::Type::stored:
						case Locator::Type::compressed_stored: {
#ifdef XAPIAND_DATA_STORAGE
							auto stored = storage_get_stored(locator);
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
				throw;
			}
		}
		reopen();
		rdb = static_cast<Xapian::Database *>(db());
		L_DATABASE_WRAP_END("Database::dump_documents:END {{endpoint:{}, flags:({})}} ({} retries)", repr(to_string()), readable_flags(flags), DB_RETRIES - t);

		initial = did;
	}

	return docs;
}


std::string
Database::to_string() const
{
	return endpoint ? endpoint->to_string() : endpoints.to_string();
}


std::string
Database::__repr__() const
{
	return string::format("<Database {} ({}){}{}{}{}{}{}{}>",
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
