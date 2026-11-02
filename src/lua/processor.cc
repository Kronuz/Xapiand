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

#include "processor.h"

#if XAPIAND_LUA

#include <cassert>
#include <cstdint>                                // for int64_t
#include <cstdlib>                                // for realloc, free
#include <functional>                             // for std::hash
#include <new>                                    // for std::bad_alloc
#include <string>
#include <string_view>

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
#include "url_parser.h"                           // for urldecode

namespace lua {

namespace detail {

// The live bytes of a state, and the cap that `metered` switches on (only while a script runs).
struct Heap {
	std::size_t used = 0;
	std::size_t limit = 0;
	bool metered = false;
};

}  // namespace detail

namespace {

// The registry key of the sentinel a bound raises: no pcall of a script catches it.
const char kill_key = 0;

// The bounds one call has crossed, kept in the thread's extra space for the hook.
struct Call {
	const Limits* limits;
	std::size_t instructions = 0;
	std::size_t unyielded = 0;
	const char* reason = nullptr;
};

void* lua_alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize) {
	auto* heap = static_cast<detail::Heap*>(ud);
	const std::size_t old = ptr != nullptr ? osize : 0;  // osize is a type tag for a new block
	if (nsize == 0) {
		heap->used -= old;
		std::free(ptr);
		return nullptr;
	}
	if (nsize > old && heap->metered && heap->used + (nsize - old) > heap->limit) {
		return nullptr;
	}
	void* block = std::realloc(ptr, nsize);
	if (block == nullptr) {
		return nullptr;
	}
	heap->used = heap->used - old + nsize;
	return block;
}

// Counts the call's instructions, ends it past a bound, and yields wherever Lua allows it.
void hook(lua_State* L, lua_Debug* /*unused*/) {
	auto* call = *static_cast<Call**>(lua_getextraspace(L));
	const bool yieldable = lua_isyieldable(L) != 0;
	call->instructions += call->limits->hook_period;
	if (call->reason == nullptr) {
		if (call->instructions > call->limits->max_instructions) {
			call->reason = "instruction";
		} else if (!yieldable && ++call->unyielded > call->limits->max_unyielded_hooks) {
			call->reason = "callback";
		}
	}
	if (call->reason != nullptr) {
		lua_rawgetp(L, LUA_REGISTRYINDEX, &kill_key);
		lua_error(L);
	}
	if (yieldable) {
		call->unyielded = 0;
		lua_yield(L, 0);
	}
}

// The base: copies of the opened libraries, a Lua `table.sort` (comparators yield), pcall and xpcall
// that rethrow what ends a call, and a factory of per-call environments. It is run once per state with
// the sentinel as its argument, then empties the state's globals, which `load` would otherwise hand out.
constexpr std::string_view base_chunk = R"LUA(
local KILL = ...
local pairs, setmetatable, getmetatable, type, error, pcall, xpcall, select, rawget =
	pairs, setmetatable, getmetatable, type, error, pcall, xpcall, select, rawget
local find = string.find
local native_sort = table.sort

local function copy(t)
	local r = {}
	for k, v in pairs(t) do r[k] = v end
	return r
end
local libs = { string = copy(string), table = copy(table), math = copy(math), utf8 = copy(utf8) }

-- A merge sort in Lua: a comparator is a Lua function called from Lua, so the hook can yield in it.
function libs.table.sort(t, comp)
	if comp == nil then return native_sort(t) end
	if type(comp) ~= "function" then
		error("bad argument #2 to 'sort' (function expected, got " .. type(comp) .. ")", 2)
	end
	local n = #t
	local a, b = {}, {}
	for i = 1, n do a[i] = t[i] end
	local width = 1
	while width < n do
		local i = 1
		while i <= n do
			local mid, hi = i + width, i + 2 * width
			if mid > n + 1 then mid = n + 1 end
			if hi > n + 1 then hi = n + 1 end
			local l, r, k = i, mid, i
			while l < mid and r < hi do
				if comp(a[r], a[l]) then b[k] = a[r]; r = r + 1 else b[k] = a[l]; l = l + 1 end
				k = k + 1
			end
			while l < mid do b[k] = a[l]; l = l + 1; k = k + 1 end
			while r < hi do b[k] = a[r]; r = r + 1; k = k + 1 end
			i = hi
		end
		a, b = b, a
		width = width * 2
	end
	for i = 1, n do t[i] = a[i] end
end

-- What no script may catch: a bound crossed (the sentinel), the matcher's step budget, memory.
local function fatal(e)
	return e == KILL or (type(e) == "string" and (e == "not enough memory" or find(e, "(step budget)", 1, true) ~= nil))
end
local function after(ok, ...)
	if not ok and fatal((...)) then error((...), 0) end
	return ok, ...
end
local pcall0, xpcall0 = pcall, xpcall
local function pcall1(f, ...) return after(pcall0(f, ...)) end
local function xpcall1(f, h, ...)
	return after(xpcall0(f, function(e) if fatal(e) then return e end return h(e) end, ...))
end

-- A finalizer runs with hooks off, so a __gc that never returns would stall the loop: scripts get none.
local function setmetatable1(t, mt)
	if type(mt) == "table" and rawget(mt, "__gc") ~= nil then
		error("a script cannot set a __gc metamethod", 2)
	end
	return setmetatable(t, mt)
end

local base = {
	_VERSION = _VERSION, assert = assert, error = error, getmetatable = getmetatable, ipairs = ipairs,
	next = next, pairs = pairs, pcall = pcall1, print = print, rawequal = rawequal, rawget = rawget,
	rawlen = rawlen, rawset = rawset, select = select, setmetatable = setmetatable1, tonumber = tonumber,
	tostring = tostring, type = type, xpcall = xpcall1,
}
local env_mt = { __index = base, __metatable = false }
local proxy_mt = {}
for name, lib in pairs(libs) do proxy_mt[name] = { __index = lib, __metatable = false } end
getmetatable("").__metatable = "string"

for k in pairs(_ENV) do _ENV[k] = nil end

return function()
	local env = setmetatable({}, env_mt)
	env._G = env
	for name, mt in pairs(proxy_mt) do env[name] = setmetatable({}, mt) end
	return env
end
)LUA";

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


