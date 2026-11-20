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

// A negative text is never a positive number: strtoull reads the sign and wraps "-1" to 2^64 - 1.

#include "test_harness.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "database/schema.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "serialize.h"

namespace {

constexpr auto U64_MAX = std::numeric_limits<uint64_t>::max();

required_spc_t spc(FieldType type) {
	required_spc_t s;
	s.set_type(type);
	return s;
}

// The serializer refuses each negative spelling with a 400 that names the value.
void test_negative_text_is_refused() {
	for (const char* text : { "-1", "-0x1", " -1", "-0", "-18446744073709551615", "-18446744073709551616", "1-", "--1" }) {
		CHECK_THROW(Serialize::positive(std::string_view(text)), SerializationError);
		CHECK_THROW(Serialize::serialize(spc(FieldType::positive), std::string_view(text)), SerializationError);
		try {
			(void)Serialize::positive(std::string_view(text));
		} catch (const ClientError& e) {
			const std::string message = e.get_message();
			CHECK(message.find(text) != std::string::npos);
		}
	}
}

// Zero and the largest positive still serialize, to the bytes of the number.
void test_positive_text_still_serializes() {
	CHECK_EQ(Serialize::positive(std::string_view("0")), Serialize::positive(uint64_t(0)));
	CHECK_EQ(Serialize::positive(std::string_view("7")), Serialize::positive(uint64_t(7)));
	CHECK_EQ(Serialize::positive(std::string_view("18446744073709551615")), Serialize::positive(U64_MAX));
	CHECK_EQ(Unserialize::positive(Serialize::positive(std::string_view("18446744073709551615"))), U64_MAX);
	CHECK_THROW(Serialize::positive(std::string_view("18446744073709551616")), SerializationError);
	CHECK_THROW(Serialize::positive(std::string_view("4x")), SerializationError);
}

// A negative whose magnitude is 2^64 - 1 wraps to 1 in strtoull; the guess reads it as the float it is.
void test_guess_reads_a_large_negative_as_a_float() {
	const MsgPack text(std::string("-18446744073709551615"));
	CHECK(Serialize::guess_type(text) == FieldType::floating);
	const auto [type, bytes] = Serialize::guess_serialize(text);
	CHECK(type == FieldType::floating);
	CHECK_EQ(bytes, Serialize::floating(-18446744073709551615.0));
	CHECK(Serialize::guess_type(MsgPack(std::string("18446744073709551615"))) == FieldType::positive);
}

}  // namespace

TEST_MAIN(test_negative_text_is_refused, test_positive_text_still_serializes, test_guess_reads_a_large_negative_as_a_float)
