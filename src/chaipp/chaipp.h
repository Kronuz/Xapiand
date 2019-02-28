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

#include "config.h"          // for XAPIAND_CHAISCRIPT

#if XAPIAND_CHAISCRIPT

#include "string_view.hh"
#include <unordered_map>

#include "exception.h"
#include "lru.h"
#include "module.h"
#include "msgpack.h"


namespace chaipp {

inline size_t hash(std::string_view source) {
	std::hash<std::string_view> hash_fn;
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

		std::shared_ptr<Processor> compile(size_t script_hash, size_t body_hash, std::string_view script_name, std::string_view script_body) {
			std::unique_lock<std::mutex> lk(mtx);
			auto it = script_lru.find(script_hash);
			if (it != script_lru.end()) {
				if (script_body.empty() || it->second.first == body_hash) {
					return it->second.second;
				}
			}
			lk.unlock();

			auto processor = std::make_shared<Processor>(script_name, script_body);

			lk.lock();
			return script_lru.emplace(script_hash, std::make_pair(body_hash, std::move(processor))).first->second.second;
		}

		std::shared_ptr<Processor> compile(std::string_view script_name, std::string_view script_body) {
			if (script_name.empty()) {
				auto body_hash = hash(script_body);
				return compile(body_hash, body_hash, "", script_body);
			} else if (script_body.empty()) {
				auto script_hash = hash(script_name);
				return compile(script_hash, script_hash, "", script_name);
			} else {
				auto script_hash = hash(script_name);
				auto body_hash = hash(script_body);
				return compile(script_hash, body_hash, script_name, script_body);
			}
		}
	};

	chaiscript::ChaiScript_Basic chai;
	chaiscript::AST_NodePtr ast;

public:
	Processor(std::string_view script_name, std::string_view script_body) :
		chai(chaipp::Std_Lib::library(),
			std::make_unique<chaiscript::parser::ChaiScript_Parser<chaiscript::eval::Noop_Tracer, chaiscript::optimizer::Optimizer_Default>>()) {
		ast = chai.get_parser().parse(std::string(script_body), std::string(script_name));
		L_MAGENTA(ast->to_string());
	}

	void operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params) {
		chai.add(chaiscript::const_var(std::ref(method)), "method");
		chai.add(chaiscript::var(std::ref(doc)), "doc");
		chai.add(chaiscript::const_var(std::ref(old_doc)), "old_doc");
		for (auto it = params.begin(); it != params.end(); ++it) {
			chai.add(chaiscript::const_var(std::ref(it.value())), it->str());
		}
		try {
			chai.eval(*ast);
		} catch (chaiscript::Boxed_Value &bv) {
			try {
				auto exc = chai.boxed_cast<chaiscript::exception::eval_error>(bv);
				L_ERR(exc.pretty_print());
			} catch (const chaiscript::exception::bad_boxed_cast &) {
				try {
					auto exc = chai.boxed_cast<std::exception>(bv);
					L_ERR("Exception: {}", exc.what());
				} catch (const chaiscript::exception::bad_boxed_cast &exc) {
					L_ERR("Exception (bad_boxed_cast): {}", exc.what());
					throw;
				}
			}
		}
	}

	static Engine& engine() {
		static Engine* engine = new Engine(SCRIPTS_CACHE_SIZE);
		return *engine;
	}

	static auto compile(size_t script_hash, size_t body_hash, std::string_view script_name, std::string_view script_body) {
		return engine().compile(script_hash, body_hash, script_name, script_body);
	}

	static auto compile(std::string_view script_name, std::string_view script_body) {
		return engine().compile(script_name, script_body);
	}
};

}; // End namespace chaipp

#endif
