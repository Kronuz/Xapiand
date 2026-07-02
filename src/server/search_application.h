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

#include <memory>

#include "http_handler.h"                   // Kronuz/http: HttpHandler, ResponseWriter, BodySink
#include "http_message.h"                   // Kronuz/http: Request, Response


// Xapiand's search engine as ONE http::HttpHandler (Leg 2, architecture B).
//
// SearchApplication owns everything application-specific: the request dispatch that
// HttpClient::prepare() did (now a declarative route table over the generic radix
// router, http::MethodRouter, plus a small classifier for Xapiand's ":command" URL
// sub-syntax), the ~28 *_view endpoints, and the MsgPack content-negotiation /
// response formatting. The generic transport -- HTTP parsing, HTTP/1.1 framing, the
// reactor, and the un-stallable worker offload -- lives entirely in the library
// (http::HttpConnection + http::Dispatcher), which drives this handler through the
// value-semantic seam handle(Request, ResponseWriter).
//
// STATUS: LIVE (Leg 2 stage 3c-6). HttpServer creates http::HttpConnection bound to a
// shared stateless SearchApplication; the legacy HttpClient is gone. handle() rebuilds
// an application Request from the library's http::Request, runs the route table's
// selected view, and emits the response through the library ResponseWriter.
class SearchApplication : public http::HttpHandler {
public:
	void handle(const http::Request& request, http::ResponseWriter& response) override;

	// Search endpoints are CPU-bound/blocking (Xapian), so they must always run on a
	// worker thread, never the reactor -- the un-stallable model. (A cheap route like
	// a health check could return false here to stay inline; the search views do not.)
	bool should_offload(const http::Request& request) const override;
};
