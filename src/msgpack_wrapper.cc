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

#include "exception.h"
#include "log.h"
#include "utils.h"


MsgPack::MsgPack()
	: handler(std::make_shared<object_handle>()),
	  parent_obj(nullptr),
	  obj(&handler->obj) { }


MsgPack::MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object* o, msgpack::object* p)
	: handler(unpacked),
	  parent_obj(p),
	  obj(o) { }


MsgPack::MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z)
	: handler(std::make_shared<object_handle>(o, std::move(z))),
	  parent_obj(nullptr),
	  obj(&handler->obj) { }


MsgPack::MsgPack(msgpack::unpacked& u)
	: handler(std::make_shared<object_handle>(u.get(), std::move(u.zone()))),
	  parent_obj(nullptr),
	  obj(&handler->obj) { }


MsgPack::MsgPack(const std::string& buffer)
	: handler(make_handler(buffer)),
	  parent_obj(nullptr),
	  obj(&handler->obj) { }


MsgPack::MsgPack(const rapidjson::Document& doc)
	: handler(make_handler(doc)),
	  parent_obj(nullptr),
	  obj(&handler->obj) { }


MsgPack::MsgPack(MsgPack&& other) noexcept
	: handler(std::move(other.handler)),
	  parent_obj(std::move(other.parent_obj)),
	  obj(std::move(other.obj)) { }


