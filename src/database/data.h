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
#include <string_view>
#include <vector>                                 // for std::vector
#include <set>                                    // for std::set


class MsgPack;


constexpr int STORED_CONTENT_TYPE  = 0;
constexpr int STORED_BLOB          = 1;

constexpr char DATABASE_DATA_HEADER_MAGIC        = 0x11;
constexpr char DATABASE_DATA_FOOTER_MAGIC        = 0x15;

constexpr std::string_view DATABASE_DATA_EMPTY     = std::string_view("\x11\x00\x15", 3);
constexpr std::string_view DATABASE_DATA_MAP       = std::string_view("\x11\x03\x00\x00\x80\x00\x15", 7);
constexpr std::string_view DATABASE_DATA_UNDEFINED = std::string_view("\x11\x05\x00\x00\xd4\x00\x00\x00\x15", 9);

constexpr std::string_view ANY_CONTENT_TYPE               = "*/*";
constexpr std::string_view HTML_CONTENT_TYPE              = "text/html";
constexpr std::string_view TEXT_CONTENT_TYPE              = "text/plain";
constexpr std::string_view JSON_CONTENT_TYPE              = "application/json";
constexpr std::string_view X_JSON_CONTENT_TYPE            = "application/x-json";
constexpr std::string_view YAML_CONTENT_TYPE              = "application/yaml";
constexpr std::string_view X_YAML_CONTENT_TYPE            = "application/x-yaml";
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

	ct_type_t(const std::string& first, const std::string& second);

	ct_type_t(std::string_view ct_type_str);

	ct_type_t(const char* ct_type_str);

	bool operator==(const ct_type_t& other) const noexcept;

	bool operator!=(const ct_type_t& other) const noexcept;

	bool operator<(const ct_type_t& other) const noexcept;

	void clear() noexcept;

	bool empty() const noexcept;

	std::string to_string() const;
};


static const ct_type_t no_type{};
static const ct_type_t any_type(ANY_CONTENT_TYPE);
static const ct_type_t html_type(HTML_CONTENT_TYPE);
static const ct_type_t text_type(TEXT_CONTENT_TYPE);
static const ct_type_t json_type(JSON_CONTENT_TYPE);
static const ct_type_t x_json_type(X_JSON_CONTENT_TYPE);
static const ct_type_t yaml_type(YAML_CONTENT_TYPE);
static const ct_type_t x_yaml_type(X_YAML_CONTENT_TYPE);
static const ct_type_t ndjson_type(NDJSON_CONTENT_TYPE);
static const ct_type_t x_ndjson_type(X_NDJSON_CONTENT_TYPE);
static const ct_type_t msgpack_type(MSGPACK_CONTENT_TYPE);
static const ct_type_t x_msgpack_type(X_MSGPACK_CONTENT_TYPE);
static const std::vector<const ct_type_t*> msgpack_serializers({
	&json_type, &x_json_type,
	&yaml_type, &x_yaml_type,
	&msgpack_type, &x_msgpack_type,
});


struct Accept {
	int position;
	double priority;

	ct_type_t ct_type;
	int indent;

	Accept(int position, double priority, ct_type_t ct_type, int indent);
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

using accept_set_t = std::set<Accept, accept_preference_comp<Accept>>;


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

	void data(std::string_view new_data);

	void data(std::string&& new_data);

	std::string_view data() const;

	static Locator unserialise(std::string_view locator_str);

	std::string serialise() const;

	bool operator==(const Locator& other) const noexcept;

	bool operator!=(const Locator& other) const noexcept;

	bool operator<(const Locator& other) const noexcept;
};


class Data {
	std::string serialised;
	std::vector<Locator> locators;

	std::vector<Locator> pending;

	void feed(std::string&& new_serialised, std::string&& new_version);

	void flush(const std::vector<Locator>& ops);

public:
	mutable std::string version;

	Data();
	Data(std::string&& serialised);
	Data(std::string&& serialised, std::string&& version);
	Data(Data&& other);
	Data(const Data& other);

	Data& operator=(Data&& other);
	Data& operator=(const Data& other);

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

	void flush();

	const std::string& serialise() const;

	bool operator==(const Data& other) const noexcept;

	const Locator& operator[](size_t pos) const;

	bool empty() const;

	size_t size() const;

	std::vector<Locator>::iterator begin();

	std::vector<Locator>::iterator end();

	std::vector<Locator>::const_iterator begin() const;

	std::vector<Locator>::const_iterator end() const;

	const Locator* get(const ct_type_t& ct_type) const;

	MsgPack get_obj() const;

	void set_obj(const MsgPack& object);

	std::pair<const Locator*, const Accept*> get_accepted(const accept_set_t& accept_set, const ct_type_t& mime_type = {}) const;
};
