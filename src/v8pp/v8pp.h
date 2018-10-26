/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#if XAPIAND_V8

#include <array>               // for array
#include <atomic>              // for atomic_bool
#include <condition_variable>  // for condition_variable
#include <mutex>               // for mutex
#include <stdio.h>             // for size_t, snprintf
#include <stdlib.h>            // for malloc, free
#include <string.h>            // for memset
#include <string>              // for string
#include <thread>              // for thread
#include <unordered_map>       // for unordered_map

#include "lru.h"               // for LRU
#include "wrapper.h"           // for wrap


namespace v8pp {

// v8 version supported: v8-5.1


constexpr auto DURATION_SCRIPT = std::chrono::milliseconds(100);


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
		auto convertor = convert<MsgPack>();
		printf("%s ", convert<const char*>()(v8::String::Utf8Value(args[0])));
		for (int i = 1; i < argc; ++i) {
			printf(" %s", convertor(args[i]).to_string().c_str());
		}
	}
	printf("\n");
}


static void return_data(const v8::FunctionCallbackInfo<v8::Value>& args) {
	args.GetReturnValue().Set(args.Data());
}


class Processor {

	class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
	public:
		void* AllocateUninitialized(size_t length) {
			void* data = malloc(length);
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


	class ScriptLRU : public lru::LRU<size_t, std::pair<size_t, std::shared_ptr<Processor>>> {
	public:
		explicit ScriptLRU(ssize_t max_size)
			: LRU(max_size) { }
	};


	class Engine {
		v8::Platform* platform;
		ArrayBufferAllocator allocator;

		ScriptLRU script_lru;
		std::mutex mtx;

	public:
		v8::Isolate::CreateParams create_params;

		explicit Engine(ssize_t max_size)
			: platform(v8::platform::CreateDefaultPlatform()),
			  script_lru(max_size)
		{
			create_params.array_buffer_allocator = &allocator;
			v8::V8::InitializePlatform(platform);
			v8::V8::InitializeICU();
			v8::V8::Initialize();
		}

		~Engine() {
			script_lru.clear();
			v8::V8::Dispose();
			v8::V8::ShutdownPlatform();
			delete platform;
		}

		std::shared_ptr<Processor> compile(size_t script_hash, size_t body_hash, const std::string& script_body) {
			std::unique_lock<std::mutex> lk(mtx);
			auto it = script_lru.find(script_hash);
			if (it != script_lru.end()) {
				if (script_body.empty() || it->second.first == body_hash) {
					return it->second.second;
				}
			}
			lk.unlock();

			auto processor = std::make_shared<Processor>(script_body);

			lk.lock();
			return script_lru.emplace(script_hash, std::make_pair(body_hash, std::move(processor))).first->second.second;
		}

		std::shared_ptr<Processor> compile(const std::string& script_name, const std::string& script_body) {
			if (script_name.empty()) {
				auto body_hash = hash(script_body);
				return compile(body_hash, body_hash, script_body);
			} else if (script_body.empty()) {
				auto script_hash = hash(script_name);
				return compile(script_hash, script_hash, script_name);
			} else {
				auto script_hash = hash(script_name);
				auto body_hash = hash(script_body);
				return compile(script_hash, body_hash, script_body);
			}
		}
	};

	class PropertyHandler {
		v8::Isolate* isolate;
		v8::Global<v8::ObjectTemplate> obj_template;
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
				return v8::Undefined(isolate);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString") {
				auto string = wapped_type.toString(info);
				return v8::FunctionTemplate::New(isolate, return_data, string)->GetFunction();
			}

			if (name_str == "valueOf") {
				auto value = wapped_type.getter("_value", info, obj_template.Get(isolate));
				return v8::FunctionTemplate::New(isolate, return_data, value)->GetFunction();
			}

			return wapped_type.getter(name_str, info, obj_template.Get(isolate));
		}

