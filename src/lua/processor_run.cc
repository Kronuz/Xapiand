/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Running a script call: the MsgPack and Lua conversion, the call's resume loop and the state's collection.

#include "processor.h"
#include "processor_detail.h"

#if XAPIAND_LUA

#include <algorithm>                              // for std::min
#include <atomic>                                 // for std::atomic
#include <cassert>
#include <chrono>                                 // for std::chrono::steady_clock
#include <cstdint>                                // for int64_t
#include <cstdlib>                                // for realloc, free
#include <exception>                              // for std::exception_ptr
#include <functional>                             // for std::hash
#include <new>                                    // for std::bad_alloc
#include <optional>                               // for std::optional
#include <string>
#include <string_view>
#include <thread>                                 // for std::this_thread
#include <utility>                                // for std::exchange

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "lua/exception.h"                        // for lua::ScriptLimitError
#include "database/handler.h"                     // for DatabaseHandler
#include "engine/error.h"                         // for engine::Error
#include "task.hh"                                // for task::task
#include "xapiand_exception.h"                    // for ClientError, THROW
#include "log.h"                                  // for L_ERR, L_INFO
#include "lru.h"                                  // for lru::lru
#include "manager.h"                              // for XapiandManager::*
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr
#include "server/search_service.h"                 // for BUFFERED_BODY_CAP
#include "url_parser.h"                           // for urldecode

namespace lua {

namespace {

// Restores a stack top when a conversion throws.
struct StackGuard {
	lua_State* state;
	int top;
	explicit StackGuard(lua_State* l) : state(l), top(lua_gettop(l)) { }
	~StackGuard() { lua_settop(state, top); }
};

// Releases a call's registry slots; the thread is collected when its slot goes.
struct Refs {
	lua_State* state;
	int thread = LUA_NOREF;
	int env = LUA_NOREF;
	explicit Refs(lua_State* l) : state(l) { }
	~Refs() {
		luaL_unref(state, LUA_REGISTRYINDEX, thread);
		luaL_unref(state, LUA_REGISTRYINDEX, env);
	}
};

constexpr int max_depth = 200;

// --- MsgPack <-> Lua conversion -------------------------------------------
//
// The document is handed to scripts as a plain Lua table (objects -> string-keyed tables, arrays ->
// 1-based tables, scalars -> Lua values) and converted back after the script runs. Both directions
// use the raw API, so no metamethod of the script's runs outside the call's hook.

void push_msgpack(lua_State* L, const MsgPack& m, int depth) {
	if (depth > max_depth || !lua_checkstack(L, 4)) {
		THROW(ClientError, "The document is nested too deeply for a script");
	}
	switch (m.get_type()) {
		case MsgPack::Type::MAP: {
			lua_createtable(L, 0, static_cast<int>(m.size()));
			for (auto it = m.begin(); it != m.end(); ++it) {
				auto key = it->str();
				lua_pushlstring(L, key.data(), key.size());
				push_msgpack(L, it.value(), depth + 1);
				lua_rawset(L, -3);
			}
			return;
		}
		case MsgPack::Type::ARRAY: {
			lua_createtable(L, static_cast<int>(m.size()), 0);
			lua_Integer i = 1;
			for (auto it = m.begin(); it != m.end(); ++it) {
				push_msgpack(L, it.value(), depth + 1);
				lua_rawseti(L, -2, i++);
			}
			return;
		}
		case MsgPack::Type::STR: {
			auto str = m.str_view();
			lua_pushlstring(L, str.data(), str.size());
			return;
		}
		case MsgPack::Type::BOOLEAN:
			lua_pushboolean(L, m.boolean());
			return;
		case MsgPack::Type::POSITIVE_INTEGER:
			lua_pushinteger(L, static_cast<lua_Integer>(m.u64()));
			return;
		case MsgPack::Type::NEGATIVE_INTEGER:
			lua_pushinteger(L, static_cast<lua_Integer>(m.i64()));
			return;
		case MsgPack::Type::FLOAT:
			lua_pushnumber(L, m.f64());
			return;
		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED:
		default:
			lua_pushnil(L);
			return;
	}
}


// What the conversion may still spend: values (the host spends about 500 bytes on each) and string bytes.
struct Walk {
	std::size_t nodes_left;
	std::size_t bytes_left;

	// A value costs `weight` units: 1 for a scalar or string, more for a container (the host spends about 500 bytes
	// on a scalar and about 4 KiB on a small map).
	void node(std::size_t weight = 1) {
		if (weight > nodes_left) {
			THROW(ScriptResultTooLarge, "The script's result has more than the {} values a script may return", Limits{}.max_result_nodes);
		}
		nodes_left -= weight;
	}

