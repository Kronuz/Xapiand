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

#include "engine/strata/document.h"

#include <algorithm>               // for std::sort, std::unique
#include <cstdint>                 // for std::uint8_t, std::uint64_t

#include "engine/error.h"          // for Error, errc


namespace engine::strata {

namespace {

constexpr std::uint8_t serial_version = 1;

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


void
StrataDocument::add_term(std::string_view term, termcount wdf_inc)
{
	auto it = terms_.find(term);
	if (it == terms_.end()) {
		it = terms_.emplace(std::string(term), DocTerm{}).first;
	}
	it->second.wdf += wdf_inc;
}


void
StrataDocument::add_boolean_term(std::string_view term)
{
	add_term(term, 0);
}


void
StrataDocument::set_key(std::string_view term)
{
	key_.assign(term);
	add_term(term, 0);
}


void
StrataDocument::add_posting(std::string_view term, termpos position, termcount wdf_inc)
{
	add_term(term, wdf_inc);
	terms_.find(term)->second.positions.push_back(position);
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
	for (auto& entry : terms_) {
		auto& positions = entry.second.positions;
		if (!std::is_sorted(positions.begin(), positions.end()) || std::adjacent_find(positions.begin(), positions.end()) != positions.end()) {
			std::sort(positions.begin(), positions.end());
			positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
		}
	}
}


// version, key, data, values (slot, bytes), terms (term, wdf, positions as increasing deltas).
std::string
StrataDocument::serialise() const
{
	std::string out;
	out.push_back(static_cast<char>(serial_version));
	put(out, key_);
	put(out, data_);
	put(out, values_.size());
	for (const auto& [slot, value] : values_) {
		put(out, slot);
		put(out, value);
	}
	put(out, terms_.size());
	for (const auto& [term, info] : terms_) {
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
	return out;
}


void
StrataDocument::unserialise_from(std::string_view serialised)
{
	Cursor in(serialised);
	if (in.byte() != serial_version) {
		malformed("unknown version");
	}
	StrataDocument result;
	result.key_.assign(in.bytes());
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
	for (auto n = in.count(3); n; --n) {
		auto term = std::string(in.bytes());
		auto& info = result.terms_[std::move(term)];
		const auto wdf = in.number();
		if (wdf > 0xffffffffULL) {
			malformed("wdf out of range");
		}
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
	if (!in.done()) {
		malformed("trailing bytes");
	}
	if (!result.key_.empty() && !result.terms_.contains(result.key_)) {
		malformed("key is not a term");
	}
	*this = std::move(result);
}


#ifdef ENGINE_CENSUS
void
StrataDocument::census_terms(const std::function<void(std::string_view, termcount, std::size_t)>& visit) const
{
	for (const auto& [term, info] : terms_) {
		visit(term, info.wdf, info.positions.size());
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
