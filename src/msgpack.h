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

#define MSGPACK_MAP_INIT_SIZE 64
#define MSGPACK_ARRAY_INIT_SIZE 64


class MsgPack;


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
			: zone(std::make_unique<msgpack::zone>())
		{
			user.set_zone(*zone.get());
			obj.type = msgpack::type::NIL;
		}
	};

	std::shared_ptr<object_handle> handler;

	std::shared_ptr<MsgPack::object_handle> make_handler();
	std::shared_ptr<object_handle> make_handler(const std::string& buffer);
	std::shared_ptr<MsgPack::object_handle> make_handler(const rapidjson::Document& doc);

	void expand_map(size_t r_size);
	void expand_array(size_t r_size);

public:
	msgpack::object* parent_obj;
	msgpack::object* obj;

private:
	size_t m_alloc;

public:
	MsgPack();
	MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object* o, msgpack::object* p);
	MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z);
	MsgPack(msgpack::unpacked& u);
	MsgPack(const std::string& buffer);
	MsgPack(const rapidjson::Document& doc);
	MsgPack(MsgPack&& other) noexcept;
	MsgPack(const MsgPack& other);

	MsgPack operator[](const MsgPack& o);
	MsgPack operator[](const std::string& key);
	MsgPack operator[](uint32_t off);

	MsgPack at(const MsgPack& o) const;
	MsgPack at(const std::string& key) const;
	MsgPack at(uint32_t off) const;

	bool find(const MsgPack& o) const;
	bool find(const std::string& key) const;
	bool find(uint32_t off) const;

	std::string to_json_string(bool prettify=false) const;
	std::string to_string() const;
	rapidjson::Document to_json() const;
	void reserve(size_t n);
	size_t capacity() const noexcept;
	size_t size() const noexcept;
	bool erase(const std::string& key);
	bool erase(uint32_t off);
	MsgPack duplicate() const;
	void reset(MsgPack&& other) noexcept;
	void reset(const MsgPack& other);
	MsgPack path(const std::vector<std::string>& path) const;

	template<typename T>
	MsgPack& operator=(T&& v) {
		msgpack::object o(std::forward<T>(v), handler->zone.get());
		obj->type = o.type;
		obj->via = o.via;
		m_alloc = size();
		return *this;
	}

	template<typename T>
	void insert_item_to_array(unsigned offset, T&& v) {
		if (obj->type == msgpack::type::NIL) {
			obj->type = msgpack::type::ARRAY;
			obj->via.array.ptr = nullptr;
			obj->via.array.size = 0;
		}

		if (obj->type == msgpack::type::ARRAY) {
			if (static_cast<unsigned>(obj->via.array.size - 1) < offset) {
				auto r_size = offset + 1;
				expand_array(r_size);

				const msgpack::object* npend(obj->via.array.ptr + offset);
				for (auto np = obj->via.array.ptr + obj->via.array.size; np != npend; ++np) {
					msgpack::detail::unpack_nil(*np);
					msgpack::detail::unpack_array_item(*obj, *np);
				}

				msgpack::detail::unpack_array_item(*obj, msgpack::object(std::forward<T>(v), handler->zone.get()));
			} else {
				expand_array(obj->via.array.size + 1);

				auto p = obj->via.array.ptr + offset;

				if (p->type == msgpack::type::NIL) {
					at(offset) = msgpack::object(std::forward<T>(v), handler->zone.get());
				} else {
					memmove(p + 1, p, (obj->via.array.size - offset) * sizeof(msgpack::object));
					++obj->via.array.size;
					at(offset) = msgpack::object(std::forward<T>(v), handler->zone.get());
				}
			}
		} else {
			throw msgpack::type_error();
		}
	}

	template<typename T>
	void add_item_to_array(T&& v) {
		if (obj->type == msgpack::type::NIL) {
			obj->type = msgpack::type::ARRAY;
			obj->via.array.ptr = nullptr;
			obj->via.array.size = 0;
		}

		if (obj->type == msgpack::type::ARRAY) {
			auto r_size = obj->via.array.size + 1;
			expand_array(r_size);
			msgpack::detail::unpack_array_item(*obj, msgpack::object(std::forward<T>(v), handler->zone.get()));
		} else {
			throw msgpack::type_error();
		}
	}

	MsgPack parent() {
		if (parent_obj) {
			return MsgPack(handler, parent_obj, nullptr);
		} else {
			return MsgPack();
		}
	}

	template<typename T, bool is_const_iterator = true>
	class _iterator : public std::iterator<std::input_iterator_tag, MsgPack> {

		using MsgPack_type = typename std::conditional<is_const_iterator, const T, T>::type;

		MsgPack_type* obj;
		uint32_t off;

		friend class MsgPack;

	public:
		_iterator(T* o, uint32_t _off)
			: obj(o),
			  off(_off) { }

		_iterator(const T* o, uint32_t _off)
			: obj(o),
			  off(_off) { }

		_iterator(const _iterator& it)
			: obj(it.obj),
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
			obj = other.obj;
			off = other.off;
			return *this;
		}

		MsgPack_type operator*() const {
			switch (obj->obj->type) {
				case msgpack::type::MAP:
					return MsgPack(obj->handler, &obj->obj->via.map.ptr[off].key, obj->obj);
				case msgpack::type::ARRAY:
					return MsgPack(obj->handler, &obj->obj->via.array.ptr[off], obj->obj);
				default:
					throw msgpack::type_error();
			}
		}

		bool operator==(const _iterator& other) const {
			return *obj == *other.obj && off == other.off;
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
		return obj->type != msgpack::type::NIL;
	}
};


inline bool operator==(const MsgPack& x, const MsgPack& y) {
	return *x.obj == *y.obj;
}


inline bool operator!=(const MsgPack& x, const MsgPack& y) {
	return !(x == y);
}


inline std::ostream& operator<<(std::ostream& s, const MsgPack& o) {
	s << *o.obj;
	return s;
}
