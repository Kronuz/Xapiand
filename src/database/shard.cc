/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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
#include <unistd.h>                // for unlink
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "at_end.h"               // for at_end::assert_none
#include "database/data.h"        // for Locator
#include "database/flags.h"       // for readable_flags, DB_*
#include "database/pool.h"        // for ShardEndpoint
#include "database/handler.h"     // for committer
#include "database/utils.h"       // for DB_SLOT_VERSION
#include "database/wal.h"         // for DatabaseWAL
#include "engine/chores.h"          // for engine::Chores
#include "engine/open.h"            // for engine::OpenOptions
#include "engine/strata/shard.h"    // for engine::strata::borrow
#include "engine/sync.h"            // for engine::sync_wait
#include "xapiand_exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "index_resolver_lru.h"   // for IndexSettings
#include "length.h"               // for serialise_string
#include "log.h"                  // for L_CALL
#include "manager.h"              // for XapiandManager
#include "msgpack.h"              // for MsgPack
#include "random.hh"              // for random_int
#include "repr.hh"                // for repr
#include "reserved/fields.h"      // for ID_FIELD_NAME
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
// and its new version in the stored data, compressing the record once, and clears the shards slot.
static void
finalise_write(engine::Document& doc, engine::WriteResult& info, const std::optional<NumericId>& numeric)
{
	bool data_modified = false;
	Data data(doc.get_data());
	auto data_obj = data.get_obj();
	if (numeric) {
		auto did = static_cast<double>((info.did - 1) * numeric->n_shards + numeric->shard_num + 1);  // unshard number and shard docid to docid in multi-db
		auto did_serialised = sortable_serialise(did);
		info.term = document_key(did_serialised, 'N');
		doc.set_key(info.term);
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
	const auto* main_locator = data.get("");
	if (data_modified || (main_locator != nullptr && main_locator->type == Locator::Type::inplace)) {   // prepare leaves it raw
		data.set_obj(data_obj);
		data.flush();
		doc.set_data(data.serialise());
	}
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
	auto shard = static_cast<ShardEndpoint*>(param);
	assert(shard);

	head.magic = _LEGACY_STORAGE_MAGIC;
	strncpy(head.uuid, shard->uuid().c_str(), sizeof(head.uuid));
	head.offset = STORAGE_START_BLOCK_OFFSET;
}


void
DataHeaderV1::validate(void* param, void* /*unused*/)
{
	if (head.magic != _LEGACY_STORAGE_MAGIC) {
		THROW(StorageCorruptVolume, "Bad data storage header magic number");
	}

	auto shard = static_cast<ShardEndpoint*>(param);
	if (UUID(head.uuid) != UUID(shard->uuid())) {
		THROW(StorageCorruptVolume, "Data storage UUID mismatch");
	}
}


void
DataHeaderV2::init(void* param, void* /*unused*/)
{
	auto shard = static_cast<ShardEndpoint*>(param);
	assert(shard);

	// The engine stamps meta (magic/txnid/high-water/checksum); we only carry the
	// shard uuid so a foreign volume is rejected on open.
	memset(uuid, 0, sizeof(uuid));
	strncpy(uuid, shard->uuid().c_str(), sizeof(uuid));
}


void
DataHeaderV2::validate(void* param, void* /*unused*/)
{
	// The engine has already checked the meta magic + whole-block checksum; this
	// is the shard-domain check.
	auto shard = static_cast<ShardEndpoint*>(param);
	if (UUID(uuid) != UUID(shard->uuid())) {
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
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 * the parts that touch the engine, the blob storage and the WAL
 */

ShardEndpoint::ShardEndpoint(DatabasePool& database_pool_, std::string_view path_) :
	Endpoint(path_),
	database_pool(database_pool_),
	revision(0),
	chores(std::make_unique<IndexChores>(*database_pool_.work_pool_)),
	modified(false),
	applied_(0),
	last_flags_(0)
{
}


std::string
ShardEndpoint::outstanding()
{
	if (!turn.try_lock()) {
		return "the turn is held or awaited";
	}
	turn.unlock();
	return {};
}


ShardEndpoint::~ShardEndpoint()
{
	at_end::assert_none("ShardEndpoint", outstanding());
}


DatabaseWALWriter&
ShardEndpoint::wal_writer() const
{
	if (auto* writer = database_pool.wal_writer_.load(std::memory_order_acquire)) {
		return *writer;
	}
	return *XapiandManager::manager(true)->wal_writer;
}


void
ShardEndpoint::note_failure(const engine::Error& error) noexcept
{
	switch (error.kind) {
		case engine::errc::closed:
		case engine::errc::reopen:
		case engine::errc::corrupt:
		case engine::errc::database:
			stale_.store(true, std::memory_order_release);
			break;
		default:
			break;
	}
}


// What the log replays into while the index is opened: the engine alone, with chores inline.
class ReplayTarget final : public WalTarget {
	ShardEndpoint& entry_;
	engine::Shard& engine_;
	int flags_;
	engine::rev start_revision_;

public:
	ReplayTarget(ShardEndpoint& entry, engine::Shard& eng, int flags) :
		entry_(entry), engine_(eng), flags_(flags), start_revision_(eng.revision()) { }

	const std::string& path() const override {
		return entry_.path;
	}

	engine::Shard& engine() override {
		return engine_;
	}

	engine::rev start_revision() const override {
		return start_revision_;
	}

	bool commit(bool wal) override {
		return engine::sync_wait(entry_.do_commit(engine_, engine::inline_chores(), flags_, wal));
	}

	void replace_document(engine::docid did, engine::Document&& doc, bool commit_, bool wal, bool version) override {
		engine::sync_wait(entry_.do_replace(engine_, engine::inline_chores(), flags_, did, std::move(doc), wal, version));
		if (commit_) {
			commit(wal);
		}
	}

	void delete_document(engine::docid did, bool commit_, bool wal, bool) override {
		engine::sync_wait(entry_.do_delete(engine_, flags_, did, wal));
		if (commit_) {
			commit(wal);
		}
	}

	void set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal) override {
		engine::sync_wait(entry_.do_set_metadata(engine_, flags_, key, value, wal));
		if (commit_) {
			commit(wal);
		}
	}

	void add_spelling(const std::string& word, engine::termcount freq, bool commit_, bool wal) override {
		engine::sync_wait(entry_.do_add_spelling(engine_, flags_, word, freq, wal));
		if (commit_) {
			commit(wal);
		}
	}

	void remove_spelling(const std::string& word, engine::termcount freq, bool commit_, bool wal) override {
		engine::sync_wait(entry_.do_remove_spelling(engine_, flags_, word, freq, wal));
		if (commit_) {
			commit(wal);
		}
	}
};


// The open's work (open_payload, x-pool D6), in the catalog's open chore with the slot `opening`: nobody else is on
// the index, so the replay holds no turn and its writes run their chores inline. The engine then takes the index's
// chores. A throw fails the open, and the catalog destroys this entry, then the index.
void
ShardEndpoint::open_blocking(prospect::index& index)
{
	L_CALL("ShardEndpoint::open_blocking()");

	L_DATABASE("Opening shard {}", repr(to_string()));

	engine::OpenOptions options;
	options.chores = &engine::inline_chores();
	options.host = true;
	options.background = &database_pool.executor();
	index_ = &index;
	readers_ = index.reader();
	engine_ = engine::strata::borrow(index, readers_, path, options);

	{
		std::lock_guard<std::mutex> lk(mtx);
		uuid_ = engine_->uuid();
	}
	set_revision(engine_->revision());
	modified.store(false, std::memory_order_relaxed);

#ifdef XAPIAND_DATA_STORAGE
	writable_storage = std::make_unique<DataStorage>(path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_COMPRESS_MODE | STORAGE_SYNC_MODE);
	{
		// The shared writer continues in the newest volume on disk; uploads and rollovers take the numbers above it.
		unsigned long long newest = 0;
		try {
			newest = writable_storage->get_volumes_range(DATA_STORAGE_PATH).second;
		} catch (const StorageNotFound&) { }
		writable_storage->volume = static_cast<uint32_t>(newest);
		next_volume_.store(newest + 1, std::memory_order_relaxed);
	}
#endif  // XAPIAND_DATA_STORAGE

#if XAPIAND_DATABASE_WAL
	// A log that is ahead of the committed revision is replayed by whoever opens the index: the open holds it.
	{
		ReplayTarget target(*this, *engine_, DB_WRITABLE | DB_OPEN);
		DatabaseWAL wal(&target);
		if (wal.execute()) {
			modified.store(true, std::memory_order_relaxed);
			// Commits the replay like any commit (readers see it, the WAL gets its COMMIT line); a failure propagates.
			target.commit(true);
		}
	}
#endif  // XAPIAND_DATABASE_WAL

	engine_->use_chores(chores.get());
}


ShardEndpoint::InlineEngine::InlineEngine(ShardEndpoint& entry, const char* why) :
	entry_(entry),
	blocking_(why)
{
	entry_.engine_->use_chores(&engine::inline_chores());
	engines_inline_.fetch_add(1, std::memory_order_relaxed);
}


ShardEndpoint::InlineEngine::~InlineEngine()
{
	entry_.engine_->use_chores(entry_.chores.get());
	engines_inline_.fetch_sub(1, std::memory_order_relaxed);
}


// The catalog's commit chore (x-pool D7): no pin is held, so the turn is free, and the engine's chores run inline,
// since this runs in a chore of the pool its group is on.
void
ShardEndpoint::commit_blocking()
{
	L_CALL("ShardEndpoint::commit_blocking()");

	if (!modified.load()) {
		// Nothing applied since the last commit: no revision and no WAL line (C25). A finished merge's output is all
		// the index has to sync, which its commit does without a new revision.
		if (index_->merge_output_unsynced()) {
			index_->commit();
		}
		return;
	}
	const bool locked = turn.try_lock();
	assert(locked && "the catalog's commit chore runs with no pin, so with no turn held");
	struct Unlock {
		ShardEndpoint& entry;
		bool locked;
		~Unlock() {
			if (locked) {
				entry.turn.unlock();
			}
		}
	} unlock{*this, locked};
	InlineEngine inline_engine(*this, "the catalog's commit chore runs on a worker");
	const int saved = last_flags_.load();
	engine::sync_wait(do_commit(*engine_, engine::inline_chores(), saved ? saved : (DB_WRITABLE | DB_OPEN), true));
	committed_ = applied_.load();
}


/*
 * Commit: one at a time, coalesced by position.
 */

struct ShardEndpoint::CommitFlight {
	std::uint64_t covers = 0;  // the writes applied when the commit started
	task::event done;
	std::exception_ptr error;
	bool changed = false;
};


task::task<bool>
ShardEndpoint::commit(int flags, bool wal_, task::cancel_token cancel)
{
	L_CALL("ShardEndpoint::commit(({}), {})", readable_flags(flags), wal_);

	if (!modified.load()) {
		L_DATABASE("Commit on shard {} was discarded, because there are not changes", repr(to_string()));
		co_return false;
	}

	// The caller's writes were all applied before this call: a commit that began after they were, and has
	// not finished, covers them. One that began earlier may not, so the caller waits for its own turn.
	const auto mine = applied_.load();
	std::shared_ptr<CommitFlight> joined;
	{
		std::lock_guard<std::mutex> lk(this->mtx);
		if (commit_flight_ && commit_flight_->covers >= mine) {
			joined = commit_flight_;
		}
	}
	if (joined) {
		co_await joined->done;
		if (joined->error) {
			std::rethrow_exception(joined->error);
		}
		co_return joined->changed;
	}

	auto turn_ = co_await take_turn(flags, std::move(cancel));
	if (committed_ >= mine || !modified.load()) {
		co_return false;  // a commit that ran while this one waited covers it
	}
	auto flight = std::make_shared<CommitFlight>();
	flight->covers = applied_.load();
	{
		std::lock_guard<std::mutex> lk(this->mtx);
		commit_flight_ = flight;
	}
	std::exception_ptr error;
	bool changed = false;
	try {
		changed = co_await do_commit(*engine_, *chores, flags, wal_);
		committed_ = flight->covers;
	} catch (const engine::Error& exc) {
		note_failure(exc);
		error = std::current_exception();
	} catch (...) {
		error = std::current_exception();
	}
	{
		std::lock_guard<std::mutex> lk(this->mtx);
		commit_flight_.reset();
	}
	flight->error = error;
	flight->changed = changed;
	flight->done.set();
	if (error) {
		std::rethrow_exception(error);
	}
	co_return changed;
}


task::task<bool>
ShardEndpoint::settle_merges(int flags)
{
	L_CALL("ShardEndpoint::settle_merges(({}))", readable_flags(flags));

	auto turn_ = co_await take_turn(flags);
	bool pending = false;
	co_await chores->run(engine::ChoreClass::flush, [this, &pending] {
		index_->check_in();   // publishes the merges that finished
		if (index_->merge_output_unsynced()) {
			index_->commit();   // nothing else is uncommitted: no revision, so no WAL line (C25)
		}
		pending = index_->merges_pending() != 0 || index_->merge_output_unsynced();
	});
	co_return pending;
}


task::task<bool>
ShardEndpoint::do_commit(engine::Shard& eng, engine::Chores& chores_, int flags, [[maybe_unused]] bool wal_)
{
	L_CALL("ShardEndpoint::do_commit(({}), {})", readable_flags(flags), wal_);

	if (!modified.load()) {
		co_return false;
	}

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

#ifdef XAPIAND_DATA_STORAGE
	co_await storage_commit(chores_);
#endif  // XAPIAND_DATA_STORAGE
	const auto prior_revision = eng.revision();
	co_await eng.commit(engine::Transaction::none);
	modified.store(false, std::memory_order_relaxed);
	const auto current_revision = eng.revision();
	if (prior_revision == current_revision) {
		L_DATABASE("Commit on shard {} was discarded, because it turned out not to change the revision", repr(to_string()));
		co_return false;
	}
	assert(current_revision == prior_revision + 1);
	L_DATABASE("Commit on shard {}: {} -> {}", repr(to_string()), prior_revision, current_revision);
	set_revision(current_revision);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_commit(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			prior_revision);
	}
#endif
	co_return true;
}


/*
 * The writes. Each runs under the index's turn: the engine's apply, the counters and the WAL line are one
 * unit, so the WAL's order is the apply's order.
 */

task::task<engine::WriteResult>
ShardEndpoint::do_add(engine::Shard& eng, engine::Chores& chores_, int flags, engine::Document&& doc, [[maybe_unused]] bool wal_, bool version_)
{
	L_CALL("ShardEndpoint::do_add(<doc>, {})", wal_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = co_await storage_push_blobs(chores_, doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#else
	(void)chores_;
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteArgs args;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, {}); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	engine::WriteResult info;
	try {
		info = co_await eng.add(doc, args);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		wal_writer().write_replace_document(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			info.did,
			std::move(doc));
	}
#else
	(void)flags;
#endif
	co_return info;
}


task::task<engine::WriteResult>
ShardEndpoint::do_replace(engine::Shard& eng, engine::Chores& chores_, int flags, engine::docid shard_did, engine::Document&& doc, [[maybe_unused]] bool wal_, bool version_, bool must_exist)
{
	L_CALL("ShardEndpoint::do_replace({}, <doc>, {})", shard_did, wal_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = co_await storage_push_blobs(chores_, doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#else
	(void)chores_;
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteResult info;
	info.did = shard_did;

	engine::WriteArgs args;
	args.must_exist = must_exist;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, std::nullopt); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	try {
		info = co_await eng.replace(shard_did, doc, args);
	} catch (const engine::Error& error) {
		note_failure(error);
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // a retry elsewhere pushes the blobs again
		}
#endif  // XAPIAND_DATA_STORAGE
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		wal_writer().write_replace_document(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			info.did,
			std::move(doc));
	}
#else
	(void)flags;
#endif
	co_return info;
}


task::task<engine::WriteResult>
ShardEndpoint::do_replace_term(engine::Shard& eng, engine::Chores& chores_, int flags, const std::string& term, engine::Document&& doc, [[maybe_unused]] bool wal_, bool version_)
{
	L_CALL("ShardEndpoint::do_replace_term({}, <doc>, {}, {})", repr(term), wal_, version_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

#ifdef XAPIAND_DATA_STORAGE
	auto pushed = co_await storage_push_blobs(chores_, doc.get_data());
	if (!pushed.first.empty()) {
		doc.set_data(pushed.first);
	}
#else
	(void)chores_;
#endif  // XAPIAND_DATA_STORAGE

	engine::WriteArgs args;
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	auto n_shards_ser = doc.get_value(DB_SLOT_SHARDS);

	engine::WriteResult info;
	info.term = term;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, std::nullopt); };
	try {
		const char *p = n_shards_ser.data();
		const char *p_end = p + n_shards_ser.size();
		size_t shard_num = p == p_end ? 0 : unserialise_length(&p, p_end);
		size_t n_shards = p == p_end ? 1 : unserialise_length(&p, p_end);
		if (term == NEW_NUMERIC_KEY) {
			// A new numeric id: the shard assigns the document id, and the multi-db id derives from it.
			args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result, NumericId{shard_num, n_shards}); };
			info = co_await eng.add(doc, args);
		} else {
			// Found by its key; a new numeric id takes the document id it names when that is free, so the ids the
			// shard numbers (above its last document id) are never taken.
			if (const auto did = numeric_docid(term); did != 0 && (did - 1) % n_shards == shard_num) {
				args.new_did = (did - 1) / n_shards + 1;  // docid in the multi-db to the docid in the shard
			}
			info = co_await eng.replace_by_key(term, doc, args);
		}
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
#ifdef XAPIAND_DATA_STORAGE
		if (!pushed.second.empty()) {
			doc.set_data(pushed.second);  // restore data with blobs
		}
#endif  // XAPIAND_DATA_STORAGE
		wal_writer().write_replace_document(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			info.did,
			std::move(doc));
	}
