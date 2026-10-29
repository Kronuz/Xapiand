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

// Callbacks and operators through the seam. One corpus is written to a set of strata shards (and, as a
// whole, to one more shard); every search is checked against a model of the corpus: the documents found
// and the estimate are exact, a search over shards weighs as one shard holding every document, hits come
// in weight order, a spy sees what the model says and a key maker orders as its keys do. On one shard
// and on five.

#include "test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <tuple>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace {

using engine::Op;
using engine::Query;

constexpr double tolerance = 1e-9;

// Value slots (slot 1 is the shard's own version slot).
constexpr engine::valueno KEY = 21, GROUP = 22, NUMBER = 23, EMPTY = 29;

// --- callbacks ---

// Orders by the value of one slot.
class SlotKey final : public engine::KeyMaker {
public:
	explicit SlotKey(engine::valueno slot) : slot_(slot) { }
	std::string key(const engine::DocView& doc) const override { return doc.value(slot_); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(slot_); }
	std::string name() const override { return "SlotKey"; }
	std::string serialise() const override { return std::to_string(slot_); }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view s) const override { return std::make_unique<SlotKey>(static_cast<engine::valueno>(std::stoul(std::string(s)))); }

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
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view) const override { return std::make_unique<ThrowingKey>(); }
};

// Counts documents per group (slot 2) and sums the numbers of slot 3, and keeps the documents it saw.
// What it holds serialises and merges, as an engine that matches shards apart needs.
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
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<GroupSpy>(); }
	std::string serialise_results() const override {
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
	void merge_results(std::string_view serialised) override {
		std::istringstream in{std::string(serialised)};
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
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view) const override { return clone(); }
	std::string description() const override { return "Between"; }

private:
	engine::valueno slot_;
	std::string low_, high_;
	double max_;
};

// --- the corpus ---

struct Index {
	std::vector<std::unique_ptr<engine::Shard>> owned;

	std::vector<engine::Shard*> shards(std::size_t n) const {
		std::vector<engine::Shard*> list;
		for (std::size_t i = 0; i < n && i < owned.size(); ++i) {
			list.push_back(owned[i].get());
		}
		return list;
	}
};

const std::vector<std::string> words = {"apple", "apricot", "apply", "banana", "band", "bandana", "bond", "cat", "cart", "cast", "coat", "caf\xc3\xa9", "cafe", "caf\xc3\xa9s", "na\xc3\xafve"};

std::string pad(unsigned n, int width) {
	char buffer[32];
	std::snprintf(buffer, sizeof buffer, "%0*u", width, n);
	return buffer;
}

std::string path_of(const char* name, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_callbacks_") + name + "_" + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

struct Spec {
	std::map<std::string, unsigned> terms;
	std::string key, group, number, data;
	bool has_number = true;
	bool live = true;
};

Spec random_spec(std::mt19937& rng, int shard, int i) {
	Spec spec;
	const int k = 1 + static_cast<int>(rng() % 4);
	for (int n = 0; n < k; ++n) {
		spec.terms[words[rng() % words.size()]] += 1 + rng() % 3;
	}
	spec.terms["common"] = 1;
	spec.terms["tag" + std::to_string(i % 5)] = 1 + i % 3;
	if (i % 2 == 0) {
		spec.terms["even"] = 2;
	}
	spec.key = pad(rng() % 1000000, 6) + "-" + std::to_string(shard) + "-" + std::to_string(i);
	spec.group = "g" + std::to_string(rng() % 4);
	spec.has_number = i % 7 != 0;
	spec.number = pad(rng() % 1000, 5);
	spec.data = "d" + std::to_string(shard) + "_" + std::to_string(i);
	return spec;
}

std::string key_of(int shard, int i) {
	return "Q" + std::to_string(shard) + "_" + std::to_string(i);
}

engine::Document make(int shard, int i, const Spec& spec) {
	engine::Document doc;
	doc.set_key(key_of(shard, i));
	const auto flat = doc.field("f");   // the terms are flat strings; no field name is joined to them
	for (const auto& [term, wdf] : spec.terms) {
		doc.add_term(flat, term, wdf);
	}
	doc.add_boolean_term(flat, "B" + spec.group);   // a term without frequency, as the fields of a schema make
	doc.add_value(KEY, spec.key);
	doc.add_value(GROUP, spec.group);
	if (spec.has_number) {
		doc.add_value(NUMBER, spec.number);
	}
	doc.set_data(spec.data);
	return doc;
}

struct Corpus {
	Index strata;
	Index merged;   // every document of every shard in one shard: the whole-index statistics
	std::vector<std::vector<Spec>> specs;
};

// The documents on `shard_count` shards, `commits` commits per shard (several segments), and all of them in one more shard.
void build(const char* name, int shard_count, int count, int commits, Corpus& corpus) {
	std::mt19937 rng(4321);
	corpus.specs.assign(static_cast<std::size_t>(shard_count), {});
	corpus.strata.owned.clear();
	for (int s = 0; s < shard_count; ++s) {
		corpus.strata.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, s), true, true)).shard);
	}
	corpus.merged.owned.clear();
	corpus.merged.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, 99), true, true)).shard);
	for (int s = 0; s < shard_count; ++s) {
		auto& list = corpus.specs[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			for (int n = 0; n < count / commits; ++n) {
				const int i = static_cast<int>(list.size()) + 1;
				list.push_back(random_spec(rng, s, i));
				// Shard 0 also holds a few terms of known, distinct frequencies.
				if (s == 0) {
					for (const auto& [term, upto] : {std::pair{"mf_a", 5}, {"mf_b", 3}, {"mf_c", 8}, {"mf_d", 1}}) {
						if (i <= upto) {
							list.back().terms[term] = 1;
						}
					}
				}
				for (auto* index : {&corpus.strata, &corpus.merged}) {
					auto doc = make(s, i, list.back());
					engine::sync_wait(index->owned[index == &corpus.merged ? 0 : static_cast<std::size_t>(s)]->add(doc, {}));
				}
			}
			engine::sync_wait(corpus.strata.owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
		}
	}
	engine::sync_wait(corpus.merged.owned[0]->commit(engine::Transaction::none));
}

