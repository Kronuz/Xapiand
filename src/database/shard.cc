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
#include <cstdint>                // for std::uint64_t
#include <optional>                // for std::optional
#include <sys/types.h>            // for uint32_t, uint8_t, ssize_t

#include "at_end.h"               // for at_end::assert_none
#include "database/data.h"        // for Locator
#include "database/flags.h"       // for readable_flags, DB_*
#include "database/pool.h"        // for ShardEndpoint
#include "database/restore_batch.h" // for RestoreBatcher, restore_results
#include "database/shard_group.h" // for ShardGroup, ShardGroups
#include "database/handler.h"     // for committer
#include "database/utils.h"       // for DB_SLOT_VERSION
#include "engine/chores.h"          // for engine::Chores
#include "engine/open.h"            // for engine::OpenOptions
#include "engine/strata/shard.h"    // for engine::strata::borrow
#include "engine/sync.h"            // for engine::sync_wait
#include "xapiand_exception.h"            // for THROW, Error, MSG_Error, Exception, DocNot...
#include "index_resolver_lru.h"   // for IndexSettings
#include "length.h"               // for serialize_string
#include "log.h"                  // for L_CALL
#include "manager.h"              // for XapiandManager
#include "msgpack.h"              // for MsgPack
#include "random.hh"              // for random_int
#include "repr.hh"                // for repr
#include "reserved/fields.h"      // for ID_FIELD_NAME
#include "serialize.h"             // for Serialize::floating
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


// The version a write requires, as its document asks for it in the version slot.
static inline std::optional<engine::rev>
expected_version(const engine::Document& doc)
{
	auto serialized = doc.get_value(DB_SLOT_VERSION);
	if (serialized.empty()) {
		return std::nullopt;
	}
	return Unserialize::version(serialized);
}


// Completes a document the engine is about to write: embeds its new version in the stored data, compressing the
// record once, and clears the shards slot.
static void
finalise_write(engine::Document& doc, engine::WriteResult& info)
{
	bool data_modified = false;
	Data data(doc.get_data());
	auto data_obj = data.get_obj();
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
		doc.set_data(data.serialize());
	}
}


/*
 *  ____        _        _                    _____           _             _       _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  ___| ____|_ __   __| |_ __   ___ (_)_ __ | |_
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \  _| | '_ \ / _` | '_ \ / _ \| | '_ \| __|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/ |___| | | | (_| | |_) | (_) | | | | | |_
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|_____|_| |_|\__,_| .__/ \___/|_|_| |_|\__|
 *                                                             |_|
 * the parts that touch the engine and the group
 */

ShardEndpoint::ShardEndpoint(DatabasePool& database_pool_, std::string_view path_) :
	Endpoint(path_),
	database_pool(database_pool_),
	revision(0),
	chores(std::make_unique<IndexChores>(*database_pool_.work_pool_)),
	modified(false),
	applied_(0)
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


// The largest id a float `_id` holds with every integer below it: 2^53.
inline constexpr std::uint64_t MAX_FLOAT_ID = std::uint64_t{1} << 53;

// Writes an issued numeric id into a new document: its stored `_id` (in its placeholder's type), its id slot and its
// key, which is the one a PUT of that id names (an integer's and an equal float's are one), so the log and the stored
// data carry the id too. The record stays raw for finalise_write to compress once.
static std::string
issue_id(engine::Document& doc, std::uint64_t issued)
{
	Data data(doc.get_data());
	auto obj = data.get_obj();
	if (auto it = obj.find(ID_FIELD_NAME); it != obj.end()) {
		auto& value = it.value();
		switch (value.get_type()) {
			case MsgPack::Type::POSITIVE_INTEGER:
				value = issued;
				break;
			case MsgPack::Type::NEGATIVE_INTEGER:
				value = static_cast<int64_t>(issued);
				break;
			case MsgPack::Type::FLOAT:
				if (issued > MAX_FLOAT_ID) {
					throw engine::Error(engine::errc::invalid, "No numeric id is left to issue");
				}
				value = static_cast<double>(issued);
				break;
			default:
				break;
		}
		data.set_obj(obj, false);
		data.flush();
		doc.set_data(data.serialize());
	}
	auto key = numeric_key(issued);
	doc.set_key(key);
	doc.add_value(DB_SLOT_ID, Serialize::positive(issued));  // the id slot's value, as index_item gives it
	return key;
}


void
ShardEndpoint::raise_id_high(std::uint64_t id) noexcept
{
	if (id > id_high_) {
		id_high_ = id;
		id_high_moved_ = true;
	}
}


