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

#include "chaiscript/chaiscript.hpp"

#include "msgpack.hpp"           // for msgpack::object


namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor {

	template<>
	struct convert<chaiscript::Boxed_Value> {
		msgpack::object const& operator()(msgpack::object const& o, chaiscript::Boxed_Value& v) const {
			switch (o.type) {
				case msgpack::type::BOOLEAN:
					v = chaiscript::Boxed_Value(o.via.boolean);
					break;
				case msgpack::type::POSITIVE_INTEGER:
					v = chaiscript::Boxed_Value(o.via.u64);
					break;
				case msgpack::type::NEGATIVE_INTEGER:
					v = chaiscript::Boxed_Value(o.via.i64);
					break;
				case msgpack::type::FLOAT:
					v = chaiscript::Boxed_Value(o.via.f64);
					break;
				case msgpack::type::BIN: // fall through
				case msgpack::type::STR:
					v = chaiscript::Boxed_Value(std::string(o.via.str.ptr, o.via.str.size));
					break;
				case msgpack::type::ARRAY: {
					std::vector<chaiscript::Boxed_Value> vec;
					vec.reserve(o.via.array.size);
					msgpack::object* ptr = o.via.array.ptr;
					msgpack::object* END = ptr + o.via.array.size;
					for (; ptr < END; ++ptr) {
						chaiscript::Boxed_Value val;
						ptr->convert(&val);
						vec.push_back(val);
					}
					v = chaiscript::Boxed_Value(vec);
					break;
				}
				case msgpack::type::MAP: {
					std::map<std::string, chaiscript::Boxed_Value> map;
					msgpack::object_kv* ptr = o.via.map.ptr;
					msgpack::object_kv* END = ptr + o.via.map.size;
					for (; ptr < END; ++ptr) {
						std::string key(ptr->key.via.str.ptr, ptr->key.via.str.size);
						chaiscript::Boxed_Value val;
						ptr->val.convert(&val);
						map.emplace(key, val);
					}
					v = chaiscript::Boxed_Value(map);
					break;
				}
				case msgpack::type::NIL:
					chaiscript::Boxed_Value(nullptr);
				default:
					chaiscript::Boxed_Value();
					break;
			}
			return o;
		}
	};


	template<>
	struct pack<chaiscript::Boxed_Value> {
		template <typename Stream>
		msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, chaiscript::Boxed_Value const& v) const {
			if (v.is_type(chaiscript::user_type<std::map<std::string, chaiscript::Boxed_Value>>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::map<std::string, chaiscript::Boxed_Value>&>(v);
				o.pack_map(cast_val.size());
				for (const auto& pair : cast_val) {
					o.pack_str(pair.first.size()).pack_str_body(pair.first.data(), pair.first.size());
					o.pack(pair.second);
				}
				return o;
			} else if (v.is_type(chaiscript::user_type<std::vector<chaiscript::Boxed_Value>>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
				o.pack_array(cast_val.size());
				for (const auto& val : cast_val) {
					o.pack(val);
				}
				return o;
			} else if (v.is_type(chaiscript::user_type<std::string>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::string&>(v);
				auto size = cast_val.size();
				return o.pack_str(size).pack_str_body(cast_val.data(), size);
			} else if (v.get_type_info().is_arithmetic()) {
				if (v.is_type(chaiscript::user_type<int8_t>())) {
					return o.pack_int8(chaiscript::boxed_cast<int8_t>(v));
				} else if (v.is_type(chaiscript::user_type<int16_t>())) {
					return o.pack_int16(chaiscript::boxed_cast<int16_t>(v));
				} else if (v.is_type(chaiscript::user_type<int32_t>())) {
					return o.pack_int32(chaiscript::boxed_cast<int32_t>(v));
				} else if (v.is_type(chaiscript::user_type<int64_t>())) {
					return o.pack_int64(chaiscript::boxed_cast<int64_t>(v));
				} else if (v.is_type(chaiscript::user_type<char>())) {
					if (v.is_ref()) {
						const char* cast_val = chaiscript::boxed_cast<const char*>(v);
						auto size = std::strlen(cast_val);
						return o.pack_str(size).pack_str_body(cast_val, size);
					} else {
						return o.pack_char(chaiscript::boxed_cast<char>(v));
					}
				} else if (v.is_type(chaiscript::user_type<signed char>())) {
					return o.pack_signed_char(chaiscript::boxed_cast<signed char>(v));
				} else if (v.is_type(chaiscript::user_type<short>())) {
					return o.pack_short(chaiscript::boxed_cast<short>(v));
				} else if (v.is_type(chaiscript::user_type<int>())) {
					return o.pack_int(chaiscript::boxed_cast<int>(v));
				} else if (v.is_type(chaiscript::user_type<long>())) {
					return o.pack_long(chaiscript::boxed_cast<long>(v));
				} else if (v.is_type(chaiscript::user_type<long long>())) {
					return o.pack_long_long(chaiscript::boxed_cast<long long>(v));
				} else if (v.is_type(chaiscript::user_type<uint8_t>())) {
					return o.pack_uint8(chaiscript::boxed_cast<uint8_t>(v));
				} else if (v.is_type(chaiscript::user_type<uint16_t>())) {
					return o.pack_uint16(chaiscript::boxed_cast<uint16_t>(v));
				} else if (v.is_type(chaiscript::user_type<uint32_t>())) {
					return o.pack_uint32(chaiscript::boxed_cast<uint32_t>(v));
				} else if (v.is_type(chaiscript::user_type<uint64_t>())) {
					return o.pack_uint64(chaiscript::boxed_cast<uint64_t>(v));
				} else if (v.is_type(chaiscript::user_type<unsigned char>())) {
					return o.pack_unsigned_char(chaiscript::boxed_cast<unsigned char>(v));
				} else if (v.is_type(chaiscript::user_type<unsigned short>())) {
					return o.pack_unsigned_short(chaiscript::boxed_cast<unsigned short>(v));
				} else if (v.is_type(chaiscript::user_type<unsigned int>())) {
					return o.pack_unsigned_int(chaiscript::boxed_cast<unsigned int>(v));
				} else if (v.is_type(chaiscript::user_type<unsigned long>())) {
					return o.pack_unsigned_long(chaiscript::boxed_cast<unsigned long>(v));
				} else if (v.is_type(chaiscript::user_type<unsigned long long>())) {
					return o.pack_unsigned_long_long(chaiscript::boxed_cast<unsigned long long>(v));
				} else if (v.is_type(chaiscript::user_type<float>())) {
					return o.pack_float(chaiscript::boxed_cast<float>(v));
				} else if (v.is_type(chaiscript::user_type<double>())) {
					return o.pack_double(chaiscript::boxed_cast<double>(v));
				} else if (v.is_type(chaiscript::user_type<bool>())) {
					return chaiscript::boxed_cast<bool>(v) ? o.pack_true() : o.pack_false();
				} else {
					return o;
				}
			} else if (v.is_undef()) {
				o.pack_ext(1, type::EXT);
				char type[1] = { 0 }; // (char)Type::UNDEFINED & MSGPACK_EXT_MASK
				return o.pack_ext_body(type, 1);
			} else if (v.is_null()) {
				return o.pack_nil();
			}
		}
	};


	template<>
	struct object_with_zone<chaiscript::Boxed_Value> {
		void operator()(msgpack::object::with_zone& o, chaiscript::Boxed_Value const& v) const {
			if (v.is_type(chaiscript::user_type<std::map<std::string, chaiscript::Boxed_Value>>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::map<std::string, chaiscript::Boxed_Value>&>(v);
				o.type = type::MAP;
				if (cast_val.empty()) {
					o.via.map.ptr = nullptr;
					o.via.map.size = 0;
				} else {
					auto size = cast_val.size();
					object_kv* p = (object_kv*)o.zone.allocate_align(sizeof(object_kv) * size);
					o.via.map.ptr = p;
					o.via.map.size = size;
					for (const auto& pair : cast_val) {
						p->key = msgpack::object(pair.first, o.zone);
						p->val = msgpack::object(pair.second, o.zone);
						++p;
					}
				}
			} else if (v.is_type(chaiscript::user_type<std::vector<chaiscript::Boxed_Value>>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
				o.type = type::ARRAY;
				if (cast_val.empty()) {
					o.via.array.ptr = nullptr;
					o.via.array.size = 0;
				} else {
					auto size = cast_val.size();
					msgpack::object* p = (msgpack::object*)o.zone.allocate_align(sizeof(msgpack::object) * size);
					o.via.array.ptr = p;
					o.via.array.size = size;
					for (const auto& val : cast_val) {
						*p = msgpack::object(val, o.zone);
						++p;
					}
				}
			} else if (v.is_type(chaiscript::user_type<std::string>())) {
				const auto& cast_val = chaiscript::boxed_cast<const std::string&>(v);
				o.type = type::STR;
				auto size = cast_val.size();
				char* ptr = (char*)o.zone.allocate_align(size);
				memcpy(ptr, cast_val.data(), size);
				o.via.str.ptr = ptr;
				o.via.str.size = size;
			} else if (v.get_type_info().is_arithmetic()) {
				if (v.is_type(chaiscript::user_type<int8_t>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<int8_t>(v);
				} else if (v.is_type(chaiscript::user_type<int16_t>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<int16_t>(v);
				} else if (v.is_type(chaiscript::user_type<int32_t>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<int32_t>(v);
				} else if (v.is_type(chaiscript::user_type<int64_t>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<int64_t>(v);
				} else if (v.is_type(chaiscript::user_type<char>())) {
					if (v.is_ref()) {
						const char* _ptr = chaiscript::boxed_cast<const char*>(v);
						o.type = type::STR;
						auto size = std::strlen(_ptr);
						char* ptr = (char*)o.zone.allocate_align(size);
						memcpy(ptr, _ptr, size);
						o.via.str.ptr = ptr;
						o.via.str.size = size;
					} else {
						o.type = type::NEGATIVE_INTEGER;
						o.via.i64 = chaiscript::boxed_cast<char>(v);
					}
				} else if (v.is_type(chaiscript::user_type<signed char>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<signed char>(v);
				} else if (v.is_type(chaiscript::user_type<short>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<short>(v);
				} else if (v.is_type(chaiscript::user_type<int>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<int>(v);
				} else if (v.is_type(chaiscript::user_type<long>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<long>(v);
				} else if (v.is_type(chaiscript::user_type<long long>())) {
					o.type = type::NEGATIVE_INTEGER;
					o.via.i64 = chaiscript::boxed_cast<long long>(v);
				} else if (v.is_type(chaiscript::user_type<uint8_t>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<uint8_t>(v);
				} else if (v.is_type(chaiscript::user_type<uint16_t>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<uint16_t>(v);
				} else if (v.is_type(chaiscript::user_type<uint32_t>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<uint32_t>(v);
				} else if (v.is_type(chaiscript::user_type<uint64_t>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<uint64_t>(v);
				} else if (v.is_type(chaiscript::user_type<unsigned char>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<unsigned char>(v);
				} else if (v.is_type(chaiscript::user_type<unsigned short>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<unsigned short>(v);
				} else if (v.is_type(chaiscript::user_type<unsigned int>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<unsigned int>(v);
				} else if (v.is_type(chaiscript::user_type<unsigned long>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<unsigned long>(v);
				} else if (v.is_type(chaiscript::user_type<unsigned long long>())) {
					o.type = type::POSITIVE_INTEGER;
					o.via.u64 = chaiscript::boxed_cast<unsigned long long>(v);
				} else if (v.is_type(chaiscript::user_type<float>())) {
					o.type = type::FLOAT;
					o.via.f64 = chaiscript::boxed_cast<float>(v);
				} else if (v.is_type(chaiscript::user_type<double>())) {
					o.type = type::FLOAT;
					o.via.f64 = chaiscript::boxed_cast<double>(v);
				} else if (v.is_type(chaiscript::user_type<bool>())) {
					o.type = type::BOOLEAN;
					o.via.boolean = chaiscript::boxed_cast<bool>(v);
				}
			} else if (v.is_undef()) {
				char* ptr = static_cast<char*>(o.zone.allocate_align(1));
				o.type = type::EXT;
				o.via.ext.ptr = ptr;
				o.via.ext.size = 1;
				ptr[0] = (char)0; // Type::UNDEFINED & MSGPACK_EXT_MASK;
			} else if (v.is_null()) {
				o.type = type::NIL;
			}
		}
	};
}}}
