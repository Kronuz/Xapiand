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

#include "engine/strata/document.h"
#include <algorithm>               // for std::sort, std::unique
#include <cstdint>                 // for std::uint8_t, std::uint64_t
#include <cstring>                 // for std::memcpy

#include "engine/error.h"          // for Error, errc


namespace engine::strata {

namespace {

// Formats 1 and 2 (a flat term, no fields) are named but no longer read.
constexpr std::uint8_t serial_version = 3;

void
put(std::string& out, std::uint64_t value)
{
	while (value >= 0x80) {
		out.push_back(static_cast<char>(value | 0x80));
		value >>= 7;
	}
	out.push_back(static_cast<char>(value));
}


void
put(std::string& out, std::string_view bytes)
{
	put(out, bytes.size());
	out.append(bytes);
}


[[noreturn]] void
malformed(const char* what)
{
	throw Error(errc::syntax, std::string("Bad serialised document: ") + what);
}


// A reader over serialised bytes that never reads past the end.
class Cursor {
public:
	explicit Cursor(std::string_view bytes) : bytes_(bytes) { }

	std::uint64_t number() {
		std::uint64_t value = 0;
		for (unsigned shift = 0; shift < 64; shift += 7) {
			if (bytes_.empty()) {
				malformed("truncated number");
			}
			const auto byte = static_cast<std::uint8_t>(bytes_.front());
			bytes_.remove_prefix(1);
			if (shift == 63 && byte > 1) {
				malformed("number out of range");
			}
			value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
			if (!(byte & 0x80)) {
				return value;
			}
		}
		malformed("number out of range");
	}

	// A count of entries that each take at least `each` more bytes: it cannot exceed what is left.
	std::size_t count(std::size_t each = 1) {
		const auto n = number();
		if (n > bytes_.size() / each) {
			malformed("count exceeds the input");
		}
		return static_cast<std::size_t>(n);
	}

	std::string_view bytes() {
		const auto n = number();
		if (n > bytes_.size()) {
			malformed("truncated string");
		}
		auto result = bytes_.substr(0, n);
		bytes_.remove_prefix(n);
		return result;
	}

	std::uint8_t byte() {
		if (bytes_.empty()) {
			malformed("truncated");
		}
		const auto result = static_cast<std::uint8_t>(bytes_.front());
		bytes_.remove_prefix(1);
		return result;
	}

