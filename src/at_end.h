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

#pragma once

// The destructor rule of the index registry, the lease and the chores (docs/design/phase7c.md D14): they end
// with nothing outstanding, no pending chore, no claimed reader and no waiting turn. Debug builds abort with
// the owner and what is left; release builds do not look.

#include <string>   // for std::string

#ifndef NDEBUG
#include <cstdio>   // for std::fprintf
#include <cstdlib>  // for std::abort
#endif

namespace at_end {

inline void assert_none([[maybe_unused]] const char* owner, [[maybe_unused]] const std::string& left) {
#ifndef NDEBUG
	if (!left.empty()) {
		std::fprintf(stderr, "%s ended with work outstanding: %s\n", owner, left.c_str());
		std::abort();
	}
#endif
}

}  // namespace at_end
