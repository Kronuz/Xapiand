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

#include "config.h"            // for XAPIAND_V8

#ifdef XAPIAND_V8

#include "convert.h"


namespace v8pp {

// Generic Wrap.
template <typename T, typename Enabler = void>
struct wrap;


// MsgPack Wrap
template <>
struct wrap<MsgPack> {
	convert<MsgPack> convertor;

	v8::Local<v8::Value> toValue(v8::Isolate* isolate, const MsgPack& arg, const v8::Local<v8::ObjectTemplate>& obj_template) const {
		switch (arg.getType()) {
			case MsgPack::Type::MAP: {
				v8::Local<v8::Object> obj = obj_template->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
				obj->SetInternalField(0, v8::External::New(isolate, const_cast<MsgPack*>(&arg)));
				return obj;
			}
			case MsgPack::Type::ARRAY: {
				v8::Local<v8::Object> obj = obj_template->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
				obj->SetInternalField(0, v8::External::New(isolate, const_cast<MsgPack*>(&arg)));
				return obj;
			}
			case MsgPack::Type::STR: {
				auto arg_str = arg.str();
				return v8::String::NewFromUtf8(isolate, arg_str.data(), v8::NewStringType::kNormal, arg_str.size()).ToLocalChecked();
			}
			case MsgPack::Type::POSITIVE_INTEGER:
				return v8::Integer::New(isolate, arg.u64());
			case MsgPack::Type::NEGATIVE_INTEGER:
				return v8::Integer::New(isolate, arg.i64());
			case MsgPack::Type::FLOAT:
				return v8::Number::New(isolate, arg.f64());
			case MsgPack::Type::BOOLEAN:
				return v8::Boolean::New(isolate, arg.boolean());
			case MsgPack::Type::UNDEFINED:
				return v8::Undefined(isolate);
			case MsgPack::Type::NIL:
				return v8::Null(isolate);
			default:
				return v8::Undefined(isolate);
		}
	}

	v8::Local<v8::Value> toString(const v8::PropertyCallbackInfo<v8::Value>& info) const {
		auto isolate = info.GetIsolate();
		const auto& obj = convertor(info);
		auto str = obj.to_string();
		return v8::String::NewFromUtf8(isolate, str.data(), v8::NewStringType::kNormal, str.size()).ToLocalChecked();
	}

	v8::Local<v8::Value> getter(const std::string& property, const v8::PropertyCallbackInfo<v8::Value>& info, const v8::Local<v8::ObjectTemplate>& obj_template) const {
		auto isolate = info.GetIsolate();
		const auto& obj = convertor(info);
		try {
			return toValue(isolate, obj.at(property), obj_template);
		} catch (const std::out_of_range&) {
		} catch (const msgpack::type_error&) {
			if (property == RESERVED_VALUE) {
				return toValue(isolate, obj, obj_template);
			}
		}
		return v8::Undefined(isolate);
	}

	v8::Local<v8::Value> getter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info, const v8::Local<v8::ObjectTemplate>& obj_template) const {
		auto isolate = info.GetIsolate();
		const auto& obj = convertor(info);
		try {
			return toValue(isolate, obj.at(index), obj_template);
		} catch (const std::out_of_range&) {
		} catch (const msgpack::type_error&) { }
		return v8::Undefined(isolate);
	}

	void setter(const std::string& property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
		auto& obj = convertor(info);
		try {
			auto &inner_obj = obj[property];
			auto msgpack_value = convertor(value);
			if (!msgpack_value.is_map() && property != RESERVED_VALUE) {
				try {
					inner_obj[RESERVED_VALUE] = msgpack_value;
					return;
				} catch (const msgpack::type_error&) { }
			}
			inner_obj = msgpack_value;
		} catch (const msgpack::type_error&) {
			if (property == RESERVED_VALUE) {
				obj = convertor(value);
			}
		}
	}

	void setter(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
		auto& obj = convertor(info);
		try {
			auto &inner_obj = obj[index];
			auto msgpack_value = convertor(value);
			if (!msgpack_value.is_map()) {
				try {
					inner_obj[RESERVED_VALUE] = msgpack_value;
					return;
				} catch (const msgpack::type_error&) { }
			}
			inner_obj = msgpack_value;
		} catch (const msgpack::type_error&) { }
	}

	void deleter(const std::string& property, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
		auto& obj = convertor(info);
		try {
			obj.erase(property);
		} catch (const msgpack::type_error&) { }
	}

	void deleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) const {
		auto& obj = convertor(info);
		try {
			obj.erase(index);
		} catch (const msgpack::type_error&) { }
	}

	v8::Local<v8::Array> enumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
		auto isolate = info.GetIsolate();
		const auto& obj = convertor(info);
		switch (obj.getType()) {
			case MsgPack::Type::MAP: {
				v8::Local<v8::Array> result = v8::Array::New(isolate, obj.size());
				int i = 0;
				for (const auto& key : obj) {
					result->Set(i++, v8::String::NewFromUtf8(isolate, key.str().c_str(), v8::NewStringType::kNormal).ToLocalChecked());
				}
				return result;
			}
			case MsgPack::Type::ARRAY: {
				auto size = obj.size();
				v8::Local<v8::Array> result = v8::Array::New(isolate, size);
				for (size_t i = 0; i < size; ++i) {
					result->Set(i, v8::Integer::New(isolate, i));
				}
				return result;
			}
			default:
				return v8::Array::New(isolate, 0);
		}
	}
};

}; // End namespace v8pp

#endif
