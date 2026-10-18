/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
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

#include <cstddef>                          // for std::size_t, std::ptrdiff_t
#include <string>                           // for std::string
#include <vector>                           // for std::vector


// The leader's choice of a new primary for a shard whose primary died. The Raft
// leader already holds the cluster majority, so the shard's own copies are not a
// quorum: the dead ones cannot vote, and with one replica a majority of all copies
// would need the dead primary itself.
namespace primary_election {

struct Copy {
	bool active = false;       // the copy's node is alive
	bool voted = false;        // it answered this election
	bool eligible = false;     // its node may lead
	std::string uuid;          // its database's UUID; empty: no local copy (never pulled)
	unsigned long long revision = 0;
};

// The index of the copy to promote, or -1 to keep waiting: while the primary
// (copies[0]) lives, until every live copy has voted, or with no eligible one.
// The shard's database is the UUID most live copies hold (a tie goes to shard
// order); a copy of another database is never promoted. Among the rest it prefers
// a copy that exists, then the highest revision, then shard order.
inline std::ptrdiff_t elect(const std::vector<Copy>& copies) {
	if (copies.empty() || copies[0].active) {
		return -1;
	}
	for (const auto& copy : copies) {
		if (copy.active && !copy.voted) {
			return -1;
		}
	}

	const std::string* database = nullptr;
	std::size_t holders = 0;
	for (const auto& copy : copies) {
		if (!copy.active || copy.uuid.empty()) {
			continue;
		}
		std::size_t count = 0;
		for (const auto& other : copies) {
			count += other.active && other.uuid == copy.uuid;
		}
		if (count > holders) {
			holders = count;
			database = &copy.uuid;
		}
	}

	std::ptrdiff_t elected = -1;
	for (std::ptrdiff_t i = 0, n = static_cast<std::ptrdiff_t>(copies.size()); i < n; ++i) {
		const auto& copy = copies[i];
		if (!copy.active || !copy.eligible) {
			continue;
		}
		bool exists = !copy.uuid.empty();
		if (exists && copy.uuid != *database) {
			continue;
		}
		if (elected < 0) {
			elected = i;
			continue;
		}
		const auto& best = copies[elected];
		bool best_exists = !best.uuid.empty();
		if (exists != best_exists ? exists : copy.revision > best.revision) {
			elected = i;
		}
	}
	return elected;
}

}  // namespace primary_election
