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


DocTerm&
StrataDocument::term_of(const Field& field, std::string_view term)
{
	auto it = fields_.find(field.name);
	if (it == fields_.end()) {
		it = fields_.emplace(field.name, DocField{}).first;
	}
	it->second.kind = std::max(it->second.kind, field.kind);
	auto& terms = it->second.terms;
	auto found = terms.find(term);
	if (found == terms.end()) {
		found = terms.emplace(std::string(term), DocTerm{}).first;
	}
	return found->second;
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
	entry.positions.push_back(position);
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


void
StrataDocument::normalise()
{
	for (auto& field : fields_) {
		for (auto& entry : field.second.terms) {
			auto& positions = entry.second.positions;
			if (!std::is_sorted(positions.begin(), positions.end()) || std::adjacent_find(positions.begin(), positions.end()) != positions.end()) {
				std::sort(positions.begin(), positions.end());
				positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
			}
		}
	}
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
	for (const auto& [name, field] : fields_) {
		put(out, name);
		out.push_back(static_cast<char>(field.kind));
		put(out, field.terms.size());
		for (const auto& [term, info] : field.terms) {
			put(out, term);
			put(out, info.wdf);
			std::vector<termpos> positions = info.positions;
			std::sort(positions.begin(), positions.end());
			positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
			put(out, positions.size());
			termpos previous = 0;
			for (auto position : positions) {
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
		const auto added = result.fields_.emplace(std::move(name), DocField{static_cast<FieldKind>(kind), {}});
		if (!added.second) {
			malformed("field twice");
		}
		auto& terms = added.first->second.terms;
		for (auto n = in.count(3); n; --n) {
			auto term = std::string(in.bytes());
			const auto wdf = in.number();
			if (wdf > 0xffffffffULL) {
				malformed("wdf out of range");
			}
			const auto entry = terms.emplace(std::move(term), DocTerm{});
			if (!entry.second) {
				malformed("term twice");
			}
			auto& info = entry.first->second;
			info.wdf = static_cast<termcount>(wdf);
			std::uint64_t position = 0;
			for (auto p = in.count(); p; --p) {
				position += in.number();
				if (position > 0xffffffffULL) {
					malformed("position out of range");
				}
				info.positions.push_back(static_cast<termpos>(position));
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
	// Diagnostic only: the Xapian spelling, a field's name followed by the term.
	for (const auto& [name, field] : fields_) {
		for (const auto& [term, info] : field.terms) {
			visit(name + term, info.wdf, info.positions.size());
		}
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
