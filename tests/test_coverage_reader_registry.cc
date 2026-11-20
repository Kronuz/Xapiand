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


// Measures the bytes of an index's reader registry (strata's chunked hazard registry, which an open index holds for its
// readers): nothing but the registry object while idle, one chunk when a first reader arrives, a chunk per
// `chunk_records` concurrent readers after that, up to the capacity `readers_per_index` gives it. The values printed
// are the per-open-index figures docs/dev/coverage.md states; the checks pin their arithmetic.

#include "test_harness.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "strata/core/hazard.hh"

namespace {

using Registry = strata::reader_registry;

std::size_t bytes(const Registry& registry) {
	return sizeof(Registry) + registry.allocated() / Registry::chunk_records() * Registry::chunk_bytes();
}

void measure(const char* label, std::size_t capacity) {
	static const int view = 1;
	std::atomic<const int*> current{&view};
	const auto chunk = Registry::chunk_bytes();
	const auto per_chunk = Registry::chunk_records();

	Registry registry(capacity);
	const auto idle = bytes(registry);
	CHECK_EQ(registry.capacity(), capacity);
	CHECK_EQ(registry.allocated(), static_cast<std::size_t>(0));
	CHECK_EQ(idle, sizeof(Registry));

	std::vector<Registry::guard<int>> readers;
	readers.push_back(registry.acquire(current));
	CHECK_EQ(registry.allocated(), per_chunk);
	const auto first = bytes(registry);
	CHECK_EQ(first, sizeof(Registry) + chunk);

	// A chunk more for each `chunk_records` readers held at once.
	for (std::size_t n = 2; n <= per_chunk + 1; ++n) {
		readers.push_back(registry.acquire(current));
	}
	CHECK_EQ(registry.allocated(), 2 * per_chunk);

	// Up to the capacity, rounded up to whole chunks; one more is refused.
	while (readers.size() < capacity) {
		readers.push_back(registry.acquire(current));
	}
	const auto full = bytes(registry);
	CHECK_EQ(registry.allocated(), (capacity + per_chunk - 1) / per_chunk * per_chunk);
	bool refused = false;
	try {
		for (std::size_t extra = 0; extra < per_chunk; ++extra) {
			readers.push_back(registry.acquire(current));
		}
	} catch (const strata::too_many_readers&) {
		refused = true;
	}
	CHECK(refused);
	CHECK_EQ(registry.capacity(), capacity);

	// Releasing the readers frees the records for reuse and keeps the chunks.
	readers.clear();
	CHECK_FALSE(registry.claimed());
	CHECK_EQ(bytes(registry), full);
	std::printf("registry %-34s capacity %4zu records: idle %zu B, first reader %zu B, %zu readers %zu B (chunk %zu B, %zu records)\n",
		label, capacity, idle, first, capacity, full, chunk, per_chunk);
}

void test_registry_bytes() {
	// readers_per_index() is 16 per reactor plus twice the pool workers and the control loop.
	measure("one reactor, one CPU", 1 * 16 + 2 * (1 + 1));
	measure("16 CPUs, 10 reactors", 10 * 16 + 2 * (16 + 1));
	measure("engine default", 128);
}

void test_the_empty_registry_holds_no_reader() {
	Registry registry(16);
	const std::atomic<const int*> none{nullptr};
	auto guard = registry.acquire(none);
	CHECK_FALSE(static_cast<bool>(guard));
	CHECK_EQ(registry.allocated(), static_cast<std::size_t>(0));
}

}  // namespace

TEST_MAIN(test_registry_bytes, test_the_empty_registry_holds_no_reader)