#else
	(void)flags;
#endif
	co_return info;
}


task::task<void>
ShardEndpoint::do_delete(engine::Shard& eng, [[maybe_unused]] int flags, engine::docid shard_did, [[maybe_unused]] bool wal_, std::optional<engine::rev> expected_)
{
	L_CALL("ShardEndpoint::do_delete({}, {})", shard_did, wal_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		co_await eng.erase(shard_did, true, expected_);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_delete_document(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			shard_did);
	}
#endif
}


task::task<void>
ShardEndpoint::do_delete_term(engine::Shard& eng, [[maybe_unused]] int flags, const std::string& term, [[maybe_unused]] bool wal_, std::optional<engine::rev> expected_)
{
	L_CALL("ShardEndpoint::do_delete_term({}, {})", repr(term), wal_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	[[maybe_unused]] engine::docid shard_did = 0;
	try {
		shard_did = co_await eng.erase_by_key(term, true, expected_);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_delete_document(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			shard_did);
	}
#endif
}


task::task<void>
ShardEndpoint::do_set_metadata(engine::Shard& eng, [[maybe_unused]] int flags, const std::string& key, const std::string& value, [[maybe_unused]] bool wal_)
{
	L_CALL("ShardEndpoint::do_set_metadata({}, {}, {})", repr(key), repr(value), wal_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		co_await eng.set_metadata(key, value);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_set_metadata(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			key,
			value);
	}
#endif
}


task::task<void>
ShardEndpoint::do_add_spelling(engine::Shard& eng, [[maybe_unused]] int flags, const std::string& word, engine::termcount freqinc, [[maybe_unused]] bool wal_)
{
	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		co_await eng.add_spelling(word, freqinc);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_add_spelling(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			word,
			freqinc);
	}
#endif
}


