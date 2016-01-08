/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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


class MsgPack;


inline bool operator==(const MsgPack& x, const MsgPack& y);


class MsgPack {
	class object_handle {
		msgpack::object obj;
		std::unique_ptr<msgpack::zone> zone;
		msgpack::detail::unpack_user user;

		friend MsgPack;

	public:
		object_handle(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z)
			: obj(o),
			  zone(std::move(z))
		{
			user.set_zone(*zone.get());
		}

		object_handle(object_handle&& _handler) noexcept
			: obj(std::move(_handler.obj)),
			  zone(std::move(_handler.zone)),
			  user(std::move(_handler.user)) { }

		object_handle(const object_handle&) = delete;

		object_handle()
			: obj(),
			  zone(new msgpack::zone)
		{
			user.set_zone(*zone.get());
			obj.type = msgpack::type::MAP;
			obj.via.map.size = 0;
			obj.via.map.ptr = nullptr;
		}
	};

	std::shared_ptr<object_handle> handler;

	std::shared_ptr<MsgPack::object_handle> make_handler();
	std::shared_ptr<object_handle> make_handler(const std::string& buffer);
	std::shared_ptr<MsgPack::object_handle> make_handler(const rapidjson::Document& doc);

public:
	msgpack::object& obj;

	MsgPack();
	MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object& o);
	MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z);
	MsgPack(msgpack::unpacked& u);
	MsgPack(const std::string& buffer);
	MsgPack(const rapidjson::Document& doc);
	MsgPack(MsgPack&& other) noexcept;
	MsgPack(const MsgPack& other);

	MsgPack operator[](const MsgPack& o) const;
	MsgPack operator[](const std::string& name) const;
	MsgPack operator[](uint32_t off) const;

	std::string to_json_string(bool prettify);
	static rapidjson::Document to_json(msgpack::object &ob);
	std::string to_string();
	
	inline std::string getKey() const {
		return std::string(obj.via.map.ptr->key.via.str.ptr, obj.via.map.ptr->key.via.str.size);
	}

	template<typename T, typename = std::enable_if_t<!std::is_base_of<MsgPack, std::decay_t<T>>::value>>
	MsgPack& operator=(T&& v) {
		msgpack::object o(std::forward<T>(v), handler->zone.get());
		obj.type = o.type;
		obj.via = o.via;
		return *this;
	}

	MsgPack& operator=(const MsgPack& other) {
		handler = other.handler;
		obj = handler->obj;
		return *this;
	}

	MsgPack& operator=(MsgPack&& other) {
		handler = std::move(other.handler);
		obj = handler->obj;
		return *this;
	}

	class iterator {
		MsgPack* obj;
		uint32_t off;

		friend class MsgPack;

	public:
		iterator(MsgPack* o, uint32_t _off)
			: obj(o),
			  off(_off) { }

		iterator(const iterator& it)
			: obj(it.obj),
			  off(it.off) { }

		iterator& operator++() {
			++off;
			return *this;
		}

		iterator operator++(int) {
			iterator tmp(*this);
			++off;
			return tmp;
		}

		iterator operator=(const iterator& other) {
			obj = other.obj;
			off = other.off;
			return *this;
		}

		MsgPack operator*() const {
			switch (obj->obj.type) {
				case msgpack::type::MAP:
					return MsgPack(obj->handler, obj->obj.via.map.ptr[off].key);
				case msgpack::type::ARRAY:
					return MsgPack(obj->handler, obj->obj.via.array.ptr[off]);
				default:
					throw msgpack::type_error();
			}
		}

		bool operator==(const iterator& other) const {
			return *obj == *other.obj && off == other.off;
		}

		bool operator!=(const iterator& other) const {
			return !operator==(other);
		}

		explicit operator bool() const {
			return obj->obj.type == msgpack::type::MAP ? obj->obj.via.map.size != off : obj->obj.via.array.size != off;
		}
	};

	using const_iterator = const iterator;

	iterator begin() {
		return iterator(this, 0);
	}

	const_iterator begin() const { return begin(); }
	const_iterator cbegin() const { return begin(); }

	iterator end() {
		return iterator(this, obj.type == msgpack::type::MAP ? obj.via.map.size : obj.via.array.size);
	}

	const_iterator end() const { return end(); }
	const_iterator cend() const { return end(); }

	explicit operator bool() const {
		return obj.type == msgpack::type::MAP ? obj.via.map.size : obj.via.array.size;
	}
};


inline bool operator==(const MsgPack& x, const MsgPack& y) {
	return x.obj == y.obj;
}


inline bool operator!=(const MsgPack& x, const MsgPack& y) {
	return !(x == y);
}


inline std::ostream& operator<<(std::ostream& s, const MsgPack& o) {
	s << o.obj;
	return s;
}
