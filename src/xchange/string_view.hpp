/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include <string_view>           // for std::string_view

#include "msgpack.hpp"           // for msgpack::object


namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor {

	template <>
	struct convert<std::string_view> {
		msgpack::object const& operator()(msgpack::object const& o, std::string_view& v) const {
			switch (o.type) {
			case msgpack::type::BIN:
				v = std::string_view(o.via.bin.ptr, o.via.bin.size);
				break;
			case msgpack::type::STR:
				v = std::string_view(o.via.str.ptr, o.via.str.size);
				break;
			default:
				THROW(msgpack::type_error);
				break;
			}
			return o;
		}
	};

	template <>
	struct pack<std::string_view> {
		template <typename Stream>
		msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::string_view& v) const {
			uint32_t size = checked_get_container_size(v.size());
			o.pack_str(size);
			o.pack_str_body(v.data(), size);
			return o;
		}
	};

	template <>
	struct object<std::string_view> {
		void operator()(msgpack::object& o, const std::string_view& v) const {
			uint32_t size = checked_get_container_size(v.size());
			o.type = msgpack::type::STR;
			o.via.str.ptr = v.data();
			o.via.str.size = size;
		}
	};

	template <>
	struct object_with_zone<std::string_view> {
		void operator()(msgpack::object::with_zone& o, const std::string_view& v) const {
			uint32_t size = checked_get_container_size(v.size());
			o.type = msgpack::type::STR;
			char* ptr = static_cast<char*>(o.zone.allocate_align(size));
			o.via.str.ptr = ptr;
			o.via.str.size = size;
			std::memcpy(ptr, v.data(), v.size());
		}
	};

}}}
