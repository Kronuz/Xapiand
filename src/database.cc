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

#include "cassert.h"              // for ASSERT
#include "database_data.h"        // for Locator
#include "database_flags.h"       // for readable_flags
#include "database_shard.h"       // for Shard
#include "database_utils.h"       // for DB_SLOT_SHARDS
#include "exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "hashes.hh"              // for fnv1ah64::hash
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_CALL
#include "logger.h"               // for Logging
#include "lz4/xxhash.h"           // for XXH32_update, XXH32_state_t
#include "manager.h"              // for XapiandManager, sig_exit, trigger_replication
#include "msgpack.h"              // for MsgPack
#include "random.hh"              // for random_int
#include "repr.hh"                // for repr
#include "string.hh"              // for string::format

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
// #undef L_DATABASE_WRAP_BEGIN
// #define L_DATABASE_WRAP_BEGIN L_DELAYED_100
// #undef L_DATABASE_WRAP_END
// #define L_DATABASE_WRAP_END L_DELAYED_N_UNLOG


//  ____        _        _
// |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
// | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
// | |_| | (_| | || (_| | |_) | (_| \__ \  __/
// |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|
//

Database::Database(std::vector<std::shared_ptr<Shard>>&& shards_, const Endpoints& endpoints_, int flags_)
	: flags(flags_),
	  closed(false),
	  _shards(std::move(shards_)),
	  endpoints(endpoints_)
{
	reopen();
}


Database::~Database() noexcept
{
	try {
		if (log) {
			log->clear();
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


bool
Database::reopen()
{
	L_CALL("Database::reopen() {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	L_DATABASE_WRAP_BEGIN("Database::reopen:BEGIN {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));
	L_DATABASE_WRAP_END("Database::reopen:END {{endpoint:{}, flags:({})}}", repr(to_string()), readable_flags(flags));

	ASSERT(!_shards.empty());
	_database = std::make_unique<Xapian::Database>();
	for (auto& shard : _shards) {
		_database->add_database(*shard->db());
	}
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
}


void
Database::do_close(bool commit_, bool closed_, bool throw_exceptions)
{
	L_CALL("Database::do_close({}, {}, {}) {{endpoint:{}, database:{}, closed:{}}}", commit_, closed_, repr(to_string()), _database ? "<database>" : "null", is_closed(), throw_exceptions);
	ignore_unused(commit_);

	if (_database) {
		try {
			_database.reset();
		} catch (...) {
			if (throw_exceptions) {
				throw;
			}
			L_WARNING("WARNING: Internal database close failed!");
		}
	}

	reset();

	closed.store(closed_, std::memory_order_relaxed);
}


void
Database::close()
{
	L_CALL("Database::close()");

	if (is_closed()) {
		return;
	}

	do_close(true, true);
}


bool
Database::commit(bool wal_, bool send_update)
{
	L_CALL("Database::commit({})", wal_);

	ASSERT(!_shards.empty());
	bool ret = true;
	for (auto& shard : _shards) {
		ret = shard->commit(wal_, send_update) || ret;
	}
	return ret;
}


void
Database::begin_transaction(bool flushed)
{
	L_CALL("Database::begin_transaction({})", flushed);

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		shard->begin_transaction(flushed);
	}
}


void
Database::commit_transaction()
{
	L_CALL("Database::commit_transaction()");

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		shard->commit_transaction();
	}
}


void
Database::cancel_transaction()
{
	L_CALL("Database::cancel_transaction()");

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		shard->cancel_transaction();
	}
}


void
Database::delete_document(Xapian::docid did, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::delete_document({}, {}, {})", did, commit_, wal_);

	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = (did - 1) % n_shards;
	Xapian::docid shard_did = (did - 1) / n_shards + 1;
	auto& shard = _shards[shard_num];
	shard->delete_document(shard_did, commit_, wal_, version_);
}


void
Database::delete_document_term(const std::string& term, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = fnv1ah64::hash(term) % n_shards;
	auto& shard = _shards[shard_num];
	shard->delete_document_term(term, commit_, wal_, version_);
}


