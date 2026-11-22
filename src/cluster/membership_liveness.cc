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

#include "cluster/membership_liveness.h"

#include <algorithm>  // for std::max, std::sort

namespace cluster::membership {

const char* name(Liveness l) noexcept {
	switch (l) {
		case Liveness::up: return "up";
		case Liveness::suspect: return "suspect";
		case Liveness::down: return "down";
	}
	return "?";
}

LivenessTable::LivenessTable(Clock::time_point elected, Timing timing) : elected_(elected), timing_(timing) {}

Liveness LivenessTable::judge(const Entry& e, Clock::time_point now) const {
	const auto silent = now - e.heard;
	if (silent >= timing_.interval * timing_.down_after) {
		return Liveness::down;
	}
	if (silent >= timing_.interval * timing_.suspect_after) {
		return Liveness::suspect;
	}
	return Liveness::up;
}

void LivenessTable::expect(std::uint64_t node, std::uint64_t boot, Clock::time_point now) {
	auto [it, added] = entries_.try_emplace(node, Entry{boot, std::max(now, elected_), Liveness::up});
	if (!added && it->second.boot == 0) {
		it->second.boot = boot;
	}
}

void LivenessTable::forget(std::uint64_t node) {
	entries_.erase(node);
}

std::optional<Transition> LivenessTable::heard(std::uint64_t node, std::uint64_t boot, Clock::time_point now) {
	auto [it, added] = entries_.try_emplace(node, Entry{boot, now, Liveness::up});
	if (added) {
		return Transition{node, Liveness::down, Liveness::up};
	}
	auto& e = it->second;
	e.heard = e.boot == boot ? std::max(e.heard, now) : now;
	e.boot = boot;
	if (e.state == Liveness::up) {
		return std::nullopt;
	}
	Transition t{node, e.state, Liveness::up};
	e.state = Liveness::up;
	return t;
}

std::vector<Transition> LivenessTable::sweep(Clock::time_point now) {
	std::vector<Transition> out;
	for (auto& [node, e] : entries_) {
		const auto to = judge(e, now);
		if (to != e.state) {
			out.push_back({node, e.state, to});
			e.state = to;
		}
	}
	std::sort(out.begin(), out.end(), [](const Transition& a, const Transition& b) { return a.node < b.node; });
	return out;
}

Liveness LivenessTable::state(std::uint64_t node) const {
	const auto it = entries_.find(node);
	return it == entries_.end() ? Liveness::down : it->second.state;
}

bool LivenessTable::duplicate(std::uint64_t node, std::uint64_t admitted, std::uint64_t boot, Clock::time_point now) const {
	if (admitted == 0 || boot == admitted) {
		return false;
	}
	auto last = elected_;
	if (const auto it = entries_.find(node); it != entries_.end() && it->second.boot == admitted) {
		last = std::max(last, it->second.heard);
	}
	return now - last < timing_.duplicate_window;
}

} // namespace cluster::membership