// The open's work (open_payload, x-pool D6), in the catalog's open chore with the slot `opening`: nobody else is on
// the index, so the open holds no turn and runs its chores inline. The engine then takes the index's chores, and the
// group recovers last. A throw fails the open, and the catalog destroys this entry, then the index.
void
ShardEndpoint::open_blocking(kronuz::journal::IO& directory, prospect::index& index)
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
	{
		// The id counter as the last commit left it; the group's catch-up below raises it to the log's.
		const auto stored = engine::sync_wait(engine_->metadata(std::string(ID_HIGH_METADATA)));
		id_high_ = stored.empty() ? 0 : MsgPack::unserialize(stored).u64();
		id_high_moved_ = false;
	}

	engine_->use_chores(chores.get());

	// The group last: it recovers on the index as the open left it (D169), and its failed open fails this one.
	if (auto* groups = database_pool.groups()) {
		refuse_without_journal(*groups, directory, index);
		group_ = std::make_unique<ShardGroup>(*groups, directory, index);
		auto* eng = &dynamic_cast<engine::strata::Shard&>(*engine_);
		group_->app.applied = [eng] { eng->applied_by_group(); };
		group_->on_checkpoint = [eng] { eng->checkpointed(); };
		group_->group.open_blocking();
		try {
			if (group_->group.damaged()) {
				throw engine::Error(engine::errc::corrupt, group_->group.damage_reason());
			}
			// What the log committed before the open is applied before anything is served, and reads see it; the id
			// counter is the log's.
			std::vector<std::uint64_t> left_open;
			std::string high;
			group_->catch_up([&](const ShardGroupState& state, prospect::index& applied) {
				for (const auto& chunked : state.open) {
					left_open.push_back(chunked.first);
				}
				if (const auto it = applied.metadata().find(std::string(ID_HIGH_METADATA)); it != applied.metadata().end()) {
					high = it->second;
				}
			});
			// The ready point (A9): the chunked commands this term's earlier proposers left open are aborted.
			group_->abort_open(left_open);
			if (!high.empty()) {
				id_high_ = std::max(id_high_, MsgPack::unserialize(high).u64());
			}
			id_high_logged_ = id_high_;
			id_high_moved_ = false;
		} catch (...) {
			refuse_opened_group(std::current_exception());
		}
	}
}


// An index that holds state (strata applied a log position, or the file set has files) whose group the journal does
// not hold: its journal was lost or moved. A new group would restart the log and the file set over that state
// (deleting its blob volumes, writing new records where old locators point), so the open refuses and touches nothing.
void
ShardEndpoint::refuse_without_journal(ShardGroups& groups, kronuz::journal::IO& directory, prospect::index& index)
{
	const auto id = ShardGroups::group_id(index.manifest().uuid);
	if (groups.holds(id)) {
		return;
	}
	bool files = false;
	for (auto cursor = directory.scan_directory(); auto name = cursor->next();) {
		if (name->starts_with(strata::group::bundle_detail::prefix)) {
			files = true;
			break;
		}
	}
	const auto applied = index.applied().index;
	if (applied == 0 && !files) {
		return;   // a new index: its group is created
	}
	const auto reason = strings::format("The journal in {} holds no group {:016x} for index {}, which holds state (applied through {}{}): restore the journal, or restore the index from a dump", repr(groups.directory()), id, repr(to_string()), applied, files ? ", blob volumes" : "");
	L_ERR("{}", reason);
	throw engine::Error(engine::errc::corrupt, reason);
}


// After the group's open, a failure (a damaged replica, a catch-up that throws) closes the group on its loop before
// the open fails: the catalog then destroys a group no longer attached. A damaged group's close skips its
// checkpoints; its own error is the damage again, so the refusal names the first cause.
[[noreturn]] void
ShardEndpoint::refuse_opened_group(std::exception_ptr failed)
{
	std::string reason;
	try {
		std::rethrow_exception(failed);
	} catch (const std::exception& exc) {
		reason = exc.what();
	} catch (...) {
		reason = "unknown error";
	}
	if (group_->group.damaged()) {
		reason = group_->group.damage_reason();
	}
	L_ERR("The index {} refuses to open: {}", repr(to_string()), reason);
	group_->quiesce();
	try {
		group_->group.close_blocking();
	} catch (...) {
		// The damage, or a failed checkpoint: the refusal below is the open's answer.
	}
	group_.reset();
	throw engine::Error(engine::errc::corrupt, "The index " + to_string() + " refuses to open: " + reason);
}


