// brew install v8-315
// c++ -std=c++14 -fsanitize=address -g -O2 -o test tst.cpp -lv8 -L/usr/local/opt/v8-315/lib -I/usr/local/opt/v8-315/include && ./test

#include <iostream>
#include <cassert>
#include <string>
#include <map>
#include <array>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <v8.h>

#include "../msgpack.h"


static const char* to_cstr(const v8::String::Utf8Value& value) {
	return *value ? *value : "<string conversion failed>";
}


/*
 * Convert a JavaScript string to a std::string. To not bother too
 * much with string encodings we just use ascii.
 */
static std::string to_str(v8::Local<v8::Value> value) {
	return to_cstr(v8::String::Utf8Value(value));
}



static void report_exception(v8::TryCatch* try_catch) {
	fprintf(stderr, "++++ Inside report_exception\n");
	auto exception_string = to_str(try_catch->Exception());

	fprintf(stderr, "++++ Inside report_exception 2\n");

	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error;
		// just print the exception.
		fprintf(stderr, "++++ Inside report_exception 3\n");
		fprintf(stderr, "%s\n", exception_string.c_str());
	} else {
		// Print (filename):(line number): (message).
		fprintf(stderr, "++++ Inside report_exception 4\n");
		fprintf(stderr, "%s\n", to_cstr(v8::String::Utf8Value(message->GetScriptResourceName())));
		fprintf(stderr, "%s:%i\n", to_cstr(v8::String::Utf8Value(message->GetScriptResourceName())), message->GetLineNumber());
		fprintf(stderr, "%s:%i: %s\n", to_cstr(v8::String::Utf8Value(message->GetScriptResourceName())), message->GetLineNumber(), exception_string.c_str());
		fprintf(stderr, "++++ Inside report_exception 5\n");

		// Print line of source code.
		fprintf(stderr, "%s\n", to_cstr(v8::String::Utf8Value(message->GetSourceLine())));
		fprintf(stderr, "++++ Inside report_exception 6\n");

		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			fprintf(stderr, " ");
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			fprintf(stderr, "^");
		}
		fprintf(stderr, "\n");
	}
}


static v8::Handle<v8::Value>
print(const v8::Arguments& args) {
	auto argc = args.Length();
	if (argc) {
		printf("%s", to_cstr(v8::String::Utf8Value(args[0])));
		for (int i = 1; i < argc; i++) {
			printf(" %s", to_cstr(v8::String::Utf8Value(args[i])));
		}
	}
	printf("\n");
	fflush(stdout);
	return v8::Undefined();
}


class Processor {
	/*
	 * Wrap C++ function into new V8 function.
	 */
	struct Function {
		Processor* that;
		v8::Persistent<v8::Function> function;

		Function(Processor* that_, v8::Persistent<v8::Function> function_)
			: that(that_), function(function_) { }

		~Function() {
			function.Dispose();
		}

		template<typename... Args>
		void operator()(Args&&... args) {
			fprintf(stderr, "+++ Function::operator()\n");
			that->invoke(function, std::forward<Args>(args)...);
		}
	};

	struct Wrapper {
		static Processor&
		_processor(const v8::AccessorInfo& info) {
			v8::Handle<v8::External> data = v8::Handle<v8::External>::Cast(info.Data());
			return *(static_cast<Processor*>(data->Value()));
		}

		static MsgPack&
		_msgpack(const v8::AccessorInfo& info) {
			v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(info.Holder()->GetInternalField(0));
			return *(static_cast<MsgPack*>(field->Value()));
		}

		static v8::Handle<v8::Value>
		_to_string(const v8::Arguments& args) {
			return v8::String::New(to_cstr(v8::String::Utf8Value(args.Data())));
		}

		static void
		_setter(MsgPack& internal_obj, const v8::Local<v8::Value>& value_obj) {
			if (value_obj->IsBoolean()) {
				internal_obj = value_obj->BooleanValue();
			} else if (value_obj->IsInt32()) {
				internal_obj = value_obj->IntegerValue();
			} else if (value_obj->IsNumber()) {
				internal_obj = value_obj->NumberValue();
			} else if (value_obj->IsObject()) {
				auto properties = value_obj->ToObject()->GetPropertyNames();
				auto length = properties->ToObject()->Get(v8::String::New("length"))->Uint32Value();
				for (int i = 0; i < length; ++i) {
				    _setter(internal_obj[to_str(properties->Get(i))], value_obj->ToObject()->Get(i));
				}
			} else if (value_obj->IsArray()) {
				auto length = value_obj->ToObject()->Get(v8::String::New("length"))->Uint32Value();
				internal_obj = { };
				for (int i = 0; i < length; ++i) {
					_setter(internal_obj[i], value_obj->ToObject()->Get(i));
				}
			} else {
				internal_obj = to_str(value_obj);
			}
		}

