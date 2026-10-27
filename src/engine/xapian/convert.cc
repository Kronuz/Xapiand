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

#include "engine/xapian/convert.h"

#include "engine/xapian/errors.h"      // for the Failure types
#include "engine/census.h"         // for ENGINE_CENSUS_DOCVIEW

#include <atomic>                // for std::atomic
#include <type_traits>             // for std::is_same_v, std::is_unsigned_v

#include "database/flags.h"        // for DB_CREATE_OR_OPEN, DB_CREATE, ...
#include "database/utils.h"        // for DB_SLOT_VERSION
#include "sortable_serialise.h"    // for sortable_unserialise


// The seam's types are the Xapian ones.
static_assert(std::is_same_v<engine::docid, Xapian::docid>);
static_assert(std::is_same_v<engine::doccount, Xapian::doccount>);
static_assert(std::is_same_v<engine::valueno, Xapian::valueno>);
static_assert(std::is_same_v<engine::termcount, Xapian::termcount>);
static_assert(std::is_same_v<engine::termpos, Xapian::termpos>);
static_assert(sizeof(engine::rev) == sizeof(Xapian::rev) && std::is_unsigned_v<Xapian::rev>);
static_assert(engine::BAD_VALUENO == Xapian::BAD_VALUENO);
static_assert(engine::DB_CREATE_OR_OPEN == Xapian::DB_CREATE_OR_OPEN);
static_assert(engine::DB_CREATE_OR_OVERWRITE == Xapian::DB_CREATE_OR_OVERWRITE);
static_assert(engine::DB_CREATE == Xapian::DB_CREATE);
static_assert(engine::DB_OPEN == Xapian::DB_OPEN);
static_assert(engine::DB_ACTION_MASK == Xapian::DB_ACTION_MASK_);


namespace engine::xapian {

void
XapianDocument::add_term(std::string_view term, termcount wdf_inc)
{
	doc_.add_term(term, wdf_inc);
}


void
XapianDocument::add_boolean_term(std::string_view term)
{
	doc_.add_boolean_term(term);
}


void
XapianDocument::add_posting(std::string_view term, termpos position, termcount wdf_inc)
{
	doc_.add_posting(term, position, wdf_inc);
}


void
XapianDocument::add_value(valueno slot, std::string value)
{
	doc_.add_value(slot, value);  // an empty value clears the slot
}


std::string
XapianDocument::get_value(valueno slot) const
{
	return doc_.get_value(slot);
}


void
XapianDocument::set_data(std::string data)
{
	doc_.set_data(std::move(data));
}


std::string
XapianDocument::get_data() const
{
	return doc_.get_data();
}


std::string
XapianDocument::serialise() const
{
	return doc_.serialise();
}


#ifdef ENGINE_CENSUS
void
XapianDocument::census_terms(const std::function<void(std::string_view, termcount, std::size_t)>& visit) const
{
	for (auto it = doc_.termlist_begin(); it != doc_.termlist_end(); ++it) {
		visit(*it, it.get_wdf(), it.positionlist_count());
	}
}


std::size_t
XapianDocument::census_values() const
{
	return doc_.values_count();
}
#endif


const Xapian::Document&
xapian_document(const Document& doc)
{
	return static_cast<const XapianDocument&>(doc.native()).xapian();
}


Document
from_xapian(const Xapian::Document& doc)
{
	Document result;
	const auto terms_end = doc.termlist_end();
	for (auto it = doc.termlist_begin(); it != terms_end; ++it) {
		auto name = *it;
		result.add_term(name, it.get_wdf());
		const auto positions_end = it.positionlist_end();
		for (auto pit = it.positionlist_begin(); pit != positions_end; ++pit) {
			result.add_posting(name, *pit, 0);
		}
	}
	const auto values_end = doc.values_end();
	for (auto it = doc.values_begin(); it != values_end; ++it) {
		result.add_value(it.get_valueno(), *it);
	}
	result.set_data(doc.get_data());
	return result;
}


WriteResult
from_xapian(const Xapian::DocumentInfo& info)
{
	return WriteResult{info.did, info.version, info.term};
}


Xapian::DocumentInfo
to_xapian(const WriteResult& result)
{
	Xapian::DocumentInfo info;
	info.did = result.did;
	info.version = result.version;
	info.term = result.term;
	return info;
}


std::string
version()
{
	return std::to_string(Xapian::major_version()) + "." + std::to_string(Xapian::minor_version()) + "." + std::to_string(Xapian::revision());
}


errc
classify_current() noexcept
{
	// Catch clauses follow Xapian's hierarchy, so the most derived come first: a
	// DatabaseNotFoundError is also a DatabaseOpeningError.
	try {
		throw;
	} catch (const Xapian::DatabaseNotFoundError&) {
		return errc::not_found;
	} catch (const Xapian::DatabaseNotAvailableError&) {
		return errc::not_available;
	} catch (const Xapian::DatabaseModifiedError&) {
		return errc::reopen;
	} catch (const Xapian::DatabaseCorruptError&) {
		return errc::corrupt;
	} catch (const Xapian::DatabaseOpeningError&) {
		return errc::opening;
	} catch (const Xapian::DatabaseClosedError&) {
		return errc::closed;
	} catch (const Xapian::NetworkTimeoutError&) {
		return errc::timeout;
	} catch (const Xapian::NetworkError&) {
		return errc::network;
	} catch (const Xapian::DocNotFoundError&) {
		return errc::doc_not_found;
	} catch (const Xapian::DocVersionConflictError&) {
		return errc::version_conflict;
	} catch (const Xapian::InvalidArgumentError&) {
		return errc::invalid;
	} catch (const Xapian::InvalidOperationError&) {
		return errc::invalid;
	} catch (const Xapian::QueryParserError&) {
		return errc::syntax;
	} catch (const Xapian::DatabaseError&) {
		return errc::database;
	} catch (...) {
		return errc::internal;
	}
}


void
rethrow_translated()
{
	try {
		throw;
	} catch (const Error&) {
		throw;
	} catch (const std::exception&) {
		throw;  // Xapiand's own exceptions, some of which also derive from a Xapian type
	} catch (const Xapian::DatabaseNotFoundError& e) {
		throw NotFound(e);
	} catch (const Xapian::DatabaseNotAvailableError& e) {
		throw NotAvailable(e);
	} catch (const Xapian::DatabaseModifiedError& e) {
		throw Reopen(e);
	} catch (const Xapian::DatabaseCorruptError& e) {
		throw Corrupt(e);
	} catch (const Xapian::DatabaseOpeningError& e) {
		throw Opening(e);
	} catch (const Xapian::DatabaseClosedError& e) {
		throw Closed(e);
	} catch (const Xapian::NetworkTimeoutError& e) {
		throw Timeout(e);
	} catch (const Xapian::NetworkError& e) {
		throw Network(e);
	} catch (const Xapian::DocNotFoundError& e) {
		throw DocNotFound(e);
	} catch (const Xapian::DocVersionConflictError& e) {
		throw VersionConflict(e);
	} catch (const Xapian::InvalidArgumentError& e) {
		throw InvalidArgument(e);
	} catch (const Xapian::InvalidOperationError& e) {
		throw InvalidOperation(e);
	} catch (const Xapian::QueryParserError& e) {
		throw QueryParser(e);
	} catch (const Xapian::DatabaseError& e) {
		throw DatabaseFailure(e);
	} catch (const Xapian::Error& e) {
		throw Other(e);
	}
}


namespace {

// Runs `f`, turning whatever Xapian throws into an engine::Error.
template <typename F>
auto
guarded(F&& f) -> decltype(f())
{
	try {
		return f();
	} catch (...) {
		rethrow_translated();
	}
}


class XapianDocView final : public DocView {
public:
	explicit XapianDocView(Xapian::Document doc) : doc_(std::move(doc)) {
		live.fetch_add(1, std::memory_order_relaxed);
	}