// The catalog's close_payload, before the index's close: the group checkpoints what it applied and keeps its closed
// record for a reopen in this process. A throw (fenced, a failed checkpoint) leaves its log and last bundle standing.
void
ShardEndpoint::close_group()
{
	if (!group_) {
		return;
	}
	auto& group = group_->group;
	group_->quiesce();
	group.close_blocking();
	database_pool.groups()->closed(group.id(), group.closed_record());
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
		// Nothing applied since the last commit: no revision (C25). A finished merge's output is all
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
	engine::sync_wait(do_commit(*engine_));
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
ShardEndpoint::commit(int flags, task::cancel_token cancel)
{
	L_CALL("ShardEndpoint::commit(({}))", readable_flags(flags));

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
		changed = group_ ? co_await commit_group() : co_await do_commit(*engine_);
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
ShardEndpoint::commit_group()
{
	if (!modified.load()) {
		co_return false;
	}
	auto& eng = dynamic_cast<engine::strata::Shard&>(*engine_);
	try {
		co_await checkpoint(*group_);
	} catch (...) {
		eng.rethrow_failure();   // a fenced index is the catalog's to replace (x-pool D2)
	}
	modified.store(false, std::memory_order_relaxed);
	set_revision(eng.revision());
	co_return true;
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
			index_->commit();   // nothing else is uncommitted: no revision (C25)
		}
		pending = index_->merges_pending() != 0 || index_->merge_output_unsynced();
	});
	co_return pending;
}


task::task<bool>
ShardEndpoint::do_commit(engine::Shard& eng)
{
	L_CALL("ShardEndpoint::do_commit()");

	if (!modified.load()) {
		co_return false;
	}

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	const auto prior_revision = eng.revision();
	if (id_high_moved_ && !group_) {
		co_await eng.set_metadata(std::string(ID_HIGH_METADATA), MsgPack(id_high_).serialize());  // durable with its documents
	}
	co_await eng.commit(engine::Transaction::none);
	id_high_moved_ = false;
	modified.store(false, std::memory_order_relaxed);
	const auto current_revision = eng.revision();
	if (prior_revision == current_revision) {
		L_DATABASE("Commit on shard {} was discarded, because it turned out not to change the revision", repr(to_string()));
		co_return false;
	}
	assert(current_revision == prior_revision + 1);
	L_DATABASE("Commit on shard {}: {} -> {}", repr(to_string()), prior_revision, current_revision);
	set_revision(current_revision);
	co_return true;
}


/*
 * The writes. Each runs under the index's turn: its proposal through the group (or the engine's apply without one)
 * and the counters are one unit.
 */

namespace {

// A write's level (D4): `quorum` answers once this replica applied it, with its verdict; `none` once it is placed.
strata::group::ack
ack_of(bool at_placement) noexcept
{
	return at_placement ? strata::group::ack::none : strata::group::ack::quorum;
}

// A command through the shard's group (D3), chunked past the entry limit (D12) with an id of the term the loop sees
// now; each entry proposed on the group's loop, the answer on the caller's scheduler.
task::task<strata::group::answer>
propose(ShardGroup& shard_group, const Command& command, strata::group::ack level = strata::group::ack::quorum)
{
	const auto id = co_await next_chunked_id(shard_group);
	const auto entries = group_proposer(shard_group);
	co_return co_await propose_command(entries, command, id, level);
}

// An upload's final entry (D12): the command of the document that names its blob, proposed once. A second raises
// errc::not_available: the first ended the upload's id whatever it applied.
task::task<strata::group::answer>
propose_upload_command(ShardGroup& shard_group, ChunkedUpload& upload, const Command& command, strata::group::ack level)
{
	if (upload.finished()) {
		throw engine::Error(engine::errc::not_available, "The blob's upload ended: send it again");
	}
	upload.finish();
	const auto entries = group_proposer(shard_group);
	co_return co_await propose_upload_final(entries, upload, command, level);
}

// What the proposer of a write hears once its replica applied it: the verdict's refusal, or the result prepare
// predicted, checked against it.
engine::WriteResult
landed(engine::Shard& eng, const engine::PreparedWrite& prepared, const strata::group::answer& answer)
{
	if (!answer.applied) {
		throw engine::Error(engine::errc::not_available, "The write was not applied: retry it");
	}
	return dynamic_cast<engine::strata::Shard&>(eng).landed(prepared, *answer.applied);
}

// A command write of the id counter's metadata (docs/design/one-id.md).
CommandWrite
id_high_write(std::uint64_t high)
{
	CommandWrite write;
	write.op = CommandOp::set_metadata;
	write.key = std::string(ID_HIGH_METADATA);
	write.document.bytes = MsgPack(high).serialize();
	return write;
}

bool
is_stored(const Locator& locator) noexcept
{
	return locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored;
}

// Whether a document names a stored blob: its record lives in a group's file set, so it needs the index's group.
bool
names_stored(const engine::Document& doc)
{
	const auto bytes = doc.get_data();
	if (bytes.empty()) {
		return false;
	}
	const Data data{std::string(bytes)};
	return std::any_of(data.begin(), data.end(), [](const Locator& locator) { return is_stored(locator); });
}

// A document's new stored blobs as its command's records (D3): each compressed here, at prepare, so every voter
// writes the same bytes, and its locator left a placeholder at volume 0 of the record's codec and size, which apply
// places. A placeholder already at volume 0 is `upload`'s: its record is the upload stream. `unstaged` keeps the
// data as it was when the document changed, for the caller's retry.
std::vector<CommandBlob>
stage_blobs(engine::Document& doc, const ChunkedUpload* upload, std::string& unstaged)
{
	std::vector<CommandBlob> blobs;
	auto bytes = doc.get_data();
	if (bytes.empty()) {
		return blobs;
	}
	std::vector<StoredPlace> places;
	{
		const Data data{std::string(bytes)};
		for (const auto& locator : data) {
			if (!is_stored(locator)) {
				continue;
			}
			auto ct_type = locator.ct_type.to_string();
			if (!locator.raw.empty()) {
				Locator packed{locator.ct_type};
				packed.type = Locator::Type::zstd_compressed_stored;
				packed.data(locator.data());
				auto& blob = blobs.emplace_back();
				blob.ct_type = ct_type;
				blob.record.bytes = std::string(packed.raw);
				places.push_back({std::move(ct_type), static_cast<std::uint8_t>(packed.type), 0, 0, blob.record.bytes.size()});
			} else if (locator.volume == 0) {
				if (upload == nullptr) {
					throw engine::Error(engine::errc::invalid, "A stored blob names no record");
				}
				blobs.push_back({std::move(ct_type), upload->record()});
			}
		}
	}
	if (!places.empty()) {
		unstaged = bytes;
		doc.set_data(place_stored(std::move(bytes), places));
	}
	return blobs;
}

}  // namespace


