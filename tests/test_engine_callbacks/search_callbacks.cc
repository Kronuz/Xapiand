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

// Key makers, spies and posting sources in a search, on one shard and on five.

#include "support.h"

namespace detail {

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
				std::string serialize() const override { return ""; }
				std::unique_ptr<engine::MatchSpy> unserialize(std::string_view) const override { return nullptr; }
				std::string serialize_results() const override { return ""; }
				void merge_results(std::string_view) override { }
				std::string description() const override { return "IdSpy"; }
			} from_strata;
			run(corpus.strata, n, t("common"), [&](engine::SearchRequest& r) { r.spy = &from_strata; });
			CHECK_EQ(from_strata.ids.size(), static_cast<std::size_t>(shard_count * 60));
			CHECK_EQ(from_strata.ids.count(1), static_cast<std::size_t>(shard_count));
			CHECK_EQ(from_strata.ids.count(60), static_cast<std::size_t>(shard_count));
			CHECK_EQ(from_strata.ids.count(61), std::size_t(0));
		}
		// A spy that asks for the document's serialization is refused: the strata engine keeps no term lists.
		{
			struct TermsSpy final : engine::MatchSpy {
				void see(const engine::DocView& doc, double) override {
					(void)doc.serialize();
				}
				std::unique_ptr<engine::MatchSpy> clone() const override { return nullptr; }
				std::string name() const override { return "TermsSpy"; }
				std::string serialize() const override { return ""; }
				std::unique_ptr<engine::MatchSpy> unserialize(std::string_view) const override { return nullptr; }
				std::string serialize_results() const override { return ""; }
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

}  // namespace detail
