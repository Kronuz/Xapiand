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

// bench_number_codec VALUES.tsv [PASSES]: ns per value of Serialise:: and Unserialise:: over a real-value set
// (design T8). VALUES.tsv lines are `set TAB i|u|d TAB value`. Each direction runs PASSES times over every value
// and the fastest pass is reported. It builds against the old codec's tree too (tests/bench_number_codec.cmake).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "serialise.h"

struct Value {
	char kind;
	std::int64_t i;
	std::uint64_t u;
	double d;
};

static std::string encode(const Value& v) {
	switch (v.kind) {
		case 'i': return Serialise::integer(v.i);
		case 'u': return Serialise::positive(v.u);
		default: return Serialise::floating(v.d);
	}
}

static double decode(char kind, const std::string& bytes) {
	switch (kind) {
		case 'i': return static_cast<double>(Unserialise::integer(bytes));
		case 'u': return static_cast<double>(Unserialise::positive(bytes));
		default: return static_cast<double>(Unserialise::floating(bytes));
	}
}

template <typename F>
static double fastest_ns(int passes, std::size_t count, F&& pass) {
	double best = 1e300;
	for (int p = 0; p < passes; ++p) {
		const auto start = std::chrono::steady_clock::now();
		pass();
		const std::chrono::duration<double, std::nano> took = std::chrono::steady_clock::now() - start;
		best = std::min(best, took.count() / static_cast<double>(count));
	}
	return best;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: %s VALUES.tsv [PASSES]\n", argv[0]);
		return 2;
	}
	const int passes = argc > 2 ? std::atoi(argv[2]) : 20;
	std::ifstream in(argv[1]);
	std::vector<Value> values;
	std::string set, kind, text;
	while (std::getline(in, set, '\t') && std::getline(in, kind, '\t') && std::getline(in, text)) {
		Value v{kind[0], 0, 0, 0};
		if (v.kind == 'i') v.i = std::strtoll(text.c_str(), nullptr, 10);
		else if (v.kind == 'u') v.u = std::strtoull(text.c_str(), nullptr, 10);
		else v.d = std::strtod(text.c_str(), nullptr);
		values.push_back(v);
	}
	if (values.empty()) {
		std::fprintf(stderr, "no values in %s\n", argv[1]);
		return 1;
	}
	std::vector<std::string> encoded;
	encoded.reserve(values.size());
	std::size_t bytes = 0;
	for (const auto& v : values) {
		encoded.push_back(encode(v));
		bytes += encoded.back().size();
	}
	volatile std::size_t sink_size = 0;
	volatile double sink = 0;
	const double encode_ns = fastest_ns(passes, values.size(), [&] {
		std::size_t total = 0;
		for (const auto& v : values) total += encode(v).size();
		sink_size = sink_size + total;
	});
	const double decode_ns = fastest_ns(passes, values.size(), [&] {
		double total = 0;
		for (std::size_t k = 0; k < values.size(); ++k) total += decode(values[k].kind, encoded[k]);
		sink = sink + total;
	});
	std::printf("values %zu bytes/value %.3f passes %d encode_ns %.2f decode_ns %.2f\n", values.size(),
		static_cast<double>(bytes) / static_cast<double>(values.size()), passes, encode_ns, decode_ns);
	return 0;
}
