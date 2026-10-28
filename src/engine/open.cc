/*
 * Copyright (c) 2026 Dubalu LLC
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

#include "engine/open.h"

#include "engine/strata/shard.h"   // for engine::strata::open_local
#include "engine/xapian/shard.h"   // for engine::xapian::open_local


namespace engine {

task::task<Opened>
open_local(Kind kind, const std::string& path, bool writable, bool create_missing, task::scheduler_ref on)
{
	Opened result;
	if (kind == Kind::strata) {
		auto opened = co_await strata::open_local(path, writable, create_missing, on);
		result.shard = std::move(opened.shard);
		result.created = opened.created;
	} else {
		auto opened = co_await xapian::open_local(path, writable, create_missing, on);
		result.shard = std::move(opened.shard);
		result.created = opened.created;
	}
	co_return result;
}

}  // namespace engine
