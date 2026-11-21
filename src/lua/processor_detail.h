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

#pragma once

#include "processor.h"

#if XAPIAND_LUA

#include <cstddef>                                // for std::size_t
#include <string>
#include <string_view>

extern "C" {
#include <lua.h>
}

#include "msgpack.h"                              // for MsgPack
#include "task.hh"                                // for task::task, task::budget


namespace lua {

namespace detail {

// A lua_State with the base and one compiled script, and the live bytes its allocator meters against
// the cap that `metered` switches on (only while a script runs).
struct State {
	std::size_t used = 0;
	std::size_t limit = 0;
	bool metered = false;
	std::size_t settled = 0;            // `used` after the last collection
	lua_State* L = nullptr;
	int func_ref = 0;                   // the compiled chunk, in the registry
	int new_env_ref = 0;                // the base's environment factory, in the registry
	int newest_ref = 0;                 // a table made before each collection, in the registry (see `collect`)

	State(std::string_view name, const std::string& source, std::size_t limit);
	~State();
	State(const State&) = delete;
	State& operator=(const State&) = delete;
};

}  // namespace detail

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

extern thread_local Call* current_call;
extern thread_local std::size_t printed;

void hook(lua_State* L, lua_Debug* ar);
void throw_if_condemned(const Call& call, const Limits& limits);

task::task<void> collect(detail::State& state, task::budget* budget);
task::task<void> run(detail::State& state, const Limits& limits, std::string_view method, MsgPack& doc, const MsgPack& old_doc,
	const std::string& stored_params, const MsgPack& params, task::budget* budget);

}  // namespace lua

#endif
