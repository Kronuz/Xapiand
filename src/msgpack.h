/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "msgpack.hpp"

#include "rapidjson/document.h"
#include "xchange/rapidjson.hpp"

#include <unordered_map>

#define MSGPACK_MAP_INIT_SIZE 64
#define MSGPACK_ARRAY_INIT_SIZE 64


class MsgPack;

struct MsgPackBody;


using MapPack = std::unordered_map<std::string, std::shared_ptr<MsgPackBody>>;


struct MsgPackBody {
	uint32_t pos;
	msgpack::object* obj;
	MapPack map;
	ssize_t m_alloc;

	MsgPackBody(uint32_t pos_, msgpack::object* obj_, const MapPack& map_=MapPack(), ssize_t malloc_=-1)
		: pos(pos_),
		  obj(obj_),
		  map(map_),
		  m_alloc(malloc_) { }
};


namespace msgpack {
	MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
		namespace adaptor {
			template <>
			struct convert<MsgPack> {
				const msgpack::object& operator()(const msgpack::object& o, MsgPack& v) const;
			};

			template <>
			struct pack<MsgPack> {
				template <typename Stream>
				msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const MsgPack& v) const;
			};

			template <>
			struct object<MsgPack> {
				void operator()(msgpack::object& o, const MsgPack& v) const;
			};

			template <>
			struct object_with_zone<MsgPack> {
				void operator()(msgpack::object::with_zone& o, const MsgPack& v) const;
			};
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack


inline bool operator==(const MsgPack& x, const MsgPack& y);


class MsgPack {
	class object_handle {
		std::unique_ptr<msgpack::zone> zone;
		msgpack::detail::unpack_user user;
		msgpack::object obj;

		friend MsgPack;

	public:
		object_handle(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z)
			: zone(std::move(z)),
			  obj(o)
		{
			user.set_zone(*zone);
		}

		object_handle(const msgpack::object& o)
			: zone(std::make_unique<msgpack::zone>())
		{
			user.set_zone(*zone);
			obj = msgpack::object(o, *zone);
		}

		object_handle(object_handle&& _handler) noexcept
			: zone(std::move(_handler.zone)),
			  user(std::move(_handler.user)),
			  obj(std::move(_handler.obj)) { }


		object_handle()
			: zone(std::make_unique<msgpack::zone>())
		{
			user.set_zone(*zone);
			obj.type = msgpack::type::NIL;
		}

		object_handle(const std::string& buffer) {
			msgpack::unpacked u;
			msgpack::unpack(&u, buffer.data(), buffer.size());
			zone = msgpack::move(u.zone());
			user.set_zone(*zone);
			obj = u.get();
		}

		object_handle(const rapidjson::Document& doc) {
			zone = std::make_unique<msgpack::zone>();
			user.set_zone(*zone);
			obj = msgpack::object(doc, *zone);
		}

		object_handle(msgpack::unpacked& u)
			: zone(std::move(u.zone())),
			  obj(u.get())
		{
			user.set_zone(*zone);
		}

		object_handle(const object_handle&) = delete;
	};

	MsgPack(const std::shared_ptr<object_handle>& handler_, const std::shared_ptr<MsgPackBody>& body_, const std::shared_ptr<MsgPackBody>& p_body_);
	MsgPack(const std::shared_ptr<MsgPackBody>& body_, std::unique_ptr<msgpack::zone>&& z);

	std::shared_ptr<object_handle> handler;
	std::shared_ptr<MsgPackBody> parent_body;

	static std::shared_ptr<object_handle> make_handler(const std::string& buffer);
	static std::shared_ptr<object_handle> make_handler(const rapidjson::Document& doc);

	void init();
	void expand_map(size_t r_size);
	void expand_array(size_t r_size);

	friend msgpack::adaptor::convert<MsgPack>;

public:
	std::shared_ptr<MsgPackBody> body;

	MsgPack();
	MsgPack(MsgPack&& other) noexcept;
	MsgPack(const MsgPack& other);

	template <typename T, typename = std::enable_if_t<!std::is_base_of<MsgPack, std::decay_t<T>>::value>>
	MsgPack(T&& value)
		: handler(std::make_shared<object_handle>(std::forward<T>(value))),
		  parent_body(nullptr),
		  body(std::make_shared<MsgPackBody>(0, &handler->obj))
	{
		init();
	}

