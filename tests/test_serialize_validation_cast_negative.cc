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


// Cast never reads a negative text as an unsigned number: strtoull reads the sign and wraps "-1" to 2^64 - 1.

#include "test_harness.h"

#include <cstdint>
#include <limits>
#include <string>

#include "cast.h"
#include "database/schema.h"
#include "exception_engine.h"
#include "msgpack.h"

namespace {

constexpr auto U64_MAX = std::numeric_limits<uint64_t>::max();

MsgPack text(const char* s) {
	return MsgPack(std::string(s));
}

// An untyped text out of int64's range is the float it spells, never a wrapped positive.
void test_untyped_negative_text_is_never_positive() {
	for (const char* s : { "-18446744073709551615", "-18446744073709551616", "-9223372036854775809" }) {
		const auto got = Cast::cast(FieldType::empty, text(s));
		CHECK(got.get_type() == MsgPack::Type::FLOAT);
		if (got.get_type() == MsgPack::Type::FLOAT) {
			CHECK_EQ(got.f64(), std::stod(s));
		} else {
			std::fprintf(stderr, "  FAIL: Cast::cast(empty, %s) gave %s\n", s, got.to_string().c_str());
		}
	}
}

// The numbers each kind of text spells are unchanged.
void test_untyped_number_text_still_casts() {
	CHECK_EQ(Cast::cast(FieldType::empty, text("-1")).i64(), -1);
	CHECK_EQ(Cast::cast(FieldType::empty, text("-9223372036854775808")).i64(), std::numeric_limits<int64_t>::min());
	CHECK_EQ(Cast::cast(FieldType::empty, text("7")).i64(), 7);
	CHECK_EQ(Cast::cast(FieldType::empty, text("18446744073709551615")).u64(), U64_MAX);
	CHECK_EQ(Cast::cast(FieldType::empty, text("2.5")).f64(), 2.5);
	CHECK_EQ(Cast::cast(FieldType::empty, text("word")).str(), std::string("word"));
}

// A positive refuses each negative spelling with a 400 naming the value, and still takes zero and U64_MAX.
void test_positive_refuses_negative_text() {
	for (const char* s : { "-1", "-0x1", " -1", "-0", "-18446744073709551615" }) {
		CHECK_THROW(Cast::positive(text(s)), CastError);
		CHECK_THROW(Cast::cast(FieldType::positive, text(s)), CastError);
		try {
			(void)Cast::positive(text(s));
		} catch (const ClientError& e) {
			const std::string message = e.get_message();
			CHECK(message.find(s) != std::string::npos);
		}
	}
	CHECK_EQ(Cast::positive(text("0")), uint64_t(0));
	CHECK_EQ(Cast::positive(text("18446744073709551615")), U64_MAX);
}

}  // namespace

TEST_MAIN(test_untyped_negative_text_is_never_positive, test_untyped_number_text_still_casts, test_positive_refuses_negative_text)
