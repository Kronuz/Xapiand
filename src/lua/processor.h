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

#include "config.h"  // for XAPIAND_LUA

#if XAPIAND_LUA

#include <chrono>            // for std::chrono::milliseconds
#include <cstddef>           // for std::size_t
#include <memory>            // for std::shared_ptr
#include <string>
#include <string_view>
#include <thread>            // for std::thread::id

#include "msgpack.h"         // for MsgPack
#include "script.h"          // for Script
#include "task.hh"           // for task::task, task::budget

struct lua_State;

namespace lua::detail {
struct Heap;
}

namespace lua {

// The bounds of one script call (D10). The defaults are the design's estimates.
struct Limits {
	std::size_t hook_period = 10'000;               // VM instructions between two hook calls
	std::size_t max_instructions = 100'000'000;     // per call
	std::size_t max_unyielded_hooks = 100;          // hook periods in a row without a yield point
	std::size_t max_bytes = 64u << 20;              // live bytes of a script's state
	std::size_t max_work = 500'000'000;             // per call: matcher steps, bytes and elements the library touches (about a nanosecond each)
	std::size_t slice_work = 200'000;               // work after which the call yields at its next instruction
	std::size_t max_print_bytes = 4096;             // what `print` may log per call
	std::size_t max_message_bytes = 1024;           // a script error's message, as logged and as answered
	std::size_t max_result_bytes = 16u << 20;       // string and key bytes of the converted result: the body cap (BUFFERED_BODY_CAP)
	std::size_t max_result_nodes = (16u << 20) / 64; // value units of the converted result (a map is 8, an array 2, a scalar 1): about 500 host bytes a unit
	bool allow_close = false;                       // tests only: lets a script declare to-be-closed variables
};


// Document-transform scripting on Lua 5.4. A script compiles once into a chunk of the form
// `local _ENV = ...; <body>`; each call runs it in its own lua_newthread with its own environment
// table (`_method`, `_doc`, `_old_doc`, the params, `_G` as the table itself, and its own
// proxies of `string`, `table`, `math` and `utf8`), over a base that holds the opened libraries.
// A call yields to its reactor at the hook (every `hook_period` instructions) wherever Lua
// allows it, so scripts run in slices; calls of one script interleave without sharing globals.
//
// A Processor belongs to the thread that compiled it (each thread keeps its own cache); the byte
// cap is charged to the state's live bytes, so interleaved calls on one script share it.
class Processor {
	size_t hash;
	std::unique_ptr<detail::Heap> heap;
	lua_State* L = nullptr;
	int func_ref = 0;               // the compiled chunk, in the registry
	int new_env_ref = 0;            // the base's environment factory, in the registry
	MsgPack script_params;
	Limits limits;
	std::thread::id owner;
	std::chrono::steady_clock::time_point checked;   // a foreign script: when its document was last read

public:
	// Compiles `body` as the script `name`. A syntax error is a ClientError.
	Processor(std::string_view name, std::string_view body, MsgPack params, Limits limits = {});
	~Processor();
	Processor(const Processor&) = delete;
	Processor& operator=(const Processor&) = delete;

	// Runs the script on `doc`. `budget` (the request's slice) is awaited at each yield; null runs
	// the call to its end. A bound crossed (instructions, work, memory, matcher steps, a long run of
	// callbacks) is a ScriptLimitError, a result past its budget a ScriptResultTooLarge, and any
	// other script error a ScriptError; the document is as it was after any of them.
	task::task<void> operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params, task::budget* budget = nullptr);

	// The calling thread's compiled script for `script`; a foreign script's document is read (awaited) on a miss,
	// and again once a cached one is older than `foreign_revalidation()`, so a rewritten script takes effect.
	static task::task<std::shared_ptr<Processor>> compile(const Script& script);
	// The same for a script with a body of its own: no read, so no wait.
	static std::shared_ptr<Processor> compile_local(const Script& script);

	// How long a cached foreign script is trusted before its document is read again.
	static std::chrono::milliseconds foreign_revalidation();
	static void set_foreign_revalidation(std::chrono::milliseconds age);

	// Test seams: this thread's cached script under a local name, and a foreign reference.
	static std::shared_ptr<Processor> cached_local(const std::string& name);
	static std::shared_ptr<Processor> cached_foreign(const std::string& reference);
	static void remember_foreign(const std::string& reference, std::shared_ptr<Processor> processor);

	size_t get_hash() const {
		return hash;
	}
};

}; // End namespace lua

#endif