task::task<engine::termcount>
ShardEndpoint::do_remove_spelling(engine::Shard& eng, [[maybe_unused]] int flags, const std::string& word, engine::termcount freqdec, [[maybe_unused]] bool wal_)
{
	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	engine::termcount result = 0;
	try {
		result = co_await eng.remove_spelling(word, freqdec);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
#if XAPIAND_DATABASE_WAL
	if (wal_ && has_db_writable(flags) && !has_db_disable_wal(flags)) {
		wal_writer().write_remove_spelling(
			has_db_synchronous_wal(flags),
			path,
			eng.uuid(),
			eng.revision(),
			word,
			freqdec);
	}
#endif
	co_return result;
}


/*
 * The data storage: blobs written under the turn, read on readers of their own, all in chores.
 */
#ifdef XAPIAND_DATA_STORAGE

std::pair<std::string, std::string>
ShardEndpoint::storage_push_blobs_blocking(std::string&& doc_data)
{
	L_CALL("ShardEndpoint::storage_push_blobs_blocking()");

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
								writable_storage->open(strings::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
							}
							offset = writable_storage->write(serialise_strings({ locator.ct_type.to_string(), locator.raw }));
							break;
						} catch (StorageEOF) {
							writable_storage->volume = static_cast<uint32_t>(next_volume_.fetch_add(1));
							writable_storage->open(strings::format(DATA_STORAGE_PATH "{}", writable_storage->volume));
						} catch (const StorageLegacyReadOnly&) {
							// The latest volume is a legacy v1 volume (read-only).
							// Start a fresh v2 volume above it for new writes; the
							// old v1 volumes stay readable and age out over time.
							writable_storage->volume = static_cast<uint32_t>(next_volume_.fetch_add(1));
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


task::task<std::pair<std::string, std::string>>
ShardEndpoint::storage_push_blobs(engine::Chores& chores_, std::string doc_data)
{
	if (doc_data.empty()) {
		co_return std::pair<std::string, std::string>();
	}
	std::pair<std::string, std::string> pushed;
	co_await chores_.run(engine::ChoreClass::flush, [&] { pushed = storage_push_blobs_blocking(std::move(doc_data)); });
	co_return pushed;
}


task::task<void>
ShardEndpoint::storage_commit(engine::Chores& chores_)
{
	if (writable_storage) {
		co_await chores_.run(engine::ChoreClass::flush, [this] { writable_storage->commit(); });
	}
}


// The state of a streamed upload: a storage of its own on a volume nobody else writes, and the record being streamed
// into it. Every call that touches the device runs in a chore.
struct StoredUpload::Impl {
	ShardEndpoint& entry;
	std::uint64_t volume;
	std::unique_ptr<DataStorage> storage;
	std::optional<DataStorage::StreamWriter> writer;
	std::uint32_t record_offset = 0;
	bool ended = false;
	bool aborted = false;

	Impl(ShardEndpoint& entry_, std::uint64_t volume_) : entry(entry_), volume(volume_) { }

	std::string name() const {
		return strings::format(DATA_STORAGE_PATH "{}", volume);
	}
};


StoredUpload::StoredUpload(std::unique_ptr<Impl> impl_, engine::Chores& chores_) : impl(std::move(impl_)), chores(chores_) { }


StoredUpload::~StoredUpload()
{
	// An upload that neither ended nor was aborted leaves a volume behind: say so, a later reclaim collects it.
	if (impl && !impl->ended && !impl->aborted) {
		L_WARNING("A streamed upload of {} was dropped without ending or aborting", repr(impl->name()));
	}
}


ssize_t
StoredUpload::volume() const noexcept
{
	return static_cast<ssize_t>(impl->volume);
}


size_t
StoredUpload::offset() const noexcept
{
	return impl->record_offset;
}


task::task<void>
StoredUpload::append(std::string chunk)
{
	co_await chores.run(engine::ChoreClass::flush, [this, &chunk] { impl->writer->append(chunk); });
}


task::task<void>
StoredUpload::end()
{
	co_await chores.run(engine::ChoreClass::flush, [this] {
		impl->record_offset = impl->writer->commit();   // the data is synced, then the volume's header publishes the record
		impl->writer.reset();
		impl->storage->close();
	});
	impl->ended = true;
}


task::task<void>
StoredUpload::abort() noexcept
{
	if (impl->ended || impl->aborted) {
		co_return;
	}
	impl->aborted = true;
	try {
		co_await chores.run(engine::ChoreClass::flush, [this] {
			impl->writer.reset();
			if (impl->storage) {
				impl->storage->close();
			}
			const auto file = strings::format("{}/{}", impl->entry.path, impl->name());
			::unlink(file.c_str());
		});
	} catch (...) {
		L_EXC("Unable to remove the volume of an aborted upload");
	}
}


task::task<std::unique_ptr<StoredUpload>>
ShardEndpoint::upload_begin(engine::Chores& chores_, std::string ct_type, std::uint64_t size)
{
	// The record is the content type, then the blob, as a `stored` record is: the framing is written first and
	// the blob's bytes follow.
	static_assert(STORED_UPLOAD_LIMIT + 1024 == DataStorage::stream_max_size, "the upload limit leaves 1 KiB for the framing");
	std::string head = serialise_string(ct_type) + serialise_length(size);
	const std::uint64_t record = head.size() + size;
	if (record > DataStorage::stream_max_size) {
		THROW(PayloadTooLarge, "A stored blob has a size limit of {}", strings::from_bytes(DataStorage::stream_max_size - head.size()));
	}

	auto impl = std::make_unique<StoredUpload::Impl>(*this, next_volume_.fetch_add(1));
	std::exception_ptr error;
	try {
		co_await chores_.run(engine::ChoreClass::flush, [&] {
			if (!writable_storage) {
				THROW(ClientError, "The index has no data storage to upload a blob to");
			}
			impl->storage = std::make_unique<DataStorage>(path, this, STORAGE_OPEN | STORAGE_WRITABLE | STORAGE_CREATE | STORAGE_SYNC_MODE);
			impl->storage->open(impl->name());
			impl->writer.emplace(*impl->storage, record);
			impl->writer->append(head);
		});
	} catch (...) {
		error = std::current_exception();
	}
	std::unique_ptr<StoredUpload> upload(new StoredUpload(std::move(impl), chores_));
	if (error) {
		co_await upload->abort();
		try {
			std::rethrow_exception(error);
		} catch (const StorageTooLarge&) {
			THROW(PayloadTooLarge, "A stored blob has a size limit of {}", strings::from_bytes(DataStorage::stream_max_size));
		} catch (const StorageEOF&) {
			THROW(PayloadTooLarge, "The blob does not fit a volume of the data storage");
		}
	}
	co_return upload;
}


std::string
ShardEndpoint::storage_read_blocking(const Locator& locator)
{
	assert(locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored);
	assert(locator.volume != -1);

	DataStorage reader(path, this, STORAGE_OPEN);
	reader.open(strings::format(DATA_STORAGE_PATH "{}", locator.volume));
	reader.seek(static_cast<uint32_t>(locator.offset));
	return reader.read();
}


std::string
ShardEndpoint::storage_read_blocking(std::string_view key)
{
	const char *p = key.data();
	const char *p_end = p + key.size();
	++p;  // the lead byte that says it is a locator
	ssize_t volume = unserialise_length(&p, p_end);
	size_t offset = unserialise_length(&p, p_end);
	DataStorage reader(path, this, STORAGE_OPEN);
	reader.open(strings::format(DATA_STORAGE_PATH "{}", volume));
	reader.seek(static_cast<uint32_t>(offset));
	return reader.read();
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

Shard::Shard(IndexPin pin, int flags_, task::cancel_token cancel_)
	: pin_(std::move(pin)),
	  pool_(pin_.pool()),
	  path_(pin_->path),
	  flags(flags_),
	  cancel(std::move(cancel_))
{
}


Shard::Shard(DatabasePool& pool, std::string path, int flags_, task::cancel_token cancel_)
	: pool_(&pool),
	  path_(std::move(path)),
	  missing_(true),
	  flags(flags_),
	  cancel(std::move(cancel_))
{
}


ShardEndpoint&
Shard::endpoint() const
{
	if (!pin_) {
		if (missing_) {
			throw engine::Error(engine::errc::not_found, "No such database: " + path_);
		}
		throw engine::Error(engine::errc::closed, "Database has been closed");
	}
	return pin_.entry();
}


std::shared_ptr<const Node>
Shard::node() const
{
	return pin_ ? endpoint().node() : Endpoint(path_).node();
}


void
Shard::release() noexcept
{
#if XAPIAND_DATABASE_WAL
	// A restore writes no log, so the log of what it replaced must not be replayed over it.
	if (is_restore() && is_writable() && pin_) {
		try {
			endpoint().wal_writer().delete_wal(is_synchronous_wal(), endpoint().path);
		} catch (...) {
			L_EXC("Unable to delete the WAL of {}", repr(endpoint().to_string()));
		}
	}
#endif
	// A use ends with no map made outside the index's lease (x-pool D12).
	assert(!pin_ || pin_->index_ == nullptr || pin_->index_->unleased_maps() == 0);
	if (pin_ && pin_->stale_.load(std::memory_order_acquire)) {
		pin_.reopen();   // a write found the index unusable: the catalog replaces it once its holders let go
	}
}


task::task<void>
Shard::ready(bool use)
{
	if (!pin_) {
		pin_ = co_await pool_->pin(Endpoint(path_), flags, cancel, use);
		missing_ = false;
	}
}


engine::Shard&
Shard::engine()
{
	return endpoint().engine();
}


task::task<engine::Shard*>
Shard::engine_for_read()
{
	co_await ready();
	co_await endpoint().fresh(flags, cancel);
	co_return &endpoint().engine();
}


task::task<engine::docid>
Shard::written_key(const std::string& term)
{
	co_await ready();
	auto turn = co_await endpoint().take_turn(flags, cancel);
	co_return co_await endpoint().engine().written_key(term);
}


task::task<engine::doccount>
Shard::written_count()
{
	co_await ready();
	auto turn = co_await endpoint().take_turn(flags, cancel);
	co_return co_await endpoint().engine().written_count();
}


task::task<std::vector<engine::docid>>
Shard::written_keys(const std::vector<std::string>& terms)
{
	co_await ready();
	auto turn = co_await endpoint().take_turn(flags, cancel);
	auto& eng = endpoint().engine();
	std::vector<engine::docid> dids;
	dids.reserve(terms.size());
	for (const auto& term : terms) {
		dids.push_back(co_await eng.written_key(term));
	}
	co_return dids;
}


task::task<void>
Shard::apply_slice(engine::Chores& chores, std::vector<SliceWrite>& writes, std::size_t& at, std::chrono::nanoseconds slice, std::unordered_set<std::string>& missed_terms)
{
	L_CALL("Shard::apply_slice(<{} writes>, {})", writes.size(), at);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto& entry = endpoint();
		entry.last_flags_.store(flags, std::memory_order_relaxed);
		const int write_flags = flags;
		co_await chores.run(engine::ChoreClass::flush, [&] {
			ShardEndpoint::InlineEngine inline_engine(entry, "a RESTORE's apply slice writes on a worker in the index's turn");
			if (DocIndexer::seams.throw_in_slice.exchange(false)) {
				throw engine::Error(engine::errc::internal, "Test seam: the apply slice throws");
			}
			auto& eng = entry.engine();
			const auto until = std::chrono::steady_clock::now() + slice;
			const auto first = at;
			const auto limit = DocIndexer::seams.writes_per_slice.load(std::memory_order_relaxed);   // a test's fixed slice
			while (at < writes.size()) {
				auto& write = writes[at++];
				if (missed_terms.count(write.term) != 0) {
					write.missed = true;   // a later write of a term whose document left: in input order, after it
				} else if (const auto& stale_term = DocIndexer::seams.stale_on_term; !stale_term.empty() && write.term.ends_with(stale_term)) {
					const engine::Error error(engine::errc::reopen, "Test seam: a write marks the index stale");
					entry.note_failure(error);
					write.error = std::make_exception_ptr(error);
				} else if (write.shard_did != 0) {
					try {
						write.info = engine::sync_wait(entry.do_replace(eng, engine::inline_chores(), write_flags, write.shard_did, std::move(write.doc), true, false, true));
					} catch (const engine::Error& error) {
						if (engine::is_missing(error.kind)) {
							write.missed = true;   // gone since placement: placed afresh by the caller (B1)
							missed_terms.insert(write.term);
						} else {
							write.error = std::current_exception();
						}
					} catch (...) {
						write.error = std::current_exception();
					}
				} else {
					try {
						write.info = engine::sync_wait(entry.do_replace_term(eng, engine::inline_chores(), write_flags, write.term, std::move(write.doc), true, false));
					} catch (...) {
						write.error = std::current_exception();
					}
				}
				if (write.error && entry.stale_.load(std::memory_order_acquire)) {
					break;   // the index must be replaced: the later writes reopen it, as one request each would
				}
				if (limit != 0 ? at - first >= limit : std::chrono::steady_clock::now() >= until) {
					break;
				}
			}
		});
	}
	co_await finish_write(false, true);
}


task::task<bool>
Shard::reopen()
{
	// A reopen may wait for the index's replacement, which waits for its views: a held view could be one.
	engine::check_no_view_held("an open");
	const bool held = static_cast<bool>(pin_);
	const bool replaced = stale_ || !pin_ || pin_->stale_.load(std::memory_order_acquire);
	if (std::exchange(stale_, false) || (pin_ && pin_->stale_.load(std::memory_order_acquire))) {
		pin_.reopen();   // the catalog replaces the index once its other holders let go (x-pool D2)
	}
	co_await ready(!held);   // the re-pin of a pin handed back is not a use (CT-R31)
	co_return replaced;
}


void
Shard::retry_read(const engine::Error& error, int t)
{
	switch (engine::read_fate(error.kind)) {
		case engine::fate::retry_close:
			if (t == 0) { stale_ = true; throw; }
			return;
		case engine::fate::close_retry:
			stale_ = true;
			if (t == 0) { throw; }
			return;
		case engine::fate::close_fail:
			stale_ = true;
			throw;
		case engine::fate::fail:
			throw;
	}
}


void
Shard::collect_failure(const engine::Error& error, std::exception_ptr& eptr, size_t& valid, bool writes)
{
	auto fate = engine::read_fate(error.kind);
	if (writes && error.kind == engine::errc::reopen) {
		fate = engine::fate::close_fail;
	}
	switch (fate) {
		case engine::fate::retry_close:
			eptr = std::current_exception();
			--valid;
			return;
		case engine::fate::close_retry:
			stale_ = true;
			eptr = std::current_exception();
			--valid;
			return;
		case engine::fate::close_fail:
			stale_ = true;
			throw;
		case engine::fate::fail:
			throw;
	}
}


task::task<bool>
Shard::commit(bool wal_)
{
	L_CALL("Shard::commit({})", wal_);

	assert(is_writable());
	assert(is_write_active());

	co_return co_await endpoint().commit(flags, wal_);
}


// What follows a write that did not commit: the debounced autocommit.
static void
schedule_autocommit(const Shard& shard)
{
	if (shard.is_autocommit_active() && shard.endpoint().is_modified()) {
		if (!XapiandManager::schedule_autocommit(shard.endpoint())) {
			committer_commit(Endpoint(shard.endpoint()));   // no control loop to wait on (the shutdown began): commit now
		}
	}
}


// What follows a write: its commit when it asked for one, else the debounced autocommit. The write is applied
// and logged by now. A commit whose turn times out (busy) does not fail the write, which a client would retry
// (and an add without an id would then duplicate): the write stands, the autocommit commits it, and the
// client is told it succeeded.
task::task<void>
Shard::finish_write(bool commit_, bool wal_)
{
	if (!commit_) {
		schedule_autocommit(*this);
		co_return;
	}
	try {
		co_await commit(wal_);
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::busy) {
			throw;
		}
		L_WARNING("The commit of a write on {} waited too long for the index and was left to the autocommit", repr(endpoint().to_string()));
		schedule_autocommit(*this);
	}
}


task::task<void>
Shard::delete_document(engine::docid shard_did, bool commit_, bool wal_, std::optional<engine::rev> expected_)
{
	L_CALL("Shard::delete_document({}, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_delete(*eng, flags, shard_did, wal_, expected_);
	}
	co_await finish_write(commit_, wal_);
}


task::task<void>
Shard::delete_document_term(const std::string& term, bool commit_, bool wal_, std::optional<engine::rev> expected_)
{
	L_CALL("Shard::delete_document_term({}, {}, {})", repr(term), commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_delete_term(*eng, flags, term, wal_, expected_);
	}
	co_await finish_write(commit_, wal_);
}


task::task<engine::WriteResult>
Shard::add_document(engine::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::add_document(<doc>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_add(*eng, *endpoint().chores, flags, std::move(doc), wal_, version_);
	}
	co_await finish_write(commit_, wal_);
	co_return info;
}


task::task<engine::WriteResult>
Shard::replace_document(engine::docid shard_did, engine::Document&& doc, bool commit_, bool wal_, bool version_, bool must_exist)
{
	L_CALL("Shard::replace_document({}, <doc>, {}, {})", shard_did, commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_replace(*eng, *endpoint().chores, flags, shard_did, std::move(doc), wal_, version_, must_exist);
	}
	co_await finish_write(commit_, wal_);
	co_return info;
}


task::task<engine::WriteResult>
Shard::replace_document_term(const std::string& term, engine::Document&& doc, bool commit_, bool wal_, bool version_)
{
	L_CALL("Shard::replace_document_term({}, <doc>, {}, {}, {})", repr(term), commit_, wal_, version_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_replace_term(*eng, *endpoint().chores, flags, term, std::move(doc), wal_, version_);
	}
	co_await finish_write(commit_, wal_);
	co_return info;
}


task::task<void>
Shard::add_spelling(const std::string& word, engine::termcount freqinc, bool commit_, bool wal_)
{
	L_CALL("Shard::add_spelling(<word, <freqinc>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_add_spelling(*eng, flags, word, freqinc, wal_);
	}
	co_await finish_write(commit_, wal_);
}


task::task<engine::termcount>
Shard::remove_spelling(const std::string& word, engine::termcount freqdec, bool commit_, bool wal_)
{
	L_CALL("Shard::remove_spelling(<word>, <freqdec>, {}, {})", commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	engine::termcount result;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		result = co_await endpoint().do_remove_spelling(*eng, flags, word, freqdec, wal_);
	}
	co_await finish_write(commit_, wal_);
	co_return result;
}


task::task<void>
Shard::set_metadata(const std::string& key, const std::string& value, bool commit_, bool wal_)
{
	L_CALL("Shard::set_metadata({}, {}, {}, {})", repr(key), repr(value), commit_, wal_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		endpoint().last_flags_.store(flags, std::memory_order_relaxed);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_set_metadata(*eng, flags, key, value, wal_);
	}
	co_await finish_write(commit_, wal_);
}


task::task<engine::CheckResult>
Shard::check()
{
	L_CALL("Shard::check()");

	co_await ready();
	auto turn = co_await endpoint().take_turn(flags, cancel);
	co_return co_await endpoint().engine().check();
}


task::task<std::unique_ptr<engine::DocView>>
Shard::get_view(engine::docid shard_did)
{
	L_CALL("Shard::get_view({})", shard_did);

	std::unique_ptr<engine::DocView> doc;

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto* eng = co_await engine_for_read();
			doc = co_await eng->get(shard_did);
			break;
		} catch (const engine::Error& error) {
			retry_read(error, t);
		}
		co_await reopen();
	}

	co_return doc;
}


