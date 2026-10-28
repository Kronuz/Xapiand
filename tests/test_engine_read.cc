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
// and the DocView it hands out, each compared with what Xapian itself reports.

#include "test_harness.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#include "engine/sync.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/errors.h"
#include "engine/xapian/shard.h"
#include "engine/xapian/xapian.h"

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
std::unique_ptr<engine::xapian::Shard> build(const std::string& path) {
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (int i = 1; i <= kDocs; ++i) {
		engine::Document doc;
		doc.add_boolean_term("Q" + std::to_string(i));
		doc.add_posting("alpha", 1, 2);
		doc.add_posting("alpha", 5);
		doc.add_posting("beta" + std::to_string(i % 3), 2, 3);
		if (i % 2 == 0) {
			doc.add_term("even", 4);
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
	auto* db = shard->database();
	auto stats = engine::sync_wait(shard->stats());
	CHECK_EQ(stats.uuid, db->get_uuid());
	CHECK_EQ(stats.revision, db->get_revision());
	CHECK_EQ(stats.doc_count, db->get_doccount());
	CHECK_EQ(stats.lastdocid, db->get_lastdocid());
	CHECK_EQ(stats.total_length, db->get_total_length());
	CHECK_EQ(stats.doclen_lower, db->get_doclength_lower_bound());
	CHECK_EQ(stats.doclen_upper, db->get_doclength_upper_bound());
	CHECK_TRUE(stats.avlength == db->get_avlength());
	CHECK_EQ(stats.has_positions, db->has_positions());
	CHECK_TRUE(stats.has_positions);
	CHECK_EQ(stats.doc_count, static_cast<engine::doccount>(kDocs - kDocs / 4));
	CHECK_EQ(stats.lastdocid, static_cast<engine::docid>(kDocs));
	CHECK_EQ(shard->doc_count(), stats.doc_count);
}

void test_find_key_and_get() {
	auto shard = build(path_of("engine_read_get.db"));
	auto* db = shard->database();
	for (int i = 1; i <= kDocs; ++i) {
		const bool erased = i % 4 == 0;
		auto did = engine::sync_wait(shard->find_key("Q" + std::to_string(i)));
		CHECK_EQ(did, erased ? 0u : static_cast<engine::docid>(i));
		if (erased) {
			CHECK_TRUE(kind_of([&] { engine::sync_wait(shard->get(static_cast<engine::docid>(i))); }) == engine::errc::doc_not_found);
			continue;
		}
		auto view = engine::sync_wait(shard->get(did));
		auto doc = db->get_document(did);
		CHECK_EQ(view->did(), did);
		CHECK_EQ(view->data(), doc.get_data());
		CHECK_EQ(view->value(3), doc.get_value(3));
		CHECK_EQ(view->value(9), doc.get_value(9));
		CHECK_EQ(view->value(4), "");
		CHECK_EQ(view->serialise(), doc.serialise());

		// values: every value, in slot order
		std::vector<std::pair<engine::valueno, std::string>> values;
		view->values([&](engine::valueno slot, std::string_view value) { values.emplace_back(slot, std::string(value)); });
		std::vector<std::pair<engine::valueno, std::string>> want;
		for (auto it = doc.values_begin(); it != doc.values_end(); ++it) {
			want.emplace_back(it.get_valueno(), *it);
		}
		CHECK_TRUE(values == want);
		CHECK_GE(values.size(), 2u);  // the engine adds its version slot

		// term details: term order, wdf, frequency in the database, positions
		struct Row { std::string term; engine::termcount wdf; std::optional<engine::doccount> freq; std::vector<engine::termpos> pos; };
		std::vector<Row> rows, expect;
		view->term_details([&](const engine::TermDetail& d) { rows.push_back({std::string(d.term), d.wdf, d.termfreq, d.positions}); });
		for (auto it = doc.termlist_begin(); it != doc.termlist_end(); ++it) {
			Row row{*it, it.get_wdf(), it.get_termfreq(), {}};
			for (auto p = it.positionlist_begin(); p != it.positionlist_end(); ++p) {
				row.pos.push_back(*p);
			}
			expect.push_back(std::move(row));
		}
		CHECK_EQ(rows.size(), expect.size());
		for (size_t r = 0; r < rows.size() && r < expect.size(); ++r) {
			CHECK_EQ(rows[r].term, expect[r].term);
			CHECK_EQ(rows[r].wdf, expect[r].wdf);
			CHECK_TRUE(rows[r].freq == expect[r].freq);
			CHECK_TRUE(rows[r].pos == expect[r].pos);
		}
		CHECK_TRUE(!rows.empty() && rows.front().freq.has_value());
	}
	// the document with positions reports them; the one with only a boolean term reports none
	auto view = engine::sync_wait(shard->get(1));
	bool saw_alpha = false;
	view->term_details([&](const engine::TermDetail& d) {
		if (d.term == "alpha") { saw_alpha = d.positions == std::vector<engine::termpos>{1, 5}; }
		if (d.term == "Q1") { CHECK_TRUE(d.positions.empty()); }
	});
	CHECK_TRUE(saw_alpha);
}

void test_view_outlives_shard() {
	std::unique_ptr<engine::DocView> view;
	std::string want;
	{
		auto shard = build(path_of("engine_read_own.db"));
		view = engine::sync_wait(shard->get(2));
		auto doc = shard->database()->get_document(2);
		for (auto it = doc.termlist_begin(); it != doc.termlist_end(); ++it) {
			want += *it + ",";
		}
	}
	CHECK_EQ(view->data(), "data 2");
	CHECK_EQ(view->value(3), "v3-2");
	std::string joined;
	view->term_details([&](const engine::TermDetail& d) { joined += std::string(d.term) + ","; });
	CHECK_EQ(joined, want);
}

// Every view an engine read makes is destroyed: none alive after get, scan (also a scan cut short by
// a throwing sink and resumed, as the dump retry does) and find_key.
void test_views_do_not_leak() {
	auto shard = build(path_of("engine_read_live.db"));
	CHECK_EQ(engine::xapian::live_views(), 0u);

	// the counter sees a view that is held, and the release of it
	auto held = engine::sync_wait(shard->get(1));
	CHECK_EQ(engine::xapian::live_views(), 1u);
	held.reset();
	CHECK_EQ(engine::xapian::live_views(), 0u);

	for (int i = 1; i <= kDocs; ++i) {
		const bool erased = i % 4 == 0;
		CHECK_EQ(engine::sync_wait(shard->find_key("Q" + std::to_string(i))), erased ? 0u : static_cast<engine::docid>(i));
		if (!erased) {
			{ auto view = engine::sync_wait(shard->get(static_cast<engine::docid>(i))); view->data(); }
		} else {
			CHECK_TRUE(kind_of([&] { engine::sync_wait(shard->get(static_cast<engine::docid>(i))); }) == engine::errc::doc_not_found);
		}
		CHECK_EQ(engine::xapian::live_views(), 0u);
	}

	// a full scan, with one view alive inside the sink and none after it
	size_t inside = 0, seen = 0;
	engine::sync_wait(shard->scan(1, [&](const engine::DocView&) {
		++seen;
		inside = engine::xapian::live_views();
	}));
	CHECK_EQ(seen, static_cast<size_t>(kDocs - kDocs / 4));
	CHECK_EQ(inside, 1u);
	CHECK_EQ(engine::xapian::live_views(), 0u);

	// the retry path: a sink that throws mid-scan, then the scan resumed from the last id
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
	CHECK_EQ(engine::xapian::live_views(), 0u);
	size_t resumed = 0;
	engine::sync_wait(shard->scan(last, [&](const engine::DocView&) { ++resumed; }));
	CHECK_EQ(resumed, static_cast<size_t>(kDocs - kDocs / 4) - 2);
	CHECK_EQ(engine::xapian::live_views(), 0u);

	// a view that outlives its shard is still counted until it goes
	{
		auto own = build(path_of("engine_read_live_own.db"));
		held = engine::sync_wait(own->get(2));
	}
	CHECK_EQ(engine::xapian::live_views(), 1u);
	held.reset();
	CHECK_EQ(engine::xapian::live_views(), 0u);
}

void test_scan() {
	auto shard = build(path_of("engine_read_scan.db"));
	auto* db = shard->database();
	for (engine::docid from : {0u, 1u, 2u, 4u, 5u, 23u, 24u, 25u, 1000u}) {
		std::vector<engine::docid> got, want;
		std::vector<std::string> data, want_data;
		engine::sync_wait(shard->scan(from, [&](const engine::DocView& doc) {
			got.push_back(doc.did());
			data.push_back(doc.data());
		}));
		auto it = db->postlist_begin("");
		it.skip_to(from);
		for (; it != db->postlist_end(""); ++it) {
			want.push_back(*it);
			want_data.push_back(db->get_document(*it).get_data());
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
	auto* db = shard->database();
	std::vector<std::string> want(db->metadata_keys_begin(), db->metadata_keys_end());
	CHECK_TRUE(keys == want);
}

void test_check() {
	auto path = path_of("engine_read_check.db");
	{
		auto shard = build(path);
	}
	auto good = engine::sync_wait(engine::xapian::check(path));
	CHECK_EQ(good.errors, 0u);
	CHECK_TRUE(good.failure.empty());

	// a path that is not a database fails with Xapian's own description, as a value
	auto missing = engine::sync_wait(engine::xapian::check(path + ".missing"));
	CHECK_FALSE(missing.failure.empty());
	std::string direct;
	try {
		Xapian::Database::check(path + ".missing");
	} catch (const Xapian::Error& error) {
		direct = error.get_description();
	}
	CHECK_EQ(missing.failure, direct);
}

struct Unrelated { virtual ~Unrelated() = default; };

void test_native() {
	auto shard = build(path_of("engine_read_native.db"));
	engine::Shard& seam = *shard;
	CHECK_TRUE(&seam.native<engine::xapian::Shard>() == shard.get());
	CHECK_TRUE(seam.native<engine::xapian::Shard>().database() == shard->database());
	bool refused = false;
	try {
		seam.native<Unrelated>();
	} catch (const std::bad_cast&) {
		refused = true;
	}
	CHECK_TRUE(refused);
}

}  // namespace

TEST_MAIN(test_stats, test_find_key_and_get, test_view_outlives_shard, test_views_do_not_leak, test_scan, test_metadata, test_check, test_native)