		static v8::Handle<v8::Value>
		getter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return v8::Undefined();
			}

			auto name_str = to_str(property);

			MsgPack& obj = _msgpack(info);

			if (name_str == "toString") {
				auto str = obj.to_string();
				return v8::FunctionTemplate::New(_to_string, v8::String::New(str.data(), str.size()))->GetFunction();
			}

			Processor& processor = _processor(info);
			try {
				return processor.wrap(obj.at(name_str));
			} catch (const std::out_of_range&) {
				return v8::Undefined();
			}
		}

		static v8::Handle<v8::Value>
		getter(uint32_t index, const v8::AccessorInfo& info) {
			Processor& processor = _processor(info);
			MsgPack& obj = _msgpack(info);

			try {
				return processor.wrap(obj.at(index));
			} catch (const std::out_of_range&) {
				return v8::Undefined();
			}
		}

		static v8::Handle<v8::Value>
		setter(v8::Local<v8::String> property, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined()) {
				return value_obj;
			}

			auto name_str = to_str(property);

			if (name_str == "toString") {
				return value_obj;
			}

			if (name_str == "valueOf") {
				return value_obj;
			}

			MsgPack& obj =_msgpack(info);

			_setter(obj[name_str], value_obj);

			return value_obj;
		}

		static v8::Handle<v8::Value>
		setter(uint32_t index, v8::Local<v8::Value> value_obj, const v8::AccessorInfo& info) {
			MsgPack& obj =_msgpack(info);

			_setter(obj[index], value_obj);

			return value_obj;
		}

		static v8::Handle<v8::Boolean>
		deleter(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined())
				return v8::Boolean::New(false);

			auto name_str = to_str(property);

			if (name_str == "toString") {
				return v8::Boolean::New(false);
			}

			if (name_str == "valueOf") {
				return v8::Boolean::New(false);
			}

			MsgPack& obj =_msgpack(info);

			obj.erase(name_str);

			return v8::Boolean::New(true);
		}

		static v8::Handle<v8::Boolean>
		deleter(uint32_t index, const v8::AccessorInfo& info) {
			MsgPack& obj =_msgpack(info);

			obj.erase(index);

			return v8::Boolean::New(true);
		}


		static v8::Handle<v8::Array>
		enumerator(const v8::AccessorInfo& info) {
			v8::Handle<v8::Array> result = v8::Array::New(1);
			result->Set(0, v8::String::New("Universal Answer"));
			return result;
		}


		static v8::Handle<v8::Integer>
		query(v8::Local<v8::String> property, const v8::AccessorInfo& info) {
			if (property.IsEmpty() || property->IsNull() || property->IsUndefined())
				return v8::Integer::New(v8::None);

			auto name_str = to_str(property);

			if (name_str == "toString") {
				return v8::Integer::New(v8::ReadOnly | v8::DontDelete | v8::DontEnum);
			}

			if (name_str == "valueOf") {
				return v8::Integer::New(v8::ReadOnly | v8::DontDelete | v8::DontEnum);
			}

			return v8::Integer::New(v8::None);
		}

		static v8::Handle<v8::Integer>
		query(uint32_t index, const v8::AccessorInfo& info) {
			return v8::Integer::New(v8::None);
		}
	};

	v8::Isolate* isolate;
	bool initialized;

	v8::Persistent<v8::ObjectTemplate> wrapper;
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
				fprintf(stderr, "++++ ERROR\n");
				report_exception(&try_catch);
				fprintf(stderr, "++++ ERROR 2\n");
				return;
			}
		}

		initialized = !script.IsEmpty();

		if (initialized) {
			{
				v8::TryCatch try_catch;
				script->Run();
				if (try_catch.HasCaught()) {
					fprintf(stderr, "++++ ERROR 2\n");
					report_exception(&try_catch);
					return;
				}
			}

			v8::Handle<v8::ObjectTemplate> raw_obj_wrapper = v8::ObjectTemplate::New();
			wrapper = v8::Persistent<v8::ObjectTemplate>::New(raw_obj_wrapper);
			wrapper->SetInternalFieldCount(1);
			wrapper->SetNamedPropertyHandler(Wrapper::getter, Wrapper::setter, Wrapper::query, Wrapper::deleter, Wrapper::enumerator, v8::External::New(this));
			wrapper->SetIndexedPropertyHandler(Wrapper::getter, Wrapper::setter, Wrapper::query, Wrapper::deleter, Wrapper::enumerator, v8::External::New(this));
		}
	}

	v8::Persistent<v8::Function>
	extract_function(const std::string& name) {
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


	v8::Handle<v8::Value>
	wrap(MsgPack& arg)
	{
		v8::Handle<v8::Value> ret;

		switch (arg.getType()) {
			case MsgPack::Type::MAP: {
				v8::Handle<v8::Object> obj = wrapper->NewInstance();
				obj->SetInternalField(0, v8::External::New(&arg));
				return obj;
			}
			case MsgPack::Type::ARRAY: {
				v8::Handle<v8::Object> obj = wrapper->NewInstance();
				obj->SetInternalField(0, v8::External::New(&arg));
				return obj;
			}
			case MsgPack::Type::STR: {
				auto arg_str = arg.as_string();
				return v8::String::New(arg_str.data(), arg_str.size());
			}
			case MsgPack::Type::POSITIVE_INTEGER: {
				return v8::Integer::New(arg.as_u64());
			}
			case MsgPack::Type::NEGATIVE_INTEGER: {
				return v8::Integer::New(arg.as_i64());
			}
			case MsgPack::Type::FLOAT: {
				return v8::Number::New(arg.as_f64());
			}
			case MsgPack::Type::BOOLEAN: {
				return v8::Boolean::New(arg.as_bool());
			}
			case MsgPack::Type::UNDEFINED: {
				fprintf(stderr, "++++ UNDEFINED\n");
				v8::Handle<v8::Object> obj = wrapper->NewInstance();
				obj->SetInternalField(0, v8::External::New(&arg));
				return obj;
			}
			case MsgPack::Type::NIL: {
				fprintf(stderr, "++++ NIL\n");
				return v8::Null();
			}
			default:
				return v8::Undefined();
		}
	}

	template<typename... Args>
	v8::Handle<v8::Value>
	invoke(const v8::Persistent<v8::Function>& function, Args&&... arguments) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::Context::Scope context_scope(context);
		v8::HandleScope handle_scope;

		std::array<v8::Handle<v8::Value>, sizeof...(arguments)> args{ wrap(std::forward<Args>(arguments))... };

		v8::TryCatch try_catch;
		// Invoke the function, giving the global object as 'this' and one args
		fprintf(stderr, "++++ Size args: %zu\n", args.size());
		v8::Handle<v8::Value> result = function->Call(context->Global(), args.size(), args.data());
		fprintf(stderr, "%s\n", "++++ Pass");
		if (try_catch.HasCaught()) {
			report_exception(&try_catch);
		}
		return result;
	}

