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

#include "wrapper.h"

#include <cassert>
#include <iostream>
#include <map>
#include <array>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace v8pp {

// v8 version supported: v8-315

static size_t hash(const std::string& source) {
	std::hash<std::string> hash_fn;
	return hash_fn(source);
}


static void report_exception(v8::TryCatch* try_catch) {
	auto exception_string = convert<std::string>()(try_catch->Exception());

	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error;
		// just print the exception.
		fprintf(stderr, "%s\n", exception_string.c_str());
	} else {
		// Print (filename):(line number): (message).
		fprintf(stderr, "%s:%i: %s\n", convert<const char*>()(v8::String::Utf8Value(message->GetScriptResourceName())), message->GetLineNumber(), exception_string.c_str());

		// Print line of source code.
		fprintf(stderr, "%s\n", convert<const char*>()(v8::String::Utf8Value(message->GetSourceLine())));

		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; ++i) {
			fprintf(stderr, " ");
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; ++i) {
			fprintf(stderr, "^");
		}
		fprintf(stderr, "\n");
	}
}


class Processor;


static v8::Handle<v8::Value> print(const v8::Arguments& args) {
	auto argc = args.Length();
	if (argc) {
		printf("%s ", convert<const char*>()(v8::String::Utf8Value(args[0])));
		for (int i = 1; i < argc; ++i) {
			printf(" %s", convert<MsgPack>()(args[i]).to_string().c_str());
		}
	}
	printf("\n");

	return v8::Undefined();
}


struct Wrapper {
private:
	friend class Processor;

	v8::Persistent<v8::ObjectTemplate> _obj_template;
	wrap<MsgPack> _wrap;

	static v8::Handle<v8::Value> _to_string(const v8::Arguments& args) {
		return v8::String::New(convert<const char*>()(v8::String::Utf8Value(args.Data())));
	}

	static Wrapper& _wrapper(const v8::AccessorInfo& info) {
		auto data = v8::Handle<v8::External>::Cast(info.Data());
		return *(static_cast<Wrapper*>(data->Value()));
	}

	static v8::Handle<v8::Value> getter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
			return v8::Undefined();
		}

		auto name_str = convert<std::string>()(property);
		auto& wrapper = _wrapper(info);

		if (name_str == "toString") {
			auto str = wrapper._wrap.to_string(info);
			return v8::FunctionTemplate::New(_to_string, v8::String::New(str.data(), str.size()))->GetFunction();
		}

		return  wrapper._wrap.getter(name_str, info, wrapper._obj_template);
	}

	static v8::Handle<v8::Value> getter(uint32_t index, const v8::AccessorInfo& info) {
		auto& wrapper = _wrapper(info);
		return wrapper._wrap.getter(index, info, wrapper._obj_template);
	}

	static v8::Handle<v8::Value> setter(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
		if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
			return value_obj;
		}

		auto name_str = convert<std::string>()(property);

		if (name_str == "toString" || name_str == "valueOf") {
			return value_obj;
		}

		_wrapper(info)._wrap.setter(name_str, value_obj, info);
		return value_obj;
	}

	static v8::Handle<v8::Value> setter(uint32_t index, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
		_wrapper(info)._wrap.setter(index, value_obj, info);
		return value_obj;
	}

	static v8::Handle<v8::Boolean> deleter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
			return v8::Boolean::New(false);
		}

		auto name_str = convert<std::string>()(property);

		if (name_str == "toString" || name_str == "valueOf") {
			return v8::Boolean::New(false);
		}

		_wrapper(info)._wrap.deleter(name_str, info);
		return v8::Boolean::New(true);
	}

	static v8::Handle<v8::Boolean> deleter(uint32_t index, const v8::AccessorInfo& info) {
		_wrapper(info)._wrap.deleter(index, info);
		return v8::Boolean::New(true);
	}

	static v8::Handle<v8::Array> enumerator(const v8::AccessorInfo& info) {
		const auto& obj = convert<MsgPack>()(info);
		switch (obj.getType()) {
			case MsgPack::Type::MAP: {
				v8::Handle<v8::Array> result = v8::Array::New(obj.size());
				int i = 0;
				for (const auto& key : obj) {
					result->Set(i++, v8::String::New(key.as_string().c_str()));
				}
				return result;
			}
			case MsgPack::Type::ARRAY: {
				v8::Handle<v8::Array> result = v8::Array::New(obj.size());
				int i = 0;
				for (const auto& val : obj) {
					result->Set(i, v8::Integer::New(i));
					++i;
				}
				return result;
			}
			default:
				return v8::Array::New(0);
		}
	}

	static v8::Handle<v8::Integer> query(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
		if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
			return v8::Integer::New(v8::None);
		}

		auto name_str = convert<std::string>()(property);

		if (name_str == "toString" || name_str == "valueOf") {
			return v8::Integer::New(v8::ReadOnly | v8::DontDelete | v8::DontEnum);
		}

		return v8::Integer::New(v8::None);
	}

	static v8::Handle<v8::Integer> query(uint32_t index, const v8::AccessorInfo& info) {
		return v8::Integer::New(v8::None);
	}

