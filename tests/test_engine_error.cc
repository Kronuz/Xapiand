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

// Unit tests for the error kinds: the Xapian-to-kind translation and its texts, the retry fates,
// the DocView exception contract, and the HTTP answer for each exception against the one the
// Xapian-typed dispatch gave.

#include "test_harness.h"

#include <cerrno>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "cartesian.h"
#include "datetime.h"
#include "engine/codec.h"
#include "engine/error.h"
#include "engine/sync.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/errors.h"
#include "engine/xapian/shard.h"
#include "engine/xapian/xapian.h"
#include "exception_xapian.h"
#include "hashes.hh"
#include "http_parser.h"
#include "log.h"
#include "phf.hh"
#include "server/http_utils.h"
#include "xapian/common/serialise-double.h"
#include "xapiand_exception.h"

namespace {

using engine::errc;

// Throws `f`'s exception through the adapter's translation, as every Shard method does.
void translated(const std::function<void()>& f) {
	try {
		f();
	} catch (...) {
		engine::xapian::rethrow_translated();
	}
}

template <typename X>
void expect_row(const X& original, errc kind) {
	bool caught = false;
	try {
		translated([&] { throw original; });
	} catch (const engine::Error& error) {
		caught = true;
		CHECK_EQ(error.kind, kind);
		CHECK_EQ(error.reason(), original.get_msg());
		CHECK_EQ(error.description(), original.get_description());
		const char* system = original.get_error_string();
		CHECK_EQ(error.detail(), std::string(system ? system : ""));
	}
	CHECK_TRUE(caught);
}

// One row per Xapian exception: the kind and the three texts the old dispatch read.
void test_accessor_table() {
	expect_row(Xapian::DatabaseNotFoundError("nf", "ctx"), errc::not_found);
	expect_row(Xapian::DatabaseNotAvailableError("na"), errc::not_available);
	expect_row(Xapian::DatabaseModifiedError("mod"), errc::reopen);
	expect_row(Xapian::DatabaseCorruptError("cor"), errc::corrupt);
	expect_row(Xapian::DatabaseOpeningError("open", "ctx"), errc::opening);
	expect_row(Xapian::DatabaseClosedError("closed"), errc::closed);
	expect_row(Xapian::NetworkTimeoutError("timeout", "ctx"), errc::timeout);
	expect_row(Xapian::NetworkError("net"), errc::network);
	expect_row(Xapian::NetworkError("Connection refused", "ctx", ECONNREFUSED), errc::network);
	expect_row(Xapian::DocNotFoundError("dnf"), errc::doc_not_found);
	expect_row(Xapian::DocVersionConflictError("conflict"), errc::version_conflict);
	expect_row(Xapian::InvalidArgumentError("arg"), errc::invalid);
	expect_row(Xapian::InvalidOperationError("op"), errc::invalid);
	expect_row(Xapian::QueryParserError("syntax"), errc::syntax);
	expect_row(Xapian::DatabaseError("db"), errc::database);
	expect_row(Xapian::DatabaseLockError("lock"), errc::database);
	expect_row(Xapian::RangeError("range"), errc::internal);
	expect_row(Xapian::InternalError("internal"), errc::internal);
	expect_row(Xapian::AssertionError("assertion"), errc::internal);
}

void test_translation_passes_others() {
	bool same = false;
	try {
		translated([] { throw std::runtime_error("plain"); });
	} catch (const std::runtime_error& error) {
		same = std::string(error.what()) == "plain";
	}
	CHECK_TRUE(same);

	// Xapiand's own exceptions keep their type even when they also derive from a Xapian one.
	bool own = false;
	try {
		translated([] { THROW(SerialisationError, "bad bytes"); });
	} catch (const SerialisationError&) {
		own = true;
	} catch (...) { }
	CHECK_TRUE(own);

	// An engine error is rethrown as it is.
	bool kept = false;
	try {
		translated([] { throw engine::Error(errc::timeout, "t"); });
	} catch (const engine::Error& error) {
		kept = error.kind == errc::timeout && error.reason() == "t" && error.description() == "t";
	}
	CHECK_TRUE(kept);
}

void test_fates() {
	using engine::fate;
	for (auto kind : {errc::reopen, errc::opening, errc::not_found, errc::not_available, errc::timeout, errc::network}) {
		CHECK_TRUE(engine::read_fate(kind) == fate::retry_close);
	}
	CHECK_TRUE(engine::read_fate(errc::closed) == fate::close_retry);
	CHECK_TRUE(engine::read_fate(errc::corrupt) == fate::close_fail);
	CHECK_TRUE(engine::read_fate(errc::database) == fate::close_fail);
	for (auto kind : {errc::doc_not_found, errc::version_conflict, errc::invalid, errc::syntax, errc::internal}) {
		CHECK_TRUE(engine::read_fate(kind) == fate::fail);
	}
	for (auto kind : {errc::doc_not_found, errc::not_found}) {
		CHECK_TRUE(engine::is_missing(kind));
	}
	for (auto kind : {errc::reopen, errc::opening, errc::closed, errc::not_available, errc::timeout, errc::network, errc::version_conflict, errc::corrupt, errc::database, errc::invalid, errc::syntax, errc::internal}) {
		CHECK_TRUE(!engine::is_missing(kind));
	}
	CHECK_EQ(std::string(engine::to_string(errc::syntax)), "syntax");
}

// A database closed under a view: every method that reads the shard throws an engine::Error of
// the kind Xapian's exception stands for.
void test_view_throws_engine_errors() {
	const auto path = (std::filesystem::current_path() / "engine_error_view.db").string();
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	{
		engine::Document doc;
		doc.add_boolean_term("Q1");
		doc.add_posting("alpha", 1, 2);
		doc.add_value(3, "v3");
		doc.set_data("data");
		engine::sync_wait(opened.shard->add(doc, {}));
		engine::sync_wait(opened.shard->commit(engine::Transaction::none));
	}
	auto view = engine::sync_wait(opened.shard->get(1));
	opened.shard->database()->close();

	auto kind_of = [](const std::function<void()>& f) {
		try {
			f();
		} catch (const engine::Error& error) {
			return error.kind;
		} catch (...) {
			return errc::invalid;  // anything that is not an engine error is a failure of the contract
		}
		return errc::internal;
	};
	int threw = 0;
	auto expect = [&](const std::function<void()>& f) {
		const auto kind = kind_of(f);
		if (kind != errc::internal) {
			CHECK_TRUE(kind == errc::closed);
			++threw;
		}
	};
	expect([&] { view->data(); });
	expect([&] { view->value(3); });
	expect([&] { view->version(); });
	expect([&] { view->terms("", [](std::string_view, engine::termcount) {}); });
	expect([&] { view->values([](engine::valueno, std::string_view) {}); });
	expect([&] { view->term_details([](const engine::TermDetail&) {}); });
	expect([&] { (void)view->serialise(); });
	// A view reads lazily, so at least the data fetch hits the closed shard.
	CHECK_TRUE(threw > 0);
	std::printf("view methods that threw after close: %d of 7\n", threw);
}

// ---- HTTP answers ---------------------------------------------------------------------------

// The dispatch as it was when it caught Xapian types (copied from before the kinds).
template <typename Func>
http_errors_t old_catch_http_errors(Func&& func) {
	http_errors_t http_errors;
	try {
		http_errors.ret = func();
		http_errors.error_code = HTTP_STATUS_OK;
	}
#define OLD_ARM(TYPE, CODE) catch (const TYPE& exc) { \
		http_errors.error_code = CODE; \
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.get_msg(); \
	}
	catch (const MissingTypeError& exc) {
		http_errors.error_code = HTTP_STATUS_PRECONDITION_FAILED;
		http_errors.error = exc.what();
	}
	OLD_ARM(Xapian::DocNotFoundError, HTTP_STATUS_NOT_FOUND)
	OLD_ARM(Xapian::DatabaseNotFoundError, HTTP_STATUS_NOT_FOUND)
	OLD_ARM(Xapian::DocVersionConflictError, HTTP_STATUS_CONFLICT)
	OLD_ARM(Xapian::DatabaseNotAvailableError, HTTP_STATUS_SERVICE_UNAVAILABLE)
	OLD_ARM(Xapian::NetworkTimeoutError, HTTP_STATUS_GATEWAY_TIMEOUT)
	OLD_ARM(Xapian::DatabaseModifiedError, HTTP_STATUS_SERVICE_UNAVAILABLE)
	catch (const Xapian::NetworkError& exc) {
		std::string msg;
		const char* error_string = exc.get_error_string();
		if (!error_string) {
			msg = exc.get_msg();
			error_string = msg.c_str();
		}
		constexpr static auto _ = phf::make_phf({
			hhl("Can't assign requested address"),
			hhl("Connection refused"),
			hhl("Connection reset by peer"),
			hhl("Connection closed unexpectedly"),
		});
		switch (_.fhhl(error_string)) {
			case _.fhhl("Can't assign requested address"):
			case _.fhhl("Connection refused"):
			case _.fhhl("Connection reset by peer"):
			case _.fhhl("Connection closed unexpectedly"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			default:
				http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				http_errors.error = exc.get_description();
		}
	}
	catch (const DatetimeError& exc) {
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	}
	catch (const CartesianError& exc) {
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	}
	catch (const ClientError& exc) {
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	}
	catch (const BaseException& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = *exc.get_message() != 0 ? exc.get_message() : "Unkown BaseException!";
	}
	catch (const Xapian::Error& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = exc.get_description();
	}
	catch (const std::exception& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = *exc.what() != 0 ? exc.what() : "Unkown std::exception!";
	}
	catch (...) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = "Unknown exception!";
	}
#undef OLD_ARM
	return http_errors;
}