task::task<engine::WriteResult>
ShardEndpoint::write_document(engine::Shard& eng, engine::WriteOp op, engine::docid did, std::string key, engine::Document* doc, engine::WriteArgs args, ChunkedUpload* upload, CommandWrite* batched, bool at_placement)
{
	if (group_ == nullptr) {
		if (upload != nullptr || batched != nullptr || (doc != nullptr && names_stored(*doc))) {
			throw engine::Error(engine::errc::invalid, "A stored blob, a chunked upload or a batch needs the index's group");
		}
		const auto prepared = co_await eng.prepare(op, did, key, doc, std::move(args));
		co_return co_await eng.apply(prepared, doc);
	}
	std::string unstaged;
	std::exception_ptr failed;
	try {
		auto blobs = doc != nullptr ? stage_blobs(*doc, upload, unstaged) : std::vector<CommandBlob>();
		const auto prepared = co_await eng.prepare(op, did, key, doc, std::move(args));
		Command command;
		command.writes.push_back(command_write(prepared, doc));
		command.writes.back().blobs = std::move(blobs);
		if (batched != nullptr) {
			*batched = std::move(command.writes.back());
			co_return prepared.result;
		}
		// The id counter this write moves goes in its command, so it is durable with the document.
		const auto high = std::max(id_high_, op == engine::WriteOp::replace_key ? numeric_id(key) : std::uint64_t{0});
		if (high > id_high_logged_) {
			command.writes.push_back(id_high_write(high));
		}
		const auto level = ack_of(at_placement);
		const auto answer = upload != nullptr ? co_await propose_upload_command(*group_, *upload, command, level) : co_await propose(*group_, command, level);
		id_high_logged_ = std::max(id_high_logged_, high);
		if (at_placement) {
			unapplied_ = true;   // the turn's next holder waits for its apply
			engine::WriteResult placed;
			placed.term = prepared.result.term;   // no docid or version: its verdict is not known yet
			co_return placed;
		}
		co_return landed(eng, prepared, answer);
	} catch (...) {
		failed = std::current_exception();
	}
	if (!unstaged.empty()) {
		doc->set_data(std::move(unstaged));   // a retry stages the blobs again
	}
	std::rethrow_exception(failed);
}


