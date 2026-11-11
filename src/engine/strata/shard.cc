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
#include <chrono>                  // for std::chrono
#include <coroutine>               // for std::coroutine_handle
#ifdef XAPIAND_CRASH_POINTS
#include <csignal>                 // for SIGKILL
#include <cstdlib>                 // for std::getenv
#include <unistd.h>                // for getpid, kill
#endif
#include <cstdio>                  // for std::snprintf
#include <filesystem>              // for std::filesystem
#include <limits>                  // for std::numeric_limits
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
#include "engine/host.h"           // for engine::sleep_for
#include "engine/strata/document.h"  // for StrataDocument
#include "engine/strata/owed.h"  // for apply_settling
#include "engine/strata/viewed.h"  // for Viewed
#include "engine/strata/doc_view.h"  // for SegmentDocView, OwnedDocView
#include "engine/strata/values.h"  // for the value columns


namespace engine::strata {

namespace {

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
	} catch (const ::strata::index_closed& exc) {
		throw Error(errc::closed, exc.what());
	} catch (const ::strata::too_many_readers& exc) {
		throw Error(errc::busy, std::string("Too many searches are reading at once: ") + exc.what());
	} catch (const ::strata::readers_active& exc) {
		throw Error(errc::busy, exc.what());
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

}  // namespace


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


prospect::index::options
index_options(const OpenOptions& host)
{
	prospect::index::options options;
	options.keyed = true;
	options.auto_merge = true;
	options.defer_io = host.host;
	options.background = host.background;
	options.reader_records = host.readers_per_index;
	if (host.buffered_docs_cap != 0) {
		options.buffered_docs_cap = host.buffered_docs_cap;
	}
	return options;
}




// The index of one shard and the state its handles share. Everything that changes the index (the writes,
// refresh, commit, close, reopen and the housekeeping) runs one at a time, as the host's turn lets it;
// the reads take no turn and no lock: each acquires the index's published view through `readers`, and the
// index's own registry (strata R4.1) caps the views held at once (too_many_readers, a 503).
struct Core {
	std::string dir;
	std::chrono::milliseconds drain_timeout{30000};  // how long a close or a reopen waits for the readers of the old index
	std::unique_ptr<kronuz::journal::PosixIO> io;  // the hostless path's; a catalog's index has the catalog's
	prospect::index::options options;      // how the index is opened, again after a reopen
	bool host = false;                     // a host serialises the writers and refreshes before it reads
	// A host's catalog owns the index, its IO and its views' handle for as long as the pin that made this Core
	// holds the slot (docs/design/x-pool.md D2): a fence is the catalog's to replace, never this Core's.
	bool borrowed = false;
	std::optional<prospect::index> owned;  // the hostless path's index; empty after a reopen that failed
	prospect::index* index = nullptr;      // owned's, or the catalog's
	// The views' handle; null while there is no index to read. A reader loads it with no lock and no count: every
	// handle ever published is kept in `handles` until the Core ends, so the pointer a reader holds stays valid
	// (a handle of a replaced index raises index_closed when it is used). A borrowed Core points at the slot's.
	std::atomic<const prospect::index::reader_handle*> readers{nullptr};
	std::vector<std::unique_ptr<const prospect::index::reader_handle>> handles;   // the owner's: appended under the turn
	std::atomic<std::uint64_t> refused{0};  // views refused because the index's readers were at their cap
	std::string uuid;
	std::atomic<bool> fenced{false};  // the index refused to go on after an I/O error
	std::atomic<bool> retiring{false};  // the index is being replaced or closed: its readers are let go of
	std::atomic<bool> dirty{false};  // applied since the last refresh: reads need a refresh first
	bool pending = false;      // applied since the last commit
	bool transaction = false;
	std::atomic<bool> closed{false};  // Shard::close released the index on purpose
	std::atomic<std::uint64_t> epoch{0};  // bumped whenever the index is replaced: writes it held are gone

	// What the owner publishes for the threads that read without a turn.
	std::atomic<rev> committed_revision{0};          // the manifest's, as of the last commit
	std::atomic<std::uint64_t> committed_length{0};  // the live length the manifest holds
	std::atomic<std::size_t> owed{0};                // background work and views not yet freed

