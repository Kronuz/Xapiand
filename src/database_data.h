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

#pragma once

#include <string>
#include "string_view.hh"
#include <vector>                  // for std::vector

#include "length.h"                // for serialise_length()
#include "msgpack.h"               // for MsgPack
#include "string.hh"               // for string::*
#include "utype.hh"                // for toUType
#include "compressor_lz4.h"        // for compress_lz4, decompress_lz4


constexpr int STORED_CONTENT_TYPE  = 0;
constexpr int STORED_BLOB          = 1;

constexpr char DATABASE_DATA_HEADER_MAGIC        = 0x11;
constexpr char DATABASE_DATA_FOOTER_MAGIC        = 0x15;

constexpr char DATABASE_DATA_DEFAULT[]  = { DATABASE_DATA_HEADER_MAGIC, 0x03, 0x00, 0x00, '\x80', 0x00, DATABASE_DATA_FOOTER_MAGIC };

constexpr std::string_view ANY_CONTENT_TYPE               = "*/*";
constexpr std::string_view HTML_CONTENT_TYPE              = "text/html";
constexpr std::string_view TEXT_CONTENT_TYPE              = "text/plain";
constexpr std::string_view JSON_CONTENT_TYPE              = "application/json";
constexpr std::string_view NDJSON_CONTENT_TYPE            = "application/ndjson";
constexpr std::string_view X_NDJSON_CONTENT_TYPE          = "application/x-ndjson";
constexpr std::string_view MSGPACK_CONTENT_TYPE           = "application/msgpack";
constexpr std::string_view X_MSGPACK_CONTENT_TYPE         = "application/x-msgpack";
constexpr std::string_view FORM_URLENCODED_CONTENT_TYPE   = "application/www-form-urlencoded";
constexpr std::string_view X_FORM_URLENCODED_CONTENT_TYPE = "application/x-www-form-urlencoded";


struct ct_type_t {
	std::string first;
	std::string second;

	ct_type_t() = default;

	ct_type_t(const std::string& first, const std::string& second) :
		first(first),
		second(second) { }

	ct_type_t(std::string_view ct_type_str) {
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

	ct_type_t(const char* ct_type_str) : ct_type_t(std::string_view(ct_type_str)) { }

	bool operator==(const ct_type_t& other) const noexcept {
		return first == other.first && second == other.second;
	}

	bool operator!=(const ct_type_t& other) const noexcept {
		return !operator==(other);
	}

	bool operator<(const ct_type_t& other) const noexcept {
		return first != other.first ? first < other.first : second < other.second;
	}

	void clear() noexcept {
		first.clear();
		second.clear();
	}

	bool empty() const noexcept {
		return first.empty() && second.empty();
	}

	std::string to_string() const {
		return empty() ? "" : first + "/" + second;
	}
};


template <typename T>
struct accept_preference_comp {
	constexpr bool operator()(const T& l, const T& r) const noexcept {
		if (l.priority == r.priority) {
			return l.position < r.position;
		}
		return l.priority > r.priority;
	}
};


struct Accept {
	int position;
	double priority;

	ct_type_t ct_type;
	int indent;

	Accept(int position, double priority, ct_type_t ct_type, int indent)
		: position(position), priority(priority), ct_type(ct_type), indent(indent) { }
};
using accept_set_t = std::set<Accept, accept_preference_comp<Accept>>;


static const ct_type_t no_type{};
static const ct_type_t any_type(ANY_CONTENT_TYPE);
static const ct_type_t html_type(HTML_CONTENT_TYPE);
static const ct_type_t text_type(TEXT_CONTENT_TYPE);
static const ct_type_t json_type(JSON_CONTENT_TYPE);
static const ct_type_t ndjson_type(NDJSON_CONTENT_TYPE);
static const ct_type_t x_ndjson_type(X_NDJSON_CONTENT_TYPE);
static const ct_type_t msgpack_type(MSGPACK_CONTENT_TYPE);
static const ct_type_t x_msgpack_type(X_MSGPACK_CONTENT_TYPE);
static const std::vector<ct_type_t> msgpack_serializers({ json_type, msgpack_type, x_msgpack_type });


class Locator {
	std::string _raw_holder;
	mutable std::string _raw_decompressed;

public:
	enum class Type : uint8_t {
		inplace,
		stored,
		compressed_inplace,
		compressed_stored,
	};

	Type type;

	ct_type_t ct_type;

	std::string_view raw;

	ssize_t volume;
	size_t offset;
	size_t size;

	template <typename C, typename = std::enable_if_t<not std::is_same<Locator, std::decay_t<C>>::value>>
	Locator(C&& ct_type) :
		type(Type::compressed_inplace),
		ct_type(std::forward<C>(ct_type)),
		volume(-1),
		offset(0),
		size(0) { }

