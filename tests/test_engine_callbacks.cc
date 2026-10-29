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

// Callbacks and operators on both engines. One corpus is written to a Xapian and a strata shard set and
// every search runs on both through the seam: the documents found, their order, the estimate, the
// weights (within a stated tolerance), what a spy accumulated and what a key maker ordered must agree,
// on one shard and on five.

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
#include "engine/kind.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace {

using engine::Kind;
using engine::Op;
using engine::Query;

constexpr double tolerance = 1e-9;

// Value slots: the Xapian engine reserves the first twenty for its own bookkeeping.
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
	std::set<std::string> datas;   // by stored data: a spy of the Xapian engine is given the id within its shard
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
	Kind kind;
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

std::string path_of(const char* name, Kind kind, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_callbacks_") + name + "_" + engine::to_string(kind) + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

struct Spec {
	std::map<std::string, unsigned> terms;
	std::string key, group, number, data;
	bool has_number = true;
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

engine::Document make(Kind kind, int shard, int i, const Spec& spec) {
	engine::Document doc(kind);
	doc.set_key("Q" + std::to_string(shard) + "_" + std::to_string(i));
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
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Index merged{Kind::strata, {}};   // every document of every shard in one shard: the whole-index statistics
	std::vector<std::vector<Spec>> specs;
};

// The same documents on both engines, `commits` commits per shard (several segments on strata).
void build(const char* name, int shard_count, int count, int commits, Corpus& corpus) {
	std::mt19937 rng(4321);
	corpus.specs.assign(static_cast<std::size_t>(shard_count), {});
	for (auto* index : {&corpus.xapian, &corpus.strata}) {
		index->owned.clear();
		for (int s = 0; s < shard_count; ++s) {
			index->owned.push_back(engine::sync_wait(engine::open_local(index->kind, path_of(name, index->kind, s), true, true)).shard);
		}
	}
	corpus.merged.owned.clear();
	corpus.merged.owned.push_back(engine::sync_wait(engine::open_local(Kind::strata, path_of(name, Kind::strata, 99), true, true)).shard);
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
				for (auto* index : {&corpus.xapian, &corpus.strata, &corpus.merged}) {
					auto doc = make(index->kind, s, i, list.back());
					engine::sync_wait(index->owned[index == &corpus.merged ? 0 : static_cast<std::size_t>(s)]->add(doc, {}));
				}
			}
			for (auto* index : {&corpus.xapian, &corpus.strata}) {
				engine::sync_wait(index->owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
			}
		}
	}
	engine::sync_wait(corpus.merged.owned[0]->commit(engine::Transaction::none));
}

// --- running and comparing ---

using Tweak = std::function<void(engine::SearchRequest&)>;

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, const Tweak& tweak = [](engine::SearchRequest&) { }) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100000;
	request.check_at_least = 100000;
	request.origin = "test";
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

// Hits in the same order up to ties: the weights agree position by position, and the ids of each run of
// equal weights agree as sets.
bool same_ranking(const std::vector<engine::Hit>& a, const std::vector<engine::Hit>& b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (std::size_t i = 0; i < a.size();) {
		std::size_t j = i;
		std::set<engine::docid> left, right;
		while (j < a.size() && close(a[j].weight, a[i].weight)) {
			if (!close(a[j].weight, b[j].weight)) {
				return false;
			}
			left.insert(a[j].did);
			right.insert(b[j].did);
			++j;
		}
		if (j == i || left != right) {
			return false;
		}
		i = j;
	}
	return true;
}

std::set<engine::docid> ids_of(const engine::SearchResult& result) {
	std::set<engine::docid> ids;
	for (const auto& hit : result.hits) {
		ids.insert(hit.did);
	}
	return ids;
}

