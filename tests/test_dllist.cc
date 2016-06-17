/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_dllist.h"

#include "utils.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#define NUM_THREADS 10
#define S_ELEMENTS 100
#define D_ELEMENTS 2 * S_ELEMENTS
#define T_ELEMENTS 3 * S_ELEMENTS


std::string repr_results(const DLList<std::pair<int, char>>& l, bool sort) {
	std::vector<std::string> res;
	for (const auto& elem : l) {
		res.push_back(std::string(1, elem.second) + std::to_string(elem.first));
	}

	if (sort) {
		std::sort(res.begin(), res.end());
	}

	auto out = std::string();
	const auto it_e = res.end();
	for (auto it = res.begin(); it != it_e; ) {
		out.append(*it++);
		if (it != it_e) {
			out.append(" ");
		}
	}

	return out;
}


int test_iterators() {
	DLList<std::string> mylist;
	auto it = mylist.begin();
	it = mylist.insert(it, "10");
	it = mylist.insert(it, "20");
	it = mylist.insert(it, "30");
	it = mylist.insert(it, "40");
	mylist.insert(it, "50");

	std::string str, expected("50 40 30 20 10 ");
	for (const auto& val : mylist) {
		str.append(val).append(" ");
	}

	int err = 0;

	// Test for loop.
	if (expected != str) {
		L_ERR(nullptr, "ERROR: DLList with for loop is not working!. Result: %s  Expect: %s\n", str.c_str(), expected.c_str());
		++err;
	}

	// Test begin() and end().
	expected.assign("50 40 30 20 10 10 20 30 40 50 ");
	str.clear();
	const auto& it_e = mylist.end();
	for (auto it = mylist.begin(); it != it_e; ++it) {
		str.append(*it).append(" ");
	}

	auto eit = mylist.end();
	while (--eit) {
		str.append(*eit).append(" ");
	}

	if (expected != str) {
		L_ERR(nullptr, "ERROR: DLList::[begin()/end()] is not working. Result: %s  Expect: %s\n", str.c_str(), expected.c_str());
		++err;
	}

	// Test cbegin() and cend()
	str.clear();
	const auto& it_ce = mylist.cend();
	for (auto it = mylist.cbegin(); it != it_ce; ++it) {
		str.append(*it).append(" ");
	}

	auto cit = mylist.cend();
	while (--cit) {
		str.append(*cit).append(" ");
	}

	if (expected != str) {
		L_ERR(nullptr, "ERROR: DLList::c[begin()/end()] is not working. Result: %s  Expect: %s\n", str.c_str(), expected.c_str());
		++err;
	}

	// Test rbegin() and rend().
	expected.assign("10 20 30 40 50 50 40 30 20 10 ");
	str.clear();
	const auto& it_re = mylist.rend();
	for (auto it = mylist.rbegin(); it != it_re; ++it) {
		str.append(*it).append(" ");
	}

	auto rit = mylist.rend();
	while (--rit) {
		str.append(*rit).append(" ");
	}

	if (expected != str) {
		L_ERR(nullptr, "ERROR: DLList::r[begin()/end()] is not working. Result: %s  Expect: %s\n", str.c_str(), expected.c_str());
		++err;
	}

	// Test crbegin() and crend()
	str.clear();
	const auto& it_cre = mylist.crend();
	for (auto it = mylist.crbegin(); it != it_cre; ++it) {
		str.append(*it).append(" ");
	}

	auto crit = mylist.crend();
	while (--crit) {
		str.append(*crit).append(" ");
	}

	if (expected != str) {
		L_ERR(nullptr, "ERROR: DLList::cr[begin()/end()] is not working. Result: %s  Expect: %s\n", str.c_str(), expected.c_str());
		++err;
	}

	RETURN(err);
}


int test_push_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.push_front(std::make_pair<int, char>(1, 'a'));
	mylist.push_front(std::make_pair<int, char>(1, 'b'));
	mylist.push_front(std::make_pair<int, char>(1, 'c'));
	mylist.push_front(std::make_pair<int, char>(1, 'd'));
	mylist.push_front(std::make_pair<int, char>(1, 'e'));
	mylist.push_front(std::make_pair<int, char>(1, 'f'));
	mylist.push_front(std::make_pair<int, char>(1, 'g'));
	mylist.push_front(std::make_pair<int, char>(1, 'h'));
	mylist.push_front(std::make_pair<int, char>(1, 'i'));
	mylist.push_front(std::make_pair<int, char>(1, 'j'));
	mylist.push_front(std::make_pair<int, char>(1, 'k'));

	auto result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::push_front() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	RETURN(1);
}


