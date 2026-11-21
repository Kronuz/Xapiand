/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// What the Lua tests share: the call helpers and the scripts more than one piece uses.

#include "../test_harness.h"
#include "../utils_node.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "endpoint.h"
#include "lua/exception.h"
#include "lua/processor.h"
#include "opts.h"
#include "task.hh"

#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "database/flags.h"
#include "database/handler.h"
#include "engine/sync.h"
#include "manager.h"
#include "msgpack.h"
#include "traceback.h"
#include "xapiand_exception.h"

namespace detail {

using lua::Limits;
using lua::Processor;

// What one call left behind.
struct Outcome {
	MsgPack doc;
	bool limit = false;      // ScriptLimitError
	bool too_large = false;  // ScriptResultTooLarge
	bool script_error = false;   // ScriptError: the script's own
	bool cancelled = false;
	bool failed = false;     // any other exception
	std::string what;
	bool ok() const { return !limit && !too_large && !script_error && !cancelled && !failed; }
};

Outcome outcome_of(std::exception_ptr error);

MsgPack doc_of(std::initializer_list<std::pair<const char*, int64_t>> fields);

// Runs one call to its end on this thread (no budget: it never suspends).
Outcome call(Processor& processor, MsgPack doc, const MsgPack& params = MsgPack::MAP());

Outcome call(const std::string& body, MsgPack doc, Limits limits = {});

// A call that runs on a scheduler under a budget, to interleave with others.
struct Pending {
	MsgPack doc;
	MsgPack old_doc = MsgPack::MAP();
	MsgPack params = MsgPack::MAP();
	bool finished = false;
	Outcome outcome;
};

void start(task::inline_scheduler& scheduler, Pending& pending, Processor& processor, task::budget& budget);

constexpr std::chrono::nanoseconds every_checkpoint{1};   // a slice so short that every checkpoint yields

// A nil assigned to a field leaves no field.
bool absent(const MsgPack& doc, const char* key);

std::string str(const MsgPack& doc, const char* key);

Limits quick();

// Other work on the loop: `rounds` turns, then it reports whether the script had finished.
task::task<void> other_requests(std::size_t rounds, std::size_t& turns, const bool& script_finished, bool& script_finished_at_end);

// About 800,000 short strings in 800 tables, alive at the call's end, and a print once they are built.
extern const std::string short_strings_800k;

}  // namespace detail