	~XapianDocView() override {
		live.fetch_sub(1, std::memory_order_relaxed);
	}

	XapianDocView(const XapianDocView&) = delete;
	XapianDocView& operator=(const XapianDocView&) = delete;

	static inline std::atomic<std::size_t> live{0};

	docid did() const override {
		ENGINE_CENSUS_DOCVIEW("did");
		return guarded([&] { return doc_.get_docid(); });
	}

	std::string data() const override {
		ENGINE_CENSUS_DOCVIEW("data");
		return guarded([&] { return doc_.get_data(); });
	}

	std::string value(valueno slot) const override {
		ENGINE_CENSUS_DOCVIEW("value");
		return guarded([&] { return doc_.get_value(slot); });
	}

	rev version() const override {
		ENGINE_CENSUS_DOCVIEW("version");
		return guarded([&] {
			auto serialised = doc_.get_value(DB_SLOT_VERSION);
			return serialised.empty() ? 0 : static_cast<rev>(sortable_unserialise(serialised));
		});
	}

	void terms(std::string_view prefix, const TermSink& sink) const override {
		ENGINE_CENSUS_DOCVIEW("terms");
		guarded([&] {
			// A termlist is sorted, so the terms with a prefix are contiguous.
			const auto end = doc_.termlist_end();
			auto it = doc_.termlist_begin();
			it.skip_to(std::string(prefix));
			for (; it != end; ++it) {
				auto name = *it;
				if (name.compare(0, prefix.size(), prefix) != 0) {
					break;
				}
				sink(name, it.get_wdf());
			}
		});
	}

	void values(const ValueSink& sink) const override {
		ENGINE_CENSUS_DOCVIEW("values");
		guarded([&] {
			const auto end = doc_.values_end();
			for (auto it = doc_.values_begin(); it != end; ++it) {
				sink(it.get_valueno(), *it);
			}
		});
	}

	void term_details(const TermDetailSink& sink) const override {
		ENGINE_CENSUS_DOCVIEW("term_details");
		guarded([&] {
			const auto end = doc_.termlist_end();
			for (auto it = doc_.termlist_begin(); it != end; ++it) {
				TermDetail detail;
				auto name = *it;
				detail.term = name;
				detail.wdf = it.get_wdf();
				try {
					detail.termfreq = it.get_termfreq();
				} catch (const Xapian::InvalidOperationError&) { }  // not tied to a database
				const auto pend = it.positionlist_end();
				detail.positions.reserve(it.positionlist_count());
				for (auto pit = it.positionlist_begin(); pit != pend; ++pit) {
					detail.positions.push_back(*pit);
				}
				sink(detail);
			}
		});
	}

	std::string serialise() const override {
		ENGINE_CENSUS_DOCVIEW("serialise");
		return guarded([&] { return doc_.serialise(); });
	}

private:
	Xapian::Document doc_;
};

}  // namespace


std::unique_ptr<DocView>
make_view(Xapian::Document doc)
{
	return std::make_unique<XapianDocView>(std::move(doc));
}


std::size_t
live_views() noexcept
{
	return XapianDocView::live.load(std::memory_order_relaxed);
}


}  // namespace engine::xapian


engine::Document::Document() : native_(std::make_unique<xapian::XapianDocument>()) { }


// The stored form of an engine document is Xapian's own, which keeps old write-ahead log lines readable.
engine::Document
engine::Document::unserialise(std::string_view serialised)
{
	return xapian::from_xapian(Xapian::Document::unserialise(std::string(serialised)));
}
