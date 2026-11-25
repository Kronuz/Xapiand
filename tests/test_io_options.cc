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

// --full-flush-ordering reaches the options of every directory IO Xapiand builds, with the barrier warning.

#include "test_harness.h"

#include "engine/strata/io_options.h"

namespace {

void test_the_switch_reaches_the_io_options() {
	const auto saved = opts.full_flush_ordering;
	opts.full_flush_ordering = false;
	CHECK(engine::strata::posix_io_options().flush_mode == kronuz::journal::FlushMode::plain);
	opts.full_flush_ordering = true;
	CHECK(engine::strata::posix_io_options().flush_mode == kronuz::journal::FlushMode::full);
	opts.full_flush_ordering = saved;
}

void test_the_options_log_the_barrier_warning() {
	CHECK(engine::strata::posix_io_options().barrier_warning != nullptr);
}

}  // namespace

TEST_MAIN(
	test_the_switch_reaches_the_io_options,
	test_the_options_log_the_barrier_warning
)
