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

#include "test_forward_list.h"

#include "../src/log.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>


template<typename Compare>
std::string repr_results(const ForwardList<std::pair<int, char>, Compare>& l, bool sort) {
	std::vector<std::string> res;
	for (auto it = l.begin(); it != l.end(); ++it) {
		res.push_back(std::string(1, it->second) + std::to_string(it->first));
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


int test_push_front() {
	ForwardList<std::pair<int, char>> mylist;

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

	std::string result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_insert_after() {
	ForwardList<std::pair<int, char>> mylist;
	ForwardList<std::pair<int, char>>::iterator it;

	it = mylist.insert_after(mylist.before_begin(), std::make_pair<int, char>(1, 'a'));
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'b'));
	it = mylist.insert_after(it, 5, std::make_pair<int, char>(1, 'c'));
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'd'));
	it = mylist.insert_after(it, { std::make_pair<int, char>(1, 'e'), std::make_pair<int, char>(2, 'e'), std::make_pair<int, char>(3, 'e'), std::make_pair<int, char>(4, 'e') });
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'f'));
	it = mylist.insert_after(mylist.begin(), std::make_pair<int, char>(1, 'g'));
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'h'));
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'i'));
	it = mylist.insert_after(mylist.before_begin(), 3, std::make_pair<int, char>(1, 'j'));
	it = mylist.insert_after(it, std::make_pair<int, char>(1, 'k'));
	it = mylist.insert_after(mylist.before_begin(), mylist.begin(), mylist.end());

	std::string result = repr_results(mylist, false);

	std::string expected("j1 j1 j1 k1 a1 g1 h1 i1 b1 c1 c1 c1 c1 c1 d1 e1 e2 e3 e4 f1 j1 j1 j1 k1 a1 g1 h1 i1 b1 c1 c1 c1 c1 c1 d1 e1 e2 e3 e4 f1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_emplace_front() {
	ForwardList<std::pair<int, char>> mylist;

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

	std::string result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_emplace_after() {
	ForwardList<std::pair<int, char>> mylist;
	ForwardList<std::pair<int, char>>::iterator it;

	it = mylist.emplace_after(mylist.before_begin(), 1, 'a');
	it = mylist.emplace_after(it, 1, 'b');
	it = mylist.emplace_after(it, 1, 'c');
	it = mylist.emplace_after(it, 1, 'd');
	it = mylist.emplace_after(it, 1, 'e');
	it = mylist.emplace_after(mylist.before_begin(), 1, 'f');
	it = mylist.emplace_after(it, 1, 'g');
	it = mylist.emplace_after(it, 1, 'h');
	it = mylist.emplace_after(mylist.begin(), 1, 'i');
	it = mylist.emplace_after(it, 1, 'j');
	it = mylist.emplace_after(it, 1, 'k');

	std::string result = repr_results(mylist, false);

	std::string expected("f1 i1 j1 k1 g1 h1 a1 b1 c1 d1 e1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_pop_front() {
	ForwardList<std::pair<int, char>> mylist;

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

	int del = mylist.size() / 2;
	for (int i = 0; i < del; ++i) {
		mylist.pop_front();
	}

	std::string result = repr_results(mylist, false);

	std::string expected("f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_erase_after() {
	ForwardList<std::pair<int, char>> mylist;

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

	int cont = 0;
	for (auto it = mylist.begin(); it != mylist.end(); ++cont) {
		if (cont % 2 == 0) {
			it = mylist.erase_after(it);
		} else {
			++it;
		}
	}

	std::string result = repr_results(mylist, false);

	std::string expected("k1 i1 h1 f1 e1 c1 b1");

	int err = 0;
	if (result != expected) {
		++err;
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	}

	mylist.erase_after(mylist.begin(), mylist.end());
	std::string result2 = repr_results(mylist, false);
	std::string expected2("k1");

	if (result2 != expected2) {
		++err;
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result2.c_str(), expected2.c_str());
	}

	return err;
}


int test_erase() {
	ForwardList<std::pair<int, char>> mylist;

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

	int cont = 0;
	for (auto it = mylist.begin(); it != mylist.end(); ++cont) {
		if (cont % 2 == 0) {
			it = mylist.erase(it);
		} else {
			++it;
		}
	}

	std::string result = repr_results(mylist, false);

	std::string expected("j1 h1 f1 d1 b1");

	int err = 0;
	if (result != expected) {
		++err;
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
	}

	mylist.erase_after(mylist.before_begin(), mylist.end());
	std::string result2 = repr_results(mylist, false);
	std::string expected2("");

	if (result2 != expected2) {
		++err;
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result2.c_str(), expected2.c_str());
	}

	return err;
}


int test_remove() {
	auto comparator = [](const std::pair<int, char>& p1, const std::pair<int, char>& p2) {
		return p1.first == p2.first && p1.second == p2.second;
	};

	ForwardList<std::pair<int, char>, decltype(comparator)> mylist(comparator);

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(2, 'a');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(3, 'a');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(2, 'b');
	mylist.emplace_front(4, 'a');
	mylist.emplace_front(1, 'h');

	mylist.remove(std::make_pair(1, 'a'));
	mylist.remove(std::make_pair(4, 'a'));
	mylist.remove(std::make_pair(1, 'b'));
	mylist.remove(std::make_pair(2, 'b'));

	std::string result = repr_results(mylist, false);

	std::string expected("h1 g1 f1 a3 e1 d1 a2 c1");

	if (result == expected) {
		return 0;
	} else {
		L_ERR(nullptr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_find() {
	auto comparator = [](const std::weak_ptr<int>& v1, const std::weak_ptr<int>& v2) {
		return v1.lock() == v2.lock();
	};

	ForwardList<std::weak_ptr<int>, decltype(comparator)> l(comparator);
	auto p1 = std::make_shared<int>(10);
	l.push_front(p1);
	auto p2 = std::make_shared<int>(20);
	l.push_front(p2);
	auto p3 = std::make_shared<int>(30);
	l.push_front(p3);
	auto p4 = std::make_shared<int>(40);
	l.push_front(p4);


	if (!l.find(p2)) {
		L_ERR(nullptr, "ForwardList::find is not working");
		return 1;
	}

	if (!l.find(p4)) {
		L_ERR(nullptr, "ForwardList::find is not working");
		return 1;
	}

	l.remove(p2);
	l.remove(p4);

	if (l.find(p2)) {
		L_ERR(nullptr, "ForwardList::find is not working");
		return 1;
	}

	if (l.find(p4)) {
		L_ERR(nullptr, "ForwardList::find is not working");
		return 1;
	}

	return 0;
}


int test_single_producer_consumer() {
	auto comparator = [](const std::pair<int, char>& p1, const std::pair<int, char>& p2) {
		return p1.second == p2.second;
	};

	ForwardList<std::pair<int, char>, decltype(comparator)> mylist(comparator);
	int err = 0;

	// Test several push_fronts
	size_t elements = 0;
	for (char c = 'a'; c <= 'z'; ++c) {
		elements += 100 * c;
		for (int i = 0; i < 100 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.push_front(ele);
			if (c == 'a') {
				mylist.remove(ele);
				--elements;
			}
		}
	}

	std::pair<int, char> ele(1, 'a');
	if (mylist.find(ele)) {
		L_ERR(nullptr, "ForwardList::remove is not working");
		++err;
	}

	// Test size
	if (elements != mylist.size()) {
		L_ERR(nullptr, "ForwardList::size is not working");
		++err;
	}


	// Test several insert_afters
	for (char c = 'a'; c <= 'z'; ++c) {
		elements += 100 * c;
		for (int i = 0; i < 100 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.insert_after(mylist.before_begin(), ele);
		}
	}

	// Test size
	if (elements != mylist.size()) {
		L_ERR(nullptr, "ForwardList::size is not working");
		++err;
	}

	for (int i = 0; i < 50000; ++i) {
		mylist.pop_front();
		mylist.erase_after(mylist.before_begin());
		mylist.erase(mylist.begin());
		elements -= 3;
	}

	// Test clear
	mylist.clear();
	if (mylist.size() != 0) {
		L_ERR(nullptr, "ForwardList::clear is not working");
		++err;
	}

	return err;
}


void task_producer(ForwardList<int>& mylist, std::atomic_size_t& elements) {
	for (int i = 0; i < 1000; ++i) {
		mylist.insert_after(mylist.before_begin(), i);
		++elements;
		mylist.push_front(i);
		++elements;
		mylist.emplace_after(mylist.before_begin(), i);
		++elements;
		mylist.emplace_front(i);
		++elements;
	}
}


int test_multiple_producers() {
	ForwardList<int> mylist;
	std::atomic_size_t elements(0);

	ForwardList<std::thread> threads;
	for (int i = 0; i < 10; ++i) {
		threads.emplace_front(task_producer, std::ref(mylist), std::ref(elements));
	}

	for (auto& ele : threads) {
		try {
			ele.join();
		} catch (const std::system_error&) {
		}
	}

	if (elements.load() == mylist.size()) {
		return 0;
	} else {
		L_ERR(nullptr, "Elements in List: %zu  Elements_count: %zu", mylist.size(), elements.load());
		return 1;
	}
}


void task_producer_consumer(ForwardList<std::pair<int, char>>& mylist, std::atomic_size_t& elements) {
	// Test several push_fronts
	for (char c = 'a'; c <= 'z'; ++c) {
		elements += 10 * c;
		for (int i = 0; i < 10 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.push_front(ele);
		}
	}

	// Test several insert_afters
	for (char c = 'a'; c <= 'z'; ++c) {
		elements += 10 * c;
		for (int i = 0; i < 10 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.insert_after(mylist.before_begin(), ele);
		}
	}

	for (int i = 0; i < 500; ++i) {
		mylist.pop_front();
		mylist.erase_after(mylist.before_begin());
		mylist.erase(mylist.begin());
		elements -= 3;
	}
}


int test_multiple_producers_consumers() {
	ForwardList<std::pair<int, char>> mylist;

	std::atomic_size_t elements(0);

	ForwardList<std::thread> threads;
	for (int i = 0; i < 10; ++i) {
		threads.emplace_front(task_producer_consumer, std::ref(mylist), std::ref(elements));
	}

	for (auto& ele : threads) {
		try {
			ele.join();
		} catch (const std::system_error&) {
		}
	}

	if (elements.load() == mylist.size()) {
		return 0;
	} else {
		L_ERR(nullptr, "Elements in List: %zu  Elements_count: %zu", mylist.size(), elements.load());
		return 1;
	}
}


void task_producer_allconsumer(ForwardList<std::pair<int, char>>& mylist) {
	// Test several push_fronts
	for (char c = 'a'; c <= 'z'; ++c) {
		for (int i = 0; i < 10 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.push_front(ele);
			mylist.pop_front();
		}
	}

	// Test several insert_afters
	for (char c = 'a'; c <= 'z'; ++c) {
		for (int i = 0; i < 10 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.insert_after(mylist.before_begin(), ele);
			mylist.pop_front();
		}
	}
}


int test_multiple_producers_consumers_v2() {
	ForwardList<std::pair<int, char>> mylist;

	ForwardList<std::thread> threads;
	for (int i = 0; i < 10; ++i) {
		threads.emplace_front(task_producer_allconsumer, std::ref(mylist));
	}

	for (auto& ele : threads) {
		try {
			ele.join();
		} catch (const std::system_error&) {
		}
	}

	if (mylist.size() == 0) {
		return 0;
	} else {
		L_ERR(nullptr, "Elements in List: %zu  Expected: 0", mylist.size());
		return 1;
	}
}
