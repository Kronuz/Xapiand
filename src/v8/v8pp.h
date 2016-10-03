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


constexpr size_t TIME_SCRIPT = 100; // Milliseconds.


inline static size_t hash(const std::string& source) {
	std::hash<std::string> hash_fn;
	return hash_fn(source);
}


static std::string report_exception(v8::TryCatch* try_catch) {
	auto exception_string = convert<std::string>()(try_catch->Exception());

	v8::Local<v8::Message> message = try_catch->Message();
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


static void print(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto argc = args.Length();
	if (argc) {
		printf("%s ", convert<const char*>()(v8::String::Utf8Value(args[0])));
		for (int i = 1; i < argc; ++i) {
			printf(" %s", convert<MsgPack>()(args[i]).to_string().c_str());
		}
	}
	printf("\n");
}

static void to_string(const v8::FunctionCallbackInfo<v8::Value>& args) {
	args.GetReturnValue().Set(convert<const char*>()(args));
}


class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
public:
	void* AllocateUninitialized(size_t length) {
		void* data = malloc(length);
		if (data == nullptr) {
			fprintf(stderr, "ERROR!!!\n");
		}
		return data;
	}
	void* Allocate(size_t length) {
		void* data = AllocateUninitialized(length);
		return data == nullptr ? data : memset(data, 0, length);
	}
	void Free(void* data, size_t) {
		free(data);
	}
};


class Processor {
	class V8Initializer {
		v8::Platform* platform;
		ArrayBufferAllocator allocator;
		v8::Isolate::CreateParams create_params;

	public:
		V8Initializer() : platform(v8::platform::CreateDefaultPlatform()) {
			create_params.array_buffer_allocator = &allocator;
			v8::V8::InitializePlatform(platform);
			v8::V8::InitializeICU();
			v8::V8::Initialize();
		}

		~V8Initializer() {
			v8::V8::Dispose();
			v8::V8::ShutdownPlatform();
			delete platform;
		}

		const v8::Isolate::CreateParams& CreateParams() {
			return create_params;
		}
	};

	struct PropertyHandler {
		Processor* processor;
		wrap<MsgPack> wapped_type;

		static void property_getter_cb(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->property_getter(property, info));
		}

