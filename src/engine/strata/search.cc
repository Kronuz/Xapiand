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


#include "engine/strata/search.h"

#include <algorithm>               // for std::push_heap, std::pop_heap, std::sort_heap, std::remove_if
#include <cmath>                   // for std::nextafter
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <functional>              // for std::less
#include <limits>                  // for std::numeric_limits
#include <memory>                  // for std::make_unique
#include <optional>                // for std::optional
#include <stdexcept>               // for std::invalid_argument, std::exception
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <system_error>            // for std::system_error
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "prospect/index.hh"       // for prospect::index
#include "prospect/operators.hh"   // for prospect::scale_scorer
#include "prospect/searcher.hh"    // for prospect::term_table, live_scorer

#include "engine/doc_view.h"       // for DocView
#include "engine/error.h"          // for Error, errc
#include "engine/strata/pin.h"     // for Pinned
#include "engine/strata/shard.h"   // for Shard


namespace engine::strata {

namespace {

namespace p = ::prospect;

// The prefix that routes a term to the index's key table instead of its dictionary. A query never names a key:
// the id term is also an ordinary term of its document, so every term is looked up in the dictionary. This one
// function is where the lookup depends on how the index keeps keys (strata's design/fields-and-keys).
std::string_view
term_key_prefix()
{
	return {};
}

// A query nests this deep before it is refused, so a hostile one cannot exhaust the stack.
constexpr int max_depth = 1000;

[[noreturn]] void
unsupported(const std::string& what)
{
	throw Error(errc::syntax, "The strata engine does not search " + what + " yet");
}

// Rethrows the exception being handled as an engine error of its kind. Call only inside a catch block.
[[noreturn]] void
rethrow_translated()
{
	try {
		throw;
	} catch (const Error&) {
		throw;
	} catch (const ::strata::index_not_created& exc) {
		throw Error(errc::not_found, exc.what());
	} catch (const ::strata::index_fenced& exc) {
		throw Error(errc::database, exc.what());
	} catch (const ::strata::corrupt_segment& exc) {
		throw Error(errc::corrupt, exc.what());
	} catch (const ::strata::corrupt_index& exc) {
		throw Error(errc::corrupt, exc.what());
	} catch (const std::system_error& exc) {
		throw Error(errc::database, exc.what());
	} catch (const std::invalid_argument& exc) {
		throw Error(errc::invalid, exc.what());
	} catch (const std::exception& exc) {
		throw Error(errc::internal, exc.what());
	}
}


// Every document of a segment, weighing nothing: the empty term and match_all.
class AllScorer final : public p::scorer {
public:
	explicit AllScorer(p::ordinal_t count) : count_(count) { }

	p::ordinal_t doc() const noexcept override { return doc_; }
	p::ordinal_t next() override { return doc_ = (started_ ? (doc_ == p::no_more_docs ? doc_ : at(doc_ + 1)) : at(0)); }
	p::ordinal_t advance(p::ordinal_t target) override {
		if (started_ && doc_ >= target) { return doc_; }
		return doc_ = at(target);
	}
	std::uint64_t cost() const noexcept override { return count_; }
	double score() override { return 0.0; }
	double max_score(p::ordinal_t, p::ordinal_t) override { return 0.0; }
	p::ordinal_t block_end(p::ordinal_t) override { return p::no_more_docs; }
	double max_score_total() const noexcept override { return 0.0; }
	std::size_t score_terms() const noexcept override { return 1; }
	std::optional<std::uint64_t> exact_count() override { return count_; }
	bool matches_all() const noexcept override { return true; }
	std::unique_ptr<p::scorer> fresh() const override { return std::make_unique<AllScorer>(count_); }

private:
	p::ordinal_t at(p::ordinal_t ord) {
		started_ = true;
		return ord < count_ ? ord : p::no_more_docs;
	}