task::task<engine::WriteResult>
ShardEndpoint::do_add(engine::Shard& eng, engine::Document&& doc, bool version_, bool at_placement)
{
	L_CALL("ShardEndpoint::do_add(<doc>, {})", version_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	engine::WriteArgs args;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	engine::WriteResult info;
	try {
		info = co_await write_document(eng, engine::WriteOp::add, 0, std::string(), &doc, std::move(args), nullptr, nullptr, at_placement);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
	co_return info;
}


task::task<engine::WriteResult>
ShardEndpoint::do_replace(engine::Shard& eng, engine::docid shard_did, engine::Document&& doc, bool version_, bool must_exist, ChunkedUpload* upload, bool at_placement)
{
	L_CALL("ShardEndpoint::do_replace({}, <doc>, {})", shard_did, version_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	engine::WriteResult info;
	info.did = shard_did;

	engine::WriteArgs args;
	args.must_exist = must_exist;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result); };
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}

	try {
		info = co_await write_document(eng, engine::WriteOp::replace, shard_did, std::string(), &doc, std::move(args), upload, nullptr, at_placement);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
	co_return info;
}


task::task<engine::WriteResult>
ShardEndpoint::do_replace_term(engine::Shard& eng, const std::string& term, engine::Document&& doc, bool version_, ChunkedUpload* upload, CommandWrite* batched, bool at_placement)
{
	L_CALL("ShardEndpoint::do_replace_term({}, <doc>, {})", repr(term), version_);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	std::string key = term;
	std::uint64_t issued = 0;
	if (term == NEW_NUMERIC_KEY) {
		// A new numeric id: issued from the counter, in a bucket this shard owns of the shards the handler placed it in.
		const auto shards = doc.get_value(DB_SLOT_SHARDS);
		const char *p = shards.data();
		const char *p_end = p + shards.size();
		const size_t shard_num = p == p_end ? 0 : unserialize_length(&p, p_end);
		const size_t n_shards = p == p_end ? 1 : unserialize_length(&p, p_end);
		const std::uint64_t floor = p == p_end ? 0 : unserialize_length(&p, p_end);  // the other shards' counters
		issued = next_auto_id(std::max(id_high_, floor), shard_num, n_shards);
		if (issued == 0) {
			throw engine::Error(engine::errc::invalid, "No numeric id is left to issue");
		}
		key = issue_id(doc, issued);  // before the blobs' push, whose record the log keeps
		raise_id_high(issued);  // taken even if the write fails: a retry gets the next one
	}

	engine::WriteArgs args;
	if (version_) {
		args.expected_version = expected_version(doc);
	} else {
		doc.add_value(DB_SLOT_VERSION, "");
	}
	if (issued != 0 && !args.expected_version) {
		args.expected_version = 0;  // a counter behind an existing id conflicts, never overwrites
	}

	engine::WriteResult info;
	info.term = key;
	args.finalise = [&](engine::Document& document, engine::WriteResult& result) { finalise_write(document, result); };
	try {
		info = co_await write_document(eng, engine::WriteOp::replace_key, 0, key, &doc, std::move(args), upload, batched, at_placement);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	if (issued == 0) {
		raise_id_high(numeric_id(key));  // an explicit numeric id advances the counter past it
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
	co_return info;
}


task::task<void>
ShardEndpoint::do_delete(engine::Shard& eng, engine::docid shard_did, std::optional<engine::rev> expected_, bool at_placement)
{
	L_CALL("ShardEndpoint::do_delete({})", shard_did);

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		engine::WriteArgs args;
		args.expected_version = expected_;
		co_await write_document(eng, engine::WriteOp::erase, shard_did, std::string(), nullptr, std::move(args), nullptr, nullptr, at_placement);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
}


task::task<void>
ShardEndpoint::do_delete_term(engine::Shard& eng, const std::string& term, std::optional<engine::rev> expected_, bool at_placement)
{
	L_CALL("ShardEndpoint::do_delete_term({})", repr(term));

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		engine::WriteArgs args;
		args.expected_version = expected_;
		co_await write_document(eng, engine::WriteOp::erase_key, 0, term, nullptr, std::move(args), nullptr, nullptr, at_placement);
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
}


task::task<void>
ShardEndpoint::do_set_metadata(engine::Shard& eng, const std::string& key, const std::string& value)
{
	L_CALL("ShardEndpoint::do_set_metadata({}, {})", repr(key), repr(value));

	RANDOM_ERRORS_DB_THROW(engine::Error, engine::errc::database, "Random Error");

	try {
		if (group_) {
			CommandWrite write;
			write.op = CommandOp::set_metadata;
			write.key = key;
			write.document.bytes = value;
			Command command;
			command.writes.push_back(std::move(write));
			landed(eng, engine::PreparedWrite{}, co_await propose(*group_, command));
		} else {
			co_await eng.set_metadata(key, value);
		}
	} catch (const engine::Error& error) {
		note_failure(error);
		throw;
	}
	applied_.fetch_add(1, std::memory_order_relaxed);
	modified.store(true, std::memory_order_relaxed);
}


task::task<void>
ShardEndpoint::do_add_spelling(engine::Shard& eng, const std::string& word, engine::termcount freqinc)
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
}


task::task<engine::termcount>
ShardEndpoint::do_remove_spelling(engine::Shard& eng, const std::string& word, engine::termcount freqdec)
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
	co_return result;
}


/*
 * The blobs: records apply wrote in the group's file set (D16), read in chores.
 */

std::string
ShardEndpoint::storage_read_blocking(const Locator& locator)
{
	assert(locator.type == Locator::Type::stored || locator.type == Locator::Type::lz4_compressed_stored || locator.type == Locator::Type::zstd_compressed_stored);
	assert(locator.volume != -1);

	if (!group_) {
		throw engine::Error(engine::errc::invalid, "A stored blob needs the index's group");
	}
	auto blob = read_stored(*group_->directory, static_cast<std::uint8_t>(locator.type), static_cast<std::uint64_t>(locator.volume), locator.offset, locator.size);
	return serialize_strings({ locator.ct_type.to_string(), blob });
}



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
	if (has_db_linearizable(flags)) {
		co_await endpoint().caught_up();   // read=linearizable (D4): every write committed before the read is applied
	}
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


task::task<std::pair<engine::doccount, std::uint64_t>>
Shard::written_load()
{
	co_await ready();
	auto turn = co_await endpoint().take_turn(flags, cancel);
	auto& entry = endpoint();
	co_return std::make_pair(co_await entry.engine().written_count(), entry.id_high_);
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

	if (endpoint().group() != nullptr) {
		co_await restore_slice(chores, writes, at, slice, missed_terms);
		co_await finish_write(false);
		co_return;
	}

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto& entry = endpoint();
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
						write.info = engine::sync_wait(entry.do_replace(eng, write.shard_did, std::move(write.doc), false, true));
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
						write.info = engine::sync_wait(entry.do_replace_term(eng, write.term, std::move(write.doc), false));
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
	co_await finish_write(false);
}


// apply_slice() through the index's group (D17): in the turn, a chore prepares the slice's writes, each an
// unconditional replace by key, into RestoreBatcher's commands of as many writes as fit one log entry; each command
// is then proposed in input order at the use's level, and at quorum its verdict assigns its writes' docids and
// versions; at ack=none it answers once placed, with neither. The chore also encodes each command's entry: the loop
// only proposes it.
task::task<void>
Shard::restore_slice(engine::Chores& chores, std::vector<SliceWrite>& writes, std::size_t& at, std::chrono::nanoseconds slice, std::unordered_set<std::string>& missed_terms)
{
	auto turn = co_await endpoint().take_turn(flags, cancel);
	auto& entry = endpoint();
	struct Batch {
		Command command;                   // the writes' keys, and their bytes when `entry` is empty
		std::vector<std::size_t> writes;   // their positions in `writes`, in the command's order
		std::string entry;                 // the command's one log entry; empty when it is chunked
	};
	std::vector<Batch> batches;
	RestoreBatcher batcher;
	std::vector<std::size_t> filling;
	const auto close = [&](Command command) {
		Batch batch{std::move(command), std::exchange(filling, {}), {}};
		if (auto entries = split_command(batch.command, 0); entries.size() == 1) {
			batch.entry = std::move(entries.front());
			for (auto& write : batch.command.writes) {
				std::string().swap(write.document.bytes);   // the entry holds them
				write.blobs.clear();
			}
		}
		batches.push_back(std::move(batch));
	};
	// Chores of `slice` each prepare the writes until the sub-run ends, so each proposal, and the group's syncs it
	// waits for, carries a full entry's worth of writes but the last. A test's fixed slice is one chore. The last
	// chore closes the open command, so the loop encodes no entry. A chore that fails after the first ends the slice
	// with the writes before it, which are proposed.
	const auto first = at;
	const auto limit = DocIndexer::seams.writes_per_slice.load(std::memory_order_relaxed);   // a test's fixed slice
	bool ended = false;   // a write left the index to be replaced
	bool done = false;    // the slice's last chore ran
	std::exception_ptr cut;
	std::uint64_t carried = 0;   // the id counter the slice's last command carries, durable with its writes
	const auto close_open = [&] {
		if (auto closed = batcher.flush()) {
			if (entry.id_high_ > entry.id_high_logged_) {
				carried = entry.id_high_;
				closed->writes.push_back(id_high_write(carried));
			}
			close(std::move(*closed));
		}
	};
	const auto prepare = [&] {
		ShardEndpoint::InlineEngine inline_engine(entry, "a RESTORE's apply slice prepares on a worker in the index's turn");
		if (DocIndexer::seams.throw_in_slice.exchange(false)) {
			throw engine::Error(engine::errc::internal, "Test seam: the apply slice throws");
		}
		auto& eng = entry.engine();
		const auto until = std::chrono::steady_clock::now() + slice;
		while (at < writes.size()) {
			const auto position = at++;
			auto& write = writes[position];
			if (missed_terms.count(write.term) != 0) {
				write.missed = true;   // a later write of a term whose document left: in input order, after it
			} else if (const auto& stale_term = DocIndexer::seams.stale_on_term; !stale_term.empty() && write.term.ends_with(stale_term)) {
				const engine::Error error(engine::errc::reopen, "Test seam: a write marks the index stale");
				entry.note_failure(error);
				write.error = std::make_exception_ptr(error);
			} else if (write.shard_did != 0 && engine::sync_wait(eng.written_key(write.term)) != write.shard_did) {
				write.missed = true;   // gone from its docid since placement (erased, or written again): placed afresh (B1)
				missed_terms.insert(write.term);
			} else {
				try {
					CommandWrite batched;
					write.info = engine::sync_wait(entry.do_replace_term(eng, write.term, std::move(write.doc), false, nullptr, &batched));
					if (auto closed = batcher.add(std::move(batched), 0)) {
						close(std::move(*closed));
					}
					filling.push_back(position);
				} catch (...) {
					write.error = std::current_exception();
				}
			}
			if (write.error && entry.stale_.load(std::memory_order_acquire)) {
				ended = true;   // the index must be replaced: the later writes reopen it, as one request each would
				break;
			}
			if (limit != 0 ? at - first >= limit : std::chrono::steady_clock::now() >= until) {
				break;
			}
		}
		done = ended || limit != 0 || at >= writes.size();
		if (done) {
			close_open();
		}
	};
	for (bool first_chore = true; !done; first_chore = false) {
		try {
			co_await chores.run(engine::ChoreClass::flush, prepare);
		} catch (...) {
			if (first_chore) {
				throw;
			}
			cut = std::current_exception();
			close_open();
			break;
		}
	}
	auto& eng = entry.engine();
	const bool at_placement = has_db_ack_none(flags);
	const auto level = ack_of(at_placement);
	bool last_failed = false;
	for (auto& batch : batches) {
		std::exception_ptr failed;
		try {
			auto proposing = batch.entry.empty() ? propose(*entry.group(), batch.command, level)
				: propose_entry(*entry.group(), std::move(batch.entry), level);
			const auto answer = co_await std::move(proposing);
			if (at_placement) {
				entry.unapplied_ = true;
				for (const auto position : batch.writes) {
					writes[position].info = engine::WriteResult{0, 0, std::move(writes[position].info.term)};
				}
				continue;
			}
			landed(eng, engine::PreparedWrite{}, answer);   // a refusal raises for the whole batch
			const auto results = restore_results(batch.command, *answer.applied);
			for (std::size_t i = 0; i < batch.writes.size(); ++i) {
				writes[batch.writes[i]].info = results[i];
			}
		} catch (...) {
			failed = std::current_exception();
		}
		last_failed = failed != nullptr;
		if (failed) {
			for (const auto position : batch.writes) {
				writes[position].error = failed;
			}
		}
	}
	if (carried != 0 && !last_failed) {
		entry.id_high_logged_ = std::max(entry.id_high_logged_, carried);   // the last command carried it
	}
	if (entry.id_high_ > entry.id_high_logged_) {
		// The ids the slice issued or wrote that no command carried, durable once this lands; a failure leaves it
		// to the next write's command.
		const auto high = entry.id_high_;
		Command command;
		command.writes.push_back(id_high_write(high));
		try {
			const auto answer = co_await propose(*entry.group(), command, level);
			if (at_placement) {
				entry.unapplied_ = true;
			} else {
				landed(eng, engine::PreparedWrite{}, answer);
			}
			entry.id_high_logged_ = std::max(entry.id_high_logged_, high);
		} catch (...) {
			L_EXC("The id counter of a RESTORE's slice was not logged");
		}
	}
	if (cut) {
		std::rethrow_exception(cut);   // the next write fails, as a slice failing before it would fail it
	}
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
Shard::commit()
{
	L_CALL("Shard::commit()");

	assert(is_writable());
	assert(is_write_active());

	co_return co_await endpoint().commit(flags);
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


// What follows a write: its refresh level (D4), then its commit when it asked for one, else the debounced
// autocommit. The write is logged by now, and applied unless it was answered at placement (ack=none): refresh=true
// and refresh=wait_for wait for that apply. A read refreshes a shard its writes left dirty first (Q-F27 (a)), so
// on the leader's replica a write is visible to every read once applied: wait_for asks no more, and refresh=true
// refreshes now, so no read pays it. A commit whose turn times out (busy) does not fail the write, which a
// client would retry (and an add without an id would then duplicate): the write stands, the autocommit commits it,
// and the client is told it succeeded.
task::task<void>
Shard::finish_write(bool commit_)
{
	if (has_db_refresh(flags) || has_db_refresh_wait_for(flags)) {
		if (has_db_ack_none(flags)) {
			co_await endpoint().caught_up();
		}
		if (has_db_refresh(flags)) {
			co_await endpoint().fresh(flags, cancel);
		}
	}
	if (!commit_) {
		schedule_autocommit(*this);
		co_return;
	}
	try {
		co_await commit();
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::busy) {
			throw;
		}
		L_WARNING("The commit of a write on {} waited too long for the index and was left to the autocommit", repr(endpoint().to_string()));
		schedule_autocommit(*this);
	}
}


task::task<void>
Shard::delete_document(engine::docid shard_did, bool commit_, std::optional<engine::rev> expected_)
{
	L_CALL("Shard::delete_document({}, {})", shard_did, commit_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_delete(*eng, shard_did, expected_, has_db_ack_none(flags));
	}
	co_await finish_write(commit_);
}


task::task<void>
Shard::delete_document_term(const std::string& term, bool commit_, std::optional<engine::rev> expected_)
{
	L_CALL("Shard::delete_document_term({}, {})", repr(term), commit_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_delete_term(*eng, term, expected_, has_db_ack_none(flags));
	}
	co_await finish_write(commit_);
}


task::task<engine::WriteResult>
Shard::add_document(engine::Document&& doc, bool commit_, bool version_)
{
	L_CALL("Shard::add_document(<doc>, {}, {})", commit_, version_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_add(*eng, std::move(doc), version_, has_db_ack_none(flags));
	}
	co_await finish_write(commit_);
	co_return info;
}


task::task<engine::WriteResult>
Shard::replace_document(engine::docid shard_did, engine::Document&& doc, bool commit_, bool version_, bool must_exist, ChunkedUpload* upload)
{
	L_CALL("Shard::replace_document({}, <doc>, {}, {})", shard_did, commit_, version_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_replace(*eng, shard_did, std::move(doc), version_, must_exist, upload, has_db_ack_none(flags));
	}
	co_await finish_write(commit_);
	co_return info;
}


task::task<engine::WriteResult>
Shard::replace_document_term(const std::string& term, engine::Document&& doc, bool commit_, bool version_, ChunkedUpload* upload)
{
	L_CALL("Shard::replace_document_term({}, <doc>, {}, {})", repr(term), commit_, version_);

	assert(is_writable());
	assert(is_write_active());

	engine::WriteResult info;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		info = co_await endpoint().do_replace_term(*eng, term, std::move(doc), version_, upload, nullptr, has_db_ack_none(flags));
	}
	co_await finish_write(commit_);
	co_return info;
}


ShardGroup*
Shard::group() const
{
	return endpoint().group();
}


task::task<void>
Shard::add_spelling(const std::string& word, engine::termcount freqinc, bool commit_)
{
	L_CALL("Shard::add_spelling(<word, <freqinc>, {})", commit_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_add_spelling(*eng, word, freqinc);
	}
	co_await finish_write(commit_);
}


task::task<engine::termcount>
Shard::remove_spelling(const std::string& word, engine::termcount freqdec, bool commit_)
{
	L_CALL("Shard::remove_spelling(<word>, <freqdec>, {})", commit_);

	assert(is_writable());
	assert(is_write_active());

	engine::termcount result;
	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		result = co_await endpoint().do_remove_spelling(*eng, word, freqdec);
	}
	co_await finish_write(commit_);
	co_return result;
}


