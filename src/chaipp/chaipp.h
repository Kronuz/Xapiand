/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#if XAPIAND_CHAISCRIPT

#include "convert.h"
#include "wrapper.h"


namespace chaipp {

inline static size_t hash(const std::string& source) {
	std::hash<std::string> hash_fn;
	return hash_fn(source);
}


class Processor {
	class ScriptLRU : public lru::LRU<size_t, std::shared_ptr<Processor>> {
	public:
		ScriptLRU(ssize_t max_size) : LRU(max_size) { }
	};

	class Engine {
		ScriptLRU script_lru;
		std::mutex mtx;

	public:
		Engine(ssize_t max_size)
			: script_lru(max_size)
		{ }

		std::shared_ptr<Processor> compile(const std::string& script_name, const std::string& script_body) {
			auto script_hash = hash(script_name.empty() ? script_body : script_name);

			std::unique_lock<std::mutex> lk(mtx);
			auto it = script_lru.find(script_hash);
			if (it != script_lru.end()) {
				return it->second;
			}
			lk.unlock();

			if (script_body.empty()) {
				try {
					return compile(script_name, script_name);
				} catch (...) {
					throw std::out_of_range("Script not found: " + script_name);
				}
			}
			auto processor = std::make_shared<Processor>(script_name, script_body);

			lk.lock();
			return script_lru.emplace(script_hash, std::move(processor)).first->second;
		}
	};

	/*
	* Wrap ChaiScript function into new C++ function.
	*/
	class Function {
		chaiscript::Boxed_Value value;

		template <typename... Args>
		MsgPack eval(Args&&... args) const {
			try {
				auto func = chaiscript::boxed_cast<std::function<chaiscript::Boxed_Value(Args...)>>(value);
				return chaipp::convert<MsgPack>()(func(std::forward<Args>(args)...));
			} catch (const chaiscript::exception::bad_boxed_cast& er) {
				throw InvalidArgument(er.what());
			} catch (const chaiscript::exception::eval_error& er) {
				throw ScriptSyntaxError(er.pretty_print());
			}
		}

	public:
		Function(chaiscript::Boxed_Value&& value_)
			: value(std::move(value_)) { }

		Function(const chaiscript::Boxed_Value& value_)
			: value(value_) { }

		Function(Function&& o) noexcept
			: value(std::move(o.value)) { }

		Function(const Function& o)
			: value(o.value) { }

		Function& operator=(Function&& o) noexcept {
			value = std::move(o.value);
			return *this;
		}

		Function& operator=(const Function& o) {
			value = o.value;
			return *this;
		}

		template <typename... Args>
		MsgPack operator()(Args&&... args) const {
			return eval(chaipp::wrap<MsgPack>()(std::forward<Args>(args))...);
		}
	};

	chaiscript::ChaiScript chai;
	std::map<std::string, Function> functions;

public:
	Processor(const std::string&, const std::string& script_source) {
		try {
			chai.eval(script_source);
		} catch (const std::exception& er) {
			throw ScriptSyntaxError(er.what());
		}
	}

	const Function& operator[](const std::string& name) {
		try {
			return functions.at(name);
		} catch (const std::out_of_range&) {
			try {
				auto p = functions.emplace(name, chai.eval(name));
				return p.first->second;
			} catch (const chaiscript::exception::eval_error& er) {
				throw ReferenceError(er.pretty_print());
			}
		}
	}

	static Engine& engine(ssize_t max_size) {
		static Engine engine(max_size);
		return engine;
	}

	static auto compile(const std::string& script_name, const std::string& script_body) {
		return engine(0).compile(script_name, script_body);
	}
};

}; // End namespace chaipp

#endif