	~Core() {
		// A transaction that is still open is discarded. A borrowed index is committed by the catalog's commit chore.
		if (!borrowed && index && pending && !transaction && !index->closing()) {
			try {
				index->commit();  // a dropped writer keeps its work
			} catch (const std::exception& exc) {
				L_ERR("strata: the commit on closing {} failed: {}", dir, exc.what());
			} catch (...) {
				L_ERR("strata: the commit on closing {} failed", dir);
			}
		}
	}

	// Whether the handle can write with the index: a writable handle that has not seen the latest
	// replacement lost its writes with it and must be closed, so that opening it again replays the log.
	void check(const Handle& handle) const;

	// The same for a read, which also needs a published view to read.
	void check_read(const Handle& handle) const;

	bool unusable() const noexcept { return fenced.load() || retiring.load() || !index; }

	// Gives the readers the index's views and records the owner's figures. Call it after the index is made.
	void adopt_index() {
		handles.push_back(std::make_unique<const prospect::index::reader_handle>(index->reader()));
		readers.store(handles.back().get(), std::memory_order_release);
		note();
	}

	// The owner's figures for the readers and the housekeeping: call it when a refresh, commit, reclaim
	// or reopen finished.
	void note() noexcept {
		if (!index) {
			owed.store(0, std::memory_order_relaxed);
			return;
		}
		const auto& manifest = index->manifest();
		committed_revision.store(manifest.revision, std::memory_order_relaxed);
		committed_length.store(manifest.live_length, std::memory_order_relaxed);
		// A view that readers released stays until the next refresh, commit or reclaim frees it (C16).
		const auto held = index->views_held().views;
		owed.store(index->merges_pending() + index->reclaim_pending() + index->deletes_pending() + (held > 1 ? held - 1 : 0),
			std::memory_order_relaxed);
	}

	// Starts replacing the index: nobody can acquire its views from now on. The views readers hold keep
	// their segments until they are released (drain).
	void begin_retire() {
		retiring = true;
		readers.store(nullptr, std::memory_order_release);
		if (index && !index->closing()) {
			index->close();
		}
	}

	// Frees the views released since; true once none is held and the index has let go of its files.
	bool finish_retire() {
		return !index || index->try_finish_close();
	}

	// Opens the index again from its last commit; what was written since is lost. The handle that asks
	// (`seen`) knows of the replacement. Call it with no index (after retire).
	void open_index(std::atomic<std::uint64_t>* seen = nullptr) {
		const auto replaced = ++epoch;
		if (seen) {
			*seen = replaced;
		}
		pending = dirty = transaction = false;
		closed = false;
		index = nullptr;
		owned.reset();
		index = &owned.emplace(prospect::index::open(*io, options));
		try {
			verify_fields();
		} catch (...) {
			index = nullptr;  // a failed check leaves no index, as a failed open does
			owned.reset();
			readers.store(nullptr, std::memory_order_release);
			throw;
		}
		fenced = false;
		retiring = false;
		adopt_index();
	}

	void refresh() {
		if (dirty) {
			index->refresh();
			dirty = false;
			note();
		}
	}

	// A read's refresh. A host refreshes before it reads, in a chore (Shard::refresh), so a read does no I/O;
	// a shard with no host reads in the thread that writes it, and flushes here.
	void refresh_for_read() {
		if (!host) {
			refresh();
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
			note();
		}
	}

	rev revision() const { return committed_revision.load(std::memory_order_relaxed); }

