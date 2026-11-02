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

#include <algorithm>                              // for std::min
#include <cassert>
#include <cstdint>                                // for int64_t
#include <cstdlib>                                // for realloc, free
#include <functional>                             // for std::hash
#include <new>                                    // for std::bad_alloc
#include <string>
#include <string_view>
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

// One call's meters, kept in the thread's extra space for the hook and, while the call is resumed,
// in `current_call` for the library patch (contrib/lua/xapiand-limits.patch) to charge.
struct Call {
	const Limits* limits;
	lua_State* co = nullptr;
	std::size_t instructions = 0;
	std::size_t unyielded = 0;         // instructions since the last yield point, while the call cannot yield
	std::size_t count = 0;             // the interval the count hook is armed with
	std::size_t work = 0;              // the library's and the VM's work so far (units of about a nanosecond)
	std::size_t slice_work = 0;        // ...since the last yield
	std::size_t prints_left = 0;       // bytes `print` may still log
	const char* reason = nullptr;      // the bound that condemned the call: it cannot be resumed or caught
};

thread_local Call* current_call = nullptr;

constexpr std::size_t max_print_line = 1024;

static_assert(Limits{}.max_result_bytes == BUFFERED_BODY_CAP, "a script's result is bounded like the body it replaces");

void hook(lua_State* L, lua_Debug* ar);

void arm(Call& call, std::size_t count) {
	call.count = count;
	lua_sethook(call.co, hook, LUA_MASKCOUNT, static_cast<int>(count));
}

// Condemns the call: every instruction from now on raises, whatever a script catches.
void condemn(Call& call, const char* why) {
	if (call.reason == nullptr) {
		call.reason = why;
	}
	if (call.count != 1) {
		arm(call, 1);
	}
}

// Adds work; past the cap the call is condemned, past a slice the hook is armed to yield at the next instruction.
void note(Call& call, std::size_t units) {
	call.work += units;
	call.slice_work += units;
	if (call.reason == nullptr) {
		if (call.work > call.limits->max_work) {
			condemn(call, "work");
		} else if (call.slice_work > call.limits->slice_work && call.count != 1) {
			arm(call, 1);
		}
	}
}

void raise_kill(lua_State* L) {
	lua_rawgetp(L, LUA_REGISTRYINDEX, &kill_key);
	lua_error(L);
}