		static void index_getter_cb(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->index_getter(index, info));
		}

		static void property_setter_cb(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->property_setter(property, value_obj, info));
		}

		static void index_setter_cb(uint32_t index, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->index_setter(index, value_obj, info));
		}

		static void property_deleter_cb(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->property_deleter(property, info));
		}

		static void index_deleter_cb(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->index_deleter(index, info));
		}

		static void property_query_cb(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Integer>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->property_query(property, info));
		}

		static void index_query_cb(uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->index_query(index, info));
		}

		static void enumerator_cb(const v8::PropertyCallbackInfo<v8::Array>& info) {
			PropertyHandler* self = static_cast<PropertyHandler*>(v8::Local<v8::External>::Cast(info.Data())->Value());
			info.GetReturnValue().Set(self->enumerator(info));
		}

		v8::Local<v8::Value> property_getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Undefined(processor->isolate);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString") {
				auto str = wapped_type.to_string(info);
				return v8::FunctionTemplate::New(processor->isolate, to_string, v8::String::NewFromUtf8(processor->isolate, str.data(), v8::NewStringType::kNormal, str.size()).ToLocalChecked())->GetFunction();
			}

			v8::Local<v8::ObjectTemplate> obj_template_ = v8::Local<v8::ObjectTemplate>::New(processor->isolate, processor->obj_template);
			return  wapped_type.getter(name_str, info, obj_template_);
		}

		v8::Local<v8::Value> index_getter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
			v8::Local<v8::ObjectTemplate> obj_template_ = v8::Local<v8::ObjectTemplate>::New(processor->isolate, processor->obj_template);
			return wapped_type.getter(index, info, obj_template_);
		}

		v8::Local<v8::Value> property_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info) {
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

		v8::Local<v8::Value> index_setter(uint32_t index, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info) {
			wapped_type.setter(index, value_obj, info);
			return value_obj;
		}

		v8::Local<v8::Boolean> property_deleter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Boolean::New(processor->isolate, false);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Boolean::New(processor->isolate, false);
			}

			wapped_type.deleter(name_str, info);
			return v8::Boolean::New(processor->isolate, true);
		}

		v8::Local<v8::Boolean> index_deleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
			wapped_type.deleter(index, info);
			return v8::Boolean::New(processor->isolate, true);
		}

		v8::Local<v8::Integer> property_query(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Integer>&) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Integer::New(processor->isolate, v8::None);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Integer::New(processor->isolate, v8::ReadOnly | v8::DontDelete | v8::DontEnum);
			}

			return v8::Integer::New(processor->isolate, v8::None);
		}

		v8::Local<v8::Integer> index_query(uint32_t, const v8::PropertyCallbackInfo<v8::Integer>&) {
			return v8::Integer::New(processor->isolate, v8::None);
		}

		v8::Local<v8::Array> enumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
			const auto& obj = convert<MsgPack>()(info);
			switch (obj.getType()) {
				case MsgPack::Type::MAP: {
					v8::Local<v8::Array> result = v8::Array::New(processor->isolate, obj.size());
					int i = 0;
					for (const auto& key : obj) {
						result->Set(i++, v8::String::NewFromUtf8(processor->isolate, key.as_string().c_str(), v8::NewStringType::kNormal).ToLocalChecked());
					}
					return result;
				}
				case MsgPack::Type::ARRAY: {
					auto size = obj.size();
					v8::Local<v8::Array> result = v8::Array::New(processor->isolate, size);
					for (size_t i = 0; i < size; ++i) {
						result->Set(i, v8::Integer::New(processor->isolate, i));
					}
					return result;
				}
				default:
					return v8::Array::New(processor->isolate, 0);
			}
		}

		PropertyHandler(Processor* processor_) : processor(processor_) { }

		v8::Local<v8::Value> operator()(MsgPack& arg) const {
			v8::Local<v8::ObjectTemplate> obj_template_ = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), processor->obj_template);
			return wapped_type.toValue(processor->isolate, arg, obj_template_);
		}
	};

public:
	/*
	 * Wrap C++ function into new V8 function.
	 */
	struct Function {
		Processor* processor;
		v8::Global<v8::Function> function;

		Function(Processor* processor_, v8::Global<v8::Function>&& function_)
			: processor(processor_),
			  function(std::move(function_)) { }

		~Function() {
			function.Reset();
		}

		template <typename... Args>
		MsgPack operator()(Args&&... args) {
			return processor->invoke(function, std::forward<Args>(args)...);
		}
	};

