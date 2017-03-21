/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_serialise_list.h"

#include "../src/serialise_list.h"
#include "utils.h"


inline int testing(const std::vector<std::string>& strs) {
	auto serialised = StringList::serialise(strs.begin(), strs.end());

	int cont = 0;

	StringList s(serialised);
	if (s.size() != strs.size()) {
		L_ERR(nullptr, "StringList is not working. Size: %zu Expected: %zu", s.size(), strs.size());
		++cont;
	}

	std::vector<std::string> res;
	StringList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != strs.size()) {
		L_ERR(nullptr, "StringList::unserialise is not working. Size: %zu Expected: %zu", res.size(), strs.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : strs) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR(nullptr, "StringList is not working. Result: [%s, %s] Expected: %s", it_s->c_str(), it_r->c_str(), elem.c_str());
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
		L_ERR(nullptr, "CartesianList is not working. Size: %zu Expected: %zu", s.size(), ptos.size());
		++cont;
	}

	std::vector<Cartesian> res;
	CartesianList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != ptos.size()) {
		L_ERR(nullptr, "CartesianList::unserialise is not working. Size: %zu Expected: %zu", res.size(), ptos.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : ptos) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR(nullptr, "CartesianList is not working. Result: [%s, %s] Expected: %s", it_s->to_string().c_str(), it_r->to_string().c_str(), elem.to_string().c_str());
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
		L_ERR(nullptr, "RangeList is not working. Size: %zu Expected: %zu", s.size(), ranges.size());
		++cont;
	}

	std::vector<range_t> res;
	RangeList::unserialise(serialised, std::back_inserter(res));
	if (res.size() != ranges.size()) {
		L_ERR(nullptr, "RangeList::unserialise is not working. Size: %zu Expected: %zu", res.size(), ranges.size());
		++cont;
	}

	auto it_s = s.begin();
	auto it_r = res.begin();
	for (const auto& elem : ranges) {
		if (*it_s != elem || *it_r != elem) {
			L_ERR(nullptr, "RangeList is not working. Result: [%s, %s] Expected: %s", it_s->to_string().c_str(), it_r->to_string().c_str(), elem.to_string().c_str());
			++cont;
		}
		++it_s;
		++it_r;
	}

	return cont;
}


int test_StringList() {
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
	std::vector<Cartesian> ptos;

	int cont = testing(ptos);

	ptos.emplace_back(1, 2, 3);
	cont += testing(ptos);

	ptos.emplace_back(4, 5, 6);
	ptos.emplace_back(7, 8, 9);
	ptos.emplace_back(10, 11, 12);
	ptos.emplace_back(13, 14, 15);
	ptos.emplace_back(16, 17, 18);
	ptos.emplace_back(19, 20, 21);
	ptos.emplace_back(22, 23, 24);
	cont += testing(ptos);

	RETURN(cont);
}


int test_RangeList() {
	std::vector<range_t> ranges;

	int cont = testing(ranges);

	ranges.emplace_back(1, 10);
	cont += testing(ranges);

	ranges.emplace_back(20, 30);
	ranges.emplace_back(40, 50);
	ranges.emplace_back(60, 70);
	ranges.emplace_back(80, 90);
	ranges.emplace_back(100, 110);
	ranges.emplace_back(120, 130);
	ranges.emplace_back(140, 150);
	cont += testing(ranges);

	RETURN(cont);
}
