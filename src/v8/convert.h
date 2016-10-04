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

#if XAPIAND_V8

#include "../msgpack.h"
#include "exception.h"

#include <v8.h>


namespace v8pp {


// Generic converter.
template <typename T, typename Enabler = void>
struct convert;


// String converter.
template <typename charT, typename traits, typename Alloc>
struct convert<std::basic_string<charT, traits, Alloc>> {
private:
	const charT* to_cstr(const v8::String::Utf8Value& value) const {
		return *value ? reinterpret_cast<const charT*>(*value) : "<string conversion failed>";
	}

	using value_type = std::basic_string<charT, traits, Alloc>;

public:
	v8::Handle<v8::String> operator()(const v8::Arguments& args) const {
		return v8::String::New(to_cstr(v8::String::Utf8Value(args.Data())));
	}

	value_type operator()(const v8::String::Utf8Value& value) const {
		return to_cstr(value);
	}

	value_type operator()(v8::Handle<v8::Value> value) const {
		return to_cstr(v8::String::Utf8Value(value));
	}
};


// Const char* converter
template <typename charT>
struct convert<const charT*> {
private:
	inline const charT* to_cstr(const v8::String::Utf8Value& value) const {
		return *value ? reinterpret_cast<const charT*>(*value) : "<string conversion failed>";
	}

public:
	v8::Handle<v8::String> operator()(const v8::Arguments& args) const {
		return v8::String::New(to_cstr(v8::String::Utf8Value(args.Data())));
	}

	const charT* operator()(const v8::String::Utf8Value& value) const {
		return to_cstr(value);
	}

	const charT* operator()(v8::Handle<v8::Value> value) const {
		return to_cstr(v8::String::Utf8Value(value));
	}
};


// MsgPack converter.
template <>
struct convert<MsgPack> {
private:
	static inline void process(MsgPack& o, v8::Handle<v8::Value> v, std::vector<v8::Local<v8::Object>>& visitObjects) {
		if (v->IsBoolean()) {
			o = v->BooleanValue();
		} else if (v->IsInt32() || v->IsUint32()) {
			msgpack::object mo(-1);  // Ugly hack to force msgpack to get V8 Value as signed integer
			mo.via.i64 = v->IntegerValue();
			o = mo;
		} else if (v->IsNumber()) {
			o = v->NumberValue();
		} else if (v->IsString()) {
			o = convert<std::string>()(v);
		} else if (v->IsArray()) {
			auto o_v8 = v->ToObject();
			auto length = o_v8->Get(v8::String::New("length"))->Uint32Value();
			for (size_t i = 0; i < length; ++i) {
				process(o[i], o_v8->Get(i), visitObjects);
			}
		} else if (v->IsObject()) {
			auto o_v8 = v->ToObject();
			if (std::find(visitObjects.begin(), visitObjects.end(), o_v8) == visitObjects.end()) {
				visitObjects.push_back(o_v8);
				auto properties = o_v8->GetPropertyNames();
				auto length = properties->ToObject()->Get(v8::String::New("length"))->Uint32Value();
				for (size_t i = 0; i < length; ++i) {
					process(o[convert<std::string>()(properties->Get(i))], o_v8->Get(properties->Get(i)), visitObjects);
				}
			} else {
				throw CycleDetectionError();
			}
		} else if (v->IsUndefined()) {
			o = MsgPack();
		} else {
			o = convert<std::string>()(v);
		}
	}

public:
	void operator()(MsgPack& obj, const v8::Handle<v8::Value>& value) const {
		std::vector<v8::Local<v8::Object>> visitObjects;
		process(obj, value, visitObjects);
	}

	MsgPack operator()(v8::Handle<v8::Value> val) const {
		if (val->IsObject()) {
			auto o_v8 = v8::Handle<v8::Object>::Cast(val);
			if (o_v8->InternalFieldCount() == 1) {
				auto field = v8::Handle<v8::External>::Cast(o_v8->GetInternalField(0));
				return *(static_cast<const MsgPack*>(field->Value()));
			}
		}

		MsgPack res;
		std::vector<v8::Local<v8::Object>> visitObjects;
		process(res, val, visitObjects);
		return res;
	}

	MsgPack& operator()(const v8::AccessorInfo& info) const {
		auto field = v8::Handle<v8::External>::Cast(info.Holder()->GetInternalField(0));
		return *(static_cast<MsgPack*>(field->Value()));
	}
};

}; // End namespace v8pp

#endif
