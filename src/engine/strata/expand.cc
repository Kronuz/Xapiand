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

#include "engine/strata/expand.h"

#include <algorithm>               // for std::min, std::sort, std::stable_sort
#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <set>                     // for std::set
#include <string>                  // for std::string
#include <vector>                  // for std::vector
#include <utility>                 // for std::move

#include "prospect/reader.hh"      // for prospect::inverted_plugin

#include "engine/error.h"          // for Error, errc
#include "engine/strata/pattern.h"  // for wildcard_prefix, wildcard_matches, within_edit_distance, head


namespace engine::strata {

namespace {

// The names of the fields a node reads: its own, or with an empty pattern (Query::field) every field of the
// snapshot whose name starts with it.
std::vector<std::string>
field_names(const QueryNode& node, std::span<const prospect::segment_snapshot> segments)
{
	if (node.kind != QueryKind::wildcard || !node.str.empty() || node.field == KEY_FIELD) {
		return {node.field};
	}
	std::set<std::string> names;
	for (const auto& snapshot : segments) {
		const auto& table = snapshot.table();
		for (std::size_t id = 0; id < table.size(); ++id) {
			if (table[id].name.starts_with(node.field)) {
				names.insert(table[id].name);
			}
		}
	}
	return {names.begin(), names.end()};
}

}  // namespace


std::vector<FieldTerm>
expand(const QueryNode& node, std::span<const prospect::segment_snapshot> segments, prospect::term_table& terms)
{
	const bool wildcard = node.kind == QueryKind::wildcard;
	std::string prefix;
	std::string_view tail;
	if (wildcard) {
		prefix = wildcard_prefix(node.str, node.pattern_flags);
	} else {
		// An edit distance's prefix length counts the term's characters.
		const auto lead = head(node.str, node.prefix_length);
		prefix = std::string(lead);
		tail = std::string_view(node.str).substr(lead.size());
	}
	if (!wildcard && node.field == KEY_FIELD) {
		throw Error(errc::syntax, "The strata engine does not expand terms of the key field");
	}

	std::set<FieldTerm> found;
	for (const auto& name : field_names(node, segments)) {
		const auto matches = [&](std::string_view term) {
			return wildcard ? wildcard_matches(node.str, term, node.pattern_flags) : within_edit_distance(tail, term.substr(prefix.size()), node.distance);
		};
		for (const auto& snapshot : segments) {
			const auto id = snapshot.field_id(name);
			if (!id) {
				continue;
			}
			if (snapshot.key_field() == id) {
				const auto from = prefix;
				snapshot.seg->get<prospect::keys_plugin>().scan(from, [&](std::string_view key, auto) {
					if (!key.starts_with(from)) {
						return false;
					}
					if (matches(key)) {
						found.insert({name, std::string(key)});
					}
					return true;
				});
				continue;
			}
			snapshot.seg->get<prospect::inverted_plugin>().scan_dictionary(*id, prefix, [&](std::string_view term, std::string_view) {
				if (!term.starts_with(prefix)) {
					return false;
				}
				if (matches(term)) {
					found.insert({name, std::string(term)});
				}
				return true;
			});
		}
	}

	std::vector<FieldTerm> result(found.begin(), found.end());
	const std::size_t limit = node.parameter;
	if (limit != 0 && result.size() > limit) {
		switch (node.limit) {
			case Expansion::error:
				throw Error(errc::invalid, "The pattern \"" + node.field + node.str + "\" expands to more than " + std::to_string(limit) + " terms");
			case Expansion::first:
				result.resize(limit);
				break;
			case Expansion::most_frequent: {
				std::vector<std::pair<std::uint64_t, std::size_t>> by_frequency;
				by_frequency.reserve(result.size());
				for (std::size_t i = 0; i < result.size(); ++i) {
					by_frequency.emplace_back(terms.df(result[i].field, result[i].term), i);
				}
				std::stable_sort(by_frequency.begin(), by_frequency.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
				by_frequency.resize(limit);
				std::vector<std::size_t> keep;
				for (const auto& entry : by_frequency) {
					keep.push_back(entry.second);
				}
				std::sort(keep.begin(), keep.end());
				std::vector<FieldTerm> kept;
				for (const auto i : keep) {
					kept.push_back(std::move(result[i]));
				}
				result = std::move(kept);
				break;
			}
		}
	}
	return result;
}

}  // namespace engine::strata