	p::ordinal_t count_;
	p::ordinal_t doc_ = 0;
	bool started_ = false;
};


// Refuses, before any segment is read, what the engine does not search yet, so the answer does not
// depend on which segments a short-circuited operator reaches.
void
validate(const Query& query, int depth)
{
	if (depth > max_depth) {
		throw Error(errc::syntax, "The query is nested too deeply");
	}
	const auto* node = query.node();
	if (node == nullptr) {
		return;
	}
	switch (node->kind) {
		case QueryKind::term:
			return;
		case QueryKind::combine:
			switch (node->op) {
				case Op::AND:
				case Op::OR:
				case Op::AND_NOT:
				case Op::AND_MAYBE:
				case Op::FILTER:
					break;
				case Op::PHRASE:
				case Op::NEAR: {
					for (const auto& sub : node->subqueries) {
						const auto* leaf = sub.node();
						if (leaf == nullptr || leaf->kind != QueryKind::term || leaf->str.empty()) {
							unsupported(std::string(to_string(node->op)) + " of anything but terms");
						}
					}
					const std::size_t n = node->subqueries.size();
					if (node->op == Op::PHRASE && node->parameter != 0 && node->parameter != n) {
						unsupported("a PHRASE with a window other than its length");
					}
					return;
				}
				default:
					unsupported(std::string("the ") + to_string(node->op) + " operator");
			}
			for (const auto& sub : node->subqueries) {
				validate(sub, depth + 1);
			}
			return;
		case QueryKind::append: {
			// A chain of appends is walked along its spine, so its length costs no stack.
			const Query* spine = &query;
			while (spine->node() != nullptr && spine->node()->kind == QueryKind::append) {
				switch (spine->node()->op) {
					case Op::AND:
					case Op::OR:
					case Op::AND_NOT:
					case Op::AND_MAYBE:
					case Op::FILTER:
						break;
					default:
						unsupported(std::string("the ") + to_string(spine->node()->op) + " operator");
				}
				validate(spine->node()->subqueries[1], depth + 1);
				spine = &spine->node()->subqueries[0];
			}
			validate(*spine, depth + 1);
			return;
		}
		case QueryKind::scale:
			validate(node->subqueries[0], depth + 1);
			return;
		case QueryKind::wildcard:
			unsupported("wildcards");
		case QueryKind::edit_distance:
			unsupported("edit distances");
		case QueryKind::source:
			unsupported("posting sources");
		case QueryKind::text:
			unsupported("query text");
	}
}


// What building a query for one segment reads: the segment, and the term table and statistics of
// the snapshot that spans every segment of every shard, so a weight is the same in all of them.
struct Builder {
	const p::segment_snapshot& snap;
	p::term_table& terms;

	const p::inverted_plugin::reader& reader() const { return snap.seg->get<p::inverted_plugin>(); }