int test_emplace_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');

	auto result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::emplace_front() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	RETURN(1);
}


int test_push_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.push_back(std::make_pair<int, char>(1, 'a'));
	mylist.push_back(std::make_pair<int, char>(1, 'b'));
	mylist.push_back(std::make_pair<int, char>(1, 'c'));
	mylist.push_back(std::make_pair<int, char>(1, 'd'));
	mylist.push_back(std::make_pair<int, char>(1, 'e'));
	mylist.push_back(std::make_pair<int, char>(1, 'f'));
	mylist.push_back(std::make_pair<int, char>(1, 'g'));
	mylist.push_back(std::make_pair<int, char>(1, 'h'));
	mylist.push_back(std::make_pair<int, char>(1, 'i'));
	mylist.push_back(std::make_pair<int, char>(1, 'j'));
	mylist.push_back(std::make_pair<int, char>(1, 'k'));

	auto result = repr_results(mylist, false);

	std::string expected("a1 b1 c1 d1 e1 f1 g1 h1 i1 j1 k1");

	if (result == expected) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::push_back() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	RETURN(1);
}


int test_emplace_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_back(1, 'a');
	mylist.emplace_back(1, 'b');
	mylist.emplace_back(1, 'c');
	mylist.emplace_back(1, 'd');
	mylist.emplace_back(1, 'e');
	mylist.emplace_back(1, 'f');
	mylist.emplace_back(1, 'g');
	mylist.emplace_back(1, 'h');
	mylist.emplace_back(1, 'i');
	mylist.emplace_back(1, 'j');
	mylist.emplace_back(1, 'k');

	auto result = repr_results(mylist, false);

	std::string expected("a1 b1 c1 d1 e1 f1 g1 h1 i1 j1 k1");

	if (result == expected) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::emplace_back() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	RETURN(1);
}