		v8::Local<v8::Value> index_getter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
			return wapped_type.getter(index, info, obj_template.Get(isolate));
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
				return v8::Boolean::New(isolate, false);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Boolean::New(isolate, false);
			}

			wapped_type.deleter(name_str, info);
			return v8::Boolean::New(isolate, true);
		}

		v8::Local<v8::Boolean> index_deleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
			wapped_type.deleter(index, info);
			return v8::Boolean::New(isolate, true);
		}

		v8::Local<v8::Integer> property_query(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Integer>&) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Integer::New(isolate, v8::None);
			}

			auto name_str = convert<std::string>()(property);

			if (name_str == "toString" || name_str == "valueOf") {
				return v8::Integer::New(isolate, v8::ReadOnly | v8::DontDelete | v8::DontEnum);
			}

			return v8::Integer::New(isolate, v8::None);
		}

		v8::Local<v8::Integer> index_query(uint32_t, const v8::PropertyCallbackInfo<v8::Integer>&) {
			return v8::Integer::New(isolate, v8::None);
		}

		v8::Local<v8::Array> enumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
			return wapped_type.enumerator(info);
		}

	public:
		explicit PropertyHandler(v8::Isolate* isolate_)
			: isolate(isolate_)
		{
			v8::Local<v8::ObjectTemplate> obj_template_ = v8::ObjectTemplate::New(isolate);
			obj_template_->SetInternalFieldCount(1);
			obj_template_->SetNamedPropertyHandler(property_getter_cb, property_setter_cb, property_query_cb, property_deleter_cb, enumerator_cb, v8::External::New(isolate, this));
			obj_template_->SetIndexedPropertyHandler(index_getter_cb, index_setter_cb, index_query_cb, index_deleter_cb, enumerator_cb, v8::External::New(isolate, this));
			obj_template.Reset(isolate, obj_template_);
		}

		PropertyHandler(PropertyHandler&&) = delete;
		PropertyHandler(const PropertyHandler&) = delete;

		PropertyHandler& operator=(PropertyHandler&&) = delete;
		PropertyHandler& operator=(const PropertyHandler&) = delete;

		~PropertyHandler() {
			obj_template.Reset();
		}

		v8::Local<v8::Value> operator()(const MsgPack& arg) const {
			return wapped_type.toValue(isolate, arg, obj_template.Get(isolate));
		}
	};

	/*
	 * Wrap C++ function into new V8 function.
	 */
	class Function {
		Processor* processor;
		v8::Global<v8::Function> function;

	public:
		Function(Processor* processor_, v8::Global<v8::Function>&& function_)
			: processor(processor_),
			  function(std::move(function_)) { }

		Function(Function&& o) noexcept
			: processor(std::move(o.processor)),
			  function(std::move(o.function)) { }

		Function(const Function&) = delete;

		Function& operator=(Function&& o) noexcept {
			processor = std::move(o.processor);
			function = std::move(o.function);
			return *this;
		}

		Function& operator=(const Function& o) = delete;

		~Function() {
			function.Reset();
		}

		template <typename... Args>
		MsgPack operator()(Args&&... args) const {
			return processor->invoke(function, std::forward<Args>(args)...);
		}
	};

	v8::Isolate* isolate;

	std::unique_ptr<PropertyHandler> property_handler;
	v8::Global<v8::Context> context;
	std::unordered_map<std::string, const Function> functions;

	// Auxiliar variables for kill a script.
	std::mutex kill_mtx;
	std::condition_variable kill_cond;
	std::atomic_bool finished;

	void Initialize(const std::string& script_source_) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		property_handler = std::make_unique<PropertyHandler>(isolate);

		v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New(isolate);

		global_template->Set(v8::String::NewFromUtf8(isolate, "print", v8::NewStringType::kNormal).ToLocalChecked(), v8::FunctionTemplate::New(isolate, print));

		auto context_ = v8::Context::New(isolate, nullptr, global_template);
		context.Reset(isolate, context_);

		// Enter the new context so all the following operations take place within it.
		v8::Context::Scope context_scope(context_);
		v8::TryCatch try_catch;

		std::string script_name_("_script");
		v8::Local<v8::String> script_name = v8::String::NewFromUtf8(isolate, script_name_.data(), v8::NewStringType::kNormal, script_name_.size()).ToLocalChecked();
		v8::Local<v8::String> script_source = v8::String::NewFromUtf8(isolate, script_source_.data(), v8::NewStringType::kNormal, script_source_.size()).ToLocalChecked();
		v8::Local<v8::Script> script = v8::Script::Compile(script_source, script_name);

		script->Run();

		if (try_catch.HasCaught()) {
			throw ScriptSyntaxError(std::string("ScriptSyntaxError: ").append(report_exception(&try_catch)));
		}
	}

	v8::Global<v8::Function> extract_function(const std::string& name) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		auto context_ = context.Get(isolate);
		v8::Context::Scope context_scope(context_);

		// The script compiled and ran correctly.  Now we fetch out the function from the global object.
		auto global = context_->Global();
		auto name_value = v8::String::NewFromUtf8(isolate, name.data(), v8::NewStringType::kNormal, name.size()).ToLocalChecked();
		v8::Local<v8::Value> function_val = global->Get(name_value);

		// If there is no function, or if it is not a function, ignore
		if (function_val->IsFunction()) {
			// It is a function; cast it to a Function and make it Global
			return v8::Global<v8::Function>(isolate, v8::Local<v8::Function>::Cast(function_val));
		}

		throw ReferenceError(std::string("Reference error to function: ").append(name));
	}

	void kill() {
		std::unique_lock<std::mutex> lk(kill_mtx);
		if (!kill_cond.wait_for(lk, DURATION_SCRIPT, [this](){ return finished.load(); }) && !v8::V8::IsExecutionTerminating(isolate)) {
			v8::V8::TerminateExecution(isolate);
			finished = true;
		}
	}

	template<typename... Args>
	MsgPack invoke(const v8::Global<v8::Function>& function, Args&&... arguments) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		auto context_ = context.Get(isolate);
		v8::Context::Scope context_scope(context_);

		std::array<v8::Local<v8::Value>, sizeof...(arguments)> args{{ (*property_handler)(std::forward<Args>(arguments))... }};
		v8::TryCatch try_catch;

		auto global = context_->Global();
		auto function_ = function.Get(isolate);

		finished = false;
		std::thread t_kill(&Processor::kill, this);
		t_kill.detach();

		// Invoke the function, giving the global object as 'this' and one args
		v8::Local<v8::Value> result = function_->Call(global, args.size(), args.data());

		if (finished) {
			throw TimeOutError();
		}

		finished = true;
		kill_cond.notify_one();

		if (try_catch.HasCaught()) {
			throw ScriptSyntaxError(std::string("ScriptSyntaxError: ").append(report_exception(&try_catch)));
		}

		return convert<MsgPack>()(result);
	}

public:
	Processor(const std::string& script_source)
		: isolate(v8::Isolate::New(engine().create_params)),
		  finished(false)
	{
		Initialize(script_source);
	}

	Processor(Processor&&) = delete;
	Processor(const Processor&) = delete;

	Processor& operator=(Processor&&) = delete;
	Processor& operator=(const Processor&) = delete;

	~Processor() {
		functions.clear();
		property_handler.reset();
		{
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolate_scope(isolate);
			context.Reset();
		}
		isolate->Dispose();
	}

	const Function& operator[](const std::string& name) {
		auto it = functions.find(name);
		if (it == functions.end()) {
			it = functions.emplace(name, Function(this, extract_function(name))).first;
		}
		return it->second;
	}

	static Engine& engine() {
		static Engine engine(SCRIPTS_CACHE_SIZE);
		return engine;
	}

	static auto compile(const std::string& script_name, const std::string& script_body) {
		return engine().compile(script_name, script_body);
	}

	static auto compile(size_t script_hash, size_t body_hash, const std::string& script_body) {
		return engine().compile(script_hash, body_hash, script_body);
	}
};

}; // End namespace v8pp

#endif