MsgPack::MsgPack(const MsgPack& other)
	: handler(other.handler),
	  parent_obj(other.parent_obj),
	  obj(other.obj) { }


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
	if (o.obj->type == msgpack::type::STR) {
		return operator[](std::string(o.obj->via.str.ptr, o.obj->via.str.size));
	}
	if (o.obj->type == msgpack::type::POSITIVE_INTEGER) {
		return operator[](static_cast<uint32_t>(o.obj->via.u64));
	}
	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](const std::string& key)
{
	if (obj->type == msgpack::type::NIL) {
		obj->type = msgpack::type::MAP;
		obj->via.map.ptr = nullptr;
		obj->via.map.size = 0;
		obj->via.map.m_alloc = 0;
	}

	if (obj->type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(obj->via.map.ptr + obj->via.map.size);
		for (auto p = obj->via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				if (key.compare(std::string(p->key.via.str.ptr, p->key.via.str.size)) == 0) {
					return MsgPack(handler, &p->val, obj);
				}
			}
		}

		expand_map();
		msgpack::object_kv* np(obj->via.map.ptr + obj->via.map.size);

		msgpack::detail::unpack_str(handler->user, key.data(), (uint32_t)key.size(), np->key);
		msgpack::detail::unpack_nil(np->val);
		msgpack::detail::unpack_map_item(*obj, np->key, np->val);

		return MsgPack(handler, &np->val, obj);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](uint32_t off)
{
	if (obj->type == msgpack::type::NIL) {
		obj->type = msgpack::type::ARRAY;
		obj->via.array.ptr = nullptr;
		obj->via.array.size = 0;
		obj->via.array.m_alloc = 0;
	}

	if (obj->type == msgpack::type::ARRAY) {
		auto r_size = off + 1;
		if (obj->via.array.size < r_size) {
			expand_array(r_size);

			// Initialize new elements.
			const msgpack::object* npend(obj->via.array.ptr + r_size);
			for (auto np = obj->via.array.ptr + obj->via.array.size; np != npend; ++np) {
				msgpack::detail::unpack_nil(*np);
				msgpack::detail::unpack_array_item(*obj, *np);
			}
		}

		return MsgPack(handler, &obj->via.array.ptr[off], obj);
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::at(const MsgPack& o) const
{
	if (o.obj->type == msgpack::type::STR) {
		return at(std::string(o.obj->via.str.ptr, o.obj->via.str.size));
	}
	if (o.obj->type == msgpack::type::POSITIVE_INTEGER) {
		return at(static_cast<uint32_t>(o.obj->via.u64));
	}
	throw msgpack::type_error();
}


MsgPack
MsgPack::at(const std::string& key) const
{
	if (obj->type == msgpack::type::NIL) {
		throw std::out_of_range(key);
	}

	if (obj->type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(obj->via.map.ptr + obj->via.map.size);
		for (auto p = obj->via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				if (key.compare(std::string(p->key.via.str.ptr, p->key.via.str.size)) == 0)  {
					return MsgPack(handler, &p->val, obj);
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
	if (obj->type == msgpack::type::NIL) {
		throw std::out_of_range(std::to_string(off));
	}

	if (obj->type == msgpack::type::ARRAY) {
		if (off < obj->via.array.size) {
			return MsgPack(handler, &obj->via.array.ptr[off], obj);
		}

		throw std::out_of_range(std::to_string(off));
	}

	throw msgpack::type_error();
}


bool
MsgPack::find(const MsgPack& o) const
{
	if (o.obj->type == msgpack::type::STR) {
		return find(std::string(o.obj->via.str.ptr, o.obj->via.str.size));
	}

	if (o.obj->type == msgpack::type::POSITIVE_INTEGER) {
		return find(static_cast<uint32_t>(o.obj->via.u64));
	}

	return false;
}


bool
MsgPack::find(const std::string& key) const
{
	if (obj->type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(obj->via.map.ptr + obj->via.map.size);
		for (auto p = obj->via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				if (key.compare(std::string(p->key.via.str.ptr, p->key.via.str.size)) == 0)  {
					return true;
				}
			}
		}
	}

	return false;
}


bool
MsgPack::find(uint32_t off) const
{
	if (obj->type == msgpack::type::ARRAY && off < obj->via.array.size) {
		return true;
	}

	return false;
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
		oss << *obj;
		return oss.str();
	}
}


rapidjson::Document
MsgPack::to_json() const
{
	rapidjson::Document doc;
	obj->convert(&doc);
	return doc;
}


std::string
MsgPack::to_string() const
{
	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, *obj);
	return std::string(sbuf.data(), sbuf.size());
}


void
MsgPack::expand_map()
{
	if (obj->via.map.m_alloc == obj->via.map.size) {
		unsigned nsize = obj->via.map.m_alloc > 0 ? obj->via.map.m_alloc * 2 : MSGPACK_MAP_INIT_SIZE;

		const msgpack::object_kv* p(obj->via.map.ptr);
		const msgpack::object_kv* pend(obj->via.map.ptr + obj->via.map.size);

		msgpack::detail::unpack_map()(handler->user, nsize, *obj);

		// Copy previous memory.
		for ( ; p != pend; ++p) {
			msgpack::detail::unpack_map_item(*obj, p->key, p->val);
		}

		obj->via.map.m_alloc = nsize;
	}
}


void
MsgPack::expand_array(size_t r_size)
{
	if (obj->via.array.m_alloc < r_size) {
		size_t nsize = obj->via.array.m_alloc > 0 ? obj->via.array.m_alloc * 2 : MSGPACK_ARRAY_INIT_SIZE;
		while (nsize < r_size) {
			nsize *= 2;
		}

		const msgpack::object* p(obj->via.array.ptr);
		const msgpack::object* pend(obj->via.array.ptr + obj->via.array.size);

		msgpack::detail::unpack_array()(handler->user, nsize, *obj);

		// Copy previous memory.
		for ( ; p != pend; ++p) {
			msgpack::detail::unpack_array_item(*obj, *p);
		}

		obj->via.array.m_alloc = nsize;
	}
}


size_t
MsgPack::capacity() const noexcept
{
	switch (obj->type) {
		case msgpack::type::MAP:
			return obj->via.map.m_alloc;
			break;
		case msgpack::type::ARRAY:
			return obj->via.array.m_alloc;
			break;
		default:
			return 0;
	}
}


bool
MsgPack::erase(const std::string& key)
{
	if (obj->type == msgpack::type::MAP) {
		size_t size = obj->via.map.size;
		const msgpack::object_kv* pend(obj->via.map.ptr + obj->via.map.size);
		for (auto p = obj->via.map.ptr; p != pend; ++p) {
			--size;
			if (p->key.type == msgpack::type::STR) {
				if (key.compare(std::string(p->key.via.str.ptr, p->key.via.str.size)) == 0) {
					memcpy(p, p + 1, size * sizeof(msgpack::object_kv));
					--obj->via.map.size;
					return true;
				}
			}
		}
		return false;
	}

	throw msgpack::type_error();
}


MsgPack
MsgPack::duplicate() const
{
	return MsgPack(to_string());
}


MsgPack
MsgPack::path(const std::vector<std::string> &path)
{
	MsgPack current(*this);
	for (const auto& s : path) {
		if (current.obj->type == msgpack::type::MAP) {
			current = current.at(s);
		} else if (current.obj->type == msgpack::type::ARRAY) {
			current = current.at(strict_stoi(s));
		} else {
			throw msgpack::type_error();
		}
	}
	return current;
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
				msgpack::object obj(*v.obj);
				o.type = obj.type;
				o.via = obj.via;
			}

			void object_with_zone<MsgPack>::operator()(msgpack::object::with_zone& o, const MsgPack& v) const {
				msgpack::object obj(*v.obj, o.zone);
				o.type = obj.type;
				o.via = obj.via;
			}
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack
