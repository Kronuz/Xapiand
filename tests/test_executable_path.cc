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

// get_executable_path (src/main.cc) names the running binary from the kernel on Apple, Linux and
// FreeBSD, without help from argv[0].

#include "test_harness.h"

// test_stubs.cc defines the global `opts`, so main.cc's own is renamed.
#define main xapiand_main_not_run
#define opts main_opts
#include "../src/main.cc"
#undef opts
#undef main

#include <string>

namespace {

void test_path_is_the_running_binary() {
	// argv0 is null, so only the kernel can answer.
	auto exe = get_executable_path(nullptr);
	CHECK(exe.size() > 1 && exe.front() == '/');
	const std::string name = "xapiand_test_executable_path";
	CHECK(exe.size() >= name.size() && exe.substr(exe.size() - name.size()) == name);
	CHECK_EQ(::access(exe.c_str(), X_OK), 0);

	// A misleading argv0 does not change the answer.
	CHECK_EQ(get_executable_path("/nonexistent/other"), exe);
}

}  // namespace

TEST_MAIN(test_path_is_the_running_binary)
