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

#include <algorithm>               // for std::push_heap, std::pop_heap, std::sort_heap
#include <cmath>                   // for std::nextafter
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <functional>              // for std::less
#include <limits>                  // for std::numeric_limits
#include <memory>                  // for std::make_unique
#include <optional>                // for std::optional
#include <stdexcept>               // for std::invalid_argument, std::exception
#include <string>                  // for std::string
#include <system_error>            // for std::system_error
#include <unordered_map>           // for std::unordered_map
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "prospect/index.hh"       // for prospect::index
#include "prospect/operators.hh"   // for prospect::scale_scorer
#include "prospect/searcher.hh"    // for prospect::term_table, live_scorer

#include "engine/error.h"          // for Error, errc
#include "engine/strata/doc_view.h"  // for SegmentDocView
#include "engine/strata/expand.h"  // for expand
#include "engine/strata/pin.h"     // for Pinned
#include "engine/strata/shard.h"   // for Shard
#include "engine/strata/values.h"  // for values::Slot


namespace engine::strata {

namespace {

namespace p = ::prospect;

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


bool
is_term(const Query& query)
{
	const auto* leaf = query.node();
	return leaf != nullptr && leaf->kind == QueryKind::term && !leaf->str.empty();
}

// Whether the operator joins two scorers (the rest take a list of terms).
bool
is_joining(Op op)
{
	switch (op) {
		case Op::AND:
		case Op::OR:
		case Op::AND_NOT:
		case Op::AND_MAYBE:
		case Op::FILTER:
		case Op::XOR:
		case Op::MAX:
			return true;
		default:
			return false;
	}
}

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
				case Op::PHRASE:
				case Op::NEAR: {
					for (const auto& sub : node->subqueries) {
						if (!is_term(sub)) {
							unsupported(std::string(to_string(node->op)) + " of anything but terms");
						}
					}
					const std::size_t n = node->subqueries.size();
					if (node->op == Op::PHRASE && node->parameter != 0 && node->parameter != n) {
						unsupported("a PHRASE with a window other than its length");
					}
					return;
				}
				case Op::SYNONYM:
				case Op::ELITE_SET:
					for (const auto& sub : node->subqueries) {
						if (!is_term(sub)) {
							unsupported(std::string(to_string(node->op)) + " of anything but terms");
						}
					}
					return;
				default:
					if (!is_joining(node->op)) {
						unsupported(std::string("the ") + to_string(node->op) + " operator");
					}
			}
			for (const auto& sub : node->subqueries) {
				validate(sub, depth + 1);
			}
			return;
		case QueryKind::append: {
			// A chain of appends is walked along its spine, so its length costs no stack.
			const Query* spine = &query;
			while (spine->node() != nullptr && spine->node()->kind == QueryKind::append) {
				if (!is_joining(spine->node()->op)) {
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
		case QueryKind::edit_distance:
			switch (node->op) {
				case Op::SYNONYM:
				case Op::OR:
				case Op::MAX:
					return;
				default:
					unsupported(std::string("expansions combined by ") + to_string(node->op));
			}
		case QueryKind::source:
			if (node->src == nullptr) {
				throw Error(errc::invalid, "A source node has no source");
			}
			return;
		case QueryKind::text:
			unsupported("query text");
	}
}


// The terms that act as one operator: a SYNONYM or an ELITE_SET node's, or the terms a wildcard or
// edit-distance node expands to.
struct Group {
	struct Member {
		std::string term;
		termcount wqf;
	};
	std::vector<Member> members;
	std::optional<p::weight> synonym;   // the weight of the union, for a SYNONYM combiner
};

// What a search works out once over the whole index and every segment then shares, so a document
// weighs the same in all of them: the groups of its nodes.
struct Plan {
	std::span<const p::segment_snapshot> segments;
	p::term_table& terms;
	std::string_view key_prefix;
	std::unordered_map<const QueryNode*, Group> groups;
};

// What building a query for one segment reads: the segment, and the term table and statistics of
// the snapshot that spans every segment of every shard, so a weight is the same in all of them.
struct Builder {
	const p::segment_snapshot& snap;
	p::term_table& terms;
	Plan& plan;

	const p::inverted_plugin::reader& reader() const { return snap.seg->get<p::inverted_plugin>(); }

	p::weight weight_of(const std::string& term, termcount wqf = 1) const {
		return p::weight::bm25_xapian(terms.stats(), terms.df(term), wqf);
	}
};

// The combiner of a group node: how its terms join.
Op
combiner_of(const QueryNode& node)
{
	if (node.kind == QueryKind::combine) {
		return node.op == Op::SYNONYM ? Op::SYNONYM : Op::OR;
	}
	return node.op;
}

// The group of a SYNONYM, ELITE_SET, wildcard or edit-distance node, worked out on first use.
const Group&
group_of(const QueryNode& node, const Builder& b)
{
	auto& groups = b.plan.groups;
	if (const auto found = groups.find(&node); found != groups.end()) {
		return found->second;
	}
	Group group;
	if (node.kind == QueryKind::combine) {
		for (const auto& sub : node.subqueries) {
			const auto& leaf = *sub.node();
			group.members.push_back({leaf.str, leaf.wqf});
		}
	} else {
		for (auto& term : expand(node, b.plan.segments, b.terms, b.plan.key_prefix)) {
			group.members.push_back({std::move(term), 1});
		}
	}
	if (node.kind == QueryKind::combine && node.op == Op::ELITE_SET) {
		// The heaviest terms of the whole index, by idf, in query order; equal weights keep the earlier term.
		constexpr std::size_t default_size = 10;
		const std::size_t size = node.parameter != 0 ? node.parameter : default_size;
		std::vector<std::pair<double, std::size_t>> keyed;
		for (std::size_t i = 0; i < group.members.size(); ++i) {
			const auto& member = group.members[i];
			if (b.terms.df(member.term) != 0) {
				keyed.emplace_back(b.weight_of(member.term, member.wqf).idf, i);
			}
		}
		std::stable_sort(keyed.begin(), keyed.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
		if (keyed.size() > size) {
			keyed.resize(size);
		}
		std::vector<std::size_t> kept;
		for (const auto& entry : keyed) {
			kept.push_back(entry.second);
		}
		std::sort(kept.begin(), kept.end());
		std::vector<Group::Member> members;
		for (const auto i : kept) {
			members.push_back(std::move(group.members[i]));
		}
		group.members = std::move(members);
	}
	if (combiner_of(node) == Op::SYNONYM) {
		std::vector<std::uint64_t> dfs;
		for (const auto& member : group.members) {
			dfs.push_back(b.terms.df(member.term));
		}
		const auto& stats = b.terms.stats();
		group.synonym = p::weight::bm25_xapian(stats, dfs.empty() ? 0 : p::synonym_df(dfs, stats.doc_count), 1);
	}
	return groups.emplace(&node, std::move(group)).first->second;
}

using Scorer = p::scorer_ptr;

// XOR or MAX of the scorers, which are not null.
Scorer
merge(Op op, std::vector<Scorer> children)
{
	if (children.size() == 1) {
		return std::move(children[0]);
	}
	if (op == Op::XOR) {
		return std::make_unique<p::xor_scorer>(std::move(children));
	}
	return std::make_unique<p::max_scorer>(std::move(children));
}

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
		case Op::XOR:
		case Op::MAX: {
			if (!left || !right) {
				return left ? std::move(left) : std::move(right);
			}
			std::vector<Scorer> children;
			children.push_back(std::move(left));
			children.push_back(std::move(right));
			return merge(op, std::move(children));
		}
		default:
			throw Error(errc::internal, "A strata search met an operator it does not run");
	}
}

Scorer build(const Query& query, const Builder& b, int depth);

// Appends the query, or, when `into` is set and it is an OR, the subqueries of that OR (and of the ORs
// inside it), to the list.
void
flatten_or(bool into, const Query& query, std::vector<const Query*>& list, int depth)
{
	const auto* node = query.node();
	if (depth > max_depth) {
		throw Error(errc::syntax, "The query is nested too deeply");
	}
	if (into && node != nullptr && node->kind == QueryKind::combine && node->op == Op::OR && !node->matches_nothing) {
		for (const auto& sub : node->subqueries) {
			flatten_or(true, sub, list, depth + 1);
		}
		return;
	}
	list.push_back(&query);
}

// The scorer of a group node in one segment: its terms joined by the node's combiner.
Scorer
build_group(const QueryNode& node, const Builder& b)
{
	const auto& group = group_of(node, b);
	const Op combiner = combiner_of(node);
	if (combiner == Op::SYNONYM) {
		std::vector<p::format::term_info> infos;
		for (const auto& member : group.members) {
			if (const auto* info = b.terms.info(*b.snap.seg, member.term)) {
				infos.push_back(*info);
			}
		}
		if (infos.empty()) {
			return nullptr;
		}
		return std::make_unique<p::synonym_scorer>(b.reader(), infos, *group.synonym);
	}
	std::vector<Scorer> children;
	for (const auto& member : group.members) {
		if (const auto* info = b.terms.info(*b.snap.seg, member.term)) {
			children.push_back(std::make_unique<p::term_scorer>(b.reader(), *info, b.weight_of(member.term, member.wqf)));
		}
	}
	if (children.empty()) {
		return nullptr;
	}
	if (children.size() == 1) {
		return std::move(children[0]);
	}
	if (combiner == Op::MAX) {
		return std::make_unique<p::max_scorer>(std::move(children));
	}
	return std::make_unique<p::disjunction_scorer>(std::move(children));
}

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
			if (node->op == Op::SYNONYM || node->op == Op::ELITE_SET) {
				return build_group(*node, b);
			}
			// AND_NOT, AND_MAYBE and FILTER take the first subquery against all the others.
			Scorer result;
			bool first = true;
			std::vector<Scorer> rest;
			// Xapian reads an OR inside a MAX as more subqueries of the MAX, not as a sum.
			std::vector<const Query*> flat;
			for (const auto& sub : node->subqueries) {
				flatten_or(node->op == Op::MAX, sub, flat, depth);
			}
			for (const Query* subquery : flat) {
				const Query& sub = *subquery;
				auto scorer = build(sub, b, depth + 1);
				if (first) {
					result = std::move(scorer);
					first = false;
				} else if (node->op == Op::AND || node->op == Op::OR || node->op == Op::XOR || node->op == Op::MAX) {
					rest.push_back(std::move(scorer));
				} else {
					result = join(node->op, std::move(result), std::move(scorer));
				}
			}
			if (node->op == Op::XOR || node->op == Op::MAX) {
				std::vector<Scorer> children;
				if (result) {
					children.push_back(std::move(result));
				}
				for (auto& scorer : rest) {
					if (scorer) {
						children.push_back(std::move(scorer));
					}
				}
				return children.empty() ? nullptr : merge(node->op, std::move(children));
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
			for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
				auto next = build((*it)->subqueries[1], b, depth + 1);
				accumulated = join((*it)->op, std::move(accumulated), std::move(next));
			}
			return accumulated;
		}
		case QueryKind::scale: {
			auto child = build(node->subqueries[0], b, depth + 1);
			if (!child) {
				return nullptr;
			}
			return std::make_unique<p::scale_scorer>(std::move(child), node->factor);
		}
		case QueryKind::wildcard:
		case QueryKind::edit_distance:
			return build_group(*node, b);
		case QueryKind::source: {
			const auto* column = values::Reader(*b.snap.seg).column(node->src->slot());
			if (column == nullptr) {
				return nullptr;
			}
			return std::make_unique<p::source_scorer<values::Slot, PostingSource>>(values::Slot(*column), *node->src);
		}
		default:
			throw Error(errc::internal, "A strata search met a node it does not run");
	}
}


