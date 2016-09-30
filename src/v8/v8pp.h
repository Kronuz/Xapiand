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

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <array>
#include <string>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace v8pp {

// v8 version supported: v8-315


constexpr size_t TIME_SCRIPT = 300; // Milliseconds.


inline static size_t hash(const std::string& source) {
	std::hash<std::string> hash_fn;
	return hash_fn(source);
}


static std::string report_exception(v8::TryCatch* try_catch) {
	auto exception_string = convert<std::string>()(try_catch->Exception());

	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error;
		// just print the exception.
		fprintf(stderr, "%s\n", exception_string.c_str());
		return exception_string;
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
		return exception_string;
	}
}


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

static v8::Handle<v8::Value> to_string(const v8::Arguments& args) {
	return v8::String::New(convert<const char*>()(v8::String::Utf8Value(args.Data())));
}


class Processor {
	struct PropertyHandler {
		static PropertyHandler& self(const v8::AccessorInfo& info) {
			auto data = v8::Handle<v8::External>::Cast(info.Data());
			return *(static_cast<PropertyHandler*>(data->Value()));
		}

		static v8::Handle<v8::Value> property_getter_cb(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			return self(info).property_getter(property, info);
		}

		static v8::Handle<v8::Value> index_getter_cb(uint32_t index, const v8::AccessorInfo& info) {
			return self(info).index_getter(index, info);
		}

		static v8::Handle<v8::Value> property_setter_cb(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			return self(info).property_setter(property, value_obj, info);
		}

		static v8::Handle<v8::Value> index_setter_cb(uint32_t index, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			return self(info).index_setter(index, value_obj, info);
		}

		static v8::Handle<v8::Boolean> property_deleter_cb(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			return self(info).property_deleter(property, info);
		}

		static v8::Handle<v8::Boolean> index_deleter_cb(uint32_t index, const v8::AccessorInfo& info) {
			return self(info).index_deleter(index, info);
		}

		static v8::Handle<v8::Integer> property_query_cb(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			return self(info).property_query(property, info);
		}

		static v8::Handle<v8::Integer> index_query_cb(uint32_t index, const v8::AccessorInfo& info) {
			return self(info).index_query(index, info);
		}

		static v8::Handle<v8::Array> enumerator_cb(const v8::AccessorInfo& info) {
			return self(info).enumerator(info);
		}

		v8::Persistent<v8::ObjectTemplate> obj_template;
		wrap<MsgPack> wapped_type;

		v8::Handle<v8::Value> property_getter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Undefined();
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString") {
				auto str = wapped_type.to_string(info);
				return v8::FunctionTemplate::New(to_string, v8::String::New(str.data(), str.size()))->GetFunction();
			}

			return  wapped_type.getter(name_str, info, obj_template);
		}

		v8::Handle<v8::Value> index_getter(uint32_t index, const v8::AccessorInfo& info) {
			return wapped_type.getter(index, info, obj_template);
		}

		v8::Handle<v8::Value> property_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return value_obj;
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return value_obj;
			}

			wapped_type.setter(name_str, value_obj, info);
			return value_obj;
		}

		v8::Handle<v8::Value> index_setter(uint32_t index, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			wapped_type.setter(index, value_obj, info);
			return value_obj;
		}

		v8::Handle<v8::Boolean> property_deleter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Boolean::New(false);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Boolean::New(false);
			}

			wapped_type.deleter(name_str, info);
			return v8::Boolean::New(true);
		}

		v8::Handle<v8::Boolean> index_deleter(uint32_t index, const v8::AccessorInfo& info) {
			wapped_type.deleter(index, info);
			return v8::Boolean::New(true);
		}

		v8::Handle<v8::Integer> property_query(v8::Local<v8::String> property, const v8::AccessorInfo&) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Integer::New(v8::None);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Integer::New(v8::ReadOnly | v8::DontDelete | v8::DontEnum);
			}

			return v8::Integer::New(v8::None);
		}

		v8::Handle<v8::Integer> index_query(uint32_t, const v8::AccessorInfo&) {
			return v8::Integer::New(v8::None);
		}

		v8::Handle<v8::Array> enumerator(const v8::AccessorInfo& info) {
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
					auto size = obj.size();
					v8::Handle<v8::Array> result = v8::Array::New(size);
					for (size_t i = 0; i < size; ++i) {
						result->Set(i, v8::Integer::New(i));
					}
					return result;
				}
				default:
					return v8::Array::New(0);
			}
		}

		~PropertyHandler() {
			obj_template.Dispose();
		}

		v8::Handle<v8::Value> operator()(MsgPack& arg) const {
			return wapped_type.toValue(arg, obj_template);
		}
	};

