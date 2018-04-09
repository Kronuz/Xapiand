/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "xapiand.h"


#include <string>
#include "string_view.hh"
#include <vector>                  // for std::vector

#include "length.h"                // for serialise_length()
#include "utils.h"                 // for toUType


constexpr char DATABASE_DATA_HEADER_MAGIC        = 0x11;
constexpr char DATABASE_DATA_FOOTER_MAGIC        = 0x15;

constexpr char DATABASE_DATA_DEFAULT[]  = { DATABASE_DATA_HEADER_MAGIC, 0x03, 0x00, 0x00, '\x80', 0x00, DATABASE_DATA_FOOTER_MAGIC };

constexpr std::string_view ANY_CONTENT_TYPE               = "*/*";
constexpr std::string_view HTML_CONTENT_TYPE              = "text/html";
constexpr std::string_view TEXT_CONTENT_TYPE              = "text/plain";
constexpr std::string_view JSON_CONTENT_TYPE              = "application/json";
constexpr std::string_view MSGPACK_CONTENT_TYPE           = "application/msgpack";
constexpr std::string_view X_MSGPACK_CONTENT_TYPE         = "application/x-msgpack";
constexpr std::string_view FORM_URLENCODED_CONTENT_TYPE   = "application/www-form-urlencoded";
constexpr std::string_view X_FORM_URLENCODED_CONTENT_TYPE = "application/x-www-form-urlencoded";


struct ct_type_t {
	std::string str;
	std::string_view first;
	std::string_view second;

	ct_type_t() = default;

	ct_type_t(std::string&& ct_type_str) : str(std::move(ct_type_str)) {
		const auto found = str.rfind('/');
		if (found != std::string::npos) {
			std::string_view str_view(str);
			first = str_view.substr(0, found);
			second = str_view.substr(found + 1);
		}
	}

	ct_type_t(std::string_view ct_type_str) : ct_type_t(std::string(ct_type_str)) { }

	ct_type_t(const char *ct_type_str) : ct_type_t(std::string(ct_type_str)) { }

	// ct_type_t(const char *ct_type_str) : ct_type_t(std::string(ct_type_str)) { }

	ct_type_t(const std::string& first_, const std::string& second_) : ct_type_t(first_ + "/" + second_) { }

	bool operator==(const ct_type_t& other) const noexcept {
		return str == other.str;
	}

	bool operator!=(const ct_type_t& other) const noexcept {
		return !operator==(other);
	}

	void clear() noexcept {
		str.clear();
		first = second = str;
	}

	bool empty() const noexcept {
		return str.empty();
	}

	const std::string& to_string() const {
		return str;
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

	Accept(int position, double priority, ct_type_t ct_type, int indent) : position(position), priority(priority), ct_type(ct_type), indent(indent) { }
};
using accept_set_t = std::set<Accept, accept_preference_comp<Accept>>;


static const ct_type_t no_type{};
static const ct_type_t any_type(ANY_CONTENT_TYPE);
static const ct_type_t html_type(HTML_CONTENT_TYPE);
static const ct_type_t text_type(TEXT_CONTENT_TYPE);
static const ct_type_t json_type(JSON_CONTENT_TYPE);
static const ct_type_t msgpack_type(MSGPACK_CONTENT_TYPE);
static const ct_type_t x_msgpack_type(X_MSGPACK_CONTENT_TYPE);
static const std::vector<ct_type_t> msgpack_serializers({ json_type, msgpack_type, x_msgpack_type, html_type, text_type });


class Data {
public:
	enum class Type : uint8_t {
		inplace,
		stored,
	};

	struct Locator {
		Type type;

		ct_type_t ct_type;

		std::string_view data;

		ssize_t volume;
		size_t offset;
		size_t size;

		static Locator unserialise(std::string_view locator_str) {
			Locator new_locator;
			const char *p = locator_str.data();
			const char *p_end = p + locator_str.size();
			auto length = unserialise_length(&p, p_end, true);
			new_locator.ct_type = ct_type_t(std::string_view(p, length));
			p += length;
			new_locator.type = static_cast<Type>(*p++);
			switch (new_locator.type) {
				case Type::inplace:
					new_locator.data = std::string_view(p, p_end - p);
					break;
				case Type::stored:
					new_locator.volume = unserialise_length(&p, p_end);
					new_locator.offset = unserialise_length(&p, p_end);
					new_locator.size = unserialise_length(&p, p_end);
					new_locator.data = std::string_view(p, p_end - p);
					break;
				default:
					THROW(SerialisationError, "Bad encoded data locator: Unknown type");
			}
			return new_locator;
		}

