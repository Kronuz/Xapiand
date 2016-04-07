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

#include "msgpack.h"

#include <sstream>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "xchange/rapidjson.hpp"


MsgPack::MsgPack()
	: handler(std::make_shared<object_handle>()),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj, MapPack(), 0)) { }


MsgPack::MsgPack(const msgpack::object& o)
	: handler(std::make_shared<object_handle>(o)),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj, MapPack(), 0)) { }


MsgPack::MsgPack(const std::shared_ptr<object_handle>& handler_, const std::shared_ptr<MsgPackBody>& body_, const std::shared_ptr<MsgPackBody>& p_body_)
	: handler(handler_),
	  parent_body(p_body_),
	  body(body_)
{
	init();
}


MsgPack::MsgPack(const std::shared_ptr<MsgPackBody>& body_, std::unique_ptr<msgpack::zone>&& z)
	: handler(std::make_shared<object_handle>(*body_->obj, std::move(z))),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj, body_->map, body_->m_alloc))
{
	init();
}


MsgPack::MsgPack(msgpack::unpacked& u)
	: handler(std::make_shared<object_handle>(u.get(), std::move(u.zone()))),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj))
{
	init();
}


MsgPack::MsgPack(const std::string& buffer)
	: handler(make_handler(buffer)),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj))
{
	init();
}


MsgPack::MsgPack(const rapidjson::Document& doc)
	: handler(make_handler(doc)),
	  parent_body(nullptr),
	  body(std::make_shared<MsgPackBody>(0, &handler->obj))
{
	init();
}


MsgPack::MsgPack(MsgPack&& other) noexcept
	: handler(std::move(other.handler)),
	  parent_body(std::move(other.parent_body)),
	  body(std::move(other.body)) { }


MsgPack::MsgPack(const MsgPack& other)
	: handler(other.handler),
	  parent_body(other.parent_body),
	  body(other.body) { }


std::shared_ptr<MsgPack::object_handle>
MsgPack::make_handler()
{
	return std::make_shared<MsgPack::object_handle>();
}


std::shared_ptr<MsgPack::object_handle>
MsgPack::make_handler(const std::string& buffer)
{
	msgpack::unpacked u;
	msgpack::unpack(&u, buffer.data(), buffer.size());
	return std::make_shared<MsgPack::object_handle>(u.get(), msgpack::move(u.zone()));
}


std::shared_ptr<MsgPack::object_handle>
MsgPack::make_handler(const rapidjson::Document& doc)
{
	auto zone(std::make_unique<msgpack::zone>());
	msgpack::object obj(doc, *zone.get());
	return std::make_shared<MsgPack::object_handle>(obj, msgpack::move(zone));
}


void
MsgPack::init()
{
	if (body->m_alloc == -1 && body->obj->type == msgpack::type::MAP) {
		body->map.reserve(body->obj->via.map.size);
		body->m_alloc = body->obj->via.map.size;
		uint32_t pos = 0;
		const msgpack::object_kv* pend(body->obj->via.map.ptr + body->obj->via.map.size);
		for (auto p = body->obj->via.map.ptr; p != pend; ++p, ++pos) {
			if (p->key.type == msgpack::type::STR) {
				body->map.insert(std::make_pair(std::string(p->key.via.str.ptr, p->key.via.str.size), std::make_shared<MsgPackBody>(pos, &p->val)));
			}
		}
	} else {
		body->m_alloc = body->obj->type == msgpack::type::ARRAY ? body->obj->via.array.size : 0;
	}
}


