/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "test_serialise_list.h"

#include "../src/serialise_list.h"
#include "utils.h"


inline int testing(const std::vector<std::string>& strs) {
	auto serialised = StringList::serialise(strs.begin(), strs.end());

	int cont = 0;

	StringList s(serialised);
	if (s.size() != strs.size()) {
		L_ERR("StringList is not working. Size: %zu Expected: %zu", s.size(), strs.size());
		++cont;
	}

	std::vector<std::string> res;
	StringList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != strs.size()) {
		L_ERR("StringList::unserialise is not working. Size: %zu Expected: %zu", res.size(), strs.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : strs) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR("StringList is not working. Result: [%s, %s] Expected: %s", *it_s, *it_r, elem);
			++cont;
		}
		++it_s;
		++it_r;
	}

	return cont;
}


inline int testing(const std::vector<Cartesian>& ptos) {
	auto serialised = CartesianList::serialise(ptos.begin(), ptos.end());

	int cont = 0;

	CartesianList s(serialised);
	if (s.size() != ptos.size()) {
		L_ERR("CartesianList is not working. Size: %zu Expected: %zu", s.size(), ptos.size());
		++cont;
	}

	std::vector<Cartesian> res;
	CartesianList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != ptos.size()) {
		L_ERR("CartesianList::unserialise is not working. Size: %zu Expected: %zu", res.size(), ptos.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : ptos) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR("CartesianList is not working. Result: [%s, %s] Expected: %s", it_s->to_string(), it_r->to_string(), elem.to_string());
			++cont;
		}
		++it_s;
		++it_r;
	}

	return cont;
}


inline int testing(const std::vector<range_t>& ranges) {
	auto serialised = RangeList::serialise(ranges.begin(), ranges.end());

	int cont = 0;

	RangeList s(serialised);
	if (s.size() != ranges.size()) {
		L_ERR("RangeList is not working. Size: %zu Expected: %zu", s.size(), ranges.size());
		++cont;
	}

	std::vector<range_t> res;
	RangeList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != ranges.size()) {
		L_ERR("RangeList::unserialise is not working. Size: %zu Expected: %zu", res.size(), ranges.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : ranges) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR("RangeList is not working. Result: [%s, %s] Expected: %s", it_s->to_string(), it_r->to_string(), elem.to_string());
			++cont;
		}
		++it_s;
		++it_r;
	}

	return cont;
}


int test_StringList() {
	INIT_LOG
	std::vector<std::string> strs;

	int cont = testing(strs);

	strs.emplace_back("a");
	cont += testing(strs);

	strs.emplace_back("b");
	strs.emplace_back("c");
	strs.emplace_back("d");
	strs.emplace_back("e");
	strs.emplace_back("f");
	strs.emplace_back("g");
	strs.emplace_back("h");
	strs.emplace_back("i");
	strs.emplace_back("j");
	cont += testing(strs);

	RETURN(cont);
}


int test_CartesianList() {
	INIT_LOG
	std::vector<Cartesian> ptos;

	int cont = testing(ptos);

	ptos.emplace_back(-1, 0, 0);
	cont += testing(ptos);

	ptos.emplace_back(0.267261, 0.534522, 0.801784);
	ptos.emplace_back(0.455842, 0.569803, 0.683763);
	ptos.emplace_back(0.502571, 0.574367, 0.646162);
	ptos.emplace_back(0.523424, 0.575766, 0.628109);
	ptos.emplace_back(-0.267261, 0.534522, 0.801784);
	ptos.emplace_back(0.455842, -0.569803, 0.683763);
	ptos.emplace_back(0.502571, 0.574367, -0.646162);
	ptos.emplace_back(-0.523424, -0.575766, -0.628109);
	cont += testing(ptos);

	RETURN(cont);
}


int test_RangeList() {
	INIT_LOG
	std::vector<range_t> ranges;

	int cont = testing(ranges);

	// Small level range.
	ranges.emplace_back(14363263991021568, 14363298350759935);
	cont += testing(ranges);

	ranges.emplace_back(14363315530629120, 14363332710498303);
	ranges.emplace_back(14363367070236672, 14363384250105855);
	ranges.emplace_back(14363401429975040, 14363418609844223);
	ranges.emplace_back(14363607588405248, 14363624768274431);
	ranges.emplace_back(14363641948143616, 14363676307881983);
	ranges.emplace_back(14363745027358720, 14363813746835455);
	ranges.emplace_back(14363899646181376, 14363916826050559);
	ranges.emplace_back(14363968365658112, 14364019905265663);
	cont += testing(ranges);

	RETURN(cont);
}
