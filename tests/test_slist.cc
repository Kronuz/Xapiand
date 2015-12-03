/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "test_slist.h"

#include "../src/log.h"

#include <string>
#include <thread>
#include <vector>


std::string repr_results(slist<std::string>& l, bool sort) {
	std::vector<std::string> res;
	for (auto it = l.begin(); it != l.end(); ++it) {
		res.push_back(*it);
	}

	if (sort) {
		std::sort(res.begin(), res.end());
	}

	std::string out;
	for (auto it = res.begin(); it != res.end(); ) {
		out += *it++;
		if (it != res.end()) {
			out += " ";
		}
	}

	return out;
}


void push_front(slist<std::string>& l, char type, unsigned num) {
	for (unsigned i = 0; i < num; ++i) {
		l.push_front(std::string(1, type) + std::to_string(i));
	}
}


void insert(slist<std::string>& l, char type, unsigned num) {
	auto it = l.begin();
	for (unsigned i = 0; i < num; ++i) {
		l.insert(it, std::string(1, type) + std::to_string(i));
	}
}


void consumer(slist<std::string>& l, char type, unsigned num, std::atomic_size_t& deletes) {
	for (unsigned i = 0; i < num; ++i) {
		std::string data = std::string(1, type) + std::to_string(i);
		while (!l.remove(data));
		++deletes;
	}
}


void consumer_v2(slist<std::string>& l, char type, unsigned num, std::atomic_size_t& deletes) {
	for (unsigned i = 0; i < num; ++i) {
		std::string data = std::string(1, type) + std::to_string(i);
		if (l.remove(data)) {
			++deletes;
		}
	}
}


int test_push_front() {
	slist<std::string> l;

	size_t elements = 0;
	for (char type = 'A'; type <= 'Z'; ++type) {
		elements += type * 110;
		push_front(l, type, type * 100);
		push_front(l, type - 'A' + 'a', type * 10);
	}

	if (l.size() == elements) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: %zu  Expected: %zu\n", l.size(), elements);
		return 1;
	}
}


int test_insert() {
	slist<std::string> l;

	size_t elements = 0;
	for (char type = 'A'; type <= 'Z'; ++type) {
		elements += type * 110;
		insert(l, type, type * 100);
		insert(l, type - 'A' + 'a', type * 10);
	}

	if (l.size() == elements) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: %zu  Expected: %zu\n", l.size(), elements);
		return 1;
	}
}


int test_correct_order() {
	slist<std::string> l;
	for (char type = 'A'; type <= 'Z'; ++type) {
		push_front(l, type, 1);
		push_front(l, type - 'A' + 'a', 1);
	}

	std::string res_push = repr_results(l, false);

	l.clear();
	for (char type = 'A'; type <= 'Z'; ++type) {
		insert(l, type, 1);
		insert(l, type - 'A' + 'a', 1);
	}

	std::string res_ins = repr_results(l, false);

	std::string expected("z0 Z0 y0 Y0 x0 X0 w0 W0 v0 V0 u0 U0 t0 T0 s0 S0 r0 R0 q0 Q0 p0 P0 o0 O0 n0 N0 m0 M0 l0 L0 k0 K0 j0 J0 i0 I0 h0 H0 g0 G0 f0 F0 e0 E0 d0 D0 c0 C0 b0 B0 a0 A0");

	if (res_push == expected && res_ins == expected) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements push_front: { %s }\n", res_push.c_str());
		LOG_ERR(nullptr, "Expected push_front: { %s }\n", expected.c_str());
		LOG_ERR(nullptr, "Elements insert: { %s }\n", res_ins.c_str());
		LOG_ERR(nullptr, "Expected insert: { %s }\n", expected.c_str());
		return 1;
	}
}


int test_remove() {
	slist<std::string> l;
	for (char type = 'A'; type <= 'Z'; ++type) {
		push_front(l, type, 1);
		push_front(l, type - 'A' + 'a', 1);
	}

	l.remove("A0");
	l.remove("c0");
	l.remove("C0");
	l.remove("Z0");

	std::string res = repr_results(l, false);

	std::string expected("z0 y0 Y0 x0 X0 w0 W0 v0 V0 u0 U0 t0 T0 s0 S0 r0 R0 q0 Q0 p0 P0 o0 O0 n0 N0 m0 M0 l0 L0 k0 K0 j0 J0 i0 I0 h0 H0 g0 G0 f0 F0 e0 E0 d0 D0 b0 B0 a0");

	if (res == expected) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: { %s }\n", res.c_str());
		LOG_ERR(nullptr, "Expected: { %s }\n", expected.c_str());
		return 1;
	}
}


