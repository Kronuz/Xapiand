/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "data.h"

#include <algorithm>                              // for std::none_of
#include <atomic>                                 // for std::atomic

#include "exception_engine.h"                     // for SerializationError
#include "length.h"                               // for serialize_length()
#include "msgpack.h"                              // for MsgPack
#include "xapiand_strings.hh"                             // for string::*
#include "utype.hh"                               // for toUType
#include "compressor_lz4.h"                       // for decompress_lz4 (legacy docdata)
#include "compressor_zstd.h"                      // for compress_zstd, decompress_zstd


const ct_type_t no_type{};
const ct_type_t any_type(ANY_CONTENT_TYPE);
const ct_type_t html_type(HTML_CONTENT_TYPE);
const ct_type_t text_type(TEXT_CONTENT_TYPE);
const ct_type_t json_type(JSON_CONTENT_TYPE);
const ct_type_t x_json_type(X_JSON_CONTENT_TYPE);
const ct_type_t yaml_type(YAML_CONTENT_TYPE);
const ct_type_t x_yaml_type(X_YAML_CONTENT_TYPE);
const ct_type_t ndjson_type(NDJSON_CONTENT_TYPE);
const ct_type_t x_ndjson_type(X_NDJSON_CONTENT_TYPE);
const ct_type_t msgpack_type(MSGPACK_CONTENT_TYPE);
const ct_type_t x_msgpack_type(X_MSGPACK_CONTENT_TYPE);
const std::vector<const ct_type_t*> msgpack_serializers({
	&json_type, &x_json_type,
	&yaml_type, &x_yaml_type,
	&msgpack_type, &x_msgpack_type,
});


ct_type_t::ct_type_t(const std::string& first, const std::string& second) :
	first(first),
	second(second)
{
}


ct_type_t::ct_type_t(std::string_view ct_type_str)
{
	const auto dash = ct_type_str.find('/');
	if (dash != std::string::npos) {
		auto type = ct_type_str.find_first_not_of(" \t");
		auto type_end = ct_type_str.find_last_not_of(" \t/", dash);
		auto subtype = ct_type_str.find_first_not_of(" \t/", dash);
		auto subtype_end = ct_type_str.find_last_not_of(" \t;", ct_type_str.find(';', dash));
		if (type != std::string::npos) {
			first = strings::lower(ct_type_str.substr(type, type_end - type + 1));
		}
		if (subtype != std::string::npos) {
			second = strings::lower(ct_type_str.substr(subtype, subtype_end - subtype + 1));
		}
	}
}


ct_type_t::ct_type_t(const char* ct_type_str) :
	ct_type_t(std::string_view(ct_type_str))
{
}


bool
ct_type_t::operator==(const ct_type_t& other) const noexcept
{
	return first == other.first && second == other.second;
}


bool
ct_type_t::operator!=(const ct_type_t& other) const noexcept
{
	return !operator==(other);
}


bool
ct_type_t::operator<(const ct_type_t& other) const noexcept
{
	return first != other.first ? first < other.first : second < other.second;
}


void
ct_type_t::clear() noexcept
{
	first.clear();
	second.clear();
}


bool
ct_type_t::empty() const noexcept
{
	return first.empty() && second.empty();
}


std::string
ct_type_t::to_string() const
{
	return empty() ? "" : first + "/" + second;
}


Accept::Accept(int position, double priority, ct_type_t ct_type, int indent)
	: position(position), priority(priority), ct_type(ct_type), indent(indent)
{
}


namespace {

std::atomic<std::uint64_t> record_compressions{0};

std::string
compress_record(std::string_view data)
{
	record_compressions.fetch_add(1, std::memory_order_relaxed);
	return compress_zstd(data);
}

}  // namespace


std::uint64_t
Locator::compressions() noexcept
{
	return record_compressions.load(std::memory_order_relaxed);
}


Locator::Locator(const Locator& other) :
	_raw_holder(other._raw_holder),
	_raw_decompressed(other._raw_decompressed),
	type(other.type),
	ct_type(other.ct_type),
	raw(other.raw.data() == other._raw_holder.data() ? std::string_view(_raw_holder) : other.raw),
	volume(other.volume),
	offset(other.offset),
	size(other.size) { }