// The Xapiand exception types as they were: each also a Xapian exception.
struct OldDocVersionConflict : ClientError, Xapian::DocVersionConflictError {
	explicit OldDocVersionConflict(const std::string& m) : ClientError(__func__, __FILE__, __LINE__, "ClientError", m), Xapian::DocVersionConflictError(message) { }
};
struct OldNetwork : ClientError, Xapian::NetworkError {
	explicit OldNetwork(const std::string& m) : ClientError(__func__, __FILE__, __LINE__, "ClientError", m), Xapian::NetworkError(message) { }
};
struct OldInvalidArgument : ClientError, Xapian::InvalidArgumentError {
	explicit OldInvalidArgument(const std::string& m) : ClientError(__func__, __FILE__, __LINE__, "ClientError", m), Xapian::InvalidArgumentError(message) { }
};
struct OldQueryParser : ClientError, Xapian::QueryParserError {
	explicit OldQueryParser(const std::string& m) : ClientError(__func__, __FILE__, __LINE__, "ClientError", m), Xapian::QueryParserError(message) { }
};

// The remote and replication servers answer any Xapian::Error with an error reply and keep the
// connection; a type they throw or serialise that loses the base closes the connection instead.
static_assert(std::is_base_of_v<Xapian::Error, InvalidArgumentError>);
static_assert(std::is_base_of_v<Xapian::Error, NetworkError>);
static_assert(std::is_base_of_v<Xapian::Error, SerialisationError>);
static_assert(std::is_base_of_v<Xapian::Error, engine::xapian::InvalidArgument>);

