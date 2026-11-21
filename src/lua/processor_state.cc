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

// The Lua state of a script: its metered allocator, count hook, host functions, base chunk and the library patch's meters.

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

thread_local Call* current_call = nullptr;

thread_local std::size_t printed = 0;   // bytes `print` has handed to the log on this thread

namespace {

// The registry key of the sentinel a bound raises: no pcall of a script catches it.
const char kill_key = 0;

constexpr std::size_t max_print_line = 1024;

static_assert(Limits{}.max_result_bytes == BUFFERED_BODY_CAP, "a script's result is bounded like the body it replaces");

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
		} else if (call.slice_work > call.limits->max_call_work) {
			condemn(call, "call");  // one stretch no hook can interrupt would hold the reactor too long
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
	auto* heap = static_cast<detail::State*>(ud);
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
	printed += take;
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

}  // namespace


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
	if (reason == "call") {
		THROW(ScriptLimitError, "Script exceeded its per-call work limit of {} (one library call or pattern match touched too much)", limits.max_call_work);
	}
	if (reason == "memory") {
		THROW(ScriptLimitError, "Script exceeded its memory limit of {} bytes", limits.max_bytes);
	}
	if (reason == "steps") {
		THROW(ScriptLimitError, "Script exceeded the pattern matcher's step budget (pattern too complex)");
	}
	THROW(ScriptLimitError, "Script ran more than {} instructions in a callback that cannot yield", limits.max_unyielded_hooks * limits.hook_period);
}


namespace detail {

State::State(std::string_view name, const std::string& source, std::size_t limit_) :
	limit(limit_)
{
	L = lua_newstate(lua_alloc, this);
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
		lua_newtable(L);
		newest_ref = luaL_ref(L, LUA_REGISTRYINDEX);

		const std::string chunk_name(name);
		if (luaL_loadbufferx(L, source.data(), source.size(), chunk_name.c_str(), "t") != LUA_OK) {
			THROW(ClientError, "Script {} syntax error: {}", repr(name), lua_tostring(L, -1));
		}
		func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_gc(L, LUA_GCCOLLECT);   // the compiler's garbage
		settled = used;
	} catch (...) {
		lua_close(L);
		throw;
	}
}


State::~State()
{
	lua_close(L);
}

}  // namespace detail

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

std::size_t lua_xcallwork(void)
{
	auto* call = lua::current_call;
	return call != nullptr ? call->limits->max_call_work : static_cast<std::size_t>(-1);
}

int lua_xcloseok(void)
{
	auto* call = lua::current_call;
	return call != nullptr && call->limits->allow_close;
}

}

#endif
