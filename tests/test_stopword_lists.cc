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

// Stop-word lists: the start-up probe (`--probe MODE`) that measures what loading the lists costs.

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "stopper.h"

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

// Live heap bytes, counted by the global allocation functions of this binary.
std::atomic<std::size_t> live_bytes{0};

constexpr std::size_t header = 16;

}  // namespace

void*
operator new(std::size_t size)
{
	auto* raw = static_cast<unsigned char*>(std::malloc(size + header));
	if (raw == nullptr) {
		throw std::bad_alloc();
	}
	std::memcpy(raw, &size, sizeof(size));
	live_bytes.fetch_add(size, std::memory_order_relaxed);
	return raw + header;
}

void
operator delete(void* ptr) noexcept
{
	if (ptr == nullptr) {
		return;
	}
	auto* raw = static_cast<unsigned char*>(ptr) - header;
	std::size_t size;
	std::memcpy(&size, raw, sizeof(size));
	live_bytes.fetch_sub(size, std::memory_order_relaxed);
	std::free(raw);
}

void
operator delete(void* ptr, std::size_t) noexcept
{
	operator delete(ptr);
}

namespace {

std::size_t
resident_kib()
{
#if defined(__APPLE__)
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
		return 0;
	}
	return info.resident_size / 1024;
#elif defined(__linux__)
	std::size_t total = 0, resident = 0;
	if (std::FILE* f = std::fopen("/proc/self/statm", "r")) {
		if (std::fscanf(f, "%zu %zu", &total, &resident) != 2) {
			resident = 0;
		}
		std::fclose(f);
	}
	return resident * static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)) / 1024;
#else
	return 0;
#endif
}

// One line per run: the mode, the microseconds the step took, the heap bytes and the resident KiB it added.
// Modes: `idle` (nothing), `preload` (the server's start-up call), `first` (the first lookup of LANG, after
// a preload), `cold` (the first lookup of LANG without a preload).
int
probe(const std::string& mode, const char* language)
{
	using clock = std::chrono::steady_clock;
	const auto heap0 = live_bytes.load();
	const auto rss0 = resident_kib();
	const auto t0 = clock::now();
	std::size_t words = 0;
	if (mode == "preload") {
		preloadStopWords();
	} else if (mode == "first" || mode == "cold") {
		if (mode == "first") {
			preloadStopWords();
		}
		const auto* stop = getStopWords(language);
		words = stop != nullptr && (*stop)("the") ? 1 : 0;
	}
	const auto us = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count() / 1000.0;
	std::printf("probe %s %s us=%.1f heap_bytes=%zu rss_kib=%zu stops_the=%zu\n", mode.c_str(), language, us,
		live_bytes.load() - heap0, resident_kib() - rss0, words);
	return 0;
}

}  // namespace

int
main(int argc, char** argv)
{
	if (argc > 2 && std::string(argv[1]) == "--probe") {
		return probe(argv[2], argc > 3 ? argv[3] : "en");
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