		std::string serialise() const {
			std::string result;
			result.append(serialise_string(ct_type.to_string()));
			result.push_back(toUType(type));
			switch (type) {
				case Type::inplace:
					result.append(data);
					break;
				case Type::stored:
					result.append(serialise_length(volume));
					result.append(serialise_length(offset));
					result.append(serialise_length(size));
					result.append(data);
					break;
				default:
					THROW(SerialisationError, "Bad data locator: Unknown type");
			}
			result.insert(0, serialise_length(result.size()));
			return result;
		}
	};

private:
	std::string serialised;
	std::vector<Locator> locators;
	std::string_view trailing;

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
		trailing = std::string_view(p, p_end - p);
	}

	void update(const Locator& new_locator) {
		std::string new_serialised;
		new_serialised.push_back(DATABASE_DATA_HEADER_MAGIC);
		if (!new_locator.ct_type.empty()) {
			new_serialised.append(new_locator.serialise());
		}
		for (const auto& locator : locators) {
			if (locator.ct_type != new_locator.ct_type) {
				new_serialised.append(locator.serialise());
			}
		}
		if (new_locator.ct_type.empty()) {
			new_serialised.append(new_locator.serialise());
		}
		new_serialised.push_back('\0');
		new_serialised.push_back(DATABASE_DATA_FOOTER_MAGIC);
		new_serialised.append(trailing);
		feed(std::move(new_serialised));
	}

public:
	Data() {
		feed(std::string(DATABASE_DATA_DEFAULT, sizeof(DATABASE_DATA_DEFAULT)));
	}

	Data(std::string&& serialised) {
		feed(std::move(serialised));
	}

	template <typename S>
	void update(ct_type_t&& ct_type, S&& data) {
		Locator new_locator;
		new_locator.ct_type = std::move(ct_type);
		new_locator.type = Type::inplace;
		new_locator.data = std::forward<S>(data);
		update(new_locator);
	}

	template <typename S>
	void update(const ct_type_t& ct_type, S&& data = "") {
		update(ct_type_t(ct_type), std::forward<S>(data));
	}

	template <typename S>
	void update(ct_type_t&& ct_type, ssize_t volume, size_t offset, size_t size, S&& data = "") {
		Locator new_locator;
		new_locator.ct_type = std::move(ct_type);
		new_locator.type = Type::stored;
		new_locator.volume = volume;
		if (volume == -1) {
			new_locator.offset = 0;
			new_locator.size = data.size();
		} else {
			new_locator.offset = offset;
			new_locator.size = size;
		}
		new_locator.data = std::forward<S>(data);
		update(new_locator);
	}

	template <typename S>
	void update(const ct_type_t& ct_type, ssize_t volume, size_t offset, size_t size, S&& data = "") {
		update(ct_type_t(ct_type), volume, offset, size, std::forward<S>(data));
	}

	void erase(const ct_type_t& ct_type) {
		std::string new_serialised;
		new_serialised.push_back(DATABASE_DATA_HEADER_MAGIC);
		for (const auto& locator : locators) {
			if (locator.ct_type != ct_type) {
				new_serialised.append(locator.serialise());
			}
		}
		new_serialised.push_back('\0');
		new_serialised.push_back(DATABASE_DATA_FOOTER_MAGIC);
		new_serialised.append(trailing);
		feed(std::move(new_serialised));
	}

	const std::string& serialise() const {
		return serialised;
	}

	size_t empty() const {
		return locators.empty();
	}

	size_t size() const {
		return locators.size();
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
					if (priority <= accepted_priority) {
						break;
					}
					auto& accept_ct = accept.ct_type;
					if (
						(accept_ct.first == "*" && accept_ct.second == "*") ||
						(accept_ct.first == "*" && accept_ct.second == ct_type.second) ||
						(accept_ct.first == ct_type.second && accept_ct.second == "*") ||
						(accept_ct == ct_type)
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