public:
	Wrapper() = default;

	~Wrapper() {
		_obj_template.Dispose();
	}

	v8::Handle<v8::Value> operator()(MsgPack& arg) const {
		return _wrap.toValue(arg, _obj_template);
	}
};


class Processor {
public:
	/*
	 * Wrap C++ function into new V8 function.
	 */
	struct Function {
		Processor* that;
		v8::Persistent<v8::Function> function;

		Function(Processor* that_, v8::Persistent<v8::Function> function_)
			: that(that_),
			  function(function_) { }

		~Function() {
			function.Dispose();
		}

		template<typename... Args>
		MsgPack operator()(Args&&... args) {
			return that->invoke(function, std::forward<Args>(args)...);
		}
	};

private:
	v8::Isolate* isolate;
	bool initialized;

	Wrapper wrapper;
	v8::Persistent<v8::Context> context;
	std::map<std::string, Function> functions;

	void Initialize(const std::string& script_name_, const std::string& script_source_) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a handle scope to hold the temporary references.
		v8::HandleScope handle_scope;

		v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
		global->Set(v8::String::New("print"), v8::FunctionTemplate::New(print));

		context = v8::Context::New(nullptr, global);

		// Enter the new context so all the following operations take place within it.
		v8::Context::Scope context_scope(context);

		v8::Handle<v8::Value> script_name = v8::String::New(script_name_.data(), script_name_.size());
		v8::Handle<v8::String> script_source = v8::String::New(script_source_.data(), script_source_.size());

		v8::Handle<v8::Script> script;
		{
			v8::TryCatch try_catch;
			script = v8::Script::Compile(script_source, script_name);
			if (try_catch.HasCaught()) {
				report_exception(&try_catch);
				return;
			}
		}

		initialized = !script.IsEmpty();

		if (initialized) {
			{
				v8::TryCatch try_catch;
				script->Run();
				if (try_catch.HasCaught()) {
					report_exception(&try_catch);
					return;
				}
			}
			wrapper._obj_template = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
			wrapper._obj_template->SetInternalFieldCount(1);
			wrapper._obj_template->SetNamedPropertyHandler(Wrapper::getter, Wrapper::setter, Wrapper::query, Wrapper::deleter, Wrapper::enumerator, v8::External::New(this));
			wrapper._obj_template->SetIndexedPropertyHandler(Wrapper::getter, Wrapper::setter, Wrapper::query, Wrapper::deleter, Wrapper::enumerator, v8::External::New(this));
		}
	}

	v8::Persistent<v8::Function> extract_function(const std::string& name) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::Context::Scope context_scope(context);
		v8::HandleScope handle_scope;

		// The script compiled and ran correctly.  Now we fetch out the function from the global object.
		v8::Handle<v8::Value> function_val = context->Global()->Get(v8::String::New(name.c_str()));

		// If there is no function, or if it is not a function, ignore
		if (function_val->IsFunction()) {
			// It is a function; cast it to a Function
			v8::Handle<v8::Function> function_fun = v8::Handle<v8::Function>::Cast(function_val);

			// Store the function in a Persistent handle, since we also want
			// that to remain after this call returns
			return v8::Persistent<v8::Function>::New(function_fun);
		}

		return v8::Persistent<v8::Function>();
	}

	template<typename... Args>
	MsgPack invoke(const v8::Persistent<v8::Function>& function, Args&&... arguments) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::Context::Scope context_scope(context);
		v8::HandleScope handle_scope;

		std::array<v8::Handle<v8::Value>, sizeof...(arguments)> args{ wrapper(std::forward<Args>(arguments))... };
		v8::TryCatch try_catch;
		// Invoke the function, giving the global object as 'this' and one args
		v8::Handle<v8::Value> result = function->Call(context->Global(), args.size(), args.data());
		if (try_catch.HasCaught()) {
			report_exception(&try_catch);
		}

		return convert<MsgPack>()(result);
	}

public:
	Processor(const std::string& script_name, const std::string& script_source)
		: isolate(v8::Isolate::New())
	{
		Initialize(script_name, script_source);
	}

	~Processor() {
		{
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolate_scope(isolate);
			context.Dispose();
		}

		isolate->Dispose();
	}

	Function operator[](const std::string& name) {
		try {
			return functions.at(name);
		} catch (const std::out_of_range&) {
			functions.emplace(name, Function(this, extract_function(name)));
			return functions.at(name);
		}
	}
};

}; // End namespace v8pp


class ScriptLRU : public lru::LRU<size_t, v8pp::Processor> {
public:
	ScriptLRU(ssize_t max_size=-1) : LRU(max_size) { };
};