	p::weight weight_of(const std::string& term, termcount wqf = 1) const {
		return p::weight::bm25_xapian(terms.stats(), terms.df(term), wqf);
	}
};

using Scorer = p::scorer_ptr;

// op applied to two scorers, with a null scorer matching nothing.
Scorer
join(Op op, Scorer left, Scorer right)
{
	switch (op) {
		case Op::AND: {
			if (!left || !right) {
				return nullptr;
			}
			std::vector<Scorer> children;
			children.push_back(std::move(left));
			children.push_back(std::move(right));
			return std::make_unique<p::conjunction_scorer>(std::move(children));
		}
		case Op::OR: {
			if (!left || !right) {
				return left ? std::move(left) : std::move(right);
			}
			std::vector<Scorer> children;
			children.push_back(std::move(left));
			children.push_back(std::move(right));
			return std::make_unique<p::disjunction_scorer>(std::move(children));
		}
		case Op::AND_NOT:
			if (!left || !right) {
				return left;
			}
			return std::make_unique<p::exclusion_scorer>(std::move(left), std::move(right));
		case Op::AND_MAYBE:
			if (!left || !right) {
				return left;
			}
			return std::make_unique<p::required_optional_scorer>(std::move(left), std::move(right));
		case Op::FILTER:
			if (!left || !right) {
				return nullptr;
			}
			return join(Op::AND, std::move(left), std::make_unique<p::scale_scorer>(std::move(right), 0.0));
		default:
			throw Error(errc::internal, "A strata search met an operator it does not run");
	}
}

// AND or OR of any number of scorers, with a null scorer matching nothing.
Scorer
nary(Op op, std::vector<Scorer> children)
{
	if (op == Op::AND) {
		for (const auto& child : children) {
			if (!child) {
				return nullptr;
			}
		}
	} else {
		children.erase(std::remove_if(children.begin(), children.end(), [](const Scorer& child) { return !child; }), children.end());
	}
	if (children.empty()) {
		return nullptr;
	}
	if (children.size() == 1) {
		return std::move(children[0]);
	}
	if (op == Op::AND) {
		return std::make_unique<p::conjunction_scorer>(std::move(children));
	}
	return std::make_unique<p::disjunction_scorer>(std::move(children));
}

Scorer build(const Query& query, const Builder& b, int depth);

Scorer
build_positional(const QueryNode& node, const Builder& b)
{
	const std::size_t n = node.subqueries.size();
	if (n == 1) {
		return build(node.subqueries[0], b, 0);
	}
	std::vector<p::format::term_info> infos;
	std::vector<p::weight> weights;
	infos.reserve(n);
	weights.reserve(n);
	for (const auto& sub : node.subqueries) {
		const auto& leaf = *sub.node();
		const auto* info = b.terms.info(*b.snap.seg, leaf.str);
		if (info == nullptr) {
			return nullptr;
		}
		infos.push_back(*info);
		weights.push_back(b.weight_of(leaf.str, leaf.wqf));
	}
	const std::uint32_t window = node.op == Op::NEAR ? (node.parameter != 0 ? node.parameter : static_cast<std::uint32_t>(n)) : 0;
	return std::make_unique<p::phrase_scorer>(b.reader(), infos, std::move(weights), window);
}

Scorer
build(const Query& query, const Builder& b, int depth)
{
	if (depth > max_depth) {
		throw Error(errc::syntax, "The query is nested too deeply");
	}
	const auto* node = query.node();
	if (node == nullptr || node->matches_nothing) {
		return nullptr;
	}
	switch (node->kind) {
		case QueryKind::term: {
			if (node->str.empty()) {
				return std::make_unique<AllScorer>(b.snap.seg->core().doc_count());
			}
			const auto* info = b.terms.info(*b.snap.seg, node->str);
			if (info == nullptr) {
				return nullptr;
			}
			return std::make_unique<p::term_scorer>(b.reader(), *info, b.weight_of(node->str, node->wqf));
		}
		case QueryKind::combine: {
			if (node->op == Op::PHRASE || node->op == Op::NEAR) {
				return build_positional(*node, b);
			}
			// AND_NOT, AND_MAYBE and FILTER take the first subquery against all the others.
			Scorer result;
			bool first = true;
			std::vector<Scorer> rest;
			for (const auto& sub : node->subqueries) {
				auto scorer = build(sub, b, depth + 1);
				if (first) {
					result = std::move(scorer);
					first = false;
				} else if (node->op == Op::AND || node->op == Op::OR) {
					rest.push_back(std::move(scorer));
				} else {
					result = join(node->op, std::move(result), std::move(scorer));
				}
			}
			if (node->op == Op::AND || node->op == Op::OR) {
				std::vector<Scorer> children;
				if (result) {
					children.push_back(std::move(result));
				} else if (node->op == Op::AND) {
					return nullptr;
				}
				for (auto& scorer : rest) {
					if (scorer) {
						children.push_back(std::move(scorer));
					} else if (node->op == Op::AND) {
						return nullptr;
					}
				}
				if (children.empty()) {
					return nullptr;
				}
				if (children.size() == 1) {
					return std::move(children[0]);
				}
				if (node->op == Op::AND) {
					return std::make_unique<p::conjunction_scorer>(std::move(children));
				}
				return std::make_unique<p::disjunction_scorer>(std::move(children));
			}
			return result;
		}
		case QueryKind::append: {
			// Folds a chain of appends left to right, so a long chain costs no stack.
			std::vector<const QueryNode*> chain;
			const Query* base = &query;
			while (base->node() != nullptr && base->node()->kind == QueryKind::append) {
				chain.push_back(base->node());
				base = &base->node()->subqueries[0];
			}
			auto accumulated = build(*base, b, depth + 1);
			// A run of appends with one AND or OR is one n-ary scorer, so a long chain builds (and frees) a
			// flat scorer, not one nested as deep as the chain.
			std::vector<Scorer> run;
			Op run_op = Op::INVALID;
			const auto flush = [&] {
				if (!run.empty()) {
					accumulated = nary(run_op, std::move(run));
					run.clear();
					run_op = Op::INVALID;
				}
			};
			for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
				auto next = build((*it)->subqueries[1], b, depth + 1);
				const Op op = (*it)->op;
				if ((*it)->subqueries[0].empty()) {
					// A query that matches nothing by its structure (a text that analysed to nothing) is skipped,
					// not joined; a term missing from this segment is a different thing and joins as nothing.
					flush();
					accumulated = std::move(next);
				} else if (op == Op::AND || op == Op::OR) {
					if (run_op != op) {
						flush();
						run.push_back(std::move(accumulated));
						run_op = op;
					}
					run.push_back(std::move(next));
				} else {
					flush();
					accumulated = join(op, std::move(accumulated), std::move(next));
				}
			}
			flush();
			return accumulated;
		}
		case QueryKind::scale: {
			auto child = build(node->subqueries[0], b, depth + 1);
			if (!child) {
				return nullptr;
			}
			return std::make_unique<p::scale_scorer>(std::move(child), node->factor);
		}
		default:
			throw Error(errc::internal, "A strata search met a node it does not run");
	}
}


// A matching document shown to a spy: its id in its shard, as Xapian shows it, and its stored data,
// read when asked.
// Values and term lists arrive with the plugins that hold them (P1, P2); until then a value reads empty
// and the terms are refused, as a strata shard's own DocView does.
class SpyView final : public DocView {
public:
	SpyView(const p::segment& seg, p::ordinal_t ord, docid did) : seg_(seg), ord_(ord), did_(did) { }