// A hit on its way to the result: its weight, its id across the shards and, when the request has a
// sorter, its key.
struct Candidate {
	double weight;
	std::uint64_t id;
	std::string key;
};

// Whether a ranks before b: by key when there are keys (ascending, bytewise), then heavier first,
// then by id in the requested order.
struct Ranks {
	bool descending;
	bool keyed;
	bool operator()(const Candidate& a, const Candidate& b) const noexcept {
		if (keyed && a.key != b.key) {
			return a.key < b.key;
		}
		if (a.weight != b.weight) {
			return a.weight > b.weight;
		}
		return descending ? a.id > b.id : a.id < b.id;
	}
};

docid
to_docid(std::uint64_t id)
{
	if (id > std::numeric_limits<docid>::max()) {
		throw Error(errc::internal, "A document id does not fit across the shards");
	}
	return static_cast<docid>(id);
}


SearchResult
run(std::span<engine::Shard* const> shards, const SearchRequest& request)
{
	const auto& source = request.query->source();
	if (shards.empty() || request.query->empty()) {
		return {};
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
	p::term_table terms(segments, pins.front().key_prefix_);
	Plan plan{segments, terms, pins.front().key_prefix_, {}};

	const std::uint64_t window = static_cast<std::uint64_t>(request.first) + request.maxitems;
	const std::size_t keep = static_cast<std::size_t>(std::min<std::uint64_t>(window, std::numeric_limits<doccount>::max()));
	const Ranks ranks{request.order == DocidOrder::descending, request.sorter != nullptr};
	const std::uint64_t n = mine.size();
	std::vector<Candidate> best;
	best.reserve(std::min<std::size_t>(keep, 1024));
	std::uint64_t matches = 0;
	double heaviest = 0;   // the greatest weight of any match, when every match is visited
	// A spy sees every match and a key maker keys every one, so those searches visit them all; the
	// others count first and then stop as soon as no later document can enter the window.
	const bool exhaustive = request.sorter != nullptr || request.spy != nullptr;

	for (std::size_t s = 0; s < segments.size(); ++s) {
		const auto& snap = segments[s];
		const Builder builder{snap, terms, plan};
		const auto live = [&](Scorer scorer) -> Scorer {
			if (scorer && snap.any_deleted()) {
				return std::make_unique<p::live_scorer>(std::move(scorer), *snap.deleted);
			}
			return scorer;
		};

		if (!exhaustive) {
			auto counting = live(build(source, builder, 0));
			if (!counting) {
				continue;
			}
			matches += p::count(*counting);
			if (keep == 0) {
				continue;
			}
		}

		auto scorer = live(build(source, builder, 0));
		if (!scorer) {
			continue;
		}
		const auto& core = snap.seg->core();
		const auto prune = [&] {
			if (!exhaustive && best.size() == keep && best.front().weight > 0) {
				scorer->set_min_competitive_score(std::nextafter(best.front().weight, 0.0));
			}
		};
		prune();
		for (auto ord = scorer->next(); ord != p::no_more_docs; ord = scorer->next()) {
			const double weight = scorer->score();
			const std::uint64_t id = (core.docid(ord) - 1) * n + shard_of[s] + 1;
			Candidate candidate{weight, id, {}};
			if (exhaustive) {
				++matches;
				heaviest = std::max(heaviest, weight);
				const SegmentDocView view(to_docid(id), *snap.seg, ord);
				if (request.spy != nullptr) {
					request.spy->see(view, weight);
				}
				if (keep == 0) {
					continue;
				}
				if (request.sorter != nullptr) {
					candidate.key = request.sorter->key(view);
				}
			} else if (best.size() == keep && weight < best.front().weight) {
				continue;
			}
			if (best.size() < keep) {
				best.push_back(std::move(candidate));
				std::push_heap(best.begin(), best.end(), ranks);
				prune();
			} else if (ranks(candidate, best.front())) {
				std::pop_heap(best.begin(), best.end(), ranks);
				best.back() = std::move(candidate);
				std::push_heap(best.begin(), best.end(), ranks);
				prune();
			}
		}
	}

	std::sort_heap(best.begin(), best.end(), ranks);
	SearchResult result;
	result.estimated = static_cast<doccount>(std::min<std::uint64_t>(matches, std::numeric_limits<doccount>::max()));
	const double top = exhaustive ? heaviest : (best.empty() ? 0.0 : best.front().weight);
	for (std::size_t i = request.first; i < best.size(); ++i) {
		Hit hit;
		hit.did = to_docid(best[i].id);
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