// --- the model: which documents a query matches ---

bool has_term(const Spec& spec, const std::string& term) {
	return spec.terms.contains(term) || term == "B" + spec.group;
}

// The terms of the field "f" over the live documents of the first `shards` shards, with the number of documents holding each.
using Dictionary = std::map<std::string, std::size_t>;

Dictionary dictionary_of(const Corpus& corpus, std::size_t shards) {
	Dictionary dictionary;
	for (std::size_t s = 0; s < shards; ++s) {
		for (const auto& spec : corpus.specs[s]) {
			if (!spec.live) { continue; }
			for (const auto& [term, wdf] : spec.terms) { ++dictionary[term]; }
			++dictionary["B" + spec.group];
		}
	}
	return dictionary;
}

std::vector<char32_t> code_points(const std::string& text) {
	std::vector<char32_t> out;
	for (std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t n = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
		char32_t cp = n == 1 ? lead : lead & (0xff >> (n + 1));
		for (std::size_t k = 1; k < n; ++k) { cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3f); }
		out.push_back(cp);
		i += n;
	}
	return out;
}

// Whether the term matches a pattern whose '?' (one character) and '*' (any run) count as the flags say, whole.
bool glob(const std::vector<char32_t>& pattern, std::size_t p, const std::vector<char32_t>& term, std::size_t t, unsigned flags) {
	if (p == pattern.size()) { return t == term.size(); }
	if (pattern[p] == U'*' && (flags & engine::PATTERN_MULTI)) {
		for (std::size_t skip = t; skip <= term.size(); ++skip) {
			if (glob(pattern, p + 1, term, skip, flags)) { return true; }
		}
		return false;
	}
	if (t == term.size()) { return false; }
	if (pattern[p] == U'?' && (flags & engine::PATTERN_SINGLE)) { return glob(pattern, p + 1, term, t + 1, flags); }
	return pattern[p] == term[t] && glob(pattern, p + 1, term, t + 1, flags);
}

// The optimal-string-alignment distance: insertions, deletions, substitutions and adjacent swaps count one.
std::size_t distance_of(const std::vector<char32_t>& a, const std::vector<char32_t>& b) {
	std::vector<std::vector<std::size_t>> d(a.size() + 1, std::vector<std::size_t>(b.size() + 1));
	for (std::size_t i = 0; i <= a.size(); ++i) { d[i][0] = i; }
	for (std::size_t j = 0; j <= b.size(); ++j) { d[0][j] = j; }
	for (std::size_t i = 1; i <= a.size(); ++i) {
		for (std::size_t j = 1; j <= b.size(); ++j) {
			d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
			if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) { d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1); }
		}
	}
	return d[a.size()][b.size()];
}

// The terms a wildcard or an edit-distance node stands for, within its limit; none and `error` set when
// there are more than the limit allows under the `error` policy.
struct Expansion {
	std::set<std::string> terms;
	bool error = false;
};

Expansion expand(const engine::QueryNode& node, const Dictionary& dictionary) {
	std::vector<std::string> found;
	for (const auto& [term, count] : dictionary) {
		bool match;
		if (node.kind == engine::QueryKind::wildcard) {
			match = node.pattern_flags == 0 ? term.starts_with(node.str) : glob(code_points(node.str), 0, code_points(term), 0, node.pattern_flags);
		} else {
			const auto prefix = node.str.substr(0, node.prefix_length);
			match = term.starts_with(prefix) && distance_of(code_points(node.str.substr(prefix.size())), code_points(term.substr(prefix.size()))) <= node.distance;
		}
		if (match) { found.push_back(term); }
	}
	Expansion result;
	if (node.parameter != 0 && found.size() > node.parameter) {
		switch (node.limit) {
			case engine::Expansion::error:
				result.error = true;
				return result;
			case engine::Expansion::first:
				found.resize(node.parameter);
				break;
			case engine::Expansion::most_frequent:
				std::stable_sort(found.begin(), found.end(), [&](const auto& x, const auto& y) { return dictionary.at(x) > dictionary.at(y); });
				found.resize(node.parameter);
				break;
		}
	}
	result.terms.insert(found.begin(), found.end());
	return result;
}

struct Matcher {
	const Dictionary& dictionary;
	const std::string& key;
	const Spec& spec;
	bool errored = false;

