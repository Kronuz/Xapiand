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

#include "config.h"  // for XAPIAND_LUA

#if XAPIAND_LUA

#include <memory>            // for std::shared_ptr
#include <mutex>             // for std::mutex
#include <string_view>

// Xapiand's logging system defines a function-like macro `L(...)`, which
// collides with sol2's pervasive `lua_State* L` members (written as `L(...)` in
// constructor initializer lists throughout sol2). Shield the sol2 include from
// it: this is order-independent, so processor.h is safe to include after log.h.
#pragma push_macro("L")
#undef L
#include <sol/sol.hpp>       // the Lua (sol2) scripting engine
#pragma pop_macro("L")

#include "msgpack.h"         // for MsgPack
#include "script.h"          // for Script


namespace lua {

// Document-transform scripting, backed by Lua (sol2). A script runs with the
// globals `_method`, `_doc`, `_old_doc`, and the script's parameters (each by
// name) in scope, mutating `_doc`. The document is exposed as a Lua table
// (MsgPack <-> table conversion happens around each call), so scripts are plain,
// idiomatic Lua.
class Processor {
	size_t hash;
	sol::state state;
	sol::protected_function func;   // the compiled script body
	MsgPack script_params;
	std::mutex mtx;                 // a Lua state is not re-entrant; serialize calls

public:
	Processor(const Script& script);

	void operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params);
	static std::shared_ptr<Processor> compile(const Script& script);

	size_t get_hash() {
		return hash;
	}
};

}; // End namespace lua

#endif
