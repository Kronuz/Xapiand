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

#include "engine/xapian/shard.h"

#include <coroutine>               // for std::coroutine_handle
#include <optional>                // for std::optional
#include <string_view>             // for std::string_view
#include <type_traits>             // for std::invoke_result_t

#include "config.h"                // for XAPIAND_DATABASE_WAL
#include "database/utils.h"        // for DB_SLOT_VERSION
#include "engine/xapian/convert.h" // for to_xapian, from_xapian
#include "engine/xapian/errors.h"  // for rethrow_translated, VersionConflict, DocNotFound
#include "length.h"                // for serialise_length
#include "sortable_serialise.h"    // for sortable_serialise, sortable_unserialise


namespace engine::xapian {

namespace {

// Moves the awaiting task onto `on`, when there is one.
struct Hop {
	task::scheduler_ref on;

	bool await_ready() const noexcept { return !on; }
	void await_suspend(std::coroutine_handle<> handle) const { on.post(handle); }
	void await_resume() const noexcept { }
};

// The term that records a document's version.
std::string
version_term(docid did, rev version)
{
	return "V" + serialise_length(did) + sortable_serialise(static_cast<double>(version));
}

// Adds the version bookkeeping to the document about to be written.
void
stamp(Document& doc, const WriteResult& info)
{
	doc.add_boolean_term(version_term(info.did, info.version));
	doc.add_value(DB_SLOT_VERSION, sortable_serialise(static_cast<double>(info.version)));
}

}  // namespace


Xapian::WritableDatabase&
Shard::writable() const
{
	return static_cast<Xapian::WritableDatabase&>(*database_);
}


std::optional<rev>
Shard::stored_version(docid did) const
{
	const auto prefix = "V" + serialise_length(did);
	auto& db = writable();
	const auto end = db.allterms_end(prefix);
	for (auto it = db.allterms_begin(prefix); it != end; ++it) {
		std::string term = *it;
		std::string_view stored(term);
		stored.remove_prefix(prefix.size());
		if (!stored.empty()) {
			return static_cast<rev>(sortable_unserialise(stored));
		}
	}
	return std::nullopt;
}


void
Shard::check_version(const std::optional<rev>& expected, const std::optional<rev>& stored) const
{
	if (expected && *expected != stored.value_or(0)) {
		throw VersionConflict("Version mismatch!");
	}
}


task::task<WriteResult>
Shard::add(Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		WriteResult info;
		if (!args.local) {
			co_return from_xapian(db.add_document(to_xapian(doc)));
		}
		check_version(args.expected_version, std::nullopt);
		info.did = db.get_lastdocid() + 1;
		info.version = 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		stamp(doc, info);
		db.replace_document(info.did, to_xapian(doc));
		co_return info;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<WriteResult>
Shard::replace(docid did, Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		WriteResult info;
		if (!args.local) {
			co_return from_xapian(db.replace_document(did, to_xapian(doc)));
		}
		info.did = did;
		const auto stored = stored_version(did);
		check_version(args.expected_version, stored);
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		stamp(doc, info);
		db.replace_document(did, to_xapian(doc));
		co_return info;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<WriteResult>
Shard::replace_by_key(const std::string& key, Document& doc, WriteArgs args)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		WriteResult info;
		if (!args.local) {
			co_return from_xapian(db.replace_document(key, to_xapian(doc)));
		}
		info.term = key;
		std::optional<rev> stored;
		const auto it = db.postlist_begin(key);
		if (it == db.postlist_end(key)) {
			info.did = db.get_lastdocid() + 1;
		} else {
			info.did = *it;
			stored = stored_version(info.did);
		}
		check_version(args.expected_version, stored);
		info.version = stored.value_or(0) + 1;
		if (args.finalise) {
			args.finalise(doc, info);
		}
		stamp(doc, info);
		db.replace_document(info.did, to_xapian(doc));
		co_return info;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::erase(docid did, bool local, std::optional<rev> expected_version)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		if (local && expected_version) {
			check_version(expected_version, stored_version(did));
		}
		db.delete_document(did);
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<docid>
Shard::erase_by_key(const std::string& key, bool local, std::optional<rev> expected_version)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		if (!local) {
			db.delete_document(key);
			co_return 0;
		}
		const auto it = db.postlist_begin(key);
		if (it == db.postlist_end(key)) {
			throw DocNotFound("Document not found");
		}
		const docid did = *it;
		if (expected_version) {
			check_version(expected_version, stored_version(did));
		}
		db.delete_document(did);
		co_return did;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::commit(Transaction transaction)
{
	co_await Hop{on_};
	try {
		auto& db = writable();
		if (transaction == Transaction::flushed) {
			db.commit_transaction();
			db.begin_transaction(true);
		} else if (transaction == Transaction::unflushed) {
			db.commit_transaction();
			db.commit();
			db.begin_transaction(false);
		} else {
			db.commit();
		}
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::begin_transaction(bool flushed)
{
	co_await Hop{on_};
	try {
		writable().begin_transaction(flushed);
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::commit_transaction()
{
	co_await Hop{on_};
	try {
		writable().commit_transaction();
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::cancel_transaction()
{
	co_await Hop{on_};
	try {
		writable().cancel_transaction();
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::set_metadata(const std::string& key, const std::string& value)
{
	co_await Hop{on_};
	try {
		writable().set_metadata(key, value);
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<void>
Shard::add_spelling(const std::string& word, termcount increment)
{
	co_await Hop{on_};
	try {
		writable().add_spelling(word, increment);
		co_return;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<termcount>
Shard::remove_spelling(const std::string& word, termcount decrement)
{
	co_await Hop{on_};
	try {
		co_return writable().remove_spelling(word, decrement);
	} catch (...) {
		rethrow_translated();
	}
}


task::task<bool>
Shard::reopen()
{
	co_await Hop{on_};
	try {
		co_return database_->reopen();
	} catch (...) {
		rethrow_translated();
	}
}


rev
Shard::revision() const
{
	try {
		return database_->get_revision();
	} catch (...) {
		rethrow_translated();
	}
}


std::string
Shard::uuid() const
{
	try {
		return database_->get_uuid();
	} catch (...) {
		rethrow_translated();
	}
}


unsigned
Shard::refs() const
{
	return database_->refs();
}


task::task<Opened>
open_local(const std::string& path, bool writable, bool create_missing, task::scheduler_ref on)
{
	co_await Hop{on};
	try {
		Opened opened;
		if (writable) {
#ifdef XAPIAND_DATABASE_WAL
			constexpr int sync_mode = Xapian::DB_NO_SYNC;
#else
			constexpr int sync_mode = 0;
#endif
			try {
				opened.shard = std::make_unique<Shard>(std::make_unique<Xapian::WritableDatabase>(path, Xapian::DB_OPEN | Xapian::DB_RETRY_LOCK | sync_mode), on);
			} catch (const Xapian::DatabaseNotFoundError&) {
				if (!create_missing) {
					throw;
				}
				opened.shard = std::make_unique<Shard>(std::make_unique<Xapian::WritableDatabase>(path, Xapian::DB_CREATE | Xapian::DB_RETRY_LOCK | sync_mode), on);
				opened.created = true;
			}
		} else {
			try {
				opened.shard = std::make_unique<Shard>(std::make_unique<Xapian::Database>(path, Xapian::DB_OPEN), on);
			} catch (const Xapian::DatabaseNotFoundError&) {
				if (!create_missing) {
					throw;
				}
				Xapian::WritableDatabase(path, Xapian::DB_CREATE);
				opened.created = true;
				opened.shard = std::make_unique<Shard>(std::make_unique<Xapian::Database>(path, Xapian::DB_OPEN), on);
			}
		}
		co_return opened;
	} catch (...) {
		rethrow_translated();
	}
}


task::task<std::unique_ptr<Shard>>
open_remote(const std::string& host, unsigned port, int flags, const std::string& path, bool writable, task::scheduler_ref on)
{
	co_await Hop{on};
	try {
		if (writable) {
			co_return std::make_unique<Shard>(std::make_unique<Xapian::WritableDatabase>(Xapian::Remote::open_writable(host, port, 10000, 10000, flags, path)), on);
		}
		co_return std::make_unique<Shard>(std::make_unique<Xapian::Database>(Xapian::Remote::open(host, port, 10000, 10000, flags, path)), on);
	} catch (...) {
		rethrow_translated();
	}
}


task::task<std::unique_ptr<Shard>>
open_local_if_same(const Shard& remote, const std::string& path, task::scheduler_ref on)
{
	co_await Hop{on};
	try {
		auto local = std::make_unique<Xapian::Database>(path, Xapian::DB_OPEN);
		if (local->get_uuid() != remote.database()->get_uuid()) {
			co_return nullptr;
		}
		co_return std::make_unique<Shard>(std::move(local), on);
	} catch (...) {
		rethrow_translated();
	}
}


}  // namespace engine::xapian