	bool value_accepted(const engine::PostingSource& source) const {
		std::string value;
		if (source.slot() == KEY) { value = spec.key; }
		else if (source.slot() == GROUP) { value = spec.group; }
		else if (source.slot() == NUMBER && spec.has_number) { value = spec.number; }
		return !value.empty() && source.accept(value);
	}

	bool operator()(const Query& query) {
		const auto* node = query.node();
		if (node == nullptr) { return false; }
		switch (node->kind) {
			case engine::QueryKind::term:
				if (node->field.empty() && node->str.empty()) { return true; }
				if (node->field == engine::KEY_FIELD) { return node->str == key; }
				return node->field == "f" && has_term(spec, node->str);
			case engine::QueryKind::wildcard:
			case engine::QueryKind::edit_distance: {
				const auto expansion = expand(*node, dictionary);
				errored = errored || expansion.error;
				return std::any_of(expansion.terms.begin(), expansion.terms.end(), [&](const auto& term) { return has_term(spec, term); });
			}
			case engine::QueryKind::source:
				return value_accepted(*node->src);
			case engine::QueryKind::scale:
				return (*this)(node->subqueries[0]);
			case engine::QueryKind::append:
				if (node->subqueries[0].empty()) { return (*this)(node->subqueries[1]); }
				return combine(node->op, node->subqueries);
			case engine::QueryKind::combine:
				return combine(node->op, node->subqueries);
		}
		return false;
	}

	bool combine(Op op, const std::vector<Query>& subs) {
		std::vector<bool> hit;
		for (const auto& sub : subs) { hit.push_back((*this)(sub)); }
		const auto count = static_cast<std::size_t>(std::count(hit.begin(), hit.end(), true));
		switch (op) {
			case Op::AND: return count == hit.size();
			case Op::OR:
			case Op::MAX:
			case Op::SYNONYM: return count != 0;
			case Op::AND_NOT: return hit[0] && count == 1;
			case Op::AND_MAYBE: return hit[0];
			case Op::FILTER: return hit[0] && hit[1];
			case Op::XOR: return count % 2 == 1;
			default: return false;
		}
	}
};

// The stored data of the live documents of the first `shards` shards that the query matches; `error` when an
// expansion past its limit under the `error` policy fails the search.
struct Expected {
	std::set<std::string> datas;
	bool error = false;
};

Expected expected_of(const Corpus& corpus, std::size_t shards, const Query& query) {
	const auto dictionary = dictionary_of(corpus, shards);
	Expected expected;
	for (std::size_t s = 0; s < shards; ++s) {
		for (std::size_t i = 0; i < corpus.specs[s].size(); ++i) {
			const auto& spec = corpus.specs[s][i];
			if (!spec.live) { continue; }
			const auto key = key_of(static_cast<int>(s), static_cast<int>(i + 1));
			Matcher matcher{dictionary, key, spec};
			if (matcher(query)) { expected.datas.insert(spec.data); }
			expected.error = expected.error || matcher.errored;
		}
	}
	return expected;
}

// --- running and comparing ---

using Tweak = std::function<void(engine::SearchRequest&)>;

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, const Tweak& tweak = [](engine::SearchRequest&) { }) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100000;
	request.check_at_least = 100000;
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

// The stored data of a document of the search over `shards` shards of the index.
std::string data_of(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	const auto local = static_cast<engine::docid>((did - 1) / shards + 1);
	return engine::sync_wait(index.owned[shard]->get(local))->data();
}

// The documents' weights by stored data, for comparing indexes that number their documents differently.
std::map<std::string, double> weights_by_data(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::map<std::string, double> weights;
	for (const auto& hit : result.hits) {
		weights[data_of(index, shards, hit.did)] = hit.weight;
	}
	return weights;
}

std::set<std::string> datas_of(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::set<std::string> datas;
	for (const auto& [data, weight] : weights_by_data(index, shards, result)) { datas.insert(data); }
	return datas;
}

// The ranks and the order of the hits of a full search: weight order, ranks counting from zero.
void check_ranking(const engine::SearchResult& result) {
	for (std::size_t i = 0; i < result.hits.size(); ++i) {
		CHECK_EQ(result.hits[i].rank, i);
		if (i > 0) { CHECK_GE(result.hits[i - 1].weight + 1e-12, result.hits[i].weight); }
	}
}

