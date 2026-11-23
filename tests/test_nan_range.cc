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

// A NaN range bound answers 400 naming the field on every numeric and date field, in a query's `_range`
// and in a range aggregation; a finite bound on the same field answers 200.

#include "test_harness.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "msgpack.h"
#include "query_dsl.h"
#include "server/http_utils.h"

namespace {

constexpr const char* FIELDS[] = { "i", "p", "f", "d" };  // integer, positive, floating, date
constexpr const char* BOUNDS[] = { "_from", "_to" };

std::shared_ptr<Schema> schema() {
	MsgPack props = MsgPack::MAP();
	props["i"] = MsgPack({ { "_type", "integer" }, { "_slot", 1301 } });
	props["p"] = MsgPack({ { "_type", "positive" }, { "_slot", 1302 } });
	props["f"] = MsgPack({ { "_type", "floating" }, { "_slot", 1303 } });
	props["d"] = MsgPack({ { "_type", "date" }, { "_slot", 1304 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

bool contains(std::string_view text, std::string_view part) {
	return text.find(part) != std::string_view::npos;
}

// A range with `bound` set to `value` and the other end at 1000.
MsgPack range_with(const char* bound, const MsgPack& value) {
	MsgPack range = MsgPack::MAP();
	range[bound] = value;
	range[std::string_view(bound) == "_from" ? "_to" : "_from"] = MsgPack(std::string_view(bound) == "_from" ? 1000 : 0);
	return range;
}

http_errors_t aggregate(const char* field, const MsgPack& range) {
	MsgPack ranges = MsgPack::ARRAY();
	ranges.push_back(range);
	const auto config = MsgPack({ { "_aggs", { { "r", { { "_range", { { "_field", field }, { "_ranges", ranges } } } } } } } });
	const auto s = schema();
	return catch_http_errors([&]() -> int {
		AggregationMatchSpy spy(config, s);
		return 0;
	});
}

http_errors_t query(const char* field, const MsgPack& range) {
	const auto s = schema();
	return catch_http_errors([&]() -> int {
		QueryDSL dsl(s);
		dsl.get_query(MsgPack({ { field, { { "_in", { { "_range", range } } } } } }));
		return 0;
	});
}

void test_range_aggregation_nan_bound() {
	const MsgPack nan(std::nan(""));
	for (const auto* field : FIELDS) {
		for (const auto* bound : BOUNDS) {
			const auto finite = aggregate(field, range_with(bound, MsgPack(5)));
			std::fprintf(stderr, "agg %s %s 5: %d %s\n", field, bound, finite.error_code, finite.error.c_str());
			CHECK(finite.error_code == HTTP_STATUS_OK);

			const auto answer = aggregate(field, range_with(bound, nan));
			std::fprintf(stderr, "agg %s %s NaN: %d %s\n", field, bound, answer.error_code, answer.error.c_str());
			CHECK(answer.error_code == HTTP_STATUS_BAD_REQUEST);
			CHECK(contains(answer.error, std::string("Field '") + field + "' range bound '" + bound + "' cannot be NaN"));
		}
	}
}

void test_query_range_nan_bound() {
	const MsgPack nan(std::nan(""));
	for (const auto* field : FIELDS) {
		for (const auto* bound : BOUNDS) {
			const auto finite = query(field, range_with(bound, MsgPack(5)));
			std::fprintf(stderr, "query %s %s 5: %d %s\n", field, bound, finite.error_code, finite.error.c_str());
			CHECK(finite.error_code == HTTP_STATUS_OK);

			const auto answer = query(field, range_with(bound, nan));
			std::fprintf(stderr, "query %s %s NaN: %d %s\n", field, bound, answer.error_code, answer.error.c_str());
			CHECK(answer.error_code == HTTP_STATUS_BAD_REQUEST);
			CHECK(contains(answer.error, std::string("Field '") + field + "' range bound '" + bound + "' cannot be NaN"));
		}
	}
}

}  // namespace

TEST_MAIN(test_range_aggregation_nan_bound, test_query_range_nan_bound)
