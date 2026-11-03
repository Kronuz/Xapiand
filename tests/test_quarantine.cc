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

// quarantine_files (the fs library) moves a damaged WAL aside, and logs when it cannot make the directory.
// The log call takes the template as a string: a char[PATH_MAX + 1] array formats as all 1025 bytes, which
// Apple's libc++ aborts on in vformat_to (__stack_chk_fail), and a path of PATH_MAX bytes or more leaves such
// an array unterminated.

#include "test_harness.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "fs.hh"
#include "io.hh"

namespace {

namespace fsys = std::filesystem;

void touch(const fsys::path& file) {
	int fd = io::open(file.c_str(), O_WRONLY | O_CREAT, 0644);
	if (fd != -1) {
		io::close(fd);
	}
}

fsys::path scratch(const char* name) {
	auto dir = fsys::current_path() / name;
	std::error_code ec;
	fsys::permissions(dir / "db", fsys::perms::owner_all, ec);   // a run that stopped early leaves it read-only
	fsys::remove_all(dir);
	fsys::create_directories(dir);
	return dir;
}

void test_a_damaged_wal_is_moved_aside() {
	auto dir = scratch("quarantine_ok") / "db";
	fsys::create_directories(dir);
	touch(dir / "wal.0");
	touch(dir / "keep.sx");
	quarantine_files(dir.string(), {"wal.*"});
	CHECK(fsys::exists(dir / "keep.sx"));
	CHECK(!fsys::exists(dir / "wal.0"));
	size_t aside = 0;
	for (const auto& entry : fsys::directory_iterator(dir)) {
		aside += entry.is_directory() && fsys::exists(entry.path() / "wal.0");
	}
	CHECK_EQ(aside, 1u);
}

void test_a_directory_that_cannot_be_made_is_logged_not_fatal() {
	if (::geteuid() == 0) {
		return;  // permissions do not bind root
	}
	auto dir = scratch("quarantine_denied") / "db";
	fsys::create_directories(dir);
	touch(dir / "wal.0");
	fsys::permissions(dir, fsys::perms::owner_read | fsys::perms::owner_exec);
	quarantine_files(dir.string(), {"wal.*"});   // mkdtemp fails with EACCES and the failure is logged
	fsys::permissions(dir, fsys::perms::owner_all);
	CHECK(fsys::exists(dir / "wal.0"));          // nothing moved
}

void test_a_path_past_path_max_is_logged_not_fatal() {
	auto dir = scratch("quarantine_long");
	std::string deep = dir.string();
	while (deep.size() < 2 * PATH_MAX) {
		deep += "/" + std::string(30, 'a');
	}
	quarantine_files(deep, {"*"});
	CHECK(true);   // returned: the template is a whole string, so the failure message is that string
	fsys::remove_all(dir);
}

}  // namespace

TEST_MAIN(
	test_a_damaged_wal_is_moved_aside,
	test_a_directory_that_cannot_be_made_is_logged_not_fatal,
	test_a_path_past_path_max_is_logged_not_fatal
)