public:
	/*
	 * Wrap C++ function into new V8 function.
	 */
	struct Function {
		Processor& self;
		v8::Persistent<v8::Function> function;

		Function(Processor& self_, v8::Persistent<v8::Function> function_)
			: self(self_),
			  function(function_) { }

		~Function() {
			function.Dispose();
		}

		template <typename... Args>
		MsgPack operator()(Args&&... args) {
			return self.invoke(function, std::forward<Args>(args)...);
		}
	};

private:
	v8::Isolate* isolate;
	bool initialized;

	PropertyHandler property_handler;
	v8::Persistent<v8::Context> context;
	std::map<std::string, Function> functions;

	// Auxiliar variables for kill a script.
	std::mutex mtx;
	std::condition_variable cond_kill;
	std::atomic_bool finished;

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

		v8::TryCatch try_catch;

		v8::Handle<v8::Script> script = v8::Script::Compile(script_source, script_name);

		initialized = !script.IsEmpty();

		if (initialized) {
			script->Run();
			property_handler.obj_template = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
			property_handler.obj_template->SetInternalFieldCount(1);
			property_handler.obj_template->SetNamedPropertyHandler(
				v8pp::Processor::PropertyHandler::property_getter_cb,
				v8pp::Processor::PropertyHandler::property_setter_cb,
				v8pp::Processor::PropertyHandler::property_query_cb,
				v8pp::Processor::PropertyHandler::property_deleter_cb,
				v8pp::Processor::PropertyHandler::enumerator_cb,
				v8::External::New(&property_handler));
			property_handler.obj_template->SetIndexedPropertyHandler(
				v8pp::Processor::PropertyHandler::index_getter_cb,
				v8pp::Processor::PropertyHandler::index_setter_cb,
				v8pp::Processor::PropertyHandler::index_query_cb,
				v8pp::Processor::PropertyHandler::index_deleter_cb,
				v8pp::Processor::PropertyHandler::enumerator_cb,
				v8::External::New(&property_handler));
		} else {
			throw ScriptSyntaxError(std::string("ScriptSyntaxError: ").append(report_exception(&try_catch)));
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

		throw ReferenceError(std::string("Reference error to function: ").append(name));
	}

	void kill() {
		std::unique_lock<std::mutex> lk(mtx);
		if (!cond_kill.wait_for(lk, std::chrono::milliseconds(TIME_SCRIPT), [this](){ return finished.load(); }) && !v8::V8::IsExecutionTerminating(isolate)) {
			v8::V8::TerminateExecution(isolate);
			finished = true;
		}
	}

	template<typename... Args>
	MsgPack invoke(const v8::Persistent<v8::Function>& function, Args&&... arguments) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::Context::Scope context_scope(context);
		v8::HandleScope handle_scope;

		std::array<v8::Handle<v8::Value>, sizeof...(arguments)> args{{ property_handler(std::forward<Args>(arguments))... }};
		v8::TryCatch try_catch;

		finished = false;
		std::thread t_kill(&Processor::kill, this);
		t_kill.detach();

		// Invoke the function, giving the global object as 'this' and one args
		v8::Handle<v8::Value> result = function->Call(context->Global(), args.size(), args.data());

		if (finished) {
			throw TimeOutError();
		}

		finished = true;
		cond_kill.notify_one();

		if (try_catch.HasCaught()) {
			throw ScriptSyntaxError(std::string("ScriptSyntaxError: ").append(report_exception(&try_catch)));
		}

		return convert<MsgPack>()(result);
	}

public:
	Processor(const std::string& script_name, const std::string& script_source)
		: isolate(v8::Isolate::New())
	{
		Initialize(script_name, script_source);
	}

	Processor(Processor&& o)
		: isolate(o.isolate),
		  initialized(std::move(o.initialized)),
		  property_handler(std::move(o.property_handler)),
		  context(std::move(o.context)),
		  functions(std::move(o.functions))
	{
		o.isolate = nullptr;
	}

	Processor& operator=(Processor&& o) {
		if (this != &o) {
			reset();
			isolate = o.isolate;
			initialized = std::move(o.initialized);
			property_handler = std::move(o.property_handler);
			context = std::move(o.context);
			functions = std::move(o.functions);
			o.isolate = nullptr;
		}
		return *this;
	}

	Processor(const Processor&) = delete;
	Processor& operator=(const Processor&) = delete;

	~Processor() {
		reset();
	}

	void reset() {
		if (isolate) {
			{
				v8::Locker locker(isolate);
				v8::Isolate::Scope isolate_scope(isolate);
				context.Dispose();
			}

			isolate->Dispose();
		}
	}

	Function operator[](const std::string& name) {
		try {
			return functions.at(name);
		} catch (const std::out_of_range&) {
			functions.emplace(name, Function(*this, extract_function(name)));
			return functions.at(name);
		}
	}
};

}; // End namespace v8pp
