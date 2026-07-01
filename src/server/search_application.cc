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


void
SearchApplication::handle(const http::Request& request, http::ResponseWriter& response)
{
	// SCAFFOLD (Leg 2 stage 3c-2): not yet wired into HttpServer, so this never runs.
	//
	// The port plan for the real body (each step E2E-green):
	//   1. Build Xapiand's per-request state from `request` (method, path, query,
	//      headers, body) -- the work HttpClient::on_headers_complete + url_resolve +
	//      the content-negotiation helpers do today (all now file-scope free functions
	//      taking a Request&, after stages 2-3b).
	//   2. Run HttpClient::prepare()'s method + URL-predicate dispatch to pick the view
	//      (kept as a custom switch here -- it is not radix-router-clean).
	//   3. Invoke the selected *_view against that request state.
	//   4. Rewrite the response path onto `response`: serialize the MsgPack result
	//      (serialize_response, already free) then response.status()/set_header()/
	//      write()/end(), letting the library own framing + the compression knob (this
	//      replaces http_response()/manual gzip-deflate; deflate -> zstd/gzip is the
	//      documented parity change).
	//   5. Map Xapian/ClientError -> HTTP status at this boundary (reuse catch_http_errors).
	(void)request;
	response.send(501, "SearchApplication not yet wired", "text/plain; charset=utf-8");
}


bool
SearchApplication::should_offload(const http::Request& /*request*/) const
{
	return true;
}
