/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Unit tests for Endpoint URI/path resolution and normalize_path. Revived and
// modernized from the retired oldtests endpoint suite.

#include "test_harness.h"

#include <string>
#include <string_view>

#include "endpoint.h"
#include "fs.hh"

static void test_endpoint_path() {
	struct Case { const char* cwd; const char* uri; const char* path; };
	const Case cases[] = {
		{"/var/db/xapiand/", "/",                                 "/"},
		{"/var/db/xapiand/", "/home/user/something/",            "/home/user/something"},
		{"/var/db/xapiand/", "home/////user///something/",       "home/user/something"},
		{"/",                "/////home/user/something/",        "home/user/something"},
		{"/var/db/xapiand/", "/////home/user/something/",        "/home/user/something"},
		{"/var/db/xapiand/", "/home/user/something////////",     "/home/user/something"},
		{"/var/db/xapiand/", "xapiand://home/user/something/",   "user/something"},
		{"/var/db/xapiand/", "xapiand://home////////user/something/", "/user/something"},
		{"/var/db/xapiand/", "://home/user/something/",          "home/user/something"},
		{"/var/db/xapiand/", ":///home/user/something/",         "/home/user/something"},
		{"/var/db/xapiand/", "file://home/user/something/",      "home/user/something"},
	};
	for (const auto& c : cases) {
		Endpoint::cwd = c.cwd;
		Endpoint e(c.uri);
		CHECK_EQ(e.path, std::string(c.path));
	}
}

static void test_normalize_path() {
	struct Case { const char* in; const char* out; };
	const Case cases[] = {
		{"var/db/xapiand/./",  "var/db/xapiand/"},
		{"./././",             "./"},
		{"var/./db/./xapiand", "var/db/xapiand/"},
		{"././var/db/xapiand", "./var/db/xapiand/"},
		{"./var/../",          "./"},
	};
	for (const auto& c : cases) {
		auto res = normalize_path(std::string_view(c.in), true);
		CHECK_EQ(res, std::string(c.out));
	}
}

TEST_MAIN(test_endpoint_path, test_normalize_path)
