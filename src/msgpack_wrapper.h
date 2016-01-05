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

#include "msgpack.hpp"

class MsgPack;


inline bool operator==(const MsgPack& x, const MsgPack& y);


class MsgPack {
	class object_handle {
		msgpack::object obj;
		std::unique_ptr<msgpack::zone> zone;
		msgpack::detail::unpack_user user;

		friend MsgPack;

	public:
		object_handle(const msgpack::object& obj, std::unique_ptr<msgpack::zone>&& z)
			: obj(obj),
			  zone(std::move(z))
		{
			user.set_zone(*zone.get());
		}

		object_handle(object_handle&& _handler)
			: obj(std::move(_handler.obj)),
			  zone(std::move(_handler.zone)),
			  user(std::move(_handler.user)) { }
	};

	std::shared_ptr<object_handle> handler;

	std::shared_ptr<object_handle> make_handler(const std::string& buffer);

public:
	msgpack::object& obj;

	MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object& o);
	MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z);
	MsgPack(msgpack::unpacked& u);
	MsgPack(const std::string& buffer);

	MsgPack operator[](const MsgPack& o);
	MsgPack operator[](const std::string& name);
	MsgPack operator[](uint32_t off);

	template <typename T>
	MsgPack& operator=(T v) {
		msgpack::object o(std::forward<T>(v), handler->zone.get());
		obj.type = o.type;
		obj.via = o.via;
		return *this;
	}

	struct iterator {
		MsgPack* obj;
		uint32_t off;

		friend class MsgPack;

	public:
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
	};

	using const_iterator = const iterator;

	iterator begin() {
		return {
			.obj = this,
			.off = 0
		};
	}

	const_iterator begin() const { return begin(); }
	const_iterator cbegin() const { return begin(); }

	iterator end() {
		return {
			.obj = this,
			.off = obj.type == msgpack::type::MAP ? obj.via.map.size : obj.via.array.size
		};
	}

	const_iterator end() const { return end(); }
	const_iterator cend() const { return end(); }
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