task::task<void>
Shard::set_metadata(const std::string& key, const std::string& value, bool commit_)
{
	L_CALL("Shard::set_metadata({}, {}, {})", repr(key), repr(value), commit_);

	assert(is_writable());
	assert(is_write_active());

	{
		auto turn = co_await endpoint().take_turn(flags, cancel);
		auto* eng = &endpoint().engine();
		co_await endpoint().do_set_metadata(*eng, key, value);   // a schema's or a setting's: at quorum, whatever the write's level
	}
	co_await finish_write(commit_);
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


task::task<std::string>
Shard::storage_get_stored(const Locator& locator)
{
	L_CALL("Shard::storage_get_stored()");

	std::string value;
	co_await ready();
	co_await endpoint().chores->run(engine::ChoreClass::query, [&] { value = endpoint().storage_read_blocking(locator); });
	co_return value;
}


std::string
Shard::to_string() const
{
	return pin_ ? endpoint().to_string() : Endpoint(path_).to_string();
}


std::string
Shard::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Shard {} ({}){}>",
		repr(to_string()),
		readable_flags(flags),
		is_writable() ? " " + DARK_STEEL_BLUE + "(writable)" + STEEL_BLUE : "");
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
		co_await entry->commit(DB_OPEN | DB_WRITABLE);
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
