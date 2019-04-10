/*
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

#include "length.h"                               // for serialise_length()
#include "msgpack.h"                              // for MsgPack
#include "string.hh"                              // for string::*
#include "utype.hh"                               // for toUType
#include "compressor_lz4.h"                       // for compress_lz4, decompress_lz4


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
			first = string::lower(ct_type_str.substr(type, type_end - type + 1));
		}
		if (subtype != std::string::npos) {
			second = string::lower(ct_type_str.substr(subtype, subtype_end - subtype + 1));
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


void
Locator::data(std::string_view new_data)
{
	size = new_data.size();
	switch (type) {
		case Type::compressed_inplace:
			if (size >= 128) {
				_raw_holder = compress_lz4(new_data);
				if (_raw_holder.size() < new_data.size()) {
					raw = _raw_holder;
					break;
				}
			}
			type = Type::inplace;
			[[fallthrough]];
		case Type::inplace:
			raw = new_data;
			break;
		case Type::compressed_stored:
			if (size >= 128) {
				_raw_holder = compress_lz4(new_data);
				if (_raw_holder.size() < new_data.size()) {
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
		case Type::compressed_inplace:
			if (size >= 128) {
				_raw_holder = compress_lz4(new_data);
				if (_raw_holder.size() < new_data.size()) {
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
		case Type::compressed_stored:
			if (size >= 128) {
				_raw_holder = compress_lz4(new_data);
				if (_raw_holder.size() < new_data.size()) {
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
		case Type::compressed_inplace:
		case Type::compressed_stored:
			if (_raw_decompressed.empty() && !raw.empty()) {
				_raw_decompressed = decompress_lz4(raw);
			}
			return _raw_decompressed;
	}
}


Locator
Locator::unserialise(std::string_view locator_str)
{
	const char *p = locator_str.data();
	const char *p_end = p + locator_str.size();
	auto length = unserialise_length(&p, p_end, true);
	Locator locator(ct_type_t(std::string_view(p, length)));
	p += length;
	locator.type = static_cast<Type>(*p++);
	switch (locator.type) {
		case Type::inplace:
		case Type::compressed_inplace:
			locator.raw = std::string_view(p, p_end - p);
			locator.size = p_end - p;
			break;
		case Type::stored:
		case Type::compressed_stored:
			locator.volume = unserialise_length(&p, p_end);
			locator.offset = unserialise_length(&p, p_end);
			locator.size = unserialise_length(&p, p_end);
			locator.raw = std::string_view(p, p_end - p);
			break;
		default:
			THROW(SerialisationError, "Bad encoded data locator: Unknown type");
	}
	return locator;
}


std::string
Locator::serialise() const
{
	if (size == 0) {
		return "";
	}
	std::string result;
	result.append(serialise_string(ct_type.to_string()));
	result.push_back(toUType(type));
	switch (type) {
		case Type::inplace:
		case Type::compressed_inplace:
			break;
		case Type::stored:
		case Type::compressed_stored:
			result.append(serialise_length(volume));
			result.append(serialise_length(offset));
			result.append(serialise_length(size));
			break;
		default:
			THROW(SerialisationError, "Bad data locator: Unknown type");
	}
	result.append(raw);
	result.insert(0, serialise_length(result.size()));
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
Data::feed(std::string&& new_serialised, std::string&& new_version)
{
	version = std::move(new_version);
	serialised = std::move(new_serialised);
	locators.clear();
	if (serialised.size() < 6) {
		return;
	}
	const char *p = serialised.data();
	const char *p_end = p + serialised.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) {
		return;
	}
	while (p < p_end) {
		try {
			auto length = unserialise_length(&p, p_end, true);
			if (!length) {
				break;
			}
			locators.emplace_back(Locator::unserialise(std::string_view(p, length)));
			p += length;
		} catch (const SerialisationError& exc) {
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
	std::vector<Locator> new_locators;

	// First disable current locators which are inside ops
	for (auto& op : ops) {
		for (auto& locator : locators) {
			if (locator.size && locator == op) {
				locator.size = 0;
			}
		}
		if (op.ct_type.empty() && op.size) {
			// and push empty op first (if any)
			new_locators.push_back(op);
		}
	}

	// Then push the remaining locators
	for (auto& locator : locators) {
		if (locator.size) {
			new_locators.push_back(locator);
		}
	}
	// and afterwards the passed ops (except empty which should go first)
	for (auto& op : ops) {
		if (!op.ct_type.empty() && op.size) {
			new_locators.push_back(op);
		}
	}

	// Now replace old locators and serialize
	locators = new_locators;

	serialised.clear();
	serialised.push_back(DATABASE_DATA_HEADER_MAGIC);
	for (auto& locator : locators) {
		serialised.append(locator.serialise());
	}
	serialised.push_back('\0');
	serialised.push_back(DATABASE_DATA_FOOTER_MAGIC);
}


Data::Data()
{
	feed(std::string(DATABASE_DATA_MAP), std::string());
}


Data::Data(std::string&& serialised)
{
	feed(std::move(serialised), std::string());
}


Data::Data(std::string&& serialised, std::string&& version)
{
	feed(std::move(serialised), std::move(version));
}


Data::Data(Data&& other)
{
	feed(std::move(other.serialised), std::move(other.version));
	pending = std::move(other.pending);
}


Data::Data(const Data& other)
{
	auto new_serialised = other.serialised;
	auto new_version = other.version;
	feed(std::move(new_serialised), std::move(new_version));
	flush(other.pending);
}


Data&
Data::operator=(Data&& other)
{
	feed(std::move(other.serialised), std::move(other.version));
	pending = std::move(other.pending);
	return *this;
}


Data&
Data::operator=(const Data& other)
{
	auto new_serialised = other.serialised;
	auto new_version = other.version;
	feed(std::move(new_serialised), std::move(new_version));
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
Data::serialise() const
{
	if (serialised == DATABASE_DATA_EMPTY || serialised == DATABASE_DATA_MAP || serialised == DATABASE_DATA_UNDEFINED) {
		static const std::string empty;
		return empty;
	}
	return serialised;
}


bool
Data::operator==(const Data& other) const noexcept
{
	return serialise() == other.serialise();
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
	return main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack::MAP();
}


void
Data::set_obj(const MsgPack& object)
{
	update("", object.serialise());
}


std::pair<const Locator*, const Accept*>
Data::get_accepted(const accept_set_t& accept_set, const ct_type_t& mime_type) const
{
	const Locator* accepted = nullptr;
	const Accept* accepted_by = nullptr;
	double accepted_priority = -1.0;
	for (auto& locator : *this) {
		std::vector<ct_type_t> ct_types;
		if (locator.ct_type.empty()) {
			ct_types = msgpack_serializers;
		} else {
			ct_types.push_back(locator.ct_type);
		}
		for (auto& ct_type : ct_types) {
			for (auto& accept : accept_set) {
				if (
					(accept.ct_type.first == "*" && accept.ct_type.second == "*") ||
					(accept.ct_type.first == "*" && accept.ct_type.second == ct_type.second) ||
					(accept.ct_type.first == ct_type.first && accept.ct_type.second == "*") ||
					(accept.ct_type == ct_type)
				) {
					if (
						!mime_type.empty() &&
						ct_type.first == mime_type.first &&
						ct_type.second == mime_type.second
					) {
						accepted = &locator;
						accepted_by = &accept;
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
	return std::make_pair(accepted, accepted_by);
}
