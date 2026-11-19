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

// Coverage tests for src/engine/strata/document.cc: serialized documents that are out of range, repeat a field or a term,
// or run a number past 64 bits are refused with the reason named, and a well-formed one reads back.

#include "test_harness.h"

#include <cstdint>
#include <string>

#include "engine/document.h"
#include "engine/error.h"

namespace {

// Builds the serialized form by hand: version 3, a document version, the key, its frequency, the data, then the value
// and field counts.
struct Bytes {
	std::string out;

	Bytes& byte(std::uint8_t b) { out.push_back(static_cast<char>(b)); return *this; }
	Bytes& num(std::uint64_t v) {
		while (v >= 0x80) { byte(static_cast<std::uint8_t>(v | 0x80)); v >>= 7; }
		return byte(static_cast<std::uint8_t>(v));
	}
	Bytes& str(const std::string& s) { num(s.size()); out += s; return *this; }
	Bytes& head(std::uint64_t key_wdf = 0) { return byte(3).num(0).str("").num(key_wdf).str("").num(0); }
};

// The reason the document's bytes are refused with, or "" when they are read.
std::string refusal(const std::string& bytes) {
	try {
		(void)engine::Document::unserialize(bytes);
	} catch (const engine::Error& e) {
		return e.reason();
	}
	return {};
}

void refused(const std::string& bytes, const std::string& what) {
	const auto reason = refusal(bytes);
	if (reason != "Bad serialized document: " + what) {
		std::fprintf(stderr, "  wanted '%s', got '%s'\n", what.c_str(), reason.c_str());
	}
	CHECK_EQ(reason, "Bad serialized document: " + what);
}

void test_numbers_out_of_range() {
	const std::uint64_t over = 1ULL << 32;
	refused(Bytes().byte(3).num(0).str("").num(over).str("").num(0).num(0).out, "key frequency out of range");
	Bytes ten;
	ten.byte(3).num(0).str("");
	for (int i = 0; i < 9; ++i) { ten.byte(0xff); }
	ten.byte(2);  // a tenth byte that does not fit in 64 bits
	refused(ten.out, "number out of range");
	refused(Bytes().head().num(1).str("f").byte(0).num(1).str("t").num(over).num(0).out, "wdf out of range");
	refused(Bytes().head().num(1).str("f").byte(0).num(1).str("t").num(1).num(1).num(over).out, "position out of range");
}

void test_repeats() {
	refused(Bytes().head().num(2).str("f").byte(0).num(0).str("f").byte(0).num(0).out, "field twice");
	refused(Bytes().head().num(1).str("f").byte(0).num(2).str("t").num(1).num(0).str("t").num(1).num(0).out, "term twice");
}

void test_well_formed() {
	CHECK_EQ(refusal(Bytes().head().num(1).str("f").byte(0).num(1).str("t").num(1).num(1).num(5).out), "");
	CHECK_EQ(refusal(Bytes().head().num(0).out), "");
}

}  // namespace

TEST_MAIN(test_numbers_out_of_range, test_repeats, test_well_formed)
