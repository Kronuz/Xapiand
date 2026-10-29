/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "engine/strata/shard.h"

#include <algorithm>               // for std::sort
#include <array>                   // for std::array
#include <atomic>                  // for std::atomic
#include <coroutine>               // for std::coroutine_handle
#ifdef XAPIAND_CRASH_POINTS
#include <csignal>                 // for SIGKILL
#include <cstdlib>                 // for std::getenv
#include <unistd.h>                // for getpid, kill
#endif
#include <cstdio>                  // for std::snprintf
#include <filesystem>              // for std::filesystem
#include <map>                     // for std::map
#include <mutex>                   // for std::mutex, std::lock_guard
#include <optional>                // for std::optional
#include <random>                  // for std::random_device
#include <system_error>            // for std::system_error

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "prospect/index.hh"       // for prospect::index

#include "log.h"                   // for L_ERR
#include "sortable_serialise.h"    // for sortable_serialise

#include "database/utils.h"        // for DB_SLOT_VERSION
#include "engine/census.h"         // for ENGINE_CENSUS_*
#include "engine/error.h"          // for Error, errc
#include "engine/strata/document.h"  // for StrataDocument
#include "engine/strata/pin.h"    // for Pinned
#include "engine/strata/doc_view.h"  // for SegmentDocView, OwnedDocView
#include "engine/strata/values.h"  // for the value columns


namespace engine::strata {

namespace {

// Moves the awaiting task onto `on`, when there is one.
struct Hop {
	task::scheduler_ref on;

	bool await_ready() const noexcept { return !on; }
	void await_suspend(std::coroutine_handle<> handle) const { on.post(handle); }
	void await_resume() const noexcept { }
};


#ifdef XAPIAND_CRASH_POINTS
// Crash tests only: XAPIAND_STRATA_CRASH=point:n kills the process (SIGKILL) at the n-th pass through
// that point (commit.before, commit.flushed, commit.after, replay).
void
crash_point(const char* point)
{
	static const std::string spec = [] {
		const char* env = std::getenv("XAPIAND_STRATA_CRASH");
		return std::string(env ? env : "");
	}();
	static std::atomic<long> hits{0};
	const std::string_view want(spec);
	const auto colon = want.find(':');
	if (colon != std::string_view::npos && want.substr(0, colon) == point &&
		++hits == std::atol(std::string(want.substr(colon + 1)).c_str())) {
		::kill(::getpid(), SIGKILL);
	}
}
#else
inline void crash_point(const char*) { }
#endif

constexpr std::string_view reserved_meta_prefix = "strata.";

}  // namespace

void mark_fenced(Core* core) noexcept;

// Rethrows the exception being handled as an engine error of its kind; a fenced index marks its Core,
// so the next handle opened on it gets a fresh index. Call only inside a catch block.
[[noreturn]] void
rethrow_translated(Core* core, bool callback_threw)
{
	try {
		throw;
	} catch (const Error&) {
		throw;
	} catch (const ::strata::index_not_created& exc) {
		throw Error(errc::not_found, exc.what());
	} catch (const ::strata::index_fenced& exc) {
		mark_fenced(core);
		throw Error(errc::database, exc.what());
	} catch (const ::strata::corrupt_segment& exc) {
		throw Error(errc::corrupt, exc.what());
	} catch (const ::strata::corrupt_index& exc) {
		throw Error(errc::corrupt, exc.what());
	} catch (const std::system_error& exc) {
		throw Error(errc::database, exc.what());
	} catch (const std::invalid_argument& exc) {
		throw Error(errc::invalid, exc.what());
	} catch (const std::exception& exc) {
		if (callback_threw) {
			throw;
		}
		throw Error(errc::internal, exc.what());
	}
}


namespace {

std::string
format_uuid(const std::array<char, 16>& bytes)
{
	char text[37];
	std::snprintf(text, sizeof(text), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		static_cast<unsigned char>(bytes[0]), static_cast<unsigned char>(bytes[1]), static_cast<unsigned char>(bytes[2]), static_cast<unsigned char>(bytes[3]),
		static_cast<unsigned char>(bytes[4]), static_cast<unsigned char>(bytes[5]), static_cast<unsigned char>(bytes[6]), static_cast<unsigned char>(bytes[7]),
		static_cast<unsigned char>(bytes[8]), static_cast<unsigned char>(bytes[9]), static_cast<unsigned char>(bytes[10]), static_cast<unsigned char>(bytes[11]),
		static_cast<unsigned char>(bytes[12]), static_cast<unsigned char>(bytes[13]), static_cast<unsigned char>(bytes[14]), static_cast<unsigned char>(bytes[15]));
	return text;
}

std::array<char, 16>
new_uuid()
{
	std::random_device random;
	std::array<char, 16> bytes;
	for (std::size_t i = 0; i < bytes.size(); i += 4) {
		const auto word = random();
		for (std::size_t j = 0; j < 4; ++j) {
			bytes[i + j] = static_cast<char>(word >> (8 * j));
		}
	}
	bytes[6] = static_cast<char>((bytes[6] & 0x0f) | 0x40);
	bytes[8] = static_cast<char>((bytes[8] & 0x3f) | 0x80);
	return bytes;
}

constexpr kronuz::journal::PosixIOOptions io_options{true, true};

prospect::index::options
index_options()
{
	prospect::index::options options;
	options.keyed = true;
	options.auto_merge = true;
	return options;
}

}  // namespace


// The index of one shard and the state its handles share.
struct Core {
	std::string dir;
	std::unique_ptr<kronuz::journal::PosixIO> io;
	std::optional<prospect::index> index;  // empty after a reopen that failed: recover() retries it
	std::string uuid;
	std::mutex mtx;
	std::atomic<bool> fenced{false};  // the index refused to go on after an I/O error
	bool dirty = false;        // applied since the last refresh: reads by id need a refresh first
	bool pending = false;      // applied since the last commit
	bool transaction = false;
	std::atomic<std::uint64_t> epoch{0};  // bumped whenever the index is replaced: writes it held are gone

