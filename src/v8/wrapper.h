/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "convert.h"


namespace v8pp {

// Generic Wrap.
template <typename T, typename Enabler = void>
struct wrap;


// MsgPack Wrap
template <>
struct wrap<MsgPack> {
	inline v8::Handle<v8::Value> toValue(MsgPack& arg, v8::Persistent<v8::ObjectTemplate> obj_template) const {
		switch (arg.getType()) {
			case MsgPack::Type::MAP: {
				v8::Handle<v8::Object> obj = obj_template->NewInstance();
				obj->SetInternalField(0, v8::External::New(&arg));
				return obj;
			}
			case MsgPack::Type::ARRAY: {
				v8::Handle<v8::Object> obj = obj_template->NewInstance();
				obj->SetInternalField(0, v8::External::New(&arg));
				return obj;
			}
			case MsgPack::Type::STR: {
				auto arg_str = arg.as_string();
				return v8::String::New(arg_str.data(), arg_str.size());
			}
			case MsgPack::Type::POSITIVE_INTEGER:
				return v8::Integer::New(arg.as_u64());
			case MsgPack::Type::NEGATIVE_INTEGER:
				return v8::Integer::New(arg.as_i64());
			case MsgPack::Type::FLOAT:
				return v8::Number::New(arg.as_f64());
			case MsgPack::Type::BOOLEAN:
				return v8::Boolean::New(arg.as_bool());
			case MsgPack::Type::UNDEFINED:
				return v8::Undefined();
			case MsgPack::Type::NIL:
				return v8::Null();
			default:
				return v8::Undefined();
		}
	}

	inline std::string to_string(const v8::AccessorInfo& info) const {
		auto& obj = convert<MsgPack>()(info);
		return obj.to_string();
	}

	inline v8::Handle<v8::Value> getter(const std::string& property, const v8::AccessorInfo& info, v8::Persistent<v8::ObjectTemplate> obj_template) const {
		auto& obj = convert<MsgPack>()(info);
		try {
			return toValue(obj.at(property), obj_template);
		} catch (const std::out_of_range&) {
		} catch (const msgpack::type_error&) {
			if (property == "_value") {
				return toValue(obj, obj_template);
			}
		}
		return v8::Undefined();
	}

	inline v8::Handle<v8::Value> getter(uint32_t index, const v8::AccessorInfo& info, v8::Persistent<v8::ObjectTemplate> obj_template) const {
		auto& obj = convert<MsgPack>()(info);
		try {
			return toValue(obj.at(index), obj_template);
		} catch (const std::out_of_range&) {
		} catch (const msgpack::type_error&) { }
		return v8::Undefined();
	}

	inline void setter(const std::string& property, v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
		auto& obj = convert<MsgPack>()(info);
		try {
			auto &inner_obj = obj[property];
			auto msgpack_value = convert<MsgPack>()(value);
			if (!msgpack_value.is_map()) {
				try {
					inner_obj["_value"] = msgpack_value;
					return;
				} catch (const std::out_of_range&) {
				} catch (const msgpack::type_error&) { }
			}
			inner_obj = msgpack_value;
		} catch (const msgpack::type_error&) {
			if (property == "_value") {
				obj = convert<MsgPack>()(value);
			}
		}
	}

	inline void setter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
		auto& obj = convert<MsgPack>()(info);
		try {
			auto &inner_obj = obj[index];
			auto msgpack_value = convert<MsgPack>()(value);
			if (!msgpack_value.is_map()) {
				try {
					inner_obj["_value"] = msgpack_value;
					return;
				} catch (const std::out_of_range&) {
				} catch (const msgpack::type_error&) { }
			}
			inner_obj = msgpack_value;
		} catch (const msgpack::type_error&) { }
	}

	inline void deleter(const std::string& property, const v8::AccessorInfo& info) {
		auto& obj = convert<MsgPack>()(info);
		obj.erase(property);
	}

	inline void deleter(uint32_t index, const v8::AccessorInfo& info) const {
		auto& obj = convert<MsgPack>()(info);
		obj.erase(index);
	}
};

}; // End namespace v8pp
