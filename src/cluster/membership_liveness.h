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

/*
 * The root leader's volatile keepalive table (cluster-release D6; docs/design/cluster-membership.md
 * M7; docs/mechanisms/cluster-membership.md). Every node sends a keepalive every 500 ms; a node
 * silent for two intervals is `suspect` and for three is `down`, and only transitions are
 * reported. The same table answers M7's `duplicate`: another boot of a node whose admitted boot
 * was heard within the window. A new leader starts a table at its election and counts silence
 * from then. Not thread-safe: one loop owns it.
 */

#pragma once

#include <chrono>         // for std::chrono::steady_clock, std::chrono::milliseconds
#include <cstdint>        // for std::uint8_t, std::uint64_t
#include <optional>       // for std::optional
#include <unordered_map>  // for std::unordered_map
#include <vector>         // for std::vector

namespace cluster::membership {

using Clock = std::chrono::steady_clock;

enum class Liveness : std::uint8_t { up, suspect, down };
const char* name(Liveness l) noexcept;

struct Timing {
	Clock::duration interval = std::chrono::milliseconds(500);  // a node's keepalive period
	unsigned suspect_after = 2, down_after = 3;                   // silent intervals
	Clock::duration duplicate_window = std::chrono::seconds(2);  // M7
};

struct Transition {
	std::uint64_t node = 0;
	Liveness from = Liveness::up, to = Liveness::up;
	bool operator==(const Transition&) const = default;
};

class LivenessTable {
public:
	explicit LivenessTable(Clock::time_point elected, Timing timing = {});

	// A directory node this leader expects keepalives from, admitted as `boot` (0: none).
	// Silence counts from the election, or from now for a node added later.
	void expect(std::uint64_t node, std::uint64_t boot, Clock::time_point now);
	// A retired node: no longer tracked or reported.
	void forget(std::uint64_t node);
	// A keepalive from `node`'s admitted `boot`. A new boot (a restart) replaces the entry.
	// Returns the transition to `up` when this ends a suspect or down state.
	std::optional<Transition> heard(std::uint64_t node, std::uint64_t boot, Clock::time_point now);
	// The transitions silence caused since the last sweep, in node order.
	std::vector<Transition> sweep(Clock::time_point now);

	Liveness state(std::uint64_t node) const;  // `down` for a node not tracked
	// M7: `boot` is another process of `node`, whose admitted boot (`admitted`, from the directory)
	// was heard within the window; a boot this table never heard counts as heard at the election.
	bool duplicate(std::uint64_t node, std::uint64_t admitted, std::uint64_t boot, Clock::time_point now) const;

	std::size_t size() const noexcept { return entries_.size(); }

private:
	struct Entry {
		std::uint64_t boot = 0;
		Clock::time_point heard;
		Liveness state = Liveness::up;
	};
	Liveness judge(const Entry& e, Clock::time_point now) const;

	Clock::time_point elected_;
	Timing timing_;
	std::unordered_map<std::uint64_t, Entry> entries_;
};

} // namespace cluster::membership
