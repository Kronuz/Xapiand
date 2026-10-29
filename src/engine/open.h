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

#include <cstddef>                 // for std::size_t
#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::string

#include "engine/kind.h"           // for Kind
#include "engine/shard.h"          // for Shard
#include "task.hh"                 // for task::task, task::scheduler_ref


namespace engine {

struct Opened {
	std::unique_ptr<Shard> shard;
	bool created = false;
};

// Opens (or, when create_missing, creates) the local shard at `path` of the given engine. The shard
// hops onto `on` for each call, as the engines' own openers do.
task::task<Opened> open_local(Kind kind, const std::string& path, bool writable, bool create_missing, task::scheduler_ref on = {});

// The engine's own integrity check of the shard at `path`, by the engine whose files are there
// (kind_of). `errors` counts what the check found; `failure` says why the check could not run or
// did not pass.
struct CheckResult {
	std::size_t errors = 0;
	std::string failure;
};
task::task<CheckResult> check(const std::string& path, task::scheduler_ref on = {});

}  // namespace engine
