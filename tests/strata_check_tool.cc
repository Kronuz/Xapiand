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

// strata_check_tool SHARD_DIR...: runs the strata engine's own integrity check (index::check, which
// verifies every file's digest, plus the field-table check) on each shard directory, with no server up.
// Exit 0 when every shard passes. Used by harness/engine_crash_check.sh; built by hand against the
// objects of a server build (see docs/gates/phase7a.md), not part of the CMake targets.

#include <cstdio>

#include "engine/strata/shard.h"
#include "engine/sync.h"

int
main(int argc, char** argv)
{
	int bad = 0;
	for (int i = 1; i < argc; ++i) {
		const auto result = engine::sync_wait(engine::strata::check(argv[i]));
		if (result.errors != 0 || !result.failure.empty()) {
			std::printf("%s: FAIL errors=%d %s\n", argv[i], static_cast<int>(result.errors), result.failure.c_str());
			++bad;
		} else {
			std::printf("%s: ok\n", argv[i]);
		}
	}
	return bad == 0 ? 0 : 1;
}
