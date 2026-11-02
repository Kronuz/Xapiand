/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2018 Dubalu LLC
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
#include "xapiand_exception.h"

static void test_endpoint_path() {
	struct Case { const char* cwd; const char* uri; const char* path; };
	const Case cases[] = {
		{"/var/db/xapiand/", "home/////user///something/",       "home/user/something"},
		{"/",                "/////home/user/something/",        "home/user/something"},
		{"/var/db/xapiand/", "/var/db/xapiand/home/user/",       "home/user"},
		{"/var/db/xapiand/", "//var//db/xapiand/a/./b////",      "a/b"},
		{"/var/db/xapiand/", "xapiand://home/user/something/",   "user/something"},
		{"/var/db/xapiand/", "://home/user/something/",          "home/user/something"},
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
		{"././var/db/xapiand", "var/db/xapiand/"},
		{"./var/../",          "./"},
		{"",                   "./"},
		{"a/../b",             "b/"},
		{"../a",               "../a/"},
		{"//x",                "/x/"},
	};
	for (const auto& c : cases) {
		auto res = normalize_path(std::string_view(c.in), true);
		CHECK_EQ(res, std::string(c.out));
	}
}

// An index path names something strictly inside the data directory, or the Endpoint refuses it.
static bool refused(const char* cwd, std::string_view uri) {
	Endpoint::cwd = cwd;
	try {
		Endpoint e(uri);
		return false;
	} catch (const ClientError&) {
		return true;
	}
}

static void test_endpoint_refuses_paths_outside_the_base() {
	// The base itself: empty, ".", and anything that normalizes back onto it.
	for (const char* cwd : {"/var/db/xapiand/", "/"}) {
		for (const char* uri : {"", ".", "./", "a/..", "a/b/../..", "./a/../."}) {
			CHECK_TRUE(refused(cwd, uri));
		}
		for (const char* uri : {"a", "a/b", "./a", "a/../b", "a/./b/", "..a", "a..", "a/..b", ".hidden", "a b"}) {
			CHECK_FALSE(refused(cwd, uri));
		}
	}
	// Above the base (the root has no above).
	for (const char* uri : {"..", "../x", "a/../../x", "a/../..", "../../../etc/passwd", "./a/../..", "../xapiand", "../xapiand2/x"}) {
		CHECK_TRUE(refused("/var/db/xapiand/", uri));
	}
	// An absolute path is inside the base or refused, however it is written.
	for (const char* uri : {"/", "//", "/home/user/something/", "/////home/user/something/", "/var/db/xapiand", "/var/db/xapiand/",
	                        "/var/db/xapiand/.", "/var/db/xapiand/a/..", "/var/db/xapiand2/x", "/var/db/x", "/var/db/xapiand/../x",
	                        "/etc/passwd", "//etc/passwd", ":///home/user/something/", "xapiand://home////////user/something/"}) {
		CHECK_TRUE(refused("/var/db/xapiand/", uri));
	}
	CHECK_TRUE(refused("/", "/"));
	CHECK_FALSE(refused("/", "/etc/x"));  // everything is inside the root
	// What a relative path resolves to stays relative to the base.
	Endpoint::cwd = "/var/db/xapiand/";
	CHECK_EQ(Endpoint("a/../b").path, std::string("b"));
	CHECK_EQ(Endpoint("./a//b/").path, std::string("a/b"));
	CHECK_EQ(Endpoint("/var/db/xapiand/a/../b").path, std::string("b"));
}

TEST_MAIN(test_endpoint_path, test_normalize_path, test_endpoint_refuses_paths_outside_the_base)