void* lua_alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize) {
	auto* heap = static_cast<detail::Heap*>(ud);
	const std::size_t old = ptr != nullptr ? osize : 0;  // osize is a type tag for a new block
	if (nsize == 0) {
		heap->used -= old;
		std::free(ptr);
		return nullptr;
	}
	if (nsize > old && heap->metered && heap->used + (nsize - old) > heap->limit) {
		if (current_call != nullptr) {
			condemn(*current_call, "memory");
		}
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
	const auto& limits = *call->limits;
	const bool yieldable = lua_isyieldable(L) != 0;
	call->instructions += call->count;
	if (call->reason == nullptr) {
		if (call->instructions > limits.max_instructions) {
			condemn(*call, "instruction");
		} else if (call->work > limits.max_work) {
			condemn(*call, "work");
		} else if (!yieldable) {
			call->unyielded += call->count;
			if (call->unyielded > limits.max_unyielded_hooks * limits.hook_period) {
				condemn(*call, "callback");
			}
		}
	}
	if (call->reason != nullptr) {
		raise_kill(L);
	}
	if (call->count != limits.hook_period) {
		arm(*call, limits.hook_period);
	}
	call->slice_work = 0;
	if (yieldable) {
		call->unyielded = 0;
		lua_yield(L, 0);
	}
}

// What the library patch and the base call into.
int host_condemned(lua_State* L) {
	lua_pushboolean(L, current_call != nullptr && current_call->reason != nullptr);
	return 1;
}

// print(s): logs the first bytes of s, within the call's budget.
int host_print(lua_State* L) {
	std::size_t len = 0;
	const char* str = luaL_checklstring(L, 1, &len);
	Call* call = current_call;
	if (call == nullptr || call->prints_left == 0) {
		return 0;
	}
	const std::size_t take = std::min({len, call->prints_left, max_print_line});
	const bool truncated = take < len || take == call->prints_left;
	call->prints_left -= take;
	if (truncated) {
		call->prints_left = 0;
	}
	L_INFO("Script print: {}{}", repr(std::string_view(str, take)), truncated ? " ...[truncated]" : "");
	return 0;
}

// The base: copies of the opened libraries (less the functions the host cannot charge), a Lua
// `table.sort`, a `print` that logs within a budget, pcall and xpcall that rethrow what ends a call,
// and a factory of per-call environments. It is run once per state with the sentinel and two host
// functions as its arguments, then empties the state's globals, which `load` would otherwise hand out.
constexpr std::string_view base_chunk = R"LUA(
local KILL, condemned, host_print = ...
local pairs, setmetatable, getmetatable, type, error, pcall, xpcall, select, rawget, tostring =
	pairs, setmetatable, getmetatable, type, error, pcall, xpcall, select, rawget, tostring
local find, concat = string.find, table.concat

local function copy(t)
	local r = {}
	for k, v in pairs(t) do r[k] = v end
	return r
end
local libs = { string = copy(string), table = copy(table), math = copy(math), utf8 = copy(utf8) }

-- Functions whose cost the host cannot charge are retired.
libs.string.dump, libs.string.pack, libs.string.packsize, libs.string.unpack = nil, nil, nil, nil

-- A merge sort in Lua, so comparisons are instructions: the hook counts and yields in them.
function libs.table.sort(t, comp)
	if comp == nil then
		comp = function(x, y) return x < y end
	elseif type(comp) ~= "function" then
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

-- What no script may catch: once the host has condemned the call (a bound crossed), whatever value
-- the error carries, so a handler that replaces it changes nothing. The values are the fallback.
local function fatal(e)
	return condemned() or e == KILL or (type(e) == "string" and (e == "not enough memory" or find(e, "(step budget)", 1, true) ~= nil))
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

-- A finalizer runs with hooks off and a close handler or a weak table has no bound: scripts get none.
local function setmetatable1(t, mt)
	if type(mt) == "table" then
		if rawget(mt, "__gc") ~= nil then error("a script cannot set a __gc metamethod", 2) end
		if rawget(mt, "__close") ~= nil then error("a script cannot set a __close metamethod", 2) end
		if rawget(mt, "__mode") ~= nil then error("a script cannot set a __mode metamethod", 2) end
	end
	return setmetatable(t, mt)
end

local function print1(...)
	local n = select("#", ...)
	local parts = {}
	for i = 1, n do parts[i] = tostring((select(i, ...))) end
	return host_print(concat(parts, "\t"))
end

local base = {
	_VERSION = _VERSION, assert = assert, error = error, getmetatable = getmetatable, ipairs = ipairs,
	next = next, pairs = pairs, pcall = pcall1, print = print1, rawequal = rawequal, rawget = rawget,
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

// Ends the request when the call crossed a bound, whatever its error or result says.
void throw_if_condemned(const Call& call, const Limits& limits) {
	if (call.reason == nullptr) {
		return;
	}
	const std::string_view reason = call.reason;
	if (reason == "instruction") {
		THROW(ScriptLimitError, "Script exceeded its instruction limit of {}", limits.max_instructions);
	}
	if (reason == "work") {
		THROW(ScriptLimitError, "Script exceeded its work limit of {} (what its string, table and pattern calls touch)", limits.max_work);
	}
	if (reason == "memory") {
		THROW(ScriptLimitError, "Script exceeded its memory limit of {} bytes", limits.max_bytes);
	}
	if (reason == "steps") {
		THROW(ScriptLimitError, "Script exceeded the pattern matcher's step budget (pattern too complex)");
	}
	THROW(ScriptLimitError, "Script ran more than {} instructions in a callback that cannot yield", limits.max_unyielded_hooks * limits.hook_period);
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
		lua_pushcfunction(L, host_condemned);
		lua_pushcfunction(L, host_print);
		if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
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
	call.count = limits.hook_period;
	call.prints_left = limits.max_print_bytes;
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
		call.co = co;
		lua_sethook(co, hook, LUA_MASKCOUNT, static_cast<int>(limits.hook_period));
		lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
		lua_pushvalue(L, env);
		lua_xmove(L, co, 2);
	}

	int nargs = 1;
	for (;;) {
		assert(owner == std::this_thread::get_id());
		int nres = 0;
		Call* const outer = std::exchange(current_call, &call);
		heap->metered = true;
		const int status = lua_resume(co, L, nargs, &nres);
		heap->metered = false;
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


// The library patch's meter (contrib/lua/xapiand-limits.patch); no call is running, no charge.
extern "C" {

void lua_xnote(std::size_t units)
{
	if (auto* call = lua::current_call) {
		lua::note(*call, units);
	}
}

void lua_xcharge(lua_State* L, std::size_t units)
{
	if (auto* call = lua::current_call) {
		lua::note(*call, units);
		if (call->reason != nullptr) {
			lua::raise_kill(L);
		}
	}
}

void lua_xcondemn(lua_State* /*L*/, const char* why)
{
	if (auto* call = lua::current_call) {
		lua::condemn(*call, why);
	}
}

int lua_xcloseok(void)
{
	auto* call = lua::current_call;
	return call != nullptr && call->limits->allow_close;
}

}

#endif