// A search checked against the model: the documents and the estimate are the model's, the ranking is by
// weight, and (when `weights`) the weights are those of one shard holding every document. With a window
// the hits are that slice of the full ranking.
engine::SearchResult both(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak = [](engine::SearchRequest&) { }, bool weights = true) {
	const auto want = expected_of(corpus, shards, query);
	const auto full = run(corpus.strata, shards, query);
	const bool ok = datas_of(corpus.strata, shards, full) == want.datas && full.estimated == want.datas.size() && full.hits.size() == want.datas.size();
	if (!ok) {
		std::fprintf(stderr, "  differs: %s (%zu shards): %zu hits est %u, model %zu\n", what, shards, full.hits.size(), full.estimated, want.datas.size());
	}
	CHECK_TRUE(ok);
	check_ranking(full);
	if (weights && shards == corpus.specs.size()) {
		const auto whole = run(corpus.merged, 1, query);
		const auto mine = weights_by_data(corpus.strata, shards, full), theirs = weights_by_data(corpus.merged, 1, whole);
		bool same = mine.size() == theirs.size();
		for (const auto& [data, weight] : mine) {
			const auto it = theirs.find(data);
			same = same && it != theirs.end() && close(weight, it->second);
		}
		if (!same) { std::fprintf(stderr, "  weights differ from the whole index's: %s (%zu shards)\n", what, shards); }
		CHECK_TRUE(same);
	}
	engine::SearchRequest window;
	window.maxitems = 100000;   // as run() starts every request
	tweak(window);
	const auto windowed = run(corpus.strata, shards, query, tweak);
	CHECK_EQ(windowed.estimated, full.estimated);
	const std::size_t begin = std::min<std::size_t>(window.first, full.hits.size());
	const std::size_t end = std::min<std::size_t>(begin + window.maxitems, full.hits.size());
	CHECK_EQ(windowed.hits.size(), end - begin);
	if (windowed.hits.size() != end - begin) { return full; }
	for (std::size_t i = begin; i < end; ++i) {
		CHECK_EQ(windowed.hits[i - begin].did, full.hits[i].did);
		CHECK_EQ(windowed.hits[i - begin].rank, i);
	}
	return full;
}

// Like both(), but for searches whose terms weigh with statistics that are not the whole index's on
// several shards (a wildcard or edit-distance expansion): the documents and the estimate are the model's,
// and the weights those of the search of one shard holding every document.
void same_documents(const Corpus& corpus, std::size_t shards, const Query& query, const char* what) {
	const auto want = expected_of(corpus, shards, query);
	if (want.error) {
		CHECK_THROW(run(corpus.strata, shards, query), engine::Error);
		return;
	}
	const auto s = run(corpus.strata, shards, query);
	const auto m = run(corpus.merged, 1, query);
	bool ok = datas_of(corpus.strata, shards, s) == want.datas && s.estimated == want.datas.size() && m.estimated == s.estimated;
	check_ranking(s);
	const auto sw = weights_by_data(corpus.strata, shards, s);
	const auto mw = weights_by_data(corpus.merged, 1, m);
	ok = ok && sw.size() == mw.size();
	if (shards == 1) {
		for (const auto& [data, w] : sw) {
			const auto it = mw.find(data);
			ok = ok && it != mw.end() && close(w, it->second);
		}
	}
	if (!ok) {
		std::fprintf(stderr, "  differs: %s (%zu shards)\n", what, shards);
	}
	CHECK_TRUE(ok);
}

// The kind of the engine error the call raises; none when it raises nothing.
template <typename F>
std::optional<engine::errc> error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return std::nullopt;
}

// Whether the call raises a T, not translated to an engine error.
template <typename T, typename F>
bool throws_as(F&& f) {
	try {
		f();
	} catch (const T&) {
		return true;
	} catch (...) {
	}
	return false;
}

Query t(const char* term, engine::termcount wqf = 1) {
	return Query::term("f", term, wqf);
}

// The key of the document with this id in the search over `shards` shards.
std::string key_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(KEY);
}

std::string group_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(GROUP);
}

// --- tests ---

void test_sorter() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("sorter", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		SlotKey by_key(KEY), by_group(GROUP);
		const std::vector<std::pair<const char*, Query>> queries = {
			{"term", t("common")},
			{"or", Query::combine(Op::OR, t("apple"), t("band"))},
			{"and", Query::combine(Op::AND, t("common"), t("even"))},
			{"match_all", Query::match_all()},
			{"none", t("absent")},
		};
		for (const auto& [name, query] : queries) {
			const auto want = expected_of(corpus, n, query);
			// The documents' keys, in the order a sort by key gives: unique keys, so the order is total.
			std::vector<std::string> keys;
			for (std::size_t s = 0; s < n; ++s) {
				for (const auto& spec : corpus.specs[s]) {
					if (spec.live && want.datas.contains(spec.data)) { keys.push_back(spec.key); }
				}
			}
			std::sort(keys.begin(), keys.end());
			for (const auto order : {engine::DocidOrder::ascending, engine::DocidOrder::descending}) {
				const auto sorted = run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.sorter = &by_key; r.order = order; });
				CHECK_EQ(sorted.estimated, want.datas.size());
				REQUIRE_EQ(sorted.hits.size(), keys.size());
				for (std::size_t i = 0; i < keys.size(); ++i) {
					CHECK_EQ(key_value(corpus.strata, n, sorted.hits[i].did), keys[i]);
					CHECK_EQ(sorted.hits[i].rank, i);
				}
				// A window is that slice of the order.
				const auto window = run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.sorter = &by_key; r.order = order; r.first = 3; r.maxitems = 7; });
				CHECK_EQ(window.estimated, want.datas.size());
				const std::size_t end = std::min<std::size_t>(10, keys.size());
				const std::size_t begin = std::min<std::size_t>(3, keys.size());
				REQUIRE_EQ(window.hits.size(), end - begin);
				for (std::size_t i = begin; i < end; ++i) {
					CHECK_EQ(window.hits[i - begin].did, sorted.hits[i].did);
				}
				// Shared keys: ties fall back to relevance, then to the document order.
				const auto grouped = run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.sorter = &by_group; r.order = order; });
				CHECK_EQ(grouped.hits.size(), keys.size());
				CHECK_TRUE(datas_of(corpus.strata, n, grouped) == want.datas);
				for (std::size_t i = 1; i < grouped.hits.size(); ++i) {
					const auto& before = grouped.hits[i - 1];
					const auto& hit = grouped.hits[i];
					const auto g0 = group_value(corpus.strata, n, before.did), g1 = group_value(corpus.strata, n, hit.did);
					CHECK_TRUE(g0 <= g1);
					if (g0 == g1) {
						CHECK_GE(before.weight + 1e-12, hit.weight);
						if (close(before.weight, hit.weight)) {
							CHECK_TRUE(order == engine::DocidOrder::descending ? before.did > hit.did : before.did < hit.did);
						}
					}
				}
			}
			const auto r = run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.sorter = &by_key; });
			CHECK_EQ(r.estimated, run(corpus.strata, n, query).estimated);
			CHECK_EQ(r.estimated, want.datas.size());
			(void)name;
		}
		// A key maker that throws fails the search, and the next search is fine.
		ThrowingKey throwing;
		CHECK_TRUE(throws_as<ClientLike>([&] { run(corpus.strata, n, t("common"), [&](engine::SearchRequest& q) { q.sorter = &throwing; }); }));
		CHECK_EQ(run(corpus.strata, n, t("common")).hits.size(), static_cast<std::size_t>(shard_count * 60));
	}
}