#ifdef XAPIAND_DATA_STORAGE
std::string
Database::storage_get_stored(const Locator& locator, Xapian::docid did)
{
	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = (did - 1) % n_shards;
	auto& shard = _shards[shard_num];
	return shard->storage_get_stored(locator);
}
#endif /* XAPIAND_DATA_STORAGE */


Xapian::docid
Database::add_document(Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::add_document(<doc>, {}, {})", commit_, wal_);

	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = random_int(0, n_shards);
	auto& shard = _shards[shard_num];
	doc.add_value(DB_SLOT_SHARDS, serialise_length(shard_num) + serialise_length(n_shards));
	auto did = shard->add_document(std::move(doc), commit_, wal_, version_);
	return (did - 1) * n_shards + shard_num + 1;
}


Xapian::docid
Database::replace_document(Xapian::docid did, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::replace_document({}, <doc>, {}, {})", did, commit_, wal_);

	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = (did - 1) % n_shards;
	Xapian::docid shard_did = (did - 1) / n_shards + 1;
	auto& shard = _shards[shard_num];
	doc.add_value(DB_SLOT_SHARDS, serialise_length(shard_num) + serialise_length(n_shards));
	shard->replace_document(shard_did, std::move(doc), commit_, wal_, version_);
	return did;
}


Xapian::docid
Database::replace_document_term(const std::string& term, Xapian::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Database::replace_document_term({}, <doc>, {}, {})", repr(term), commit_, wal_);

	ASSERT(!_shards.empty());
	size_t n_shards = _shards.size();
	size_t shard_num = fnv1ah64::hash(term) % n_shards;
	auto& shard = _shards[shard_num];
	doc.add_value(DB_SLOT_SHARDS, serialise_length(shard_num) + serialise_length(n_shards));
	auto did = shard->replace_document_term(term, std::move(doc), commit_, wal_, version_);
	return (did - 1) * n_shards + shard_num + 1;
}


void
Database::add_spelling(const std::string& word, Xapian::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL("Database::add_spelling(<word, <freqinc>, {}, {})", commit_, wal_);

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		shard->add_spelling(word, freqinc, commit_, wal_);
	}
}


Xapian::termcount
Database::remove_spelling(const std::string& word, Xapian::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Database::remove_spelling(<word>, <freqdec>, {}, {})", commit_, wal_);

	Xapian::termcount result = 0;

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		result = shard->remove_spelling(word, freqdec, commit_, wal_);
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
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, false); throw; }
				do_close(false, is_closed(), false);
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
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, false); throw; }
				do_close(false, is_closed(), false);
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

	ASSERT(!_shards.empty());
	return _shards[0]->get_metadata(key);
}


std::vector<std::string>
Database::get_metadata_keys()
{
	L_CALL("Database::get_metadata_keys()");
	ASSERT(!_shards.empty());
	return _shards[0]->get_metadata_keys();
}


void
Database::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL("Database::set_metadata({}, {}, {}, {})", repr(key), repr(value), commit_, wal_);

	ASSERT(!_shards.empty());
	for (auto& shard : _shards) {
		shard->set_metadata(key, value, commit_, wal_);
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
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, false); throw; }
				do_close(false, is_closed(), false);
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
							auto stored = storage_get_stored(locator, did);
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
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, false); throw; }
				do_close(false, is_closed(), false);
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
							auto stored = storage_get_stored(locator, did);
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
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::NetworkError& exc) {
			if (t == 0) { do_close(true, true, false); throw; }
		} catch (const Xapian::DatabaseError& exc) {
			if (exc.get_msg() == "Database has been closed") {
				if (t == 0) { do_close(true, true, false); throw; }
				do_close(false, is_closed(), false);
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
	return endpoints.to_string();
}


std::string
Database::__repr__() const
{
	return string::format("<Database {} ({}){}>",
		repr(to_string()),
		readable_flags(flags),
		is_closed() ? " (closed)" : "");
}
