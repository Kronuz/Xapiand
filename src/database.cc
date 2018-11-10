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

#include <algorithm>              // for std::move
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "cassert.hh"             // for assert

#include "database_flags.h"       // DB_*
#include "database_autocommit.h"  // for DatabaseAutocommit
#include "database_pool.h"        // for DatabaseQueue
#include "database_wal.h"         // for DatabaseWAL, DatabaseWALWriter
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "fs.hh"                  // for exists, build_path_index
#include "ignore_unused.h"        // for ignore_unused
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_OBJ, L_CALL
#include "lz4/xxhash.h"           // for XXH32_update, XXH32_state_t
#include "manager.h"              // for XapiandManager::manager, sig_exit
#include "msgpack.h"              // for MsgPack
#include "repr.hh"                // for repr
#include "storage.h"              // for STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "string.hh"              // for string::from_delta, string::format


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DATABASE
// #define L_DATABASE L_SLATE_BLUE


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
	const auto* database = static_cast<const Database*>(param);
	assert(database);

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

Database::Database(std::shared_ptr<DatabaseQueue>& queue_, int flags_)
	: weak_queue(queue_),
	  endpoints(queue_->endpoints),
	  flags(flags_),
	  hash(endpoints.hash()),
	  modified(false),
	  transaction(Transaction::none),
	  reopen_time(std::chrono::system_clock::now()),
	  reopen_revision(0),
	  incomplete(false),
	  closed(false),
	  is_writable(false),
	  is_writable_and_local(false),
	  is_writable_and_local_with_wal(false)
{
	reopen();

	queue_->inc_count();
}


Database::~Database()
{
	try {
		do_close(true, true, Database::Transaction::none);
	} catch (...) {
	}

	if (auto queue = weak_queue.lock()) {
		queue->dec_count();
	}
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
	modified = false;
	dbs.clear();
#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif  // XAPIAND_DATA_STORAGE

	auto endpoints_size = endpoints.size();
	if (endpoints_size != 1) {
		THROW(Error, "Writable database must have one single endpoint");
	}

	db = std::make_unique<Xapian::WritableDatabase>();

	const auto& endpoint = endpoints[0];
	if (endpoint.empty()) {
		THROW(Error, "Database must not have empty endpoints");
	}

	Xapian::WritableDatabase wsdb;
	bool local = false;
	int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
		? Xapian::DB_CREATE_OR_OPEN
		: Xapian::DB_OPEN;
#ifdef XAPIAND_CLUSTERING
	if (!endpoint.is_local()) {
		int port = (endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : endpoint.port;
		wsdb = Xapian::Remote::open_writable(endpoint.host, port, 0, 10000, _flags | XAPIAN_DB_SYNC_MODE, endpoint.path);
		// Writable remote databases do not have a local fallback
	}
	else
#endif  // XAPIAND_CLUSTERING
	{
		if ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN) {
			build_path_index(endpoint.path);
		}
		try {
			wsdb = Xapian::WritableDatabase(endpoint.path, _flags | XAPIAN_DB_SYNC_MODE);
		} catch (const Xapian::DatabaseOpeningError&) {
			if (!exists(endpoint.path + "/iamglass")) {
				if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN) {
					THROW(DatabaseNotFoundError, "Database not found: %s", repr(endpoint.to_string()));
				}
				wsdb = Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE_OR_OVERWRITE | XAPIAN_DB_SYNC_MODE);
			}
			throw;
		}
		local = true;
	}

	db->add_database(wsdb);
	dbs.emplace_back(wsdb, local);

	if (local) {
		reopen_revision = get_revision();
	}

	if (transaction != Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		wdb->begin_transaction(transaction == Transaction::flushed);
	}

#ifdef XAPIAND_DATA_STORAGE
	if (local) {
		if ((flags & DB_NOSTORAGE) == DB_NOSTORAGE) {
			writable_storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS | STORAGE_SYNC_MODE));
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		} else {
			writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		}
	} else {
		writable_storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		storages.push_back(std::unique_ptr<DataStorage>(nullptr));
	}
#endif  // XAPIAND_DATA_STORAGE
	assert(dbs.size() == endpoints_size);

	is_writable = true;
	is_writable_and_local = local;
	is_writable_and_local_with_wal = is_writable_and_local && ((flags & DB_NO_WAL) != DB_NO_WAL);