int test_erase() {
	slist<std::string> l;
	for (char type = 'A'; type <= 'Z'; ++type) {
		push_front(l, type, 1);
		push_front(l, type - 'A' + 'a', 1);
	}

	for (auto it = l.begin(); it != l.end(); ++it) {
		if (it->at(0) >= 'A' && it->at(0) <= 'Z') {
			l.erase(it);
		}
	}

	std::string res = repr_results(l, false);

	std::string expected("z0 y0 x0 w0 v0 u0 t0 s0 r0 q0 p0 o0 n0 m0 l0 k0 j0 i0 h0 g0 f0 e0 d0 c0 b0 a0");

	if (res == expected) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: { %s }\n", res.c_str());
		LOG_ERR(nullptr, "Expected: { %s }\n", expected.c_str());
		return 1;
	}
}


int test_pop_front() {
	slist<std::string> l;
	for (char type = 'A'; type <= 'Z'; ++type) {
		push_front(l, type, 1);
		push_front(l, type - 'A' + 'a', 1);
	}

	size_t half = l.size() / 2;
	for (size_t i = 0; i < half; ++i) {
		l.pop_front();
	}

	std::string res = repr_results(l, false);

	std::string expected("m0 M0 l0 L0 k0 K0 j0 J0 i0 I0 h0 H0 g0 G0 f0 F0 e0 E0 d0 D0 c0 C0 b0 B0 a0 A0");

	if (l.size() == half && res == expected) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: { %s } (Size: %zu)\n", res.c_str(), l.size());
		LOG_ERR(nullptr, "Expected: { %s } (Size: %zu)\n", expected.c_str(), half);
		return 1;
	}
}


int test_find() {
	slist<std::string> l;
	for (char type = 'A'; type <= 'Z'; ++type) {
		insert(l, type, type - 1);
		insert(l, type - 'A' + 'a', type - 1);
	}

	size_t found = 0;
	for (char type = 'A'; type <= 'Z'; ++type) {
		for (int i = 0; i < type; ++i) {
			found += l.find(std::string(1, type) + std::to_string(i));
			found += l.find(std::string(1, type - 'A' + 'a') + std::to_string(i));
		}
	}

	if (found == l.size()) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements found: %zu  Expected: %zu\n", found, l.size());
		return 1;
	}
}


int test_multiple_producers() {
	slist<std::string> l;

	size_t elements = 0;
	std::vector<std::thread> threads;
	for (char type = 'A'; type <= 'Z'; ++type) {
		elements += type * 110;
		threads.emplace_back([&l](char _t, unsigned _n){ insert(l, _t, _n); }, type, type * 100);
		threads.emplace_back([&l](char _t, unsigned _n){ push_front(l, _t, _n); }, type - 'A' + 'a', type * 10);
	}

	for (auto& t : threads) {
		t.join();
	}

	if (l.size() == elements) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: %zu  Expected: %zu\n", l.size(), elements);
		return 1;
	}
}


int test_multiple_producers_consumers() {
	slist<std::string> l;

	std::vector<std::thread> threads;
	std::atomic_size_t deletes(0);
	size_t elements = 0;
	for (char type = 'A'; type <= 'Z'; ++type) {
		elements += 2 * type;
		threads.emplace_back([&l](char _t, unsigned _n){ insert(l, _t, _n); }, type, type);
		threads.emplace_back([&l](char _t, unsigned _n){ push_front(l, _t, _n); }, type - 'A' + 'a', type);
		threads.emplace_back([&l, &deletes](char _t, unsigned _n){ consumer(l, _t, _n, deletes); }, type, type);
		threads.emplace_back([&l, &deletes](char _t, unsigned _n){ consumer(l, _t, _n, deletes); }, type - 'A' + 'a', type);
	}

	for (auto& t : threads) {
		t.join();
	}

	if (l.size() == static_cast<size_t>(elements - deletes.load())) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: %zu  Expected: %zu\n", l.size(), elements - deletes.load());
		return 1;
	}
}


int test_multiple_producers_consumers_v2() {
	slist<std::string> l;

	std::vector<std::thread> threads;
	std::atomic_size_t deletes(0);
	int elements = 0;
	for (char type = 'A'; type <= 'Z'; ++type) {
		elements += 2 * type;
		threads.emplace_back([&l](char _t, unsigned _n){ insert(l, _t, _n); }, type, type);
		threads.emplace_back([&l](char _t, unsigned _n){ push_front(l, _t, _n); }, type - 'A' + 'a', type);
		threads.emplace_back([&l, &deletes](char _t, unsigned _n){ consumer_v2(l, _t, _n, deletes); }, type, type);
		threads.emplace_back([&l, &deletes](char _t, unsigned _n){ consumer_v2(l, _t, _n, deletes); }, type - 'A' + 'a', type);
	}

	for (auto& t : threads) {
		t.join();
	}

	if (l.size() == static_cast<size_t>(elements - deletes.load())) {
		return 0;
	} else {
		LOG_ERR(nullptr, "Elements in the List: %zu  Expected: %d\n", l.size(), elements - deletes.load());
		return 1;
	}
}
