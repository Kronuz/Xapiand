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

// Membership's admission rule and keepalive table (src/cluster/membership_*.h):
//   admission  M7's three steps: first start, refresh, crashes between steps, backups, a copy
//              that ran, a record that lags (F17), the largest number
//   histories  4,000 random histories of starts, refreshes, kills and copies (the model's
//              explore()): no directory is admitted after another confirmed since they diverged,
//              and the last to confirm is never locked out
//   cold       a root voter's answer against its own copy while the root has no leader
//   liveness   up, suspect at 1 s, down at 1.5 s, logged once each; a new leader counts from its
//              election; a restart is a new boot
//   duplicate  another boot is refused within 2 s of the admitted one's last keepalive
//   xapiand_cluster_test_membership [case]

#include "cluster/membership_admission.h"
#include "cluster/membership_liveness.h"
#include "test_harness.h"

#include <array>    // for std::array
#include <cstring>  // for std::strcmp
#include <limits>   // for std::numeric_limits
#include <map>      // for std::map
#include <random>   // for std::mt19937_64
#include <vector>   // for std::vector

namespace {

using namespace cluster::membership;
using std::chrono::milliseconds;

// The three steps for a boot whose directory holds `disk`; `crash` stops after the offer's commit
// (1) or after the node saved it (2). Returns the last answer.
AdmitAnswer steps(AdmissionRecord& node, std::uint64_t& next, std::uint64_t boot, std::uint64_t& disk, int crash = 0) {
	auto a = admit(node, next, boot, disk);
	if (a.outcome != Admission::offer || crash == 1) {
		return a;
	}
	disk = a.number;
	if (crash == 2) {
		return a;
	}
	return admit(node, next, boot, disk);
}

void test_admission() {
	AdmissionRecord node;
	std::uint64_t next = 1, disk = 0;

	// First start: an offer, then the record with the boot.
	auto a = admit(node, next, 11, disk);
	CHECK(a == (AdmitAnswer{Admission::offer, 1}));
	CHECK(node.offered == 1 && node.offer_boot == 11 && node.record == 0);
	disk = a.number;
	CHECK(admit(node, next, 11, disk) == (AdmitAnswer{Admission::admitted, 1}));
	CHECK(node == (AdmissionRecord{1, 0, 0, 11}));

	// The hourly refresh by the running boot.
	CHECK(steps(node, next, 11, disk).outcome == Admission::admitted);
	CHECK(node.record == 2 && disk == 2 && node.boot == 11);

	// A crash after the offer's commit, before the save: the next boot starts over.
	CHECK(steps(node, next, 12, disk, 1).outcome == Admission::offer);
	CHECK(disk == 2);
	CHECK(steps(node, next, 13, disk).outcome == Admission::admitted);
	CHECK(node.boot == 13 && node.record == disk);

	// A crash after the save: the offer is the dead boot's, so the next boot gets a fresh one.
	CHECK(steps(node, next, 14, disk, 2).outcome == Admission::offer);
	const auto saved = disk;
	CHECK(node.offered == saved && node.record < saved);
	a = admit(node, next, 15, disk);
	CHECK(a.outcome == Admission::offer && a.number > saved);
	disk = a.number;
	CHECK(admit(node, next, 15, disk).outcome == Admission::admitted);

	// A backup older than one refresh is stale, on every restart, and changes nothing.
	auto backup = node.record;
	CHECK(steps(node, next, 15, disk).outcome == Admission::admitted);
	const auto before = node;
	for (std::uint64_t boot = 100; boot < 200; ++boot) {
		auto n = backup;
		CHECK(steps(node, next, boot, n) == (AdmitAnswer{Admission::stale, 0}));
	}
	CHECK(node == before);

	// A cold copy that ran makes its original stale.
	auto copy = disk;
	CHECK(steps(node, next, 300, copy).outcome == Admission::admitted);
	CHECK(steps(node, next, 301, disk).outcome == Admission::stale);

	// A record that lags the node (a root salvage lost its refresh, F17): re-admitted.
	AdmissionRecord lagging{5, 0, 0, 0};
	std::uint64_t next2 = 6, ahead = 9;
	CHECK(steps(lagging, next2, 400, ahead).outcome == Admission::admitted);
	CHECK(lagging.record == ahead && ahead > 9 && next2 > ahead);

	// The largest number has no offer above it.
	std::uint64_t top = std::numeric_limits<std::uint64_t>::max();
	CHECK(admit(lagging, next2, 401, top).outcome == Admission::stale);

	// Only the boot an offer was made to confirms it: a copy of a saved offer gets its own.
	AdmissionRecord race;
	std::uint64_t next3 = 1, d1 = 0;
	d1 = admit(race, next3, 1, d1).number;  // boot 1 saved its offer, then its directory was copied
	auto d2 = d1;
	a = admit(race, next3, 2, d2);
	CHECK(a.outcome == Admission::offer && a.number > d1 && race.offer_boot == 2);
	d2 = a.number;
	CHECK(admit(race, next3, 2, d2).outcome == Admission::admitted);
	CHECK(admit(race, next3, 1, d1).outcome == Admission::stale);
}

// The model's History (m7_admission.py): one identity's directories under one root.
struct History {
	AdmissionRecord root;
	std::uint64_t next = 1;
	std::vector<std::uint64_t> n{0};
	std::vector<std::map<std::size_t, int>> div{{}};  // divergence time from each other directory
	std::vector<int> last{-1};                         // time of each directory's last confirm
	int violations = 0;