int test_insert() {
	DLList<std::pair<int, char>> mylist;

	// set some initial values:
	mylist.emplace_back(1, 'a');          // a1
	mylist.emplace_back(1, 'b');          // a1 b1
	mylist.emplace_back(1, 'c');          // a1 b1 c1
	mylist.emplace_back(1, 'd');          // a1 b1 c1 d1
	mylist.emplace_back(1, 'e');          // a1 b1 c1 d1 e1

	auto it = mylist.begin();             // ^
	++it;  // it points now to number b1        ^

	auto it2 = mylist.insert(it, std::make_pair<int, char>(1, 'f'));  // a1 f1 b1 c1 d1 e1
	// it still points to number b1                                            ^
	// it2 points to number f1                                              ^

	mylist.insert(it, std::make_pair<int, char>(1, 'g'));  // a1 f1 g1 b1 c1 d1 e1
	--it;  // it points now to g1                                   ^

	auto it3 = mylist.insert(it2, std::make_pair<int, char>(1, 'h')); // a1 h1 f1 g1 b1 c1 d1 e1
	// it2 still points to number f1                                           ^
	// it3 points to number h1                                              ^

	int err = 0;
	if (it->second != 'g' || it2->second != 'f' || it3->second != 'h') {
		L_ERR(nullptr, "ERROR: DLList::iterator is not working!. Result: %c %c %c     Expected: g f h\n", it->second, it2->second, it3->second);
		++err;
	}

	auto result = repr_results(mylist, false);

	std::string expected("a1 h1 f1 g1 b1 c1 d1 e1");
	if (result != expected) {
		L_ERR(nullptr, "ERROR: DLList::insert() is not working. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		++err;
	}

	RETURN(err);
}


int test_pop_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	std::string pop_elem, pop_elem2;

	int del = static_cast<int>(mylist.size() / 2);
	for (int i = 0; i < del; ++i) {
		pop_elem.append(1, mylist.front()->second);
		pop_elem2.append(1, mylist.pop_front()->second);
	}

	int err = 0;
	std::string expected_pop_elem("lkjihg");
	if (pop_elem != expected_pop_elem || pop_elem2 != expected_pop_elem) {
		L_ERR(nullptr, "ERROR: DLList::front() is not working!. Result: front { %s }   Result pop_front: { %s }   Expected: { %s }", pop_elem.c_str(), pop_elem2.c_str(), expected_pop_elem.c_str());
		++err;
	}

	auto result = repr_results(mylist, false);

	std::string expected("f1 e1 d1 c1 b1 a1");
	if (result != expected) {
		L_ERR(nullptr, "ERROR: DLList::pop_front() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		++err;
	}

	RETURN(err);
}


int test_pop_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	std::string pop_elem, pop_elem2;

	int del = static_cast<int>(mylist.size() / 2);
	for (int i = 0; i < del; ++i) {
		pop_elem.append(1, mylist.back()->second);
		pop_elem2.append(1, mylist.pop_back()->second);
	}

	int err = 0;
	std::string expected_pop_elem("abcdef");
	if (pop_elem != expected_pop_elem || pop_elem2 != expected_pop_elem) {
		L_ERR(nullptr, "ERROR: DLList::back() is not working!. Result: front { %s }   Result pop_back: { %s }   Expected: { %s }", pop_elem.c_str(), pop_elem2.c_str(), expected_pop_elem.c_str());
		++err;
	}

	auto result = repr_results(mylist, false);

	std::string expected("l1 k1 j1 i1 h1 g1");
	if (result != expected) {
		L_ERR(nullptr, "ERROR: DLList::pop_front() is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		++err;
	}

	RETURN(err);
}


int test_erase() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	std::string del_items;

	int cont = 0;
	for (auto it = mylist.begin(); it != mylist.end();) {
		if (cont % 2 == 0) {
			del_items.append(1, it->second);
			it = mylist.erase(it);
		} else {
			++it;
		}
		++cont;
	}

	int err = 0;
	std::string expected_del_items("ljhfdb");
	if (del_items != expected_del_items) {
		L_ERR(nullptr, "ERROR: DLList::iterator is not working!. Result: { %s }  Expected: { %s }", del_items.c_str(), expected_del_items.c_str());
		++err;
	}

	std::string result = repr_results(mylist, false);

	std::string expected("k1 i1 g1 e1 c1 a1");
	if (result != expected) {
		L_ERR(nullptr, "ERROR: DLList::erase is not working!. Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		++err;
	}

	RETURN(err);
}


int test_single_producer_consumer() {
	DLList<int> mylist;
	int err = 0;

	// Test several inserts
	size_t elements = 2500;
	for (int i = 0; i < elements; ++i) {
		mylist.push_front(i);
		mylist.push_back(i);
		mylist.emplace_front(i);
		mylist.emplace_back(i);
	}
	elements *= 4;

	// Test size.
	if (elements != mylist.size()) {
		L_ERR(nullptr, "ERROR: DLList single producer is not working!. Size: %zu   Expected: %zu\n", mylist.size(), elements);
		++err;
	}

	// Test several erases.
	for (int i = 0; i < 4000; ++i) {
		mylist.pop_front();
		mylist.pop_back();
		elements -= 2;
	}

	int i = 0;
	for (auto it = mylist.begin(); it != mylist.end(); ++i) {
		if (i % 2 == 0) {
			--elements;
			it = mylist.erase(it);
		} else {
			++it;
		}
	}

	// Test size
	if (elements != mylist.size()) {
		L_ERR(nullptr, "ERROR: DLList single consumer are not working!. Size: %zu    Expected: %zu\n", mylist.size(), elements);
		++err;
	}

	// Test clear
	mylist.clear();
	if (mylist.size() != 0) {
		L_ERR(nullptr, "ERROR: DLList::clear is not working!");
		++err;
	}

	RETURN(err);
}


int test_multi_push_emplace_front() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = D_ELEMENTS * val;
			auto end = start + S_ELEMENTS;
			auto end2 = end + S_ELEMENTS;
			for (int j = start; j < end; ++j) {
				l.push_front(j);
			}
			for (int j = end; j < end2; ++j) {
				l.emplace_front(j);
			}
		}, i);
	}

	for (auto& producer : producers) {
		producer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size());
	res.insert(l.begin(), l.end());

	if (res.size() == l.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::[push/emplace]_front() for multiples threads is not working!. Size List: %zu  Size_set: %zu\n", l.size(), res.size());
	RETURN(1);
}


int test_multi_push_emplace_back() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = D_ELEMENTS * val;
			auto end = start + S_ELEMENTS;
			auto end2 = end + S_ELEMENTS;
			for (int j = start; j < end; ++j) {
				l.push_back(j);
			}
			for (int j = end; j < end2; ++j) {
				l.emplace_back(j);
			}
		}, i);
	}

	for (auto& producer : producers) {
		producer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size());
	res.insert(l.begin(), l.end());

	if (res.size() == l.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::[push/emplace]_back() for multiples threads is not working!. Size List: %zu  Size_set: %zu\n", l.size(), res.size());
	RETURN(1);
}