void test_spy() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("spy", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const std::vector<std::pair<const char*, Query>> queries = {
			{"term", t("common")},
			{"or", Query::combine(Op::OR, t("apple"), t("band"))},
			{"and", Query::combine(Op::AND, t("common"), t("even"))},
			{"match_all", Query::match_all()},
			{"none", t("absent")},
		};
		for (const auto& [name, query] : queries) {
			// What a spy must have seen: the model's matches, grouped, and their weights as the full search gives them.
			const auto want = expected_of(corpus, n, query);
			std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> groups;
			for (std::size_t s = 0; s < n; ++s) {
				for (const auto& spec : corpus.specs[s]) {
					if (!spec.live || !want.datas.contains(spec.data)) { continue; }
					auto& entry = groups[spec.group];
					++entry.first;
					entry.second += spec.has_number ? std::stoull(spec.number) : 0;
				}
			}
			const auto full = run(corpus.strata, n, query);
			double total = 0;
			for (const auto& hit : full.hits) { total += hit.weight; }
			// The window and the checked count do not limit what the spy sees: it sees every match.
			for (const auto& [first, items, check] : {std::tuple{0u, 10u, 0u}, {0u, 0u, 1u}, {3u, 5u, 100000u}, {0u, 100000u, 0u}}) {
				GroupSpy s;
				const auto sr = run(corpus.strata, n, query, [&, first = first, items = items, check = check](engine::SearchRequest& r) { r.first = first; r.maxitems = items; r.check_at_least = check; r.spy = &s; });
				const bool ok = s.datas == want.datas && s.groups == groups && close(s.weights, total) && sr.estimated == want.datas.size();
				if (!ok) {
					std::fprintf(stderr, "  spy differs: %s (%zu shards) first %u items %u: model %zu, strata %zu\n", name, n, first, items, want.datas.size(), s.datas.size());
				}
				CHECK_TRUE(ok);
				CHECK_EQ(s.datas.size(), sr.estimated);
			}
			// A spy together with a key maker.
			GroupSpy s;
			SlotKey key(KEY);
			run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.spy = &s; r.sorter = &key; });
			CHECK_TRUE(s.datas == want.datas);
			CHECK_TRUE(s.groups == groups);
		}
		// A spy is given the document's id within its shard.
		{
			struct IdSpy final : engine::MatchSpy {
				std::multiset<engine::docid> ids;
				void see(const engine::DocView& doc, double) override { ids.insert(doc.did()); }
				std::unique_ptr<engine::MatchSpy> clone() const override { return nullptr; }
				std::string name() const override { return "IdSpy"; }
				std::string serialise() const override { return ""; }
				std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return nullptr; }
				std::string serialise_results() const override { return ""; }
				void merge_results(std::string_view) override { }
				std::string description() const override { return "IdSpy"; }
			} from_strata;
			run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &from_strata; });
			CHECK_EQ(from_strata.ids.size(), static_cast<std::size_t>(shard_count * 60));
			CHECK_EQ(from_strata.ids.count(1), static_cast<std::size_t>(shard_count));
			CHECK_EQ(from_strata.ids.count(60), static_cast<std::size_t>(shard_count));
			CHECK_EQ(from_strata.ids.count(61), std::size_t(0));
		}
		// A spy that asks for the document's serialisation is refused: the strata engine keeps no term lists.
		{
			struct TermsSpy final : engine::MatchSpy {
				void see(const engine::DocView& doc, double) override {
					(void)doc.serialise();
				}
				std::unique_ptr<engine::MatchSpy> clone() const override { return nullptr; }
				std::string name() const override { return "TermsSpy"; }
				std::string serialise() const override { return ""; }
				std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return nullptr; }
				std::string serialise_results() const override { return ""; }
				void merge_results(std::string_view) override { }
				std::string description() const override { return "TermsSpy"; }
			} asking;
			CHECK_TRUE(error_kind([&] { run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &asking; }); }) == engine::errc::syntax);
		}
		// A spy that throws fails the search; the next one is unaffected.
		GroupSpy throwing;
		throwing.throw_on = true;
		CHECK_TRUE(throws_as<ClientLike>([&] { run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &throwing; }); }));
		// A scan sink that throws leaves as thrown too.
		CHECK_TRUE(throws_as<ClientLike>([&] { engine::sync_wait(corpus.strata.owned[0]->scan(0, [](const engine::DocView&) { throw ClientLike("sink"); })); }));
		GroupSpy fine;
		CHECK_EQ(run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &fine; }).estimated, static_cast<unsigned>(shard_count * 60));
		CHECK_EQ(fine.datas.size(), static_cast<std::size_t>(shard_count * 60));
	}
}

