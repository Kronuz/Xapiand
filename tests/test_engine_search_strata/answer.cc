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

// The model's answer: the documents a query matches and their BM25 weights over the model's statistics.

#include "support.h"

namespace detail {

// --- the model's answer ---

namespace {

std::uint64_t id_of(std::size_t shard, std::size_t did, std::size_t shards) {
	return (did - 1) * shards + shard + 1;
}

bool phrase_in(const Model::Doc& doc, const std::vector<std::string>& words) {
	const auto first = doc.positions.find(words[0]);
	if (first == doc.positions.end()) {
		return false;
	}
	for (const auto start : first->second) {
		bool all = true;
		for (std::size_t k = 1; k < words.size() && all; ++k) {
			const auto it = doc.positions.find(words[k]);
			all = it != doc.positions.end() && std::binary_search(it->second.begin(), it->second.end(), start + static_cast<std::uint32_t>(k));
		}
		if (all) {
			return true;
		}
	}
	return false;
}

// Terms at distinct positions, in any order, spanning fewer than `window` positions.
bool near_in(const Model::Doc& doc, const std::vector<std::string>& words, std::uint32_t window) {
	std::vector<std::uint32_t> chosen;
	std::function<bool(std::size_t)> place = [&](std::size_t k) {
		if (k == words.size()) {
			const auto [lo, hi] = std::minmax_element(chosen.begin(), chosen.end());
			return *hi - *lo < window;
		}
		const auto it = doc.positions.find(words[k]);
		if (it == doc.positions.end()) {
			return false;
		}
		for (const auto position : it->second) {
			if (std::find(chosen.begin(), chosen.end(), position) != chosen.end()) {
				continue;
			}
			chosen.push_back(position);
			if (place(k + 1)) {
				return true;
			}
			chosen.pop_back();
		}
		return false;
	};
	return place(0);
}

}  // namespace

// --- the model's weights: BM25 (k1 = 1, k3 = 1, b = 0.5, minimum normalised length 0.5) ---

Statistics statistics_of(const Model& model, std::size_t shards) {
	Statistics st;
	for (std::size_t s = 0; s < shards; ++s) {
		for (const auto& doc : model.shards[s]) {
			if (!doc.live) { continue; }
			st.docs += 1;
			st.length += doc.length;
			for (const auto& term : doc.terms) { st.df[term] += 1; }
		}
	}
	return st;
}

namespace {

// The BM25 weight of a term of document frequency df occurring tf times in the document.
double bm25_of(const Statistics& st, const Model::Doc& doc, double df, double tf, double wqf) {
	double tw = (st.docs - df + 0.5) / (df + 0.5);
	if (tw < 2) { tw = tw * 0.5 + 1; }
	const double idf = std::log(tw) * 2.0 * wqf / (1.0 + wqf) * 2.0;
	const double K = 1.0 * (0.5 + 0.5 * std::max(doc.length / (st.length / st.docs), 0.5));
	return idf * tf / (K + tf);
}

double bm25(const Statistics& st, const Model::Doc& doc, const std::string& term, double wqf) {
	const auto it = doc.wdf.find(term);
	if (it == doc.wdf.end() || term.empty()) { return 0.0; }
	return bm25_of(st, doc, st.df.at(term), it->second, wqf);
}

// The terms a SYNONYM counts as one weigh as one term: of the size of their union (as independent sets,
// rounded), occurring as often as all of them together, at most the document's length.
double synonym_weight(const Statistics& st, const Model::Doc& doc, const std::vector<std::string>& terms) {
	double miss = 1, wdf = 0;
	for (const auto& term : terms) {
		miss *= 1.0 - st.df.at(term) / st.docs;
		const auto it = doc.wdf.find(term);
		wdf += it == doc.wdf.end() ? 0 : it->second;
	}
	const double df = terms.size() == 1 ? st.df.at(terms[0]) : std::floor(st.docs * (1.0 - miss) + 0.5);
	return wdf == 0 ? 0.0 : bm25_of(st, doc, df, std::min<double>(wdf, doc.length), 1.0);
}

// What fills one position of a phrase: where it occurs in the document and what it weighs.
struct Slot {
	std::vector<std::uint32_t> positions;
	double weight = 0;
};

std::size_t edit_distance_of(const std::string& a, const std::string& b) {
	std::vector<std::size_t> row(b.size() + 1);
	for (std::size_t j = 0; j <= b.size(); ++j) { row[j] = j; }
	for (std::size_t i = 1; i <= a.size(); ++i) {
		std::size_t diagonal = row[0];
		row[0] = i;
		for (std::size_t j = 1; j <= b.size(); ++j) {
			const std::size_t up = row[j];
			row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
			diagonal = up;
		}
	}
	return row[b.size()];
}

void add_positions(const Model::Doc& doc, const std::string& term, std::vector<std::uint32_t>& into) {
	if (const auto it = doc.positions.find(term); it != doc.positions.end()) {
		into.insert(into.end(), it->second.begin(), it->second.end());
	}
}

// The slot a word, an expansion, or an OR or SYNONYM of those fills.
Slot slot_in(const Statistics& st, const Model::Doc& doc, const Query& query) {
	const auto& node = *query.node();
	Slot slot;
	std::vector<std::string> members;
	switch (node.kind) {
		case engine::QueryKind::term:
			add_positions(doc, node.str, slot.positions);
			slot.weight = bm25(st, doc, node.str, node.wqf);
			break;
		case engine::QueryKind::wildcard:
		case engine::QueryKind::edit_distance:
			for (const auto& [term, df] : st.df) {
				const bool near = node.kind == engine::QueryKind::wildcard ? term.starts_with(node.str) : edit_distance_of(term, node.str) <= node.distance;
				if (near) { members.push_back(term); }
			}
			break;
		default:
			if (node.op == Op::SYNONYM) {
				for (const auto& sub : node.subqueries) { members.push_back(sub.node()->str); }
			} else {
				for (const auto& sub : node.subqueries) {
					const auto part = slot_in(st, doc, sub);
					slot.positions.insert(slot.positions.end(), part.positions.begin(), part.positions.end());
					slot.weight += part.weight;
				}
			}
	}
	if (!members.empty()) {
		for (const auto& term : members) { add_positions(doc, term, slot.positions); }
		if (node.kind == engine::QueryKind::combine || node.op == Op::SYNONYM) {
			slot.weight = synonym_weight(st, doc, members);
		} else {
			for (const auto& term : members) { slot.weight += bm25(st, doc, term, 1); }
		}
	}
	std::sort(slot.positions.begin(), slot.positions.end());
	slot.positions.erase(std::unique(slot.positions.begin(), slot.positions.end()), slot.positions.end());
	return slot;
}

// A phrase over slots: consecutive positions.
bool phrase_of(const std::vector<Slot>& slots) {
	for (const auto start : slots[0].positions) {
		bool all = true;
		for (std::size_t k = 1; k < slots.size() && all; ++k) {
			all = std::binary_search(slots[k].positions.begin(), slots[k].positions.end(), start + static_cast<std::uint32_t>(k));
		}
		if (all) { return true; }
	}
	return false;
}

// One distinct position per slot, spanning fewer than `window` positions.
bool near_of(const std::vector<Slot>& slots, std::uint32_t window) {
	std::vector<std::uint32_t> chosen;
	std::function<bool(std::size_t)> place = [&](std::size_t k) {
		if (k == slots.size()) {
			const auto [lo, hi] = std::minmax_element(chosen.begin(), chosen.end());
			return *hi - *lo < window;
		}
		for (const auto position : slots[k].positions) {
			if (std::find(chosen.begin(), chosen.end(), position) != chosen.end()) { continue; }
			chosen.push_back(position);
			if (place(k + 1)) { return true; }
			chosen.pop_back();
		}
		return false;
	};
	return place(0);
}

}  // namespace

// Whether the document matches and, if so, its weight.
std::pair<bool, double> weigh(const Statistics& st, const Model::Doc& doc, const Query& query) {
	const auto* node = query.node();
	if (node == nullptr || !doc.live) { return {false, 0.0}; }
	switch (node->kind) {
		case engine::QueryKind::term:
			if (node->str.empty()) { return {true, 0.0}; }
			return {doc.terms.contains(node->str), bm25(st, doc, node->str, node->wqf)};
		case engine::QueryKind::scale: {
			auto [hit, weight] = weigh(st, doc, node->subqueries[0]);
			return {hit, weight * node->factor};
		}
		case engine::QueryKind::combine:
		case engine::QueryKind::append: {
			const auto& subs = node->subqueries;
			if (node->kind == engine::QueryKind::append && subs[0].empty()) { return weigh(st, doc, subs[1]); }
			switch (node->op) {
				case Op::AND:
				case Op::OR: {
					bool any = false, every = true;
					double sum = 0;
					for (const auto& sub : subs) {
						const auto [hit, weight] = weigh(st, doc, sub);
						any = any || hit;
						every = every && hit;
						sum += hit ? weight : 0.0;
					}
					return {node->op == Op::AND ? every : any, sum};
				}
				case Op::AND_NOT: {
					auto left = weigh(st, doc, subs[0]);
					for (std::size_t i = 1; i < subs.size() && left.first; ++i) {
						left.first = !weigh(st, doc, subs[i]).first;
					}
					return left;
				}
				case Op::AND_MAYBE: {
					auto left = weigh(st, doc, subs[0]);
					const auto right = weigh(st, doc, subs[1]);
					if (left.first && right.first) { left.second += right.second; }
					return left;
				}
				case Op::FILTER: {
					auto left = weigh(st, doc, subs[0]);
					left.first = left.first && weigh(st, doc, subs[1]).first;
					return left;
				}
				case Op::PHRASE:
				case Op::NEAR: {
					if (query.alternatives()) {
						std::vector<Slot> slots;
						double total = 0;
						for (const auto& sub : subs) {
							slots.push_back(slot_in(st, doc, sub));
							total += slots.back().weight;
							if (slots.back().positions.empty()) { return {false, 0.0}; }
						}
						const auto window = node->parameter ? node->parameter : static_cast<std::uint32_t>(slots.size());
						return {node->op == Op::PHRASE ? phrase_of(slots) : near_of(slots, window), total};
					}
					std::vector<std::string> words;
					double sum = 0;
					for (const auto& sub : subs) {
						words.push_back(sub.node()->str);
						sum += bm25(st, doc, sub.node()->str, sub.node()->wqf);
					}
					const bool hit = node->op == Op::PHRASE ? phrase_in(doc, words) : near_in(doc, words, node->parameter ? node->parameter : static_cast<std::uint32_t>(words.size()));
					return {hit, sum};
				}
				default:
					return {false, 0.0};
			}
		}
		default:
			return {false, 0.0};
	}
}

// The ids of the matching documents and their weights.
std::map<std::uint64_t, double> model_hits(const Model& model, const Query& query, std::size_t shards) {
	const auto st = statistics_of(model, shards);
	std::map<std::uint64_t, double> hits;
	for (std::size_t s = 0; s < shards; ++s) {
		const auto& docs = model.shards[s];
		for (std::size_t d = 0; d < docs.size(); ++d) {
			const auto [hit, weight] = weigh(st, docs[d], query);
			if (hit) {
				hits[id_of(s, d + 1, shards)] = weight;
			}
		}
	}
	return hits;
}

}  // namespace detail
