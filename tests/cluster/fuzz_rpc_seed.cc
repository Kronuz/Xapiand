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

// Writes the fuzz target's seed corpus: one valid envelope per decoder, behind its selector byte.
//   fuzz_rpc_seed DIR

#include <cstdio>   // for std::fprintf
#include <fstream>  // for std::ofstream
#include <string>   // for std::string, std::to_string

#include "rpc_samples.h"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::fprintf(stderr, "usage: %s DIR\n", argv[0]);
		return 2;
	}
	const auto seeds = rpc_samples::envelopes();
	for (std::size_t i = 0; i < seeds.size(); ++i) {
		std::ofstream out(std::string(argv[1]) + "/seed" + std::to_string(i), std::ios::binary);
		out.put(static_cast<char>(i));
		out << seeds[i];
		if (!out) {
			std::fprintf(stderr, "cannot write seed %zu in %s\n", i, argv[1]);
			return 1;
		}
	}
	return 0;
}