void test_sources() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("sources", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		for (const double max_weight : {0.0, 4.0}) {
			const auto between = [&](const char* low, const char* high, engine::valueno slot = NUMBER) {
				return Query::source(std::make_unique<Between>(slot, low, high, max_weight));
			};
			const std::vector<std::pair<const char*, Query>> queries = {
				{"source", between("00100", "00600")},
				{"all", between("00000", "99999")},
				{"none", between("90000", "99999")},
				{"empty slot", between("a", "z", EMPTY)},
				{"group", between("g1", "g2", GROUP)},
				{"and", Query::combine(Op::AND, between("00100", "00600"), t("common"))},
				{"or", Query::combine(Op::OR, between("00100", "00300"), t("band"))},
				{"filter", Query::combine(Op::FILTER, t("common"), between("00100", "00600"))},
				{"and_not", Query::combine(Op::AND_NOT, t("common"), between("00100", "00600"))},
				{"and_maybe", Query::combine(Op::AND_MAYBE, t("common"), between("00100", "00600"))},
				{"two sources", Query::combine(Op::AND, between("00100", "00600"), between("g1", "g2", GROUP))},
				{"scaled", Query::scale(between("00100", "00600"), 2.5)},
			};
			for (const auto& [name, query] : queries) {
				both(corpus, n, query, name);
				both(corpus, n, query, name, [](engine::SearchRequest& r) { r.first = 4; r.maxitems = 9; });
			}
		}
		// Deleted and replaced documents leave no trace in a source.
		for (int shard = 0; shard < shard_count; ++shard) {
			auto& s = *corpus.strata.owned[static_cast<std::size_t>(shard)];
			auto& specs = corpus.specs[static_cast<std::size_t>(shard)];
			engine::sync_wait(s.erase_by_key(key_of(shard, 5), true, std::nullopt));
			specs[4].live = false;
			specs[5].number = "00250";
			specs[5].has_number = true;
			auto replacement = make(shard, 6, specs[5]);
			engine::sync_wait(s.replace_by_key(key_of(shard, 6), replacement, {}));
			engine::sync_wait(s.commit(engine::Transaction::none));
		}
		// Term statistics count deleted documents until a merge removes them, so only the documents and the
		// estimate are compared here.
		both(corpus, n, Query::source(std::make_unique<Between>(NUMBER, "00100", "00600")), "after writes", [](engine::SearchRequest&) { }, false);
		both(corpus, n, Query::combine(Op::OR, Query::source(std::make_unique<Between>(NUMBER, "00200", "00300")), t("even")), "after writes, with a term", [](engine::SearchRequest&) { }, false);
	}
}

// Terms that carry no frequency (a schema's keyword and number fields) are searched like any other.
void test_boolean_terms() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("boolean", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const std::vector<std::pair<const char*, Query>> queries = {
			{"boolean", t("Bg1")},
			{"boolean absent", t("Bg9")},
			{"and", Query::combine(Op::AND, t("Bg1"), t("common"))},
			{"or", Query::combine(Op::OR, t("Bg1"), t("Bg2"))},
			{"filter", Query::combine(Op::FILTER, t("apple"), t("Bg0"))},
			{"and_not", Query::combine(Op::AND_NOT, t("common"), t("Bg3"))},
			{"id", Query::term(std::string(engine::KEY_FIELD), "Q0_5")},
			{"wildcard", Query::wildcard("f", "Bg", 0, engine::Expansion::error, 0, Op::OR)},
		};
		for (const auto& [name, query] : queries) {
			const auto s = both(corpus, n, query, name);
			if (std::string(name) == "boolean") {
				CHECK_GT(s.hits.size(), 0u);
			}
		}
		CHECK_EQ(run(corpus.strata, n, Query::term(std::string(engine::KEY_FIELD), "Q0_5")).hits.size(), 1u);
		CHECK_EQ(run(corpus.strata, n, t("Q0_5")).hits.size(), 0u);  // the key is not a term of the field
	}
}

// Shards of one document each, with no commit between the writes and the search: every term has one posting.
void test_single_documents() {
	Corpus corpus;
	build("single", 5, 1, 1, corpus);
	for (const std::size_t n : {std::size_t{1}, std::size_t{5}}) {
		for (const auto& [name, query] : std::vector<std::pair<const char*, Query>>{{"common", t("common")}, {"boolean", t("Bg1")}, {"or", Query::combine(Op::OR, t("common"), t("Bg1"))}, {"all", Query::match_all()}}) {
			const auto s = both(corpus, n, query, name);
			if (std::string(name) == "common") {
				CHECK_EQ(s.hits.size(), n);
			}
		}
	}
}