	~Core() {
		// A transaction that is still open is discarded, as a Xapian writer discards it.
		if (index && pending && !transaction) {
			try {
				index->commit();  // a dropped writer keeps its work, as a Xapian writer does
			} catch (const std::exception& exc) {
				L_ERR("strata: the commit on closing {} failed: {}", dir, exc.what());
			} catch (...) {
				L_ERR("strata: the commit on closing {} failed", dir);
			}
		}
	}

	// Under mtx: whether the handle can use the index; a writable handle that has not seen the latest
	// replacement lost its writes with it and must be closed, so that opening it again replays the log.
	void check(const Handle& handle) const;

	bool unusable() const noexcept { return fenced.load() || !index; }

	// Under mtx: drops the index and opens it again from its last commit; what was written since is lost.
	// The handle that asks (`seen`) knows of the replacement.
	void reopen_index(std::uint64_t* seen = nullptr) {
		const auto replaced = ++epoch;
		if (seen) {
			*seen = replaced;
		}
		pending = dirty = transaction = false;
		index.reset();
		index.emplace(prospect::index::open(*io, index_options()));
		try {
			verify_fields();
		} catch (...) {
			index.reset();  // a failed check leaves no index, as a failed open does
			throw;
		}
		fenced = false;
	}

	void refresh() {
		if (dirty) {
			index->refresh();
			dirty = false;
		}
	}

	void commit() {
		if (pending) {
			crash_point("commit.before");
			index->refresh();  // the segment is on disk and the manifest does not name it
			crash_point("commit.flushed");
			index->commit();
			crash_point("commit.after");
			pending = dirty = false;
		}
	}

	rev revision() const { return index->manifest().revision; }