task::task<std::string>
Shard::get_metadata(const std::string& key)
{
	L_CALL("Shard::get_metadata({})", repr(key));

	std::string value;

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

#ifdef XAPIAND_DATA_STORAGE
	if (!key.empty() && key[0] == '\x00') {
		// The locator of a blob: a read of the data storage, in a chore.
		co_await ready();
		co_await endpoint().chores->run(engine::ChoreClass::query, [&] { value = endpoint().storage_read_blocking(std::string_view(key)); });
		co_return value;
	}
#endif  // XAPIAND_DATA_STORAGE

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto* eng = co_await engine_for_read();
			value = co_await eng->metadata(key);
			break;
		} catch (const engine::Error& error) {
			if (error.kind == engine::errc::invalid) { break; }
			retry_read(error, t);
		}
		co_await reopen();
	}

	co_return value;
}


task::task<std::vector<std::string>>
Shard::get_metadata_keys()
{
	L_CALL("Shard::get_metadata_keys()");

	std::vector<std::string> values;

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto* eng = co_await engine_for_read();
			values = co_await eng->metadata_keys();
			break;
		} catch (const engine::Error& error) {
			if (error.kind == engine::errc::invalid) { break; }
			retry_read(error, t);
		}
		values.clear();
		co_await reopen();
	}

	co_return values;
}