Locator::Locator(Locator&& other) noexcept :
	type(other.type),
	ct_type(std::move(other.ct_type)),
	raw(other.raw),
	volume(other.volume),
	offset(other.offset),
	size(other.size)
{
	const bool held = other.raw.data() == other._raw_holder.data();
	_raw_holder = std::move(other._raw_holder);
	_raw_decompressed = std::move(other._raw_decompressed);
	if (held) {
		raw = _raw_holder;   // a short holder's bytes live inside the object, not behind a pointer
	}
}


Locator&
Locator::operator=(const Locator& other)
{
	if (this != &other) {
		*this = Locator(other);
	}
	return *this;
}


Locator&
Locator::operator=(Locator&& other) noexcept
{
	if (this != &other) {
		const bool held = other.raw.data() == other._raw_holder.data();
		_raw_holder = std::move(other._raw_holder);
		_raw_decompressed = std::move(other._raw_decompressed);
		type = other.type;
		ct_type = std::move(other.ct_type);
		raw = held ? std::string_view(_raw_holder) : other.raw;
		volume = other.volume;
		offset = other.offset;
		size = other.size;
	}
	return *this;
}


void
Locator::data(std::string_view new_data)
{
	size = new_data.size();
	switch (type) {
		case Type::lz4_compressed_inplace:
		case Type::zstd_compressed_inplace:
			if (size >= 128) {
				_raw_holder = compress_record(new_data);
				if (_raw_holder.size() < new_data.size()) {
					type = Type::zstd_compressed_inplace;
					raw = _raw_holder;
					break;
				}
			}
			type = Type::inplace;
			[[fallthrough]];
		case Type::inplace:
			raw = new_data;
			break;
		case Type::lz4_compressed_stored:
		case Type::zstd_compressed_stored:
			if (size >= 128) {
				_raw_holder = compress_record(new_data);
				if (_raw_holder.size() < new_data.size()) {
					type = Type::zstd_compressed_stored;
					raw = _raw_holder;
					break;
				}
			}
			type = Type::stored;
			[[fallthrough]];
		case Type::stored:
			raw = new_data;
			break;
	}
}


void
Locator::data(std::string&& new_data)
{
	size = new_data.size();
	switch (type) {
		case Type::lz4_compressed_inplace:
		case Type::zstd_compressed_inplace:
			if (size >= 128) {
				_raw_holder = compress_record(new_data);
				if (_raw_holder.size() < new_data.size()) {
					type = Type::zstd_compressed_inplace;
					raw = _raw_holder;
					break;
				}
			}
			type = Type::inplace;
			[[fallthrough]];
		case Type::inplace:
			_raw_holder = std::move(new_data);
			raw = _raw_holder;
			break;
		case Type::lz4_compressed_stored:
		case Type::zstd_compressed_stored:
			if (size >= 128) {
				_raw_holder = compress_record(new_data);
				if (_raw_holder.size() < new_data.size()) {
					type = Type::zstd_compressed_stored;
					raw = _raw_holder;
					break;
				}
			}
			type = Type::stored;
			[[fallthrough]];
		case Type::stored:
			_raw_holder = std::move(new_data);
			raw = _raw_holder;
			break;
	}
}


std::string_view
Locator::data() const
{
	if (size == 0) {
		return "";
	}
	switch (type) {
		default:
		case Type::inplace:
		case Type::stored:
			return raw;
		case Type::lz4_compressed_inplace:
		case Type::lz4_compressed_stored:
			if (_raw_decompressed.empty() && !raw.empty()) {
				_raw_decompressed = decompress_lz4(raw);
			}
			return _raw_decompressed;
		case Type::zstd_compressed_inplace:
		case Type::zstd_compressed_stored:
			if (_raw_decompressed.empty() && !raw.empty()) {
				_raw_decompressed = decompress_zstd(raw);
			}
			return _raw_decompressed;
	}
}