struct Walk {
	std::size_t nodes_left;
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
	if (depth > max_depth || walk.nodes_left == 0 || !lua_checkstack(L, 4)) {
		THROW(ClientError, "The script's document is too large or too deeply nested");
	}
	--walk.nodes_left;
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
				MsgPack arr = MsgPack::ARRAY();
				for (std::size_t i = 1; i <= seq; ++i) {
					lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
					arr.push_back(lua_to_msgpack(L, -1, walk, depth + 1));
					lua_pop(L, 1);
				}
				return arr;
			}
			MsgPack map = MsgPack::MAP();
			lua_pushnil(L);
			while (lua_next(L, idx) != 0) {
				std::string key;
				const int kt = lua_type(L, -2);
				if (kt == LUA_TSTRING) {
					std::size_t len;
					const char* str = lua_tolstring(L, -2, &len);
					key.assign(str, len);
				} else if (kt == LUA_TNUMBER) {
					key = std::to_string(key_integer(L, -2));
				} else {
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


// The object of the document a script's foreign link names.
task::task<MsgPack>
read_foreign_script(std::string foreign_path, std::string id)
{
	Endpoint endpoint(foreign_path);
	auto endpoints = co_await XapiandManager::resolve_index_endpoints(endpoint);
	if (endpoints.empty()) {
		THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
	}
	DatabaseHandler _db_handler(endpoints, DB_OPEN);
	auto doc = co_await _db_handler.get_document(id);
	co_return co_await doc.get_obj();
}


// This thread's compiled scripts.
class ScriptLRU : public lru::lru<std::string, std::shared_ptr<Processor>> {
public:
	explicit ScriptLRU(ssize_t max_size) :
		lru::lru(max_size) { }
};

ScriptLRU& cache() {
	thread_local ScriptLRU script_lru(opts.scripts_cache_size);
	return script_lru;
}

std::shared_ptr<Processor> cached(const std::string& name, std::string_view body) {
	auto& lru = cache();
	auto it = lru.find(name);
	if (it != lru.end()) {
		if (body.empty() || it->second->get_hash() == std::hash<std::string_view>()(body)) {
			return it->second;
		}
	}
	return nullptr;
}

std::shared_ptr<Processor> remember(const std::string& name, std::shared_ptr<Processor> processor) {
	L_INFO("Script {} ({:x}) compiled and ready.", repr(name), processor->get_hash());
	auto inserted = cache().emplace(name, processor);
	if (!inserted.second) {
		inserted.first->second = processor;  // the script changed under its name
	}
	return processor;
}

}  // namespace


Processor::Processor(std::string_view name, std::string_view body, MsgPack params, Limits limits_) :
	hash(std::hash<std::string_view>()(body)),
	heap(std::make_unique<detail::Heap>()),
	script_params(std::move(params)),
	limits(limits_),
	owner(std::this_thread::get_id())
{
	heap->limit = limits.max_bytes;
	L = lua_newstate(lua_alloc, heap.get());
	if (L == nullptr) {
		throw std::bad_alloc();
	}
	try {
		// A safe subset of the Lua standard library: enough to transform documents, without io/os
		// (no filesystem or process access from a script).
		static const struct { const char* name; lua_CFunction opener; } libs[] = {
			{ LUA_GNAME, luaopen_base },
			{ LUA_STRLIBNAME, luaopen_string },
			{ LUA_TABLIBNAME, luaopen_table },
			{ LUA_MATHLIBNAME, luaopen_math },
			{ LUA_UTF8LIBNAME, luaopen_utf8 },
		};
		for (const auto& lib : libs) {
			luaL_requiref(L, lib.name, lib.opener, 1);
			lua_pop(L, 1);
		}

		// The sentinel, then the base that closes over it.
		lua_newtable(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &kill_key);
		if (luaL_loadbufferx(L, base_chunk.data(), base_chunk.size(), "=xapiand-base", "t") != LUA_OK) {
			THROW(ClientError, "Script base failed to load: {}", lua_tostring(L, -1));
		}
		lua_rawgetp(L, LUA_REGISTRYINDEX, &kill_key);
		if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
			THROW(ClientError, "Script base failed to run: {}", lua_tostring(L, -1));
		}
		new_env_ref = luaL_ref(L, LUA_REGISTRYINDEX);

		script_params.lock();

		// The body on the line of `local _ENV`, so line numbers hold.
		std::string source = "local _ENV = ...; ";
		source.append(body);
		const std::string chunk_name(name);
		if (luaL_loadbufferx(L, source.data(), source.size(), chunk_name.c_str(), "t") != LUA_OK) {
			THROW(ClientError, "Script {} syntax error: {}", repr(name), lua_tostring(L, -1));
		}
		func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	} catch (...) {
		lua_close(L);
		throw;
	}
}