	void bytes(std::size_t n) {
		if (n > bytes_left) {
			THROW(ScriptResultTooLarge, "The script's result is past the {} bytes of strings a script may return", Limits{}.max_result_bytes);
		}
		bytes_left -= n;
	}
};

// A number as the document stores it: an integer when it is one, a double otherwise.
MsgPack number_to_msgpack(lua_State* L, int idx) {
	if (lua_isinteger(L, idx)) {
		return MsgPack(static_cast<int64_t>(lua_tointeger(L, idx)));
	}
	const lua_Number d = lua_tonumber(L, idx);
	if (d >= -9223372036854775808.0 && d < 9223372036854775808.0) {
		const auto li = static_cast<lua_Integer>(d);
		if (static_cast<lua_Number>(li) == d) {
			return MsgPack(static_cast<int64_t>(li));
		}
	}
	return MsgPack(d);
}


// A numeric key as an integer (a fractional or out-of-range one truncates or becomes 0).
lua_Integer key_integer(lua_State* L, int idx) {
	if (lua_isinteger(L, idx)) {
		return lua_tointeger(L, idx);
	}
	const lua_Number d = lua_tonumber(L, idx);
	return d >= -9223372036854775808.0 && d < 9223372036854775808.0 ? static_cast<lua_Integer>(d) : 0;
}


// The value at `idx` as a MsgPack. A table is an array iff its only keys are the contiguous
// integers 1..#t; otherwise it is a map (keys that are neither strings nor numbers are dropped).
// Depth and total nodes are bounded: a script can build a cycle or a table that shares its parts.
MsgPack lua_to_msgpack(lua_State* L, int idx, Walk& walk, int depth) {
	if (depth > max_depth || !lua_checkstack(L, 4)) {
		THROW(ClientError, "The script's document is too deeply nested");
	}
	walk.node();
	idx = lua_absindex(L, idx);
	switch (lua_type(L, idx)) {
		case LUA_TTABLE: {
			StackGuard guard(L);
			const auto seq = static_cast<std::size_t>(lua_rawlen(L, idx));
			std::size_t count = 0;
			bool pure_seq = true;
			lua_pushnil(L);
			while (lua_next(L, idx) != 0) {
				++count;
				if (lua_type(L, -2) != LUA_TNUMBER) {
					pure_seq = false;
				}
				lua_pop(L, 1);
			}
			if (pure_seq && seq > 0 && count == seq) {
				walk.node(1);   // an array: 2 units in all
				MsgPack arr = MsgPack::ARRAY();
				for (std::size_t i = 1; i <= seq; ++i) {
					lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
					arr.push_back(lua_to_msgpack(L, -1, walk, depth + 1));
					lua_pop(L, 1);
				}
				return arr;
			}
			walk.node(7);   // a map: 8 units in all
			MsgPack map = MsgPack::MAP();
			lua_pushnil(L);
			while (lua_next(L, idx) != 0) {
				std::string key;
				const int kt = lua_type(L, -2);
				if (kt == LUA_TSTRING) {
					std::size_t len;
					const char* str = lua_tolstring(L, -2, &len);
					walk.bytes(len);
					key.assign(str, len);
				} else if (kt == LUA_TNUMBER) {
					key = std::to_string(key_integer(L, -2));
					walk.bytes(key.size());
				} else {
					walk.node();   // a dropped entry still costs a visit
					lua_pop(L, 1);
					continue;
				}
				map.put(key, lua_to_msgpack(L, -1, walk, depth + 1));
				lua_pop(L, 1);
			}
			return map;
		}
		case LUA_TSTRING: {
			std::size_t len;
			const char* str = lua_tolstring(L, idx, &len);
			walk.bytes(len);
			return MsgPack(std::string(str, len));
		}
		case LUA_TNUMBER:
			return number_to_msgpack(L, idx);
		case LUA_TBOOLEAN:
			return MsgPack(lua_toboolean(L, idx) != 0);
		default:
			return MsgPack::NIL();
	}
}

// The slices a collection runs in once its request is cancelled: REQUEST_SLICE (search_views.cc).
constexpr std::chrono::microseconds collect_slice{200};

}  // namespace


// Collects what a call left on its state, a `LUA_GCSTEP` at a time with a checkpoint between steps: the cycle in
// progress, then whole cycles until one frees almost nothing (each cycle halves a string table the call grew).
// A sweep first frees the run of dead objects at the head of the object list at once (lgc.c `sweeptolive`), and
// the newest objects are the call's, so a fresh live table goes at the head first. A cancellation does not end
// the collection: it runs on in slices of its own.
task::task<void> collect(detail::State& state, task::budget* budget) {
	lua_State* L = state.L;
	lua_newtable(L);
	lua_rawseti(L, LUA_REGISTRYINDEX, state.newest_ref);
	// A basic step is about 8 KiB of the heap: a cycle over a 64 MiB heap is about 8,000 steps. Past `max_steps`,
	// one full collection ends it (cycles always end; this bounds the loop should a step stop advancing).
	constexpr int max_steps = 1 << 16;
	constexpr int max_cycles = 16;
	std::optional<task::budget> own;
	std::size_t at_end = state.used;
	for (int steps = 0, cycles = 0;; ++steps) {
		if (steps == max_steps) {
			lua_gc(L, LUA_GCCOLLECT);
			break;
		}
		if (lua_gc(L, LUA_GCSTEP, 0) != 0) {
			const bool idle = state.used + 1024 > at_end;   // the last whole cycle freed under 1 KiB
			if (++cycles == max_cycles || (cycles >= 2 && idle)) {
				break;
			}
			at_end = state.used;
		}
		if (budget != nullptr) {
			try {
				co_await budget->checkpoint();
			} catch (const task::cancelled&) {
				budget = &own.emplace(collect_slice);
			}
		}
	}
	state.settled = state.used;
}

// Runs one call of the state's script on `doc`.
task::task<void> run(detail::State& state, const Limits& limits, std::string_view method, MsgPack& doc, const MsgPack& old_doc,
	const std::string& stored_params, const MsgPack& params, task::budget* budget) {
	lua_State* L = state.L;
	[[maybe_unused]] const auto owner = std::this_thread::get_id();
	Call call{&limits};
	call.count = limits.hook_period;
	call.prints_left = limits.max_print_bytes;
	Refs refs(L);
	lua_State* co;
	{
		// The call's environment, over the base.
		StackGuard guard(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, state.new_env_ref);
		lua_call(L, 0, 1);
		const int env = lua_gettop(L);
		auto set = [&](const char* key, auto&& push) {
			lua_pushstring(L, key);
			push();
			lua_rawset(L, env);
		};
		set("_method", [&] { lua_pushlstring(L, method.data(), method.size()); });
		set("_doc", [&] { push_msgpack(L, doc, 0); });
		set("_old_doc", [&] { push_msgpack(L, old_doc, 0); });
		auto merged_params = stored_params.empty() ? MsgPack::MAP() : MsgPack::unserialize(stored_params);
		merged_params.update(params);
		for (auto it = merged_params.begin(); it != merged_params.end(); ++it) {
			auto key = it->str();
			set(key.c_str(), [&] { push_msgpack(L, it.value(), 0); });
		}
		lua_pushvalue(L, env);
		refs.env = luaL_ref(L, LUA_REGISTRYINDEX);

		// Its own thread, anchored for the call; the chunk and the environment are its first resume's.
		co = lua_newthread(L);
		refs.thread = luaL_ref(L, LUA_REGISTRYINDEX);
		*static_cast<Call**>(lua_getextraspace(co)) = &call;
		call.co = co;
		lua_sethook(co, hook, LUA_MASKCOUNT, static_cast<int>(limits.hook_period));
		lua_rawgeti(L, LUA_REGISTRYINDEX, state.func_ref);
		lua_pushvalue(L, env);
		lua_xmove(L, co, 2);
	}

	int nargs = 1;
	for (;;) {
		assert(owner == std::this_thread::get_id());  // a state is its thread's own
		int nres = 0;
		Call* const outer = std::exchange(current_call, &call);
		state.metered = true;
		const int status = lua_resume(co, L, nargs, &nres);
		state.metered = false;
		current_call = outer;
		nargs = 0;
		if (status == LUA_YIELD) {
			if (budget != nullptr) {
				co_await budget->checkpoint();
			}
			continue;
		}
		if (status == LUA_OK) {
			break;
		}

		// An error: a bound the call crossed ends the request, whatever the error says (a script can
		// replace the error in flight); any other error is the script's own.
		std::size_t len = 0;
		const char* text = lua_type(co, -1) == LUA_TSTRING ? lua_tolstring(co, -1, &len) : nullptr;
		std::string message = text != nullptr ? std::string(text, std::min(len, limits.max_message_bytes)) : "(error object is not a string)";
		if (text != nullptr && len > limits.max_message_bytes) {
			message += " ...[truncated]";
		}
		lua_pop(co, 1);
		throw_if_condemned(call, limits);
		if (status == LUA_ERRMEM || message == "not enough memory") {
			THROW(ScriptLimitError, "Script exceeded its memory limit of {} bytes", limits.max_bytes);
		}
		if (message.find("(step budget)") != std::string::npos) {
			THROW(ScriptLimitError, "Script exceeded the pattern matcher's step budget: {}", message);
		}
		THROW(ScriptError, "Script error: {}", message);
	}
	throw_if_condemned(call, limits);

	// Apply the script's changes back onto the document.
	StackGuard guard(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, refs.env);
	lua_pushstring(L, "_doc");
	lua_rawget(L, -2);
	Walk walk{limits.max_result_nodes, limits.max_result_bytes};
	doc = lua_to_msgpack(L, -1, walk, 0);
}


}; // End namespace lua

#endif
