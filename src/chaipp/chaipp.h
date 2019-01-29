/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "config.h"          // for XAPIAND_CHAISCRIPT

#if XAPIAND_CHAISCRIPT

#include <unordered_map>

#include "exception.h"
#include "lru.h"
#include "module.h"


namespace chaipp {

inline size_t hash(const std::string& source) {
	std::hash<std::string> hash_fn;
	return hash_fn(source);
}


class Processor {

	class ScriptLRU : public lru::LRU<size_t, std::pair<size_t, std::shared_ptr<Processor>>> {
	public:
		explicit ScriptLRU(ssize_t max_size)
			: LRU(max_size) { }
	};


	class Engine {
		ScriptLRU script_lru;
		std::mutex mtx;

	public:
		explicit Engine(ssize_t max_size)
			: script_lru(max_size) { }

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

	/*
	 * Wrap ChaiScript function into new C++ function.
	 */
	class Function {
		chaiscript::Boxed_Value value;

	public:
		explicit Function(chaiscript::Boxed_Value&& value_)
			: value(std::move(value_)) { }

		explicit Function(const chaiscript::Boxed_Value& value_)
			: value(value_) { }

		Function(Function&& o) noexcept
			: value(std::move(o.value)) { }

		Function(const Function& o) = delete;

		Function& operator=(Function&& o) noexcept {
			value = std::move(o.value);
			return *this;
		}

		Function& operator=(const Function& o) = delete;

		template <typename... Args>
		MsgPack operator()(Args&&... args) const {
			try {
				auto func = chaiscript::boxed_cast<std::function<MsgPack(Args...)>>(value);
				return func(std::forward<Args>(args)...);
			} catch (const chaiscript::exception::bad_boxed_cast& er) {
				throw InvalidArgument(er.what());
			} catch (const chaiscript::exception::eval_error& er) {
				throw ScriptSyntaxError(er.pretty_print());
			}
		}
	};

	chaiscript::ChaiScript chai;
	std::unordered_map<std::string, const Function> functions;

public:
	Processor(const std::string& script_source) {
		static auto module_msgpack = ModuleMsgPack();
		chai.add(module_msgpack);

		try {
			chai.eval(script_source);
		} catch (const std::exception& er) {
			throw ScriptSyntaxError(er.what());
		}
	}

	const Function& operator[](const std::string& name) {
		auto it = functions.find(name);
		if (it == functions.end()) {
			try {
				it = functions.emplace(name, Function(chai.eval(name))).first;
			} catch (const chaiscript::exception::eval_error& er) {
				throw ReferenceError(er.pretty_print());
			}
		}
		return it->second;
	}

	static Engine& engine() {
		static Engine* engine = new Engine(SCRIPTS_CACHE_SIZE);
		return *engine;
	}

	static auto compile(size_t script_hash, size_t body_hash, const std::string& script_body) {
		return engine().compile(script_hash, body_hash, script_body);
	}

	static auto compile(const std::string& script_name, const std::string& script_body) {
		return engine().compile(script_name, script_body);
	}
};

}; // End namespace chaipp

#endif