int test_multi_insert() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto it = l.begin();
			auto start = D_ELEMENTS * val;
			auto end = start + D_ELEMENTS;
			for (int j = start; j < end; ++j) {
				it = l.insert(it, j);
			}
		}, i);
	}

	for (auto& producer : producers) {
		producer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size());
	res.insert(l.begin(), l.end());

	if (res.size() == l.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList::insert() for multiples threads is not working!. Size List: %zu  Size_set: %zu\n", l.size(), res.size());
	RETURN(1);
}


int test_multi_producers() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = T_ELEMENTS * val;
			auto end = start + S_ELEMENTS;
			auto end2 = end + S_ELEMENTS;
			auto end3 = end2 + S_ELEMENTS;
			auto it = l.begin();
			for (int j = start; j < end; ++j) {
				l.push_front(j);
			}
			for (int j = end; j < end2; ++j) {
				l.push_back(j);
			}
			for (int j = end2; j < end3; ++j) {
				it = l.insert(it, j);
			}
		}, i);
	}

	for (auto& producer : producers) {
		producer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size());
	res.insert(l.begin(), l.end());

	if (res.size() == l.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList for multiple producers is not working!. Size List: %zu   Size_set: %zu\n", l.size(), res.size());
	RETURN(1);
}


int test_multi_push_pop_front() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = D_ELEMENTS * val;
			auto end = start + S_ELEMENTS;
			auto end2 = end + S_ELEMENTS;
			for (int j = start; j < end; ++j) {
				l.push_front(j);
			}
			for (int j = end; j < end2; ++j) {
				l.emplace_front(j);
			}
		}, i);
	}

	DLList<int> elem_del;
	std::vector<std::thread> consumers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		consumers.emplace_back([&l, &elem_del]() {
			for (int j = 0; j < S_ELEMENTS; ++j) {
				try {
					elem_del.push_front(*l.pop_front());
				} catch (const std::out_of_range&) { }
			}
		});
	}

	for (auto& producer : producers) {
		producer.join();
	}

	for (auto& consumer : consumers) {
		consumer.join();
	}

	size_t total_elems = NUM_THREADS * D_ELEMENTS;
	if (total_elems != l.size() + elem_del.size()) {
		L_ERR(nullptr, "ERROR: DLList with multiple push_fronts and multiple pop_fronts is not working!. Size List: %zu  Deleted Elem: %zu total_elems: %zu\n", l.size(), elem_del.size(), total_elems);
		RETURN(1);
	}

	std::unordered_set<int> res;
	res.reserve(total_elems);
	res.insert(l.begin(), l.end());
	res.insert(elem_del.begin(), elem_del.end());

	L_ERR(nullptr, "Size List: %zu  Deleted Elem: %zu Set size: %zu  total_elems: %zu\n", l.size(), elem_del.size(), res.size(), total_elems);
	if (total_elems == res.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList with multiple push_fronts and multiple pop_fronts is not working!\n");
	RETURN(1);
}


int test_multi_push_pop_back() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = D_ELEMENTS * val;
			auto end = start + S_ELEMENTS;
			auto end2 = end + S_ELEMENTS;
			for (int j = start; j < end; ++j) {
				l.push_back(j);
			}
			for (int j = end; j < end2; ++j) {
				l.emplace_back(j);
			}
		}, i);
	}

	DLList<int> elem_del;
	std::vector<std::thread> consumers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		consumers.emplace_back([&l, &elem_del]() {
			for (int j = 0; j < S_ELEMENTS; ++j) {
				try {
					elem_del.push_back(*l.pop_back());
				} catch (const std::out_of_range&) { }
			}
		});
	}

	for (auto& producer : producers) {
		producer.join();
	}

	for (auto& consumer : consumers) {
		consumer.join();
	}

	size_t total_elems = NUM_THREADS * D_ELEMENTS;
	if (total_elems != l.size() + elem_del.size()) {
		L_ERR(nullptr, "ERROR: DLList with multiple push_backs and multiple pop_backs is not working!. Size List: %zu  Deleted Elem: %zu total_elems: %zu\n", l.size(), elem_del.size(), total_elems);
		RETURN(1);
	}

	std::unordered_set<int> res;
	res.reserve(total_elems);
	res.insert(l.begin(), l.end());
	res.insert(elem_del.begin(), elem_del.end());

	L_ERR(nullptr, "Size List: %zu  Deleted Elem: %zu Set size: %zu  total_elems: %zu\n", l.size(), elem_del.size(), res.size(), total_elems);
	if (total_elems == res.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList with multiple push_backs and multiple pop_backs is not working!\n");
	RETURN(1);
}