	bool run(std::size_t i, std::uint64_t boot, int crash, int clock) {
		if (steps(root, next, boot, n[i], crash).outcome != Admission::admitted || crash) {
			return false;
		}
		for (std::size_t j = 0; j < n.size(); ++j) {
			const auto d = div[i].find(j);
			if (j != i && last[j] > (d == div[i].end() ? -1 : d->second)) {
				++violations;
			}
		}
		last[i] = clock;
		return true;
	}

	void copy(std::size_t s, int clock) {
		const auto c = n.size();
		n.push_back(n[s]);
		div.push_back(div[s]);
		last.push_back(-1);
		for (std::size_t j = 0; j < c; ++j) {
			const auto d = div[j].find(s);
			div[j][c] = j == s ? clock : (d == div[j].end() ? -1 : d->second);
		}
		div[c][s] = clock;
	}
};

void test_histories() {
	std::mt19937_64 rng(20261121);
	std::uniform_real_distribution<double> event(0, 1);
	int violations = 0, lockouts = 0;
	for (int trial = 0; trial < 4000; ++trial) {
		History h;
		bool holding = false;
		std::size_t held = 0;
		std::uint64_t held_boot = 0;
		for (int clock = 0; clock < 40; ++clock) {
			const double e = event(rng);
			const int crash = std::array{0, 0, 1, 2}[rng() % 4];
			if (clock == 0 || e < 0.3) {
				const auto i = std::size_t(rng() % h.n.size());
				const auto boot = (rng() >> 2) + 1;
				if (!holding && h.run(i, boot, crash, clock)) {
					holding = true, held = i, held_boot = boot;
				}
			} else if (e < 0.5) {
				if (holding && !h.run(held, held_boot, crash, clock)) {
					holding = false;  // killed mid-refresh, or refused
				}
			} else if (e < 0.65) {
				holding = false;
			} else {
				h.copy(std::size_t(rng() % h.n.size()), clock);
			}
		}
		std::size_t newest = 0;
		for (std::size_t j = 1; j < h.n.size(); ++j) {
			newest = h.last[j] > h.last[newest] ? j : newest;
		}
		lockouts += !h.run(newest, (rng() >> 2) + 1, 0, 40);
		violations += h.violations;
	}
	std::printf("  violations %d, lockouts %d over 4000 histories\n", violations, lockouts);
	CHECK_OP(violations, ==, 0);
	CHECK_OP(lockouts, ==, 0);
}

void test_cold() {
	const AdmissionRecord copy{4, 6, 77, 0};
	CHECK(cold_check(copy, 3) == ColdAnswer::stale);
	CHECK(cold_check(copy, 4) == ColdAnswer::accepted);
	CHECK(cold_check(copy, 6) == ColdAnswer::accepted);
	CHECK(cold_check(copy, 5) == ColdAnswer::wait);  // above the record: the copy may lag
	CHECK(cold_check(copy, 9) == ColdAnswer::wait);
	CHECK(cold_check(AdmissionRecord{4, 0, 0, 0}, 0) == ColdAnswer::stale);
}

void test_liveness() {
	const auto t0 = Clock::time_point{} + std::chrono::hours(1);
	LivenessTable table(t0);
	table.expect(1, 11, t0);
	table.expect(2, 22, t0);
	CHECK(table.sweep(t0 + milliseconds(999)).empty());

	// Node 1 keeps its keepalives; node 2 dies at the election.
	for (int ms = 500; ms <= 3000; ms += 500) {
		CHECK(!table.heard(1, 11, t0 + milliseconds(ms)));
	}
	auto s = table.sweep(t0 + milliseconds(1000));
	CHECK(s.size() == 1 && s[0] == (Transition{2, Liveness::up, Liveness::suspect}));
	CHECK(table.sweep(t0 + milliseconds(1250)).empty());  // logged once
	s = table.sweep(t0 + milliseconds(1500));
	CHECK(s.size() == 1 && s[0] == (Transition{2, Liveness::suspect, Liveness::down}));
	CHECK(table.state(2) == Liveness::down && table.state(1) == Liveness::up);
	CHECK(table.sweep(t0 + milliseconds(1750)).empty());

	// A sweep late by more than an interval reports up to down in one step.
	table.expect(3, 33, t0 + milliseconds(1750));
	s = table.sweep(t0 + milliseconds(3500));
	CHECK(s.size() == 1 && s[0] == (Transition{3, Liveness::up, Liveness::down}));

	// Node 2 restarts: a new boot comes back up; a further keepalive logs nothing.
	auto up = table.heard(2, 222, t0 + milliseconds(4000));
	CHECK(up && *up == (Transition{2, Liveness::down, Liveness::up}));
	CHECK(!table.heard(2, 222, t0 + milliseconds(4500)));

	// An untracked node is down until heard; its first keepalive logs `up`.
	CHECK(table.state(9) == Liveness::down);
	up = table.heard(9, 99, t0 + milliseconds(4500));
	CHECK(up && *up == (Transition{9, Liveness::down, Liveness::up}));
	table.forget(9);
	CHECK(table.state(9) == Liveness::down && table.size() == 3);

	// With 250 ms sweeps, a node killed right after a keepalive is logged down within 2 s.
	LivenessTable t2(t0);
	t2.expect(5, 55, t0);
	const auto killed = t0 + milliseconds(500);
	t2.heard(5, 55, killed);
	auto when = killed;
	while (t2.state(5) != Liveness::down) {
		when += milliseconds(250);
		t2.sweep(when);
	}
	CHECK(when - killed <= std::chrono::seconds(2));
}

void test_duplicate() {
	const auto t0 = Clock::time_point{} + std::chrono::hours(1);
	LivenessTable table(t0);
	// A boot this leader never heard counts from its election.
	CHECK(table.duplicate(1, 11, 12, t0 + milliseconds(1999)));
	CHECK(!table.duplicate(1, 11, 12, t0 + milliseconds(2000)));
	CHECK(!table.duplicate(1, 11, 11, t0));  // the admitted boot itself
	CHECK(!table.duplicate(1, 0, 12, t0));   // no admitted boot: a clean stop

	// A hot copy beside a running original is refused while the original is heard.
	table.expect(1, 11, t0);
	for (int ms = 0; ms <= 10000; ms += 500) {
		table.heard(1, 11, t0 + milliseconds(ms));
		CHECK(table.duplicate(1, 11, 12, t0 + milliseconds(ms + 250)));
	}
	// The original dies at 10 s: its successor is admitted from 12 s.
	CHECK(table.duplicate(1, 11, 13, t0 + milliseconds(11999)));
	CHECK(!table.duplicate(1, 11, 13, t0 + milliseconds(12000)));

	// The table heard another boot than the directory's admitted one: count from the election.
	LivenessTable t2(t0);
	t2.heard(1, 99, t0 + milliseconds(5000));
	CHECK(!t2.duplicate(1, 11, 12, t0 + milliseconds(5000)));
}

} // namespace

int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*run)();
	};
	const Case cases[] = {{"admission", test_admission}, {"histories", test_histories}, {"cold", test_cold},
	                      {"liveness", test_liveness}, {"duplicate", test_duplicate}};
	for (const auto& c : cases) {
		if (argc > 1 && std::strcmp(argv[1], c.name) != 0)
			continue;
		std::printf("%s\n", c.name);
		c.run();
	}
	if (testh::failures) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
