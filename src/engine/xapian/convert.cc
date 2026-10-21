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

Xapian::Document
to_xapian(const Document& doc)
{
	Xapian::Document result;
	for (const auto& [name, term] : doc.terms()) {
		if (term.positions.empty()) {
			if (term.wdf == 0) {
				result.add_boolean_term(name);
			} else {
				result.add_term(name, term.wdf);
			}
			continue;
		}
		auto wdf = term.wdf;  // the first position carries the frequency
		for (auto position : term.positions) {
			result.add_posting(name, position, wdf);
			wdf = 0;
		}
	}
	for (const auto& [slot, value] : doc.values()) {
		result.add_value(slot, value);
	}
	auto data = doc.get_data();
	if (!data.empty()) {
		result.set_data(std::move(data));
	}
	return result;
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
		return errc::invalid;
	} catch (...) {
		return errc::internal;
	}
}


Error
translate_current()
{
	const auto code = classify_current();
	try {
		throw;
	} catch (const Xapian::Error& error) {
		return Error(code, error.get_description());
	} catch (const std::exception& error) {
		return Error(code, error.what());
	} catch (...) {
		return Error(code, "unknown error");
	}
}


namespace {

class XapianDocView final : public DocView {
public:
	explicit XapianDocView(Xapian::Document doc) : doc_(std::move(doc)) { }

	docid did() const override {
		return doc_.get_docid();
	}

	std::string data() const override {
		return doc_.get_data();
	}

	std::string value(valueno slot) const override {
		return doc_.get_value(slot);
	}

	rev version() const override {
		auto serialised = doc_.get_value(DB_SLOT_VERSION);
		return serialised.empty() ? 0 : static_cast<rev>(sortable_unserialise(serialised));
	}

	void terms(std::string_view prefix, const TermSink& sink) const override {
		const auto end = doc_.termlist_end();
		for (auto it = doc_.termlist_begin(); it != end; ++it) {
			auto name = *it;
			if (name.compare(0, prefix.size(), prefix) == 0) {
				sink(name, it.get_wdf());
			}
		}
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


}  // namespace engine::xapian


// The stored form of an engine document is Xapian's own, which keeps old write-ahead log lines readable.
std::string
engine::Document::serialise() const
{
	return xapian::to_xapian(*this).serialise();
}


engine::Document
engine::Document::unserialise(std::string_view serialised)
{
	return xapian::from_xapian(Xapian::Document::unserialise(std::string(serialised)));
}
