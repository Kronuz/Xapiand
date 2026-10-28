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
#include <utility>                 // for std::move

#include "prospect/reader.hh"      // for prospect::inverted_plugin

#include "engine/error.h"          // for Error, errc
#include "engine/strata/pattern.h"  // for wildcard_prefix, wildcard_matches, within_edit_distance, head


namespace engine::strata {

bool
in_key_space(std::string_view literal, std::string_view key_prefix)
{
	return !key_prefix.empty() && literal.starts_with(key_prefix);
}


std::vector<std::string>
expand(const QueryNode& node, std::span<const prospect::segment_snapshot> segments, prospect::term_table& terms, std::string_view key_prefix)
{
	const bool wildcard = node.kind == QueryKind::wildcard;
	std::string prefix;
	std::string_view tail;
	if (wildcard) {
		prefix = wildcard_prefix(node.str, node.pattern_flags);
	} else {
		prefix = std::string(head(node.str, node.prefix_length));
		tail = std::string_view(node.str).substr(prefix.size());
	}
	if (in_key_space(prefix, key_prefix)) {
		throw Error(errc::syntax, "The strata engine does not expand terms of the key field");
	}

	std::set<std::string> found;
	for (const auto& snapshot : segments) {
		snapshot.seg->get<prospect::inverted_plugin>().scan_dictionary(prefix, [&](std::string_view key, std::string_view) {
			if (!key.starts_with(prefix)) {
				return false;
			}
			if (wildcard ? wildcard_matches(node.str, key, node.pattern_flags) : within_edit_distance(tail, key.substr(prefix.size()), node.distance)) {
				found.emplace(key);
			}
			return true;
		});
	}

	std::vector<std::string> result(found.begin(), found.end());
	const std::size_t limit = node.parameter;
	if (limit != 0 && result.size() > limit) {
		switch (node.limit) {
			case Expansion::error:
				throw Error(errc::invalid, "The pattern \"" + node.str + "\" expands to more than " + std::to_string(limit) + " terms");
			case Expansion::first:
				result.resize(limit);
				break;
			case Expansion::most_frequent: {
				std::vector<std::pair<std::uint64_t, std::size_t>> by_frequency;
				by_frequency.reserve(result.size());
				for (std::size_t i = 0; i < result.size(); ++i) {
					by_frequency.emplace_back(terms.df(result[i]), i);
				}
				std::stable_sort(by_frequency.begin(), by_frequency.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
				by_frequency.resize(limit);
				std::vector<std::size_t> keep;
				for (const auto& entry : by_frequency) {
					keep.push_back(entry.second);
				}
				std::sort(keep.begin(), keep.end());
				std::vector<std::string> kept;
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
