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
	// The un-stallable fast path: cheap, non-blocking endpoints run inline on the
	// reactor; only the Xapian-bound (blocking, potentially slow) endpoints offload to
	// a worker so they never stall the loop. Classified here from the method + path
	// alone (before the view is chosen), so it is a conservative split: the couple of
	// genuinely trivial reads run inline, everything that can touch a database offloads.
	if (request.method == "OPTIONS") {
		return false;   // no body, no database work
	}
	if (request.method == "GET") {
		// GET / (node/cluster info) and GET /:metrics (Prometheus) are cheap and
		// non-blocking; every other GET is a database read.
		if (request.path == "/" || request.path.find(":metrics") != std::string::npos) {
			return false;
		}
	}
	return true;
}
