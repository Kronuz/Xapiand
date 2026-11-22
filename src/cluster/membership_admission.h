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
 * A boot's admission (docs/design/cluster-membership.md M7, rule "above" of its model
 * m7_admission.py; docs/mechanisms/cluster-membership.md). Each process start is a boot that
 * presents the admission number its directory saved. The root leader answers `stale` below the
 * node's record and otherwise commits a fresh offer above both; the node saves the offer and
 * presents it, and only the boot the offer was made to completes it as the new record. So every
 * directory older than the last confirm holds less than the record. Pure functions over the
 * root's node record: the caller commits the record they change.
 */

#pragma once

#include <cstdint>  // for std::uint8_t, std::uint64_t

namespace cluster::membership {

// A node's admission fields in the root's directory (M1's Node record).
struct AdmissionRecord {
	std::uint64_t record = 0;      // the confirmed admission number
	std::uint64_t offered = 0;     // the pending offer, 0 for none
	std::uint64_t offer_boot = 0;  // the boot the pending offer was made to
	std::uint64_t boot = 0;        // the admitted boot, 0 after a clean stop
	bool operator==(const AdmissionRecord&) const = default;
};

enum class Admission : std::uint8_t {
	offer,     // save `number` and present it
	admitted,  // `number` is the record; the boot may open groups
	stale,     // an older copy of the directory: empty it and rejoin as a new node
};

struct AdmitAnswer {
	Admission outcome;
	std::uint64_t number = 0;  // the offer, or the record once admitted; 0 when stale
	bool operator==(const AdmitAnswer&) const = default;
};

// One of the three steps, a root leader's command: `boot` presents `number`. `next` is the
// cluster record's next admission number; both it and `node` are committed with the answer.
// A number above the record means the record lags (a root salvage lost a refresh or the join, F17)
// and gets an offer like any other.
AdmitAnswer admit(AdmissionRecord& node, std::uint64_t& next, std::uint64_t boot, std::uint64_t number) noexcept;

// A root voter's answer to another voter while the root has no leader, against its own copy of
// that voter's record: `wait` means the copy may lag and only a leader decides.
enum class ColdAnswer : std::uint8_t { accepted, stale, wait };
ColdAnswer cold_check(const AdmissionRecord& copy, std::uint64_t number) noexcept;

const char* name(Admission a) noexcept;
const char* name(ColdAnswer a) noexcept;

} // namespace cluster::membership