// The same search on both engines: same documents, same estimate, the same ranking up to ties.
engine::SearchResult both(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak = [](engine::SearchRequest&) { }) {
	const auto x = run(corpus.xapian, shards, query, tweak);
	const auto s = run(corpus.strata, shards, query, tweak);
	const bool ok = ids_of(x) == ids_of(s) && x.estimated == s.estimated && x.hits.size() == s.hits.size() && same_ranking(x.hits, s.hits);
	if (!ok) {
		std::fprintf(stderr, "  differs: %s (%zu shards): xapian %zu hits est %u, strata %zu hits est %u, same ids %d\n", what, shards, x.hits.size(), x.estimated, s.hits.size(), s.estimated, ids_of(x) == ids_of(s));
		std::map<engine::docid, double> xw;
		for (const auto& h : x.hits) { xw[h.did] = h.weight; }
		int shown = 0;
		for (const auto& h : s.hits) {
			const auto it = xw.find(h.did);
			if (it != xw.end() && !close(it->second, h.weight) && shown++ < 3) {
				std::fprintf(stderr, "    doc %u: xapian %.12g strata %.12g\n", h.did, it->second, h.weight);
			}
		}
	}
	CHECK_TRUE(ok);
	return s;
}

// The documents' weights by stored data, for comparing indexes that number their documents differently.
std::map<std::string, double> weights_by_data(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::map<std::string, double> weights;
	for (const auto& hit : result.hits) {
		const std::size_t shard = (hit.did - 1) % shards;
		const auto local = static_cast<engine::docid>((hit.did - 1) / shards + 1);
		weights[engine::sync_wait(index.owned[shard]->get(local))->data()] = hit.weight;
	}
	return weights;
}

// Like both(), but for searches whose terms Xapian weighs with statistics that are not the whole index's
// on several shards (a wildcard or edit-distance expansion): there the documents and the estimate must agree
// with Xapian, and the weights with the strata engine's own search of one shard holding every document.
void same_documents(const Corpus& corpus, std::size_t shards, const Query& query, const char* what) {
	if (shards == 1) {
		both(corpus, shards, query, what);
		return;
	}
	const auto x = run(corpus.xapian, shards, query);
	const auto s = run(corpus.strata, shards, query);
	const auto m = run(corpus.merged, 1, query);
	bool ok = ids_of(x).size() == ids_of(s).size() && x.estimated == s.estimated && s.estimated == m.estimated;
	std::set<std::string> xd, sd;
	for (const auto& [data, w] : weights_by_data(corpus.xapian, shards, x)) { xd.insert(data); }
	const auto sw = weights_by_data(corpus.strata, shards, s);
	const auto mw = weights_by_data(corpus.merged, 1, m);
	for (const auto& [data, w] : sw) { sd.insert(data); }
	ok = ok && xd == sd && sw.size() == mw.size();
	for (const auto& [data, w] : sw) {
		const auto it = mw.find(data);
		ok = ok && it != mw.end() && close(w, it->second);
	}
	if (!ok) {
		std::fprintf(stderr, "  differs (whole-index weights): %s (%zu shards)\n", what, shards);
	}
	CHECK_TRUE(ok);
}

