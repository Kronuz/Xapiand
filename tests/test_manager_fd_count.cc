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

// The open-descriptor gauge behind `GET /:metrics` (`xapiand_file_descriptors`): it counts each
// descriptor the process opens or closes exactly once, and agrees with probing every descriptor.

#include "test_harness.h"

#include <algorithm>
#include <cstddef>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include "system.hh"

static std::size_t probe_every_descriptor() {
	std::size_t cnt = 0;
	const std::size_t top = std::min<std::size_t>(get_max_files_per_proc(), 4096);
	for (std::size_t fd = 0; fd < top; ++fd) {
		if (::fcntl(static_cast<int>(fd), F_GETFD) != -1) {
			++cnt;
		}
	}
	return cnt;
}

static void test_count_rises_by_exactly_n() {
	constexpr std::size_t n = 25;
	const std::size_t base = get_open_files_per_proc();
	CHECK_EQ(base, probe_every_descriptor());

	std::vector<int> fds;
	for (std::size_t i = 0; i < n; ++i) {
		fds.push_back(::open("/dev/null", O_RDONLY));
		REQUIRE(fds.back() >= 0);
	}
	CHECK_EQ(get_open_files_per_proc(), base + n);
	CHECK_EQ(get_open_files_per_proc(), probe_every_descriptor());

	for (int fd : fds) {
		::close(fd);
	}
	CHECK_EQ(get_open_files_per_proc(), base);
}

static void test_highest_descriptor_counts() {
	if (get_max_files_per_proc() <= 2048) {
		return;
	}
	const std::size_t base = get_open_files_per_proc();
	const int high = ::dup2(0, 2000);
	REQUIRE_EQ(high, 2000);
	CHECK_EQ(get_open_files_per_proc(), base + 1);
	::close(high);
	CHECK_EQ(get_open_files_per_proc(), base);
}

TEST_MAIN(test_count_rises_by_exactly_n, test_highest_descriptor_counts)
