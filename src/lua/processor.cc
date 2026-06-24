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

#include "processor.h"

#if XAPIAND_LUA

#include <cstdint>                                // for int64_t
#include <functional>                             // for std::hash
#include <string>
#include <string_view>

#include "lua/exception.h"                        // for lua::ScriptSyntaxError
#include "database/handler.h"                     // for DatabaseHandler
#include "exception.h"                            // for ClientError, THROW
#include "log.h"                                  // for L_ERR, L_INFO
#include "lru.h"                                  // for lru::lru
#include "manager.h"                              // for XapiandManager::*
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr
#include "url_parser.h"                           // for urldecode

namespace lua {

namespace internal {

// --- MsgPack <-> Lua conversion -------------------------------------------
//
// The document is handed to scripts as a plain Lua table (objects -> string-keyed
// tables, arrays -> 1-based tables, scalars -> Lua values) and converted back
// after the script runs. This keeps scripts idiomatic Lua and removes the need to
// bind the MsgPack type into the engine.

static sol::object msgpack_to_lua(sol::state_view state, const MsgPack& m) {
	switch (m.get_type()) {
		case MsgPack::Type::MAP: {
			sol::table t = state.create_table();
			for (auto it = m.begin(); it != m.end(); ++it) {
				t[it->str()] = msgpack_to_lua(state, it.value());
			}
			return t;
		}
		case MsgPack::Type::ARRAY: {
			sol::table t = state.create_table();
			lua_Integer i = 1;
			for (auto it = m.begin(); it != m.end(); ++it) {
				t[i++] = msgpack_to_lua(state, it.value());
			}
			return t;
		}
		case MsgPack::Type::STR:
			return sol::make_object(state, m.str_view());
		case MsgPack::Type::BOOLEAN:
			return sol::make_object(state, m.boolean());
		case MsgPack::Type::POSITIVE_INTEGER:
			return sol::make_object(state, static_cast<lua_Integer>(m.u64()));
		case MsgPack::Type::NEGATIVE_INTEGER:
			return sol::make_object(state, static_cast<lua_Integer>(m.i64()));
		case MsgPack::Type::FLOAT:
			return sol::make_object(state, m.f64());
		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED:
		default:
			return sol::make_object(state, sol::lua_nil);
	}
}


static MsgPack lua_to_msgpack(const sol::object& o) {
	switch (o.get_type()) {
		case sol::type::table: {
			sol::table t = o.as<sol::table>();
			// A table is an array iff its only keys are the contiguous integers
			// 1..#t; otherwise it is treated as a map.
			std::size_t seq = t.size();
			std::size_t count = 0;
			bool pure_seq = true;
			for (auto&& kv : t) {
				++count;
				if (kv.first.get_type() != sol::type::number) {
					pure_seq = false;
				}
			}
			if (pure_seq && seq > 0 && count == seq) {
				MsgPack arr = MsgPack::ARRAY();
				for (std::size_t i = 1; i <= seq; ++i) {
					arr.push_back(lua_to_msgpack(t[i]));
				}
				return arr;
			}
			MsgPack map = MsgPack::MAP();
			for (auto&& kv : t) {
				std::string key;
				auto kt = kv.first.get_type();
				if (kt == sol::type::string) {
					key = kv.first.as<std::string>();
				} else if (kt == sol::type::number) {
					key = std::to_string(kv.first.as<lua_Integer>());
				} else {
					continue;
				}
				map.put(key, lua_to_msgpack(kv.second));
			}
			return map;
		}
		case sol::type::string:
			return MsgPack(o.as<std::string>());
		case sol::type::number: {
			double d = o.as<double>();
			auto li = o.as<lua_Integer>();
			if (static_cast<double>(li) == d) {
				return MsgPack(static_cast<int64_t>(li));
			}
			return MsgPack(d);
		}
		case sol::type::boolean:
			return MsgPack(o.as<bool>());
		case sol::type::lua_nil:
		case sol::type::none:
		default:
			return MsgPack::NIL();
	}
}


class ScriptLRU : public lru::lru<std::string, std::shared_ptr<Processor>> {
public:
	explicit ScriptLRU(ssize_t max_size) :
		lru::lru(max_size) { }
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