	bool done() const noexcept { return bytes_.empty(); }

private:
	std::string_view bytes_;
};

}  // namespace


namespace {

// A short-input hash of a term's bytes, 8 at a time.
std::uint32_t
hash_of(std::string_view bytes) noexcept
{
	constexpr std::uint64_t mul = 0x9e3779b97f4a7c15ULL;
	std::uint64_t h = bytes.size() * mul;
	const char* p = bytes.data();
	std::size_t n = bytes.size();
	for (; n >= 8; n -= 8, p += 8) {
		std::uint64_t w;
		std::memcpy(&w, p, 8);
		h = (h ^ w) * mul;
		h ^= h >> 29;
	}
	if (n) {
		std::uint64_t w = 0;
		std::memcpy(&w, p, n);
		h = (h ^ w) * mul;
		h ^= h >> 29;
	}
	return static_cast<std::uint32_t>(h ^ (h >> 32));
}

// Up to this many terms a field is searched by its hashes in order, with no table.
constexpr std::size_t linear_terms = 8;

}  // namespace


namespace {

// The hash of a term under a field.
std::uint32_t
mix(std::uint32_t hash, std::size_t field) noexcept
{
	hash ^= static_cast<std::uint32_t>(field) * 0x9e3779b1u;
	return hash ^ (hash >> 15);
}

}  // namespace


void
StrataDocument::rebuild(std::size_t slots)
{
	table_.assign(slots, 0);
	const std::size_t mask = slots - 1;
	for (std::size_t i = 0; i < terms_.size(); ++i) {
		auto at = terms_[i].hash & mask;
		while (table_[at]) {
			at = (at + 1) & mask;
		}
		table_[at] = static_cast<std::uint32_t>(i + 1);
	}
}


const DocTerm*
StrataDocument::find_term(const DocField& field, std::string_view bytes) const noexcept
{
	const auto index = static_cast<std::size_t>(&field - fields_.data());
	const auto hash = mix(hash_of(bytes), index);
	if (table_.empty()) {
		for (const auto& t : terms_) {
			if (t.hash == hash && t.field == index && term_bytes(t) == bytes) {
				return &t;
			}
		}
		return nullptr;
	}
	const std::size_t mask = table_.size() - 1;
	for (auto at = hash & mask; table_[at]; at = (at + 1) & mask) {
		const auto& t = terms_[table_[at] - 1];
		if (t.hash == hash && t.field == index && term_bytes(t) == bytes) {
			return &t;
		}
	}
	return nullptr;
}


const DocTerm*
StrataDocument::find_term(std::string_view field_name, std::string_view bytes) const noexcept
{
	const auto* f = field(field_name);
	return f ? find_term(*f, bytes) : nullptr;
}


DocTerm&
StrataDocument::intern(std::size_t field, std::string_view bytes)
{
	const auto hash = mix(hash_of(bytes), field);
	if (table_.empty()) {
		for (auto& t : terms_) {
			if (t.hash == hash && t.field == field && term_bytes(t) == bytes) {
				return t;
			}
		}
	} else {
		const std::size_t mask = table_.size() - 1;
		for (auto at = hash & mask; table_[at]; at = (at + 1) & mask) {
			auto& t = terms_[table_[at] - 1];
			if (t.hash == hash && t.field == field && term_bytes(t) == bytes) {
				return t;
			}
		}
	}
	if (bytes_.size() + bytes.size() > 0xfffffff0u) {
		throw Error(errc::invalid, "The document's terms are too large");
	}
	if (terms_.empty()) {
		terms_.reserve(192);
		bytes_.reserve(1024);
	}
	auto& added = terms_.emplace_back();
	added.field = static_cast<std::uint32_t>(field);
	added.offset = static_cast<std::uint32_t>(bytes_.size());
	added.size = static_cast<std::uint32_t>(bytes.size());
	added.hash = hash;
	bytes_.append(bytes);
	++fields_[field].term_count;
	if (terms_.size() > linear_terms) {
		if (table_.size() < terms_.size() * 2) {
			rebuild(std::max<std::size_t>(table_.size() * 2, 64));
		} else {
			const std::size_t mask = table_.size() - 1;
			auto at = hash & mask;
			while (table_[at]) {
				at = (at + 1) & mask;
			}
			table_[at] = static_cast<std::uint32_t>(terms_.size());
		}
	}
	return added;
}


void
StrataDocument::group(Grouped& out) const
{
	out.starts.assign(fields_.size() + 1, 0);
	for (std::size_t f = 0; f < fields_.size(); ++f) {
		out.starts[f + 1] = out.starts[f] + fields_[f].term_count;
	}
	out.order.resize(terms_.size());
	std::vector<std::uint32_t> next(out.starts.begin(), out.starts.end() - 1);
	for (std::size_t i = 0; i < terms_.size(); ++i) {
		out.order[next[terms_[i].field]++] = static_cast<std::uint32_t>(i);
	}
}


void
StrataDocument::push_position(DocTerm& term, termpos position)
{
	if (nodes_.empty()) {
		nodes_.reserve(128);
	}
	const auto index = static_cast<std::uint32_t>(nodes_.size());
	nodes_.push_back({position, DocTerm::none});
	if (term.last == DocTerm::none) {
		term.first = index;
	} else {
		nodes_[term.last].next = index;
	}
	term.last = index;
	++term.count;
}


void
StrataDocument::positions_into(const DocTerm& term, std::vector<termpos>& out) const
{
	out.clear();
	out.reserve(term.count);
	bool increasing = true;
	for (auto at = term.first; at != DocTerm::none; at = nodes_[at].next) {
		const auto position = nodes_[at].position;
		increasing = increasing && (out.empty() || out.back() < position);
		out.push_back(position);
	}
	if (!increasing) {
		std::sort(out.begin(), out.end());
		out.erase(std::unique(out.begin(), out.end()), out.end());
	}
}


std::vector<termpos>
StrataDocument::positions_of(const DocTerm& term) const
{
	std::vector<termpos> out;
	positions_into(term, out);
	return out;
}


const DocField*
StrataDocument::field(std::string_view name) const noexcept
{
	for (const auto& f : fields_) {
		if (f.name == name) {
			return &f;
		}
	}
	return nullptr;
}


std::size_t
StrataDocument::field_of(const Field& field)
{
	constexpr std::uint32_t cached_ids = 1024;  // the host numbers a document's fields from 0
	const auto id = field.id;
	if (id < slot_.size() && slot_[id] && fields_[slot_[id] - 1].name == field.name) {
		return slot_[id] - 1;
	}
	std::size_t index = 0;
	while (index < fields_.size() && fields_[index].name != field.name) {
		++index;
	}
	if (index == fields_.size()) {
		if (fields_.empty()) {
			fields_.reserve(64);
		}
		fields_.emplace_back();
		fields_.back().name = field.name;
	}
	if (id < cached_ids) {
		if (id >= slot_.size()) {
			slot_.resize(id + 1, 0);
		}
		slot_[id] = static_cast<std::uint32_t>(index + 1);
	}
	return index;
}


DocTerm&
StrataDocument::term_of(const Field& field, std::string_view term)
{
	const auto index = field_of(field);
	auto& f = fields_[index];
	f.kind = std::max(f.kind, field.kind);
	return intern(index, term);
}


void
StrataDocument::add_term(const Field& field, std::string_view term, termcount wdf_inc)
{
	if (field.name == KEY_FIELD) {
		key_wdf_ += wdf_inc;
		return;
	}
	term_of(field, term).wdf += wdf_inc;
}


void
StrataDocument::add_boolean_term(const Field& field, std::string_view term)
{
	add_term(field, term, 0);
}


void
StrataDocument::set_key(std::string_view key)
{
	key_.assign(key);
}


void
StrataDocument::add_posting(const Field& field, std::string_view term, termpos position, termcount wdf_inc)
{
	if (field.name == KEY_FIELD) {
		key_wdf_ += wdf_inc;
		return;
	}
	auto& entry = term_of(field, term);
	entry.wdf += wdf_inc;
	push_position(entry, position);
}


void
StrataDocument::add_value(valueno slot, std::string value)
{
	if (value.empty()) {
		values_.erase(slot);
	} else {
		values_[slot] = std::move(value);
	}
}


std::string
StrataDocument::get_value(valueno slot) const
{
	auto it = values_.find(slot);
	return it == values_.end() ? std::string() : it->second;
}


void
StrataDocument::set_data(std::string data)
{
	data_ = std::move(data);
}


std::string
StrataDocument::get_data() const
{
	return data_;
}


// format, written version, key, key frequency, data, values (slot, bytes), fields (name, kind, terms (term, wdf,
// positions as increasing deltas)).
std::string
StrataDocument::serialise() const
{
	std::string out;
	out.push_back(static_cast<char>(serial_version));
	put(out, version_);
	put(out, key_);
	put(out, key_wdf_);
	put(out, data_);
	put(out, values_.size());
	for (const auto& [slot, value] : values_) {
		put(out, slot);
		put(out, value);
	}
	put(out, fields_.size());
	std::vector<termpos> scratch;
	Grouped grouped;
	group(grouped);
	for (std::size_t f = 0; f < fields_.size(); ++f) {
		const auto& field = fields_[f];
		put(out, field.name);
		out.push_back(static_cast<char>(field.kind));
		put(out, field.term_count);
		for (auto at = grouped.starts[f]; at < grouped.starts[f + 1]; ++at) {
			const auto& info = terms_[grouped.order[at]];
			put(out, term_bytes(info));
			put(out, info.wdf);
			positions_into(info, scratch);
			put(out, scratch.size());
			termpos previous = 0;
			for (auto position : scratch) {
				put(out, position - previous);
				previous = position;
			}
		}
	}
	return out;
}


void
StrataDocument::unserialise_from(std::string_view serialised)
{
	Cursor in(serialised);
	const auto format = in.byte();
	if (format == 1 || format == 2) {
		malformed("formats 1 and 2 predate fields and are no longer read");
	}
	if (format != serial_version) {
		malformed("unknown version");
	}
	StrataDocument result;
	result.version_ = in.number();
	result.replayed_ = result.version_ != 0;
	result.key_.assign(in.bytes());
	const auto key_wdf = in.number();
	if (key_wdf > 0xffffffffULL) {
		malformed("key frequency out of range");
	}
	result.key_wdf_ = static_cast<termcount>(key_wdf);
	result.data_.assign(in.bytes());
	for (auto n = in.count(2); n; --n) {
		const auto slot = in.number();
		if (slot > 0xffffffffULL) {
			malformed("value slot out of range");
		}
		auto value = std::string(in.bytes());
		if (value.empty()) {
			malformed("empty value");
		}
		result.values_[static_cast<valueno>(slot)] = std::move(value);
	}
	for (auto f = in.count(3); f; --f) {
		auto name = std::string(in.bytes());
		const auto kind = in.byte();
		if (name.empty() || name == KEY_FIELD) {
			malformed("field name");
		}
		if (kind > static_cast<std::uint8_t>(FieldKind::text)) {
			malformed("field kind");
		}
		if (result.field(name)) {
			malformed("field twice");
		}
		const auto field = result.fields_.size();
		auto& added_field = result.fields_.emplace_back();
		added_field.name = std::move(name);
		added_field.kind = static_cast<FieldKind>(kind);
		for (auto n = in.count(3); n; --n) {
			const auto term = in.bytes();
			const auto wdf = in.number();
			if (wdf > 0xffffffffULL) {
				malformed("wdf out of range");
			}
			const auto before = result.terms_.size();
			auto& info = result.intern(field, term);
			if (result.terms_.size() == before) {
				malformed("term twice");
			}
			info.wdf = static_cast<termcount>(wdf);
			std::uint64_t position = 0;
			for (auto p = in.count(); p; --p) {
				position += in.number();
				if (position > 0xffffffffULL) {
					malformed("position out of range");
				}
				result.push_position(info, static_cast<termpos>(position));
			}
		}
	}
	if (!in.done()) {
		malformed("trailing bytes");
	}
	*this = std::move(result);
}


#ifdef ENGINE_CENSUS
void
StrataDocument::census_terms(const std::function<void(std::string_view, termcount, std::size_t)>& visit) const
{
	// Diagnostic only: a field's name followed by the term.
	std::map<std::string, const DocTerm*> sorted;
	for (const auto& info : terms_) {
		sorted.emplace(fields_[info.field].name + std::string(term_bytes(info)), &info);
	}
	for (const auto& [spelled, info] : sorted) {
		visit(spelled, info->wdf, positions_of(*info).size());
	}
}


std::size_t
StrataDocument::census_values() const
{
	return values_.size();
}
#endif


std::unique_ptr<Document::Native>
new_native()
{
	return std::make_unique<StrataDocument>();
}


Document
unserialise_document(std::string_view serialised)
{
	auto native = std::make_unique<StrataDocument>();
	native->unserialise_from(serialised);
	return Document(std::move(native));
}


StrataDocument&
strata_document(Document& doc)
{
	return static_cast<StrataDocument&>(doc.native());
}


const StrataDocument&
strata_document(const Document& doc)
{
	return static_cast<const StrataDocument&>(doc.native());
}

}  // namespace engine::strata
