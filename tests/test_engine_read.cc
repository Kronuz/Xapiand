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

// Unit tests for the read slice of the engine shard (stats, find_key, get, scan, metadata, check)
// and the DocView it hands out, each compared with what the test stored.

#include "test_harness.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/error.h"
#include "engine/open.h"
#include "engine/sync.h"

namespace {

constexpr int kDocs = 24;

std::string path_of(const char* name) {
	return (std::filesystem::current_path() / name).string();
}

engine::errc kind_of(const std::function<void()>& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

// Documents 1..kDocs with a key term, positions, two values and data; every fourth is erased.
std::unique_ptr<engine::Shard> build(const std::string& path) {
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (int i = 1; i <= kDocs; ++i) {
		engine::Document doc;
		doc.set_key("Q" + std::to_string(i));
		doc.add_posting(doc.field("f"), "alpha", 1, 2);
		doc.add_posting(doc.field("f"), "alpha", 5);
		doc.add_posting(doc.field("f"), "beta" + std::to_string(i % 3), 2, 3);
		if (i % 2 == 0) {
			doc.add_term(doc.field("f"), "even", 4);
		}
		doc.add_value(3, "v3-" + std::to_string(i));
		doc.add_value(9, std::string("v9\0x", 4) + std::to_string(i));
		doc.set_data("data " + std::to_string(i));
		engine::sync_wait(shard.add(doc, {}));
	}
	for (int i = 4; i <= kDocs; i += 4) {
		engine::sync_wait(shard.erase(static_cast<engine::docid>(i), true, std::nullopt));
	}
	engine::sync_wait(shard.set_metadata("k1", "one"));
	engine::sync_wait(shard.set_metadata("k2", std::string("tw\0o", 4)));
	engine::sync_wait(shard.commit(engine::Transaction::none));
	return std::move(opened.shard);
}

void test_stats() {
	auto shard = build(path_of("engine_read_stats.db"));
	auto stats = engine::sync_wait(shard->stats());
	CHECK_FALSE(stats.uuid.empty());
	CHECK_EQ(stats.uuid, shard->uuid());
	CHECK_EQ(stats.revision, shard->revision());
	CHECK_TRUE(stats.has_positions);
	CHECK_EQ(stats.doc_count, static_cast<engine::doccount>(kDocs - kDocs / 4));
	CHECK_EQ(stats.lastdocid, static_cast<engine::docid>(kDocs));
	CHECK_EQ(shard->doc_count(), stats.doc_count);
	// A live document has alpha (3), beta (3) and, when its id is even, even (4): 6 or 10 terms long.
	CHECK_EQ(stats.doclen_lower, 6u);
	CHECK_EQ(stats.doclen_upper, 10u);
	CHECK_EQ(stats.total_length, 12u * 6 + 6u * 10);
	CHECK_TRUE(stats.avlength == static_cast<double>(stats.total_length) / stats.doc_count);
}

void test_find_key_and_get() {
	auto shard = build(path_of("engine_read_get.db"));
	for (int i = 1; i <= kDocs; ++i) {
		const bool erased = i % 4 == 0;
		auto did = engine::sync_wait(shard->find_key("Q" + std::to_string(i)));
		CHECK_EQ(did, erased ? 0u : static_cast<engine::docid>(i));
		if (erased) {
			CHECK_TRUE(kind_of([&] { engine::sync_wait(shard->get(static_cast<engine::docid>(i))); }) == engine::errc::doc_not_found);
			continue;
		}
		auto view = engine::sync_wait(shard->get(did));
		CHECK_EQ(view->did(), did);
		CHECK_EQ(view->data(), "data " + std::to_string(i));
		CHECK_EQ(view->value(3), "v3-" + std::to_string(i));
		CHECK_EQ(view->value(9), std::string("v9\0x", 4) + std::to_string(i));
		CHECK_EQ(view->value(4), "");

		// values: every value, in slot order (the engine adds its version slot, 1)
		std::vector<std::pair<engine::valueno, std::string>> values;
		view->values([&](engine::valueno slot, std::string_view value) {
			if (slot != 1) {
				values.emplace_back(slot, std::string(value));
			}
		});
		const std::vector<std::pair<engine::valueno, std::string>> want = {
			{3, "v3-" + std::to_string(i)}, {9, std::string("v9\0x", 4) + std::to_string(i)},
		};
		CHECK_TRUE(values == want);
		CHECK_EQ(view->version(), 1u);
	}
}

void test_view_outlives_shard() {
	std::unique_ptr<engine::DocView> view;
	{
		auto shard = build(path_of("engine_read_own.db"));
		view = engine::sync_wait(shard->get(2));
	}
	CHECK_EQ(view->data(), "data 2");
	CHECK_EQ(view->value(3), "v3-2");
}

// A scan cut short by a throwing sink and resumed from the last id, as the dump retry does, sees the
// documents after the one it stopped at.
void test_scan_resumes_after_a_throwing_sink() {
	auto shard = build(path_of("engine_read_resume.db"));
	engine::docid last = 0;
	int count = 0;
	bool thrown = false;
	try {
		engine::sync_wait(shard->scan(1, [&](const engine::DocView& doc) {
			last = doc.did();
			if (++count == 3) {
				throw std::runtime_error("stop");
			}
		}));
	} catch (const std::runtime_error&) {
		thrown = true;
	}
	CHECK_TRUE(thrown);
	size_t resumed = 0;
	engine::sync_wait(shard->scan(last, [&](const engine::DocView&) { ++resumed; }));
	CHECK_EQ(resumed, static_cast<size_t>(kDocs - kDocs / 4) - 2);
}

void test_scan() {
	auto shard = build(path_of("engine_read_scan.db"));
	for (engine::docid from : {0u, 1u, 2u, 4u, 5u, 23u, 24u, 25u, 1000u}) {
		std::vector<engine::docid> got, want;
		std::vector<std::string> data, want_data;
		engine::sync_wait(shard->scan(from, [&](const engine::DocView& doc) {
			got.push_back(doc.did());
			data.push_back(doc.data());
		}));
		for (int i = 1; i <= kDocs; ++i) {
			if (i % 4 != 0 && static_cast<engine::docid>(i) >= from) {
				want.push_back(static_cast<engine::docid>(i));
				want_data.push_back("data " + std::to_string(i));
			}
		}
		CHECK_TRUE(got == want);
		CHECK_TRUE(data == want_data);
	}
	std::vector<engine::docid> all;
	engine::sync_wait(shard->scan(1, [&](const engine::DocView& doc) { all.push_back(doc.did()); }));
	CHECK_EQ(all.size(), static_cast<size_t>(kDocs - kDocs / 4));
	for (size_t i = 1; i < all.size(); ++i) {
		CHECK_LT(all[i - 1], all[i]);
	}

	// a sink that throws ends the scan with its own exception, and the shard stays usable
	int seen = 0;
	bool thrown = false;
	try {
		engine::sync_wait(shard->scan(1, [&](const engine::DocView&) {
			if (++seen == 3) {
				throw std::runtime_error("stop");
			}
		}));
	} catch (const std::runtime_error& error) {
		thrown = std::string(error.what()) == "stop";
	}
	CHECK_TRUE(thrown);
	CHECK_EQ(seen, 3);
	CHECK_EQ(engine::sync_wait(shard->find_key("Q1")), 1u);
}

void test_metadata() {
	auto shard = build(path_of("engine_read_meta.db"));
	CHECK_EQ(engine::sync_wait(shard->metadata("k1")), "one");
	CHECK_EQ(engine::sync_wait(shard->metadata("k2")), std::string("tw\0o", 4));
	CHECK_EQ(engine::sync_wait(shard->metadata("absent")), "");
	auto keys = engine::sync_wait(shard->metadata_keys());
	CHECK_TRUE((keys == std::vector<std::string>{"k1", "k2"}));
}

void test_check() {
	auto path = path_of("engine_read_check.db");
	{
		auto shard = build(path);
	}
	auto good = engine::sync_wait(engine::check(path));
	CHECK_EQ(good.errors, 0u);
	CHECK_TRUE(good.failure.empty());

	// a path that is not a database fails with a description, as a value
	auto missing = engine::sync_wait(engine::check(path + ".missing"));
	CHECK_FALSE(missing.failure.empty());
}

}  // namespace

TEST_MAIN(test_stats, test_find_key_and_get, test_view_outlives_shard, test_scan_resumes_after_a_throwing_sink, test_scan, test_metadata, test_check)