// The same search with a fixed order of hits: the documents, in order, agree exactly.
void same_order(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak) {
	const auto x = run(corpus.xapian, shards, query, tweak);
	const auto s = run(corpus.strata, shards, query, tweak);
	bool ok = x.hits.size() == s.hits.size() && x.estimated == s.estimated;
	for (std::size_t i = 0; ok && i < x.hits.size(); ++i) {
		ok = x.hits[i].did == s.hits[i].did && x.hits[i].rank == s.hits[i].rank && close(x.hits[i].weight, s.hits[i].weight);
	}
	if (!ok) {
		std::fprintf(stderr, "  differs in order: %s (%zu shards): xapian %zu hits, strata %zu hits\n", what, shards, x.hits.size(), s.hits.size());
		for (std::size_t i = 0; i < std::min(x.hits.size(), s.hits.size()); ++i) {
			if (x.hits[i].did != s.hits[i].did || !close(x.hits[i].weight, s.hits[i].weight)) {
				const auto key_of = [&](const Index& index, engine::docid did) {
					const std::size_t n = shards;
					return engine::sync_wait(index.owned[(did - 1) % n]->get(static_cast<engine::docid>((did - 1) / n + 1)))->value(KEY);
				};
				std::fprintf(stderr, "    first at %zu: xapian doc %u rank %u w %.9g key %s, strata doc %u rank %u w %.9g key %s\n", i, x.hits[i].did, x.hits[i].rank, x.hits[i].weight, key_of(corpus.xapian, x.hits[i].did).c_str(), s.hits[i].did, s.hits[i].rank, s.hits[i].weight, key_of(corpus.strata, s.hits[i].did).c_str());
				break;
			}
		}
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
			for (const auto order : {engine::DocidOrder::ascending, engine::DocidOrder::descending}) {
				// Unique keys: the order is total.
				same_order(corpus, n, query, name, [&](engine::SearchRequest& r) { r.sorter = &by_key; r.order = order; });
				same_order(corpus, n, query, name, [&](engine::SearchRequest& r) { r.sorter = &by_key; r.order = order; r.first = 3; r.maxitems = 7; });
				// Shared keys: ties fall back to relevance, then to the document order.
				same_order(corpus, n, query, name, [&](engine::SearchRequest& r) { r.sorter = &by_group; r.order = order; });
			}
			const auto r = run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.sorter = &by_key; });
			CHECK_EQ(r.estimated, run(corpus.strata, n, query).estimated);
		}
		// The key order is the keys' order.
		const auto r = run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.sorter = &by_key; });
		REQUIRE_EQ(r.hits.size(), static_cast<std::size_t>(shard_count * 60));
		auto view = [&](engine::docid did) {
			const std::size_t shard = (did - 1) % n, local = (did - 1) / n + 1;
			return engine::sync_wait(corpus.strata.owned[shard]->get(static_cast<engine::docid>(local)))->value(KEY);
		};
		for (std::size_t i = 1; i < r.hits.size(); ++i) {
			CHECK_LT(view(r.hits[i - 1].did), view(r.hits[i].did));
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
			// The window and the checked count do not limit what the spy sees: it sees every match.
			for (const auto& [first, items, check] : {std::tuple{0u, 10u, 0u}, {0u, 0u, 1u}, {3u, 5u, 100000u}, {0u, 100000u, 0u}}) {
				GroupSpy x, s;
				const auto tweak = [&](GroupSpy& spy) {
					return [&, first = first, items = items, check = check](engine::SearchRequest& r) { r.first = first; r.maxitems = items; r.check_at_least = check; r.spy = &spy; };
				};
				// Xapian stops checking at its window unless it is asked to check everything.
				const auto xr = run(corpus.xapian, n, query, [&](engine::SearchRequest& r) { tweak(x)(r); r.check_at_least = engine::CHECK_ALL; });
				const auto sr = run(corpus.strata, n, query, tweak(s));
				const bool ok = x.datas == s.datas && x.groups == s.groups && close(x.weights, s.weights) && sr.estimated == xr.estimated;
				if (!ok) {
					std::fprintf(stderr, "  spy differs: %s (%zu shards) first %u items %u: xapian %zu, strata %zu\n", name, n, first, items, x.datas.size(), s.datas.size());
				}
				CHECK_TRUE(ok);
				CHECK_EQ(s.datas.size(), sr.estimated);
			}
			// A spy together with a key maker.
			GroupSpy x, s;
			SlotKey key(KEY);
			run(corpus.xapian, n, query, [&](engine::SearchRequest& r) { r.spy = &x; r.sorter = &key; });
			run(corpus.strata, n, query, [&](engine::SearchRequest& r) { r.spy = &s; r.sorter = &key; });
			CHECK_TRUE(x.datas == s.datas);
			CHECK_TRUE(x.groups == s.groups);
		}
		// A spy is given the document's id within its shard, as the Xapian engine gives it.
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
			} from_xapian, from_strata;
			run(corpus.xapian, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &from_xapian; r.check_at_least = engine::CHECK_ALL; });
			run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &from_strata; });
			CHECK_TRUE(from_xapian.ids == from_strata.ids);
			CHECK_EQ(from_strata.ids.size(), static_cast<std::size_t>(shard_count * 60));
			CHECK_EQ(from_strata.ids.count(1), static_cast<std::size_t>(shard_count));
		}
		// A spy that asks for the document's terms, term details or serialisation is refused: the strata engine keeps no term lists.
		for (const int ask : {0, 1, 2}) {
			struct TermsSpy final : engine::MatchSpy {
				int ask = 0;
				void see(const engine::DocView& doc, double) override {
					if (ask == 0) { doc.terms("", [](std::string_view, engine::termcount) { }); }
					if (ask == 1) { doc.term_details([](const engine::TermDetail&) { }); }
					if (ask == 2) { (void)doc.serialise(); }
				}
				std::unique_ptr<engine::MatchSpy> clone() const override { return nullptr; }
				std::string name() const override { return "TermsSpy"; }
				std::string serialise() const override { return ""; }
				std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return nullptr; }
				std::string serialise_results() const override { return ""; }
				void merge_results(std::string_view) override { }
				std::string description() const override { return "TermsSpy"; }
			} asking;
			asking.ask = ask;
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
		for (auto* index : {&corpus.xapian, &corpus.strata}) {
			for (int shard = 0; shard < shard_count; ++shard) {
				auto& s = *index->owned[static_cast<std::size_t>(shard)];
				engine::sync_wait(s.erase_by_key("Q" + std::to_string(shard) + "_5", true, std::nullopt));
				auto replacement = make(index->kind, shard, 6, corpus.specs[static_cast<std::size_t>(shard)][5]);
				replacement.add_value(NUMBER, "00250");
				engine::sync_wait(s.replace_by_key("Q" + std::to_string(shard) + "_6", replacement, {}));
				engine::sync_wait(s.commit(engine::Transaction::none));
			}
		}
		// Term statistics count deleted documents until a merge removes them (the strata engine), so only the
		// documents and the estimate are compared here, and the weights of a search by source alone.
		both(corpus, n, Query::source(std::make_unique<Between>(NUMBER, "00100", "00600")), "after writes");
		const auto query = Query::combine(Op::OR, Query::source(std::make_unique<Between>(NUMBER, "00200", "00300")), t("even"));
		const auto x = run(corpus.xapian, n, query);
		const auto s = run(corpus.strata, n, query);
		CHECK_TRUE(ids_of(x) == ids_of(s));
		CHECK_EQ(x.estimated, s.estimated);
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
			{"id", t("Q0_5")},
			{"wildcard", Query::wildcard("f", "Bg", 0, engine::Expansion::error, 0, Op::OR)},
		};
		for (const auto& [name, query] : queries) {
			const auto s = both(corpus, n, query, name);
			if (std::string(name) == "boolean") {
				CHECK_GT(s.hits.size(), 0u);
			}
		}
		CHECK_EQ(run(corpus.strata, n, t("Q0_5")).hits.size(), 1u);
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
			{"elite", Query::combine(Op::ELITE_SET, {t("common"), t("even"), t("band"), t("apple"), t("cat")}, 2)},
			{"elite all", Query::combine(Op::ELITE_SET, {t("band"), t("apple")}, 5)},
			{"elite default", Query::combine(Op::ELITE_SET, {t("band"), t("apple"), t("cat")})},
			{"elite absent", Query::combine(Op::ELITE_SET, {t("absent"), t("band"), t("apple")}, 2)},
			{"elite and", Query::combine(Op::AND, Query::combine(Op::ELITE_SET, {t("band"), t("apple"), t("cat"), t("coat")}, 2), t("common"))},
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

// What a wildcard or edit-distance node stands for is the same terms on both engines, whatever the pattern.
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
		for (const auto& [name, query] : queries) {
			same_documents(corpus, n, query, name.c_str());
		}
		// Past the limit with `error` fails the search on both engines.
		CHECK_TRUE(error_kind([&] { run(corpus.xapian, n, wild("ban", 2)); }).has_value());
		CHECK_TRUE(error_kind([&] { run(corpus.strata, n, wild("ban", 2)); }).has_value());
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