	// Under mtx: every segment's dictionary names only fields of the table and none is the key field
	// (fielddict::check_fields); a segment that does not is corrupt. The merge's inputs are the segments
	// this verifies at open, so a merge cannot carry such an id into a new one.
	void verify_fields() const {
		const auto& table = index->fields();
		for (const auto& view : index->segments()) {
			view.seg()->get<prospect::inverted_plugin>().dictionary().check_fields(table.size(), table.key_field());
		}
	}
};

void
mark_fenced(Core* core) noexcept
{
	if (core) {
		core->fenced = true;
	}
}


namespace {

// The handles of a shard in this process share its Core; the entry serialises opening and closing it.
struct Entry {
	std::mutex mtx;
	int handles = 0;
	std::unique_ptr<Core> core;
};

std::mutex registry_mutex;
std::map<std::string, std::shared_ptr<Entry>> registry;

std::shared_ptr<Entry>
entry_for(const std::string& dir)
{
	std::lock_guard<std::mutex> lk(registry_mutex);
	auto& entry = registry[dir];
	if (!entry) {
		entry = std::make_shared<Entry>();
	}
	return entry;
}

}  // namespace


// One handle on a Core. The last one to go closes the index.
struct Handle {
	std::shared_ptr<Entry> entry;
	Core* core;
	bool writable = false;
	std::uint64_t seen = 0;  // the latest replacement of the index this handle knows of; under core->mtx

	Handle(std::shared_ptr<Entry> entry_, Core* core_) : entry(std::move(entry_)), core(core_) { }
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;

