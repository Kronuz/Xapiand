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

// Posting sources, key makers and match spies in a search, and who frees what a search makes.

#include "support.h"

namespace detail {

namespace {

// Documents whose slot-21 value holds a number in [lo, hi]; the weight is the number.
class Between final : public engine::PostingSource {
public:
	Between() = default;
	Between(int lo, int hi) : lo_(lo), hi_(hi) { }

	engine::valueno slot() const override { return 21; }
	bool accept(std::string_view value) const override { return !value.empty() && std::stoi(std::string(value)) >= lo_ && std::stoi(std::string(value)) <= hi_; }
	double weight(std::string_view value) const override { return std::stoi(std::string(value)); }
	double max_weight() const override { return 100; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Between>(lo_, hi_); }
	std::string name() const override { return "Between"; }
	std::string serialize() const override { return std::to_string(lo_) + "," + std::to_string(hi_); }
	std::unique_ptr<engine::PostingSource> unserialize(std::string_view s) const override {
		auto comma = s.find(',');
		if (comma == std::string_view::npos) {
			throw engine::Error(engine::errc::invalid, "Bad serialized Between");
		}
		return std::make_unique<Between>(std::stoi(std::string(s.substr(0, comma))), std::stoi(std::string(s.substr(comma + 1))));
	}
	std::string description() const override { return "Between " + serialize(); }

private:
	int lo_ = 0, hi_ = 0;
};

// Orders documents by the slot-21 value, descending.
class ByValue final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView& doc) const override {
		auto value = doc.value(21);
		for (auto& ch : value) {
			ch = static_cast<char>(0xff - static_cast<unsigned char>(ch));
		}
		return value;
	}
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<ByValue>(); }
	std::string name() const override { return "ByValue"; }
	std::string serialize() const override { return "v"; }
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view s) const override {
		if (s != "v") {
			throw engine::Error(engine::errc::invalid, "Bad serialized ByValue");
		}
		return std::make_unique<ByValue>();
	}
};

// Sums the weights it sees and remembers the data of the documents.
class Collect final : public engine::MatchSpy {
public:
	void see(const engine::DocView& doc, double weight) override {
		seen.insert(doc.did());
		total_weight += weight;
		data += doc.data();
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<Collect>(); }
	std::string name() const override { return "Collect"; }
	std::string serialize() const override { return "c"; }
	std::unique_ptr<engine::MatchSpy> unserialize(std::string_view s) const override {
		if (s != "c") {
			throw engine::Error(engine::errc::invalid, "Bad serialized Collect");
		}
		return std::make_unique<Collect>();
	}
	std::string serialize_results() const override {
		std::string result;
		for (auto did : seen) {
			result += std::to_string(did) + ",";
		}
		return result;
	}
	void merge_results(std::string_view s) override {
		std::string number;
		for (char ch : s) {
			if (ch == ',') {
				seen.insert(static_cast<engine::docid>(std::stoul(number)));
				number.clear();
			} else {
				number += ch;
			}
		}
	}
	std::string description() const override { return "Collect"; }

	std::set<engine::docid> seen;
	double total_weight = 0;
	std::string data;
};