private:
	v8::Isolate* isolate;
	bool initialized;

	PropertyHandler property_handler;
	v8::Global<v8::Context> context;
	v8::Global<v8::ObjectTemplate> obj_template;
	std::map<std::string, Function> functions;

	// Auxiliar variables for kill a script.
	std::mutex mtx;
	std::condition_variable cond_kill;
	std::atomic_bool finished;

	void Initialize(const std::string& script_name_, const std::string& script_source_) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a handle scope to hold the temporary references.
		v8::HandleScope handle_scope(isolate);

		v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
		global->Set(v8::String::NewFromUtf8(isolate, "print", v8::NewStringType::kNormal).ToLocalChecked(), v8::FunctionTemplate::New(isolate, print));

		v8::Local<v8::Context> context_ = v8::Context::New(isolate, nullptr, global);
		context.Reset(isolate, context_);

		// Enter the new context so all the following operations take place within it.
		v8::Context::Scope context_scope(context_);

		v8::Local<v8::String> script_name = v8::String::NewFromUtf8(isolate, script_name_.data(), v8::NewStringType::kNormal, script_name_.size()).ToLocalChecked();
		v8::Local<v8::String> script_source = v8::String::NewFromUtf8(isolate, script_source_.data(), v8::NewStringType::kNormal, script_source_.size()).ToLocalChecked();

		v8::TryCatch try_catch;

		v8::Local<v8::Script> script = v8::Script::Compile(script_source, script_name);

		initialized = !script.IsEmpty();

		if (initialized) {
			script->Run();
			v8::Local<v8::ObjectTemplate> obj_template_ = v8::ObjectTemplate::New(isolate);
			obj_template_->SetInternalFieldCount(1);
			obj_template_->SetNamedPropertyHandler(
				v8pp::Processor::PropertyHandler::property_getter_cb,
				v8pp::Processor::PropertyHandler::property_setter_cb,
				v8pp::Processor::PropertyHandler::property_query_cb,
				v8pp::Processor::PropertyHandler::property_deleter_cb,
				v8pp::Processor::PropertyHandler::enumerator_cb,
				v8::External::New(isolate, &property_handler));
			obj_template_->SetIndexedPropertyHandler(
				v8pp::Processor::PropertyHandler::index_getter_cb,
				v8pp::Processor::PropertyHandler::index_setter_cb,
				v8pp::Processor::PropertyHandler::index_query_cb,
				v8pp::Processor::PropertyHandler::index_deleter_cb,
				v8pp::Processor::PropertyHandler::enumerator_cb,
				v8::External::New(isolate, &property_handler));
			obj_template.Reset(isolate, obj_template_);
		} else {
			throw ScriptSyntaxError(std::string("ScriptSyntaxError: ").append(report_exception(&try_catch)));
		}
	}

	v8::Global<v8::Function>&& extract_function(const std::string& name) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		v8::Local<v8::Context> context_ = v8::Local<v8::Context>::New(isolate, context);
		v8::Context::Scope context_scope(context_);

		// The script compiled and ran correctly.  Now we fetch out the function from the global object.
		v8::Local<v8::Value> function_val = context_->Global()->Get(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), name.c_str(), v8::NewStringType::kNormal).ToLocalChecked());

		// If there is no function, or if it is not a function, ignore
		if (function_val->IsFunction()) {
			// It is a function; cast it to a Function
			v8::Local<v8::Function> function_fun = v8::Local<v8::Function>::Cast(function_val);

			// Store the function in a Global handle, since we also want
			// that to remain after this call returns
			v8::Global<v8::Function> function;
			function.Reset(v8::Isolate::GetCurrent(), function_fun);
			return std::move(function);
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
	MsgPack invoke(const v8::Global<v8::Function>& function, Args&&... arguments) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		v8::Local<v8::Context> context_ = v8::Local<v8::Context>::New(isolate, context);
		v8::Context::Scope context_scope(context_);

		std::array<v8::Local<v8::Value>, sizeof...(arguments)> args{{ property_handler(std::forward<Args>(arguments))... }};
		v8::TryCatch try_catch;

		finished = false;
		std::thread t_kill(&Processor::kill, this);
		t_kill.detach();

		// Invoke the function, giving the global object as 'this' and one args
		v8::Local<v8::Function> function_ = v8::Local<v8::Function>::New(isolate, function);
		v8::Local<v8::Value> result = function_->Call(context_->Global(), args.size(), args.data());

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

	static const v8::Isolate::CreateParams& CreateParams() {
		static V8Initializer init;
		return init.CreateParams();
	}

public:
	Processor(const std::string& script_name, const std::string& script_source)
		: isolate(v8::Isolate::New(CreateParams())),
		  property_handler(this)
	{
		Initialize(script_name, script_source);
	}

	Processor(Processor&& o)
		: isolate(std::move(o.isolate)),
		  initialized(std::move(o.initialized)),
		  property_handler(std::move(o.property_handler)),
		  context(std::move(o.context)),
		  obj_template(std::move(o.obj_template)),
		  functions(std::move(o.functions))
	{ }

	Processor& operator=(Processor&& o) {
		if (this != &o) {
			reset();
			isolate = std::move(o.isolate);
			initialized = std::move(o.initialized);
			property_handler = std::move(o.property_handler);
			context = std::move(o.context);
			obj_template = std::move(o.obj_template);
			functions = std::move(o.functions);
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
				context.Reset();
			}

			isolate->Dispose();
		}
	}

	Function& operator[](const std::string& name) {
		try {
			return functions.at(name);
		} catch (const std::out_of_range&) {
			functions.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(this, extract_function(name)));
			return functions.at(name);
		}
	}
};

}; // End namespace v8pp

#endif
