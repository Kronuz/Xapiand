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

#include "chaipp.h"

#if XAPIAND_CHAISCRIPT

#include <functional>                             // for std::hash
#include "string_view.hh"

#include "module.h"                               // for chaipp::Module
#include "exception.h"                            // for chaipp::Error
#include "log.h"                                  // for L_EXC
#include "lru.h"                                  // for lru::LRU
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr

namespace chaipp {

namespace internal {

class ScriptLRU : public lru::LRU<std::string, std::pair<size_t, std::shared_ptr<Processor>>> {
public:
	explicit ScriptLRU(ssize_t max_size)
		: LRU(max_size) { }
};


class Engine {
	ScriptLRU script_lru;
	std::mutex mtx;

public:
	explicit Engine(ssize_t max_size);

	std::shared_ptr<Processor> compile(std::string_view script_name, std::string_view script_body);

	static Engine& engine();
};


Engine::Engine(ssize_t max_size) :
	script_lru(max_size)
{
}


std::shared_ptr<Processor>
Engine::compile(std::string_view script_name, std::string_view script_body)
{
	std::hash<std::string_view> hash_fn;
	auto script_body_hash = hash_fn(script_body);

	std::unique_lock<std::mutex> lk(mtx);
	auto it = script_lru.find(std::string(script_name));  // FIXME: This copies script_name as LRU's std::unordered_map cannot find std::string_view
	if (it != script_lru.end()) {
		if (script_body.empty() || it->second.first == script_body_hash) {
			return it->second.second;
		}
	}
	lk.unlock();

	auto processor = std::make_shared<Processor>(script_name, script_body);

	L_INFO("Script {} #{:x} compiled and ready.", repr(script_name), script_body_hash);

	lk.lock();
	return script_lru.emplace(std::string(script_name), std::make_pair(script_body_hash, std::move(processor))).first->second.second;
}


Engine&
Engine::engine() {
	static Engine* engine = new Engine(SCRIPTS_CACHE_SIZE);
	return *engine;
}

}; // End namespace internal


Processor::Processor(std::string_view script_name, std::string_view script_body) :
	chai(Module::library(),
	std::make_unique<chaiscript::parser::ChaiScript_Parser<chaiscript::eval::Noop_Tracer, chaiscript::optimizer::Optimizer_Default>>())
{
	ast = chai.get_parser().parse(std::string(script_body), std::string(script_name));
	// L_MAGENTA(ast->to_string());
}


void
Processor::operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params)
{
	chai.add(chaiscript::const_var(std::ref(method)), "_method");
	chai.add(chaiscript::var(std::ref(doc)), "_doc");
	chai.add(chaiscript::const_var(std::ref(old_doc)), "_old_doc");
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


std::shared_ptr<Processor>
Processor::compile(std::string_view script_name, std::string_view script_body)
{
	return internal::Engine::engine().compile(script_name, script_body);
}


}; // End namespace chaipp

#endif
