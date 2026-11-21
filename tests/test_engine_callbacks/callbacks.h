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

#pragma once

// The key makers, spies and posting sources the callback tests hand to a search.

#include "../test_harness.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "engine/callbacks.h"

namespace detail {

// Orders by the value of one slot.
class SlotKey final : public engine::KeyMaker {
public:
	explicit SlotKey(engine::valueno slot) : slot_(slot) { }
	std::string key(const engine::DocView& doc) const override { return doc.value(slot_); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(slot_); }
	std::string name() const override { return "SlotKey"; }
	std::string serialize() const override { return std::to_string(slot_); }
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view s) const override { return std::make_unique<SlotKey>(static_cast<engine::valueno>(std::stoul(std::string(s)))); }

private:
	engine::valueno slot_;
};

// Stands for Xapiand's own client errors, which are not engine errors: they must leave a search as thrown.
struct ClientLike : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class ThrowingKey final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView&) const override { throw ClientLike("key"); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<ThrowingKey>(); }
	std::string name() const override { return "ThrowingKey"; }
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view) const override { return std::make_unique<ThrowingKey>(); }
};

// Counts documents per group (slot 2) and sums the numbers of slot 3, and keeps the documents it saw.
// What it holds serializes and merges, as an engine that matches shards apart needs.
class GroupSpy final : public engine::MatchSpy {
public:
	std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> groups;
	std::set<std::string> datas;   // by stored data: a spy is given the id within its shard
	double weights = 0;
	bool throw_on = false;

	void see(const engine::DocView& doc, double weight) override {
		if (throw_on && datas.size() == 2) {
			throw ClientLike("spy");
		}
		auto& entry = groups[doc.value(GROUP)];
		++entry.first;
		const auto number = doc.value(NUMBER);
		entry.second += number.empty() ? 0 : std::stoull(number);
		datas.insert(doc.data());
		weights += weight;
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<GroupSpy>(); }
	std::string name() const override { return "GroupSpy"; }
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialize(std::string_view) const override { return std::make_unique<GroupSpy>(); }
	std::string serialize_results() const override {
		std::ostringstream out;
		out.precision(17);
		for (const auto& [group, entry] : groups) {
			out << "g " << group << ' ' << entry.first << ' ' << entry.second << '\n';
		}
		for (const auto& data : datas) {
			out << "i " << data << '\n';
		}
		out << "w " << weights << '\n';
		return out.str();
	}
	void merge_results(std::string_view serialized) override {
		std::istringstream in{std::string(serialized)};
		std::string kind;
		while (in >> kind) {
			if (kind == "g") {
				std::string group;
				std::uint64_t count, sum;
				in >> group >> count >> sum;
				groups[group].first += count;
				groups[group].second += sum;
			} else if (kind == "i") {
				std::string data;
				in >> data;
				datas.insert(data);
			} else {
				double w;
				in >> w;
				weights += w;
			}
		}
	}
	std::string description() const override { return "GroupSpy"; }
};

// Accepts the values of a slot between two bounds (memcmp order); weighs by the last digit.
class Between final : public engine::PostingSource {
public:
	Between(engine::valueno slot, std::string low, std::string high, double max_weight = 0) : slot_(slot), low_(std::move(low)), high_(std::move(high)), max_(max_weight) { }
	engine::valueno slot() const override { return slot_; }
	bool accept(std::string_view value) const override { return value >= low_ && value <= high_; }
	double weight(std::string_view value) const override { return 1.0 + 0.25 * (value.back() - '0'); }
	double max_weight() const override { return max_; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Between>(slot_, low_, high_, max_); }
	std::string name() const override { return "Between"; }
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::PostingSource> unserialize(std::string_view) const override { return clone(); }
	std::string description() const override { return "Between"; }

private:
	engine::valueno slot_;
	std::string low_, high_;
	double max_;
};

}  // namespace detail