int test_multi_insert_erase() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto it = l.begin();
			auto start = D_ELEMENTS * val;
			auto end = start + D_ELEMENTS;
			for (int j = start; j < end; ++j) {
				it = l.insert(it, j);
			}
		}, i);
	}

	DLList<int> elem_del;
	std::vector<std::thread> consumers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		consumers.emplace_back([&l, &elem_del]() {
			int cont = 0;
			for (auto it = l.begin(); it != l.end(); ++cont) {
				if (cont % 2 == 0) {
					elem_del.insert(elem_del.begin(), *it);
					it = l.erase(it);
				} else {
					++it;
				}
			}
		});
	}

	for (auto& producer : producers) {
		producer.join();
	}

	for (auto& consumer : consumers) {
		consumer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size() + elem_del.size());
	res.insert(l.begin(), l.end());
	res.insert(elem_del.begin(), elem_del.end());

	size_t total_elems = NUM_THREADS * D_ELEMENTS;
	L_ERR(nullptr, "Size List: %zu  Deleted Elem: %zu Set size: %zu  total_elems: %zu\n", l.size(), elem_del.size(), res.size(), total_elems);
	if (total_elems == res.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList with multiple inserts and multiple erases is not working!\n");
	RETURN(1);
}


int test_multiple_producers_single_consumer() {
	DLList<int> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		producers.emplace_back([&l](int val) {
			auto start = D_ELEMENTS * val;
			auto end = start + D_ELEMENTS;
			for (int j = start; j < end; ++j) {
				l.push_back(j);
			}
		}, i);
	}

	DLList<int> elem_del;
	std::atomic_bool running(true);
	auto consumer = std::thread([&l, &elem_del, &running]() {
		int cont = 0;
		while (running.load()) {
			for (auto it = l.begin(); it != l.end(); ++cont) {
				if (cont % 2 == 0) {
					elem_del.push_front(*it);
					it = l.erase(it);
				} else {
					++it;
				}
			}
		}
	});

	for (auto& producer : producers) {
		producer.join();
	}

	running.store(false);

	consumer.join();

	std::unordered_set<int> res;
	res.reserve(l.size() + elem_del.size());
	res.insert(l.begin(), l.end());
	res.insert(elem_del.begin(), elem_del.end());

	size_t total_elems = NUM_THREADS * D_ELEMENTS;
	L_ERR(nullptr, "Size List: %zu  Deleted Elem: %zu Set size: %zu  total_elems: %zu\n", l.size(), elem_del.size(), res.size(), total_elems);
	if (total_elems == res.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList with multiple producers and single consumer is not working!\n");
	RETURN(1);
}


int test_single_producer_multiple_consumers() {
	DLList<int> l;

	auto producer = std::thread([&l]() {
		const auto end = NUM_THREADS * D_ELEMENTS;
		for (int j = 0; j < end; ++j) {
			l.push_back(j);
		}
	});

	DLList<int> elem_del;
	std::vector<std::thread> consumers;
	for (int i = 0; i < NUM_THREADS; ++i) {
		consumers.emplace_back([&l, &elem_del]() {
			int cont = 0;
			for (auto it = l.begin(); it != l.end(); ++cont) {
				if (cont % 2 == 0) {
					elem_del.insert(elem_del.begin(), *it);
					it = l.erase(it);
				} else {
					++it;
				}
			}
		});
	}

	producer.join();

	for (auto& consumer : consumers) {
		consumer.join();
	}

	std::unordered_set<int> res;
	res.reserve(l.size() + elem_del.size());
	res.insert(l.begin(), l.end());
	res.insert(elem_del.begin(), elem_del.end());

	size_t total_elems = NUM_THREADS * D_ELEMENTS;
	L_ERR(nullptr, "Size List: %zu  Deleted Elem: %zu Set size: %zu  total_elems: %zu\n", l.size(), elem_del.size(), res.size(), total_elems);
	if (total_elems == res.size()) {
		RETURN(0);
	}

	L_ERR(nullptr, "ERROR: DLList with single producer and multiple consumers is not worsking!\n");
	RETURN(1);
}