// An append onto a query that is empty by its structure (a text that analyses to nothing) is the next query.
void test_appends() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("appends", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const Query nothing = Query::combine(Op::AND, t("apple"), Query());
		CHECK_TRUE(nothing.empty());
		const std::vector<std::pair<const char*, Query>> queries = {
			{"append to nothing", Query::append(Op::AND, nothing, t("Bg1"))},
			{"twice", Query::append(Op::AND, Query::append(Op::AND, nothing, t("Bg1")), t("common"))},
			{"or", Query::append(Op::OR, Query::append(Op::AND, nothing, t("band")), t("apple"))},
			{"after a missing term", Query::append(Op::OR, t("absent"), t("apple"))},
			{"and with a missing term", Query::append(Op::AND, t("absent"), t("apple"))},
			{"nothing on nothing", Query::append(Op::AND, nothing, nothing)},
		};
		for (const auto& [name, query] : queries) {
			both(corpus, n, query, name);
		}
		CHECK_GT(run(corpus.strata, n, queries[0].second).hits.size(), 0u);
	}
}

void test_operators() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("operators", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const auto syn = [](std::vector<Query> terms) { return Query::combine(Op::SYNONYM, std::move(terms)); };
		const std::vector<std::pair<const char*, Query>> queries = {
			{"synonym", syn({t("apple"), t("apricot")})},
			{"synonym three", syn({t("band"), t("bandana"), t("bond")})},
			{"synonym one", syn({t("band")})},
			{"synonym absent", syn({t("band"), t("absent")})},
			{"synonym all absent", syn({t("absent"), t("nothing")})},
			{"synonym repeated", syn({t("band"), t("band")})},
			{"synonym and", Query::combine(Op::AND, syn({t("apple"), t("cat")}), t("common"))},
			{"synonym or", Query::combine(Op::OR, syn({t("apple"), t("cat")}), t("even"))},
			{"xor", Query::combine(Op::XOR, {t("apple"), t("band"), t("cat")})},
			{"xor two", Query::combine(Op::XOR, t("apple"), t("band"))},
			{"xor nested", Query::combine(Op::XOR, Query::combine(Op::XOR, t("apple"), t("band")), t("cat"))},
			{"xor of and", Query::combine(Op::XOR, Query::combine(Op::AND, t("apple"), t("even")), t("band"))},
			{"xor absent", Query::combine(Op::XOR, t("apple"), t("absent"))},
			{"xor appended", Query::append(Op::XOR, t("apple"), t("band"))},
			{"max", Query::combine(Op::MAX, {t("apple"), t("band"), t("cat")})},
			{"max two", Query::combine(Op::MAX, t("apple"), t("band"))},
			{"max wqf", Query::combine(Op::MAX, t("apple", 3), t("band"))},
			{"max of or", Query::combine(Op::MAX, Query::combine(Op::OR, t("apple"), t("cat")), t("even"))},
			{"xor of or", Query::combine(Op::XOR, Query::combine(Op::OR, t("apple"), t("cat")), t("even"))},
			{"max of nested or", Query::combine(Op::MAX, Query::combine(Op::OR, Query::combine(Op::OR, t("apple"), t("cat")), t("coat")), t("even"))},
			{"max of or of and", Query::combine(Op::MAX, Query::combine(Op::OR, Query::combine(Op::AND, t("apple"), t("common")), t("cat")), t("even"))},
			{"max of max", Query::combine(Op::MAX, Query::combine(Op::MAX, t("apple"), t("cat")), t("even"))},
			{"and of or in max", Query::combine(Op::AND, Query::combine(Op::MAX, Query::combine(Op::OR, t("apple"), t("cat")), t("even")), t("common"))},
			{"max of and", Query::combine(Op::MAX, Query::combine(Op::AND, t("common"), t("even")), t("band"))},
			{"max of scale", Query::combine(Op::MAX, Query::scale(t("apple"), 2.0), t("band"))},
			{"max of synonym", Query::combine(Op::MAX, syn({t("apple"), t("cat")}), t("band"))},
			{"max appended", Query::append(Op::MAX, t("apple"), t("band"))},
			{"scale", Query::scale(Query::combine(Op::OR, t("apple"), t("band")), 2.5)},
			{"scale zero", Query::scale(t("apple"), 0.0)},
			{"scale synonym", Query::scale(syn({t("apple"), t("cat")}), 3.0)},
			{"and_maybe", Query::combine(Op::AND_MAYBE, t("common"), syn({t("apple"), t("cat")}))},
			{"filter", Query::combine(Op::FILTER, syn({t("apple"), t("cat")}), t("even"))},
		};
		for (const auto& [name, query] : queries) {
			both(corpus, n, query, name);
			both(corpus, n, query, name, [](engine::SearchRequest& r) { r.first = 2; r.maxitems = 6; });
		}
	}
}