	docid did() const override { return did_; }
	std::string data() const override { return seg_.get<::strata::stored_plugin>().stored(ord_); }
	rev version() const override { return seg_.get<::strata::keys_plugin>().version(ord_); }
	std::string value(valueno) const override { return {}; }
	void values(const ValueSink&) const override { }
	void terms(std::string_view, const TermSink&) const override { unavailable(); }
	void term_details(const TermDetailSink&) const override { unavailable(); }
	std::string serialise() const override { unavailable(); }

private:
	[[noreturn]] static void unavailable() {
		throw Error(errc::not_available, "The strata engine does not keep term lists yet");
	}

	const p::segment& seg_;
	p::ordinal_t ord_;
	docid did_;
};


// A hit on its way to the result: its weight and its id across the shards.
struct Candidate {
	double weight;
	std::uint64_t id;
};

// Whether a ranks before b: heavier first, then by id in the requested order.
struct Ranks {
	bool descending;
	bool operator()(const Candidate& a, const Candidate& b) const noexcept {
		if (a.weight != b.weight) {
			return a.weight > b.weight;
		}
		return descending ? a.id > b.id : a.id < b.id;
	}
};


SearchResult
run(std::span<engine::Shard* const> shards, const SearchRequest& request)
{
	const auto& source = request.query->source();
	if (shards.empty() || request.query->empty()) {
		return {};
	}
	if (request.sorter != nullptr) {
		unsupported("sorted results");
	}
	if (request.collapse_key != BAD_VALUENO) {
		unsupported("collapsed results");
	}
	if (request.fuzzy || request.nearest) {
		unsupported("similar documents");
	}
	if (request.percent_threshold != 0 || request.weight_threshold != 0) {
		unsupported("weight cutoffs");
	}
	validate(source, 0);

	// Pin every distinct index once, in a fixed order, so searches that share indexes cannot deadlock.
	std::vector<Shard*> mine;
	mine.reserve(shards.size());
	for (auto* shard : shards) {
		auto* strata = dynamic_cast<Shard*>(shard);
		if (strata == nullptr) {
			throw Error(errc::invalid, "A strata search was given a shard of another engine");
		}
		mine.push_back(strata);
	}
	std::vector<Shard*> order = mine;
	std::sort(order.begin(), order.end(), [](const Shard* a, const Shard* b) { return std::less<>{}(a->identity(), b->identity()); });
	std::vector<Pinned> pins;
	pins.reserve(order.size());
	for (std::size_t i = 0; i < order.size(); ++i) {
		if (i == 0 || order[i]->identity() != order[i - 1]->identity()) {
			pins.push_back(order[i]->pin());
		}
	}
	const auto pin_of = [&](const Shard* shard) -> const Pinned& {
		for (const auto& pin : pins) {
			if (pin.identity_ == shard->identity()) {
				return pin;
			}
		}
		throw Error(errc::internal, "A strata search lost a pinned shard");
	};

	// One snapshot over the shards' segments: statistics and document frequencies are the whole index's.
	std::vector<p::segment_snapshot> segments;
	std::vector<std::size_t> shard_of;
	for (std::size_t i = 0; i < mine.size(); ++i) {
		for (const auto& view : pin_of(mine[i]).segments()) {
			segments.push_back({view.seg, view.deleted, view.dead});
			shard_of.push_back(i);
		}
	}
	p::term_table terms(segments, term_key_prefix());

	const std::uint64_t window = static_cast<std::uint64_t>(request.first) + request.maxitems;
	const std::size_t keep = static_cast<std::size_t>(std::min<std::uint64_t>(window, std::numeric_limits<doccount>::max()));
	const Ranks ranks{request.order == DocidOrder::descending};
	const std::uint64_t n = mine.size();
	std::vector<Candidate> best;
	best.reserve(std::min<std::size_t>(keep, 1024));
	std::uint64_t matches = 0;

	for (std::size_t s = 0; s < segments.size(); ++s) {
		const auto& snap = segments[s];
		const Builder builder{snap, terms};
		const auto live = [&](Scorer scorer) -> Scorer {
			if (scorer && snap.any_deleted()) {
				return std::make_unique<p::live_scorer>(std::move(scorer), *snap.deleted);
			}
			return scorer;
		};

		auto counting = live(build(source, builder, 0));
		if (!counting) {
			continue;
		}
		const auto& core = snap.seg->core();
		const auto interleave = [&](p::ordinal_t ord) { return (core.docid(ord) - 1) * n + shard_of[s] + 1; };
		if (request.spy != nullptr) {
			// The spy sees every match once, as Xapian shows it the documents it examines.
			for (auto ord = counting->next(); ord != p::no_more_docs; ord = counting->next()) {
				request.spy->see(SpyView(*snap.seg, ord, static_cast<docid>(core.docid(ord))), counting->score());
				++matches;
			}
		} else {
			matches += p::count(*counting);
		}
		if (keep == 0) {
			continue;
		}

		auto scorer = live(build(source, builder, 0));
		const auto prune = [&] {
			if (best.size() == keep && best.front().weight > 0) {
				scorer->set_min_competitive_score(std::nextafter(best.front().weight, 0.0));
			}
		};
		prune();
		for (auto ord = scorer->next(); ord != p::no_more_docs; ord = scorer->next()) {
			const double weight = scorer->score();
			if (best.size() == keep && weight < best.front().weight) {
				continue;
			}
			const std::uint64_t id = interleave(ord);
			const Candidate candidate{weight, id};
			if (best.size() < keep) {
				best.push_back(candidate);
				std::push_heap(best.begin(), best.end(), ranks);
				prune();
			} else if (ranks(candidate, best.front())) {
				std::pop_heap(best.begin(), best.end(), ranks);
				best.back() = candidate;
				std::push_heap(best.begin(), best.end(), ranks);
				prune();
			}
		}
	}

	std::sort_heap(best.begin(), best.end(), ranks);
	SearchResult result;
	result.estimated = static_cast<doccount>(std::min<std::uint64_t>(matches, std::numeric_limits<doccount>::max()));
	const double top = best.empty() ? 0.0 : best.front().weight;
	for (std::size_t i = request.first; i < best.size(); ++i) {
		if (best[i].id > std::numeric_limits<docid>::max()) {
			throw Error(errc::internal, "A document id does not fit across the shards");
		}
		Hit hit;
		hit.did = static_cast<docid>(best[i].id);
		hit.weight = best[i].weight;
		hit.rank = static_cast<doccount>(i);
		hit.percent = top > 0 ? static_cast<int>(best[i].weight / top * 100.0 + 0.5) : 100;
		result.hits.push_back(hit);
	}
	return result;
}

}  // namespace


task::task<SearchResult>
search(std::span<engine::Shard* const> shards, const SearchRequest& request)
{
	try {
		co_return run(shards, request);
	} catch (...) {
		rethrow_translated();
	}
}

}  // namespace engine::strata