MsgPack
MsgPack::operator[](const MsgPack& o)
{
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


MsgPack
MsgPack::operator[](const std::string& key)
{
	auto it = body->map.find(key);
	if (it != body->map.end()) {
		return MsgPack(handler, it->second, body);
	}

	if (body->obj->type == msgpack::type::NIL) {
		body->obj->type = msgpack::type::MAP;
		body->obj->via.map.ptr = nullptr;
		body->obj->via.map.size = 0;
	}

	if (body->obj->type == msgpack::type::MAP) {
		expand_map(body->obj->via.map.size);
		msgpack::object_kv* np(body->obj->via.map.ptr + body->obj->via.map.size);

		msgpack::detail::unpack_str(handler->user, key.data(), (uint32_t)key.size(), np->key);
		msgpack::detail::unpack_nil(np->val);
		msgpack::detail::unpack_map_item(*body->obj, np->key, np->val);
		auto ins_it = body->map.insert(std::make_pair(key, std::make_shared<MsgPackBody>(body->obj->via.map.size, &np->val)));

		return MsgPack(handler, ins_it.first->second, body);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](uint32_t off)
{
	if (body->obj->type == msgpack::type::NIL) {
		body->obj->type = msgpack::type::ARRAY;
		body->obj->via.array.ptr = nullptr;
		body->obj->via.array.size = 0;
	}

	if (body->obj->type == msgpack::type::ARRAY) {
		auto r_size = off + 1;
		if (body->obj->via.array.size < r_size) {
			expand_array(r_size);

			// Initialize new elements.
			const msgpack::object* npend(body->obj->via.array.ptr + r_size);
			for (auto np = body->obj->via.array.ptr + body->obj->via.array.size; np != npend; ++np) {
				msgpack::detail::unpack_nil(*np);
				msgpack::detail::unpack_array_item(*body->obj, *np);
			}
		}

		return MsgPack(handler, std::make_shared<MsgPackBody>(off, &body->obj->via.array.ptr[off]), body);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::at(const MsgPack& o) const
{
	switch (o.body->obj->type) {
		case msgpack::type::STR:
			return at(std::string(o.body->obj->via.str.ptr, o.body->obj->via.str.size));
		case msgpack::type::POSITIVE_INTEGER:
			return at(static_cast<uint32_t>(o.body->obj->via.u64));
		case msgpack::type::NEGATIVE_INTEGER:
			return at(static_cast<uint32_t>(o.body->obj->via.i64));
		default:
			throw msgpack::type_error();
	}
}


MsgPack
MsgPack::at(const std::string& key) const
{
	if (body->obj->type == msgpack::type::MAP) {
		return MsgPack(handler, body->map.at(key), body);
	} else if (body->obj->type == msgpack::type::NIL) {
		throw std::out_of_range(key);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::at(uint32_t off) const
{
	if (body->obj->type == msgpack::type::ARRAY) {
		if (off < body->obj->via.array.size) {
			return MsgPack(handler, std::make_shared<MsgPackBody>(off, &body->obj->via.array.ptr[off]), body);
		}
		throw std::out_of_range(std::to_string(off));
	} else if (body->obj->type == msgpack::type::NIL) {
		throw std::out_of_range(std::to_string(off));
	}

	throw msgpack::type_error();
}


bool
MsgPack::find(const MsgPack& o) const
{
	switch (o.body->obj->type) {
		case msgpack::type::STR:
			return find(std::string(o.body->obj->via.str.ptr, o.body->obj->via.str.size));
		case msgpack::type::POSITIVE_INTEGER:
			return find(static_cast<uint32_t>(o.body->obj->via.u64));
		case msgpack::type::NEGATIVE_INTEGER:
			return find(static_cast<uint32_t>(o.body->obj->via.i64));
		default:
			return false;
	}
}


bool
MsgPack::find(const std::string& key) const
{
	return body->map.find(key) != body->map.end();
}


bool
MsgPack::find(uint32_t off) const
{
	return body->obj->type == msgpack::type::ARRAY && off < body->obj->via.array.size;
}


bool
MsgPack::erase(const MsgPack& o)
{
	switch (o.body->obj->type) {
		case msgpack::type::STR:
			return erase(std::string(o.body->obj->via.str.ptr, o.body->obj->via.str.size));
		case msgpack::type::POSITIVE_INTEGER:
			return erase(static_cast<uint32_t>(o.body->obj->via.u64));
		case msgpack::type::NEGATIVE_INTEGER:
			return erase(static_cast<uint32_t>(o.body->obj->via.i64));
		default:
			return false;
	}
}


bool
MsgPack::erase(const std::string& key)
{
	if (body->obj->type == msgpack::type::MAP) {
		auto it = body->map.find(key);
		if (it != body->map.end()) {
			auto p = body->obj->via.map.ptr + it->second->pos;
			memmove(p, p + 1, (body->obj->via.map.size - it->second->pos - 1) * sizeof(msgpack::object_kv));
			--body->obj->via.map.size;
			body->map.erase(key);
			return true;
		} else {
			return false;
		}
	}

	throw msgpack::type_error();
}


bool
MsgPack::erase(uint32_t off)
{
	if (body->obj->type == msgpack::type::ARRAY) {
		auto r_size = off + 1;
		if (body->obj->via.array.size >= r_size) {
			auto p = body->obj->via.array.ptr + off;
			memmove(p, p + 1, (body->obj->via.array.size - off - 1) * sizeof(msgpack::object));
			--body->obj->via.array.size;
			return true;
		} else {
			return false;
		}
	}

	throw msgpack::type_error();
}


std::string
MsgPack::to_json_string(bool prettify) const
{
	if (prettify) {
		rapidjson::Document doc = to_json();
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		return std::string(buffer.GetString(), buffer.GetSize());
	} else {
		std::ostringstream oss;
		oss << *body->obj;
		return oss.str();
	}
}


rapidjson::Document
MsgPack::to_json() const
{
	rapidjson::Document doc;
	body->obj->convert(&doc);
	return doc;
}


std::string
MsgPack::to_string() const
{
	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, *body->obj);
	return std::string(sbuf.data(), sbuf.size());
}


void
MsgPack::expand_map(size_t r_size)
{
	if (body->m_alloc == static_cast<ssize_t>(r_size)) {
		size_t nsize = body->m_alloc > 0 ? body->m_alloc * 2 : MSGPACK_MAP_INIT_SIZE;
		while (nsize < r_size) {
			nsize *= 2;
		}

		const msgpack::object_kv* p(body->obj->via.map.ptr);
		const msgpack::object_kv* pend(body->obj->via.map.ptr + body->obj->via.map.size);

		msgpack::detail::unpack_map()(handler->user, static_cast<uint32_t>(nsize), *body->obj);

		// Copy previous memory.
		for ( ; p != pend; ++p) {
			msgpack::detail::unpack_map_item(*body->obj, p->key, p->val);
		}

		body->map.reserve(nsize);
		body->m_alloc = nsize;
	}
}


void
MsgPack::expand_array(size_t r_size)
{
	if (body->m_alloc < static_cast<ssize_t>(r_size)) {
		size_t nsize = body->m_alloc > 0 ? body->m_alloc * 2 : MSGPACK_ARRAY_INIT_SIZE;
		while (nsize < r_size) {
			nsize *= 2;
		}

		const msgpack::object* p(body->obj->via.array.ptr);
		const msgpack::object* pend(body->obj->via.array.ptr + body->obj->via.array.size);

		msgpack::detail::unpack_array()(handler->user, static_cast<uint32_t>(nsize), *body->obj);

		// Copy previous memory.
		for ( ; p != pend; ++p) {
			msgpack::detail::unpack_array_item(*body->obj, *p);
		}

		body->m_alloc = nsize;
	}
}


void
MsgPack::reset(MsgPack&& other) noexcept
{
	handler = std::move(other.handler);
	parent_body = std::move(other.parent_body);
	body = std::move(other.body);
}


void
MsgPack::reset(const MsgPack& other)
{
	handler = other.handler;
	parent_body = other.parent_body;
	body = other.body;
}


MsgPack
MsgPack::path(const std::vector<std::string>& path) const
{
	MsgPack current(*this);
	for (const auto& s : path) {
		switch (current.body->obj->type) {
			case msgpack::type::MAP:
				try {
					current.reset(current.at(s));
				} catch (const std::out_of_range&) {
					throw std::out_of_range("The map must contain an object at key:" + s);
				}
				break;

			case msgpack::type::ARRAY: {
				std::string::size_type sz;
				int pos = std::stoi(s, &sz);
				if (pos < 0 || sz != s.size()) {
					throw std::invalid_argument("The index for the array must be a positive integer, it is: " + s);
				}
				try {
					current.reset(current.at(pos));
				} catch (const std::out_of_range&) {
					throw std::out_of_range(("The array must contain an object at index: " + s).c_str());
				}
				break;
			}

			default:
				throw std::invalid_argument(("The container must be a map or an array to access: " + s).c_str());
		}
	}

	return current;
}


namespace msgpack {
	MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
		namespace adaptor {
			const msgpack::object& convert<MsgPack>::operator()(const msgpack::object& o, MsgPack& v) const {
				v = MsgPack(v.body, std::make_unique<msgpack::zone>());
				return o;
			}

			template <typename Stream>
			msgpack::packer<Stream>& pack<MsgPack>::operator()(msgpack::packer<Stream>& o, const MsgPack& v) const {
				o << v;
				return o;
			}

			void object<MsgPack>::operator()(msgpack::object& o, const MsgPack& v) const {
				msgpack::object obj(*v.body->obj);
				o.type = obj.type;
				o.via = obj.via;
			}

			void object_with_zone<MsgPack>::operator()(msgpack::object::with_zone& o, const MsgPack& v) const {
				msgpack::object obj(*v.body->obj, o.zone);
				o.type = obj.type;
				o.via = obj.via;
			}
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack
