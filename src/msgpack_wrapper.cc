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

#include <sstream>

#include "msgpack_wrapper.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "xchange/rapidjson.hpp"

#include "log.h"


MsgPack::MsgPack()
	: handler(std::make_shared<object_handle>()),
	  m_alloc(0),
	  obj(handler->obj) { }


MsgPack::MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object& o)
	: handler(unpacked),
	  m_alloc(o.type == msgpack::type::MAP ? o.via.map.size : 0),
	  obj(o) { }


MsgPack::MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z)
	: handler(std::make_shared<object_handle>(o, std::move(z))),
	  m_alloc(o.type == msgpack::type::MAP ? o.via.map.size : 0),
	  obj(handler->obj) { }


MsgPack::MsgPack(msgpack::unpacked& u)
	: handler(std::make_shared<object_handle>(u.get(), std::move(u.zone()))),
	  m_alloc(handler->obj.type == msgpack::type::MAP ? handler->obj.via.map.size : 0),
	  obj(handler->obj) { }


MsgPack::MsgPack(const std::string& buffer)
	: handler(make_handler(buffer)),
	  m_alloc(handler->obj.type == msgpack::type::MAP ? handler->obj.via.map.size : 0),
	  obj(handler->obj) { }


MsgPack::MsgPack(const rapidjson::Document& doc)
	: handler(make_handler(doc)),
	  m_alloc(handler->obj.type == msgpack::type::MAP ? handler->obj.via.map.size : 0),
	  obj(handler->obj) { }


MsgPack::MsgPack(MsgPack&& other) noexcept
	: handler(std::move(other.handler)),
	  m_alloc(std::move(other.m_alloc)),
	  obj(handler->obj) { }


MsgPack::MsgPack(const MsgPack& other)
	: handler(other.handler),
	  m_alloc(other.m_alloc),
	  obj(handler->obj) { }


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


MsgPack
MsgPack::operator[](const MsgPack& o)
{
	if (o.obj.type == msgpack::type::STR) {
		return operator[](std::string(o.obj.via.str.ptr, o.obj.via.str.size));
	}
	if (o.obj.type == msgpack::type::POSITIVE_INTEGER) {
		return operator[](static_cast<uint32_t>(o.obj.via.u64));
	}
	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](const std::string& key)
{
	if (obj.type == msgpack::type::NIL) {
		obj.type = msgpack::type::MAP;
		obj.via.map.ptr = nullptr;
		obj.via.map.size = 0;
	}

	if (obj.type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(obj.via.map.ptr + obj.via.map.size);
		for (auto p = obj.via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				if (key.compare(std::string(p->key.via.str.ptr, p->key.via.str.size)) == 0) {
					return MsgPack(handler, p->val);
				}
			}
		}

		expand_map();
		auto np = obj.via.map.ptr + obj.via.map.size;

		msgpack::detail::unpack_str(handler->user, key.data(), (uint32_t)key.size(), np->key);
		msgpack::detail::unpack_nil(np->val);
		msgpack::detail::unpack_map_item(obj, np->key, np->val);

		return MsgPack(handler, np->val);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](uint32_t off)
{
	if (obj.type == msgpack::type::NIL) {
		obj.type = msgpack::type::ARRAY;
		obj.via.array.ptr = nullptr;
		obj.via.array.size = 0;
	}

	if (obj.type == msgpack::type::ARRAY) {
		if (obj.via.array.size < off + 1) {
			const msgpack::object* p(obj.via.array.ptr);
			const msgpack::object* pend(obj.via.array.ptr + obj.via.array.size);

			msgpack::detail::unpack_array()(handler->user, off + 1, obj);

			msgpack::object* np(obj.via.array.ptr);
			const msgpack::object* npend(obj.via.array.ptr + off + 1);

			// Copy previous memory.
			for ( ; p != pend; ++p, ++np) {
				msgpack::detail::unpack_array_item(obj, *p);
			}

			// Initialize new elements.
			for ( ; np != npend; ++np) {
				msgpack::detail::unpack_nil(*np);
				msgpack::detail::unpack_array_item(obj, *np);
			}
		}

		return MsgPack(handler, obj.via.array.ptr[off]);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::at(const MsgPack& o) const
{
	if (o.obj.type == msgpack::type::STR) {
		return at(std::string(o.obj.via.str.ptr, o.obj.via.str.size));
	}
	if (o.obj.type == msgpack::type::POSITIVE_INTEGER) {
		return at(static_cast<uint32_t>(o.obj.via.u64));
	}
	throw msgpack::type_error();
}


MsgPack
MsgPack::at(const std::string& key) const
{
	if (obj.type == msgpack::type::NIL) {
		throw std::out_of_range(key);
	}

	if (obj.type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(obj.via.map.ptr + obj.via.map.size);
		for (auto p = obj.via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				if (key == std::string(p->key.via.str.ptr, p->key.via.str.size)) {
					return MsgPack(handler, p->val);
				}
			}
		}

		throw std::out_of_range(key);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::at(uint32_t off) const
{
	if (obj.type == msgpack::type::NIL) {
		throw std::out_of_range(std::to_string(off));
	}

	if (obj.type == msgpack::type::ARRAY) {
		if (off < obj.via.array.size) {
			return MsgPack(handler, obj.via.array.ptr[off]);
		}

		throw std::out_of_range(std::to_string(off));
	}

	throw msgpack::type_error();
}


std::string
MsgPack::to_json_string(bool prettify)
{
	if (prettify) {
		rapidjson::Document doc = to_json();
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		return std::string(buffer.GetString(), buffer.GetSize());
	} else {
		std::ostringstream oss;
		oss << obj;
		return oss.str();
	}
}


rapidjson::Document
MsgPack::to_json()
{
	rapidjson::Document doc;
	obj.convert(&doc);
	return doc;
}


std::string
MsgPack::to_string()
{
	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, obj);
	return std::string(sbuf.data(), sbuf.size());
}


void
MsgPack::expand_map()
{
	if (m_alloc == obj.via.map.size) {
		size_t nsize = m_alloc > 0 ? m_alloc * 2 : MSGPACK_MAP_INIT_SIZE;

		const msgpack::object_kv* p(obj.via.map.ptr);
		const msgpack::object_kv* pend(obj.via.map.ptr + obj.via.map.size);

		msgpack::detail::unpack_map()(handler->user, nsize, obj);

		// Copy previous memory.
		for ( ; p != pend; ++p) {
			msgpack::detail::unpack_map_item(obj, p->key, p->val);
		}

		m_alloc = nsize;
	}
}


namespace msgpack {
	MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
		namespace adaptor {
			const msgpack::object& convert<MsgPack>::operator()(const msgpack::object& o, MsgPack& v) const {
				v = MsgPack(o, std::make_unique<msgpack::zone>());
				return o;
			}

			template <typename Stream>
			msgpack::packer<Stream>& pack<MsgPack>::operator()(msgpack::packer<Stream>& o, const MsgPack& v) const {
				o << v;
				return o;
			}

			void object<MsgPack>::operator()(msgpack::object& o, const MsgPack& v) const {
				msgpack::object obj(v.obj);
				o.type = obj.type;
				o.via = obj.via;
			}

			void object_with_zone<MsgPack>::operator()(msgpack::object::with_zone& o, const MsgPack& v) const {
				msgpack::object obj(v.obj, o.zone);
				o.type = obj.type;
				o.via = obj.via;
			}
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack
