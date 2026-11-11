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

// Unit tests for the error kinds: the retry fates, the translation of what strata throws into
// engine errors, the HTTP answer for each kind, and the plain form of the doubles on the wire.

#include "test_harness.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "cartesian.h"
#include "datetime.h"
#include "engine/codec.h"
#include "engine/error.h"
#include "engine/strata/shard.h"
#include "exception_engine.h"
#include "http_parser.h"
#include "server/http_utils.h"
#include "xapiand_exception.h"

namespace {

using engine::errc;

void test_fates() {
	using engine::fate;
	for (auto kind : {errc::reopen, errc::opening, errc::not_found, errc::not_available, errc::timeout, errc::network}) {
		CHECK_TRUE(engine::read_fate(kind) == fate::retry_close);
	}
	CHECK_TRUE(engine::read_fate(errc::closed) == fate::close_retry);
	CHECK_TRUE(engine::read_fate(errc::corrupt) == fate::close_fail);
	CHECK_TRUE(engine::read_fate(errc::database) == fate::close_fail);
	for (auto kind : {errc::doc_not_found, errc::version_conflict, errc::invalid, errc::syntax, errc::internal, errc::busy}) {  // busy: the turn did not come; a retry would wait again
		CHECK_TRUE(engine::read_fate(kind) == fate::fail);
	}
	for (auto kind : {errc::doc_not_found, errc::not_found}) {
		CHECK_TRUE(engine::is_missing(kind));
	}
	for (auto kind : {errc::reopen, errc::opening, errc::closed, errc::not_available, errc::timeout, errc::network, errc::version_conflict, errc::corrupt, errc::database, errc::invalid, errc::syntax, errc::internal}) {
		CHECK_TRUE(!engine::is_missing(kind));
	}
	CHECK_EQ(std::string(engine::to_string(errc::syntax)), "syntax");
	CHECK_EQ(std::string(engine::to_string(errc::version_conflict)), "version_conflict");
}

// What strata throws, as the engine error of its kind (callers act on the kind, not the type).
void test_translation() {
	const auto translate = [](const std::function<void()>& thrower, bool callback_threw = false) {
		try {
			try {
				thrower();
			} catch (...) {
				engine::strata::rethrow_translated(nullptr, callback_threw);
			}
		} catch (const engine::Error& error) {
			return std::pair{error.kind, error.reason()};
		} catch (...) {
			return std::pair{errc::closed, std::string("not an engine error")};  // a marker: it escaped as thrown
		}
		return std::pair{errc::closed, std::string("did not throw")};
	};
	auto row = translate([] { throw std::invalid_argument("bad"); });
	CHECK_TRUE(row.first == errc::invalid);
	CHECK_EQ(row.second, "bad");
	row = translate([] { throw std::system_error(std::make_error_code(std::errc::no_space_on_device), "disk"); });
	CHECK_TRUE(row.first == errc::database);
	CHECK_TRUE(row.second.find("disk") != std::string::npos);
	row = translate([] { throw std::runtime_error("plain"); });
	CHECK_TRUE(row.first == errc::internal);
	CHECK_EQ(row.second, "plain");

	// An engine error passes through as it is.
	row = translate([] { throw engine::Error(errc::timeout, "t"); });
	CHECK_TRUE(row.first == errc::timeout);
	CHECK_EQ(row.second, "t");

	// Whatever a caller's callback threw leaves as thrown.
	row = translate([] { throw std::runtime_error("from the callback"); }, true);
	CHECK_EQ(row.second, "not an engine error");
}

// ---- HTTP answers ---------------------------------------------------------------------------

std::pair<int, std::string> answer(const std::function<void()>& thrower) {
	auto answered = catch_http_errors([&] {
		thrower();
		return 0;
	});
	return {static_cast<int>(answered.error_code), answered.error};
}

// Each engine error kind answers with the status the client can act on.
void test_http_answers() {
	const auto engine_error = [](errc kind) { return [kind] { throw engine::Error(kind, "the reason", "the description", "the detail"); }; };
	const std::vector<std::pair<errc, std::pair<int, std::string>>> table = {
		{errc::doc_not_found, {HTTP_STATUS_NOT_FOUND, "Not Found: the reason"}},
		{errc::not_found, {HTTP_STATUS_NOT_FOUND, "Not Found: the reason"}},
		{errc::version_conflict, {HTTP_STATUS_CONFLICT, "Conflict: the reason"}},
		{errc::not_available, {HTTP_STATUS_SERVICE_UNAVAILABLE, "Service Unavailable: the reason"}},
		{errc::busy, {HTTP_STATUS_SERVICE_UNAVAILABLE, "Service Unavailable: the reason"}},
		{errc::reopen, {HTTP_STATUS_SERVICE_UNAVAILABLE, "Service Unavailable: the reason"}},
		{errc::timeout, {HTTP_STATUS_GATEWAY_TIMEOUT, "Gateway Timeout: the reason"}},
		{errc::invalid, {HTTP_STATUS_BAD_REQUEST, "Bad Request: the description"}},
		{errc::syntax, {HTTP_STATUS_BAD_REQUEST, "Bad Request: the description"}},
		{errc::corrupt, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
		{errc::database, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
		{errc::opening, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
		{errc::closed, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
		{errc::internal, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
		// A transport failure the cluster named is a bad gateway; any other text is a server error.
		{errc::network, {HTTP_STATUS_INTERNAL_SERVER_ERROR, "the description"}},
	};
	for (const auto& [kind, expected] : table) {
		const auto got = answer(engine_error(kind));
		if (got != expected) {
			std::printf("MISMATCH %s: got %d '%s'\n", engine::to_string(kind), got.first, got.second.c_str());
		}
		CHECK_EQ(got.first, expected.first);
		CHECK_EQ(got.second, expected.second);
	}

	// The detail of a network failure picks the gateway answer.
	const auto refused = answer([] { throw engine::Error(errc::network, "connect", "connect failed", "Connection refused"); });
	CHECK_EQ(refused.first, static_cast<int>(HTTP_STATUS_BAD_GATEWAY));
	CHECK_EQ(refused.second, "Bad Gateway: Connection refused");

	// Client errors that are also engine errors answer as their kind says, not as a server error.
	CHECK_EQ(answer([] { THROW(QueryParserError, "bad query"); }).first, static_cast<int>(HTTP_STATUS_BAD_REQUEST));
	CHECK_EQ(answer([] { THROW(InvalidTextConfigError, "bad config"); }).first, static_cast<int>(HTTP_STATUS_BAD_REQUEST));

	// Not engine errors.
	CHECK_EQ(answer([] { THROW(ClientError, "client"); }).first, static_cast<int>(HTTP_STATUS_BAD_REQUEST));
	CHECK_EQ(answer([] { THROW(MissingTypeError, "missing type"); }).first, static_cast<int>(HTTP_STATUS_PRECONDITION_FAILED));
	CHECK_EQ(answer([] { THROW(SerialisationError, "bad bytes"); }).first, static_cast<int>(HTTP_STATUS_BAD_REQUEST));
	CHECK_EQ(answer([] { throw std::runtime_error("rt"); }).first, static_cast<int>(HTTP_STATUS_INTERNAL_SERVER_ERROR));
	CHECK_EQ(answer([] { throw 3; }).first, static_cast<int>(HTTP_STATUS_INTERNAL_SERVER_ERROR));
}

// The doubles that metric_serialise.h and the callbacks put on the wire: eight bytes, the IEEE bits
// least significant byte first.
void test_double_bytes() {
	CHECK_EQ(engine::serialise_double(1.0), std::string("\x00\x00\x00\x00\x00\x00\xf0\x3f", 8));
	CHECK_EQ(engine::serialise_double(-2.0), std::string("\x00\x00\x00\x00\x00\x00\x00\xc0", 8));
	CHECK_EQ(engine::serialise_double(0.0), std::string(8, '\0'));
	CHECK_EQ(engine::serialise_double(-0.0), std::string("\x00\x00\x00\x00\x00\x00\x00\x80", 8));
	CHECK_EQ(engine::serialise_double(0.1), std::string("\x9a\x99\x99\x99\x99\x99\xb9\x3f", 8));

	const std::vector<double> values = {0.0, -0.0, 1.0, -1.0, 0.1, 0.7, 0.25, 1e-300, 1e300, 3.141592653589793,
		-2.5e-7, 65536.0, 4503599627370496.0, 123456789.123456789, 1.7976931348623157e308, 2.2250738585072014e-308};
	for (double v : values) {
		const auto bytes = engine::serialise_double(v);
		CHECK_EQ(bytes.size(), 8u);
		const char* p = bytes.data();
		CHECK_TRUE(engine::unserialise_double(&p, bytes.data() + bytes.size()) == v);
		CHECK_TRUE(p == bytes.data() + bytes.size());
	}
	bool rejected = false;
	try {
		const char* p = "";
		engine::unserialise_double(&p, p);
	} catch (const engine::Error& error) {
		rejected = error.kind == errc::invalid;
	}
	CHECK_TRUE(rejected);
}

}  // namespace

TEST_MAIN(test_fates, test_translation, test_http_answers, test_double_bytes)
