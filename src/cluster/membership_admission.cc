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

#include "cluster/membership_admission.h"

#include <algorithm>  // for std::max
#include <limits>     // for std::numeric_limits

namespace cluster::membership {

AdmitAnswer admit(AdmissionRecord& node, std::uint64_t& next, std::uint64_t boot, std::uint64_t number) noexcept {
	// Numbers come from `next`, so the largest one has no offer above it and is never issued.
	if (number < node.record || number == std::numeric_limits<std::uint64_t>::max()) {
		return {Admission::stale, 0};
	}
	if (node.offered != 0 && number == node.offered && boot == node.offer_boot) {
		node.record = number;
		node.offered = node.offer_boot = 0;
		node.boot = boot;
		return {Admission::admitted, number};
	}
	// Above the record, the presented number and every number issued.
	next = std::max({next, node.record + 1, number + 1});
	node.offered = next++;
	node.offer_boot = boot;
	return {Admission::offer, node.offered};
}

ColdAnswer cold_check(const AdmissionRecord& copy, std::uint64_t number) noexcept {
	if (number < copy.record) {
		return ColdAnswer::stale;
	}
	if (number == copy.record || (copy.offered != 0 && number == copy.offered)) {
		return ColdAnswer::accepted;
	}
	return ColdAnswer::wait;
}

const char* name(Admission a) noexcept {
	switch (a) {
		case Admission::offer: return "offer";
		case Admission::admitted: return "admitted";
		case Admission::stale: return "stale";
	}
	return "?";
}

const char* name(ColdAnswer a) noexcept {
	switch (a) {
		case ColdAnswer::accepted: return "accepted";
		case ColdAnswer::stale: return "stale";
		case ColdAnswer::wait: return "wait";
	}
	return "?";
}

} // namespace cluster::membership