Locator
Locator::unserialize(std::string_view locator_str)
{
	const char *p = locator_str.data();
	const char *p_end = p + locator_str.size();
	auto length = unserialize_length_and_check(&p, p_end);
	Locator locator(ct_type_t(std::string_view(p, length)));
	p += length;
	locator.type = static_cast<Type>(*p++);
	switch (locator.type) {
		case Type::inplace:
		case Type::lz4_compressed_inplace:
		case Type::zstd_compressed_inplace:
			locator.raw = std::string_view(p, p_end - p);
			locator.size = p_end - p;
			break;
		case Type::stored:
		case Type::lz4_compressed_stored:
		case Type::zstd_compressed_stored:
			locator.volume = unserialize_length(&p, p_end);
			locator.offset = unserialize_length(&p, p_end);
			locator.size = unserialize_length(&p, p_end);
			locator.raw = std::string_view(p, p_end - p);
			break;
		default:
			THROW(SerializationError, "Bad encoded data locator: Unknown type");
	}
	return locator;
}


std::string
Locator::serialize() const
{
	if (size == 0) {
		return "";
	}
	std::string result;
	result.append(serialize_string(ct_type.to_string()));
	result.push_back(toUType(type));
	switch (type) {
		case Type::inplace:
		case Type::lz4_compressed_inplace:
		case Type::zstd_compressed_inplace:
			break;
		case Type::stored:
		case Type::lz4_compressed_stored:
		case Type::zstd_compressed_stored:
			result.append(serialize_length(volume));
			result.append(serialize_length(offset));
			result.append(serialize_length(size));
			break;
		default:
			THROW(SerializationError, "Bad data locator: Unknown type");
	}
	result.append(raw);
	result.insert(0, serialize_length(result.size()));
	return result;
}


bool
Locator::operator==(const Locator& other) const noexcept
{
	return ct_type == other.ct_type;
}


bool
Locator::operator!=(const Locator& other) const noexcept
{
	return !operator==(other);
}


bool
Locator::operator<(const Locator& other) const noexcept
{
	return ct_type < other.ct_type;
}


void
Data::feed(std::string&& new_serialized, std::string&& new_version)
{
	version = std::move(new_version);
	serialized = std::move(new_serialized);
	parse();
}


void
Data::parse()
{
	locators.clear();
	if (serialized.size() < 6) {
		return;
	}
	const char *p = serialized.data();
	const char *p_end = p + serialized.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) {
		return;
	}
	while (p < p_end) {
		try {
			auto length = unserialize_length_and_check(&p, p_end);
			if (!length) {
				break;
			}
			locators.emplace_back(Locator::unserialize(std::string_view(p, length)));
			p += length;
		} catch (const SerializationError& exc) {
			locators.clear();
			return;
		}
	}
	if (p > p_end) {
		locators.clear();
		return;
	}
	if (*p++ != DATABASE_DATA_FOOTER_MAGIC) {
		locators.clear();
		return;
	}
	if (p != p_end) {
		locators.clear();
		return;
	}
}


void
Data::flush(const std::vector<Locator>& ops)
{
	// The kept locators in order: an op with no type (the main object) first, then the current locators no op
	// replaces, then the other ops.
	std::vector<const Locator*> kept;
	for (const auto& op : ops) {
		if (op.ct_type.empty() && op.size) {
			kept.push_back(&op);
		}
	}
	for (const auto& locator : locators) {
		if (locator.size && std::none_of(ops.begin(), ops.end(), [&](const Locator& op) { return locator == op; })) {
			kept.push_back(&locator);
		}
	}
	for (const auto& op : ops) {
		if (!op.ct_type.empty() && op.size) {
			kept.push_back(&op);
		}
	}

	// The new bytes are built apart from the ones the locators view, then parsed: every locator views them.
	std::string bytes;
	bytes.push_back(DATABASE_DATA_HEADER_MAGIC);
	for (const auto* locator : kept) {
		bytes.append(locator->serialize());
	}
	bytes.push_back('\0');
	bytes.push_back(DATABASE_DATA_FOOTER_MAGIC);
	serialized = std::move(bytes);
	parse();
}


Data::Data()
{
	feed(std::string(DATABASE_DATA_MAP), std::string());
}


Data::Data(std::string&& serialized)
{
	feed(std::move(serialized), std::string());
}


Data::Data(std::string&& serialized, std::string&& version)
{
	feed(std::move(serialized), std::move(version));
}