#ifdef XAPIAND_DATA_STORAGE
task::task<std::string>
Shard::storage_get_stored(const Locator& locator)
{
	L_CALL("Shard::storage_get_stored()");

	std::string value;
	co_await ready();
	co_await endpoint().chores->run(engine::ChoreClass::query, [&] { value = endpoint().storage_read_blocking(locator); });
	co_return value;
}


task::task<std::unique_ptr<StoredUpload>>
Shard::upload_begin(engine::Chores& chores, std::string ct_type, std::uint64_t size)
{
	L_CALL("Shard::upload_begin()");

	co_await ready();
	co_return co_await endpoint().upload_begin(chores, std::move(ct_type), size);
}
#endif /* XAPIAND_DATA_STORAGE */


std::string
Shard::to_string() const
{
	return pin_ ? endpoint().to_string() : Endpoint(path_).to_string();
}


std::string
Shard::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Shard {} ({}){}{}>",
		repr(to_string()),
		readable_flags(flags),
		is_writable() ? " " + DARK_STEEL_BLUE + "(writable)" + STEEL_BLUE : "",
		is_wal_active() ? " " + DARK_STEEL_BLUE + "(active WAL)" + STEEL_BLUE : "");
}


// The debounced autocommit of an index: a commit in its own turn, under a chore's pin, then its merges settled; nothing
// when the index is not open. A busy index's merges are published and synced here, since no write's turn may come.
task::task<bool>
autocommit(DatabasePool& pool, Endpoint endpoint)
{
	auto entry = co_await pool.pin_chore(endpoint);
	if (!entry) {
		co_return false;
	}
	const auto start = std::chrono::steady_clock::now();
	std::string error;
	bool pending = false;
	try {
		co_await entry->commit(DB_OPEN | DB_WRITABLE, true);
		pending = co_await entry->settle_merges(DB_OPEN | DB_WRITABLE);
	} catch (const engine::Error& exc) {
		pending = exc.kind == engine::errc::busy;   // the turn did not come: try again later
		error = exc.description();
	} catch (const Exception& exc) {
		error = exc.get_message();
	} catch (const std::exception& exc) {
		error = exc.what();
	}
	const auto end = std::chrono::steady_clock::now();
	if (error.empty()) {
		L_DEBUG("Autocommit of {} succeeded after {}", repr(entry->to_string()), strings::from_delta(start, end));
	} else {
		L_WARNING("Autocommit of {} failed after {}: {}", repr(entry->to_string()), strings::from_delta(start, end), error);
	}
	co_return pending;
}