// What a wildcard or edit-distance node stands for is the terms the model says, whatever the pattern.
void test_expansions() {
	Corpus corpus;
	for (const int shard_count : {1, 5}) {
		build("expansions", shard_count, 60, 3, corpus);
		const std::size_t n = static_cast<std::size_t>(shard_count);
		const auto wild = [](const char* pattern, engine::termcount max = 0, engine::Expansion limit = engine::Expansion::error, unsigned flags = 0, Op combiner = Op::SYNONYM) {
			return Query::wildcard("f", pattern, max, limit, flags, combiner);
		};
		const auto fuzzy = [](const char* pattern, unsigned distance, std::size_t prefix = 0, engine::termcount max = 0, engine::Expansion limit = engine::Expansion::error, Op combiner = Op::SYNONYM) {
			return Query::edit_distance("f", pattern, max, limit, combiner, distance, prefix);
		};
		const unsigned S = engine::PATTERN_SINGLE, M = engine::PATTERN_MULTI;
		std::vector<std::pair<std::string, Query>> queries;
		for (const Op combiner : {Op::SYNONYM, Op::OR, Op::MAX}) {
			const std::string c = std::string(" ") + engine::to_string(combiner);
			queries.emplace_back("prefix ban" + c, wild("ban", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("prefix ca" + c, wild("ca", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("prefix caf" + c, wild("caf", 0, engine::Expansion::error, 0, combiner));
			queries.emplace_back("single b?nd" + c, wild("b?nd", 0, engine::Expansion::error, S, combiner));
			queries.emplace_back("multi *an*" + c, wild("*an*", 0, engine::Expansion::error, M, combiner));
			queries.emplace_back("multi ca*t" + c, wild("ca*t", 0, engine::Expansion::error, M, combiner));
			queries.emplace_back("both ?a*" + c, wild("?a*", 0, engine::Expansion::error, S | M, combiner));
			queries.emplace_back("fuzzy bnad 1" + c, fuzzy("bnad", 1, 0, 0, engine::Expansion::error, combiner));
			queries.emplace_back("fuzzy aple 2" + c, fuzzy("aple", 2, 0, 0, engine::Expansion::error, combiner));
			queries.emplace_back("fuzzy cat 1 prefix 1" + c, fuzzy("cat", 1, 1, 0, engine::Expansion::error, combiner));
		}
		queries.emplace_back("none", wild("zzz"));
		queries.emplace_back("single unicode caf?", wild("caf?", 0, engine::Expansion::error, S));
		queries.emplace_back("single unicode na?ve", wild("na?ve", 0, engine::Expansion::error, S));
		queries.emplace_back("fuzzy cafe 1", fuzzy("cafe", 1));
		queries.emplace_back("fuzzy naive 1", fuzzy("naive", 1));
		queries.emplace_back("fuzzy band 3", fuzzy("band", 3));
		queries.emplace_back("limit first", wild("ban", 2, engine::Expansion::first));
		queries.emplace_back("limit most frequent", wild("mf_", 2, engine::Expansion::most_frequent));
		queries.emplace_back("limit most frequent, 3", wild("mf_", 3, engine::Expansion::most_frequent, 0, Op::OR));
		queries.emplace_back("limit first, mf", wild("mf_", 2, engine::Expansion::first));
		queries.emplace_back("limit unmet", wild("ban", 10, engine::Expansion::error));
		queries.emplace_back("fuzzy limit first", fuzzy("bnad", 2, 0, 2, engine::Expansion::first));
		queries.emplace_back("wildcard and", Query::combine(Op::AND, wild("ban"), t("common")));
		queries.emplace_back("wildcard or fuzzy", Query::combine(Op::OR, wild("ap"), fuzzy("cart", 1)));
		std::size_t found = 0;
		for (const auto& [name, query] : queries) {
			same_documents(corpus, n, query, name.c_str());
			found += !expected_of(corpus, n, query).datas.empty();
		}
		CHECK_GT(found, queries.size() / 2);
		// The limits pick the terms the corpus gives them: the first two prefixed "mf_" by byte order, the two with the most documents.
		CHECK_EQ(expected_of(corpus, n, wild("mf_", 2, engine::Expansion::first)).datas.size(), 5u);   // mf_a (docs 1-5) and mf_b (1-3)
		CHECK_EQ(expected_of(corpus, n, wild("mf_", 2, engine::Expansion::most_frequent)).datas.size(), 8u);   // mf_c (docs 1-8) and mf_a (1-5)
		// Past the limit with `error` fails the search.
		CHECK_TRUE(expected_of(corpus, n, wild("ban", 2)).error);
		CHECK_TRUE(error_kind([&] { run(corpus.strata, n, wild("ban", 2)); }).has_value());
		CHECK_TRUE(expected_of(corpus, n, fuzzy("bnad", 2, 0, 1)).error);
		CHECK_TRUE(error_kind([&] { run(corpus.strata, n, fuzzy("bnad", 2, 0, 1)); }).has_value());
	}
}

}  // namespace

static void test_sorter_() { test_sorter(); }
static void test_spy_() { test_spy(); }
static void test_sources_() { test_sources(); }
static void test_operators_() { test_operators(); }
static void test_expansions_() { test_expansions(); }

int main() {
	const struct { const char* name; void (*run)(); } tests[] = {
		{"sorter", test_sorter_}, {"boolean", test_boolean_terms}, {"single", test_single_documents}, {"appends", test_appends},
		{"spy", test_spy_}, {"sources", test_sources_}, {"operators", test_operators_}, {"expansions", test_expansions_},
	};
	const char* only = std::getenv("ONLY");
	for (const auto& test : tests) {
		if (only == nullptr || std::string(test.name) == only) {
			test.run();
		}
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