// Ten documents with the term "t", the numbers 1 to 10 in slot 21 and "d1" to "d10" as data.
std::unique_ptr<engine::Shard> make_db() {
	auto shard = testsupport::scratch_shard("engine_query_ten");
	for (int i = 1; i <= 10; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		doc.add_value(21, std::to_string(i));
		doc.set_data("d" + std::to_string(i));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

}  // namespace

void test_posting_source() {
	auto db = make_db();

	// The search accepts exactly what the source accepts, through every match path.
	const auto query = Query::source(std::make_unique<Between>(3, 6));
	auto dids = ranked(*db, query);
	CHECK_EQ(dids.size(), 4U);
	CHECK_TRUE((std::set<engine::docid>(dids.begin(), dids.end()) == std::set<engine::docid>{ 3, 4, 5, 6 }));

	// A filter: skip_to and check run when the source meets another term.
	CHECK_TRUE((run(*db, Query::combine(Op::FILTER, Query::term("f", "t"), Query::source(std::make_unique<Between>(8, 9)))) == std::set<engine::docid>{ 8, 9 }));
	CHECK_TRUE((run(*db, Query::combine(Op::AND, Query::source(std::make_unique<Between>(2, 9)), Query::source(std::make_unique<Between>(5, 20)))) == std::set<engine::docid>{ 5, 6, 7, 8, 9 }));

	// The weight is the source's: the highest value ranks first.
	CHECK_EQ(dids.front(), 6U);

	// A malformed serialization is an invalid argument; a good one rebuilds the source.
	CHECK_THROW(Between().unserialize("bad"), engine::Error);
	CHECK_EQ(Between().unserialize("3,6")->description(), "Between 3,6");
	CHECK_EQ(Between().name(), "Between");
	CHECK_EQ(Between().description(), "Between 0,0");
}

void test_key_maker() {
	auto db = make_db();
	ByValue sorter;
	auto dids = ranked(*db, Query::term("f", "t"), &sorter);
	CHECK_EQ(dids.size(), 10U);
	// Descending by the number's text: "9", "8", ... "2", then "1" before its extension "10".
	CHECK_EQ(dids.front(), 9U);
	CHECK_EQ(dids.back(), 10U);

	auto rebuilt = sorter.unserialize("v");
	CHECK_TRUE(ranked(*db, Query::term("f", "t"), rebuilt.get()) == dids);
	CHECK_THROW(sorter.unserialize("x"), engine::Error);
}

void test_match_spy() {
	auto db = make_db();
	Collect collect;
	auto dids = ranked(*db, Query::term("f", "t"), nullptr, &collect);
	CHECK_EQ(dids.size(), 10U);
	CHECK_EQ(collect.seen.size(), 10U);
	CHECK_EQ(collect.data, "d1d2d3d4d5d6d7d8d9d10");
	CHECK_TRUE(collect.total_weight > 0);

	// Results of other shards merge through the serialized form.
	auto remote = collect.unserialize("c");
	auto remote_dids = ranked(*db, Query::combine(Op::AND, Query::term("f", "t"), Query::source(std::make_unique<Between>(9, 10))), nullptr, remote.get());
	CHECK_EQ(remote_dids.size(), 2u);
	Collect merged;
	merged.merge_results(remote->serialize_results());
	CHECK_EQ(merged.seen.size(), remote_dids.size());
	CHECK_TRUE((merged.seen == std::set<engine::docid>{ 9, 10 }));
	CHECK_THROW(collect.unserialize("x"), engine::Error);
}

namespace {

// Counts its live instances, to see whether a search frees what it makes.
class Counted final : public engine::PostingSource {
public:
	static inline int live = 0;

	Counted() { ++live; }
	Counted(const Counted&) = delete;
	~Counted() override { --live; }

	engine::valueno slot() const override { return 21; }
	bool accept(std::string_view value) const override { return !value.empty(); }
	double weight(std::string_view) const override { return 1.0; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Counted>(); }
	std::string name() const override { return "Counted"; }
	std::string serialize() const override { return "c"; }
	std::unique_ptr<engine::PostingSource> unserialize(std::string_view) const override { return std::make_unique<Counted>(); }
	std::string description() const override { return "Counted"; }
};

}  // namespace

// A search hands each shard its own clone of the source and frees it at the end: only the query's
// source stays alive.
void test_source_ownership() {
	auto db = make_db();
	Counted::live = 0;
	{
		auto query = Query::source(std::make_unique<Counted>());
		CHECK_EQ(Counted::live, 1);
		for (int i = 0; i < 20; ++i) {
			auto copy = query;
			CHECK_EQ(run(*db, copy).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);

		auto combined = Query::combine(Op::FILTER, Query::term("f", "t"), query);
		for (int i = 0; i < 20; ++i) {
			CHECK_EQ(run(*db, combined).size(), 10U);
		}
		CHECK_EQ(Counted::live, 1);
	}
	CHECK_EQ(Counted::live, 0);
}

}  // namespace detail
