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

// An index name the filesystem refuses (APFS on an unassigned code point: EILSEQ) answers errc::invalid, a 400
// naming the index, not a 500. Only macOS refuses such names; elsewhere the test is skipped.

#include "test_harness.h"

#include <filesystem>
#include <string>

#include "database/flags.h"
#include "database/pool.h"
#include "engine/error.h"
#include "engine/sync.h"
#include "pool_support.h"

namespace {

void test_an_unassignable_name_is_a_400() {
#ifdef __APPLE__
	DatabasePool pool(control_loop(), 8);
	const std::string path = "pool_name_test_\xCD\xB8.db";  // U+0378, unassigned
	std::filesystem::remove_all("pool_name_test_.db");
	bool invalid = false;
	std::string what;
	try {
		(void)pin_index(pool, path, DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_AUTOCOMMIT);
	} catch (const engine::Error& error) {
		invalid = error.kind == engine::errc::invalid;
		what = error.description();
	} catch (...) {
	}
	CHECK_TRUE(invalid);
	CHECK_TRUE(what.find(path) != std::string::npos);
	pool.finish();
	CHECK_TRUE(pool.join());
#else
	std::fprintf(stderr, "  skipped: only APFS refuses unassigned code points\n");
#endif
}

}  // namespace

TEST_MAIN(
	test_an_unassignable_name_is_a_400
)
