/*
 * Copyright (c) 2026 Dubalu LLC
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
#include <coroutine>               // for std::coroutine_handle
#include <cstdio>                  // for std::snprintf
#include <filesystem>              // for std::filesystem
#include <map>                     // for std::map
#include <mutex>                   // for std::mutex, std::lock_guard
#include <optional>                // for std::optional
#include <random>                  // for std::random_device
#include <system_error>            // for std::system_error

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "prospect/index.hh"       // for prospect::index

#include "engine/census.h"         // for ENGINE_CENSUS_*
#include "engine/error.h"          // for Error, errc
#include "engine/strata/document.h"  // for StrataDocument
#include "engine/strata/pin.h"    // for Pinned
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

// A key leads with a byte no term can start with (terms are UTF-8 text), so a field named "Quote"
// never collides with the key prefix; the term follows it unchanged.
constexpr char key_lead = '\xff';

std::string
engine_key(std::string_view term)
{
	std::string key;
	key.reserve(term.size() + 1);
	key.push_back(key_lead);
	key.append(term);
	return key;
}

constexpr std::string_view reserved_meta_prefix = "strata.";

// Rethrows the exception being handled as an engine error of its kind. Call only inside a catch block.
[[noreturn]] void
rethrow_translated()
{
	try {
		throw;
	} catch (const Error&) {
		throw;
	} catch (const ::strata::index_not_created& exc) {
		throw Error(errc::not_found, exc.what());
	} catch (const ::strata::index_fenced& exc) {
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
		throw Error(errc::internal, exc.what());
	}
}

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
	options.key_prefix = std::string(1, key_lead);
	options.auto_merge = true;
	return options;
}

}  // namespace


// The index of one shard and the state its handles share.
struct Core {
	std::string dir;
	std::unique_ptr<kronuz::journal::PosixIO> io;
	std::optional<prospect::index> index;
	std::string uuid;
	std::mutex mtx;
	bool dirty = false;        // applied since the last refresh: reads by id need a refresh first
	bool pending = false;      // applied since the last commit
	bool transaction = false;

	~Core() {
		if (index && pending) {
			try {
				index->commit();  // a dropped writer keeps its work, as a Xapian writer does
			} catch (...) { }
		}
	}

	void refresh() {
		if (dirty) {
			index->refresh();
			dirty = false;
		}
	}

	void commit() {
		if (pending) {
			index->commit();
			pending = dirty = false;
		}
	}

	rev revision() const { return index->manifest().revision; }
};


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
		entry->core = std::move(core);
	}
	++entry->handles;
	return std::make_unique<Handle>(std::move(entry), entry->core.get());
}

// A document as the index has it by id: its version and, when asked, its stored record.
struct Located {
	docid did = 0;
	rev version = 0;
	std::string stored;
};

// The live document with this id, or none. Reads what the index has flushed, so a buffer with
// writes is flushed first.
std::optional<Located>
locate(Core& core, docid did, bool want_stored)
{
	if (did == 0 || did > core.index->last_docid()) {
		return std::nullopt;
	}
	core.refresh();
	const auto segments = core.index->segments();
	for (auto it = segments.end(); it != segments.begin();) {
		const auto& view = *--it;
		const auto* seg = view.seg;
		const auto ord = seg->core().ordinal_of(did);
		if (!ord || !view.live(*ord)) {
			continue;
		}
		Located found;
		found.did = did;
		found.version = seg->get<::strata::keys_plugin>().version(*ord);
		if (want_stored) {
			found.stored = seg->get<::strata::stored_plugin>().stored(*ord);
		}
		return found;
	}
	return std::nullopt;
}

// The live documents of the flushed segments (the manifest's count waits for a commit).
doccount
live_count(Core& core)
{
	std::uint64_t live = 0;
	for (const auto& view : core.index->segments()) {
		live += view.seg->core().doc_count() - view.dead;
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

// Fills the index's document from ours.
void
fill(prospect::index::document& out, StrataDocument& doc)
{
	if (doc.key().empty()) {
		// A version belongs to a key in strata (a keyless document has none), and every Xapiand document has one.
		throw Error(errc::invalid, "The document has no key");
	}
	doc.normalise();
	out.key = engine_key(doc.key());
	out.stored = doc.data();
	out.terms.clear();
	out.terms.reserve(doc.terms().size());
	std::uint64_t length = 0;
	for (const auto& [term, info] : doc.terms()) {
		out.terms.push_back({term, info.wdf, info.positions});
		length += info.wdf;
	}
	out.length = static_cast<std::uint32_t>(std::min<std::uint64_t>(length, 0xffffffffULL));
	values::put(out, doc);
}

// Applies one operation; its docid and version as the index assigned them.
WriteResult
apply(Core& core, prospect::operation&& op, const WriteResult& expected)
{
	prospect::transaction t;
	t.ops.push_back(std::move(op));
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

class StrataDocView final : public DocView {
public:
	StrataDocView(docid did, rev version, std::string data) : did_(did), version_(version), data_(std::move(data)) { }

	docid did() const override { return did_; }
	std::string data() const override { return data_; }
	rev version() const override { return version_; }

	// TODO(P1): values come from the values plugin's columns once prospect has it.
	std::string value(valueno) const override { return {}; }
	void values(const ValueSink&) const override { }

	// TODO(P2): term lists. Until prospect has them a document's terms cannot be read back.
	void terms(std::string_view, const TermSink&) const override { unavailable(); }
	void term_details(const TermDetailSink&) const override { unavailable(); }
	std::string serialise() const override { unavailable(); }

private:
	[[noreturn]] static void unavailable() {
		throw Error(errc::not_available, "The strata engine does not keep term lists yet");
	}

	docid did_;
	rev version_;
	std::string data_;
};

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
		check_version(args.expected_version, std::nullopt);
		WriteResult info;
		info.did = static_cast<docid>(core.index->last_docid() + 1);
		info.version = 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		prospect::operation op;
		op.type = prospect::operation::add;
		fill(op.doc, native_of(doc));
		co_return apply(core, std::move(op), info);
	} catch (...) {
		rethrow_translated();
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
		auto& native = native_of(doc);
		std::optional<rev> stored;
		if (!native.key().empty()) {
			// A document with a key is found by it; the id must be the one that holds the key.
			if (const auto found = core.index->find_key(engine_key(native.key()))) {
				if (found->first != did) {
					throw Error(errc::invalid, "The key belongs to another document");
				}
				stored = static_cast<rev>(found->second);
			} else if (const auto located = locate(core, did, false)) {
				stored = located->version;
			}
		} else if (const auto located = locate(core, did, false)) {
			stored = located->version;
		}
		check_version(args.expected_version, stored);
		WriteResult info;
		info.did = did;
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		prospect::operation op;
		op.type = prospect::operation::replace_docid;
		op.docid = did;
		fill(op.doc, native);
		co_return apply(core, std::move(op), info);
	} catch (...) {
		rethrow_translated();
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
		const auto ekey = engine_key(key);
		WriteResult info;
		info.term = key;
		std::optional<rev> stored;
		if (const auto found = core.index->find_key(ekey)) {
			info.did = static_cast<docid>(found->first);
			stored = static_cast<rev>(found->second);
		} else {
			info.did = static_cast<docid>(core.index->last_docid() + 1);
		}
		check_version(args.expected_version, stored);
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		ENGINE_CENSUS_DOCUMENT(doc, info.term);
		auto& native = native_of(doc);
		if (native.key() != key) {
			native.set_key(key);
		}
		prospect::operation op;
		op.type = prospect::operation::replace_key;
		op.key = ekey;
		fill(op.doc, native);
		co_return apply(core, std::move(op), info);
	} catch (...) {
		rethrow_translated();
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
		const auto located = locate(core, did, false);
		if (!located) {
			throw Error(errc::doc_not_found, "Document not found");
		}
		if (local && expected_version) {
			check_version(expected_version, located->version);
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
		rethrow_translated();
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
		const auto ekey = engine_key(key);
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
		rethrow_translated();
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
		core.commit();
		(void)transaction;
		co_return;
	} catch (...) {
		rethrow_translated();
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
		if (core.transaction) {
			throw Error(errc::invalid, "A transaction is already in progress");
		}
		if (flushed) {
			core.commit();
		}
		core.transaction = true;
		co_return;
	} catch (...) {
		rethrow_translated();
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
		if (!core.transaction) {
			throw Error(errc::invalid, "No transaction is in progress");
		}
		core.commit();
		core.transaction = false;
		co_return;
	} catch (...) {
		rethrow_translated();
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
		if (!core.transaction) {
			throw Error(errc::invalid, "No transaction is in progress");
		}
		core.pending = core.dirty = false;  // what the reopen drops
		core.index.reset();
		core.index.emplace(prospect::index::open(*core.io, index_options()));
		core.transaction = false;
		co_return;
	} catch (...) {
		rethrow_translated();
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
		rethrow_translated();
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
	co_return false;  // one index in this process: a handle always sees the latest
}


task::task<Stats>
Shard::stats()
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.refresh();
		const auto& manifest = core.index->manifest();
		Stats stats;
		stats.uuid = core.uuid;
		stats.revision = manifest.revision;
		stats.lastdocid = static_cast<docid>(manifest.last_docid);
		stats.doc_count = live_count(core);
		stats.total_length = manifest.live_length;
		// TODO(P4): the document-length bounds; until then they are unknown.
		stats.avlength = stats.doc_count ? static_cast<double>(stats.total_length) / stats.doc_count : 0;
		stats.has_positions = true;
		co_return stats;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<docid>
Shard::find_key(const std::string& key)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		const auto found = core.index->find_key(engine_key(key));
		co_return found ? static_cast<docid>(found->first) : docid{0};
	} catch (...) {
		rethrow_translated();
	}
}


task::task<std::unique_ptr<DocView>>
Shard::get(docid did)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		auto found = locate(core, did, true);
		if (!found) {
			throw Error(errc::doc_not_found, "Document not found");
		}
		co_return std::make_unique<StrataDocView>(found->did, found->version, std::move(found->stored));
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::scan(docid from, const ScanSink& sink)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		core.refresh();
		struct Hit {
			docid did;
			rev version;
			std::string stored;
		};
		std::vector<Hit> hits;
		const auto segments = core.index->segments();
		for (const auto& view : segments) {
			const auto* seg = view.seg;
			const auto& core_seg = seg->core();
			for (::strata::ordinal_t ord = 0; ord < core_seg.doc_count(); ++ord) {
				const auto did = core_seg.docid(ord);
				if (did < from || !view.live(ord)) {
					continue;
				}
				hits.push_back({static_cast<docid>(did), seg->get<::strata::keys_plugin>().version(ord), seg->get<::strata::stored_plugin>().stored(ord)});
			}
		}
		std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.did < b.did; });
		for (auto& hit : hits) {
			StrataDocView view(hit.did, hit.version, std::move(hit.stored));
			sink(view);
		}
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<std::string>
Shard::metadata(const std::string& key)
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		const auto& all = core.index->metadata();
		const auto it = all.find(key);
		co_return it == all.end() ? std::string() : it->second;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<std::vector<std::string>>
Shard::metadata_keys()
{
	co_await Hop{on_};
	try {
		auto& core = *handle_->core;
		std::lock_guard<std::mutex> lk(core.mtx);
		std::vector<std::string> keys;
		for (const auto& entry : core.index->metadata()) {
			if (!entry.first.starts_with(reserved_meta_prefix)) {
				keys.push_back(entry.first);
			}
		}
		co_return keys;
	} catch (...) {
		rethrow_translated();
	}
}


rev
Shard::revision() const
{
	auto& core = *handle_->core;
	std::lock_guard<std::mutex> lk(core.mtx);
	return core.revision();
}


doccount
Shard::doc_count() const
{
	auto& core = *handle_->core;
	std::lock_guard<std::mutex> lk(core.mtx);
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
		core.refresh();
		pinned.segments_ = core.index->segments();
		pinned.identity_ = handle_->core;
	} catch (...) {
		rethrow_translated();
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
		core.refresh();
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
