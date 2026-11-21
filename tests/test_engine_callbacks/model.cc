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

// The model: which documents a query matches, with the terms a wildcard or an edit-distance node stands for.

#include "support.h"

namespace detail {

// --- the model: which documents a query matches ---

namespace {

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

}  // namespace

namespace {

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

}  // namespace

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

}  // namespace detail