	// Every segment's dictionary names only fields of the table and none is the key field
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
	std::atomic<std::uint64_t> seen{0};  // the latest replacement of the index this handle knows of (readers check it while a reopen sets it)

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
	if (closed) {
		throw Error(errc::closed, "The strata index is closed");
	}
	if (!index || retiring) {
		throw Error(errc::database, "The strata index is closed after a failed reopen; reopen the shard");
	}
	if (handle.writable && handle.seen != epoch.load()) {
		throw Error(errc::database, "The strata index was replaced, dropping this writer's unsaved writes; reopen the shard");
	}
}


void
Core::check_read(const Handle& handle) const
{
	if (closed) {
		throw Error(errc::closed, "The strata index is closed");
	}
	if (readers.load(std::memory_order_acquire) == nullptr) {
		throw Error(errc::database, "The strata index is closed after a failed reopen; reopen the shard");
	}
	if (handle.writable && handle.seen != epoch.load()) {
		throw Error(errc::database, "The strata index was replaced, dropping this writer's unsaved writes; reopen the shard");
	}
}


namespace {

// Opens the Core at `path`, or joins the one that is open. It throws not_found when the index does not
// exist and create_missing is off. A core that went unusable (fenced, or left without an index by a failed
// reopen) is returned as it is: open_local replaces its index.
std::unique_ptr<Handle>
acquire(const std::string& path, bool create_missing, bool& created, const OpenOptions& host)
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
		core->io = std::make_unique<kronuz::journal::PosixIO>(dir, 0600, kronuz::journal::PosixIOOptions{true, true});
		core->drain_timeout = host.drain_timeout;
		core->options = index_options(host);
		core->host = host.host;
		const auto& options = core->options;
		const auto create = [&] {
			const auto uuid = new_uuid();
			core->index = &core->owned.emplace(prospect::index::create(*core->io, options, uuid));
			created = true;
		};
		if (exists) {
			try {
				core->index = &core->owned.emplace(prospect::index::open(*core->io, options));
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
		core->adopt_index();
		entry->core = std::move(core);
	}
	++entry->handles;
	auto handle = std::make_unique<Handle>(std::move(entry), entry->core.get());
	handle->seen = handle->core->epoch.load();
	return handle;
}

// Waits, in the host's task, for the readers that hold the views of a closing index to let go of them: until
// then the views keep their segments, files and the directory lock. It raises `busy` when they do not within
// drain_timeout, leaving the index closing. Run it after Core::begin_retire; it needs no turn.
task::task<void>
drain_readers(Core& core, Chores& chores)
{
	check_no_view_held("a drain");
	const auto deadline = std::chrono::steady_clock::now() + core.drain_timeout;
	auto pause = std::chrono::milliseconds(1);
	for (;;) {
		co_await engine::sleep_for(pause);
		pause = std::min(pause * 2, std::chrono::milliseconds(50));
		bool done = false;
		co_await chores.run(ChoreClass::flush, [&core, &done] { done = core.finish_retire(); });
		if (done) {
			co_return;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			throw Error(errc::busy, "Readers still hold the index's views");
		}
	}
}

// Replaces the index of a core with one opened from its last commit; what was written since is lost. The
// handle that asks (`seen`) knows of the replacement. The old index is closed first, in the same chore when
// no reader holds a view of it, and the readers that do are waited for. Run it under the host's turn.
task::task<void>
reopen_core(Core& core, Chores& chores, std::atomic<std::uint64_t>* seen)
{
	if (core.borrowed) {
		// The catalog replaces its index (docs/design/x-pool.md D2): the pin's holder hands the pin back.
		throw Error(errc::reopen, "The strata index is the catalog's to replace; reopen the shard");
	}
	bool replaced = false;
	co_await chores.run(ChoreClass::flush, [&core, seen, &replaced] {
		if (core.index) {
			core.begin_retire();
			if (!core.finish_retire()) {
				return;
			}
		}
		core.open_index(seen);
		replaced = true;
	});
	if (!replaced) {
		co_await drain_readers(core, chores);
		co_await chores.run(ChoreClass::flush, [&core, seen] { core.open_index(seen); });
	}
}


// A document as the index has it by id: its version and where it is.
struct Located {
	docid did = 0;
	rev version = 0;
	const prospect::segment* seg = nullptr;
	prospect::ordinal_t ord = 0;
};

// The live document with this id in the view, or none.
std::optional<Located>
locate(const Viewed& viewed, docid did)
{
	if (did == 0 || did > viewed.view().last_docid()) {
		return std::nullopt;
	}
	const auto segments = viewed.segments();
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

// The document holding this key in the view: its id and version, or none. The newest segment that holds
// the key live answers, as the index's own lookup does.
std::optional<std::pair<docid, rev>>
locate_key(const Viewed& viewed, const std::string& key)
{
	const std::uint64_t hash = xxh64::hash(key);
	const auto segments = viewed.segments();
	for (auto it = segments.end(); it != segments.begin();) {
		const auto& view = *--it;
		const auto& keys = view.seg()->get<::strata::keys_plugin>();
		if (const auto ord = keys.find(key, ::strata::live_bits{view.deleted ? view.deleted->data() : nullptr}, hash)) {
			return std::pair{static_cast<docid>(view.seg()->core().docid(*ord)), static_cast<rev>(keys.version(*ord))};
		}
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

// The id after the shard's last: numbering past the largest docid is an error, never a wrap to the unreadable 0.
docid
next_docid(Core& core)
{
	const auto last = core.index->last_docid();
	if (last >= std::numeric_limits<docid>::max()) {
		throw Error(errc::database, "No document id left in the shard");
	}
	return static_cast<docid>(last + 1);
}

// The live documents of the flushed segments (the manifest's count waits for a commit).
doccount
live_count(const Viewed& viewed)
{
	std::uint64_t live = 0;
	for (const auto& view : viewed.segments()) {
		live += view.seg()->core().doc_count() - view.dead;
	}
	return static_cast<doccount>(live);
}

void
check_version(const std::optional<rev>& expected, const std::optional<rev>& stored)
{
	if (expected && *expected != stored.value_or(0)) {
		throw version_conflict(stored.value_or(0));
	}
}

// Stamps the version a write lands at: on the document, and in the version slot the host reads back to
// check a later write, so the slot is never the version the caller expected.
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
			throw version_conflict(verdict.current_version);
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
	out.key = doc.key();
	out.stored = doc.data();
	t.expected_fields = table.mark();
	out.fields.clear();
	out.fields.reserve(doc.fields().size());
	std::uint64_t length = doc.key_wdf();
	// Fields and terms go in the order they arrived: a new field is declared in that order, once.
	StrataDocument::Grouped grouped;
	doc.group(grouped);
	for (std::size_t f = 0; f < doc.fields().size(); ++f) {
		const auto& field = doc.fields()[f];
		const auto& name = field.name;
		prospect::inverted_plugin::field_part part;
		if (const auto id = table.find(name)) {
			part.field = ::strata::field_ref::id(*id);
		} else {
			part.field = ::strata::field_ref::declared(static_cast<std::uint32_t>(t.fields.size()));
			t.fields.push_back({name, ::strata::field_role::indexed, postings_of(field.kind), false});
		}
		part.terms.reserve(field.term_count);
		bool within = true;
		for (auto at = grouped.starts[f]; at < grouped.starts[f + 1]; ++at) {
			const auto& info = doc.terms()[grouped.order[at]];
			auto& entry = part.terms.emplace_back();
			entry.term.assign(doc.term_bytes(info));
			entry.wdf = info.wdf;
			doc.positions_into(info, entry.positions);
			part.length += info.wdf;
			within = within && info.size <= ::strata::format::max_term_bytes;
		}
		// The terms are unique by construction and positions_into makes theirs strictly increase: only the
		// size is left for the index to check.
		part.checked = within;
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

// Applies `t`, which the caller built from the index as it stands; the host's turn keeps any other write
// from changing it meanwhile. A host's index owes a flush, a drain or its bodies' maps before it takes the
// transaction (nothing was applied): that runs as a chore and the same transaction is applied again (x-pool D9).
// `visible` says the transaction changed what a read sees, so reads need a refresh.
task::task<::strata::verdict>
apply_t(Core& core, const Handle& handle, Chores& chores, prospect::transaction& t, bool visible)
{
	auto verdict = co_await apply_settling(
		[&] {
			core.check(handle);
			return core.index->apply(t, {});
		},
		[&](::strata::verdict::outcome owed) -> task::task<void> {
			co_await chores.run(ChoreClass::flush, [&core, &handle, owed] {
				core.check(handle);
				if (owed == ::strata::verdict::flush_owed) {
					core.index->refresh();
					core.dirty = false;
				} else if (owed == ::strata::verdict::drain_owed) {
					core.index->reclaim_for_space();
				} else {
					core.index->promote();   // a demoted index's bodies, mapped under the lease
				}
				core.note();
			});
		});
	if (verdict.result != ::strata::verdict::accepted) {
		reject(verdict);
	}
	core.dirty = core.dirty.load() || visible;
	core.pending = true;
	co_return verdict;
}

// Reads the id and version the index assigned to a write that apply_t accepted.
WriteResult
assigned(const ::strata::verdict& verdict, const WriteResult& expected)
{
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


Shard::Shard(std::unique_ptr<Handle> handle, bool writable, const OpenOptions& options)
	: handle_(std::move(handle)), writable_(writable), host_(options.host), chores_(options.chores ? options.chores : &inline_chores()) { }

Shard::~Shard() = default;


task::task<WriteResult>
Shard::add(Document& doc, WriteArgs args)
{
	ENGINE_CENSUS_COUNT("shard.add");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		WriteResult info;
		prospect::transaction t;
		{
			core.check(*handle_);
			check_version(args.expected_version, std::nullopt);
			info.did = next_docid(core);
			info.version = 1;
			if (args.finalise) {
				args.finalise(doc, info);
			}
			ENGINE_CENSUS_DOCUMENT(doc, info.term);
			auto& op = t.ops.emplace_back();
			op.type = prospect::operation::add;
			auto& native = native_of(doc);
			stamp_version(native, info.version);
			fill(t, op.doc, native, core.index->fields());
		}
		co_return assigned(co_await apply_t(core, *handle_, *chores_, t, true), info);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<WriteResult>
Shard::replace(docid did, Document& doc, WriteArgs args)
{
	ENGINE_CENSUS_COUNT("shard.replace");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		auto& native = native_of(doc);
		const bool replayed = native.replayed();
		WriteResult info;
		prospect::transaction t;
		{
			core.check(*handle_);
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
					if (stored && !replayed) {
						// The id holds another document: the keyed one does not exist, and the other is not overwritten.
						throw version_conflict(0);
					}
				}
			} else {
				stored = version_of(core, did);
			}
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
			auto& op = t.ops.emplace_back();
			op.type = prospect::operation::replace_docid;
			op.docid = did;
			fill(t, op.doc, native, core.index->fields());
		}
		auto result = assigned(co_await apply_t(core, *handle_, *chores_, t, true), info);
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
	ENGINE_CENSUS_COUNT("shard.replace_by_key");
	try {
		require_local(args);
		require_writable(writable_);
		auto& core = *handle_->core;
		WriteResult info;
		prospect::transaction t;
		{
			core.check(*handle_);
			const auto ekey = key;
			info.term = key;
			std::optional<rev> stored;
			bool placed = false;  // a new document at the id the caller gave
			if (const auto found = core.index->find_key(ekey)) {
				info.did = static_cast<docid>(found->first);
				stored = static_cast<rev>(found->second);
			} else if (args.new_did != 0 && !version_of(core, args.new_did)) {
				info.did = args.new_did;
				placed = true;
			} else {
				info.did = next_docid(core);
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
			auto& op = t.ops.emplace_back();
			if (placed) {
				op.type = prospect::operation::replace_docid;
				op.docid = info.did;
			} else {
				op.type = prospect::operation::replace_key;
				op.key = ekey;
			}
			fill(t, op.doc, native, core.index->fields());
		}
		co_return assigned(co_await apply_t(core, *handle_, *chores_, t, true), info);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::erase(docid did, bool local, std::optional<rev> expected_version)
{
	ENGINE_CENSUS_COUNT("shard.erase");
	try {
		require_writable(writable_);
		auto& core = *handle_->core;
		prospect::transaction t;
		{
			core.check(*handle_);
			const auto stored = version_of(core, did);
			check_version(expected_version, stored);  // first: an expected version on a missing document conflicts
			if (!stored) {
				throw Error(errc::doc_not_found, "Document not found");
			}
			t.ops.emplace_back();
			t.ops[0].type = prospect::operation::delete_docid;
			t.ops[0].docid = did;
		}
		co_await apply_t(core, *handle_, *chores_, t, true);
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<docid>
Shard::erase_by_key(const std::string& key, bool local, std::optional<rev> expected_version)
{
	ENGINE_CENSUS_COUNT("shard.erase_by_key");
	try {
		require_writable(writable_);
		auto& core = *handle_->core;
		prospect::transaction t;
		docid erased;
		{
			core.check(*handle_);
			const auto ekey = key;
			const auto found = core.index->find_key(ekey);
			check_version(expected_version, found ? std::optional<rev>(found->second) : std::nullopt);  // first, as in erase
			if (!found) {
				throw Error(errc::doc_not_found, "Document not found");
			}
			t.ops.emplace_back();
			t.ops[0].type = prospect::operation::delete_key;
			t.ops[0].key = ekey;
			erased = static_cast<docid>(found->first);
		}
		co_await apply_t(core, *handle_, *chores_, t, true);
		(void)local;
		co_return erased;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// A transaction here is a point the index returns to: the strata index has no rollback, so a cancel
// reopens it from its last commit. Opening a flushed transaction commits first, so a cancel loses only
// what the transaction wrote; an unflushed one takes the pending writes with it.
task::task<void>
Shard::commit(Transaction transaction)
{
	ENGINE_CENSUS_COUNT("shard.commit");
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::flush, [&core, this] {
			core.check(*handle_);
			core.commit();
		});
		(void)transaction;
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::begin_transaction(bool flushed)
{
	ENGINE_CENSUS_COUNT("shard.begin_transaction");
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::flush, [&core, this, flushed] {
			core.check(*handle_);
			if (core.transaction) {
				throw Error(errc::invalid, "A transaction is already in progress");
			}
			if (flushed) {
				core.commit();
			}
			core.transaction = true;
		});
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::commit_transaction()
{
	ENGINE_CENSUS_COUNT("shard.commit_transaction");
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::flush, [&core, this] {
			core.check(*handle_);
			if (!core.transaction) {
				throw Error(errc::invalid, "No transaction is in progress");
			}
			core.commit();
			core.transaction = false;
		});
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::cancel_transaction()
{
	ENGINE_CENSUS_COUNT("shard.cancel_transaction");
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::flush, [&core, this] {
			core.check(*handle_);
			if (!core.transaction) {
				throw Error(errc::invalid, "No transaction is in progress");
			}
		});
		// When the reopen fails the shard raises on every call until a reopen succeeds.
		co_await reopen_core(core, *chores_, &handle_->seen);
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::set_metadata(const std::string& key, const std::string& value)
{
	ENGINE_CENSUS_COUNT("shard.set_metadata");
	try {
		if (key.starts_with(reserved_meta_prefix)) {
			throw Error(errc::invalid, "Metadata keys starting with \"strata.\" are the engine's");
		}
		require_writable(writable_);
		auto& core = *handle_->core;
		prospect::transaction t;
		t.ops.emplace_back();
		auto& op = t.ops[0];
		op.type = value.empty() ? prospect::operation::delete_meta : prospect::operation::set_meta;
		op.meta_key = key;
		op.meta_value = value;
		// A read sees the metadata of the view it acquires: the write is visible after the next refresh.
		co_await apply_t(core, *handle_, *chores_, t, true);
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::add_spelling(const std::string&, termcount)
{
	throw Error(errc::not_available, "The strata engine does not keep spellings");
	co_return;
}


task::task<termcount>
Shard::remove_spelling(const std::string&, termcount)
{
	throw Error(errc::not_available, "The strata engine does not keep spellings");
	co_return 0;
}


task::task<bool>
Shard::reopen()
{
	try {
		// One index in this process: a handle always sees the latest. Only a fenced or failed index is replaced.
		auto& core = *handle_->core;
		const bool stale = writable_ && handle_->seen != core.epoch.load();
		if (!stale && !core.unusable()) {
			co_return false;
		}
		if (writable_ && (stale || core.fenced.load())) {
			// The writer's unsaved writes are gone with the index: it must be closed and opened again, which replays the log.
			if (core.unusable()) {
				co_await reopen_core(core, *chores_, nullptr);
			}
			throw Error(errc::reopen, "The strata index was replaced, dropping this writer's unsaved writes; reopen the shard");
		}
		co_await reopen_core(core, *chores_, &handle_->seen);
		co_return true;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<Stats>
Shard::stats()
{
	try {
		auto& core = *handle_->core;
		const auto viewed = view();
		Stats stats;
		stats.uuid = core.uuid;
		stats.revision = core.revision();
		stats.lastdocid = static_cast<docid>(viewed.view().last_docid());
		stats.doc_count = live_count(viewed);
		stats.total_length = core.committed_length.load(std::memory_order_relaxed);
		// The bounds are over every stored document of the segments that hold any, deleted ones included.
		bool any = false;
		for (const auto& segment : viewed.segments()) {
			if (!segment.seg()->core().doc_count()) { continue; }
			const auto& inverted = segment.seg()->get<prospect::inverted_plugin>();
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
	try {
		const auto viewed = view();
		const auto found = locate_key(viewed, key);
		co_return found ? found->first : docid{0};
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// The index's own lookup, as a write makes it: the buffers first, then the segments.
task::task<docid>
Shard::written_key(const std::string& key)
{
	try {
		auto& core = *handle_->core;
		core.check(*handle_);
		const auto found = core.index->find_key(key);
		co_return found ? static_cast<docid>(found->first) : docid{0};
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// The segments' live documents, with the buffers' pending deletes applied, and the buffered ones.
task::task<doccount>
Shard::written_count()
{
	try {
		auto& core = *handle_->core;
		core.check(*handle_);
		std::uint64_t live = core.index->buffered_count();
		for (const auto& segment : core.index->segments()) {
			live += segment.seg()->core().doc_count() - segment.dead;
		}
		co_return static_cast<doccount>(live);
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<std::unique_ptr<DocView>>
Shard::get(docid did)
{
	try {
		const auto viewed = view();
		const auto found = locate(viewed, did);
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
	CallbackGuard guard;
	try {
		const auto viewed = view();
		struct Hit {
			docid did;
			const prospect::segment* seg;
			prospect::ordinal_t ord;
		};
		std::vector<Hit> hits;
		for (const auto& segment : viewed.segments()) {
			const auto* seg = segment.seg();
			const auto& core_seg = seg->core();
			for (::strata::ordinal_t ord = 0; ord < core_seg.doc_count(); ++ord) {
				const auto did = core_seg.docid(ord);
				if (did < from || !segment.live(ord)) {
					continue;
				}
				hits.push_back({static_cast<docid>(did), seg, ord});
			}
		}
		std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.did < b.did; });
		for (auto& hit : hits) {
			const OwnedDocView doc(SegmentDocView(hit.did, *hit.seg, hit.ord));
			guard([&] { sink(doc); });
		}
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core, guard.threw());
	}
}


task::task<std::string>
Shard::metadata(const std::string& key)
{
	try {
		const auto viewed = view();
		const auto& all = viewed.view().metadata();
		const auto it = all.find(key);
		co_return it == all.end() ? std::string() : it->second;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<std::vector<std::string>>
Shard::metadata_keys()
{
	try {
		const auto viewed = view();
		std::vector<std::string> keys;
		for (const auto& entry : viewed.view().metadata()) {
			if (!entry.first.starts_with(reserved_meta_prefix)) {
				keys.push_back(entry.first);
			}
		}
		co_return keys;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


bool
Shard::dirty() const noexcept
{
	return handle_->core->dirty.load();
}


task::task<void>
Shard::refresh()
{
	try {
		auto& core = *handle_->core;
		if (core.dirty) {
			co_await chores_->run(ChoreClass::query, [&core, this] {
				core.check(*handle_);
				core.refresh();
			});
		}
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// Publishes the merges that finished and frees what the index queued for removal, the views readers
// released among it.
task::task<void>
Shard::reclaim()
{
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::merge, [&core, this] {
			core.check(*handle_);
			if (core.index->merges_pending() > 0 || core.dirty) {
				core.index->refresh();  // publishes the merges that finished
				core.dirty = false;
			}
			core.index->reclaim();
			core.note();
		});
		co_return;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


// Background work owed: merges not yet published, files queued for removal, deletes not yet collected, views
// that readers released and no refresh has freed.
std::size_t
Shard::merges_pending() const noexcept
{
	return handle_->core->owed.load(std::memory_order_relaxed);
}


void
Shard::use_chores(Chores* chores) noexcept
{
	chores_ = chores ? chores : &inline_chores();
}


task::task<bool>
Shard::begin_close()
{
	try {
		auto& core = *handle_->core;
		if (core.borrowed) {
			throw Error(errc::internal, "The strata index is the catalog's to close");
		}
		bool released = true;
		co_await chores_->run(ChoreClass::flush, [&core, &released] {
			if (!core.index) {
				return;
			}
			// An open transaction is discarded; pending work is kept.
			if (core.pending && !core.transaction && !core.fenced.load()) {
				core.index->commit();
				core.pending = core.dirty = false;
			}
			core.closed = true;
			core.begin_retire();
			released = core.finish_retire();
			if (released) {
				core.index = nullptr;
				core.owned.reset();
				core.owed.store(0, std::memory_order_relaxed);
			}
		});
		co_return released;
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<void>
Shard::finish_close()
{
	try {
		// Searches still read the index: it is closed when the last lets go of its view.
		auto& core = *handle_->core;
		if (core.borrowed) {
			throw Error(errc::internal, "The strata index is the catalog's to close");
		}
		co_await drain_readers(core, *chores_);
		co_await chores_->run(ChoreClass::flush, [&core] {
			core.index = nullptr;
			core.owned.reset();
			core.owed.store(0, std::memory_order_relaxed);
		});
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<CheckResult>
Shard::check()
{
	CheckResult result;
	try {
		auto& core = *handle_->core;
		co_await chores_->run(ChoreClass::flush, [&core, this] {
			core.check(*handle_);
			core.refresh();
			core.verify_fields();
			(void)core.index->check();
		});
	} catch (...) {
		try {
			rethrow_translated(handle_->core);
		} catch (const Error& error) {
			result.errors = 1;
			result.failure = error.description();
		}
	}
	co_return result;
}


rev
Shard::revision() const
{
	return handle_->core->revision();
}


doccount
Shard::doc_count() const
{
	const auto viewed = view();
	return live_count(viewed);
}


std::string
Shard::uuid() const
{
	return handle_->core->uuid;
}


std::vector<DeclaredField>
Shard::declared_fields() const
{
	const auto viewed = view();
	std::vector<DeclaredField> result;
	const auto& table = viewed.fields();
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


std::uint64_t
Shard::views_refused() const noexcept
{
	return handle_->core->refused.load(std::memory_order_relaxed);
}


std::size_t
Shard::view_handles_kept() const noexcept
{
	return handle_->core->handles.size();
}


const void*
Shard::identity() const noexcept
{
	return handle_->core;
}


Viewed
Shard::view() const
{
	auto& core = *handle_->core;
	try {
		core.check_read(*handle_);
		core.refresh_for_read();
		const auto* readers = core.readers.load(std::memory_order_acquire);
		if (readers == nullptr) {
			throw Error(errc::closed, "The strata index is closed");
		}
		try {
			return Viewed(readers->acquire(), handle_->core);
		} catch (const ::strata::too_many_readers&) {
			core.refused.fetch_add(1, std::memory_order_relaxed);
			throw;
		}
	} catch (...) {
		rethrow_translated(handle_->core);
	}
}


task::task<Opened>
open_local(const std::string& path, bool writable, bool create_missing, OpenOptions options)
{
	try {
		Opened opened;
		std::unique_ptr<Handle> handle;
		// Opening reads the manifest and maps the segments: a chore.
		Chores& chores = options.chores ? *options.chores : inline_chores();
		co_await chores.run(ChoreClass::query, [&] { handle = acquire(path, create_missing, opened.created, options); });
		handle->writable = writable;
		if (handle->core->unusable()) {
			// A fenced or failed index is replaced by one reopened from disk; the handles that live keep working on it.
			co_await reopen_core(*handle->core, chores, nullptr);
			handle->seen = handle->core->epoch.load();
		}
		opened.shard = std::make_unique<Shard>(std::move(handle), writable, options);
		co_return opened;
	} catch (...) {
		rethrow_translated();
	}
}


std::unique_ptr<Shard>
borrow(prospect::index& index, const prospect::index::reader_handle& readers, const std::string& path, const OpenOptions& options)
{
	try {
		auto core = std::make_unique<Core>();
		core->dir = (std::filesystem::path(path) / "strata").string();
		core->host = options.host;
		core->borrowed = true;
		core->index = &index;
		core->uuid = format_uuid(index.manifest().uuid);
		core->verify_fields();
		core->readers.store(&readers, std::memory_order_release);
		core->note();
		// An entry of its own, outside the hostless registry: the handle's end frees the Core, never the index.
		auto entry = std::make_shared<Entry>();
		entry->handles = 1;
		entry->core = std::move(core);
		auto* raw = entry->core.get();
		auto handle = std::make_unique<Handle>(std::move(entry), raw);
		handle->writable = true;
		return std::make_unique<Shard>(std::move(handle), true, options);
	} catch (...) {
		rethrow_translated();
	}
}


task::task<CheckResult>
check(const std::string& path, OpenOptions options)
{
	CheckResult result;
	Chores& chores = options.chores ? *options.chores : inline_chores();
	try {
		co_await chores.run(ChoreClass::query, [&] {
			bool created = false;
			auto handle = acquire(path, false, created, options);
			auto& core = *handle->core;
			core.check(*handle);
			core.refresh();
			core.verify_fields();
			(void)core.index->check();
		});
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


