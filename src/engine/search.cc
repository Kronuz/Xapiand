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


#include "engine/search.h"

#include <span>                    // for std::span

#include "engine/census.h"         // for ENGINE_CENSUS_SEARCH
#include "engine/error.h"          // for Error, errc
#include "engine/kind.h"           // for Kind
#include "engine/strata/search.h"  // for strata::search
#include "engine/xapian/search.h"  // for xapian::search


namespace engine {

task::task<SearchResult>
search(std::span<Shard* const> shards, const SearchRequest& request)
{
	ENGINE_CENSUS_SEARCH(request, shards.size());
	bool on_strata = false, on_xapian = false;
	for (const auto* shard : shards) {
		(shard->kind() == Kind::strata ? on_strata : on_xapian) = true;
	}
	if (on_strata && on_xapian) {
		throw Error(errc::invalid, "A search cannot span indexes of different engines");
	}
	if (on_strata) {
		co_return co_await strata::search(shards, request);
	}
	co_return co_await xapian::search(shards, request);
}

}  // namespace engine