public:
	Processor(const std::string& script_name_, const std::string& script_source_) : isolate(v8::Isolate::New()) {
		Initialize(script_name_, script_source_);
	}

	~Processor() {
		{
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolate_scope(isolate);

			wrapper.Dispose();
			context.Dispose();
		}

		isolate->Dispose();
	}

	Function
	operator[](const std::string& name) {
		try {
			return functions.at(name);
		} catch (const std::out_of_range&) {
			fprintf(stderr, "++++ Do not found\n");
			functions.emplace(name, Function(this, extract_function(name)));
			fprintf(stderr, "++++ Function added\n");
			return functions.at(name);
		}
	}
};


void run() {
	auto p = Processor("unnamed",
		"function on_post(old) { print('on_post:', old.five * 1) }"
		"function on_patch(old) { print('on_patch:', old.five * 3) }"
		"function on_get(old) { print('on_get:', old.five * 5) }"
		"function on_put(old) { print('on_put:', old.five * 7); }"
		"function test_object(old, nn) {"\
			"nn = {key:'key', value:'value'};"\
			"print('nn:', nn);"\
			"nn.key = 'newkey';"\
			"nn.value = 'newvalue';"\
			"print('nn:', nn);"\
		"}"
		"function test_array(old, nn) {"\
			"nn = ['key', 'value'];"\
			"print('nn:', nn);"\
			"nn[0] = 'newkey';"\
			"nn[1] = 'newvalue';"\
			"print('nn:', nn);"\
		"}"\
	);


	MsgPack old_map = {
		{ "one", 1 },
		{ "two", 2 },
		{ "three",
			{
				{ "value", 30 },
				{ "person",
					{
						{ "name", "José" },
						{ "last", "Perez" },
					}
				}
			}
		},
		{ "four", 4 },
		{ "five", 5 }
	};

	MsgPack new_map;

	p["test_array"](old_map, new_map);

	p["test_object"](old_map, new_map);
}


int
main(int argc, char* argv[]) {
	v8::V8::Initialize();

	run();

	v8::V8::Dispose();
	return 0;
}
