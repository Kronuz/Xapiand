/*
 * Copyright (c) 2026 Dubalu LLC
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


// Fuzzes the request body decoders (json_load, yaml_load, msgpack_load): structured and byte-mutated
// documents, nested at random depths around the limit. A body either decodes or is refused with an
// exception (a JSON one that decodes has at most MAX_JSON_DEPTH levels); it never crashes or recurses
// without bound.
//
//   xapiand_fuzz_body SECONDS [SEED]
//
// Exits non-zero on the first violation. Under -fsanitize it finds the memory errors and overflows.

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <unistd.h>

#include "database/utils.h"

// GCC parses the whole #if line, so __has_feature (clang only) cannot share one with defined().
#if defined(__SANITIZE_ADDRESS__)
#define FUZZ_BODY_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FUZZ_BODY_ASAN 1
#endif
#endif
#ifdef FUZZ_BODY_ASAN
#include <sanitizer/common_interface_defs.h>
#define FUZZ_BODY_DEATH_CALLBACK 1
#endif

namespace {

struct Rng {
	std::uint64_t s;
	std::uint64_t next() {
		s += 0x9e3779b97f4a7c15ULL;
		std::uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}
	std::size_t below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(next() % n); }
};

// A nesting depth biased to the limit's neighbourhood and to far beyond it.
std::size_t
depth(Rng& rng)
{
	switch (rng.below(4)) {
		case 0: return rng.below(8);
		case 1: return MAX_JSON_DEPTH - 3 + rng.below(7);
		case 2: return rng.below(2 * MAX_JSON_DEPTH);
		default: return rng.below(20000);
	}
}

std::string
json_body(Rng& rng)
{
	const auto n = depth(rng);
	std::string out;
	for (std::size_t i = 0; i < n; ++i) {
		out += rng.below(2) ? "[" : "{\"f\":";
	}
	out += "1";
	for (std::size_t i = 0; i < n; ++i) {
		out += rng.below(8) ? ']' : '}';
	}
	return out;
}

std::string
yaml_body(Rng& rng)
{
	const auto n = depth(rng);
	std::string out;
	switch (rng.below(3)) {
		case 0:
			out = std::string(n, '[') + "1" + std::string(n, ']');
			break;
		case 1:
			for (std::size_t i = 0; i < n; ++i) {
				out += "{f: ";
			}
			out += "1" + std::string(n, '}');
			break;
		default:
			for (std::size_t i = 0; i < n; ++i) {
				out += std::string(i % 400, ' ') + (rng.below(3) ? "f:\n" : "- ");
			}
			out += "1\n";
	}
	return out;
}

std::string
msgpack_body(Rng& rng)
{
	const auto n = depth(rng);
	std::string out;
	for (std::size_t i = 0; i < n; ++i) {
		switch (rng.below(5)) {
			case 0: out += "\x81\xa1" "f"; break;
			case 1: out += std::string("\xdc\x00\x01", 3); break;
			case 2: out += std::string("\xde\x00\x01\xa1" "f", 5); break;
			default: out += '\x91';
		}
	}
	return out + '\x01';
}

// Random edits: flipped bytes, cut and doubled spans.
std::string
mutate(Rng& rng, std::string text)
{
	for (auto edits = rng.below(4); edits > 0 && !text.empty(); --edits) {
		const auto at = rng.below(text.size());
		switch (rng.below(3)) {
			case 0: text[at] = static_cast<char>(rng.below(256)); break;
			case 1: text.erase(at, 1 + rng.below(16)); break;
			default: text.insert(at, text.substr(at, 1 + rng.below(16))); break;
		}
	}
	return text;
}

// The body being decoded, shown when a sanitizer stops the run.
std::string current;
int current_format = 0;

void
show_current()
{
	std::fprintf(stderr, "format %d, %zu bytes, first 200: ", current_format, current.size());
	for (std::size_t i = 0; i < current.size() && i < 200; ++i) {
		std::fprintf(stderr, "\\x%02x", static_cast<unsigned char>(current[i]));
	}
	std::fprintf(stderr, "\n");
}

}  // namespace

int
main(int argc, char** argv)
{
	const double seconds = argc > 1 ? std::atof(argv[1]) : 10;
	Rng rng{argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1};
	const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<long>(seconds * 1000));
	std::size_t runs = 0, decoded = 0;
#ifdef FUZZ_BODY_DEATH_CALLBACK
	__sanitizer_set_death_callback(show_current);
#endif
	std::signal(SIGALRM, [](int) { std::fprintf(stderr, "HANG: a body took more than 30 s; "); show_current(); ::_exit(3); });
	while (std::chrono::steady_clock::now() < end) {
		for (int i = 0; i < 50; ++i, ++runs) {
			const auto format = rng.below(3);
			auto body = format == 0 ? json_body(rng) : format == 1 ? yaml_body(rng) : msgpack_body(rng);
			if (rng.below(2)) {
				body = mutate(rng, std::move(body));
			}
			current = body;
			current_format = static_cast<int>(format);
			::alarm(30);
			try {
				if (format == 2) {
					msgpack_load(body);
				} else {
					rapidjson::Document doc;
					(format == 0 ? json_load : yaml_load)(doc, body);
					if (format == 0 && json_depth(body) > MAX_JSON_DEPTH) {
						std::printf("json_load accepted %zu levels\n", json_depth(body));
						return 1;
					}
				}
				++decoded;
			} catch (const std::exception&) {
			}
			::alarm(0);
		}
	}
	std::printf("%zu bodies, %zu decoded, %zu refused\n", runs, decoded, runs - decoded);
	return 0;
}