struct Case {
	std::string name;
	std::function<void()> raw;        // what the old code saw
	std::function<void()> engine_;    // what the new code sees (empty: the same as raw, translated)
	int expect_code = 0;              // nonzero: the new answer is this instead of the old one
	std::string expect_error;
};

void test_http_answers_unchanged() {
	std::vector<Case> cases;
	auto xapian_case = [&](const std::string& name, std::function<void()> raw) {
		cases.push_back({name, raw, nullptr});
	};
	xapian_case("DocNotFound", [] { throw Xapian::DocNotFoundError("Document not found"); });
	xapian_case("DatabaseNotFound", [] { throw Xapian::DatabaseNotFoundError("Couldn't open", "ctx"); });
	xapian_case("DocVersionConflict", [] { throw Xapian::DocVersionConflictError("Version mismatch!"); });
	xapian_case("NotAvailable", [] { throw Xapian::DatabaseNotAvailableError("Shard is not available"); });
	xapian_case("NetworkTimeout", [] { throw Xapian::NetworkTimeoutError("timed out", "ctx"); });
	xapian_case("Modified", [] { throw Xapian::DatabaseModifiedError("modified"); });
	xapian_case("Network refused", [] { throw Xapian::NetworkError("connect", "ctx", ECONNREFUSED); });
	xapian_case("Network reset", [] { throw Xapian::NetworkError("read", "ctx", ECONNRESET); });
	xapian_case("Network other", [] { throw Xapian::NetworkError("odd", "ctx", EPIPE); });
	xapian_case("Network no errno", [] { throw Xapian::NetworkError("Connection refused"); });
	xapian_case("Network text", [] { throw Xapian::NetworkError("plain"); });
	xapian_case("Opening", [] { throw Xapian::DatabaseOpeningError("opening", "ctx"); });
	xapian_case("Closed", [] { throw Xapian::DatabaseClosedError("closed"); });
	xapian_case("Corrupt", [] { throw Xapian::DatabaseCorruptError("corrupt"); });
	xapian_case("DatabaseError", [] { throw Xapian::DatabaseError("db"); });
	xapian_case("InvalidArgument", [] { throw Xapian::InvalidArgumentError("arg"); });
	xapian_case("InvalidOperation", [] { throw Xapian::InvalidOperationError("op"); });
	xapian_case("QueryParser", [] { throw Xapian::QueryParserError("Syntax: bad"); });
	// A syntax error from the query parser answers 400 (it was 500).
	cases.back().expect_code = HTTP_STATUS_BAD_REQUEST;
	cases.back().expect_error = "Bad Request: QueryParserError: Syntax: bad";
	xapian_case("RangeError", [] { throw Xapian::RangeError("range"); });
	xapian_case("Internal", [] { throw Xapian::InternalError("internal"); });
	xapian_case("Serialisation", [] { throw Xapian::SerialisationError("serial"); });
	// Not Xapian: the same in both.
	for (const auto& [name, f] : std::vector<std::pair<std::string, std::function<void()>>>{
			{"ClientError", [] { THROW(ClientError, "client"); }},
			{"MissingType", [] { THROW(MissingTypeError, "missing type"); }},
			{"SerialisationError class", [] { THROW(SerialisationError, "bad bytes"); }},
			{"runtime_error", [] { throw std::runtime_error("rt"); }},
			{"int", [] { throw 3; }},
		}) {
		cases.push_back({name, f, f});
	}
	cases.push_back({"DocVersionConflictError class", [] { throw OldDocVersionConflict("stale"); }, [] { THROW(DocVersionConflictError, "stale"); }});
	cases.push_back({"NetworkError class", [] { throw OldNetwork("Endpoint node is invalid"); }, [] { THROW(NetworkError, "Endpoint node is invalid"); }});
	cases.push_back({"InvalidArgumentError class", [] { throw OldInvalidArgument("bad arg"); }, [] { THROW(InvalidArgumentError, "bad arg"); }});
	cases.push_back({"QueryParserError class", [] { throw OldQueryParser("bad query"); }, [] { THROW(QueryParserError, "bad query"); }});

	int compared = 0;
	for (auto& c : cases) {
		auto old_answer = old_catch_http_errors([&] { c.raw(); return 0; });
		auto new_answer = catch_http_errors([&] {
			if (c.engine_) {
				c.engine_();
			} else {
				translated(c.raw);
			}
			return 0;
		});
		if (c.expect_code != 0) {
			old_answer.error_code = static_cast<http_status>(c.expect_code);
			old_answer.error = c.expect_error;
		}
		if (old_answer.error_code != new_answer.error_code || old_answer.error != new_answer.error) {
			std::printf("MISMATCH %s: old %d '%s' new %d '%s'\n", c.name.c_str(), static_cast<int>(old_answer.error_code), old_answer.error.c_str(), static_cast<int>(new_answer.error_code), new_answer.error.c_str());
		}
		CHECK_EQ(static_cast<int>(old_answer.error_code), static_cast<int>(new_answer.error_code));
		CHECK_EQ(old_answer.error, new_answer.error);
		++compared;
	}
	std::printf("http answers compared: %d\n", compared);
}

// The doubles that metric_serialise.h and the callbacks put on the wire are Xapian's bytes
// (metric_serialise.h calls engine::serialise_double, which is ::serialise_double).
void test_double_bytes_unchanged() {
	const std::vector<double> values = {0.0, -0.0, 1.0, -1.0, 0.1, 0.7, 0.25, 1e-300, 1e300, 3.141592653589793,
		-2.5e-7, 65536.0, 4503599627370496.0, 123456789.123456789, 1.7976931348623157e308, 2.2250738585072014e-308};
	for (double v : values) {
		const auto bytes = engine::serialise_double(v);
		CHECK_EQ(bytes, ::serialise_double(v));
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

TEST_MAIN(test_accessor_table, test_translation_passes_others, test_fates, test_view_throws_engine_errors, test_http_answers_unchanged, test_double_bytes_unchanged)