Processor::~Processor()
{
	lua_close(L);
}


task::task<void>
Processor::operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params, task::budget* budget)
{
	assert(owner == std::this_thread::get_id());  // a Processor is its thread's own

	Call call{&limits};
	Refs refs(L);
	lua_State* co;
	{
		// The call's environment, over the base.
		StackGuard guard(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, new_env_ref);
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
		auto merged_params = script_params;
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
		lua_sethook(co, hook, LUA_MASKCOUNT, static_cast<int>(limits.hook_period));
		lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
		lua_pushvalue(L, env);
		lua_xmove(L, co, 2);
	}

	int nargs = 1;
	for (;;) {
		assert(owner == std::this_thread::get_id());
		int nres = 0;
		heap->metered = true;
		const int status = lua_resume(co, L, nargs, &nres);
		heap->metered = false;
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

		// An error: a bound the call crossed ends the request; any other leaves the document as it was.
		std::string message = lua_type(co, -1) == LUA_TSTRING ? lua_tostring(co, -1) : "(error object is not a string)";
		lua_rawgetp(co, LUA_REGISTRYINDEX, &kill_key);
		const bool killed = lua_rawequal(co, -1, -2) != 0;
		lua_pop(co, 2);
		if (killed) {
			if (std::string_view(call.reason) == "instruction") {
				THROW(ScriptLimitError, "Script exceeded its instruction limit of {}", limits.max_instructions);
			}
			THROW(ScriptLimitError, "Script ran more than {} instructions in a callback that cannot yield", limits.max_unyielded_hooks * limits.hook_period);
		}
		if (status == LUA_ERRMEM || message == "not enough memory") {
			THROW(ScriptLimitError, "Script exceeded its memory limit of {} bytes", limits.max_bytes);
		}
		if (message.find("(step budget)") != std::string::npos) {
			THROW(ScriptLimitError, "Script exceeded the pattern matcher's step budget: {}", message);
		}
		L_ERR("Script error: {}", message);
		co_return;
	}

	// Apply the script's changes back onto the document.
	StackGuard guard(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, refs.env);
	lua_pushstring(L, "_doc");
	lua_rawget(L, -2);
	Walk walk{limits.max_bytes / 16};
	doc = lua_to_msgpack(L, -1, walk, 0);
}


std::shared_ptr<Processor>
Processor::compile_local(const Script& script)
{
	auto name_body = script.get_name_body();
	const std::string name(name_body.first);
	if (auto processor = cached(name, name_body.second)) {
		return processor;
	}
	return remember(name, std::make_shared<Processor>(name_body.first, name_body.second, script.get_params()));
}


task::task<std::shared_ptr<Processor>>
Processor::compile(const Script& script)
{
	if (script.get_types()[SPC_FOREIGN_TYPE] != FieldType::foreign) {
		co_return compile_local(script);
	}

	const std::string key(script.get_endpoint());
	if (auto processor = cached(key, {})) {
		co_return processor;
	}

	std::string_view foreign_path_view, foreign_id_view;
	split_path_id(script.get_endpoint(), foreign_path_view, foreign_id_view);
	std::string foreign_path = urldecode(foreign_path_view);
	std::string foreign_id = urldecode(foreign_id_view);
	MsgPack obj;
	try {
		obj = co_await read_foreign_script(foreign_path, foreign_id);
	} catch (const engine::Error& error) {
		switch (error.kind) {
			case engine::errc::doc_not_found: {
				THROW(ClientError, "Foreign script {}/{} doesn't exist", foreign_path, foreign_id);
				break;
			}
			case engine::errc::not_found: {
				THROW(ClientError, "Foreign script database {} doesn't exist", repr(foreign_path));
				break;
			}
			default:
				throw;
		}
	}
	// If there's no selector use "script" (to be consistent with "schema" field):
	obj = obj[SCRIPT_FIELD_NAME];
	Script foreign_script(obj);
	if (foreign_script.get_types()[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		THROW(ClientError, "Nested foreign scripts not supported!");
	}
	auto name_body = foreign_script.get_name_body();
	MsgPack params = foreign_script.get_params();
	params.update(script.get_params());

	// The read may have moved this task to another thread: the processor is built where it will run.
	if (auto processor = cached(key, {})) {
		co_return processor;
	}
	co_return remember(key, std::make_shared<Processor>(name_body.first, name_body.second, std::move(params)));
}


}; // End namespace lua

#endif
