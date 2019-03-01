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

#include "database_handler.h"                     // for DatabaseHandler
#include "exception.h"                            // for chaipp::Error
#include "log.h"                                  // for L_EXC
#include "lru.h"                                  // for lru::LRU
#include "module.h"                               // for chaipp::Module
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr

namespace chaipp {

namespace internal {

class ScriptLRU : public lru::LRU<std::string, std::shared_ptr<Processor>> {
public:
	explicit ScriptLRU(ssize_t max_size)
		: LRU(max_size) { }
};


class Engine {
	ScriptLRU script_lru;
	std::mutex mtx;

public:
	explicit Engine(ssize_t max_size);

	std::shared_ptr<Processor> compile(const Script& script);

	static Engine& engine();
};


Engine::Engine(ssize_t max_size) :
	script_lru(max_size)
{
}


std::shared_ptr<Processor>
Engine::compile(const Script& script)
{
	std::string_view script_name;
	std::string_view script_body;

	auto sep_type = script.get_types();

	if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
		script_name = script.get_endpoint();
	} else {
		auto name_body = script.get_name_body();
		script_name = name_body.first;
		script_body = name_body.second;
	}

	std::shared_ptr<Processor> processor;

	std::unique_lock<std::mutex> lk(mtx);
	auto it = script_lru.find(std::string(script_name));  // FIXME: This copies script_name as LRU's std::unordered_map cannot find std::string_view
	if (it != script_lru.end()) {
		processor = it->second;
	}
	lk.unlock();

	if (processor) {
		if (script_body.empty()) {
			return processor;
		}
		std::hash<std::string_view> hash_fn;
		if (processor->get_hash() == hash_fn(script_body)) {
			return processor;
		}
	}

	processor = std::make_shared<Processor>(script);

	L_INFO("Script {} ({:x}) compiled and ready.", repr(script_name), processor->get_hash());

	lk.lock();
	return script_lru.emplace(std::string(script_name), processor).first->second;
}


Engine&
Engine::engine() {
	static Engine* engine = new Engine(SCRIPTS_CACHE_SIZE);
	return *engine;
}

}; // End namespace internal


Processor::Processor(const Script& script) :
	chai(Module::library(),
	std::make_unique<chaiscript::parser::ChaiScript_Parser<chaiscript::eval::Noop_Tracer, chaiscript::optimizer::Optimizer_Default>>())
{
	auto sep_type = script.get_types();

	std::string script_name;
	std::string script_body;

	if (sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
		std::string_view foreign_path;
		std::string_view foreign_id;
		auto endpoint = script.get_endpoint();
		split_path_id(endpoint, foreign_path, foreign_id);
		std::string_view selector;
		auto needle = foreign_id.find_first_of(".{", 1);  // Find first of either '.' (Drill Selector) or '{' (Field selector)
		if (needle != std::string_view::npos) {
			selector = foreign_id.substr(foreign_id[needle] == '.' ? needle + 1 : needle);
			foreign_id = foreign_id.substr(0, needle);
		}
		MsgPack foreign_data_script;
		try {
			DatabaseHandler db_handler(Endpoints{Endpoint{foreign_path}}, DB_OPEN | DB_NO_WAL, HTTP_GET);
			auto doc = db_handler.get_document(foreign_id);
			foreign_data_script = doc.get_obj();
		} catch (const Xapian::DocNotFoundError&) {
			THROW(ClientError, "Foreign script {}/{} doesn't exist", foreign_path, foreign_id);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			THROW(ClientError, "Foreign script database {} doesn't exist", foreign_path);
		}
		if (!selector.empty()) {
			foreign_data_script = foreign_data_script.select(selector);
		}
		Script foreign_script(foreign_data_script);
		auto foreign_sep_type = foreign_script.get_types();
		if (foreign_sep_type[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
			THROW(ClientError, "Nested foreign scripts not supported!");
		}
		auto name_body = foreign_script.get_name_body();
		script_name = name_body.first;
		script_body = name_body.second;
		script_params = foreign_script.get_params();
		script_params.update(script.get_params());
	} else {
		auto name_body = script.get_name_body();
		script_name = name_body.first;
		script_body = name_body.second;
		script_params = script.get_params();
	}

	script_params.lock();

	ast = chai.get_parser().parse(script_body, script_name);
	// L_MAGENTA(ast->to_string());

	std::hash<std::string_view> hash_fn;
	hash = hash_fn(script_body);
}


void
Processor::operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params)
{
	chai.add(chaiscript::const_var(std::ref(method)), "_method");

	chai.add(chaiscript::var(std::ref(doc)), "_doc");
	chai.add(chaiscript::const_var(std::ref(old_doc)), "_old_doc");

	auto merged_params = script_params;
	merged_params.update(params);
	for (auto it = merged_params.begin(); it != merged_params.end(); ++it) {
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
Processor::compile(const Script& script)
{
	return internal::Engine::engine().compile(script);
}


}; // End namespace chaipp

#endif
