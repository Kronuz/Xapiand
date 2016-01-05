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

#include "msgpack_wrapper.h"


MsgPack::MsgPack(const std::shared_ptr<object_handle>& unpacked, msgpack::object& o)
	: handler(unpacked),
	  obj(o) { }


MsgPack::MsgPack(const msgpack::object& o, std::unique_ptr<msgpack::zone>&& z)
	: handler(std::make_shared<object_handle>(o, std::move(z))),
	  obj(handler->obj) { }


MsgPack::MsgPack(msgpack::unpacked& u)
	: handler(std::make_shared<object_handle>(u.get(), std::move(u.zone()))),
	  obj(handler->obj) { }


MsgPack::MsgPack(const std::string& buffer)
	: handler(make_handler(buffer)),
	  obj(handler->obj) { }


MsgPack
MsgPack::operator[](const MsgPack& o)
{
	if (o.obj.type == msgpack::type::STR) {
		return operator[](std::string(o.obj.via.str.ptr, o.obj.via.str.size));
	}
	if (o.obj.type == msgpack::type::POSITIVE_INTEGER) {
		return operator[](static_cast<int32_t>(o.obj.via.u64));
	}
	throw msgpack::type_error();
}


MsgPack
MsgPack::operator[](const std::string& name)
{
	if (obj.type == msgpack::type::NIL) {
		obj.type = msgpack::type::MAP;
		obj.via.map.ptr = nullptr;
		obj.via.map.size = 0;
	}

	if (obj.type == msgpack::type::MAP) {
		msgpack::object_kv* p(obj.via.map.ptr);
		const msgpack::object_kv* pend(obj.via.map.ptr + obj.via.map.size);
		for ( ; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) {
				// std::cout << std::string(p->key.via.str.ptr, p->key.via.str.size) << std::endl;
				if (!name.compare(p->key.via.str.ptr)) {
					return MsgPack(handler, p->val);
				}
			}
		}

		p = obj.via.map.ptr;

		msgpack::detail::unpack_map()(handler->user, obj.via.map.size + 1, obj);
		msgpack::object_kv* np(obj.via.map.ptr);

		for ( ; p != pend; ++p, ++np) {
			msgpack::detail::unpack_map_item(obj, p->key, p->val);
		}

		msgpack::detail::unpack_str(handler->user, name.data(), name.size(), np->key);
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
			msgpack::object* p(obj.via.array.ptr);
			const msgpack::object* pend(obj.via.array.ptr + obj.via.array.size);

			msgpack::detail::unpack_array()(handler->user, off + 1, obj);

			msgpack::object* np(obj.via.array.ptr);
			const msgpack::object* npend(obj.via.array.ptr + off + 1);

			for ( ; p != pend; ++p, ++np) {
				msgpack::detail::unpack_array_item(obj, *p);
			}

			for ( ; np != npend; ++np) {
				msgpack::detail::unpack_nil(*np);
				msgpack::detail::unpack_array_item(obj, *np);
			}
		}

		return MsgPack(handler, obj.via.array.ptr[off]);
	}

	throw msgpack::type_error();
}


std::shared_ptr<MsgPack::object_handle>
MsgPack::make_handler(const std::string& buffer)
{
	msgpack::unpacked u;
	msgpack::unpack(&u, buffer.data(), buffer.size());
	return std::make_shared<MsgPack::object_handle>(u.get(), msgpack::move(u.zone()));
}


// ONLINE TEST
#include <iostream>
#include <fstream>


bool read_file_contents(const std::string& filename, std::string* contents) {
	std::ifstream in(filename.data(), std::ios::in | std::ios::binary);
	if (in.bad()) {
		return false;
	}

	in.seekg(0, std::ios::end);
	contents->resize(static_cast<size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&(*contents)[0], contents->size());
	in.close();
	return true;
}


int main() {
	std::string buffer;

	if (!read_file_contents("/Users/josemaria/Downloads/xchange/build/test2.mpack", &buffer)) {
		return false;
	}

	MsgPack obj(buffer);

	obj["name"]["middle"]["other"] = "Jeremy";
	obj["range"][30] = "Other";

	for (auto x : obj) {
		std::cout << x << ":" << obj[x] << std::endl;
	}

	std::cout << obj << std::endl;
	msgpack::sbuffer sbuf;  // simple buffer
	msgpack::pack(&sbuf, obj.obj);
	std::cout << std::string(sbuf.data(), sbuf.size()) << std::endl;
#if defined(MSGPACK_USE_CPP03)
	std::cout << "It is running c++03" << std::endl;
#else
	std::cout << "It is running c++11" << std::endl;
#endif
}