#ifdef XAPIAND_DATABASE_WAL
	// If reopen_revision is not available WAL work as a log for the operations
	if (is_writable_and_local_with_wal) {

		// Create a new ConcurrentQueue producer token for this database
		producer_token = DatabaseWALWriter::new_producer_token(endpoint.path);

		// WAL required on a local writable database, open it.
		DatabaseWAL wal(this);
		if (wal.execute(true)) {
			if (auto queue = weak_queue.lock()) {
				modified = true;
			}
		}
	}
#endif  // XAPIAND_DATABASE_WAL
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
	modified = false;
	dbs.clear();
#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif  // XAPIAND_DATA_STORAGE

	auto endpoints_size = endpoints.size();
	if (endpoints_size == 0) {
		THROW(Error, "Writable database must have at least one endpoint");
	}

	db = std::make_unique<Xapian::Database>();

	size_t failures = 0;

	for (const auto& endpoint : endpoints) {
		if (endpoint.empty()) {
			THROW(Error, "Database must not have empty endpoints");
		}

		Xapian::Database rsdb;
		bool local = false;
#ifdef XAPIAND_CLUSTERING
		int _flags = ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN)
			? Xapian::DB_CREATE_OR_OPEN
			: Xapian::DB_OPEN;
		if (!endpoint.is_local()) {
			int port = (endpoint.port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : endpoint.port;
			rsdb = Xapian::Remote::open(endpoint.host, port, 10000, 10000, _flags, endpoint.path);
#ifdef XAPIAN_LOCAL_DB_FALLBACK
			try {
				Xapian::Database tmp = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
				if (tmp.get_uuid() == rsdb.get_uuid()) {
					L_DATABASE("Endpoint %s fallback to local database!", repr(endpoint.to_string()));
					// Handle remote endpoints and figure out if the endpoint is a local database
					rsdb = Xapian::Database(endpoint.path, _flags);
					local = true;
				} else {
					try {
						// If remote is master (it should be), try triggering replication
						XapiandManager::manager->trigger_replication(endpoint, Endpoint{endpoint.path});
						incomplete = true;
					} catch (...) { }
				}
			} catch (const Xapian::DatabaseOpeningError& exc) {
				if (!exists(endpoint.path + "/iamglass")) {
					try {
						// If remote is master (it should be), try triggering replication
						XapiandManager::manager->trigger_replication(endpoint, Endpoint{endpoint.path});
						incomplete = true;
					} catch (...) { }
				}
			}
#endif  // XAPIAN_LOCAL_DB_FALLBACK
		}
		else
#endif  // XAPIAND_CLUSTERING
		{
			try {
				rsdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
				local = true;
			} catch (const Xapian::DatabaseOpeningError& exc) {
				if (!exists(endpoint.path + "/iamglass")) {
					++failures;
					if ((flags & DB_CREATE_OR_OPEN) != DB_CREATE_OR_OPEN)  {
						if (endpoints.size() == failures) {
							db.reset();
							THROW(DatabaseNotFoundError, "Database not found: %s", repr(endpoint.to_string()));
						}
						incomplete = true;
					} else {
						{
							build_path_index(endpoint.path);
							Xapian::WritableDatabase(endpoint.path, Xapian::DB_CREATE_OR_OVERWRITE);
						}
						rsdb = Xapian::Database(endpoint.path, Xapian::DB_OPEN);
						local = true;
					}
				}
				throw;
			}
		}

		db->add_database(rsdb);
		dbs.emplace_back(rsdb, local);

#ifdef XAPIAND_DATA_STORAGE
		if (local) {
			// WAL required on a local database, open it.
			storages.push_back(std::make_unique<DataStorage>(endpoint.path, this, STORAGE_OPEN));
		} else {
			storages.push_back(std::unique_ptr<DataStorage>(nullptr));
		}
#endif  // XAPIAND_DATA_STORAGE
	}
	assert(dbs.size() == endpoints_size);

	is_writable = false;
	is_writable_and_local = false;
	is_writable_and_local_with_wal = false;
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
			} catch (const Xapian::DatabaseError& exc) {
				if (exc.get_msg() != "Database has been closed") {
					throw;
				}
			}
		}

		do_close(true, closed, transaction);
	}

	if ((flags & DB_WRITABLE) == DB_WRITABLE) {
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


void
Database::do_close(bool commit_, bool closed_, Transaction transaction_)
{
	L_CALL("Database::do_close(...)");

	if (
		commit_ &&
		db &&
		modified &&
		transaction == Database::Transaction::none &&
		!closed &&
		is_writable_and_local

	) {
		// Commit only local writable databases
		try {
			commit();
		} catch (...) {
		}
	}

	if (db) {
		try {
			db->close();
		} catch (...) {
		}
		db.reset();
	}

	closed = closed_;
	transaction = transaction_;
	incomplete = false;
	modified = false;
	dbs.clear();
#ifdef XAPIAND_DATA_STORAGE
	storages.clear();
	writable_storages.clear();
#endif  // XAPIAND_DATA_STORAGE
}


void
Database::close()
{
	L_CALL("Database::close()");

	if (closed) {
		return;
	}

	do_close(true, true, Transaction::none);
}


void
Database::autocommit(const std::shared_ptr<Database>& database)
{
	if (
		database->db &&
		database->modified &&
		database->transaction == Database::Transaction::none &&
		!database->closed &&
		database->is_writable_and_local
	) {
		// Auto commit only local writable databases
		DatabaseAutocommit::commit(database);
	}
}


bool
Database::commit(bool wal_, bool send_update)
{
	L_CALL("Database::commit(%s)", wal_ ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	if (!modified) {
		L_DATABASE_WRAP("Do not commit, because there are not changes");
		return false;
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Commit: t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
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
			modified = false;
			if (is_writable_and_local) {
				if (auto queue = weak_queue.lock()) {
					queue->local_revision = wdb->get_revision();
				}
			}
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(false, true, Transaction::none); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { do_close(false, true, Transaction::none); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(false, true, Transaction::none); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Commit made (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_commit(*this, send_update); }
#else
	ignore_unused(wal_);
#endif

	return true;
}


void
Database::begin_transaction(bool flushed)
{
	L_CALL("Database::begin_transaction(%s)", flushed ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	if (transaction == Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		wdb->begin_transaction(flushed);
		transaction = flushed ? Transaction::flushed : Transaction::unflushed;
	}
}


void
Database::commit_transaction()
{
	L_CALL("Database::commit_transaction()");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	if (transaction != Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		wdb->commit_transaction();
		transaction = Transaction::none;
	}
}


void
Database::cancel_transaction()
{
	L_CALL("Database::cancel_transaction()");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	if (transaction != Transaction::none) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		wdb->cancel_transaction();
		transaction = Transaction::none;
	}
}


void
Database::delete_document(Xapian::docid did, bool commit_, bool wal_)
{
	L_CALL("Database::delete_document(%d, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Deleting document: %d  t: %d", did, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(did);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		}
		reopen();
	}

	L_DATABASE_WRAP("Document deleted (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_delete_document(*this, did); }
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

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Deleting document: '%s'  t: %d", term, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(term);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		}
		reopen();
	}

	L_DATABASE_WRAP("Document deleted (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_delete_document_term(*this, term); }
#else
	ignore_unused(wal_);
#endif

	if (commit_) {
		commit(wal_);
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Database::storage_get_stored(Xapian::docid did, const Data::Locator& locator) const
{
	L_CALL("Database::storage_get_stored()");

	assert(locator.type == Data::Type::stored);
	assert(locator.volume != -1);

	assert(did > 0);
	assert(endpoints.size() > 0);
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
Database::storage_pull_blobs(Xapian::Document& doc, Xapian::docid did) const
{
	L_CALL("Database::storage_pull_blobs()");

	assert(did > 0);
	assert(endpoints.size() > 0);
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
Database::storage_push_blobs(Xapian::Document& doc, Xapian::docid) const
{
	L_CALL("Database::storage_push_blobs()");

	assert(is_writable);

	// Writable databases have only one subdatabase,
	// simply get the single storage:
	const auto& storage = writable_storages[0];
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
#endif  // XAPIAND_DATA_STORAGE


Xapian::docid
Database::add_document(Xapian::Document&& doc, bool commit_, bool wal_)
{
	L_CALL("Database::add_document(<doc>, %s, %s)", commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc, doc.get_docid()); // Only writable database get_docid is enough
#endif  // XAPIAND_DATA_STORAGE

	L_DATABASE_WRAP_INIT();

	Xapian::docid did = 0;
	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Adding new document.  t: %d", t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->add_document(doc);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Document added (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_add_document(*this, std::move(doc)); }
#else
	ignore_unused(wal_);
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit_, bool wal_)
{
	L_CALL("Database::replace_document(%d, <doc>, %s, %s)", did, commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc, did);
#endif  // XAPIAND_DATA_STORAGE

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Replacing: %d  t: %d", did, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->replace_document(did, doc);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Document replaced (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_replace_document(*this, did, std::move(doc)); }
#else
	ignore_unused(wal_);
#endif  // XAPIAND_DATABASE_WAL

	if (commit_) {
		commit(wal_);
	}

	return did;
}


Xapian::docid
Database::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_)
{
	L_CALL("Database::replace_document_term(%s, <doc>, %s, %s)", repr(term), commit_ ? "true" : "false", wal_ ? "true" : "false");

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

#ifdef XAPIAND_DATA_STORAGE
	storage_push_blobs(doc, doc.get_docid()); // Only writable database get_docid is enough
#endif  // XAPIAND_DATA_STORAGE

	L_DATABASE_WRAP_INIT();

	Xapian::docid did = 0;
	for (int t = DB_RETRIES; t; --t) {
		// L_DATABASE_WRAP("Replacing: '%s'  t: %d", term, t);
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->replace_document(term, doc);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Document replaced (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_replace_document_term(*this, term, std::move(doc)); }
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

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->add_spelling(word, freqinc);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Spelling added (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_add_spelling(*this, word, freqinc); }
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

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->remove_spelling(word, freqdec);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Spelling removed (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_remove_spelling(*this, word, freqdec); }
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

	for (int t = DB_RETRIES; t; --t) {
		try {
			Xapian::PostingIterator it = db->postlist_begin(term_id);
			if (it == db->postlist_end(term_id)) {
				THROW(DocNotFoundError, "Document not found");
			}
			did = *it;
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
		}
		reopen();
	}

	L_DATABASE_WRAP("Document found (took %s) [1]", string::from_delta(start, std::chrono::system_clock::now()));

	return did;
}


Xapian::Document
Database::get_document(Xapian::docid did, bool assume_valid_, bool pull_)
{
	L_CALL("Database::get_document(%d)", did);

	Xapian::Document doc;

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
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
#endif  // XAPIAND_DATA_STORAGE
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			THROW(DocNotFoundError, "Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			THROW(DocNotFoundError, "Document not found");
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

	for (int t = DB_RETRIES; t; --t) {
		try {
			value = db->get_metadata(key);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
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

	for (int t = DB_RETRIES; t; --t) {
		try {
			auto it = db->metadata_keys_begin();
			auto it_e = db->metadata_keys_end();
			for (; it != it_e; ++it) {
				values.push_back(*it);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
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

	if (!is_writable) {
		THROW(Error, "database is read-only");
	}

	L_DATABASE_WRAP_INIT();

	for (int t = DB_RETRIES; t; --t) {
		auto *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->set_metadata(key, value);
			modified = true;
			break;
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		}
		reopen();
	}

	L_DATABASE_WRAP("Set metadata (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

#if XAPIAND_DATABASE_WAL
	if (wal_ && is_writable_and_local_with_wal) { DatabaseWALWriter::write_set_metadata(*this, key, value); }
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
	for (int t = DB_RETRIES; t; --t) {
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
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const Xapian::InvalidArgumentError&) {
			break;
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
	for (int t = DB_RETRIES; t; --t) {
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
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
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
	for (int t = DB_RETRIES; t; --t) {
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
			if (t == 0) { close(); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseOpeningError& exc) {
			if (t == 0) { close(); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { close(); throw; }
				do_close(false, closed, transaction);
			} else {
				throw;
			}
		} catch (const SerialisationError& exc) {
			THROW(ClientError, exc.what());
		}
		reopen();
		initial = did;
	}

	L_DATABASE_WRAP("Dump documents (took %s)", string::from_delta(start, std::chrono::system_clock::now()));

	return docs;
}