	template <typename MP, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<MP>>::value>>
	inline MsgPack operator[](MP&& o) {
		switch (o.body->obj->type) {
			case msgpack::type::STR:
				return operator[](std::string(o.body->obj->via.str.ptr, o.body->obj->via.str.size));
			case msgpack::type::POSITIVE_INTEGER:
				return operator[](static_cast<uint32_t>(o.body->obj->via.u64));
			case msgpack::type::NEGATIVE_INTEGER:
				return operator[](static_cast<uint32_t>(o.body->obj->via.i64));
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack operator[](const std::string& key);
	MsgPack operator[](uint32_t off);

	MsgPack at(const MsgPack& o) const;
	MsgPack at(const std::string& key) const;
	MsgPack at(uint32_t off) const;

	bool find(const MsgPack& o) const;
	bool find(const std::string& key) const;
	bool find(uint32_t off) const;

	bool erase(const MsgPack& o);
	bool erase(const std::string& key);
	bool erase(uint32_t off);

	std::string to_json_string(bool prettify=false) const;
	std::string to_string() const;
	rapidjson::Document to_json() const;

	void reset(MsgPack&& other) noexcept;
	void reset(const MsgPack& other);
	MsgPack path(const std::vector<std::string>& path) const;

	template<typename T>
	MsgPack& operator=(T&& v) {
		msgpack::object o(std::forward<T>(v), *handler->zone);
		body->obj->type = o.type;
		body->obj->via = o.via;
		body->m_alloc = -1;
		init();
		return *this;
	}

	template<typename T>
	void insert_item_to_array(uint32_t offset, T&& v) {
		if (body->obj->type == msgpack::type::NIL) {
			body->obj->type = msgpack::type::ARRAY;
			body->obj->via.array.ptr = nullptr;
			body->obj->via.array.size = 0;
		}

		if (body->obj->type == msgpack::type::ARRAY) {
			if (body->obj->via.array.size < offset + 1) {
				auto r_size = offset + 1;
				expand_array(r_size);

				const msgpack::object* npend(body->obj->via.array.ptr + offset);
				for (auto np = body->obj->via.array.ptr + body->obj->via.array.size; np != npend; ++np) {
					msgpack::detail::unpack_nil(*np);
					msgpack::detail::unpack_array_item(*body->obj, *np);
				}

				msgpack::detail::unpack_array_item(*body->obj, msgpack::object(std::forward<T>(v), handler->zone.get()));
			} else {
				expand_array(body->obj->via.array.size + 1);

				auto p = body->obj->via.array.ptr + offset;

				if (p->type == msgpack::type::NIL) {
					at(offset) = msgpack::object(std::forward<T>(v), handler->zone.get());
				} else {
					memmove(p + 1, p, (body->obj->via.array.size - offset) * sizeof(msgpack::object));
					++body->obj->via.array.size;
					at(offset) = msgpack::object(std::forward<T>(v), handler->zone.get());
				}
			}
		} else {
			throw msgpack::type_error();
		}
	}

	template<typename T>
	void add_item_to_array(T&& v) {
		if (body->obj->type == msgpack::type::NIL) {
			body->obj->type = msgpack::type::ARRAY;
			body->obj->via.array.ptr = nullptr;
			body->obj->via.array.size = 0;
		}

		if (body->obj->type == msgpack::type::ARRAY) {
			auto r_size = body->obj->via.array.size + 1;
			expand_array(r_size);
			msgpack::detail::unpack_array_item(*body->obj, msgpack::object(std::forward<T>(v), handler->zone.get()));
		} else {
			throw msgpack::type_error();
		}
	}

	MsgPack parent() {
		if (parent_body) {
			return MsgPack(handler, parent_body, nullptr);
		} else {
			return MsgPack();
		}
	}

	template<typename T, bool is_const_iterator = true>
	class _iterator : public std::iterator<std::input_iterator_tag, MsgPack> {

		using MsgPack_type = typename std::conditional<is_const_iterator, const T, T>::type;

		MsgPack_type* pack_obj;
		uint32_t off;

		friend class MsgPack;

	public:
		_iterator(T* o, uint32_t _off)
			: pack_obj(o),
			  off(_off) { }

		_iterator(const T* o, uint32_t _off)
			: pack_obj(o),
			  off(_off) { }

		_iterator(const _iterator& it)
			: pack_obj(it.pack_obj),
			  off(it.off) { }

		_iterator& operator++() {
			++off;
			return *this;
		}

		_iterator operator++(int) {
			iterator tmp(*this);
			++off;
			return tmp;
		}

		_iterator& operator+=(int pos) {
			off += pos;
			return *this;
		}

		_iterator operator+(int pos) const {
			_iterator tmp(*this);
			tmp.off += pos;
			return tmp;
		}

		_iterator operator=(const _iterator& other) {
			pack_obj = other.pack_obj;
			off = other.off;
			return *this;
		}

		MsgPack_type operator*() const {
			switch (pack_obj->body->obj->type) {
				case msgpack::type::MAP:
					return MsgPack(pack_obj->handler, std::make_shared<MsgPackBody>(off, &pack_obj->body->obj->via.map.ptr[off].key), pack_obj->body);
				case msgpack::type::ARRAY:
					return MsgPack(pack_obj->handler, std::make_shared<MsgPackBody>(off, &pack_obj->body->obj->via.array.ptr[off]), pack_obj->body);
				default:
					throw msgpack::type_error();
			}
		}

		bool operator==(const _iterator& other) const {
			return *pack_obj == *other.pack_obj && off == other.off;
		}

		bool operator!=(const _iterator& other) const {
			return !operator==(other);
		}
	};

	using iterator = _iterator<MsgPack, false>;
	using const_iterator = _iterator<MsgPack, true>;

	iterator begin() {
		return iterator(this, 0);
	}

	const_iterator begin() const {
		return const_iterator(this, 0);
	}

	const_iterator cbegin() const {
		return const_iterator(this, 0);
	}

	iterator end() {
		return iterator(this, static_cast<uint32_t>(size()));
	}

	const_iterator end() const {
		return const_iterator(this, static_cast<uint32_t>(size()));
	}

	const_iterator cend() const {
		return const_iterator(this, static_cast<uint32_t>(size()));
	}

	explicit operator bool() const {
		return body->obj->type != msgpack::type::NIL;
	}

	inline void reserve(size_t n) {
		switch (body->obj->type) {
			case msgpack::type::MAP:
				return expand_map(n);
			case msgpack::type::ARRAY:
				return expand_array(n);
			default:
				return;
		}
	}

	inline size_t capacity() const noexcept {
		return body->m_alloc;
	}

	inline size_t size() const noexcept {
		switch (body->obj->type) {
			case msgpack::type::MAP:
				return body->obj->via.map.size;
			case msgpack::type::ARRAY:
				return body->obj->via.array.size;
			default:
				return 0;
		}
	}

	inline MsgPack clone() const {
		return MsgPack(*body->obj);
	}

	inline uint64_t get_u64() const {
		switch (body->obj->type) {
			case msgpack::type::POSITIVE_INTEGER:
				return body->obj->via.u64;
			case msgpack::type::NEGATIVE_INTEGER:
				return body->obj->via.i64;
			default:
				throw msgpack::type_error();
		}
	}

	inline int64_t get_i64() const {
		switch (body->obj->type) {
			case msgpack::type::POSITIVE_INTEGER:
				return body->obj->via.u64;
			case msgpack::type::NEGATIVE_INTEGER:
				return body->obj->via.i64;
			default:
				throw msgpack::type_error();
		}
	}

	inline double get_f64() const {
		switch (body->obj->type) {
			case msgpack::type::POSITIVE_INTEGER:
				return body->obj->via.u64;
			case msgpack::type::NEGATIVE_INTEGER:
				return body->obj->via.i64;
			case msgpack::type::FLOAT:
				return body->obj->via.f64;
			default:
				throw msgpack::type_error();
		}
	}

	inline std::string get_str() const {
		if (body->obj->type == msgpack::type::STR) {
			return std::string(body->obj->via.str.ptr, body->obj->via.str.size);
		}
		throw msgpack::type_error();
	}

	inline bool get_bool() const {
		if (body->obj->type == msgpack::type::BOOLEAN) {
			return body->obj->via.boolean;
		}
		throw msgpack::type_error();
	}

	inline msgpack::type::object_type get_type() const {
		return body->obj->type;
	}
};


inline bool operator==(const MsgPack& x, const MsgPack& y) {
	return *x.body->obj == *y.body->obj;
}


inline bool operator!=(const MsgPack& x, const MsgPack& y) {
	return !(x == y);
}


inline std::ostream& operator<<(std::ostream& s, const MsgPack& o) {
	s << *o.body->obj;
	return s;
}