Data::Data(Data&& other)
{
	feed(std::move(other.serialized), std::move(other.version));
	pending = std::move(other.pending);
}


Data::Data(const Data& other)
{
	auto new_serialized = other.serialized;
	auto new_version = other.version;
	feed(std::move(new_serialized), std::move(new_version));
	flush(other.pending);
}


Data&
Data::operator=(Data&& other)
{
	feed(std::move(other.serialized), std::move(other.version));
	pending = std::move(other.pending);
	return *this;
}


Data&
Data::operator=(const Data& other)
{
	auto new_serialized = other.serialized;
	auto new_version = other.version;
	feed(std::move(new_serialized), std::move(new_version));
	flush(other.pending);
	return *this;
}


void
Data::flush()
{
	flush(pending);
	pending.clear();
}


const std::string&
Data::serialize() const
{
	if (serialized == DATABASE_DATA_EMPTY || serialized == DATABASE_DATA_MAP || serialized == DATABASE_DATA_UNDEFINED) {
		static const std::string empty;
		return empty;
	}
	return serialized;
}


bool
Data::operator==(const Data& other) const noexcept
{
	return serialize() == other.serialize();
}


const Locator&
Data::operator[](size_t pos) const
{
	return locators.operator[](pos);
}


bool
Data::empty() const
{
	return locators.empty();
}


size_t
Data::size() const
{
	return locators.size();
}


std::vector<Locator>::iterator
Data::begin()
{
	return locators.begin();
}


std::vector<Locator>::iterator
Data::end()
{
	return locators.end();
}


std::vector<Locator>::const_iterator
Data::begin() const
{
	return locators.cbegin();
}


std::vector<Locator>::const_iterator
Data::end() const
{
	return locators.cend();
}


const Locator*
Data::get(const ct_type_t& ct_type) const
{
	for (const auto& locator : locators) {
		if (locator.ct_type == ct_type) {
			return &locator;
		}
	}
	return nullptr;
}

MsgPack
Data::get_obj() const
{
	auto main_locator = get("");
	return main_locator != nullptr ? MsgPack::unserialize(main_locator->data()) : MsgPack::MAP();
}


void
Data::set_obj(const MsgPack& object, bool compress)
{
	auto& locator = pending.emplace_back("");
	if (!compress) {
		locator.type = Locator::Type::inplace;
	}
	locator.data(object.serialize());
}


std::pair<const Locator*, const Accept*>
Data::get_accepted(const accept_set_t& accept_set, const ct_type_t& mime_type) const
{
	L_CALL("Data::get_accepted({{{}}}, {})", [&]() {
		std::vector<std::string> values;
		for (auto& accept : accept_set) {
			values.push_back(repr(accept.ct_type.to_string()));
		}
		return strings::join(values, ", ");
	}, repr(mime_type.to_string()));

	const Locator* accepted = nullptr;
	const Accept* accepted_by = nullptr;
	double accepted_priority = -1.0;
	for (auto& locator : *this) {
		std::vector<const ct_type_t*> ct_types;
		if (locator.ct_type.empty()) {
			ct_types = msgpack_serializers;
		} else {
			ct_types.push_back(&locator.ct_type);
		}
		for (auto ct_type : ct_types) {
			for (auto& accept : accept_set) {
				if (
					(accept.ct_type.first == "*" && accept.ct_type.second == "*") ||
					(accept.ct_type.first == "*" && accept.ct_type.second == ct_type->second) ||
					(accept.ct_type.first == ct_type->first && accept.ct_type.second == "*") ||
					(accept.ct_type == *ct_type)
				) {
					if (
						!mime_type.empty() &&
						ct_type->first == mime_type.first &&
						ct_type->second == mime_type.second
					) {
						accepted = &locator;
						accepted_by = &accept;
						L_DEBUG("{} accepted (from mime-type)", repr(accepted_by->ct_type.to_string()));
						return std::make_pair(accepted, accepted_by);
					}
					double priority = accept.priority;
					if (priority >= accepted_priority) {
						accepted_priority = priority;
						accepted = &locator;
						accepted_by = &accept;
					}
				}
			}
		}
	}
	L_DEBUG("{} accepted", repr(accepted_by->ct_type.to_string()));
	return std::make_pair(accepted, accepted_by);
}