	~Handle() {
		std::lock_guard<std::mutex> lk(entry->mtx);
		if (--entry->handles == 0) {
			entry->core.reset();
		}
	}
};


void
Core::check(const Handle& handle) const
{
	if (!index) {
		throw Error(errc::database, "The strata index is closed after a failed reopen; reopen the shard");
	}
	if (handle.writable && handle.seen != epoch.load()) {
		throw Error(errc::database, "The strata index was replaced, dropping this writer's unsaved writes; reopen the shard");
	}
}


namespace {

// Opens the Core at `path`, or joins the one that is open; null when it does not exist and
// create_missing is off.
std::unique_ptr<Handle>
acquire(const std::string& path, bool create_missing, bool& created)
{
	namespace fs = std::filesystem;
	const auto dir = (fs::path(path) / "strata").string();
	auto entry = entry_for(dir);
	std::lock_guard<std::mutex> lk(entry->mtx);
	created = false;
	if (!entry->core) {
		std::error_code ec;
		const bool exists = fs::exists(dir, ec);
		if (!exists && !create_missing) {
			throw Error(errc::not_found, "No such database: " + path);
		}
		fs::create_directories(dir);
		auto core = std::make_unique<Core>();
		core->dir = dir;
		core->io = std::make_unique<kronuz::journal::PosixIO>(dir, 0600, io_options);
		const auto options = index_options();
		const auto create = [&] {
			const auto uuid = new_uuid();
			core->index.emplace(prospect::index::create(*core->io, options, uuid));
			created = true;
		};
		if (exists) {
			try {
				core->index.emplace(prospect::index::open(*core->io, options));
			} catch (const ::strata::index_not_created&) {
				if (!create_missing) {
					throw Error(errc::not_found, "No such database: " + path);
				}
				create();
			}
		} else {
			create();
		}
		core->uuid = format_uuid(core->index->manifest().uuid);
		core->verify_fields();
		entry->core = std::move(core);
	} else if (entry->core->unusable()) {
		// A fenced or failed index is replaced by one reopened from disk; the handles that live keep working on it.
		std::lock_guard<std::mutex> core_lock(entry->core->mtx);
		if (entry->core->unusable()) {
			entry->core->reopen_index();
		}
	}
	++entry->handles;
	auto handle = std::make_unique<Handle>(std::move(entry), entry->core.get());
	handle->seen = handle->core->epoch.load();
	return handle;
}

// A document as the index has it by id: its version and where it is.
struct Located {
	docid did = 0;
	rev version = 0;
	const prospect::segment* seg = nullptr;
	prospect::ordinal_t ord = 0;
};

// The live document with this id, or none. Reads what the index has flushed, so a buffer with
// writes is flushed first.
std::optional<Located>
locate(Core& core, docid did)
{
	if (did == 0 || did > core.index->last_docid()) {
		return std::nullopt;
	}
	core.refresh();
	const auto segments = core.index->segments();
	for (auto it = segments.end(); it != segments.begin();) {
		const auto& view = *--it;
		const auto* seg = view.seg();
		const auto ord = seg->core().ordinal_of(did);
		if (!ord || !view.live(*ord)) {
			continue;
		}
		Located found;
		found.did = did;
		found.version = seg->get<::strata::keys_plugin>().version(*ord);
		found.seg = seg;
		found.ord = *ord;
		return found;
	}
	return std::nullopt;
}

// The version of the live document with this id, or none. It reads the buffers too, so it flushes nothing.
std::optional<rev>
version_of(Core& core, docid did)
{
	if (did == 0 || did > core.index->last_docid()) {
		return std::nullopt;
	}
	if (const auto version = core.index->find_docid(did)) {
		return static_cast<rev>(*version);
	}
	return std::nullopt;
}

// The live documents of the flushed segments (the manifest's count waits for a commit).
doccount
live_count(Core& core)
{
	std::uint64_t live = 0;
	for (const auto& view : core.index->segments()) {
		live += view.seg()->core().doc_count() - view.dead;
	}
	return static_cast<doccount>(live);
}

void
check_version(const std::optional<rev>& expected, const std::optional<rev>& stored)
{
	if (expected && *expected != stored.value_or(0)) {
		throw Error(errc::version_conflict, "Version mismatch!");
	}
}

// Stamps the version a write lands at: on the document, and in the version slot the host reads back to
// check a later write (Xapian's adapter does the same), so the slot is never the version the caller expected.
void
stamp_version(StrataDocument& native, rev version)
{
	native.stamp(version);
	native.add_value(DB_SLOT_VERSION, sortable_serialise(static_cast<double>(version)));
}

[[noreturn]] void
reject(const ::strata::verdict& verdict)
{
	switch (verdict.result) {
		case ::strata::verdict::version_conflict:
			throw Error(errc::version_conflict, "Version mismatch!");
		case ::strata::verdict::invalid:
			throw Error(errc::invalid, "The index rejected the operation");
		case ::strata::verdict::no_space:
			throw Error(errc::database, "No space left for the operation");
		case ::strata::verdict::fields_conflict:
			throw Error(errc::internal, "The field table changed under a write");
		default:
			throw Error(errc::internal, "The index did not apply the operation");
	}
}

StrataDocument&
native_of(Document& doc)
{
	auto* native = dynamic_cast<StrataDocument*>(&doc.native());
	if (!native) {
		throw Error(errc::invalid, "The document was not built for the strata engine");
	}
	return *native;
}

// What a field's kind says its postings hold (Lucene's IndexOptions).
::strata::field_postings
postings_of(FieldKind kind)
{
	switch (kind) {
		case FieldKind::boolean: return ::strata::field_postings::docs;
		case FieldKind::terms: return ::strata::field_postings::freqs;
		case FieldKind::text: return ::strata::field_postings::positions;
	}
	return ::strata::field_postings::docs;
}

// Fills the index's document from ours, and names its fields in `t`: the ones the index knows by id under
// the table's mark, the others declared by the transaction; the key field too, once.
void
fill(prospect::transaction& t, prospect::index::document& out, StrataDocument& doc, const ::strata::field_table& table)
{
	if (doc.key().empty()) {
		// A version belongs to a key in strata (a keyless document has none), and every Xapiand document has one.
		throw Error(errc::invalid, "The document has no key");
	}
	doc.normalise();
	out.key = doc.key();
	out.stored = doc.data();
	t.expected_fields = table.mark();
	out.fields.clear();
	out.fields.reserve(doc.fields().size());
	std::uint64_t length = doc.key_wdf();
	// Fields and terms go in the order they arrived: a new field is declared in that order, once.
	for (const auto& field : doc.fields()) {
		const auto& name = field.name;
		prospect::inverted_plugin::field_part part;
		if (const auto id = table.find(name)) {
			part.field = ::strata::field_ref::id(*id);
		} else {
			part.field = ::strata::field_ref::declared(static_cast<std::uint32_t>(t.fields.size()));
			t.fields.push_back({name, ::strata::field_role::indexed, postings_of(field.kind), false});
		}
		part.terms.reserve(field.terms.size());
		for (const auto& info : field.terms) {
			part.terms.push_back({info.term, info.wdf, info.positions});
			part.length += info.wdf;
		}
		length += part.length;
		out.fields.push_back(std::move(part));
	}
	if (!table.key_field()) {
		t.fields.push_back({std::string(KEY_FIELD), ::strata::field_role::key, ::strata::field_postings::docs, false});
	}
	out.length = static_cast<std::uint32_t>(std::min<std::uint64_t>(length, 0xffffffffULL));
	values::put(out, doc);
}

// Whether a replayed line (stamped with the version it was written at) is to be applied over a document
// stored at `stored`: not when the index already holds that version or a later one, and only when it is
// exactly the next; anything else means a line of the log is missing.
bool
replay_applies(const StrataDocument& native, const std::optional<rev>& stored)
{
	const rev next = stored.value_or(0) + 1;
	if (stored && *stored >= native.version()) {
		return false;
	}
	if (native.version() != next) {
		throw Error(errc::corrupt, "The log skips a version of a document: it holds version " +
			std::to_string(native.version()) + " over " + std::to_string(stored.value_or(0)));
	}
	return true;
}

// Applies one operation; its docid and version as the index assigned them.
WriteResult
apply(Core& core, prospect::transaction& t, const WriteResult& expected)
{
	const auto verdict = core.index->apply(t, {});
	if (verdict.result != ::strata::verdict::accepted) {
		reject(verdict);
	}
	core.dirty = core.pending = true;
	WriteResult result = expected;
	if (!verdict.docids.empty()) {
		result.did = static_cast<docid>(verdict.docids[0]);
		result.version = verdict.versions[0];
	}
	if (result.did != expected.did || result.version != expected.version) {
		throw Error(errc::internal, "The index assigned a different id or version than the write expected");
	}
	return result;
}

// The errors for a write that cannot happen: a remote proxy, or a read-only handle.
void
require_local(const WriteArgs& args)
{
	if (!args.local) {
		throw Error(errc::not_available, "The strata engine has no remote shards");
	}
}

void
require_writable(bool writable)
{
	if (!writable) {
		throw Error(errc::invalid, "The shard is open read-only");
	}
}

}  // namespace


Shard::Shard(std::unique_ptr<Handle> handle, bool writable, task::scheduler_ref on) : handle_(std::move(handle)), writable_(writable), on_(on) { }

Shard::~Shard() = default;


task::task<WriteResult>
Shard::add(Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.add");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		check_version(args.expected_version, std::nullopt);
		WriteResult info;
		info.did = static_cast<docid>(core.index->last_docid() + 1);
		info.version = 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		prospect::transaction t;
		auto& op = t.ops.emplace_back();
		op.type = prospect::operation::add;
		auto& native = native_of(doc);
		stamp_version(native, info.version);
		fill(t, op.doc, native, core.index->fields());
		co_return apply(core, t, info);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<WriteResult>
Shard::replace(docid did, Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.replace");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		auto& native = native_of(doc);
		const bool replayed = native.replayed();
		std::optional<rev> stored;
		if (!native.key().empty()) {
			// A document with a key is found by it; the id must be the one that holds the key.
			if (const auto found = core.index->find_key(native.key())) {
				if (found->first != did) {
					if (replayed) {
						// The key was erased and added again after this line: the index is past it.
						WriteResult skipped;
						skipped.did = did;
						skipped.version = static_cast<rev>(found->second);
						co_return skipped;
					}
					throw Error(errc::invalid, "The key belongs to another document");
				}
				stored = static_cast<rev>(found->second);
			} else {
				stored = version_of(core, did);
			}
		} else {
			stored = version_of(core, did);
		}
		WriteResult info;
		info.did = did;
		if (replayed && !replay_applies(native, stored)) {
			// A log line the index already holds: replaying it again changes nothing.
			info.version = *stored;
			co_return info;
		}
		check_version(args.expected_version, stored);
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		stamp_version(native, info.version);
		prospect::transaction t;
		auto& op = t.ops.emplace_back();
		op.type = prospect::operation::replace_docid;
		op.docid = did;
		fill(t, op.doc, native, core.index->fields());
		auto result = apply(core, t, info);
		if (replayed) {
			crash_point("replay");
		}
		co_return result;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<WriteResult>
Shard::replace_by_key(const std::string& key, Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.replace_by_key");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto ekey = key;
		WriteResult info;
		info.term = key;
		std::optional<rev> stored;
		if (const auto found = core.index->find_key(ekey)) {
			info.did = static_cast<docid>(found->first);
			stored = static_cast<rev>(found->second);
		} else {
			info.did = static_cast<docid>(core.index->last_docid() + 1);
		}
		auto& native = native_of(doc);
		if (native.replayed() && !replay_applies(native, stored)) {
			info.version = *stored;
			co_return info;
		}
		check_version(args.expected_version, stored);
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		if (native.key() != key) {
			native.set_key(key);
		}
		stamp_version(native, info.version);
		prospect::transaction t;
		auto& op = t.ops.emplace_back();
		op.type = prospect::operation::replace_key;
		op.key = ekey;
		fill(t, op.doc, native, core.index->fields());
		co_return apply(core, t, info);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::erase(docid did, bool local, std::optional<rev> expected_version)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.erase");
	try {
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto stored = version_of(core, did);
		if (!stored) {
			throw Error(errc::doc_not_found, "Document not found");
		}
		if (local && expected_version) {
			check_version(expected_version, stored);
		}
		prospect::transaction t;
		t.ops.emplace_back();
		t.ops[0].type = prospect::operation::delete_docid;
		t.ops[0].docid = did;
		const auto verdict = core.index->apply(t, {});
		if (verdict.result != ::strata::verdict::accepted) {
			reject(verdict);
		}
		core.dirty = core.pending = true;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<docid>
Shard::erase_by_key(const std::string& key, bool local, std::optional<rev> expected_version)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.erase_by_key");
	try {
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto ekey = key;
		const auto found = core.index->find_key(ekey);
		if (!found) {
			throw Error(errc::doc_not_found, "Document not found");
		}
		if (expected_version) {
			check_version(expected_version, static_cast<rev>(found->second));
		}
		prospect::transaction t;
		t.ops.emplace_back();
		t.ops[0].type = prospect::operation::delete_key;
		t.ops[0].key = ekey;
		const auto verdict = core.index->apply(t, {});
		if (verdict.result != ::strata::verdict::accepted) {
			reject(verdict);
		}
		core.dirty = core.pending = true;
		(void)local;
		co_return static_cast<docid>(found->first);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// A transaction here is a point the index returns to: the strata index has no rollback, so a cancel
// reopens it from its last commit. Opening a flushed transaction commits first, so a cancel loses only
// what the transaction wrote; an unflushed one takes the pending writes with it, as Xapian does.
task::task<void>
Shard::commit(Transaction transaction)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.commit");
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		core.commit();
		(void)transaction;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::begin_transaction(bool flushed)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.begin_transaction");
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		if (core.transaction) {
			throw Error(errc::invalid, "A transaction is already in progress");
		}
		if (flushed) {
			core.commit();
		}
		core.transaction = true;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::commit_transaction()
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.commit_transaction");
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		if (!core.transaction) {
			throw Error(errc::invalid, "No transaction is in progress");
		}
		core.commit();
		core.transaction = false;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::cancel_transaction()
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.cancel_transaction");
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		if (!core.transaction) {
			throw Error(errc::invalid, "No transaction is in progress");
		}
		core.reopen_index(&handle_->seen);  // when it fails the shard raises on every call until a reopen succeeds
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::set_metadata(const std::string& key, const std::string& value)
{
	co_await Hop{on_};
	ENGINE_CENSUS_COUNT("shard.set_metadata");
	try {
		if (key.starts_with(reserved_meta_prefix)) {
			throw Error(errc::invalid, "Metadata keys starting with \"strata.\" are the engine's");
		}
		require_writable(writable_);
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		prospect::transaction t;
		t.ops.emplace_back();
		auto& op = t.ops[0];
		op.type = value.empty() ? prospect::operation::delete_meta : prospect::operation::set_meta;
		op.meta_key = key;
		op.meta_value = value;
		const auto verdict = core.index->apply(t, {});
		if (verdict.result != ::strata::verdict::accepted) {
			reject(verdict);
		}
		core.pending = true;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::add_spelling(const std::string&, termcount)
{
	co_await Hop{on_};
	throw Error(errc::not_available, "The strata engine does not keep spellings");
}


task::task<termcount>
Shard::remove_spelling(const std::string&, termcount)
{
	co_await Hop{on_};
	throw Error(errc::not_available, "The strata engine does not keep spellings");
}


task::task<bool>
Shard::reopen()
{
	co_await Hop{on_};
	try {
		// One index in this process: a handle always sees the latest. Only a fenced or failed index is replaced.
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		const bool stale = writable_ && handle_->seen != core.epoch.load();
		if (!stale && !core.unusable()) {
			co_return false;
		}
		if (writable_ && (stale || core.fenced.load())) {
			// The writer's unsaved writes are gone with the index: it must be closed and opened again, which replays the log.
			if (core.unusable()) {
				core.reopen_index();
			}
			throw Error(errc::reopen, "The strata index was replaced, dropping this writer's unsaved writes; reopen the shard");
		}
		core.reopen_index(&handle_->seen);
		co_return true;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<Stats>
Shard::stats()
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		core.refresh();
		const auto& manifest = core.index->manifest();
		Stats stats;
		stats.uuid = core.uuid;
		stats.revision = manifest.revision;
		stats.lastdocid = static_cast<docid>(manifest.last_docid);
		stats.doc_count = live_count(core);
		stats.total_length = manifest.live_length;
		// The bounds are over every stored document of the segments that hold any, deleted ones included.
		bool any = false;
		for (const auto& view : core.index->segments()) {
			if (!view.seg()->core().doc_count()) { continue; }
			const auto& inverted = view.seg()->get<prospect::inverted_plugin>();
			stats.doclen_lower = any ? std::min<termcount>(stats.doclen_lower, inverted.min_length()) : inverted.min_length();
			stats.doclen_upper = std::max<termcount>(stats.doclen_upper, inverted.max_length());
			any = true;
		}
		stats.avlength = stats.doc_count ? static_cast<double>(stats.total_length) / stats.doc_count : 0;
		stats.has_positions = true;
		co_return stats;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<docid>
Shard::find_key(const std::string& key)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto found = core.index->find_key(key);
		co_return found ? static_cast<docid>(found->first) : docid{0};
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<std::unique_ptr<DocView>>
Shard::get(docid did)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto found = locate(core, did);
		if (!found) {
			throw Error(errc::doc_not_found, "Document not found");
		}
		co_return std::make_unique<OwnedDocView>(SegmentDocView(found->did, *found->seg, found->ord));
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::scan(docid from, const ScanSink& sink)
{
	co_await Hop{on_};
	CallbackGuard guard;
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		core.refresh();
		struct Hit {
			docid did;
			const prospect::segment* seg;
			prospect::ordinal_t ord;
		};
		std::vector<Hit> hits;
		const auto segments = core.index->segments();
		for (const auto& view : segments) {
			const auto* seg = view.seg();
			const auto& core_seg = seg->core();
			for (::strata::ordinal_t ord = 0; ord < core_seg.doc_count(); ++ord) {
				const auto did = core_seg.docid(ord);
				if (did < from || !view.live(ord)) {
					continue;
				}
				hits.push_back({static_cast<docid>(did), seg, ord});
			}
		}
		std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.did < b.did; });
		for (auto& hit : hits) {
			const OwnedDocView view(SegmentDocView(hit.did, *hit.seg, hit.ord));
			guard([&] { sink(view); });
		}
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core, guard.threw());
	}
}


task::task<std::string>
Shard::metadata(const std::string& key)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		const auto& all = core.index->metadata();
		const auto it = all.find(key);
		co_return it == all.end() ? std::string() : it->second;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<std::vector<std::string>>
Shard::metadata_keys()
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle_);
		std::vector<std::string> keys;
		for (const auto& entry : core.index->metadata()) {
			if (!entry.first.starts_with(reserved_meta_prefix)) {
				keys.push_back(entry.first);
			}
		}
		co_return keys;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


rev
Shard::revision() const
{
	auto& core = *handle_->core;
	std::lock_guard<std::mutex> lk(core.mtx);
	core.check(*handle_);
	return core.revision();
}


doccount
Shard::doc_count() const
{
	auto& core = *handle_->core;
	std::lock_guard<std::mutex> lk(core.mtx);
	core.check(*handle_);
	core.refresh();
	return live_count(core);
}


std::string
Shard::uuid() const
{
	return handle_->core->uuid;
}


unsigned
Shard::refs() const
{
	return 1;
}


std::vector<DeclaredField>
Shard::declared_fields() const
{
	auto& core = *handle_->core;
	std::lock_guard<std::mutex> lk(core.mtx);
	core.check(*handle_);
	std::vector<DeclaredField> result;
	const auto& table = core.index->fields();
	for (std::size_t id = 0; id < table.size(); ++id) {
		const auto& declaration = table[id];
		DeclaredField field;
		field.name = declaration.name;
		field.key = declaration.role == ::strata::field_role::key;
		field.kind = declaration.postings == ::strata::field_postings::positions ? FieldKind::text :
			declaration.postings == ::strata::field_postings::freqs ? FieldKind::terms : FieldKind::boolean;
		result.push_back(std::move(field));
	}
	return result;
}


const void*
Shard::identity() const noexcept
{
	return handle_->core;
}


Pinned
Shard::pin()
{
	auto& core = *handle_->core;
	Pinned pinned{std::unique_lock<std::mutex>(core.mtx)};
	try {
		core.check(*handle_);
		core.refresh();
		pinned.segments_ = core.index->segments();
		pinned.fields_ = &core.index->fields();
		pinned.identity_ = handle_->core;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
	return pinned;
}


task::task<Opened>
open_local(const std::string& path, bool writable, bool create_missing, task::scheduler_ref on)
{
	co_await Hop{on};
	try {
		Opened opened;
		auto handle = acquire(path, create_missing, opened.created);
		handle->writable = writable;
		opened.shard = std::make_unique<Shard>(std::move(handle), writable, on);
		co_return opened;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<CheckResult>
check(const std::string& path, task::scheduler_ref on)
{
	co_await Hop{on};
	CheckResult result;
	try {
		bool created = false;
		auto handle = acquire(path, false, created);
		auto& core = *handle->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.check(*handle);
		core.refresh();
		core.verify_fields();
		(void)core.index->check();
	} catch (const Error& error) {
		result.errors = 1;
		result.failure = error.description();
	} catch (const std::exception& exc) {
		result.errors = 1;
		result.failure = exc.what();
	}
	co_return result;
}

}  // namespace engine::strata
