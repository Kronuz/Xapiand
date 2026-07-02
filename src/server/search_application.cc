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

#include "search_application.h"


bool
SearchApplication::should_offload(const http::Request& request) const
{
	// Match the legacy HttpClient exactly: it ran every request that dispatched to a
	// view on the http_client_pool (i.e. offloaded it), and handled only the view-less
	// methods inline -- OPTIONS, QUIT, OPEN, CLOSE, whose responses are trivial and
	// non-blocking (written directly by the dispatch, no database work). Everything
	// else can touch a database (search, the CRUD verbs, dump/restore, metrics/info),
	// so it offloads to the worker pool -- the un-stallable model, the same set the
	// old runner offloaded. (One offload per request: the handler runs once on a
	// worker; any further pool use inside a view -- e.g. DocIndexer for bulk indexing
	// -- is the same pipeline the old runner had, not a second HTTP-level offload.)
	const std::string& m = request.method;
	if (m == "OPTIONS" || m == "QUIT" || m == "FLUSH" || m == "OPEN" || m == "CLOSE") {
		return false;
	}
	return true;
}