	if (sep_type[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		script_name = script.get_endpoint();
	} else {
		auto name_body = script.get_name_body();
		script_name = name_body.first;
		script_body = name_body.second;
	}

	std::shared_ptr<Processor> processor;

	std::unique_lock<std::mutex> lk(mtx);
	auto it = script_lru.find(std::string(script_name));  // FIXME: copies; LRU map can't find string_view
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
	static Engine* engine = new Engine(opts.scripts_cache_size);
	return *engine;
}

}; // End namespace internal


Processor::Processor(const Script& script)
{
	// A safe subset of the Lua standard library: enough to transform documents,
	// without io/os (no filesystem or process access from a script).
	state.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::utf8);

	auto sep_type = script.get_types();

	std::string script_name;
	std::string script_body;

	if (sep_type[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		std::string foreign_path, foreign_id;
		auto foreign_uri = script.get_endpoint();
		std::string_view foreign_path_view, foreign_id_view;
		split_path_id(foreign_uri, foreign_path_view, foreign_id_view);
		foreign_path = urldecode(foreign_path_view);
		foreign_id = urldecode(foreign_id_view);
		std::string_view id = foreign_id;
		MsgPack obj;
		try {
			Endpoint endpoint(foreign_path);
			auto endpoints = XapiandManager::resolve_index_endpoints(endpoint);
			if (endpoints.empty()) {
				THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
			}
			DatabaseHandler _db_handler(endpoints, DB_OPEN);
			auto doc = _db_handler.get_document(id);
			obj = doc.get_obj();
		} catch (const Xapian::DocNotFoundError&) {
			THROW(ClientError, "Foreign script {}/{} doesn't exist", foreign_path, id);
		} catch (const Xapian::DatabaseNotFoundError& exc) {
			THROW(ClientError, "Foreign script database {} doesn't exist", repr(foreign_path));
		}
		// If there's no selector use "script" (to be consistent with "schema" field):
		obj = obj[SCRIPT_FIELD_NAME];
		Script foreign_script(obj);
		auto foreign_sep_type = foreign_script.get_types();
		if (foreign_sep_type[SPC_FOREIGN_TYPE] == FieldType::foreign) {
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

	sol::load_result loaded = state.load(script_body, std::string(script_name));
	if (!loaded.valid()) {
		sol::error err = loaded;
		THROW(ClientError, "Script {} syntax error: {}", repr(script_name), err.what());
	}
	func = loaded;

	std::hash<std::string_view> hash_fn;
	hash = hash_fn(script_body);
}


void
Processor::operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params)
{
	std::lock_guard<std::mutex> lk(mtx);

	sol::state_view view(state);

	view["_method"] = std::string(method);
	view["_doc"] = internal::msgpack_to_lua(view, doc);
	view["_old_doc"] = internal::msgpack_to_lua(view, old_doc);

	auto merged_params = script_params;
	merged_params.update(params);
	for (auto it = merged_params.begin(); it != merged_params.end(); ++it) {
		view[it->str()] = internal::msgpack_to_lua(view, it.value());
	}

	sol::protected_function_result result = func();
	if (!result.valid()) {
		sol::error err = result;
		L_ERR("Script error: {}", err.what());
		return;  // leave the document unchanged on error
	}

	// Apply the script's changes back onto the document.
	sol::object out = view["_doc"];
	doc = internal::lua_to_msgpack(out);
}


std::shared_ptr<Processor>
Processor::compile(const Script& script)
{
	return internal::Engine::engine().compile(script);
}


}; // End namespace lua

#endif