	template <typename C, typename = std::enable_if_t<not std::is_same<Locator, std::decay_t<C>>::value>>
	Locator(C&& ct_type, ssize_t volume, size_t offset, size_t size) :
		type(Type::stored),
		ct_type(std::forward<C>(ct_type)),
		volume(volume),
		offset(volume == -1 ? 0 : offset),
		size(volume == -1 ? 0 : size) { }

	void data(std::string_view new_data) {
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
				/* FALLTHROUGH */
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
				/* FALLTHROUGH */
			case Type::stored:
				raw = new_data;
				break;
		}
	}

	void data(std::string&& new_data) {
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
				/* FALLTHROUGH */
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
				/* FALLTHROUGH */
			case Type::stored:
				_raw_holder = std::move(new_data);
				raw = _raw_holder;
				break;
		}
	}
	std::string_view data() const {
		if (size == 0) {
			return "";
		}
		switch (type) {
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

	static Locator unserialise(std::string_view locator_str) {
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

	std::string serialise() const {
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

	bool operator==(const Locator& other) const noexcept {
		return ct_type == other.ct_type;
	}

	bool operator!=(const Locator& other) const noexcept {
		return !operator==(other);
	}

	bool operator<(const Locator& other) const noexcept {
		return ct_type < other.ct_type;
	}
};


class Data {
	std::string serialised;
	std::vector<Locator> locators;

	std::vector<Locator> pending;

	void feed(std::string&& new_serialised) {
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

	void flush(const std::vector<Locator>& ops) {
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

public:
	Data() {
		feed(std::string(DATABASE_DATA_DEFAULT, sizeof(DATABASE_DATA_DEFAULT)));
	}

	Data(std::string&& serialised) {
		feed(std::move(serialised));
	}

	Data(Data&& other) {
		feed(std::move(other.serialised));
		pending = std::move(other.pending);
	}

	Data(const Data& other) {
		auto tmp = other.serialised;
		feed(std::move(tmp));
		flush(other.pending);
	}

	Data& operator=(Data&& other) {
		feed(std::move(other.serialised));
		pending = std::move(other.pending);
		return *this;
	}

	Data& operator=(const Data& other) {
		auto tmp = other.serialised;
		feed(std::move(tmp));
		flush(other.pending);
		return *this;
	}

	template <typename C>
	void update(C&& ct_type) {
		pending.emplace_back(std::forward<C>(ct_type));
	}

	template <typename C, typename S>
	void update(C&& ct_type, S&& data) {
		auto& locator = pending.emplace_back(std::forward<C>(ct_type));
		locator.data(std::forward<S>(data));
	}

	template <typename C>
	void update(C&& ct_type, ssize_t volume, size_t offset, size_t size) {
		pending.emplace_back(std::forward<C>(ct_type), volume, offset, size);
	}

	template <typename C, typename S>
	void update(C&& ct_type, ssize_t volume, size_t offset, size_t size, S&& data) {
		auto& locator = pending.emplace_back(std::forward<C>(ct_type), volume, offset, size);
		locator.data(std::forward<S>(data));
	}

	template <typename C>
	void erase(C&& ct_type) {
		pending.emplace_back(std::forward<C>(ct_type));
	}

	void flush() {
		flush(pending);
		pending.clear();
	}

	const std::string& serialise() const {
		return serialised;
	}

	bool operator==(const Data& other) const noexcept {
		return serialise() == other.serialise();
	}

	auto operator[](size_t pos) const {
		return locators.operator[](pos);
	}

	auto empty() const {
		return locators.empty();
	}

	auto size() const {
		return locators.size();
	}

	auto begin() {
		return locators.begin();
	}

	auto end() {
		return locators.end();
	}

	auto begin() const {
		return locators.cbegin();
	}

	auto end() const {
		return locators.cend();
	}

	const Locator* get(const ct_type_t& ct_type) const {
		for (const auto& locator : locators) {
			if (locator.ct_type == ct_type) {
				return &locator;
			}
		}
		return nullptr;
	}

	MsgPack get_obj() const {
		auto main_locator = get("");
		return main_locator != nullptr ? MsgPack::unserialise(main_locator->data()) : MsgPack(MsgPack::Type::MAP);
	}

	void set_obj(const MsgPack& object) {
		update("", object.serialise());
	}

	auto get_accepted(const accept_set_t& accept_set) const {
		const Accept* accepted_by = nullptr;
		const Locator* accepted = nullptr;
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
					double priority = accept.priority;
					if (priority < accepted_priority) {
						break;
					}
					if (
						(accept.ct_type.first == "*" && accept.ct_type.second == "*") ||
						(accept.ct_type.first == "*" && accept.ct_type.second == ct_type.second) ||
						(accept.ct_type.first == ct_type.first && accept.ct_type.second == "*") ||
						(accept.ct_type == ct_type)
					) {
						accepted_priority = priority;
						accepted = &locator;
						accepted_by = &accept;
					}
				}
			}
		}
		return std::make_pair(accepted, accepted_by);
	}
};
